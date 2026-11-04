#include "component/transfer_component.hpp"
#include "component/input_component.hpp"
#include "component/selection_component.hpp"
#include "component/pagination_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <algorithm>
#include <cmath>
#include <thread>

namespace ryn::detail {
namespace {
thread_local TransferComponentHost* active_transfer{};

struct Row final {
    TransferItem item;
    runtime::ComponentId component;
    runtime::ComponentId checkbox;
    runtime::ComponentId remove;
    runtime::ComponentId content;
    runtime::Rect full;
    Signal<String> label{String{}};
    Signal<bool> checked{false};
    Signal<bool> disabled{false};
    Signal<bool> remove_disabled{true};
    bool target{};
    bool filtered{};
    bool shown{};
    std::size_t order{};
};

struct Panel final {
    runtime::ComponentId title;
    runtime::ComponentId search;
    runtime::ComponentId all;
    runtime::ComponentId invert;
    runtime::ComponentId clear;
    runtime::ComponentId empty;
    runtime::ComponentId pagination;
    runtime::ComponentId size_changer;
    runtime::ComponentId footer;
    runtime::ComponentId custom;
    runtime::Rect viewport;
    Signal<String> summary{String{}};
    Signal<String> empty_text{String{u8"No data"}};
    Signal<bool> checked{false};
    Signal<bool> indeterminate{false};
    Signal<bool> disabled{false};
    Signal<std::size_t> total{0};
    Signal<std::size_t> page{1};
    Signal<std::size_t> page_size{10};
    Signal<std::vector<TransferItem>> filtered{std::vector<TransferItem>{}};
    Signal<TransferKeys> selected{TransferKeys{}};
    String query;
    float scroll{};
    std::size_t count{};
};

struct State final {
    TransferProps props;
    Scope bindings;
    runtime::NodeId node;
    std::vector<std::shared_ptr<Row>> rows;
    std::array<Panel, 2> panels;
    std::array<runtime::ComponentId, 2> actions;
    std::array<Signal<bool>, 2> move_disabled{Signal<bool>{true}, Signal<bool>{true}};
    Signal<bool> disabled{false};
    TransferKeys target;
    TransferKeys selected;
    std::thread::id owner_thread{std::this_thread::get_id()};
    bool ready{};
    bool mounting{true};
    bool refreshing{};
};

using S = std::shared_ptr<State>;

S state(WindowComponentServices& v, runtime::ComponentId id) {
    const auto* s = v.components().state<S>(id);
    return s ? *s : S{};
}

bool has(const TransferKeys& keys, const TransferKey& key) {
    return std::ranges::find(keys, key) != keys.end();
}

void valid_keys(const TransferKeys& keys) {
    TransferKeys seen;
    for (const auto& key : keys) {
        if ((std::holds_alternative<String>(key) && std::get<String>(key).empty()) ||
            (std::holds_alternative<double>(key) && !std::isfinite(std::get<double>(key))) || has(seen, key)) {
            throw std::invalid_argument("Transfer keys must be nonempty, unique and finite");
        }
        seen.push_back(key);
    }
}

TransferDirection side(std::size_t i) {
    return i ? TransferDirection::Right : TransferDirection::Left;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}
} // namespace

TransferComponentHost::TransferComponentHost(WindowComponentServices& v) : services_(&v) {
    v.attach(*this);
}

TransferComponentHost::~TransferComponentHost() {
    services_->detach(*this);
}

void* TransferComponentHost::begin_mount() noexcept {
    auto* old = active_transfer;
    active_transfer = this;
    return old;
}

void TransferComponentHost::end_mount(void* p) noexcept {
    active_transfer = static_cast<TransferComponentHost*>(p);
}

void TransferComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

bool TransferComponentHost::editable(runtime::ComponentId id) const {
    auto s = state(*services_, id);
    return s && active_ && read_prop(s->props.visible_) && !read_prop(s->props.disabled_) &&
           !read_prop(s->props.read_only_) && !read_prop(s->props.loading_) &&
           services_->interactions().component_active(id);
}

void TransferComponentHost::on_window_active(bool value) {
    active_ = value;
    on_input_boundary_changed();
}

void TransferComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id);
    }
}

