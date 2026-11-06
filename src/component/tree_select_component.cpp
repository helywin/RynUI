#include "component/tree_select_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/tree_select_model.hpp"
#include "component/input_affix_action.hpp"
#include "component/input_component.hpp"
#include "component/popover_component.hpp"
#include "component/tree_component.hpp"
#include "component/navigation_transition.hpp"
#include "component/localized_prop.hpp"
#include <ryn/tag.hpp>
#include <ryn/flex.hpp>
#include <map>
#include <set>
#include <limits>
#include "theme/input_tokens.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "reactive/untracked.hpp"

#include <algorithm>
#include <exception>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local TreeSelectComponentHost* active_tree_select{};

struct ItemState final {
    Signal<String> title{String{}};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
};

struct TreeSelectState final {
    explicit TreeSelectState(TreeSelectModel value) : model(std::move(value)) {}

    TreeSelectModel model;
    TreeSelectProps props;
    std::unique_ptr<NavigationTransition> motion;
    bool motion_open{};
    Signal<std::vector<TreeItem>> topology{std::vector<TreeItem>{}};
    TreeKeys values;
    TreeKeys half;
    Signal<TreeKeys> half_state{TreeKeys{}};
    std::map<std::string, String, std::less<>> labels;
    Signal<bool> multiple{false};
    Signal<bool> checkable{false};
    Signal<bool> strict{false};
    Signal<bool> input_read_only{true};
    Signal<String> query{String{}};
    bool searchable{};
    bool label_value{};
    bool rtl{};
    TreeSelectCheckedStrategy strategy{TreeSelectCheckedStrategy::ShowChild};
    TreeSelectMaxTagCount max_tags{std::size_t{100000}};
    runtime::ComponentId tag_host;
    std::size_t shown_tags{};
    std::string tag_signature;
    runtime::Size tag_geometry;
    bool tag_ready{};
    bool composing{true};

    struct TagRow {
        runtime::ComponentId id;
        Signal<String> label{String{}};
        Signal<bool> disabled{false};
    };

    std::map<std::string, TagRow, std::less<>> tags;
    TreeRef tree_ref;

    Scope bindings;
    runtime::NodeId node;
    runtime::ComponentId selector;
    runtime::ComponentId popover;
    runtime::ComponentId tree;
    runtime::ComponentId empty;
    input::InteractionId selector_hit;
    TreeSelectRef reference;
    std::vector<ItemState> items;
    Signal<String> display{String{}};
    Signal<String> placeholder{String{}};
    Signal<String> empty_label{String{u8"No data"}};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<TreeKeys> selected{TreeKeys{}};
    Signal<TreeKeys> expanded{TreeKeys{}};
    Signal<bool> popup_open{false};
    Signal<bool> input_disabled{false};
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<float> width{180};
    Signal<float> height{256};
    Signal<ThemeConfig> child_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 6> subscriptions;
    std::function<void(String)> on_change;
    std::function<void(bool)> on_open;
    std::function<void(TreeExpandInfo)> on_expand;
    bool controlled_expanded{};
    bool disabled{};
    bool read_only{};
    bool visible{true};
    bool allow_clear{};
    bool geometry_blocked{};
    bool focused_inside{};
    bool focus_pending{};
    bool ready{};
    bool refreshing{};
    bool pending{};
    bool notifying_close{};
    std::optional<std::uint64_t> close_dispatch;
};

TreeKeys tag_values(const TreeSelectState& s, const TreeKeys& input) {
    auto values = input;
    if (!s.checkable.get() || s.strict.get() || s.strategy == TreeSelectCheckedStrategy::ShowAll) {
        return values;
    }
    const auto& nodes = s.model.nodes();
    std::set<std::string, std::less<>> selected;
    std::map<std::string_view, std::size_t> indices;
    for (const auto& key : values) {
        selected.emplace(key.bytes());
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        indices.emplace(nodes[i].key.bytes(), i);
    }
    std::set<std::size_t> parents;
    if (s.strategy == TreeSelectCheckedStrategy::ShowChild) {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].parent != tree_select_no_parent && !nodes[i].disabled && !nodes[i].check_disabled &&
                nodes[i].checkable && selected.contains(nodes[i].key.bytes())) {
                parents.insert(nodes[i].parent);
            }
        }
    }
    std::erase_if(values, [&](const String& key) {
        const auto it = indices.find(key.bytes());
        if (it == indices.end()) {
            return false;
        }
        if (s.strategy == TreeSelectCheckedStrategy::ShowChild) {
            return parents.contains(it->second);
        }
        for (auto parent = nodes[it->second].parent; parent != tree_select_no_parent; parent = nodes[parent].parent) {
            if (nodes[parent].disabled || nodes[parent].check_disabled || !nodes[parent].checkable) {
                break;
            }
            if (selected.contains(nodes[parent].key.bytes())) {
                return true;
            }
        }
        return false;
    });
    return values;
}

using State = std::shared_ptr<TreeSelectState>;

State state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* ptr = services.components().state<State>(id);
    return ptr ? *ptr : State{};
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (*p == parent) {
            return true;
        }
    }
    return false;
}

struct CallbackFailure final {
    WindowComponentServices& services;
    runtime::ComponentId owner;
    int exceptions{std::uncaught_exceptions()};

    ~CallbackFailure() noexcept {
        if (std::uncaught_exceptions() > exceptions) {
            services.retire_failed_component(owner);
        }
    }
};

struct MountTransaction final {
    WindowComponentServices& services;
    runtime::ComponentId id;
    bool complete{};

    ~MountTransaction() noexcept {
        if (!complete) {
            services.retire_failed_component(id);
        }
    }
};

runtime::Rect bounds(WindowComponentServices& services, runtime::ComponentId id) {
    const auto& node = services.nodes().require(services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}
} // namespace

TreeSelectComponentHost::TreeSelectComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TreeSelectComponentHost::~TreeSelectComponentHost() {
    services_->detach(*this);
}

void* TreeSelectComponentHost::begin_mount() noexcept {
    auto* previous = active_tree_select;
    active_tree_select = this;
    return previous;
}

void TreeSelectComponentHost::end_mount(void* previous) noexcept {
    active_tree_select = static_cast<TreeSelectComponentHost*>(previous);
}

