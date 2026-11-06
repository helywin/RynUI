#include "component/transfer_component.hpp"
#include "component/localized_prop.hpp"
#include "component/form_control_binding.hpp"
#include "component/input_component.hpp"
#include "component/selection_component.hpp"
#include "component/pagination_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "reactive/untracked.hpp"
#include <ryn/button.hpp>
#include <algorithm>
#include <cmath>
#include <thread>
#include <exception>
#include <limits>
#include <memory>

namespace ryn::detail {
namespace {
thread_local TransferComponentHost* active_transfer{};

struct Row final {
    TransferItem item;
    runtime::ComponentId component;
    runtime::ComponentId checkbox;
    runtime::ComponentId remove;
    runtime::ComponentId content;
    runtime::ComponentId plain;
    runtime::Rect full;
    Signal<String> label{String{}};
    Signal<bool> checked{false};
    Signal<bool> disabled{false};
    Signal<bool> remove_disabled{true};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<String> remove_label{String{}};
    bool rtl{};
    bool remove_shown{};
    bool target{};
    bool filtered{};
    bool shown{};
    std::size_t order{};
};

struct Panel final {
    runtime::ComponentId title;
    runtime::ComponentId current;
    runtime::ComponentId remove_all;
    runtime::ComponentId remove_current;
    runtime::ComponentId icon;
    Signal<String> title_text{String{}};
    Signal<String> invert_text{String{}};
    Signal<String> clear_text{String{}};
    Signal<String> current_text{String{}};
    Signal<String> remove_all_text{String{}};
    Signal<String> remove_current_text{String{}};
    Signal<String> placeholder{String{}};
    Signal<String> query_value{String{}};
    Signal<std::size_t> selected_count{0};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    runtime::ComponentId search;
    runtime::ComponentId all;
    runtime::ComponentId count_label;
    runtime::ComponentId invert;
    runtime::ComponentId clear;
    runtime::ComponentId empty;
    runtime::ComponentId pagination;
    runtime::ComponentId size_changer;
    runtime::ComponentId footer;
    runtime::ComponentId custom;
    runtime::Rect viewport;
    runtime::Rect section;
    Signal<String> summary{String{}};
    Signal<String> count_text{String{}};
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
    std::optional<TransferKey> anchor;
};

struct State final {
    TransferProps props;
    Scope bindings;
    runtime::NodeId node;
    std::vector<std::shared_ptr<Row>> rows;
    std::array<Panel, 2> panels;
    std::array<runtime::ComponentId, 2> actions;
    std::vector<runtime::ComponentId> additional_actions;
    std::array<Signal<bool>, 2> move_disabled{Signal<bool>{true}, Signal<bool>{true}};
    Signal<bool> disabled{false};
    Signal<ThemeConfig> child_theme{ThemeConfig{}};
    Signal<TransferLocale> locale{TransferLocale{}};
    Prop<TransferLocale> locale_source{TransferLocale{}};
    Prop<IntegerPluralRule> plural_source{IntegerPluralRule::EnglishOneOther};
    std::uint64_t locale_revision{};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    std::array<Signal<String>, 2> action_label{Signal<String>{String{}}, Signal<String>{String{}}};
    std::array<Signal<String>, 2> action_icon{Signal<String>{String{}}, Signal<String>{String{}}};
    runtime::ComponentId body;
    component::RetainedSurfaceId surface;
    std::array<theme_runtime::Subscription, 5> subscriptions;
    TransferThemeToken token;
    TransferPagination pagination;
    ControlSize size{ControlSize::Middle};
    float width{};
    float height{};
    float action_width{};
    float row_height{};
    float spacing{};
    float total_height{};
    bool rtl{};
    bool zero_area{};
    bool range_select{};
    bool search{};
    bool alive{true};
    bool again{};
    TransferKeys target;
    TransferKeys selected;
    std::thread::id owner_thread{std::this_thread::get_id()};
    bool ready{};
    bool mounting{true};
    bool refreshing{};
    bool loading{};
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

// A user binding/factory/handler may throw after changing a nested control. Retire
// this generation before an enclosing host can publish a partially updated scene.
struct FailureGuard final {
    WindowComponentServices* services;
    runtime::ComponentId id;
    std::weak_ptr<State> ticket;
    int exceptions{std::uncaught_exceptions()};

    FailureGuard(WindowComponentServices* value, runtime::ComponentId component)
        : services(value), id(component), ticket(state(*value, component)) {}