TransferSnapshot TransferComponentHost::snapshot(runtime::ComponentId id) const {
    auto s = state(*services_, id);
    if (!s) {
        throw std::out_of_range("Transfer is stale");
    }
    TransferSnapshot out{s->target, s->selected, {}, {}, {}, s->actions, {}, {}};
    for (const auto& r : s->rows) {
        out.rows.push_back({r->item.key, r->component, r->checkbox, r->remove, r->target, r->checked.get(), r->shown});
    }
    for (std::size_t i = 0; i < 2; ++i) {
        out.search[i] = s->panels[i].search;
        out.select_all[i] = s->panels[i].all;
        out.pages[i] = s->panels[i].page.get();
        out.scroll[i] = s->panels[i].scroll;
    }
    return out;
}

void TransferComponentHost::reconcile(runtime::ComponentId id, const std::vector<TransferItem>& items) {
    auto s = state(*services_, id);
    if (!s) {
        return;
    }
    TransferKeys keys;
    for (const auto& x : items) {
        keys.push_back(x.key);
    }
    valid_keys(keys);
    const auto old = s->rows;
    std::vector<std::shared_ptr<Row>> next;
    std::vector<runtime::ComponentId> added;
    try {
        for (const auto& item : items) {
            auto it = std::ranges::find_if(old, [&](const auto& r) {
                return r->item.key == item.key && r->item.content.has_value() == item.content.has_value();
            });
            auto r = it == old.end() ? std::make_shared<Row>() : *it;
            if (it == old.end() && !s->props.list_) {
                Content content{[&] {
                    auto& b = runtime::require_component_build_context();
                    r->component = b.mount_component<std::shared_ptr<Row>>(r);
                    const auto node = b.root(r->component);
                    services_->nodes().require(node).clip_content = true;
                    b.on_resource_cleanup(r->component, [this, node] { services_->layout().remove_layout(node); });
                    b.mount_slot(
                        r->component, Content{[&] {
                            Checkbox(CheckboxProps{}
                                         .checked(r->checked)
                                         .disabled(r->disabled)
                                         .onChange(
                                             [this, id, key = item.key](bool value) { select_keys(id, {key}, value); }),
                                     item.content
                                         ? std::optional<CheckboxLabel>{}
                                         : std::optional{CheckboxLabel{[r] { Text(TextProps{}.content(r->label)); }}});
                            r->checkbox = services_->components().children(r->component).back();
                            if (item.content) {
                                auto& build = runtime::require_component_build_context();
                                r->content = build.mount_component<int>(0);
                                const auto content_node = build.root(r->content);
                                services_->layout().set_layout(content_node, layout::BoxLayout{{}, true});
                                build.on_resource_cleanup(r->content, [this, content_node] {
                                    services_->layout().remove_layout(content_node);
                                });
                                build.mount_slot(r->content, *item.content);
                            }
                            Button(ButtonProps{}
                                       .type(ButtonType::Text)
                                       .disabled(r->remove_disabled)
                                       .wave(false)
                                       .onClick([this, id, key = item.key] { move(id, TransferDirection::Left, key); }),
                                   ButtonContent{[] { Text(u8"×"); }});
                            r->remove = services_->components().children(r->component).back();
                        }});
                    services_->layout().set_layout(
                        node,
                        layout::ComponentLayout{
                            [](layout::LayoutEngine&, runtime::NodeId, layout::Constraints c) {
                                return c.constrain({200, 32});
                            },
                            [this, weak = std::weak_ptr{r}](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect) {
                                auto r = weak.lock();
                                if (!r) {
                                    return;
                                }
                                auto rect = r->full;
                                static_cast<void>(e.measure_child(services_->components().root(r->checkbox),
                                                                  {0, rect.width, 0, rect.height}));
                                e.place_child(services_->components().root(r->checkbox),
                                              {rect.x + 4, rect.y,
                                               r->content.valid() ? 28 : std::max(0.0F, rect.width - 36), rect.height});
                                if (r->content.valid()) {
                                    static_cast<void>(
                                        e.measure_child(services_->components().root(r->content),
                                                        {0, std::max(0.0F, rect.width - 64), 0, rect.height}));
                                    e.place_child(services_->components().root(r->content),
                                                  {rect.x + 32, rect.y, std::max(0.0F, rect.width - 64), rect.height});
                                }
                                static_cast<void>(e.measure_child(services_->components().root(r->remove),
                                                                  {28, 28, rect.height, rect.height}));
                                e.place_child(services_->components().root(r->remove),
                                              {rect.x + rect.width - 28, rect.y, 28, rect.height});
                            }});
                }};
                if (s->mounting) {
                    runtime::require_component_build_context().mount_slot(id, content);
                } else {
                    services_->append_slot(id, content);
                }
                added.push_back(r->component);
            }
            auto candidate = std::make_shared<Row>(*r);
            candidate->item = item;
            next.push_back(candidate);
        }
    } catch (...) {
        for (auto x : added) {
            services_->destroy(x);
        }
        throw;
    }
    for (const auto& r : old) {
        if (r->component.valid() &&
            std::ranges::none_of(next, [&](const auto& n) { return n->component == r->component; })) {
            services_->destroy(r->component);
        }
    }
    s->rows = std::move(next);
    if (!s->props.selected_) {
        std::erase_if(s->selected, [&](const auto& k) { return !has(keys, k); });
    }
    if (!s->props.target_) {
        std::erase_if(s->target, [&](const auto& k) { return !has(keys, k); });
    }
    refresh(id);
}