void TreeSelectComponentHost::on_destroy() noexcept {
    const auto ids = mounted_;
    for (auto id : ids) {
        static_cast<void>(intact(id));
    }
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void TreeSelectComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

bool TreeSelectComponentHost::intact(runtime::ComponentId id) {
    const auto s = state(*services_, id);
    if (!s || !services_->components().scope(id).active()) {
        return false;
    }
    if (s->ready) {
        for (auto child : {s->selector, s->popover, s->tree, s->empty}) {
            if (!services_->components().contains(child) || !descendant(*services_, child, id)) {
                services_->retire_failed_component(id);
                return false;
            }
        }
    }
    return true;
}

bool TreeSelectComponentHost::editable(runtime::ComponentId id) {
    if (!intact(id)) {
        return false;
    }
    const auto s = state(*services_, id);
    return s->visible && !s->disabled && !s->read_only && !s->geometry_blocked && window_active_ &&
           services_->interactions().component_active(id);
}

TreeSelectSnapshot TreeSelectComponentHost::snapshot(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    if (!s || !s->ready) {
        throw std::out_of_range("TreeSelect component is stale or incomplete");
    }
    const auto popup = services_->popover().snapshot(s->popover);
    TreeSelectSnapshot result{s->selector,
                              s->popover,
                              s->tree,
                              popup.geometry.bounds,
                              s->model.value(),
                              s->model.label(),
                              popup.geometry.visible,
                              s->values,
                              s->query.get(),
                              {}};
    for (const auto& [key, row] : s->tags) {
        if (services_->components().branch_active(row.id)) {
            result.tags.push_back(row.id);
        }
    }
    result.empty = s->empty;
    return result;
}

void TreeSelectComponentHost::refresh(runtime::ComponentId id) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (!s->ready) {
        return;
    }
    s->pending = true;
    if (s->refreshing) {
        return;
    }
    s->refreshing = true;

    struct RefreshGuard final {
        State state;

        ~RefreshGuard() {
            state->refreshing = false;
        }
    } guard{s};

    do {
        s->pending = false;
        const bool active = editable(id);
        // A containing popup temporarily blocks its content while constructing.
        // Commit its initial geometry before treating that temporary state as a
        // lasting suppression. Explicit local gates apply immediately.
        const bool constructing_ancestor = services_->nodes().require(s->node).place_generation == 0 && s->visible &&
                                           !s->disabled && !s->read_only && !s->geometry_blocked && window_active_;
        const bool suppress = !active && !constructing_ancestor;
        if (suppress) {
            s->tree_ref.reset();
            s->focus_pending = false;
        }
        const bool notify = suppress && s->model.suppress();
        const bool input_blocked = !s->visible || s->disabled || s->geometry_blocked || !window_active_;
        if (services_->components().set_branch_active(id, s->visible)) {
            services_->mark_scene_structure_dirty();
        }
        services_->set_input_blocked(id, input_blocked);
        if (!intact(id)) {
            return;
        }
        s->input_disabled.set(s->disabled);
        if (!intact(id)) {
            return;
        }
        s->action_disabled.set(!active);
        if (!intact(id)) {
            return;
        }
        const bool many = s->multiple.get() || s->checkable.get();
        s->input_read_only.set(s->read_only || !s->searchable);
        if (!intact(id)) {
            return;
        }
        s->display.set(s->searchable && s->model.open() ? s->query.get()
                       : many                           ? String{}
                                                        : (s->labels.contains(s->model.value().bytes())
                                                               ? s->labels.at(std::string{s->model.value().bytes()})
                                                               : s->model.label()));
        if (!intact(id)) {
            return;
        }
        s->half_state.set(s->half);
        if (!intact(id)) {
            return;
        }
        s->selected.set(many ? s->values : s->model.value().empty() ? TreeKeys{} : TreeKeys{s->model.value()});
        if (!intact(id)) {
            return;
        }
        s->clear_visible.set(s->allow_clear && (many ? !s->values.empty() : !s->model.value().empty()) && active);

        if (!intact(id)) {
            return;
        }
        const bool next_open = active && s->model.open();
        if (s->motion && s->motion_open != next_open) {
            s->motion_open = next_open;
            if (next_open) {
                s->motion->restart(true);
            } else {
                s->motion->settle();
            }
        }
        s->popup_open.set(next_open);
        if (!intact(id)) {
            return;
        }
        const bool any_root = !services_->tree().empty(s->tree);
        if (services_->components().set_branch_active(s->empty, !any_root)) {
            services_->nodes().require(services_->components().root(s->empty)).out_of_flow = any_root;
            services_->mark_scene_structure_dirty();
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        }
        if (notify) {
            notify_open(id, false);
            if (!intact(id)) {
                return;
            }
        }
    } while (s->pending);
}

void TreeSelectComponentHost::request_open(runtime::ComponentId id, bool open, bool keyboard) {
    if (!intact(id) || (open && !editable(id))) {
        return;
    }
    const auto s = state(*services_, id);
    s->focus_pending = open && keyboard;
    if (!open && !editable(id)) {
        refresh(id);
        return;
    }
    const bool notify = s->model.request_open(open);
    try {
        refresh(id);
        if (notify && intact(id)) {
            notify_open(id, open);
        }
    } catch (...) {
        s->focus_pending = false;
        throw;
    }
    if (intact(id)) {
        if (!s->model.open()) {
            s->focus_pending = false;
        }
        if (s->focus_pending) {
            services_->request_frame();
        }
    }
}

void TreeSelectComponentHost::notify_open(runtime::ComponentId id, bool open) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (open) {
        s->close_dispatch.reset();
        const auto callback = s->on_open;
        if (callback) {
            callback(true);
        }
        return;
    }
    const auto dispatch = services_->pointer().dispatch_token();
    if (s->notifying_close || (dispatch && s->close_dispatch == dispatch)) {
        return;
    }
    s->close_dispatch = dispatch;
    s->notifying_close = true;

    struct CloseGuard final {
        State state;

        ~CloseGuard() {
            state->notifying_close = false;
        }
    } guard{s};

    const auto callback = s->on_open;
    if (callback) {
        callback(false);
    }
}