    ~FailureGuard() {
        const auto current = ticket.lock();
        if (current && current->alive && std::uncaught_exceptions() > exceptions) {
            services->retire_failed_component(id);
        }
    }
};

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<ThemeConfig> child_theme(const TransferThemeToken& t) {
    auto result = std::make_unique<ThemeConfig>();
    auto& c = *result;
    c.text.tokens.color = t.colors.text;
    c.text.tokens.font_size = dp(t.typography.font_size);
    c.text.tokens.line_height = dp(t.typography.line_height);
    c.text.tokens.font_family = t.typography.font_family;
    c.alias.color_text = t.colors.text;
    c.alias.color_text_secondary = t.colors.secondary;
    c.alias.color_text_disabled = t.colors.disabled;
    c.alias.color_background_container = t.colors.background;
    c.alias.color_border = t.colors.border;
    c.alias.color_focus_outline = t.colors.focus;
    c.button.tokens.default_color = t.colors.text;
    c.button.tokens.text_color = t.colors.text;
    c.button.tokens.text_background = t.colors.hover;
    c.button.tokens.default_background = t.colors.background;
    c.button.tokens.primary_background = t.colors.primary;
    c.button.tokens.primary_color = t.colors.primary_text;
    auto& primary = c.button.tokens.colors[static_cast<std::size_t>(ButtonColor::Primary)];
    primary.base = t.colors.primary;
    primary.hover = t.colors.primary_hover;
    primary.active = t.colors.primary_active;
    primary.solid_text = t.colors.primary_text;
    c.button.tokens.default_border_color = t.colors.border;
    c.button.seed.control_height = dp(t.metrics.control_height);
    c.button.algorithm = true;
    c.button.tokens.padding_inline = dp(4);
    c.button.tokens.border_radius = dp(t.metrics.radius);
    c.button.tokens.default_shadow = ShadowList{};
    c.button.seed.font_size = dp(t.typography.font_size);
    c.checkbox.tokens.font_size = dp(t.typography.font_size);
    c.checkbox.tokens.line_height = dp(t.typography.line_height);
    c.checkbox.tokens.label = t.colors.text;
    c.checkbox.tokens.background = t.colors.background;
    c.checkbox.tokens.primary = t.colors.primary;
    c.checkbox.tokens.primary_hover = t.colors.primary_hover;
    c.checkbox.tokens.primary_active = t.colors.primary_active;
    c.checkbox.tokens.disabled_background = t.colors.disabled_background;
    c.checkbox.tokens.disabled_foreground = t.colors.disabled;
    c.checkbox.tokens.border = t.colors.border;
    c.checkbox.tokens.focus = t.colors.focus;
    c.checkbox.tokens.focus_width = dp(t.effects.focus_width);
    c.checkbox.tokens.focus_offset = dp(t.effects.focus_offset);
    c.input.tokens.background = t.colors.background;
    c.input.tokens.color = t.colors.text;
    c.input.tokens.placeholder_color = t.colors.secondary;
    c.input.tokens.border_color = t.colors.border;
    c.input.tokens.active_border_color = t.colors.primary;
    c.input.tokens.input_font_size = dp(t.typography.font_size);
    c.input.tokens.input_font_size_small = dp(t.typography.font_size);
    c.input.tokens.input_font_size_large = dp(t.typography.font_size);
    c.input.tokens.focus_width = dp(t.effects.focus_width);
    c.input.tokens.hover_border_color = t.colors.primary_hover;
    c.pagination.tokens.active = t.colors.primary;
    c.pagination.tokens.border = t.colors.primary;
    c.pagination.tokens.item_background = t.colors.background;
    c.pagination.tokens.active_background = t.colors.background;
    c.pagination.tokens.item = t.colors.text;
    c.pagination.tokens.disabled = t.colors.disabled;
    c.select.tokens.active_border_color = t.colors.primary;
    c.select.tokens.hover_border_color = t.colors.primary_hover;
    c.select.tokens.option_selected_background = t.colors.selected;
    c.select.tokens.option_active_background = t.colors.hover;
    c.select.tokens.option_color = t.colors.text;
    c.select.tokens.option_disabled_color = t.colors.disabled;
    return result;
}

void publish_theme(Signal<ThemeConfig> target, std::unique_ptr<ThemeConfig> config) {
    target.set(std::move(*config));
}

TransferDirection side(std::size_t i) {
    return i ? TransferDirection::Right : TransferDirection::Left;
}

String count_text(const TransferLocale& locale, std::size_t selected, std::size_t total, bool explicit_locale,
                  IntegerPluralRule rule) {
    // Preserve the established explicit Locale unit-field contract. Inherited packs
    // additionally own their complete count templates. Both paths use exact UInt args.
    if (explicit_locale) {
        static const MessageKey key{u8"ryn.transfer.legacy_count",
                                    String{u8"{selected} / {total} {unit}"},
                                    {{u8"selected", MessageArgType::UInt},
                                     {u8"total", MessageArgType::UInt},
                                     {u8"unit", MessageArgType::Text}}};
        return formatMessage(key, key.fallback(),
                             {{u8"selected", static_cast<std::uint64_t>(selected)},
                              {u8"total", static_cast<std::uint64_t>(total)},
                              {u8"unit", total == 1 ? locale.item_unit : locale.items_unit}},
                             IntegerPluralRule::EnglishOneOther);
    }
    static const MessageKey key{
        u8"ryn.transfer.count",
        PluralMessage{u8"total", String{u8"{selected}/{total} item"}, u8"{selected}/{total} items"},
        {{u8"selected", MessageArgType::UInt}, {u8"total", MessageArgType::UInt}}};
    return formatMessage(
        key, PluralMessage{u8"total", locale.count_one, locale.count_other},
        {{u8"selected", static_cast<std::uint64_t>(selected)}, {u8"total", static_cast<std::uint64_t>(total)}}, rule);
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
           !read_prop(s->props.read_only_) && !read_prop(s->props.loading_) && !s->zero_area &&
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
        out.viewports[i] = s->panels[i].viewport;
        out.sections[i] = s->panels[i].section;
        out.pagination[i] = s->panels[i].pagination;
        out.current[i] = s->panels[i].current;
        out.invert[i] = s->panels[i].invert;
        out.clear[i] = s->panels[i].clear;
        out.summaries[i] = s->panels[i].summary.get();
    }
    return out;
}

void TransferComponentHost::reconcile(runtime::ComponentId id, const std::vector<TransferItem>& input) {
    auto s = state(*services_, id);
    if (!s) {
        return;
    }
    std::vector<TransferItem> items = input;
    if (s->props.row_key_) {
        FailureGuard failure{services_, id};
        for (auto& item : items) {
            item.key = s->props.row_key_(item);
            if (!s->alive) {
                return;
            }
        }
    }
    TransferKeys keys;
    for (const auto& x : items) {
        keys.push_back(x.key);
    }
    valid_keys(keys);
    FailureGuard failure{services_, id};
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
                    b.mount_slot(r->component, Content{[&] {
                                     Theme(ThemeProps{}.config(r->theme), ThemeContent{[&] {
                                               FormPrivateEditorScope private_editor;
                                               Checkbox(CheckboxProps{}
                                                            .checked(r->checked)
                                                            .direction(bind([direction = r->direction] {
                                                                return direction.get() == TextDirection::RightToLeft
                                                                           ? CheckboxDirection::RightToLeft
                                                                           : CheckboxDirection::LeftToRight;
                                                            }))
                                                            .disabled(r->disabled)
                                                            .onChange([this, id, key = item.key](bool value) {
                                                                select_keys(id, {key}, value);
                                                            }),
                                                        item.content ? std::optional<CheckboxLabel>{}
                                                                     : std::optional{CheckboxLabel{[r] {
                                                                           Text(TextProps{}.content(r->label));
                                                                       }}});
                                               r->checkbox = services_->components().children(r->component).back();
                                               Text(TextProps{}.content(r->label).direction(r->direction));
                                               r->plain = services_->components().children(r->component).back();
                                               if (item.content) {
                                                   auto& build = runtime::require_component_build_context();
                                                   r->content = build.mount_component<int>(0);
                                                   const auto content_node = build.root(r->content);
                                                   services_->layout().set_layout(content_node,
                                                                                  layout::BoxLayout{{}, true});
                                                   build.on_resource_cleanup(r->content, [this, content_node] {
                                                       services_->layout().remove_layout(content_node);
                                                   });
                                                   build.mount_slot(r->content, *item.content);
                                               }
                                               Button(ButtonProps{}
                                                          .type(ButtonType::Text)
                                                          .disabled(r->remove_disabled)
                                                          .wave(false)
                                                          .onClick([this, id, key = item.key] {
                                                              move(id, TransferDirection::Left, key);
                                                          }),
                                                      ButtonContent{[label = r->remove_label] {
                                                          Text(TextProps{}.content(label));
                                                      }});
                                               r->remove = services_->components().children(r->component).back();
                                           }});
                                 }});
                    b.on_resource_cleanup(r->component, [this, component = r->component, checkbox = r->checkbox,
                                                         plain = r->plain, remove = r->remove] {
                        for (const auto owned : {component, checkbox, plain, remove}) {
                            services_->interactions().set_input_blocked(owned, false);
                        }
                    });
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
                                const float remove = r->remove_shown ? std::min(72.0F, rect.width / 3) : 0;
                                const float check_width = r->remove_shown      ? 0
                                                          : r->content.valid() ? std::min(28.0F, rect.width)
                                                                               : std::max(0.0F, rect.width - remove);
                                const float check_x = r->rtl ? rect.x + rect.width - check_width : rect.x;
                                const auto place = [&](runtime::ComponentId child, runtime::Rect bounds) {
                                    static_cast<void>(e.measure_child(services_->components().root(child),
                                                                      {0, bounds.width, 0, bounds.height}));
                                    e.place_child(services_->components().root(child), bounds);
                                };
                                if (!r->remove_shown) {
                                    place(r->checkbox, {check_x, rect.y, check_width, rect.height});
                                }
                                if (r->remove_shown && !r->content.valid()) {
                                    place(r->plain, {r->rtl ? rect.x + remove : rect.x, rect.y,
                                                     std::max(0.0F, rect.width - remove), rect.height});
                                }
                                if (r->content.valid()) {
                                    const float content_width = std::max(0.0F, rect.width - check_width - remove);
                                    place(r->content, {r->rtl ? rect.x + remove : rect.x + check_width, rect.y,
                                                       content_width, rect.height});
                                }
                                if (r->remove_shown) {
                                    place(r->remove, {r->rtl ? rect.x : rect.x + rect.width - remove, rect.y, remove,
                                                      rect.height});
                                }
                            }});
                }};
                if (s->mounting) {
                    runtime::require_component_build_context().mount_slot(s->body, content);
                } else {
                    services_->append_slot(s->body, content);
                }
                if (!s->alive) {
                    return;
                }
                added.push_back(r->component);
            }
            if (!s->alive) {
                return;
            }
            r->item = item;
            next.push_back(r);
        }
    } catch (...) {
        if (s->alive) {
            for (auto x : added) {
                services_->destroy(x);
            }
        }
        throw;
    }
    if (!s->props.selected_) {
        std::erase_if(s->selected, [&](const auto& k) { return !has(keys, k); });
    }
    if (!s->props.target_) {
        std::erase_if(s->target, [&](const auto& k) { return !has(keys, k); });
    }
    for (const auto& requested : {s->target, s->selected}) {
        if (std::ranges::any_of(requested, [&](const auto& key) { return !has(keys, key); })) {
            throw std::invalid_argument("Transfer requested key is absent from dataSource");
        }
    }
    // Publish the complete row/key authority before child cleanup can reconcile input boundaries.
    s->rows = std::move(next);
    for (const auto& r : old) {
        if (r->component.valid() &&
            std::ranges::none_of(s->rows, [&](const auto& n) { return n->component == r->component; })) {
            services_->destroy(r->component);
            if (!s->alive) {
                return;
            }
        }
    }
    refresh(id);
}

void TransferComponentHost::refresh(runtime::ComponentId id) {
    auto s = state(*services_, id);
    if (!s || !s->ready || !s->alive) {
        return;
    }
    if (s->refreshing) {
        s->again = true;
        return;
    }
    refresh_content(id);
}

void TransferComponentHost::localize(runtime::ComponentId id, TransferLocale locale, IntegerPluralRule rule) {
    run_untracked([&] {
        const auto s = state(*services_, id);
        if (!s || !s->alive || !services_->components().scope(id).active()) {
            return;
        }
        const auto revision = ++s->locale_revision;
        const auto current = [&] {
            return state(*services_, id) == s && s->alive && s->locale_revision == revision &&
                   services_->components().scope(id).active();
        };
        // Prepare only text from retained projection results. No render/filter/getter,
        // page clamping, input boundary, reconciliation or slot factory runs here.
        std::vector<std::pair<Signal<String>, String>> updates;
        for (const auto& row : s->rows) {
            updates.emplace_back(row->remove_label, locale.remove);
        }
        for (std::size_t i = 0; i < 2; ++i) {
            const auto& panel = s->panels[i];
            const auto summary = s->props.select_all_label_
                                     ? panel.count_text.get()
                                     : count_text(locale, panel.selected_count.get(), panel.total.get(),
                                                  s->props.locale_.has_value(), rule);
            if (!s->props.select_all_label_) {
                updates.emplace_back(panel.count_text, summary);
            }
            updates.emplace_back(
                panel.summary,
                String::from_utf8(std::string((panel.checked.get() ? locale.deselect_all : locale.select_all).bytes()) +
                                  " · " + std::string(summary.bytes()))
                    .value());
            if (!s->props.titles_) {
                updates.emplace_back(panel.title_text, locale.titles[i]);
            }
            if (!s->props.search_placeholder_) {
                updates.emplace_back(panel.placeholder, locale.search_placeholder);
            }
            if (s->loading || !s->props.empty_) {
                updates.emplace_back(panel.empty_text, s->loading ? locale.loading : locale.empty[i]);
            }
            updates.emplace_back(panel.invert_text, locale.select_invert);
            updates.emplace_back(panel.clear_text, locale.clear);
            updates.emplace_back(panel.current_text, locale.select_current);
            updates.emplace_back(panel.remove_all_text, locale.remove_all);
            updates.emplace_back(panel.remove_current_text, locale.remove_current);
            if (!s->props.operations_) {
                updates.emplace_back(s->action_label[i], locale.actions[i ? 0 : 1]);
            }
        }
        auto locale_signal = s->locale;
        locale_signal.set(std::move(locale));
        if (!current()) {
            return;
        }
        for (auto& [signal, text] : updates) {
            signal.set(std::move(text));
            if (!current()) {
                return;
            }
        }
    });
}