void TransferComponentHost::refresh(runtime::ComponentId id) {
    auto s = state(*services_, id);
    if (!s || !s->ready || s->refreshing) {
        return;
    }
    s->refreshing = true;

    struct Guard {
        S s;

        ~Guard() {
            s->refreshing = false;
        }
    } guard{s};

    services_->components().set_branch_active(id, read_prop(s->props.visible_));
    services_->set_input_blocked(id, !active_ || !read_prop(s->props.visible_) || read_prop(s->props.disabled_) ||
                                         read_prop(s->props.read_only_) || read_prop(s->props.loading_));
    const bool enabled = editable(id);
    s->disabled.set(!enabled);
    const auto pagination = read_prop(s->props.pagination_);
    if (!pagination.page_size) {
        throw std::invalid_argument("Transfer page size must be positive");
    }
    std::array<std::vector<std::shared_ptr<Row>>, 2> lists;
    for (auto& r : s->rows) {
        r->target = has(s->target, r->item.key);
        auto& panel = s->panels[r->target];
        r->filtered = panel.query.empty() ||
                      (s->props.filter_ ? s->props.filter_(panel.query, r->item, side(r->target))
                                        : r->item.title.bytes().find(panel.query.bytes()) != std::string_view::npos);
        if (!state(*services_, id)) {
            return;
        }
        if (r->filtered) {
            lists[r->target].push_back(r);
        }
        r->checked.set(has(s->selected, r->item.key));
        r->disabled.set(!enabled || r->item.disabled);
        r->remove_disabled.set(!enabled || r->item.disabled || !r->target || !read_prop(s->props.one_way_));
        r->label.set(s->props.render_ ? s->props.render_(r->item) : r->item.title);
        if (!state(*services_, id)) {
            return;
        }
        if (r->remove.valid()) {
            services_->components().set_branch_active(r->remove, r->target && read_prop(s->props.one_way_));
        }
    }
    for (std::size_t i = 0; i < 2; ++i) {
        auto& p = s->panels[i];
        auto& list = lists[i];
        if (i) {
            std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
                return std::ranges::find(s->target, a->item.key) < std::ranges::find(s->target, b->item.key);
            });
        }
        const auto page_size = p.page_size.get();
        const auto pages = std::max(std::size_t{1}, (list.size() + page_size - 1) / page_size);
        if (p.page.get() > pages) {
            p.page.set(pages);
        }
        TransferKeys selected;
        std::vector<TransferItem> filtered;
        std::size_t selectable = 0;
        std::size_t checked = 0;
        for (std::size_t j = 0; j < list.size(); ++j) {
            auto& r = list[j];
            r->order = j;
            r->shown = !s->props.list_ && (!pagination.enabled || (j / page_size + 1 == p.page.get()));
            filtered.push_back(r->item);
            if (has(s->selected, r->item.key)) {
                selected.push_back(r->item.key);
            }
            if (!r->item.disabled) {
                ++selectable;
                checked += has(s->selected, r->item.key);
            }
        }
        for (auto& r : s->rows) {
            if (r->target == (i != 0) && !r->filtered) {
                r->shown = false;
            }
            if (r->target == (i != 0) && r->component.valid()) {
                services_->components().set_branch_active(r->component, r->shown);
                services_->nodes().require(services_->components().root(r->component)).out_of_flow = !r->shown;
            }
        }
        services_->components().set_branch_active(p.search, read_prop(s->props.show_search_));
        services_->nodes().require(services_->components().root(p.search)).out_of_flow =
            !read_prop(s->props.show_search_);
        services_->components().set_branch_active(p.all, read_prop(s->props.select_all_));
        p.count = list.size();
        p.checked.set(selectable && checked == selectable);
        p.indeterminate.set(checked && checked < selectable);
        p.disabled.set(!enabled || !selectable);
        p.total.set(list.size());
        p.filtered.set(filtered);
        p.selected.set(selected);
        if (!state(*services_, id)) {
            return;
        }
        p.summary.set(String::from_utf8(std::to_string(selected.size()) + " / " + std::to_string(list.size())).value());
        p.empty_text.set(read_prop(s->props.loading_) ? String{u8"Loading…"} : read_prop(s->props.empty_));
        services_->components().set_branch_active(p.empty, list.empty() || read_prop(s->props.loading_));
        if (p.pagination.valid()) {
            services_->components().set_branch_active(p.pagination, pagination.enabled && !s->props.list_);
        }
        if (p.size_changer.valid()) {
            services_->components().set_branch_active(
                p.size_changer, pagination.enabled && pagination.show_size_changer && !s->props.list_);
        }
        s->move_disabled[i].set(!enabled || (i == 0 && read_prop(s->props.one_way_)) ||
                                std::ranges::none_of(s->rows, [&](const auto& r) {
                                    return r->target == (i == 0) && !r->item.disabled && has(s->selected, r->item.key);
                                }));
    }
    services_->components().set_branch_active(s->actions[0], !read_prop(s->props.one_way_));
    services_->mark_scene_structure_dirty();
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                               runtime::DirtyFlags::HitTest);
}