void TreeSelectComponentHost::choose(runtime::ComponentId id, String key) {
    if (!editable(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (s->multiple.get() || s->checkable.get()) {
        auto values = s->values;
        if (key.empty()) {
            values.clear();
        } else if (std::ranges::find(values, key) != values.end()) {
            std::erase(values, key);
        } else {
            values.push_back(key);
        }
        choose_values(id, std::move(values));
        return;
    }
    const bool notify = s->model.request_value(key);
    const auto callback = s->on_change;
    refresh(id);
    if (notify && callback && intact(id)) {
        callback(key);
    }
    if (notify && intact(id) && s->props.on_labels_) {
        s->props.on_labels_(
            key.empty()
                ? TreeSelectLabelValues{}
                : TreeSelectLabelValues{
                      {key, s->labels.contains(key.bytes()) ? s->labels.at(std::string{key.bytes()}) : key, false}});
    }
    if (intact(id) && !s->props.query_) {
        s->query.set({});
    }
    if (intact(id)) {
        request_open(id, false);
    }
}

void TreeSelectComponentHost::query(runtime::ComponentId id, String value) {
    if (!editable(id)) {
        return;
    }
    const auto s = state(*services_, id);
    CallbackFailure failure{*services_, id};
    if (!s->searchable) {
        return;
    }
    if (!s->props.query_) {
        s->query.set(value);
    }
    const auto callback = s->props.on_search_;
    request_open(id, true);
    if (intact(id) && callback) {
        callback(std::move(value));
    }
    if (intact(id)) {
        refresh(id);
    }
}

void TreeSelectComponentHost::choose_values(runtime::ComponentId id, TreeKeys values, TreeKeys half) {
    if (!editable(id)) {
        return;
    }
    const auto s = state(*services_, id);
    CallbackFailure failure{*services_, id};
    TreeKeys canonical;
    std::set<std::string_view> seen;
    for (const auto& key : values) {
        if (seen.insert(key.bytes()).second) {
            canonical.push_back(key);
        }
    }
    values = std::move(canonical);
    const bool changed = values != s->values || half != s->half;
    if (!changed) {
        return;
    }
    if (!s->props.values_ && !s->props.labels_) {
        s->values = values;
        s->half = half;
    }
    TreeKeys output = tag_values(*s, values);
    TreeSelectLabelValues labeled;
    for (const auto& key : output) {
        labeled.push_back({key, s->labels.contains(key.bytes()) ? s->labels.at(std::string{key.bytes()}) : key, false});
    }
    if (s->strict.get()) {
        for (const auto& key : half) {
            labeled.push_back(
                {key, s->labels.contains(key.bytes()) ? s->labels.at(std::string{key.bytes()}) : key, true});
        }
    }
    refresh(id);
    if (intact(id) && s->props.on_values_) {
        s->props.on_values_(output);
    }
    if (intact(id) && s->props.on_labels_) {
        s->props.on_labels_(std::move(labeled));
    }
    if (intact(id) && !s->props.query_) {
        s->query.set({});
        refresh(id);
    }
}

void TreeSelectComponentHost::tags(runtime::ComponentId id) {
    auto s = state(*services_, id);
    if (!s || !s->tag_ready) {
        return;
    }
    const auto values = tag_values(*s, s->values);
    std::map<std::string, bool, std::less<>> node_disabled;
    for (const auto& node : s->model.nodes()) {
        node_disabled.emplace(std::string{node.key.bytes()},
                              node.disabled || (s->checkable.get() && (node.check_disabled || !node.checkable)));
    }
    const auto disabled = [&](const String& key) {
        const auto node = node_disabled.find(key.bytes());
        return !editable(id) || (node != node_disabled.end() && node->second);
    };

    std::string signature =
        std::to_string(s->width.get()) + "/" + std::to_string(bounds(*services_, s->selector).height) + "/" +
        std::to_string(services_->components().theme_scope(id)->snapshot().identity()) + "/" +
        std::to_string(editable(id)) + "/" + std::to_string(s->multiple.get()) + "/" +
        std::to_string(s->checkable.get()) + "/" + std::to_string(s->rtl) + "/" + std::to_string(s->max_tags.index()) +
        "/" +
        std::to_string(std::holds_alternative<std::size_t>(s->max_tags) ? std::get<std::size_t>(s->max_tags) : 0) +
        "/" + std::to_string(static_cast<int>(s->size.get())) + "/" +
        std::to_string(static_cast<int>(s->variant.get()));
    for (const auto& key : values) {
        const auto label = s->labels.contains(key.bytes()) ? s->labels.at(std::string{key.bytes()}) : key;
        signature += "/" + std::to_string(disabled(key)) + "/" + std::to_string(key.bytes().size()) + ":" +
                     std::string{key.bytes()} + ":" + std::string{label.bytes()};
    }
    if (signature == s->tag_signature) {
        return;
    }
    s->tag_signature = std::move(signature);
    {
        const auto rect = bounds(*services_, s->selector);
        s->tag_geometry = {rect.width, rect.height};
    }
    services_->layout().set_layout(services_->components().root(s->tag_host),
                                   layout::FlexLayout{.right_to_left = s->rtl});

    const bool responsive = std::holds_alternative<TreeSelectTagCount>(s->max_tags);
    std::size_t limit = responsive ? values.size() : std::get<std::size_t>(s->max_tags);
    const bool many = s->multiple.get() || s->checkable.get();
    std::vector<std::pair<String, String>> wanted;
    if (many) {
        for (const auto& key : values) {

            String label = s->labels.contains(key.bytes()) ? s->labels.at(std::string{key.bytes()}) : key;
            auto bytes = label.bytes();
            std::size_t end = 0;
            std::size_t count = 0;
            while (end < bytes.size() && count < s->props.max_tag_text_) {
                ++end;
                while (end < bytes.size() && (static_cast<unsigned char>(bytes[end]) & 0xC0) == 0x80) {
                    ++end;
                }
                ++count;
            }
            if (end < bytes.size()) {
                label = String::from_utf8(std::string{bytes.substr(0, end)} + "…").value();
            }
            wanted.emplace_back(key, label);
        }
        wanted.emplace_back(String{}, String{});
    }
    std::set<std::string, std::less<>> keep;
    for (const auto& [key, label] : wanted) {
        keep.emplace(key.bytes());
    }
    for (auto it = s->tags.begin(); it != s->tags.end();) {
        if (!keep.contains(it->first)) {
            auto child = it->second.id;
            const auto focus = services_->focus().state().focused;
            const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
            if (record && descendant(*services_, record->component, child)) {
                static_cast<void>(s->reference.focus());
                if (!intact(id)) {
                    return;
                }
            }
            it = s->tags.erase(it);
            services_->destroy(child);
            if (!intact(id)) {
                return;
            }
        } else {
            ++it;
        }
    }
    for (const auto& [key, label] : wanted) {
        const std::string map_key{key.bytes()};
        if (!s->tags.contains(map_key)) {
            MountTransaction transaction{*services_, id};
            TreeSelectState::TagRow row;
            row.label.set(label);
            row.disabled.set(disabled(key));
            const Content tag_content{[&, this] {
                FormPrivateEditorScope private_editor;
                auto& c = runtime::require_component_build_context();
                row.id = c.mount_component<int>(0);
                const auto node = c.root(row.id);
                services_->layout().set_layout(node, layout::BoxLayout{});
                c.on_resource_cleanup(row.id, [services = services_, node, component = row.id] {
                    services->interactions().set_input_blocked(component, false);
                    services->layout().remove_layout(node);
                });
                auto close = [this, id, key] {
                    if (!editable(id)) {
                        return;
                    }
                    auto s = state(*services_, id);
                    static_cast<void>(s->reference.focus());
                    if (!intact(id)) {
                        return;
                    }
                    const auto node = std::ranges::find(s->model.nodes(), key, &TreeSelectNode::key);
                    if (node != s->model.nodes().end() &&
                        (node->disabled || (s->checkable.get() && (node->check_disabled || !node->checkable)))) {
                        return;
                    }
                    if (s->checkable.get() && !s->strict.get() && node != s->model.nodes().end()) {
                        services_->tree().check(s->tree, key);
                        return;
                    }
                    auto next = s->values;
                    std::erase(next, key);
                    choose_values(id, std::move(next));
                };
                c.mount_slot(
                    row.id, Content{[&, close] {
                        if (s->props.tag_ && !key.empty()) {
                            SlotContentAccess::function(s->props.tag_({key, row.label, row.disabled, close}))();
                        } else {
                            Tag(TagProps{}.closable(!key.empty()).visible(true).disabled(row.disabled).onClose(close),
                                TagContent{[text = row.label] { Text(TextProps{}.content(text)); }});
                        }
                    }});
            }};
            if (s->composing) {
                runtime::require_component_build_context().mount_slot(s->tag_host, tag_content);
            } else {
                services_->append_slot(s->tag_host, tag_content);
            }
            if (!intact(id)) {
                return;
            }
            s->tags.emplace(map_key, std::move(row));
            transaction.complete = true;
        }
        auto& row = s->tags.at(map_key);
        row.label.set(label);
        if (!intact(id)) {
            return;
        }
        row.disabled.set(disabled(key));
        if (!intact(id)) {
            return;
        }
    }
    std::size_t shown = std::min(limit, values.size());
    const bool measured = responsive && !s->composing && many;
    if (measured) {
        const auto r = bounds(*services_, s->selector);
        const auto token = InputTokenAccess::get(services_->components().theme_scope(s->selector)->snapshot());
        const auto size = token.size(services_->input_runtime()->size(s->selector));
        const auto main = services_->components().root(s->selector);
        const auto children = services_->nodes().require(main).children;
        const auto tag_node = services_->components().root(s->tag_host);
        auto& holder = services_->nodes().require(tag_node);
        const auto old_out = holder.out_of_flow;
        holder.out_of_flow = true;
        const auto prefix = services_->layout().measure_child(children[0], {0, r.width, 0, r.height});
        if (!intact(id)) {
            return;
        }
        services_->nodes().require(tag_node).out_of_flow = old_out;
        const auto suffix = services_->layout().measure_child(children[2], {0, r.width, 0, r.height});
        if (!intact(id)) {
            return;
        }
        const float budget =
            std::max(0.F, r.width - prefix.width - suffix.width -
                              2 * (size.padding_inline + token.border_width + token.affix_padding) - size.font_size);
        shown = 0;
        float sum = 0;
        for (std::size_t count = 0; count <= values.size(); ++count) {
            auto& overflow = s->tags.at("");
            overflow.label.set(count < values.size()
                                   ? String::from_utf8("+" + std::to_string(values.size() - count)).value()
                                   : String{});
            if (!intact(id)) {
                return;
            }
            const auto overflow_width =
                count < values.size() ? services_->layout()
                                            .measure_child(services_->components().root(overflow.id),
                                                           {0, std::numeric_limits<float>::infinity(), 0, r.height})
                                            .width
                                      : 0;
            if (sum + overflow_width <= budget + .01F) {
                shown = count;
            }
            if (count == values.size()) {
                break;
            }
            auto& tag = s->tags.at(std::string{values[count].bytes()});
            sum += services_->layout()
                       .measure_child(services_->components().root(tag.id),
                                      {0, std::numeric_limits<float>::infinity(), 0, r.height})
                       .width;
            if (!intact(id)) {
                return;
            }
            if (sum > budget) {
                break;
            }
        }
    }
    bool changed = false;
    std::vector<runtime::NodeId> order;
    for (std::size_t index = 0; index < wanted.size(); ++index) {
        const auto& key = wanted[index].first;
        auto& tag = s->tags.at(std::string{key.bytes()});
        const bool visible = key.empty() ? shown < values.size() : index < shown;
        if (key.empty()) {
            tag.label.set(visible ? String::from_utf8("+" + std::to_string(values.size() - shown)).value() : String{});
            if (!intact(id)) {
                return;
            }
        }
        const auto node = services_->components().root(tag.id);
        order.push_back(node);
        auto& record = services_->nodes().require(node);
        changed |= record.out_of_flow != !visible;
        record.out_of_flow = !visible;
        changed |= services_->components().set_branch_active(tag.id, visible);
        if (!visible) {
            const auto focus = services_->focus().state().focused;
            const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
            if (record && descendant(*services_, record->component, tag.id)) {
                static_cast<void>(s->reference.focus());
                if (!intact(id)) {
                    return;
                }
            }
        }
        services_->set_input_blocked(tag.id, !visible || !editable(id));
        if (!intact(id)) {
            return;
        }
    }
    auto& parent = services_->nodes().require(services_->components().root(s->tag_host));
    if (parent.children != order) {
        parent.children = std::move(order);
        changed = true;
    }
    if (changed || measured) {
        if (changed) {
            services_->mark_scene_structure_dirty();
        }
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::HitTest);
    }
    s->shown_tags = shown;
}

void TreeSelectComponentHost::update_theme(runtime::ComponentId id) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const auto& snapshot = services_->components().theme_scope(id)->snapshot();
    const auto& token = snapshot.tree_select();
    const auto& t = token.selector;
    auto config = std::make_shared<ThemeConfig>();
    config->input.tokens.background = t.selector_background;
    config->input.tokens.hover_background = t.selector_background;
    config->input.tokens.active_background = t.selector_background;
    config->input.tokens.hover_border_color = t.hover_border_color;
    config->input.tokens.active_border_color = t.active_border_color;
    const auto& original = InputTokenAccess::get(snapshot).active_shadow;
    std::array<ShadowLayer, ShadowList::capacity> shadow{};
    std::copy(original.layers().begin(), original.layers().end(), shadow.begin());
    for (std::size_t i = 0; i < original.size(); ++i) {
        shadow[i].color = t.active_outline_color;
    }
    config->input.tokens.active_shadow = ShadowList{std::span{shadow.data(), original.size()}};
    config->popover.tokens.padding = dp(t.popup_padding);
    config->popover.tokens.gap = dp(t.gap);
    config->popover.tokens.max_width = dp(std::max(1.0F, s->width.get()));
    config->popover.tokens.background = t.popup_background;
    config->popover.tokens.border_radius = dp(t.border_radius);
    config->popover.tokens.shadow = t.shadow;
    config->popover.tokens.z_index_popup = t.z_index_popup;
    config->popover.tokens.font_family = token.tree.metrics.font_family;
    config->popover.tokens.font_size = dp(token.tree.metrics.font_size);
    config->popover.tokens.line_height = dp(token.tree.metrics.line_height);
    config->tree.tokens.background = token.tree.colors.background;
    config->tree.tokens.text = token.tree.colors.text;
    config->tree.tokens.disabled = token.tree.colors.disabled;
    config->tree.tokens.node_hover_background = token.tree.colors.node_hover_background;
    config->tree.tokens.node_hover_text = token.tree.colors.node_hover_text;
    config->tree.tokens.node_selected_background = token.tree.colors.node_selected_background;
    config->tree.tokens.node_selected_text = token.tree.colors.node_selected_text;
    config->tree.tokens.checkbox_border = token.tree.colors.checkbox_border;
    config->tree.tokens.checkbox_background = token.tree.colors.checkbox_background;
    config->tree.tokens.checkbox_checked = token.tree.colors.checkbox_checked;
    config->tree.tokens.checkbox_mark = token.tree.colors.checkbox_mark;
    config->tree.tokens.focus = token.tree.colors.focus;
    config->tree.tokens.title_height = dp(token.tree.metrics.title_height);
    config->tree.tokens.switcher_size = dp(token.tree.metrics.switcher_size);
    config->tree.tokens.indent_size = dp(token.tree.metrics.indent_size);
    config->tree.tokens.row_gap = dp(token.tree.metrics.row_gap);
    config->tree.tokens.icon_size = dp(token.tree.metrics.icon_size);
    config->tree.tokens.icon_gap = dp(token.tree.metrics.icon_gap);
    config->tree.tokens.padding_inline = dp(token.tree.metrics.padding_inline);
    config->tree.tokens.border_radius = dp(token.tree.metrics.border_radius);
    config->tree.tokens.checkbox_size = dp(token.tree.metrics.checkbox_size);
    config->tree.tokens.border_width = dp(token.tree.metrics.border_width);
    config->tree.tokens.focus_width = dp(token.tree.metrics.focus_width);
    config->tree.tokens.focus_offset = dp(token.tree.metrics.focus_offset);
    config->tree.tokens.font_size = dp(token.tree.metrics.font_size);
    config->tree.tokens.line_height = dp(token.tree.metrics.line_height);
    config->tree.tokens.font_family = token.tree.metrics.font_family;
    s->child_theme.set(*config);
}