void TransferComponentHost::refresh_content(runtime::ComponentId id) {
    // Keep the reentry check outside the frame holding complete child themes.
    auto s = state(*services_, id);
    FailureGuard failure{services_, id};
    s->refreshing = true;

    struct Guard {
        S s;

        ~Guard() {
            s->refreshing = false;
        }
    } guard{s};

    for (unsigned attempt = 0;; ++attempt) {
        if (attempt == 32) {
            throw std::logic_error("Transfer reentrant update did not settle");
        }
        s->again = false;
        const auto token = services_->components().theme_scope(id)->snapshot().transfer();
        const auto pagination = read_prop(s->props.pagination_);
        const auto size = read_prop(s->props.size_);
        const auto direction = read_prop(s->props.direction_);
        const auto spacing = read_prop(s->props.row_spacing_);
        if (!s->alive) {
            return;
        }
        const float width = s->props.width_ ? read_prop(*s->props.width_)
                            : (size == ControlSize::Large || (pagination.enabled && !s->props.list_))
                                ? token.metrics.list_width_large
                                : token.metrics.list_width;
        const float height = s->props.height_ ? read_prop(*s->props.height_) : token.metrics.list_height;
        if (!s->alive) {
            return;
        }
        const float factor = size == ControlSize::Small ? .75F : size == ControlSize::Large ? 1.25F : 1.0F;
        const float row_height = std::max(token.metrics.item_height * factor,
                                          token.typography.line_height + token.metrics.item_padding_block * 2);
        const bool search = read_prop(s->props.show_search_);
        if (!s->alive) {
            return;
        }
        const bool paging = pagination.enabled && !s->props.list_;
        const float list_total_height =
            token.metrics.header_height * 2 + token.metrics.control_height + token.metrics.gap * 2 +
            (search ? token.metrics.control_height + token.metrics.gap : 0) + height +
            (paging ? token.metrics.control_height * (pagination.show_size_changer ? 2 : 1) + token.metrics.gap : 0) +
            ((s->props.footers_[0] || s->props.footers_[1] || s->props.footer_factory_) ? token.metrics.footer_height
                                                                                        : 0);
        const float action_height = token.metrics.header_height * 2 + token.metrics.control_height + token.metrics.gap +
                                    (search ? token.metrics.control_height + token.metrics.gap : 0) +
                                    static_cast<float>(2 + s->props.additional_actions_.size()) *
                                        (token.metrics.control_height + token.metrics.gap);
        const float total_height = std::max(list_total_height, action_height);
        const double final_extent = static_cast<double>(width) * 2 + token.metrics.actions_width +
                                    token.metrics.gap * 2 + total_height +
                                    (static_cast<double>(row_height) + spacing) * static_cast<double>(s->rows.size());
        if (!std::isfinite(width) || width <= 0 || !std::isfinite(height) || height <= 0 || !std::isfinite(spacing) ||
            spacing < 0 || !std::isfinite(total_height) || !std::isfinite(final_extent) ||
            final_extent > std::numeric_limits<float>::max() / 4 || !pagination.page_size ||
            (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large) ||
            (direction != TextDirection::Auto && direction != TextDirection::LeftToRight &&
             direction != TextDirection::RightToLeft)) {
            throw std::invalid_argument("Transfer final geometry, direction, size or pagination is invalid");
        }
        for (auto option : pagination.page_size_options) {
            if (!option) {
                throw std::invalid_argument("Transfer page size option must be positive");
            }
        }
        bool geometry = s->token.metrics != token.metrics || s->token.typography != token.typography ||
                        s->width != width || s->height != height || s->row_height != row_height ||
                        s->spacing != spacing || s->search != search ||
                        s->rtl != (direction == TextDirection::RightToLeft) || s->pagination != pagination ||
                        s->total_height != total_height;
        const bool visible = read_prop(s->props.visible_);
        const bool one_way = read_prop(s->props.one_way_);
        const bool loading = read_prop(s->props.loading_);
        const auto status = read_prop(s->props.status_);
        if (!s->alive) {
            return;
        }
        if (status != InputStatus::Default && status != InputStatus::Warning && status != InputStatus::Error) {
            throw std::invalid_argument("Transfer status is invalid");
        }
        // Compute all user-supplied render/filter/label results before publishing them.
        const auto rows = s->rows;
        const auto target = s->target;
        const auto selected_keys = s->selected;
        std::array<std::vector<std::shared_ptr<Row>>, 2> lists;
        std::vector<String> labels;
        std::vector<bool> filters;
        for (const auto& r : rows) {
            const bool right = has(target, r->item.key);
            const auto query = s->panels[right].query;
            const auto label = s->props.render_ ? s->props.render_(r->item) : r->item.title;
            if (!s->alive) {
                return;
            }
            bool filtered =
                query.empty() || (s->props.filter_ ? s->props.filter_(query, r->item, side(right))
                                                   : label.bytes().find(query.bytes()) != std::string_view::npos);
            if (!s->alive) {
                return;
            }
            labels.push_back(label);
            filters.push_back(filtered);
            if (filtered) {
                lists[right].push_back(r);
            }
        }
        if (s->again) {
            continue;
        }
        std::array<String, 2> summaries;
        for (std::size_t i = 0; i < 2; ++i) {
            const auto selected = std::ranges::count_if(
                lists[i], [&](const auto& r) { return !r->item.disabled && has(selected_keys, r->item.key); });
            summaries[i] =
                s->props.select_all_label_
                    ? s->props.select_all_label_({side(i), static_cast<std::size_t>(selected), lists[i].size()})
                    : String{};
            if (!s->alive) {
                return;
            }
        }
        if (s->again) {
            continue;
        }
        s->token = token;
        s->width = width;
        s->height = height;
        s->row_height = row_height;
        s->spacing = spacing;
        s->total_height = total_height;
        s->rtl = direction == TextDirection::RightToLeft;
        s->search = search;
        const bool pagination_changed = s->pagination != pagination;
        s->pagination = pagination;
        s->size = size;
        s->loading = loading;
        const auto branch = [&](runtime::ComponentId child, bool shown, bool blocked = false) {
            if (!child.valid() || !s->alive) {
                return;
            }
            if (services_->components().set_branch_active(child, shown)) {
                services_->mark_scene_structure_dirty();
            }
            if (child == id) {
                auto& node = services_->nodes().require(s->node);
                if (node.out_of_flow != !shown) {
                    node.out_of_flow = !shown;
                    geometry = true;
                }
            }
            services_->set_input_blocked(child, !shown || blocked);
        };
        branch(id, visible,
               !active_ || s->zero_area || read_prop(s->props.disabled_) || read_prop(s->props.read_only_) || loading);
        if (!s->alive) {
            return;
        }
        const bool enabled = editable(id);
        s->disabled.set(!enabled);
        if (!enabled) {
            s->range_select = false;
        }
        auto content_theme = child_theme(token);
        if (!enabled) {
            content_theme->text.tokens.color = token.colors.disabled;
        }
        publish_theme(s->child_theme, std::move(content_theme));
        s->direction.set(direction);
        if (!s->alive) {
            return;
        }
        if (services_->components().set_window_layer(id,
                                                     token.order ? std::optional<int>{token.order} : std::nullopt)) {
            services_->mark_scene_structure_dirty();
        }
        for (std::size_t j = 0; j < rows.size(); ++j) {
            const auto& r = rows[j];
            const bool moved = r->target != has(target, r->item.key);
            geometry = geometry || moved || r->filtered != filters[j] ||
                       r->remove_shown != (has(target, r->item.key) && one_way);
            r->target = has(target, r->item.key);
            r->filtered = filters[j];
            r->checked.set(has(selected_keys, r->item.key));
            r->disabled.set(!enabled || r->item.disabled);
            r->remove_disabled.set(!enabled || r->item.disabled || !r->target || !one_way);
            r->remove_shown = r->target && one_way;
            r->rtl = s->rtl;
            r->direction.set(direction);
            auto row_theme = child_theme(token);
            row_theme->text.tokens.color = r->disabled.get() ? token.colors.disabled : token.colors.text;
            row_theme->input.tokens.background =
                r->disabled.get() ? token.colors.disabled_background : token.colors.background;
            publish_theme(r->theme, std::move(row_theme));
            r->label.set(labels[j]);
            if (!s->alive) {
                return;
            }
            if (moved && r->component.valid()) {
                // A retained editor keeps its draft, but a move ends capture/preedit in the old list.
                services_->set_input_blocked(r->component, true);
                services_->set_input_blocked(r->component, false);
            }
            branch(r->remove, r->remove_shown);
            branch(r->checkbox, !r->remove_shown);
            branch(r->plain, r->remove_shown && !r->content.valid());
        }
        for (std::size_t i = 0; i < 2; ++i) {
            auto& p = s->panels[i];
            auto& list = lists[i];
            if (i) {
                std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
                    return std::ranges::find(target, a->item.key) < std::ranges::find(target, b->item.key);
                });
            }
            if (pagination_changed) {
                p.page_size.set(pagination.page_size);
            }
            const auto page_size = p.page_size.get();
            const auto pages = std::max(std::size_t{1}, list.size() / page_size + (list.size() % page_size != 0));
            p.page.set(std::clamp(p.page.get(), std::size_t{1}, pages));
            TransferKeys selected;
            std::vector<TransferItem> filtered;
            std::size_t selectable = 0;
            std::size_t checked = 0;
            for (std::size_t j = 0; j < list.size(); ++j) {
                auto& r = list[j];
                const bool shown = !s->props.list_ && !loading && (!paging || j / page_size + 1 == p.page.get());
                geometry = geometry || r->order != j || r->shown != shown;
                r->order = j;
                r->shown = shown;
                filtered.push_back(r->item);
                if (has(selected_keys, r->item.key)) {
                    selected.push_back(r->item.key);
                }
                if (!r->item.disabled) {
                    ++selectable;
                    checked += has(selected_keys, r->item.key);
                }
            }
            for (const auto& r : rows) {
                if (r->target != (i != 0)) {
                    continue;
                }
                if (!r->filtered) {
                    geometry = geometry || r->shown;
                    r->shown = false;
                }
                branch(r->component, r->shown, !enabled || r->item.disabled);
            }
            branch(p.search, search);
            branch(p.all, read_prop(s->props.select_all_) && !(one_way && i));
            branch(p.count_label, !read_prop(s->props.select_all_) || (one_way && i));
            branch(p.invert, !one_way || !i);
            branch(p.clear, !one_way || !i);
            branch(p.current, paging && (!one_way || !i));
            branch(p.remove_all, one_way && i);
            branch(p.remove_current, one_way && i && paging);
            branch(p.custom, !loading);
            p.count = list.size();
            p.checked.set(selectable && checked == selectable);
            p.indeterminate.set(checked && checked < selectable);
            p.disabled.set(!enabled || !selectable);
            p.total.set(list.size());
            p.selected_count.set(checked);
            p.filtered.set(filtered);
            TransferKeys side_selected;
            for (const auto& key : selected_keys) {
                if (has(target, key) == (i != 0)) {
                    side_selected.push_back(key);
                }
            }
            p.selected.set(side_selected);
            if (s->props.select_all_label_) {
                p.count_text.set(summaries[i]);
            }
            if (s->props.titles_) {
                p.title_text.set(read_prop(*s->props.titles_)[i]);
            }
            if (s->props.search_placeholder_) {
                p.placeholder.set(read_prop(*s->props.search_placeholder_));
            }
            if (!loading && s->props.empty_) {
                p.empty_text.set(read_prop(*s->props.empty_));
            }
            if (!s->alive) {
                return;
            }
            branch(p.empty, list.empty() || loading);
            branch(p.pagination, paging);
            const std::size_t visible_rows =
                paging ? std::min(page_size, list.size() - std::min(list.size(), (p.page.get() - 1) * page_size))
                       : list.size();
            p.scroll =
                std::clamp(p.scroll, 0.0F,
                           std::max(0.0F, visible_rows ? static_cast<float>(visible_rows) * (row_height + spacing) -
                                                             spacing - height
                                                       : 0));
            s->move_disabled[i].set(!enabled || (i == 0 && one_way) || std::ranges::none_of(rows, [&](const auto& r) {
                return r->target == (i == 0) && !r->item.disabled && has(selected_keys, r->item.key);
            }));
            if (s->props.operations_) {
                s->action_label[i].set(read_prop(*s->props.operations_)[i ? 0 : 1]);
            }
            s->action_icon[i].set(i != s->rtl ? String{u8"→"} : String{u8"←"});
        }
        branch(s->actions[0], !one_way);
        // Retained keys preserve editors; traversal follows their new side/order.
        const auto native_order = services_->interactions().declaration_order();
        std::vector<input::InteractionId> original_order;
        for (auto hit : native_order) {
            const auto* record = services_->interactions().find(hit);
            if (record && descendant(*services_, record->component, id)) {
                original_order.push_back(hit);
            }
        }
        std::vector<input::InteractionId> desired_order;
        const auto append_interactions = [&](runtime::ComponentId owner) {
            if (!owner.valid()) {
                return;
            }
            for (auto hit : original_order) {
                const auto* record = services_->interactions().find(hit);
                if (record && descendant(*services_, record->component, owner) &&
                    std::ranges::find(desired_order, hit) == desired_order.end()) {
                    desired_order.push_back(hit);
                }
            }
        };
        for (std::size_t i = 0; i < 2; ++i) {
            const auto& p = s->panels[i];
            for (auto child : {p.title, p.all, p.icon, p.current, p.invert, p.clear, p.remove_all, p.remove_current,
                               p.search, p.custom}) {
                append_interactions(child);
            }
            auto ordered_rows = lists[i];
            for (const auto& row : ordered_rows) {
                append_interactions(row->component);
            }
            append_interactions(p.pagination);
            append_interactions(p.footer);
            if (!i) {
                append_interactions(s->actions[1]);
                append_interactions(s->actions[0]);
                for (auto child : s->additional_actions) {
                    append_interactions(child);
                }
            }
        }
        append_interactions(id);
        if (desired_order.size() > 1 && desired_order != original_order) {
            const auto anchor = desired_order.front();
            services_->interactions().reorder_after(anchor, std::span{desired_order}.subspan(1));
        }
        if (geometry) {
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                       runtime::DirtyFlags::HitTest);
        }
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
        run_untracked([&] { localize(id, read_prop(s->locale_source), read_prop(s->plural_source)); });
        if (!s->alive || !s->again) {
            return;
        }
    }
}