void TransferComponentHost::select_keys(runtime::ComponentId id, TransferKeys keys, bool selected) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    auto next = s->selected;
    for (const auto& key : keys) {
        auto it = std::ranges::find_if(s->rows, [&](const auto& r) { return r->item.key == key; });
        if (it == s->rows.end() || (*it)->item.disabled) {
            continue;
        }
        std::erase(next, key);
        if (selected) {
            next.push_back(key);
        }
    }
    if (next == s->selected) {
        return;
    }
    if (!s->props.selected_) {
        s->selected = next;
    }
    TransferSelectInfo info;
    for (const auto& k : next) {
        (has(s->target, k) ? info.target_selected_keys : info.source_selected_keys).push_back(k);
    }
    refresh(id);
    auto cb = s->props.on_select_;
    if (cb && state(*services_, id)) {
        cb(std::move(info));
    }
}

void TransferComponentHost::select(runtime::ComponentId id, TransferDirection direction, TransferSelection action) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    TransferKeys add;
    TransferKeys remove;
    for (const auto& r : s->rows) {
        if (r->target != (direction == TransferDirection::Right) || r->item.disabled || !r->filtered ||
            (action == TransferSelection::Current && !r->shown)) {
            continue;
        }
        if (action == TransferSelection::Clear ||
            (action == TransferSelection::Invert && has(s->selected, r->item.key))) {
            remove.push_back(r->item.key);
        } else {
            add.push_back(r->item.key);
        }
    }
    auto next = s->selected;
    for (const auto& k : remove) {
        std::erase(next, k);
    }
    for (const auto& k : add) {
        if (!has(next, k)) {
            next.push_back(k);
        }
    }
    if (next == s->selected) {
        return;
    }
    TransferSelectInfo info;
    for (const auto& k : next) {
        (has(s->target, k) ? info.target_selected_keys : info.source_selected_keys).push_back(k);
    }
    if (!s->props.selected_) {
        s->selected = next;
    }
    refresh(id);
    auto cb = s->props.on_select_;
    if (cb && state(*services_, id)) {
        cb(info);
    }
}