void TreeSelectComponentHost::mount(const TreeSelectProps& p, const std::vector<TreeSelectItem>& items) {
    FormPrivateEditorScope private_editor;
    if ((p.value_ && p.default_value_) || (p.open_ && p.default_open_) || (p.expanded_ && p.default_expanded_)) {
        throw std::invalid_argument("TreeSelect controlled and default properties conflict");
    }
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("TreeSelect requires a window Input host");
    }
    if ((p.value_ && (p.values_ || p.labels_ || p.default_values_)) ||
        (p.values_ && (p.labels_ || p.default_values_)) || (p.labels_ && p.default_values_)) {
        throw std::invalid_argument("TreeSelect value authorities conflict");
    }
    auto s = std::make_shared<TreeSelectState>(TreeSelectModel{{},
                                                               p.default_value_.value_or(String{}),
                                                               (p.value_.has_value() || p.labels_.has_value()),
                                                               p.default_open_.value_or(false),
                                                               p.open_.has_value()});
    s->props = p;
    s->values = p.default_values_.value_or(TreeKeys{});
    s->tree_ref = p.tree_ref_.value_or(TreeRef{});
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<State>(s);
    MountTransaction transaction{services, id};
    s->node = build.root(id);
    s->motion = std::make_unique<NavigationTransition>(services, id);
    mounted_.push_back(id);
    s->reference = p.ref_.value_or(TreeSelectRef{});
    s->on_change = p.on_change_;
    s->on_open = p.on_open_;
    s->on_expand = p.on_expand_;
    s->controlled_expanded = p.expanded_.has_value();
    s->expanded.set(p.default_expanded_.value_or(TreeKeys{}));
    services.layout().set_layout(s->node, layout::BoxLayout{{}, true});
    build.on_resource_cleanup(id, [&services, id, node = s->node, weak = std::weak_ptr{s}] {
        if (const auto current = weak.lock()) {
            current->bindings.dispose();
        }
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    runtime::connect_layout_style(s->bindings, p.layout_, s->node, services.nodes(), services.dirty(),
                                  [this, id] { return intact(id); });
    if (!intact(id)) {
        return;
    }
    const auto bind_value = [&]<class T, class F>(const Prop<T>& prop, F apply) {
        connect_prop(s->bindings, prop, [this, id, apply](T value) {
            run_untracked([&] {
                if (intact(id)) {
                    const auto current = state(*services_, id);
                    apply(*current, std::move(value));
                    if (intact(id)) {
                        refresh(id);
                    }
                }
            });
        });
        return intact(id);
    };
    const auto empty_content = localized_prop(p.not_found_content_, build.config_scope(),
                                              [](const LocaleData& data) { return data.common.empty; });
    connect_prop(s->bindings, empty_content, [this, id](String value) {
        if (intact(id)) {
            CallbackFailure failure{*services_, id};
            const auto label = state(*services_, id)->empty_label;
            label.set(std::move(value));
        }
    });
    if (!intact(id)) {
        return;
    }
    if ((p.value_ && !bind_value(*p.value_, [](auto& v, String x) { v.model.reconcile_value(std::move(x)); })) ||
        (p.open_ && !bind_value(*p.open_,
                                [](auto& v, bool x) {
                                    v.model.reconcile_open(x);
                                    if (!x) {
                                        v.focus_pending = false;
                                    }
                                })) ||
        (p.expanded_ && !bind_value(*p.expanded_, [](auto& v, TreeKeys x) { v.expanded.set(std::move(x)); })) ||
        !bind_value(p.visible_, [](auto& v, bool x) { v.visible = x; }) ||
        !bind_value(p.disabled_, [](auto& v, bool x) { v.disabled = x; }) ||
        !bind_value(p.read_only_, [](auto& v, bool x) { v.read_only = x; }) ||
        !bind_value(p.allow_clear_, [](auto& v, bool x) { v.allow_clear = x; }) ||
        !bind_value(p.placeholder_, [](auto& v, String x) { v.placeholder.set(std::move(x)); }) ||
        !bind_value(p.size_, [](auto& v, ControlSize x) { v.size.set(x); }) ||
        !bind_value(p.variant_, [](auto& v, InputVariant x) { v.variant.set(x); }) ||
        !bind_value(p.status_, [](auto& v, InputStatus x) { v.status.set(x); }) ||
        !bind_value(p.list_height_, [](auto& v, float x) {
            if (!std::isfinite(x) || x <= 0) {
                throw std::invalid_argument("TreeSelect listHeight must be finite and positive");
            }
            v.height.set(x);
        })) {
        return;
    }
    bind_value(p.multiple_, [](auto& s, bool v) { s.multiple.set(v); });
    bind_value(p.checkable_, [](auto& s, bool v) { s.checkable.set(v); });
    bind_value(p.strict_, [](auto& s, bool v) { s.strict.set(v); });
    bind_value(p.search_, [](auto& s, bool v) { s.searchable = v; });
    bind_value(p.rtl_, [](auto& s, bool v) { s.rtl = v; });
    bind_value(p.label_value_, [](auto& s, bool v) { s.label_value = v; });
    bind_value(p.strategy_, [](auto& s, auto v) { s.strategy = v; });
    bind_value(p.max_tags_, [](auto& s, auto v) { s.max_tags = v; });
    if (p.values_) {
        bind_value(*p.values_, [](auto& s, TreeKeys v) { s.values = std::move(v); });
    }
    if (p.labels_) {
        bind_value(*p.labels_, [](auto& s, TreeSelectLabelValues v) {
            s.values.clear();
            s.half.clear();
            for (const auto& x : v) {
                (x.half_checked ? s.half : s.values).push_back(x.value);
                s.labels.insert_or_assign(std::string{x.value.bytes()}, x.label);
            }
            if (!s.multiple.get() && !s.checkable.get()) {
                s.model.reconcile_value(s.values.empty() ? String{} : s.values.front());
            }
        });
    }
    if (p.query_) {
        bind_value(*p.query_, [](auto& s, String v) { s.query.set(std::move(v)); });
    }
    connect_prop(
        s->bindings, Prop<std::vector<TreeSelectItem>>{bind([p, items] {
            auto value = p.items_ ? read_prop(*p.items_) : items;
            const auto read = [&](auto&& self, std::vector<TreeSelectItem>& list) -> void {
                for (auto& item : list) {
                    item.title_ = read_prop(item.title_);
                    item.disabled_ = read_prop(item.disabled_);
                    item.visible_ = read_prop(item.visible_);
                    item.selectable_ = read_prop(item.selectable_);
                    item.checkable_ = read_prop(item.checkable_);
                    item.check_disabled_ = read_prop(item.check_disabled_);
                    item.leaf_ = read_prop(item.leaf_);
                    self(self, item.children_);
                }
            };
            read(read, value);
            return value;
        })},
        [this, id](const auto& items) {
            run_untracked([&] {
                if (!intact(id)) {
                    return;
                }
                const auto s = state(*services_, id);
                std::vector<TreeSelectNode> nodes;
                std::map<std::string, String, std::less<>> labels = s->labels;
                const auto convert = [&](auto&& self, const std::vector<TreeSelectItem>& list, std::size_t parent,
                                         std::size_t depth) -> std::vector<TreeItem> {
                    if (depth >= 64 && !list.empty()) {
                        throw std::invalid_argument("TreeSelect depth exceeds 64");
                    }
                    std::vector<TreeItem> result;
                    for (const auto& item : list) {
                        if (nodes.size() >= 100000) {
                            throw std::invalid_argument("TreeSelect size exceeds 100000");
                        }
                        const auto index = nodes.size();
                        const auto title = read_prop(item.title_);
                        nodes.push_back({item.key_, title, parent, read_prop(item.disabled_), read_prop(item.visible_),
                                         read_prop(item.check_disabled_), read_prop(item.checkable_)});
                        labels.insert_or_assign(std::string{item.key_.bytes()}, title);
                        auto children = self(self, item.children_, index, depth + 1);
                        auto tree = TreeItem{item.key_, item.title_, std::move(children)}
                                        .disabled(item.disabled_)
                                        .visible(item.visible_)
                                        .selectable(item.selectable_)
                                        .checkable(item.checkable_)
                                        .disableCheckbox(item.check_disabled_)
                                        .isLeaf(item.leaf_)
                                        .contentRevision(item.revision_);
                        if (item.rich_title_) {
                            tree.richTitle(*item.rich_title_);
                        }
                        if (item.icon_) {
                            tree.icon(*item.icon_);
                        }
                        result.push_back(std::move(tree));
                    }
                    return result;
                };
                auto tree = convert(convert, items, tree_select_no_parent, 0);
                s->model.topology(std::move(nodes));
                s->labels = std::move(labels);
                s->topology.set(std::move(tree));
                refresh(id);
            });
        },
        [](const auto&, const auto&) { return false; });
    s->display.set(s->model.label());
    s->selected.set(s->model.value().empty() ? TreeKeys{} : TreeKeys{s->model.value()});
    s->input_disabled.set(s->disabled);
    s->action_disabled.set(s->disabled || !s->visible || s->read_only);
    services.components().set_branch_active(id, s->visible);
    services.set_input_blocked(id, s->disabled || !s->visible || !window_active_);
    if (!intact(id)) {
        return;
    }
    update_theme(id);
    build.mount_slot(
        id, Content{[&] {
            MountTransaction inner{services, id};
            Theme(ThemeProps{}.config(s->child_theme), ThemeContent{[&] {
                      Popover(
                          PopoverProps{}
                              .trigger(PopoverTriggerMode::Manual)
                              .placement(p.placement_)
                              .arrow(false)
                              .open(s->popup_open)
                              .disabled(s->input_disabled)
                              .onOpenChange([this, id](bool value) { request_open(id, value); }),
                          PopoverTrigger{[&] {
                              FormPrivateEditorScope private_editor;
                              Input(InputProps{}
                                        .value(s->display)
                                        .placeholder(s->placeholder)
                                        .readOnly(s->input_read_only)
                                        .direction(bind([rtl = p.rtl_] {
                                            return read_prop(rtl) ? TextDirection::RightToLeft
                                                                  : TextDirection::LeftToRight;
                                        }))
                                        .disabled(s->input_disabled)
                                        .size(s->size)
                                        .variant(s->variant)
                                        .status(s->status)
                                        .ref(s->reference)
                                        .autoFocus(p.auto_focus_)
                                        .onChange([this, id](String value) { query(id, std::move(value)); })
                                        .onFocus(p.on_focus_)
                                        .onBlur(p.on_blur_),
                                    InputPrefix{[&] {
                                        auto& c = runtime::require_component_build_context();
                                        if (p.prefix_) {
                                            SlotContentAccess::function (*p.prefix_)();
                                        }
                                        s->tag_host = c.mount_component<int>(0);
                                        const auto node = c.root(s->tag_host);
                                        services.layout().set_layout(node, layout::FlexLayout{});
                                        c.on_resource_cleanup(
                                            s->tag_host, [&services, node] { services.layout().remove_layout(node); });
                                        s->tag_ready = true;
                                    }},
                                    InputSuffix{[&] {
                                        mount_input_affix_action(
                                            services, IconSource{IconName::CloseCircleFilled}, s->action_disabled,
                                            [this, id] { choose(id, {}); }, s->clear_visible, false, false);
                                        if (p.suffix_) {
                                            SlotContentAccess::function (*p.suffix_)();
                                        } else {
                                            Icon(IconProps{}.name(IconName::DownOutlined).tone(TextTone::Secondary));
                                        }
                                    }});
                              if (intact(id)) {
                                  const auto input = inputs->mounted_inputs().back();
                                  s->selector = input.component;
                                  s->selector_hit = input.interaction;
                                  inputs->configure_form_reset(s->selector, [this, id] {
                                      if (!intact(id)) {
                                          return;
                                      }
                                      CallbackFailure failure{*services_, id};
                                      auto s = state(*services_, id);
                                      s->tree_ref.reset();
                                      if (!intact(id)) {
                                          return;
                                      }
                                      const auto order = services_->interactions().declaration_order();
                                      const std::vector<input::InteractionId> hits(order.begin(), order.end());
                                      for (auto hit : hits) {
                                          const auto* record = services_->interactions().find(hit);
                                          if (record && descendant(*services_, record->component, id)) {
                                              if (hit == s->selector_hit) {
                                                  // Form resets preserve the existing selector focus contract.
                                                  services_->pointer().cancel_pointer_interaction(hit);
                                              } else {
                                                  services_->pointer().cancel_interaction(hit);
                                              }
                                              if (!intact(id)) {
                                                  return;
                                              }
                                          }
                                      }
                                      s->query.set({});
                                      s->focus_pending = false;
                                      static_cast<void>(s->model.suppress());
                                      refresh(id);
                                  });
                              }
                          }},
                          PopoverSlots{
                              .content = PopoverContent{[&] {
                                  const Content default_content{[&, this] {
                                      auto tree_props =
                                          TreeProps{}
                                              .items(s->topology)
                                              .multiple(bind([multiple = s->multiple, checkable = s->checkable] {
                                                  return multiple.get() || checkable.get();
                                              }))
                                              .checkable(s->checkable)
                                              .checkStrictly(s->strict)
                                              .searchValue(s->query)
                                              .filterTreeNode(p.filter_)
                                              .virtualized(p.virtual_)
                                              .rowHeight(p.row_height_)
                                              .rtl(p.rtl_)
                                              .showLine(p.lines_)
                                              .loadData(p.loader_)
                                              .onLoad([this, id](TreeLoadInfo info) {
                                                  if (!intact(id)) {
                                                      return;
                                                  }
                                                  const auto s = state(*services_, id);
                                                  if (info.error.empty()) {
                                                      std::vector<TreeSelectNode> nodes;
                                                      const auto collect = [&](auto&& self,
                                                                               const std::vector<TreeItem>& list,
                                                                               std::size_t parent) -> void {
                                                          for (const auto& item : list) {
                                                              const auto index = nodes.size();
                                                              auto title = read_prop(item.title_);
                                                              nodes.push_back({item.key_, title, parent,
                                                                               read_prop(item.disabled_),
                                                                               read_prop(item.visible_),
                                                                               read_prop(item.check_disabled_),
                                                                               read_prop(item.checkable_)});
                                                              s->labels.insert_or_assign(std::string{item.key_.bytes()},
                                                                                         title);
                                                              self(self, item.children_, index);
                                                          }
                                                      };
                                                      collect(collect, services_->tree().data(s->tree),
                                                              tree_select_no_parent);
                                                      s->model.topology(std::move(nodes));
                                                      refresh(id);
                                                  }
                                                  if (intact(id) && s->props.on_load_) {
                                                      s->props.on_load_(std::move(info));
                                                  }
                                              })
                                              .ref(s->tree_ref)
                                              .checkedKeys(s->selected)
                                              .halfCheckedKeys(s->half_state)
                                              .onCheck([this, id](TreeCheckInfo info) {
                                                  const auto s = state(*services_, id);
                                                  if (!s) {
                                                      return;
                                                  }
                                                  std::set<std::string_view> known;
                                                  for (const auto& node : s->model.nodes()) {
                                                      known.insert(node.key.bytes());
                                                  }
                                                  for (const auto& key : s->values) {
                                                      if (!known.contains(key.bytes())) {
                                                          info.checked_keys.push_back(key);
                                                      }
                                                  }
                                                  choose_values(id, std::move(info.checked_keys),
                                                                std::move(info.half_checked_keys));
                                              })
                                              .expandedKeys(s->expanded)
                                              .selectedKeys(s->selected)
                                              .disabled(s->action_disabled)
                                              .onExpand([this, id](TreeExpandInfo info) {
                                                  if (!editable(id)) {
                                                      return;
                                                  }
                                                  const auto current = state(*services_, id);
                                                  const auto callback = current->on_expand;
                                                  if (!current->controlled_expanded) {
                                                      current->expanded.set(info.expanded_keys);
                                                  }
                                                  if (callback && intact(id)) {
                                                      callback(std::move(info));
                                                  }
                                              })
                                              .onSelect([this, id](TreeSelectInfo info) {
                                                  const auto s = state(*services_, id);
                                                  if (s && (s->multiple.get() || s->checkable.get())) {
                                                      if (!s->checkable.get()) {
                                                          choose(id, std::move(info.key));
                                                      } else {
                                                          services_->tree().check(s->tree, info.key);
                                                      }
                                                  } else {
                                                      choose(id, std::move(info.key));
                                                  }
                                              })
                                              .layout(
                                                  LayoutStyle{}
                                                      .width(bind([width = s->width] { return dp(width.get()); }))
                                                      .height(bind([height = s->height] { return dp(height.get()); })));
                                      if (p.loaded_) {
                                          tree_props.loadedKeys(*p.loaded_);
                                      }
                                      Tree(std::move(tree_props));
                                      if (!intact(id)) {
                                          return;
                                      }
                                      s->tree = services.tree().mounted().back();
                                      auto& c = runtime::require_component_build_context();
                                      s->empty = c.mount_component<int>(0);
                                      const auto node = c.root(s->empty);
                                      services.layout().set_layout(node, layout::BoxLayout{});
                                      c.on_resource_cleanup(
                                          s->empty, [&services, node] { services.layout().remove_layout(node); });
                                      c.mount_slot(s->empty,
                                                   Content{[&] { Text(TextProps{}.content(s->empty_label)); }});
                                      services.components().set_branch_active(s->empty, items.empty());
                                      services.nodes().require(node).out_of_flow = !items.empty();
                                  }};
                                  if (p.popup_) {
                                      SlotContentAccess::function(p.popup_(
                                          [default_content] { SlotContentAccess::function(default_content)(); }))();
                                  } else {
                                      SlotContentAccess::function(default_content)();
                                  }
                              }}});
                      if (intact(id)) {
                          s->popover = services.popover().mounted().back();
                      }
                  }});
            inner.complete = intact(id);
        }});
    if (!intact(id)) {
        return;
    }
    s->ready = true;
    if (!intact(id)) {
        return;
    }
    refresh(id);
    if (!intact(id)) {
        return;
    }
    const auto theme = services.components().theme_scope(id);
    const std::array accessors{
        &theme_runtime::ThemeScope::tree_select_colors,      &theme_runtime::ThemeScope::tree_select_metrics,
        &theme_runtime::ThemeScope::tree_select_effects,     &theme_runtime::ThemeScope::tree_select_order,
        &theme_runtime::ThemeScope::tree_select_tree_colors, &theme_runtime::ThemeScope::tree_select_tree_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s->subscriptions[i] =
            theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    on_focus_changed();
    s->composing = false;
    transaction.complete = intact(id);
}

bool TreeSelectComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    const auto focus = services_->focus().state().focused;
    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
    if (!record) {
        return false;
    }
    const auto component = record->component;
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto id = *it;
        if (!editable(id) || !descendant(*services_, component, id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const bool selector = component == s->selector;
        const auto session = services_->input_runtime()->sessions().active();
        const auto* editor = services_->input_runtime()->editors().find(session.owner);
        if (editor && editor->composition().active) {
            return false;
        }
        if (event.key == input::Key::escape) {
            for (auto popup : services_->popover().mounted()) {
                if (popup != s->popover && descendant(*services_, popup, id) &&
                    descendant(*services_, component, popup) && services_->popover().snapshot(popup).geometry.visible) {
                    return false;
                }
            }
        }
        if (event.key == input::Key::escape && s->model.open() && event.modifiers == input::KeyModifier::none) {
            if (event.action == input::KeyAction::down && !event.repeat) {
                request_open(id, false);
            }
            return true;
        }
        if (selector && event.modifiers == input::KeyModifier::none &&
            (event.key == input::Key::down || event.key == input::Key::up || event.key == input::Key::enter ||
             event.key == input::Key::space)) {
            if (event.action == input::KeyAction::down && !event.repeat) {
                request_open(id, true, true);
            }
            return true;
        }
        if (event.key == input::Key::tab && (s->model.open() || !selector) &&
            (event.modifiers == input::KeyModifier::none || event.modifiers == input::KeyModifier::shift)) {
            if (event.action == input::KeyAction::down) {
                std::vector<input::InteractionId> order;
                for (auto hit : services_->interactions().declaration_order()) {
                    const auto* r = services_->interactions().find(hit);
                    if (r && r->focusable && r->tab_stop && r->eligible &&
                        services_->interactions().branch_active(hit) &&
                        (hit == s->selector_hit || !descendant(*services_, r->component, id))) {
                        order.push_back(hit);
                    }
                }
                const auto position = std::ranges::find(order, s->selector_hit);
                std::optional<input::InteractionId> target;
                if (position != order.end() && order.size() > 1) {
                    const auto i = static_cast<std::size_t>(position - order.begin());
                    const auto next = event.modifiers == input::KeyModifier::shift
                                          ? (i + order.size() - 1) % order.size()
                                          : (i + 1) % order.size();
                    target = order[next];
                }
                if (intact(id)) {
                    services_->focus().defer_focus(target, input::FocusModality::keyboard);
                }
            }
            return true;
        }
    }
    return false;
}

void TreeSelectComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::up || event.button != input::PointerButton::primary || hit != origin ||
        !hit) {
        return;
    }
    const auto* record = services_->interactions().find(*hit);
    if (!record) {
        return;
    }
    const auto component = record->component;
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        if (editable(*it)) {
            const auto s = state(*services_, *it);
            // Affix actions have their own component; never treat clear as selector activation.
            if (component == s->selector) {
                request_open(*it, !s->model.open());
                return;
            }
        }
    }
}