void TransferComponentHost::select_keys(runtime::ComponentId id, TransferKeys keys, bool selected) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    valid_keys(keys);
    if (std::ranges::any_of(keys, [&](const auto& key) {
            return std::ranges::none_of(s->rows, [&](const auto& r) { return r->item.key == key; });
        })) {
        return;
    }
    FailureGuard failure{services_, id};
    const auto target_authority = s->props.target_ ? read_prop(*s->props.target_) : s->target;
    const auto selection_authority = s->props.selected_ ? read_prop(*s->props.selected_) : s->selected;
    if (!s->alive) {
        return;
    }
    valid_keys(target_authority);
    valid_keys(selection_authority);
    auto live_items = read_prop(s->props.data_);
    TransferKeys live_keys;
    for (auto& item : live_items) {
        if (s->props.row_key_) {
            item.key = s->props.row_key_(item);
            if (!s->alive) {
                return;
            }
        }
        live_keys.push_back(item.key);
    }
    valid_keys(live_keys);
    for (const auto& authority : {target_authority, selection_authority}) {
        if (std::ranges::any_of(authority, [&](const auto& key) { return !has(live_keys, key); })) {
            throw std::invalid_argument("Transfer authority key is absent from current dataSource");
        }
    }
    if (std::ranges::any_of(keys, [&](const auto& key) { return !has(live_keys, key); })) {
        return;
    }

    if (keys.size() == 1 && s->range_select) {
        const bool right = has(s->target, keys[0]);
        const auto anchor = s->panels[right].anchor;
        if (anchor) {
            std::vector<std::shared_ptr<Row>> list;
            for (const auto& row : s->rows) {
                if (row->target == right && !row->item.disabled) {
                    list.push_back(row);
                }
            }
            if (right) {
                std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
                    return std::ranges::find(s->target, a->item.key) < std::ranges::find(s->target, b->item.key);
                });
            }
            auto first = std::ranges::find_if(list, [&](const auto& row) { return row->item.key == *anchor; });
            auto last = std::ranges::find_if(list, [&](const auto& row) { return row->item.key == keys[0]; });
            if (first != list.end() && last != list.end()) {
                if (first > last) {
                    std::swap(first, last);
                }
                keys.clear();
                for (auto it = first; it <= last; ++it) {
                    keys.push_back((*it)->item.key);
                }
                selected = true;
            }
        }
    }
    if (keys.size() == 1) {
        auto& anchor = s->panels[has(s->target, keys[0])].anchor;
        anchor = selected ? std::optional<TransferKey>{keys[0]} : std::nullopt;
    }
    auto next = selection_authority;
    for (const auto& key : keys) {
        auto it = std::ranges::find_if(s->rows, [&](const auto& r) { return r->item.key == key; });
        const auto live = std::ranges::find_if(live_items, [&](const auto& item) { return item.key == key; });
        if (it == s->rows.end() || live == live_items.end() || live->disabled) {
            continue;
        }
        std::erase(next, key);
        if (selected) {
            next.push_back(key);
        }
    }
    if (next == selection_authority) {
        return;
    }
    if (!s->props.selected_) {
        s->selected = next;
    }
    TransferSelectInfo info;
    for (const auto& k : next) {
        (has(target_authority, k) ? info.target_selected_keys : info.source_selected_keys).push_back(k);
    }
    refresh(id);
    auto cb = s->props.on_select_;
    if (cb && s->alive) {
        cb(std::move(info));
    }
}

void TransferComponentHost::select(runtime::ComponentId id, TransferDirection direction, TransferSelection action) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    FailureGuard failure{services_, id};
    if (action == TransferSelection::RemoveAll || action == TransferSelection::RemoveCurrent) {
        TransferKeys keys;
        for (const auto& r : s->rows) {
            if (r->target && !r->item.disabled && r->filtered && (action == TransferSelection::RemoveAll || r->shown)) {
                keys.push_back(r->item.key);
            }
        }
        move_keys(id, TransferDirection::Left, keys);
        return;
    }
    TransferKeys add;
    TransferKeys remove;
    for (const auto& r : s->rows) {
        if (r->target != (direction == TransferDirection::Right) || r->item.disabled || !r->filtered ||
            ((action == TransferSelection::Current ||
              (action == TransferSelection::Invert && s->pagination.enabled && !s->props.list_)) &&
             !r->shown)) {
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
    if (cb && s->alive) {
        cb(info);
    }
}

void TransferComponentHost::move(runtime::ComponentId id, TransferDirection direction, std::optional<TransferKey> key) {
    auto s = state(*services_, id);
    if (!s || !editable(id) || (direction == TransferDirection::Left && read_prop(s->props.one_way_) && !key)) {
        return;
    }
    FailureGuard failure{services_, id};
    move_keys(id, direction,
              key                  ? TransferKeys{*key}
              : s->props.selected_ ? read_prop(*s->props.selected_)
                                   : s->selected);
}

void TransferComponentHost::move_keys(runtime::ComponentId id, TransferDirection direction,
                                      const TransferKeys& candidates) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    FailureGuard failure{services_, id};
    valid_keys(candidates);
    const auto target_authority = s->props.target_ ? read_prop(*s->props.target_) : s->target;
    const auto selection_authority = s->props.selected_ ? read_prop(*s->props.selected_) : s->selected;
    if (!s->alive) {
        return;
    }
    valid_keys(target_authority);
    valid_keys(selection_authority);
    auto items = read_prop(s->props.data_);
    TransferKeys keys;
    for (auto& item : items) {
        if (s->props.row_key_) {
            item.key = s->props.row_key_(item);
            if (!s->alive) {
                return;
            }
        }
        keys.push_back(item.key);
    }
    valid_keys(keys);
    for (const auto& authority : {target_authority, selection_authority}) {
        if (std::ranges::any_of(authority, [&](const auto& key) { return !has(keys, key); })) {
            throw std::invalid_argument("Transfer authority key is absent from current dataSource");
        }
    }
    if (std::ranges::any_of(candidates, [&](const auto& key) { return !has(keys, key); })) {
        return;
    }
    TransferKeys moved;
    for (const auto& candidate : candidates) {
        const auto it = std::ranges::find_if(items, [&](const auto& item) { return item.key == candidate; });
        if (it != items.end() && !it->disabled &&
            has(target_authority, candidate) == (direction == TransferDirection::Left)) {
            moved.push_back(candidate);
        }
    }
    if (moved.empty()) {
        return;
    }
    s->panels[0].anchor.reset();
    s->panels[1].anchor.reset();
    auto target = target_authority;
    for (const auto& k : moved) {
        std::erase(target, k);
    }
    if (direction == TransferDirection::Right) {
        target.insert(target.begin(), moved.begin(), moved.end());
    }
    auto selected = selection_authority;
    for (const auto& k : moved) {
        std::erase(selected, k);
    }
    TransferSelectInfo selection;
    for (const auto& k : selected) {
        (has(target_authority, k) ? selection.target_selected_keys : selection.source_selected_keys).push_back(k);
    }
    if (!s->props.target_) {
        s->target = target;
    }
    if (!s->props.selected_) {
        s->selected = selected;
    }
    refresh(id);
    const auto select_callback = s->props.on_select_;
    if (select_callback && s->alive) {
        select_callback(selection);
    }
    const auto cb = s->props.on_change_;
    if (cb && s->alive) {
        cb({target, direction, moved});
    }
}