void TransferComponentHost::move(runtime::ComponentId id, TransferDirection direction, std::optional<TransferKey> key) {
    auto s = state(*services_, id);
    if (!s || !editable(id) || (direction == TransferDirection::Left && read_prop(s->props.one_way_) && !key)) {
        return;
    }
    TransferKeys moved;
    const auto candidates = key ? TransferKeys{*key} : s->selected;
    for (const auto& candidate : candidates) {
        const auto it = std::ranges::find_if(s->rows, [&](const auto& r) { return r->item.key == candidate; });
        if (it != s->rows.end() && !(*it)->item.disabled && (*it)->target == (direction == TransferDirection::Left)) {
            moved.push_back(candidate);
        }
    }
    if (moved.empty()) {
        return;
    }
    auto target = s->target;
    for (const auto& k : moved) {
        std::erase(target, k);
    }
    if (direction == TransferDirection::Right) {
        target.insert(target.begin(), moved.begin(), moved.end());
    }
    auto selected = s->selected;
    for (const auto& k : moved) {
        std::erase(selected, k);
    }
    TransferSelectInfo selection;
    for (const auto& k : selected) {
        (has(s->target, k) ? selection.target_selected_keys : selection.source_selected_keys).push_back(k);
    }
    if (!s->props.target_) {
        s->target = target;
    }
    if (!s->props.selected_) {
        s->selected = selected;
    }
    refresh(id);
    const auto select_callback = s->props.on_select_;
    if (select_callback && state(*services_, id)) {
        select_callback(selection);
    }
    const auto cb = s->props.on_change_;
    if (cb && state(*services_, id)) {
        cb({target, direction, moved});
    }
}