void TreeSelectComponentHost::on_focus_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (!s->ready) {
            continue;
        }
        const auto focused = services_->focus().state().focused;
        const auto* r = focused ? services_->interactions().find(*focused) : nullptr;
        const bool inside = r && descendant(*services_, r->component, id);
        const bool left = s->focused_inside && !inside;
        s->focused_inside = inside;
        if (left) {
            request_open(id, false);
        }
    }
}

void TreeSelectComponentHost::on_window_active(bool active) {
    window_active_ = active;
    on_input_boundary_changed();
}

void TreeSelectComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id);
    }
}

void TreeSelectComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (intact(id)) {
            const auto s = state(*services_, id);
            if (s->ready && s->tag_ready) {
                tags(id);
            }
        }
    }
}

void TreeSelectComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        services_->popover().position_ancestors(id, viewport, clip);
        if (!intact(id)) {
            continue;
        }
        const auto anchor = bounds(*services_, s->selector);
        if (s->tag_ready && s->tag_geometry != runtime::Size{anchor.width, anchor.height}) {
            services_->request_frame();
        }
        const auto available = services_->component_clip(s->selector, viewport, clip);
        const bool blocked = anchor.width <= 0 || anchor.height <= 0 ||
                             graphics::intersect_effect_bounds(anchor, available).width <= 0 ||
                             graphics::intersect_effect_bounds(anchor, available).height <= 0;
        s->geometry_blocked = blocked;
        refresh(id);
        if (!intact(id)) {
            continue;
        }
        const float width = std::max(0.0F, std::min(anchor.width, viewport.width));
        if (s->width.get() != width) {
            s->width.set(width);
            if (!intact(id)) {
                continue;
            }
            update_theme(id);
        }
        if (intact(id)) {
            services_->popover().position_window_layer(s->popover, viewport, clip);
        }
    }
}