void TransferComponentHost::mount(const TransferProps& input) {
    FormPrivateEditorScope private_editor;
    TransferProps props = input;
    if ((props.target_ && props.default_target_) || (props.selected_ && props.default_selected_)) {
        throw std::invalid_argument("Transfer controlled/default conflict");
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
    if (!props.row_key_) {
        valid_keys(keys);
    }
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

    b.on_resource_cleanup(id, [this, s, id] {
        services_->interactions().set_input_blocked(id, false);
        s->alive = false;
        s->bindings.dispose();
        services_->layout().remove_layout(s->node);
    });
    // Evaluate application bindings inside this generation's failure boundary.
    const auto safe = [&]<class T>(Prop<T>& prop) {
        if (PropAccess::binding(prop)) {
            auto original = prop;
            prop = bind([this, id, weak = std::weak_ptr{s}, original] {
                const auto current = weak.lock();
                if (!current || !current->alive) {
                    return T{};
                }
                FailureGuard failure{services_, id};
                auto value = read_prop(original);
                if constexpr (std::same_as<T, float>) {
                    if (!std::isfinite(value) || value < 0) {
                        throw std::invalid_argument("Transfer logical length is invalid");
                    }
                } else if constexpr (std::same_as<T, ControlSize>) {
                    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
                        throw std::invalid_argument("Transfer size is invalid");
                    }
                } else if constexpr (std::same_as<T, InputStatus>) {
                    if (value != InputStatus::Default && value != InputStatus::Error && value != InputStatus::Warning) {
                        throw std::invalid_argument("Transfer status is invalid");
                    }
                } else if constexpr (std::same_as<T, TextDirection>) {
                    if (value != TextDirection::Auto && value != TextDirection::LeftToRight &&
                        value != TextDirection::RightToLeft) {
                        throw std::invalid_argument("Transfer direction is invalid");
                    }
                } else if constexpr (std::same_as<T, TransferPagination>) {
                    if (!value.page_size ||
                        std::ranges::find(value.page_size_options, std::size_t{0}) != value.page_size_options.end()) {
                        throw std::invalid_argument("Transfer pagination is invalid");
                    }
                }
                return value;
            });
        }
    };
    safe(props.data_);
    safe(props.disabled_);
    safe(props.read_only_);
    safe(props.visible_);
    safe(props.loading_);
    safe(props.show_search_);
    safe(props.select_all_);
    safe(props.one_way_);
    safe(props.size_);
    safe(props.status_);
    safe(props.direction_);
    safe(props.pagination_);
    if (props.locale_) {
        safe(*props.locale_);
    }
    safe(props.row_spacing_);
    if (props.operations_) {
        safe(*props.operations_);
    }
    safe(props.search_default_);
    if (props.target_) {
        safe(*props.target_);
    }
    if (props.selected_) {
        safe(*props.selected_);
    }
    if (props.height_) {
        safe(*props.height_);
    }
    if (props.width_) {
        safe(*props.width_);
    }
    if (props.search_placeholder_) {
        safe(*props.search_placeholder_);
    }
    if (props.empty_) {
        safe(*props.empty_);
    }
    if (props.titles_) {
        safe(*props.titles_);
    }
    s->props = props;
    s->locale_source = localized_prop(props.locale_, b.config_scope(), [](const LocaleData& d) { return d.transfer; });
    if (!props.locale_) {
        s->plural_source = localized_prop(std::optional<Prop<IntegerPluralRule>>{}, b.config_scope(),
                                          [](const LocaleData& d) { return d.plural; });
    }
    // Retained list/footer factories must see their initial data during mount.
    // Publishing an empty list first would force native child lists to append
    // rows while ComponentHost is still inside the enclosing mount transaction.
    auto initial_items = read_prop(props.data_);
    TransferKeys initial_keys;
    for (auto& item : initial_items) {
        if (props.row_key_) {
            item.key = props.row_key_(item);
            if (!s->alive) {
                transaction.done = true;
                return;
            }
        }
        initial_keys.push_back(item.key);
    }
    valid_keys(initial_keys);
    s->locale.set(read_prop(s->locale_source));
    s->loading = read_prop(props.loading_);
    s->direction.set(read_prop(props.direction_));
    s->disabled.set(!editable(id));
    publish_theme(s->child_theme, child_theme(b.theme_scope()->snapshot().transfer()));
    const auto queries = read_prop(props.search_default_);
    for (std::size_t i = 0; i < 2; ++i) {
        auto& panel = s->panels[i];
        panel.query = queries[i];
        panel.query_value.set(panel.query);
        std::vector<TransferItem> filtered;
        for (const auto& item : initial_items) {
            if (has(s->target, item.key) != (i != 0)) {
                continue;
            }
            const auto label = props.render_ ? props.render_(item) : item.title;
            if (!s->alive) {
                transaction.done = true;
                return;
            }
            const bool matches = panel.query.empty() ||
                                 (props.filter_ ? props.filter_(panel.query, item, side(i))
                                                : label.bytes().find(panel.query.bytes()) != std::string_view::npos);
            if (!s->alive) {
                transaction.done = true;
                return;
            }
            if (matches) {
                filtered.push_back(item);
            }
        }
        TransferKeys selected;
        for (const auto& key : s->selected) {
            if (has(s->target, key) == (i != 0)) {
                selected.push_back(key);
            }
        }
        if (i) {
            std::stable_sort(filtered.begin(), filtered.end(), [&](const auto& a, const auto& b) {
                return std::ranges::find(s->target, a.key) < std::ranges::find(s->target, b.key);
            });
        }
        const auto checked = static_cast<std::size_t>(std::ranges::count_if(
            filtered, [&](const auto& item) { return !item.disabled && has(selected, item.key); }));
        const auto selectable =
            static_cast<std::size_t>(std::ranges::count_if(filtered, [](const auto& item) { return !item.disabled; }));
        const auto locale = s->locale.get();
        const auto summary =
            props.select_all_label_
                ? props.select_all_label_({side(i), checked, filtered.size()})
                : count_text(locale, checked, filtered.size(), props.locale_.has_value(), read_prop(s->plural_source));
        if (!s->alive) {
            transaction.done = true;
            return;
        }
        panel.total.set(filtered.size());
        panel.count_text.set(summary);
        panel.selected_count.set(checked);
        panel.checked.set(selectable && checked == selectable);
        panel.indeterminate.set(checked && checked < selectable);
        panel.disabled.set(s->disabled.get() || !selectable);
        panel.summary.set(
            String::from_utf8(
                std::string((checked && checked == selectable ? locale.deselect_all : locale.select_all).bytes()) +
                " · " + std::string(summary.bytes()))
                .value());
        panel.title_text.set(props.titles_ ? read_prop(*props.titles_)[i] : locale.titles[i]);
        panel.placeholder.set(props.search_placeholder_ ? read_prop(*props.search_placeholder_)
                                                        : locale.search_placeholder);
        panel.empty_text.set(read_prop(props.loading_) ? locale.loading
                             : props.empty_            ? read_prop(*props.empty_)
                                                       : locale.empty[i]);
        panel.filtered.set(filtered);
        panel.selected.set(selected);
        s->action_label[i].set(props.operations_ ? read_prop(*props.operations_)[i ? 0 : 1]
                                                 : locale.actions[i ? 0 : 1]);
        s->action_icon[i].set((i != 0) != (s->direction.get() == TextDirection::RightToLeft) ? String{u8"→"}
                                                                                             : String{u8"←"});
        s->move_disabled[i].set(s->disabled.get() || (i == 0 && read_prop(props.one_way_)) ||
                                std::ranges::none_of(initial_items, [&](const auto& item) {
                                    return !item.disabled && has(s->selected, item.key) &&
                                           has(s->target, item.key) == (i == 0);
                                }));
    }
    runtime::connect_layout_style(s->bindings, props.layout_, s->node, services_->nodes(), services_->dirty());
    b.mount_slot(
        id, Content{[&] {
            Theme(
                ThemeProps{}.config(s->child_theme), ThemeContent{[&] {
                    auto& build = runtime::require_component_build_context();
                    s->body = build.mount_component<int>(0);
                    const auto body_node = build.root(s->body);
                    build.on_resource_cleanup(s->body,
                                              [this, body_node] { services_->layout().remove_layout(body_node); });
                    build.mount_slot(
                        s->body, Content{[&] {
                            const auto child = [&](auto fn) {
                                const auto before = services_->components().children(s->body).size();
                                fn();
                                const auto component = services_->components().children(s->body).at(before);
                                build.on_resource_cleanup(component, [this, component] {
                                    services_->interactions().set_input_blocked(component, false);
                                });
                                return component;
                            };
                            const auto wrapper = [&](auto slot, bool fill = true) {
                                auto& build = runtime::require_component_build_context();
                                const auto component = build.mount_component<int>(0);
                                const auto node = build.root(component);
                                services_->layout().set_layout(node, layout::BoxLayout{{}, fill});
                                services_->nodes().require(node).clip_content = true;
                                build.on_resource_cleanup(component, [this, node, component] {
                                    services_->interactions().set_input_blocked(component, false);
                                    services_->layout().remove_layout(node);
                                });
                                build.mount_slot(component, slot);
                                return component;
                            };
                            for (std::size_t i = 0; i < 2; ++i) {
                                auto& p = s->panels[i];
                                p.page_size.set(read_prop(props.pagination_).page_size);
                                p.query = read_prop(props.search_default_)[i];
                                p.query_value.set(p.query);
                                if (props.title_slots_[i]) {
                                    p.title = wrapper(*props.title_slots_[i]);
                                } else {
                                    p.title =
                                        child([&] { Text(TextProps{}.content(p.title_text).direction(s->direction)); });
                                }
                                p.all = child([&] {
                                    Checkbox(CheckboxProps{}
                                                 .checked(p.checked)
                                                 .direction(bind([direction = s->direction] {
                                                     return direction.get() == TextDirection::RightToLeft
                                                                ? CheckboxDirection::RightToLeft
                                                                : CheckboxDirection::LeftToRight;
                                                 }))
                                                 .indeterminate(p.indeterminate)
                                                 .disabled(p.disabled)
                                                 .onChange([this, id, i](bool value) {
                                                     select(id, side(i),
                                                            value ? TransferSelection::All : TransferSelection::Clear);
                                                 }),
                                             CheckboxLabel{[&, i, summary = p.summary] {
                                                 if (props.select_all_slots_[i]) {
                                                     auto slot = props.select_all_slots_[i](
                                                         {side(i), p.selected_count, p.total, p.summary});
                                                     if (s->alive) {
                                                         detail::SlotContentAccess::function(slot)();
                                                     }
                                                 } else {
                                                     Text(TextProps{}.content(summary).direction(s->direction));
                                                 }
                                             }});
                                });
                                p.count_label =
                                    child([&] { Text(TextProps{}.content(p.count_text).direction(s->direction)); });
                                p.invert = child([&] {
                                    Button(
                                        ButtonProps{}
                                            .type(ButtonType::Text)
                                            .disabled(p.disabled)
                                            .wave(false)
                                            .size(props.size_)
                                            .onClick([this, id, i] { select(id, side(i), TransferSelection::Invert); }),
                                        ButtonContent{[label = p.invert_text] { Text(TextProps{}.content(label)); }});
                                });
                                p.clear = child([&] {
                                    Button(
                                        ButtonProps{}
                                            .type(ButtonType::Text)
                                            .disabled(p.disabled)
                                            .wave(false)
                                            .size(props.size_)
                                            .onClick([this, id, i] { select(id, side(i), TransferSelection::Clear); }),
                                        ButtonContent{[label = p.clear_text] { Text(TextProps{}.content(label)); }});
                                });
                                p.search = child([&] {
                                    FormPrivateEditorScope private_editor;
                                    Input(InputProps{}
                                              .disabled(s->disabled)
                                              .status(props.status_)
                                              .size(props.size_)
                                              .direction(props.direction_)
                                              .placeholder(p.placeholder)
                                              .value(p.query_value)
                                              .allowClear(true)
                                              .onChange([this, id, i](String query) {
                                                  auto s = state(*services_, id);
                                                  if (!s || !editable(id)) {
                                                      return;
                                                  }
                                                  auto& p = s->panels[i];
                                                  FailureGuard failure{services_, id};
                                                  p.anchor.reset();
                                                  p.query = query;
                                                  p.query_value.set(query);
                                                  p.scroll = 0;
                                                  p.page.set(1);
                                                  refresh(id);
                                                  auto cb = s->props.on_search_;
                                                  if (cb && s->alive) {
                                                      cb(side(i), query);
                                                  }
                                              }));
                                });
                                p.empty = child([&] { Text(TextProps{}.content(p.empty_text)); });
                                const auto selection_button = [&](runtime::ComponentId& component, Signal<String> label,
                                                                  TransferSelection action) {
                                    component = child([&] {
                                        Button(ButtonProps{}
                                                   .type(ButtonType::Text)
                                                   .disabled(p.disabled)
                                                   .wave(false)
                                                   .size(props.size_)
                                                   .onClick([this, id, i, action] { select(id, side(i), action); }),
                                               ButtonContent{[label] { Text(TextProps{}.content(label)); }});
                                    });
                                };
                                selection_button(p.current, p.current_text, TransferSelection::Current);
                                selection_button(p.remove_all, p.remove_all_text, TransferSelection::RemoveAll);
                                selection_button(p.remove_current, p.remove_current_text,
                                                 TransferSelection::RemoveCurrent);
                                if (props.selections_icon_) {
                                    p.icon = wrapper(*props.selections_icon_);
                                }
                                p.pagination = child([&] {
                                    auto pagination_props =
                                        PaginationProps{}
                                            .total(p.total)
                                            .current(p.page)
                                            .pageSize(p.page_size)
                                            .disabled(s->disabled)
                                            .size(ControlSize::Small)
                                            .direction(s->direction)
                                            .simple(bind([pagination = props.pagination_] {
                                                return read_prop(pagination).simple;
                                            }))
                                            .showLessItems(bind([pagination = props.pagination_] {
                                                return read_prop(pagination).show_less_items;
                                            }))
                                            .showSizeChanger(bind([pagination = props.pagination_] {
                                                return read_prop(pagination).show_size_changer;
                                            }))
                                            .pageSizeOptions(bind([pagination = props.pagination_] {
                                                return read_prop(pagination).page_size_options;
                                            }))
                                            .onChange([this, id, i](std::size_t page, std::size_t size) {
                                                if (auto s = state(*services_, id); s && editable(id)) {
                                                    FailureGuard failure{services_, id};
                                                    const auto pagination = read_prop(s->props.pagination_);
                                                    if (!pagination.controlled_page_size) {
                                                        s->panels[i].page_size.set(size);
                                                    }
                                                    s->panels[i].page.set(page);
                                                    s->panels[i].scroll = 0;
                                                    refresh(id);
                                                    const auto callback = s->props.on_page_;
                                                    if (callback && s->alive) {
                                                        callback({side(i), page, size});
                                                    }
                                                }
                                            });
                                    if (props.locale_) {
                                        pagination_props.locale(
                                            bind([locale = s->locale] { return locale.get().pagination; }));
                                    }
                                    Pagination(std::move(pagination_props));
                                });
                                if (props.footers_[i]) {
                                    p.footer = wrapper(*props.footers_[i]);
                                }
                                {
                                    TransferListContext context{
                                        side(i),
                                        p.filtered,
                                        p.selected,
                                        s->disabled,
                                        [this, id, i, weak = std::weak_ptr{s}](TransferKey key, bool value) {
                                            const auto current = weak.lock();
                                            if (!current || current->owner_thread != std::this_thread::get_id() ||
                                                !current->alive) {
                                                return;
                                            }
                                            const auto target = current->props.target_
                                                                    ? read_prop(*current->props.target_)
                                                                    : current->target;
                                            if (has(target, key) == (i != 0)) {
                                                select_keys(id, {key}, value);
                                            }
                                        },
                                        [this, id, i, weak = std::weak_ptr{s}](TransferKeys keys, bool value) {
                                            const auto current = weak.lock();
                                            if (!current || current->owner_thread != std::this_thread::get_id() ||
                                                !current->alive) {
                                                return;
                                            }
                                            const auto target = current->props.target_
                                                                    ? read_prop(*current->props.target_)
                                                                    : current->target;
                                            if (std::ranges::any_of(keys, [&](const auto& key) {
                                                    return has(target, key) != (i != 0);
                                                })) {
                                                return;
                                            }
                                            select_keys(id, std::move(keys), value);
                                        },
                                        p.query_value,
                                        s->locale,
                                        s->direction,
                                        bind([one_way = props.one_way_, i] { return i != 0 && read_prop(one_way); }),
                                        [this, id, i, weak = std::weak_ptr{s}](TransferKeys keys) {
                                            const auto current = weak.lock();
                                            if (!current || current->owner_thread != std::this_thread::get_id() ||
                                                !current->alive || !i || !read_prop(current->props.one_way_)) {
                                                return;
                                            }
                                            valid_keys(keys);
                                            const auto target = current->props.target_
                                                                    ? read_prop(*current->props.target_)
                                                                    : current->target;
                                            if (std::ranges::any_of(
                                                    keys, [&](const auto& key) { return !has(target, key); })) {
                                                return;
                                            }
                                            move_keys(id, TransferDirection::Left, keys);
                                        }};
                                    if (props.list_) {
                                        auto slot = props.list_(context);
                                        if (!s->alive) {
                                            return;
                                        }
                                        p.custom = wrapper(slot);
                                    }
                                    if (props.footer_factory_ && !props.footers_[i]) {
                                        auto slot = props.footer_factory_(context);
                                        if (!s->alive) {
                                            return;
                                        }
                                        p.footer = wrapper(slot);
                                    }
                                }
                                if (props.action_slots_[i]) {
                                    std::weak_ptr<State> weak{s};
                                    auto slot = props.action_slots_[i](
                                        {side(i), s->move_disabled[i], s->action_label[i], s->direction,
                                         [this, weak, id, i] {
                                             const auto current = weak.lock();
                                             if (!current || current->owner_thread != std::this_thread::get_id() ||
                                                 !current->alive) {
                                                 return;
                                             }
                                             move(id, side(i));
                                         }});
                                    if (!s->alive) {
                                        return;
                                    }
                                    s->actions[i] = wrapper(slot, false);
                                } else {
                                    s->actions[i] = child([&] {
                                        Button(ButtonProps{}
                                                   .type(ButtonType::Primary)
                                                   .disabled(s->move_disabled[i])
                                                   .size(props.size_)
                                                   .wave(false)
                                                   .onClick([this, id, i] { move(id, side(i)); }),
                                               ButtonContent{[operations = props.operations_, i,
                                                              icon = s->action_icon[i]] {
                                                   Text(TextProps{}.content(bind([operations, i, icon] {
                                                       if (!operations) {
                                                           return icon.get();
                                                       }
                                                       const auto label = read_prop(*operations)[i ? 0 : 1];
                                                       return label.empty()
                                                                  ? String{}
                                                                  : String::from_utf8(std::string(icon.get().bytes()) +
                                                                                      " " + std::string(label.bytes()))
                                                                        .value();
                                                   })));
                                               }});
                                    });
                                }
                            }
                            for (const auto& slot : props.additional_actions_) {
                                s->additional_actions.push_back(wrapper(slot, false));
                                if (!s->alive) {
                                    return;
                                }
                            }
                        }});
                }});
        }});
    if (!s->alive) {
        transaction.done = true;
        return;
    }
    const auto body_node = services_->components().root(s->body);
    services_->layout().set_layout(body_node, layout::BoxLayout{{}, true});
    auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s->surface = services_->surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(id, [this, surface = s->surface] { services_->surfaces().destroy_content_range(surface); });
    services_->layout().set_layout(
        s->node, layout::ComponentLayout{
                     [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints limits) {
                         return measure(id, e, limits);
                     },
                     [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect rect) { place(id, e, rect); }});
    s->ready = true;
    reconcile(id, read_prop(props.data_));
    connect_prop(
        s->bindings, props.data_, [this, id](auto items) { reconcile(id, items); },
        [](const auto&, const auto&) { return false; });
    if (props.target_) {
        connect_prop(s->bindings, *props.target_, [this, id](TransferKeys keys) {
            valid_keys(keys);
            if (auto s = state(*services_, id)) {
                FailureGuard failure{services_, id};
                if (std::ranges::any_of(keys, [&](const auto& key) {
                        return std::ranges::none_of(s->rows, [&](const auto& r) { return r->item.key == key; });
                    })) {
                    throw std::invalid_argument("Transfer controlled key is absent from dataSource");
                }
                s->target = std::move(keys);
                refresh(id);
            }
        });
    }
    if (props.selected_) {
        connect_prop(s->bindings, *props.selected_, [this, id](TransferKeys keys) {
            valid_keys(keys);
            if (auto s = state(*services_, id)) {
                FailureGuard failure{services_, id};
                if (std::ranges::any_of(keys, [&](const auto& key) {
                        return std::ranges::none_of(s->rows, [&](const auto& r) { return r->item.key == key; });
                    })) {
                    throw std::invalid_argument("Transfer controlled key is absent from dataSource");
                }
                s->selected = std::move(keys);
                refresh(id);
            }
        });
    }
    connect_prop(s->bindings, props.pagination_, [this, id](TransferPagination value) {
        FailureGuard failure{services_, id};
        if (!value.page_size) {
            throw std::invalid_argument("Transfer page size must be positive");
        }
        if (auto s = state(*services_, id)) {
            for (auto& p : s->panels) {
                p.page_size.set(value.page_size);
                p.page.set(1);
                p.scroll = 0;
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
    watch(props.size_);
    watch(props.status_);
    watch(props.row_spacing_);
    connect_prop(
        s->bindings,
        Prop<std::pair<TransferLocale, IntegerPluralRule>>{bind([locale = s->locale_source, plural = s->plural_source] {
            return std::pair{read_prop(locale), read_prop(plural)};
        })},
        [this, id](auto value) { localize(id, std::move(value.first), value.second); });
    if (props.operations_) {
        watch(*props.operations_);
    }
    if (props.height_) {
        watch(*props.height_);
    }
    if (props.width_) {
        watch(*props.width_);
    }
    if (props.empty_) {
        watch(*props.empty_);
    }
    if (props.titles_) {
        watch(*props.titles_);
    }
    if (props.search_placeholder_) {
        watch(*props.search_placeholder_);
    }
    const auto theme = b.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::transfer_colors, &theme_runtime::ThemeScope::transfer_metrics,
        &theme_runtime::ThemeScope::transfer_typography, &theme_runtime::ThemeScope::transfer_effects,
        &theme_runtime::ThemeScope::transfer_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s->subscriptions[i] =
            theme->capture([this, id](auto) { refresh(id); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    for (std::size_t i = 0; i < 2; ++i) {
        services_->components().set_branch_active(s->panels[i].search, read_prop(props.show_search_));
        services_->components().set_branch_active(s->panels[i].all, read_prop(props.select_all_));
    }
    mounted_.push_back(id);
    refresh(id);
    s->mounting = false;
    transaction.done = true;
}

runtime::Size TransferComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e,
                                             layout::Constraints limits) {
    auto s = state(*services_, id);
    if (!s || !s->alive) {
        return {};
    }
    FailureGuard failure{services_, id};
    const float budget =
        std::min(limits.max_width, s->width * 2 + s->token.metrics.actions_width + s->token.metrics.gap * 2);
    s->action_width = s->token.metrics.actions_width;
    std::vector<runtime::ComponentId> actions{s->actions.begin(), s->actions.end()};
    actions.insert(actions.end(), s->additional_actions.begin(), s->additional_actions.end());
    for (auto action : actions) {
        if (action.valid() && services_->components().branch_active(action)) {
            const auto measured =
                e.measure_child(services_->components().root(action), {0, budget, 0, s->token.metrics.control_height});
            s->action_width = std::max(s->action_width, measured.width);
        }
    }
    const float width = s->width * 2 + s->action_width + s->token.metrics.gap * 2;
    if (!std::isfinite(width) || width > std::numeric_limits<float>::max() / 4) {
        throw std::invalid_argument("Transfer action geometry is invalid");
    }
    for (auto child : services_->components().children(id)) {
        static_cast<void>(e.measure_child(services_->components().root(child), {0, width, 0, s->total_height}));
    }
    return limits.constrain({width, s->total_height});
}

void TransferComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect rect) {
    auto s = state(*services_, id);
    if (!s || !s->alive) {
        return;
    }
    FailureGuard failure{services_, id};
    const auto& m = s->token.metrics;
    const float actions = std::min(std::max(m.actions_width, s->action_width), rect.width);
    const float gap = std::min(m.gap, std::max(0.0F, (rect.width - actions) / 2));
    const float width = std::max(0.0F, (rect.width - actions - gap * 2) / 2);
    const auto place = [&](runtime::ComponentId child, runtime::Rect r, bool fixed = false) {
        if (child.valid() && services_->components().contains(child)) {
            static_cast<void>(e.measure_child(services_->components().root(child),
                                              fixed ? layout::Constraints::fixed(r.width, r.height)
                                                    : layout::Constraints{0, r.width, 0, r.height}));
            e.place_child(services_->components().root(child), r, fixed, fixed);
        }
    };
    for (auto child : services_->components().children(id)) {
        place(child, rect);
    }
    const bool paging = s->pagination.enabled && !s->props.list_;
    const float row_stride = s->row_height + s->spacing;
    for (std::size_t i = 0; i < 2; ++i) {
        auto& p = s->panels[i];
        const float x = rect.x + ((i != 0) != s->rtl ? width + actions + gap * 2 : 0);
        const float inner = std::max(0.0F, width - m.padding_inline * 2);
        const float padding = std::min(m.padding_inline, width / 2);
        const auto mirrored = [&](float offset, float extent) {
            return s->rtl ? x + width - offset - extent : x + offset;
        };
        float top = rect.y;
        p.section = {x, rect.y, width, rect.height};
        place(p.title, {x + padding, top, inner, m.header_height});
        top += m.header_height;
        place(p.all, {mirrored(padding, inner), top, inner, m.header_height});
        place(p.count_label, {x + padding, top, inner, m.header_height});
        if (p.icon.valid()) {
            place(p.icon, {mirrored(width - padding - m.control_height, m.control_height), top, m.control_height,
                           m.header_height});
        }
        top += m.header_height;
        std::vector<runtime::ComponentId> choices;
        for (auto child : {p.current, p.invert, p.clear, p.remove_all, p.remove_current}) {
            if (child.valid() && services_->components().branch_active(child)) {
                choices.push_back(child);
            }
        }
        const float choice_width = choices.empty() ? 0 : inner / static_cast<float>(choices.size());
        for (std::size_t j = 0; j < choices.size(); ++j) {
            place(choices[j], {mirrored(padding + choice_width * static_cast<float>(j), choice_width), top,
                               choice_width, m.control_height});
        }
        top += m.control_height + m.gap;
        if (s->search) {
            place(p.search, {x + padding, top, inner, m.control_height});
            top += m.control_height + m.gap;
        }
        const float bottom_reserved =
            (paging ? m.control_height * (s->pagination.show_size_changer ? 2 : 1) + m.gap : 0) +
            (p.footer.valid() ? m.footer_height : 0);
        const float height = std::max(0.0F, std::min(s->height, rect.y + rect.height - top - bottom_reserved));
        p.viewport = {x + padding, top, inner, height};
        std::size_t count = 0;
        for (const auto& r : s->rows) {
            if (r->target == (i != 0) && r->shown) {
                ++count;
            }
        }
        p.scroll = std::clamp(p.scroll, 0.0F,
                              std::max(0.0F, count ? static_cast<float>(count) * row_stride - s->spacing - height : 0));
        place(p.empty, {x + padding, top, inner, std::min(m.control_height, height)});
        place(p.custom, p.viewport);
        for (const auto& r : s->rows) {
            if (r->target != (i != 0) || !r->shown || !r->component.valid()) {
                continue;
            }
            const auto order = paging ? r->order % p.page_size.get() : r->order;
            r->full = {x + padding, top + static_cast<float>(order) * row_stride - p.scroll, inner, s->row_height};
            e.place_child(services_->components().root(r->component), intersect(r->full, p.viewport), true, true);
        }
        top += height;
        if (paging) {
            const float paging_height = m.control_height * (s->pagination.show_size_changer ? 2 : 1);
            place(p.pagination, {x + padding, top, inner, paging_height}, true);
            top += paging_height + m.gap;
        }
        if (p.footer.valid()) {
            place(p.footer,
                  {x + padding, top, inner, std::max(0.0F, std::min(m.footer_height, rect.y + rect.height - top))});
        }
        // Ant actions are top-to-bottom: to target, then to source; mirrored icons retain semantic direction.
        place(s->actions[i],
              {rect.x + width + gap, p.viewport.y + static_cast<float>(1 - i) * (m.control_height + m.gap), actions,
               m.control_height});
    }
    for (std::size_t i = 0; i < s->additional_actions.size(); ++i) {
        place(s->additional_actions[i],
              {rect.x + width + gap, s->panels[0].viewport.y + static_cast<float>(i + 2) * (m.control_height + m.gap),
               actions, m.control_height});
    }
    const bool zero = rect.width <= 0 || rect.height <= 0 || width <= 0 || s->panels[0].viewport.height <= 0;
    if (zero != s->zero_area) {
        s->zero_area = zero;
        services_->set_input_blocked(id, zero || !active_ || !read_prop(s->props.visible_) ||
                                             read_prop(s->props.disabled_) || read_prop(s->props.read_only_) ||
                                             read_prop(s->props.loading_));
    }
}

void TransferComponentHost::on_focus_changed() {
    const auto focused = services_->focus().state().focused;
    const auto* hit = focused ? services_->interactions().find(*focused) : nullptr;
    if (!hit) {
        return;
    }
    const auto component = hit->component;
    for (auto id : mounted_) {
        auto s = state(*services_, id);
        if (!s || !s->alive || !editable(id)) {
            continue;
        }
        for (const auto& r : s->rows) {
            if (!r->shown || !r->component.valid() || !descendant(*services_, component, r->component)) {
                continue;
            }
            auto& p = s->panels[r->target];
            const float previous = p.scroll;
            const auto order = s->pagination.enabled ? r->order % p.page_size.get() : r->order;
            const float y = static_cast<float>(order) * (s->row_height + s->spacing);
            if (y < p.scroll) {
                p.scroll = y;
            } else if (y + s->row_height > p.scroll + p.viewport.height) {
                p.scroll = std::max(0.0F, y + s->row_height - p.viewport.height);
            }
            if (previous != p.scroll) {
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
            }
            return;
        }
    }
}

void TransferComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto s = state(*services_, id);
        if (!s || !s->alive) {
            continue;
        }
        FailureGuard failure{services_, id};
        const auto& token = s->token;
        const auto& node = services_->nodes().require(s->node);
        const graphics::EffectClip effect_clip{1, services_->component_clip(id, viewport, clip)};
        std::vector<graphics::RoundedEffectInstance> effects;
        const bool visible =
            services_->components().branch_active(id) && node.bounds.width > 0 && node.bounds.height > 0;
        const auto add = [&](runtime::Rect r, Color color, float radius = 0) {
            if (r.width <= 0 || r.height <= 0) {
                return;
            }
            auto fill = graphics::make_corner_fill_effects(
                {r, std::min(radius, std::min(r.width, r.height) / 2)}, {true, true, true, true}, color,
                services_->nodes().content_opacity(s->node), node.translation, effect_clip);
            effects.insert(effects.end(), fill.begin(), fill.end());
        };
        const auto status = read_prop(s->props.status_);
        const auto border = status == InputStatus::Error     ? token.colors.error
                            : status == InputStatus::Warning ? token.colors.warning
                                                             : token.colors.border;
        const auto focus = services_->focus().state();
        const auto* focused = focus.focused ? services_->interactions().find(*focus.focused) : nullptr;
        if (visible) {
            for (const auto& p : s->panels) {
                const auto r = p.section;
                add(r, s->disabled.get() ? token.colors.disabled_background : token.colors.background,
                    token.metrics.radius);
                add({r.x, r.y, r.width, token.metrics.header_height * 2}, token.colors.header, token.metrics.radius);
                if (p.footer.valid()) {
                    const auto b = services_->nodes().require(services_->components().root(p.footer)).bounds;
                    add(b, token.colors.footer);
                }
                if (token.metrics.border_width > 0) {
                    effects.push_back(graphics::make_outline_effect(
                        {r, token.metrics.radius}, token.metrics.border_width, 0, border,
                        services_->nodes().content_opacity(s->node), node.translation, effect_clip));
                }
            }
            const auto pointer = services_->pointer().state(input::PointerIdentity::mouse());
            const auto* hovered =
                pointer && pointer->hover_target ? services_->interactions().find(*pointer->hover_target) : nullptr;
            for (const auto& r : s->rows) {
                if (!r->shown || !r->component.valid()) {
                    continue;
                }
                const auto b = intersect(r->full, s->panels[r->target].viewport);
                if (r->checked.get()) {
                    add(b, r->disabled.get() ? token.colors.disabled_background : token.colors.selected,
                        token.metrics.radius);
                } else if (hovered && !r->disabled.get() && descendant(*services_, hovered->component, r->component)) {
                    add(b, token.colors.hover, token.metrics.radius);
                }
            }
            if (focused && focus.focus_visible && descendant(*services_, focused->component, id) &&
                token.effects.focus_width > 0) {
                auto b = services_->nodes().require(focused->node).bounds;
                if (b.width > 0 && b.height > 0) {
                    effects.push_back(graphics::make_outline_effect(
                        {b, token.metrics.radius}, token.effects.focus_width, token.effects.focus_offset,
                        token.colors.focus, services_->nodes().content_opacity(s->node), node.translation,
                        effect_clip));
                }
            }
        }
        services_->surfaces().update_content_effects(s->surface, effects);
    }
}

void TransferComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                             std::optional<input::InteractionId> hit,
                                             std::optional<input::InteractionId>) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up &&
        event.action != input::PointerAction::cancel) {
        return;
    }
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto s = state(*services_, id);
        if (!s) {
            continue;
        }
        s->range_select =
            event.action == input::PointerAction::down && event.shift && record &&
            std::ranges::any_of(s->rows, [&](const auto& r) { return r->shown && r->checkbox == record->component; });
    }
}