void TransferComponentHost::mount(const TransferProps& props) {
    if ((props.target_ && props.default_target_) || (props.selected_ && props.default_selected_)) {
        throw std::invalid_argument("Transfer controlled/default conflict");
    }
    const auto check_geometry = [](float x) {
        if (!std::isfinite(x) || x <= 0) {
            throw std::invalid_argument("Transfer list geometry must be finite and positive");
        }
    };
    check_geometry(read_prop(props.height_));
    check_geometry(read_prop(props.width_));
    if (!read_prop(props.pagination_).page_size) {
        throw std::invalid_argument("Transfer page size must be positive");
    }
    auto s = std::make_shared<State>();
    s->props = props;
    s->target = props.target_ ? read_prop(*props.target_) : props.default_target_.value_or(TransferKeys{});
    s->selected = props.selected_ ? read_prop(*props.selected_) : props.default_selected_.value_or(TransferKeys{});
    valid_keys(s->target);
    valid_keys(s->selected);
    TransferKeys keys;
    for (const auto& x : read_prop(props.data_)) {
        keys.push_back(x.key);
    }
    valid_keys(keys);
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<S>(s);
    s->node = b.root(id);

    struct Transaction {
        WindowComponentServices& v;
        runtime::ComponentId id;
        bool done{};

        ~Transaction() {
            if (!done) {
                v.retire_failed_component(id);
            }
        }
    } transaction{*services_, id};

    b.on_resource_cleanup(id, [this, s] {
        s->bindings.dispose();
        services_->layout().remove_layout(s->node);
    });
    runtime::connect_layout_style(s->bindings, props.layout_, s->node, services_->nodes(), services_->dirty());
    b.mount_slot(
        id, Content{[&] {
            const auto child = [&](auto fn) {
                const auto before = services_->components().children(id).size();
                fn();
                return services_->components().children(id).at(before);
            };
            const auto wrapper = [&](auto slot) {
                auto& build = runtime::require_component_build_context();
                const auto component = build.mount_component<int>(0);
                const auto node = build.root(component);
                services_->layout().set_layout(node, layout::BoxLayout{{}, true});
                services_->nodes().require(node).clip_content = true;
                build.on_resource_cleanup(component, [this, node] { services_->layout().remove_layout(node); });
                build.mount_slot(component, slot);
                return component;
            };
            for (std::size_t i = 0; i < 2; ++i) {
                auto& p = s->panels[i];
                p.page_size.set(read_prop(props.pagination_).page_size);
                if (props.title_slots_[i]) {
                    p.title = wrapper(*props.title_slots_[i]);
                } else {
                    p.title = child([&] { Text(TextProps{}.content(props.titles_[i])); });
                }
                p.all = child([&] {
                    Checkbox(CheckboxProps{}
                                 .checked(p.checked)
                                 .indeterminate(p.indeterminate)
                                 .disabled(p.disabled)
                                 .onChange([this, id, i](bool value) {
                                     select(id, side(i), value ? TransferSelection::All : TransferSelection::Clear);
                                 }),
                             CheckboxLabel{[summary = p.summary] { Text(TextProps{}.content(summary)); }});
                });
                p.invert = child([&] {
                    Button(ButtonProps{}
                               .type(ButtonType::Text)
                               .disabled(p.disabled)
                               .wave(false)
                               .size(props.size_)
                               .onClick([this, id, i] { select(id, side(i), TransferSelection::Invert); }),
                           ButtonContent{[] { Text(u8"Invert"); }});
                });
                p.clear = child([&] {
                    Button(ButtonProps{}
                               .type(ButtonType::Text)
                               .disabled(p.disabled)
                               .wave(false)
                               .size(props.size_)
                               .onClick([this, id, i] { select(id, side(i), TransferSelection::Clear); }),
                           ButtonContent{[] { Text(u8"Clear"); }});
                });
                p.search = child([&] {
                    Input(InputProps{}
                              .disabled(s->disabled)
                              .status(props.status_)
                              .size(props.size_)
                              .direction(props.direction_)
                              .placeholder(props.search_placeholder_)
                              .allowClear(true)
                              .onChange([this, id, i](String query) {
                                  auto s = state(*services_, id);
                                  if (!s || !editable(id)) {
                                      return;
                                  }
                                  auto& p = s->panels[i];
                                  p.query = query;
                                  p.scroll = 0;
                                  p.page.set(1);
                                  refresh(id);
                                  auto cb = s->props.on_search_;
                                  if (cb && state(*services_, id)) {
                                      cb(side(i), query);
                                  }
                              }));
                });
                p.empty = child([&] { Text(TextProps{}.content(p.empty_text)); });
                p.pagination = child([&] {
                    Pagination(PaginationProps{}
                                   .total(p.total)
                                   .current(p.page)
                                   .pageSize(p.page_size)
                                   .disabled(s->disabled)
                                   .size(ControlSize::Small)
                                   .onChange([this, id, i](std::size_t page, std::size_t) {
                                       if (auto s = state(*services_, id); s && editable(id)) {
                                           s->panels[i].page.set(page);
                                           s->panels[i].scroll = 0;
                                           refresh(id);
                                       }
                                   }));
                });
                p.size_changer = child([&] {
                    Button(
                        ButtonProps{}.type(ButtonType::Text).disabled(s->disabled).wave(false).onClick([this, id, i] {
                            if (auto s = state(*services_, id); s && editable(id)) {
                                auto& p = s->panels[i];
                                p.page_size.set(p.page_size.get() == 10 ? 20 : p.page_size.get() == 20 ? 50 : 10);
                                p.page.set(1);
                                p.scroll = 0;
                                refresh(id);
                            }
                        }),
                        ButtonContent{[] { Text(u8"10/20/50"); }});
                });
                if (props.footers_[i]) {
                    p.footer = wrapper(*props.footers_[i]);
                }
                if (props.list_) {
                    TransferListContext context{
                        side(i),
                        p.filtered,
                        p.selected,
                        s->disabled,
                        [this, id, i, weak = std::weak_ptr{s}](TransferKey key, bool value) {
                            const auto current = weak.lock();
                            if (!current || current->owner_thread != std::this_thread::get_id()) {
                                return;
                            }
                            if (has(current->target, key) == (i != 0)) {
                                select_keys(id, {key}, value);
                            }
                        },
                        [this, id, i, weak = std::weak_ptr{s}](TransferKeys keys, bool value) {
                            const auto current = weak.lock();
                            if (!current || current->owner_thread != std::this_thread::get_id()) {
                                return;
                            }
                            std::erase_if(keys, [&](const auto& key) { return has(current->target, key) != (i != 0); });
                            select_keys(id, std::move(keys), value);
                        }};
                    p.custom = wrapper(props.list_(std::move(context)));
                }
                s->actions[i] = child([&] {
                    Button(ButtonProps{}
                               .disabled(s->move_disabled[i])
                               .size(props.size_)
                               .wave(false)
                               .onClick([this, id, i] { move(id, side(i)); }),
                           ButtonContent{[i] { Text(i ? u8"→" : u8"←"); }});
                });
            }
        }});
    services_->layout().set_layout(
        s->node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) {
                auto s = state(*services_, id);
                if (!s) {
                    return runtime::Size{};
                }
                const float width = read_prop(s->props.width_) * 2 + 56;
                const float height = read_prop(s->props.height_) + 96 + (read_prop(s->props.show_search_) ? 36 : 0) +
                                     (read_prop(s->props.pagination_).enabled ? 64 : 0);
                for (auto child : services_->components().children(id)) {
                    static_cast<void>(e.measure_child(services_->components().root(child), {0, width, 0, height}));
                }
                return c.constrain({width, height});
            },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect rect) {
                auto s = state(*services_, id);
                if (!s) {
                    return;
                }
                const float width = std::max(0.0F, (rect.width - 56) / 2);
                const float row_height = services_->components().theme_scope(id)->snapshot().menu().metrics.item_height;
                const bool rtl = read_prop(s->props.direction_) == TextDirection::RightToLeft;
                const bool search = read_prop(s->props.show_search_);
                const auto pagination = read_prop(s->props.pagination_);
                for (std::size_t i = 0; i < 2; ++i) {
                    auto& p = s->panels[i];
                    const float x = rect.x + ((i != 0) != rtl ? width + 56 : 0);
                    const float top = rect.y + 64 + (search ? 36 : 0);
                    p.viewport = {x, top, width, read_prop(s->props.height_)};
                    p.scroll = std::clamp(
                        p.scroll, 0.0F,
                        std::max(0.0F, static_cast<float>(pagination.enabled ? std::min(p.count, p.page_size.get())
                                                                             : p.count) *
                                               row_height -
                                           p.viewport.height));
                    const auto place = [&](runtime::ComponentId child, runtime::Rect r) {
                        if (child.valid()) {
                            e.place_child(services_->components().root(child), r);
                        }
                    };
                    place(p.title, {x, rect.y, width, 28});
                    place(p.all, {x + 4, rect.y + 28, std::max(0.0F, width - 112), 32});
                    place(p.invert, {x + width - 108, rect.y + 28, 60, 32});
                    place(p.clear, {x + width - 48, rect.y + 28, 48, 32});
                    if (search) {
                        place(p.search, {x, rect.y + 64, width, 32});
                    }
                    place(p.empty, {x + 8, top, width - 16, 32});
                    if (p.custom.valid()) {
                        place(p.custom, p.viewport);
                    }
                    for (const auto& r : s->rows) {
                        if (r->target != (i != 0) || !r->shown || !r->component.valid()) {
                            continue;
                        }
                        const auto order = pagination.enabled ? r->order % p.page_size.get() : r->order;
                        r->full = {x, top + static_cast<float>(order) * row_height - p.scroll, width, row_height};
                        // Row owner clips its native controls to this list viewport, so
                        // fractional scrolling cannot hit the search/header underneath.
                        // Its retained subtree does not change when crossing lists.
                        auto* record = services_->components().state<std::shared_ptr<Row>>(r->component);
                        if (record) {
                            (*record)->full = r->full;
                        }
                        e.place_child(services_->components().root(r->component), intersect(r->full, p.viewport), true,
                                      true);
                    }
                    if (pagination.enabled && !s->props.list_) {
                        place(p.pagination, {x, top + p.viewport.height, width, 32});
                        if (pagination.show_size_changer) {
                            place(p.size_changer, {x, top + p.viewport.height + 32, width, 28});
                        }
                    }
                    if (p.footer.valid()) {
                        place(p.footer, {x, top + p.viewport.height + (pagination.enabled ? 64 : 0), width, 32});
                    }
                    place(s->actions[i], {rect.x + width + 8, top + 24 + static_cast<float>(i) * 40, 40, 32});
                }
            }});
    s->ready = true;
    reconcile(id, read_prop(props.data_));
    connect_prop(
        s->bindings, props.data_, [this, id](auto items) { reconcile(id, items); },
        [](const auto&, const auto&) { return false; });
    if (props.target_) {
        connect_prop(s->bindings, *props.target_, [this, id](TransferKeys keys) {
            valid_keys(keys);
            if (auto s = state(*services_, id)) {
                s->target = std::move(keys);
                refresh(id);
            }
        });
    }
    if (props.selected_) {
        connect_prop(s->bindings, *props.selected_, [this, id](TransferKeys keys) {
            valid_keys(keys);
            if (auto s = state(*services_, id)) {
                s->selected = std::move(keys);
                refresh(id);
            }
        });
    }
    connect_prop(s->bindings, props.pagination_, [this, id](TransferPagination value) {
        if (!value.page_size) {
            throw std::invalid_argument("Transfer page size must be positive");
        }
        if (auto s = state(*services_, id)) {
            for (auto& p : s->panels) {
                p.page_size.set(value.page_size);
                p.page.set(1);
            }
            refresh(id);
        }
    });
    const auto watch = [&]<class T>(const Prop<T>& p) {
        connect_prop(s->bindings, p, [this, id](const auto&) { refresh(id); });
    };
    watch(props.disabled_);
    watch(props.read_only_);
    watch(props.visible_);
    watch(props.loading_);
    watch(props.show_search_);
    watch(props.one_way_);
    watch(props.select_all_);
    watch(props.direction_);
    for (std::size_t i = 0; i < 2; ++i) {
        services_->components().set_branch_active(s->panels[i].search, read_prop(props.show_search_));
        services_->components().set_branch_active(s->panels[i].all, read_prop(props.select_all_));
    }
    mounted_.push_back(id);
    refresh(id);
    s->mounting = false;
    transaction.done = true;
}