void TreeSelectComponentHost::synchronize_auxiliary_motion() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (!s->ready || !s->motion) {
            continue;
        }
        if (!editable(id) || !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                               services_->motion_preference())
                                  .enabled()) {
            s->motion->settle();
        }
        const auto popup = services_->popover().snapshot(s->popover).popup;
        auto& node = services_->nodes().require(services_->components().root(popup));
        const float opacity = s->model.open() ? .5F + .5F * s->motion->value() : 1.F;
        if (node.opacity != opacity) {
            node.opacity = opacity;
            services_->dirty().invalidate(node.id, runtime::DirtyFlags::Material);
        }
    }
}

void TreeSelectComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!editable(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (!s->focus_pending || !services_->popover().snapshot(s->popover).geometry.visible) {
            continue;
        }
        s->focus_pending = false;
        const auto tree = services_->tree().snapshot(s->tree);
        std::optional<input::InteractionId> target;
        for (const auto& row : tree.rows) {
            const auto* r = services_->interactions().find(row.interaction);
            if (row.shown && r && r->eligible && r->focusable &&
                services_->interactions().branch_active(row.interaction)) {
                if (!target || row.selected) {
                    target = row.interaction;
                }
                if (row.selected) {
                    break;
                }
            }
        }
        if (target) {
            services_->focus().defer_focus(target, input::FocusModality::keyboard);
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void TreeSelect(TreeSelectProps props, std::vector<TreeSelectItem> items) {
    if (!detail::active_tree_select) {
        throw std::logic_error("TreeSelect requires an active window component context");
    }
    detail::active_tree_select->mount(props, items);
}
} // namespace ryn