bool TransferComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.key != input::Key::space || event.action != input::KeyAction::down || event.repeat ||
        !input::has_modifier(event.modifiers, input::KeyModifier::shift)) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (!record) {
        return false;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto s = state(*services_, id);
        if (!s || !editable(id)) {
            continue;
        }
        const auto row =
            std::ranges::find_if(s->rows, [&](const auto& r) { return r->checkbox == record->component && r->shown; });
        if (row == s->rows.end()) {
            continue;
        }
        const auto anchor = s->panels[(*row)->target].anchor;
        if (!anchor) {
            return false;
        }
        std::vector<std::shared_ptr<Row>> list;
        for (const auto& r : s->rows) {
            if (r->target == (*row)->target && !r->item.disabled) {
                list.push_back(r);
            }
        }
        if ((*row)->target) {
            std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
                return std::ranges::find(s->target, a->item.key) < std::ranges::find(s->target, b->item.key);
            });
        }
        auto first = std::ranges::find_if(list, [&](const auto& r) { return r->item.key == *anchor; });
        auto last = std::ranges::find(list, *row);
        if (first == list.end() || last == list.end()) {
            return false;
        }
        if (first > last) {
            std::swap(first, last);
        }
        TransferKeys keys;
        for (auto it = first; it <= last; ++it) {
            keys.push_back((*it)->item.key);
        }
        select_keys(id, std::move(keys), true);
        if (s->alive) {
            s->panels[(*row)->target].anchor = anchor;
        }
        return true;
    }
    return false;
}

bool TransferComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (!std::isfinite(event.delta_y)) {
        return false;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto s = state(*services_, id);
        if (!s || !editable(id) || s->props.list_) {
            continue;
        }
        for (std::size_t i = 0; i < 2; ++i) {
            auto& p = s->panels[i];
            const auto& v = p.viewport;
            const auto translation = services_->nodes().require(s->node).translation;
            if (event.x < v.x + translation.x || event.x >= v.x + v.width + translation.x ||
                event.y < v.y + translation.y || event.y >= v.y + v.height + translation.y) {
                continue;
            }
            FailureGuard failure{services_, id};
            const auto count =
                std::ranges::count_if(s->rows, [i](const auto& r) { return r->shown && r->target == (i != 0); });
            const float max = std::max(
                0.0F, count ? static_cast<float>(count) * (s->row_height + s->spacing) - s->spacing - v.height : 0);
            const float previous = p.scroll;
            p.scroll = std::clamp(previous - event.delta_y * 96, 0.0F, max);
            if (previous == p.scroll) {
                return false;
            }
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
            const auto callback = s->props.on_scroll_;
            if (callback) {
                callback({side(i), p.scroll, event.delta_y});
            }
            return true;
        }
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