bool TransferComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (!std::isfinite(event.delta_y)) {
        return false;
    }
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    for (auto id : mounted_) {
        auto s = state(*services_, id);
        if (!s || !editable(id)) {
            continue;
        }
        auto row = std::ranges::find_if(s->rows, [&](const auto& r) { return r->checkbox == target->component; });
        if (row == s->rows.end()) {
            continue;
        }
        const std::size_t i = (*row)->target;
        auto& p = s->panels[i];
        auto pagination = read_prop(s->props.pagination_);
        const float height = services_->components().theme_scope(id)->snapshot().menu().metrics.item_height;
        const float max = std::max(
            0.0F, static_cast<float>(pagination.enabled ? std::min(p.count, p.page_size.get()) : p.count) * height -
                      p.viewport.height);
        const float previous = p.scroll;
        p.scroll = std::clamp(previous - event.delta_y * 96, 0.0F, max);
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
        auto cb = s->props.on_scroll_;
        if (cb && previous != p.scroll) {
            cb({side(i), p.scroll, event.delta_y});
        }
        return true;
    }
    return false;
}
} // namespace ryn::detail

namespace ryn {
void Transfer(TransferProps props) {
    if (!detail::active_transfer) {
        throw std::logic_error("Transfer requires WindowComponentServices");
    }
    detail::active_transfer->mount(props);
}
} // namespace ryn
