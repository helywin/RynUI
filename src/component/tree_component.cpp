#include "component/tree_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/tree_model.hpp"
#include "component/navigation_transition.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "reactive/untracked.hpp"
#include <algorithm>
#include <exception>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace ryn::detail {
struct TreeRefAccess {
    static void bind(const TreeRef& ref, std::function<bool(TreeScrollTo)> scroll, std::function<void()> reset) {
        ++ref.state_->generation;
        ref.state_->scroll = std::move(scroll);
        ref.state_->reset = std::move(reset);
    }

    static auto generation(const TreeRef& ref) {
        return ref.state_->generation;
    }

    static void clear(const TreeRef& ref, std::uint64_t generation) {
        if (ref.state_->generation == generation) {
            ref.state_->scroll = {};
            ref.state_->reset = {};
        }
    }
};

namespace {
thread_local TreeComponentHost* active_tree{};
constexpr float infinity = std::numeric_limits<float>::infinity();

float focus_padding(const TreeMetricToken& metrics) {
    return metrics.focus_width > 0 ? std::max(0.0F, metrics.focus_offset + metrics.focus_width) : 0.0F;
}

struct TreeRow final {
    TreeRowSnapshot view;
    std::shared_ptr<std::size_t> index{std::make_shared<std::size_t>(0)};
    std::uint64_t revision{};
    bool rich{};
    std::vector<runtime::ComponentId> required;
    runtime::ComponentId check_mark;
    runtime::SceneFragmentId hit_fragment;
    Signal<String> title{String{}};
    Signal<IconName> arrow{IconName::RightOutlined};
    Signal<IconName> directory_icon{IconName::FolderOutlined};
    Signal<bool> expanded{false};
    Signal<bool> loading{false};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> mark_color{{1, 1, 1, 1}};
    component::RetainedSurfaceId surface;
    bool custom_icon{};
    bool disabled_before{};
    runtime::Size title_size;
    float height{};
    float top{};
    float indent{};
    float switcher_width{};
    float checkbox_width{};
    float icon_width{};
    runtime::Rect painted_bounds;
};

struct TreeState final {
    Scope bindings;
    runtime::NodeId node;
    input::InteractionId root_interaction;
    input::PointerIdentity drag_pointer{input::PointerIdentity::mouse()};
    runtime::Point drag_point;
    std::optional<animation::AnimationTime> auto_scroll;
    TreeModel model;
    TreeProps props;
    std::unique_ptr<NavigationTransition> motion;
    std::vector<TreeItem> items;
    std::vector<TreeItem> flat;
    std::map<std::string, std::size_t, std::less<>> keys;
    std::thread::id thread{std::this_thread::get_id()};

    struct Request {
        std::shared_ptr<std::atomic_bool> cancelled;
        std::uint64_t ticket{};
        AsyncOptionsCancel cancel;
    };

    std::map<std::string, Request, std::less<>> requests;
    TreeKeys loaded;
    std::uint64_t generation{};
    std::optional<std::size_t> anchor;
    bool virtualized{};
    bool rtl{};
    bool lines{};
    bool icons{true};
    bool block{};
    bool draggable{};
    float row_height{24};
    float viewport_height{};
    String query;
    std::optional<TreeDragInfo> dragging;
    runtime::Point press_point;
    bool pointer_additive{};
    bool pointer_range{};
    bool reconciling{};
    std::uint64_t validated_topology{std::numeric_limits<std::uint64_t>::max()};
    std::vector<TreeRow> rows;
    std::vector<runtime::ComponentId> required;
    std::map<std::uint32_t, std::size_t> row_indices;
    std::vector<std::size_t> shown;
    std::vector<float> offsets{0};
    std::vector<std::size_t> paint_rows;
    std::vector<bool> paint_pending;
    TreeWork work;
    runtime::Rect painted_clip;
    runtime::Rect painted_decoration_clip;
    runtime::Rect painted_bounds;
    runtime::Point painted_translation;
    float painted_opacity{-1};
    bool paint_all{true};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
    component::RetainedSurfaceId background;
    std::function<void(TreeExpandInfo)> expand;
    std::function<void(TreeSelectInfo)> select;
    std::function<void(TreeCheckInfo)> check;
    std::optional<std::size_t> entry;
    std::optional<std::size_t> hover;
    std::optional<std::size_t> pressed;
    std::optional<std::size_t> reveal;
    std::optional<TreeScrollTo> scroll_request;
    std::optional<input::InteractionId> last_focus;
    int pressed_part{};
    float scroll{};
    float content_height{};
    runtime::Rect clip;
    bool ready{};
    bool refreshing{};
    bool pending{};
    bool geometry_blocked{};
};

using State = std::shared_ptr<TreeState>;

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
    std::vector<runtime::ComponentId>& mounted;
    runtime::ComponentId owner;
    bool committed{};

    ~MountTransaction() noexcept {
        if (committed) {
            return;
        }
        services.retire_failed_component(owner);
        std::erase(mounted, owner);
    }
};

struct RetireOnFailure final {
    WindowComponentServices& services;
    runtime::ComponentId owner;
    bool complete{};

    ~RetireOnFailure() noexcept {
        if (!complete) {
            services.retire_failed_component(owner);
        }
    }
};

State state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* record = services.components().state<State>(id);
    return record ? *record : State{};
}

runtime::SemanticForeground fg(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

runtime::Rect bounds(WindowComponentServices& services, runtime::ComponentId id) {
    const auto& n = services.nodes().require(services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool contains(runtime::Rect r, float x, float y) {
    return r.width > 0 && r.height > 0 && x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

std::optional<std::size_t> focused_row(WindowComponentServices& services, const TreeState& s) {
    const auto focus = services.focus().state().focused;
    const auto* record = focus ? services.interactions().find(*focus) : nullptr;
    if (!record) {
        return {};
    }
    for (auto owner = std::optional{record->component}; owner; owner = services.components().parent(*owner)) {
        const auto found = s.row_indices.find(owner->index);
        if (found != s.row_indices.end() && s.rows[found->second].view.component == *owner) {
            return found->second;
        }
    }
    return {};
}

void queue_paint(TreeState& s, std::size_t i) {
    if (!s.paint_pending[i]) {
        s.paint_rows.push_back(i);
        s.paint_pending[i] = true;
    }
}

int part(WindowComponentServices& services, const TreeRow& row, float x, float y, bool checkable) {
    if (contains(bounds(services, row.view.switcher), x, y)) {
        return 1;
    }
    if (checkable && contains(bounds(services, row.view.checkbox), x, y)) {
        return 2;
    }
    return 0;
}
} // namespace

TreeComponentHost::TreeComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TreeComponentHost::~TreeComponentHost() {
    services_->detach(*this);
}

void* TreeComponentHost::begin_mount() noexcept {
    auto* old = active_tree;
    active_tree = this;
    return old;
}

void TreeComponentHost::end_mount(void* old) noexcept {
    active_tree = static_cast<TreeComponentHost*>(old);
}

bool TreeComponentHost::intact(runtime::ComponentId id) {
    const auto s = state(*services_, id);
    if (!s || !services_->components().scope(id).active()) {
        return false;
    }
    if (s->reconciling) {
        return true;
    }
    const auto topology = services_->nodes().topology_revision();
    if (s->validated_topology == topology) {
        return true;
    }
    for (const auto& row : s->rows) {
        if (!row.view.realized) {
            continue;
        }
        for (auto child : row.required) {
            if (!services_->components().contains(child)) {
                services_->retire_failed_component(id);
                return false;
            }
        }
    }
    for (auto child : s->required) {
        if (!services_->components().contains(child)) {
            services_->retire_failed_component(id);
            return false;
        }
    }
    s->validated_topology = topology;
    return true;
}

void TreeComponentHost::on_destroy() noexcept {
    const auto ids = mounted_;
    for (auto id : ids) {
        static_cast<void>(intact(id));
    }
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

TreeSnapshot TreeComponentHost::snapshot(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    if (!s) {
        throw std::out_of_range("Tree component is stale");
    }
    const auto checks = s->model.checks(s->model.checked);
    TreeSnapshot result{s->model.normalize(s->model.expanded),
                        s->model.normalize(s->model.selected, !s->model.multiple),
                        checks.checked,
                        checks.half_checked,
                        {},
                        s->scroll,
                        s->content_height};
    for (const auto& row : s->rows) {
        result.rows.push_back(row.view);
    }
    return result;
}

void TreeComponentHost::check(runtime::ComponentId id, const String& key) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const auto it = s->keys.find(key.bytes());
    if (it == s->keys.end()) {
        return;
    }
    const auto next = s->model.toggle_check(it->second);
    if (!s->model.checkable || !s->model.eligible(it->second) || s->model.nodes[it->second].check_disabled ||
        !s->model.nodes[it->second].checkable) {
        return;
    }
    if (!s->model.controlled_checked) {
        s->model.checked = next.checked;
        if (!s->props.half_) {
            s->model.half = next.half_checked;
        }
    }
    const auto callback = s->check;
    const bool checked = std::ranges::find(next.checked, key) != next.checked.end();
    refresh(id, true);
    if (intact(id) && callback) {
        CallbackFailure failure{*services_, id};
        callback({key, next.checked, next.half_checked, checked});
    }
}

bool TreeComponentHost::empty(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    return !s || s->shown.empty();
}

std::vector<TreeItem> TreeComponentHost::data(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    return s ? s->items : std::vector<TreeItem>{};
}

TreeWork TreeComponentHost::work(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    return s ? s->work : TreeWork{};
}

void TreeComponentHost::present(runtime::ComponentId id, std::size_t i) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (!s->ready) {
        return;
    }
    if (i >= s->rows.size()) {
        return;
    }
    auto& row = s->rows[i];
    if (!row.view.realized) {
        return;
    }
    const auto colors = services_->components().theme_scope(id)->snapshot().tree().colors;
    const bool disabled = s->model.disabled || s->model.nodes[i].disabled;
    queue_paint(*s, i);
    row.color.set(fg(disabled            ? colors.disabled
                     : row.view.selected ? colors.node_selected_text
                     : s->hover == i     ? colors.node_hover_text
                                         : colors.text));
    if (intact(id)) {
        services_->dirty().invalidate(services_->components().root(row.view.component), runtime::DirtyFlags::Material);
    }
}

void TreeComponentHost::entry(runtime::ComponentId id, std::size_t i) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const auto previous = s->entry;
    s->entry = i;
    if (previous && *previous != i && s->rows[*previous].view.realized) {
        services_->interactions().set_tab_stop(s->rows[*previous].view.interaction, false);
    }
    if (s->rows[i].view.realized) {
        services_->interactions().set_tab_stop(s->rows[i].view.interaction, true);
    }
}

void TreeComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (!s->ready) {
        return;
    }
    if (s->refreshing) {
        s->pending = true;
        return;
    }
    s->refreshing = true;

    struct Reset final {
        State s;

        ~Reset() {
            s->refreshing = false;
        }
    } reset{s};

    do {
        s->pending = false;
        const auto t = services_->components().theme_scope(id)->snapshot().tree();
        s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
        if (!intact(id)) {
            return;
        }
        s->icon_font.set({t.metrics.font_family, 400, false, t.metrics.icon_size, t.metrics.icon_size});
        if (!intact(id)) {
            return;
        }
        s->paint_all = true;
        s->shown = s->model.shown();
        s->offsets.assign(1, 0.F);
        for (auto i : s->shown) {
            const float height = s->props.item_height_ ? s->props.item_height_(s->model.nodes[i].key) : s->row_height;
            if (!std::isfinite(height) || height < 1 || height > 4096) {
                throw std::invalid_argument("Tree itemHeight must be finite in [1,4096]");
            }
            s->offsets.push_back(s->offsets.back() + height);
        }
        if (s->virtualized) {
            realize(id, std::max(1.F, bounds(*services_, id).height));
        }
        const auto& shown = s->shown;
        std::vector<bool> shown_set(s->rows.size());
        for (auto i : shown) {
            shown_set[i] = true;
        }
        if (s->virtualized) {
            const auto first = static_cast<std::size_t>(
                std::upper_bound(s->offsets.begin(), s->offsets.end(), std::max(0.F, s->scroll)) - s->offsets.begin() -
                1);
            const auto begin = first > 2 ? first - 2 : 0;
            const auto end =
                static_cast<std::size_t>(std::lower_bound(s->offsets.begin(), s->offsets.end(),
                                                          s->scroll + std::max(1.F, bounds(*services_, id).height)) -
                                         s->offsets.begin()) +
                3;
            for (std::size_t pos = 0; pos < shown.size(); ++pos) {
                shown_set[shown[pos]] = pos >= begin && pos < end;
            }
        }
        const auto previous_interaction = services_->focus().state().focused;
        const auto previous_focus = focused_row(*services_, *s);
        if (s->reveal && !shown_set[*s->reveal]) {
            s->reveal.reset();
        }
        if (geometry && !s->dragging && previous_focus && shown_set[*previous_focus]) {
            s->reveal = previous_focus;
        }
        const auto selected = s->model.normalize(s->model.selected, !s->model.multiple);
        std::unordered_set<std::string_view> selected_set;
        for (const auto& key : selected) {
            selected_set.insert(key.bytes());
        }
        const auto is_selected = [&](std::size_t i) {
            return selected_set.contains(s->model.nodes[i].key.bytes());
        };
        const auto eligible = [&](std::size_t i) {
            return s->model.eligible(i) && shown_set[i];
        };
        if (previous_focus && eligible(*previous_focus)) {
            s->entry = previous_focus;
        }
        if (!s->entry || !eligible(*s->entry)) {
            s->entry.reset();
            for (auto i : shown) {
                if (eligible(i) && is_selected(i)) {
                    s->entry = i;
                    break;
                }
            }
            for (auto i : shown) {
                if (!s->entry && eligible(i)) {
                    s->entry = i;
                    break;
                }
            }
        }
        std::optional<std::size_t> transfer;
        const bool transfer_focus =
            previous_focus &&
            (!shown_set[*previous_focus] || s->model.disabled ||
             (previous_interaction == s->rows[*previous_focus].view.interaction && !eligible(*previous_focus)) ||
             (s->model.nodes[*previous_focus].disabled && !s->rows[*previous_focus].disabled_before));
        if (transfer_focus) {
            auto ancestor = s->model.nodes[*previous_focus].parent;
            while (ancestor != tree_no_parent && !eligible(ancestor)) {
                ancestor = s->model.nodes[ancestor].parent;
            }
            transfer = ancestor != tree_no_parent ? std::optional{ancestor} : s->entry;
            s->entry = transfer;
        }
        if (!s->model.visible || s->model.disabled || s->geometry_blocked ||
            !services_->focus().state().window_active) {
            cancel_loads(id);
            if (s->motion) {
                s->motion->settle();
            }
        }
        const bool root_shown = s->model.visible && !shown.empty();
        bool structure = services_->components().set_branch_active(id, root_shown);
        services_->nodes().require(s->node).out_of_flow = !root_shown;
        const auto checked = s->model.checks(s->model.checked);
        std::set<std::string_view> checked_set;
        std::set<std::string_view> half_set;
        const auto expanded_nodes = s->model.opened();
        for (const auto& key : checked.checked) {
            checked_set.insert(key.bytes());
        }
        for (const auto& key : checked.half_checked) {
            half_set.insert(key.bytes());
        }
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            ++s->work.projected_rows;
            auto& row = s->rows[i];
            auto& view = row.view;
            if (!view.realized) {
                view.shown = false;
                continue;
            }
            services_->scene_composer().set_fragment(row.hit_fragment, {},
                                                     s->draggable ? std::optional<input::InteractionId>{}
                                                                  : std::optional{view.interaction});
            view.shown = shown_set[i];
            view.expanded = expanded_nodes[i];
            view.selected = is_selected(i);
            view.checked = checked_set.contains(s->model.nodes[i].key.bytes());
            view.indeterminate = half_set.contains(s->model.nodes[i].key.bytes());
            structure |= services_->components().set_branch_active(view.component, view.shown);
            services_->nodes().require(services_->components().root(view.component)).out_of_flow = !view.shown;
            // Keep the wrapper eligible: a disabled Tree row can contain independently enabled native controls.
            services_->interactions().set_eligible(view.interaction, view.shown);
            services_->interactions().set_focusable(view.interaction, eligible(i));
            services_->interactions().set_tab_stop(view.interaction, s->entry == i);
            const bool disabled = s->model.disabled || s->model.nodes[i].disabled;
            row.color.set(fg(
                disabled        ? t.colors.disabled
                : view.selected ? t.colors.node_selected_text
                : s->hover == i || (s->model.filtering && std::ranges::find(s->model.matches, s->model.nodes[i].key) !=
                                                              s->model.matches.end())
                    ? t.colors.node_hover_text
                    : t.colors.text));
            if (!intact(id)) {
                return;
            }
            row.mark_color.set(
                fg(disabled || s->model.nodes[i].check_disabled ? t.colors.disabled : t.colors.checkbox_mark));
            if (!intact(id)) {
                return;
            }
            row.arrow.set(view.loading          ? IconName::LoadingOutlined
                          : !view.error.empty() ? IconName::ReloadOutlined
                          : view.expanded       ? IconName::DownOutlined
                          : s->rtl              ? IconName::LeftOutlined
                                                : IconName::RightOutlined);
            if (!intact(id)) {
                return;
            }
            structure |= services_->components().set_branch_active(
                view.switcher,
                !s->model.nodes[i].leaf && (s->model.nodes[i].end > i + 1 || static_cast<bool>(s->props.loader_)));
            structure |= services_->components().set_branch_active(view.checkbox,
                                                                   s->model.checkable && s->model.nodes[i].checkable);
            structure |= services_->components().set_branch_active(row.check_mark, s->model.checkable && view.checked);
            if (row.disabled_before != disabled) {
                row.disabled_before = disabled;
                if (disabled) {
                    services_->set_input_blocked(view.component, true);
                    if (!intact(id)) {
                        return;
                    }
                }
            }
            services_->set_input_blocked(view.component, !view.shown);
            if (!intact(id)) {
                return;
            }
            if (!eligible(i) && s->pressed == i && (!s->dragging || !s->model.eligible(i))) {
                s->pressed.reset();
                services_->pointer().cancel_pointer_interaction(view.interaction);
                if (!intact(id)) {
                    return;
                }
            }
        }
        if (structure) {
            services_->mark_scene_structure_dirty();
        }
        geometry |= structure;
        services_->dirty().invalidate(s->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
        services_->set_input_blocked(id, !root_shown || s->model.disabled || s->geometry_blocked ||
                                             !services_->focus().state().window_active);
        if (!intact(id)) {
            return;
        }
        if (transfer &&
            (!services_->focus().state().focused || services_->focus().state().focused == previous_interaction)) {
            s->reveal = transfer;
            services_->focus().defer_focus(s->rows[*transfer].view.interaction, services_->focus().state().modality);
            if (!intact(id)) {
                return;
            }
        }
        geometry = true;
    } while (s->pending);
}

void TreeComponentHost::mount(const TreeProps& p, const std::vector<TreeItem>& items) {
    FormPrivateEditorScope private_editor;
    if ((p.expanded_ && p.default_expanded_) || (p.selected_ && p.default_selected_) ||
        (p.checked_ && p.default_checked_)) {
        throw std::invalid_argument("Tree controlled and default keys conflict");
    }
    auto s = std::make_shared<TreeState>();
    s->props = p;
    s->model.loadable = static_cast<bool>(p.loader_);
    s->model.controlled_expanded = p.expanded_.has_value();
    s->model.controlled_selected = p.selected_.has_value();
    s->model.controlled_checked = p.checked_.has_value();
    s->model.expanded = p.default_expanded_.value_or(TreeKeys{});
    s->model.selected = p.default_selected_.value_or(TreeKeys{});
    s->model.checked = p.default_checked_.value_or(TreeKeys{});
    s->expand = p.on_expand_;
    s->select = p.on_select_;
    s->check = p.on_check_;
    auto& services = *services_;
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<State>(s);
    MountTransaction transaction{services, mounted_, id};
    s->node = b.root(id);
    s->motion = std::make_unique<NavigationTransition>(services, id);
    mounted_.push_back(id);
    services.nodes().require(s->node).clip_content = true;
    b.on_resource_cleanup(id, [&services, id, node = s->node, weak = std::weak_ptr{s}] {
        if (auto current = weak.lock()) {
            current->bindings.dispose();
            for (auto& [key, request] : current->requests) {
                request.cancelled->store(true);
                if (request.cancel) {
                    try {
                        request.cancel();
                    } catch (...) {
                    }
                }
            }
        }
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    const auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s->background = services.surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(id,
                          [&services, surface = s->background] { services.surfaces().destroy_content_range(surface); });
    const auto root_interaction = services.interactions().create({id, s->node, {}, true, false, {}, false, false});
    s->root_interaction = root_interaction;
    input::InteractionHandlers root_handlers;
    root_handlers.target = [this, id](input::PointerDispatchContext& event) {
        if (!intact(id)) {
            return;
        }
        const auto s = state(*services_, id);
        if (!s->draggable) {
            return;
        }
        if (s->pressed && s->drag_pointer != event.event().pointer) {
            return;
        }
        if (event.kind() == input::PointerEventKind::cancel) {
            s->pressed.reset();
            s->dragging.reset();
            s->auto_scroll.reset();
            return;
        }
        if (event.kind() == input::PointerEventKind::move && s->pressed) {
            if (s->pressed_part == 0) {
                drag(id, event.event());
            }
            event.stop_propagation();
            return;
        }
        if (event.event().button != input::PointerButton::primary) {
            return;
        }
        if (event.kind() == input::PointerEventKind::down) {
            std::optional<std::size_t> target;
            for (const auto& [component, i] : s->row_indices) {
                const auto& row = s->rows[i];
                if (row.view.shown &&
                    contains(bounds(*services_, row.view.component), event.event().x, event.event().y)) {
                    target = i;
                    break;
                }
            }
            if (!target || !s->model.eligible(*target)) {
                return;
            }
            s->pressed = target;
            s->pressed_part = part(*services_, s->rows[*target], event.event().x, event.event().y, s->model.checkable);
            s->pointer_additive = event.event().control || event.event().meta;
            s->pointer_range = event.event().shift;
            s->press_point = {event.event().x, event.event().y};
            s->drag_pointer = event.event().pointer;
            entry(id, *target);
            static_cast<void>(
                services_->focus().request_focus(s->rows[*target].view.interaction, input::FocusModality::pointer));
            if (!intact(id) || !s->pressed) {
                return;
            }
            static_cast<void>(event.capture_pointer());
            event.stop_propagation();
        } else if (event.kind() == input::PointerEventKind::up && s->pressed) {
            static_cast<void>(event.release_pointer_capture());
            event.stop_propagation();
            if (s->dragging) {
                drag(id, event.event());
                return;
            }
            const auto i = *s->pressed;
            s->pressed.reset();
            s->auto_scroll.reset();
            if (i >= s->rows.size() || !s->rows[i].view.realized || !s->rows[i].view.shown) {
                return;
            }
            const auto& row = s->rows[i];
            const auto zone = part(*services_, row, event.event().x, event.event().y, s->model.checkable);
            if (zone == s->pressed_part &&
                contains(bounds(*services_, row.view.component), event.event().x, event.event().y)) {
                const auto key = s->model.nodes[i].key;
                activate(id, i, zone, s->pointer_additive, s->pointer_range);
                if (intact(id) && zone == 0 && s->props.directory_ &&
                    s->props.expand_action_ == DirectoryExpandAction::DoubleClick && event.event().click_count == 2) {
                    const auto current = s->keys.find(key.bytes());
                    if (current != s->keys.end()) {
                        activate(id, current->second, 1);
                    }
                }
            }
        }
    };
    services.interactions().set_handlers(root_interaction, std::move(root_handlers));
    const auto root_hit = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(root_hit, {}, root_interaction);
    b.on_resource_cleanup(id, [&services, root_hit, root_interaction] {
        services.pointer().cancel_interaction(root_interaction);
        services.focus().cancel_interaction(root_interaction);
        services.interactions().remove(root_interaction);
        services.scene_composer().remove_fragment(root_hit);
    });
    runtime::connect_layout_style(s->bindings, p.layout_, s->node, services.nodes(), services.dirty(),
                                  [this, id] { return intact(id); });
    if (!intact(id)) {
        return;
    }
    const auto bind_value = [&]<class T, class Apply>(const Prop<T>& prop, Apply apply) {
        connect_prop(s->bindings, prop, [this, id, apply](T value) {
            run_untracked([&] {
                if (intact(id)) {
                    const auto current = state(*services_, id);
                    apply(*current, std::move(value));
                    if (intact(id)) {
                        refresh(id, true);
                    }
                }
            });
        });
        return intact(id);
    };
    if (!bind_value(p.visible_, [](auto& v, bool x) { v.model.visible = x; }) ||
        !bind_value(p.disabled_, [](auto& v, bool x) { v.model.disabled = x; }) ||
        !bind_value(p.checkable_, [](auto& v, bool x) { v.model.checkable = x; })) {
        return;
    }
    if (p.expanded_ && !bind_value(*p.expanded_, [](auto& v, TreeKeys x) { v.model.expanded = std::move(x); })) {
        return;
    }
    if (p.selected_ && !bind_value(*p.selected_, [](auto& v, TreeKeys x) { v.model.selected = std::move(x); })) {
        return;
    }
    if (p.checked_ && !bind_value(*p.checked_, [](auto& v, TreeKeys x) { v.model.checked = std::move(x); })) {
        return;
    }
    s->loaded = p.default_loaded_;
    const auto extra_bool = [&](const auto& prop, auto apply) {
        return bind_value(prop, apply);
    };
    extra_bool(p.multiple_, [](auto& s, bool v) { s.model.multiple = v; });
    extra_bool(p.selectable_, [](auto& s, bool v) { s.model.selectable = v; });
    extra_bool(p.strict_, [](auto& s, bool v) { s.model.strict = v; });
    extra_bool(p.auto_expand_, [](auto& s, bool v) { s.model.auto_expand = v; });
    extra_bool(p.virtual_, [](auto& s, bool v) { s.virtualized = v; });
    extra_bool(p.rtl_, [](auto& s, bool v) { s.rtl = v; });
    extra_bool(p.lines_, [](auto& s, bool v) { s.lines = v; });
    extra_bool(p.icons_, [](auto& s, bool v) { s.icons = v; });
    extra_bool(p.block_, [](auto& s, bool v) { s.block = v; });
    extra_bool(p.draggable_, [this, id](auto& s, bool v) {
        const bool changed = s.draggable != v;
        s.draggable = v;
        if (changed && s.pressed) {
            const auto row = s.rows[*s.pressed].view.interaction;
            s.pressed.reset();
            s.dragging.reset();
            s.auto_scroll.reset();
            services_->pointer().cancel_pointer_interaction(s.root_interaction);
            if (intact(id)) {
                services_->pointer().cancel_pointer_interaction(row);
            }
        }
    });
    bind_value(p.row_height_, [](auto& s, float v) {
        if (!std::isfinite(v) || v < 1 || v > 4096) {
            throw std::invalid_argument("Tree rowHeight must be finite in [1,4096]");
        }
        s.row_height = v;
    });
    bind_value(p.search_, [this, id](auto& s, String v) {
        s.query = std::move(v);
        if (!s.flat.empty()) {
            reconcile(id, s.items);
        }
    });
    if (p.half_) {
        bind_value(*p.half_, [](auto& s, TreeKeys v) { s.model.half = std::move(v); });
    }
    if (p.loaded_) {
        bind_value(*p.loaded_, [](auto& s, TreeKeys v) { s.loaded = std::move(v); });
    }
    connect_prop(
        s->bindings, Prop<std::vector<TreeItem>>{bind([p, items] {
            auto value = p.items_ ? read_prop(*p.items_) : items;
            const auto read = [&](auto&& self, std::vector<TreeItem>& list) -> void {
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
        [this, id](const auto& value) { run_untracked([&] { reconcile(id, value); }); },
        [](const auto&, const auto&) { return false; });
    if (!intact(id)) {
        return;
    }
    if (p.expand_all_) {
        for (const auto& node : s->model.nodes) {
            if (node.end > s->keys.at(std::string{node.key.bytes()}) + 1) {
                s->model.expanded.push_back(node.key);
            }
        }
    }

    services.layout().set_layout(
        s->node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    const auto theme = b.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::tree_colors, &theme_runtime::ThemeScope::tree_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        auto subscription =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
        if (!intact(id)) {
            return;
        }
        s->subscriptions[i] = std::move(subscription);
    }
    s->ready = true;
    refresh(id, true);
    if (!intact(id)) {
        return;
    }
    if (p.ref_) {
        TreeRefAccess::bind(
            *p.ref_,
            [this, id](TreeScrollTo target) {
                if (!intact(id)) {
                    return false;
                }
                auto s = state(*services_, id);
                auto it = s->keys.find(target.key.bytes());
                if (it == s->keys.end()) {
                    return false;
                }
                auto visible = s->model.shown();
                auto pos = std::ranges::find(visible, it->second);
                if (pos == visible.end()) {
                    return false;
                }
                s->reveal = it->second;
                s->scroll_request = target;
                if (s->virtualized) {
                    s->scroll = s->offsets[static_cast<std::size_t>(pos - visible.begin())] + target.offset;
                    realize(id, std::max(1.F, bounds(*services_, id).height));
                }
                refresh(id, true);
                return intact(id);
            },
            [this, id] { cancel_loads(id); });
        const auto ref = *p.ref_;
        const auto ticket = TreeRefAccess::generation(ref);
        b.on_resource_cleanup(id, [ref, ticket] { TreeRefAccess::clear(ref, ticket); });
    }
    transaction.committed = intact(id);
}

void TreeComponentHost::mount_rows(runtime::ComponentId id, const std::vector<std::size_t>& indices) {
    if (indices.empty()) {
        return;
    }
    auto s = state(*services_, id);
    auto& services = *services_;
    const auto user_slot = [this, id](const auto& slot) {
        return Content{[this, id, callback = SlotContentAccess::function(slot)] {
            FormPrivateEditorScope private_editor;
            // Retire before the enclosing slot/append transaction can roll back a focused child.
            RetireOnFailure rollback{*services_, id};
            callback();
            rollback.complete = intact(id);
        }};
    };
    std::size_t current_index{};
    const auto remember_generated = [&](auto&& self, runtime::ComponentId parent) -> void {
        for (auto child : services.components().children(parent)) {
            s->rows[current_index].required.push_back(child);
            self(self, child);
        }
    };
    const auto region = [&](bool centered = false) {
        auto& c = runtime::require_component_build_context();
        const auto child = c.mount_component<int>(0);
        s->rows[current_index].required.push_back(child);
        const auto node = c.root(child);
        services.nodes().require(node).clip_content = true;
        services.nodes().require(node).clip_decorations = false;
        services.layout().set_layout(node, layout::BoxLayout{});
        if (centered) {
            services.layout().set_layout(
                node, layout::ComponentLayout{
                          [&services](layout::LayoutEngine& e, runtime::NodeId n, layout::Constraints c) {
                              runtime::Size result;
                              for (auto child : services.nodes().require(n).children) {
                                  const auto size = e.measure_child(child, {0, c.max_width, 0, c.max_height});
                                  result.width = std::max(result.width, size.width);
                                  result.height = std::max(result.height, size.height);
                              }
                              return c.constrain(result);
                          },
                          [&services](layout::LayoutEngine& e, runtime::NodeId n, runtime::Rect r) {
                              for (auto child : services.nodes().require(n).children) {
                                  const auto natural = services.nodes().require(child).layout_size;
                                  const float w = std::min(r.width, natural.width);
                                  const float h = std::min(r.height, natural.height);
                                  e.place_child(child, {r.x + (r.width - w) / 2, r.y + (r.height - h) / 2, w, h});
                              }
                          }});
        }
        c.on_resource_cleanup(child, [&services, child, node] {
            services.interactions().set_input_blocked(child, false);
            services.layout().remove_layout(node);
        });
        return child;
    };
    const auto t = services.components().theme_scope(id)->snapshot().tree();
    s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
    s->icon_font.set({t.metrics.font_family, 400, false, t.metrics.icon_size, t.metrics.icon_size});
    const Content rows_content{[&] {
        auto& c = runtime::require_component_build_context();
        for (auto i : indices) {
            current_index = i;
            if (!intact(id)) {
                return;
            }
            auto& row = s->rows[i];
            row.view.component = region();
            row.view.realized = true;
            *row.index = i;
            const auto row_id = row.view.component;
            s->row_indices.emplace(row_id.index, i);
            const auto node = c.root(row_id);
            row.view.interaction = services.interactions().create({row_id, node, {}, true, true, {}, true, false});
            const auto hit = c.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
            row.hit_fragment = hit;
            services.scene_composer().set_fragment(hit, {}, row.view.interaction);
            const auto paint = c.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
            row.surface = services.surfaces().create_content_range(paint, {});
            c.on_resource_cleanup(row_id, [&services, hit, interaction = row.view.interaction, surface = row.surface] {
                services.pointer().cancel_interaction(interaction);
                services.focus().cancel_interaction(interaction);
                services.interactions().remove(interaction);
                services.scene_composer().remove_fragment(hit);
                services.surfaces().destroy_content_range(surface);
            });
            input::InteractionHandlers handlers;
            handlers.target = [this, id, cell = row.index](input::PointerDispatchContext& event) {
                const auto i = *cell;
                if (!intact(id)) {
                    return;
                }
                const auto current = state(*services_, id);
                if (i >= current->rows.size() || current->rows[i].index != cell) {
                    return;
                }
                if (event.kind() == input::PointerEventKind::cancel) {
                    current->pressed.reset();
                    current->dragging.reset();
                    return;
                }
                if (event.kind() == input::PointerEventKind::move ||
                    (event.kind() == input::PointerEventKind::up && current->dragging)) {
                    drag(id, event.event());
                    if (!intact(id) || current->dragging || !current->pressed) {
                        return;
                    }
                }
                const auto& row = current->rows[i];
                if (!current->model.eligible(i) || !row.view.shown ||
                    !services_->interactions().branch_active(row.view.interaction) ||
                    event.event().button != input::PointerButton::primary) {
                    return;
                }
                const int zone = part(*services_, row, event.event().x, event.event().y, current->model.checkable);
                if (event.kind() == input::PointerEventKind::down) {
                    current->pressed = i;
                    current->pressed_part = zone;
                    current->pointer_additive = event.event().control || event.event().meta;
                    current->pointer_range = event.event().shift;
                    current->press_point = {event.event().x, event.event().y};
                    entry(id, i);
                    static_cast<void>(event.capture_pointer());
                    event.stop_propagation();
                } else if (event.kind() == input::PointerEventKind::up) {
                    const auto clip = services_->nodes().content_clip(services_->components().root(row.view.component),
                                                                      current->clip);
                    const bool hit =
                        current->pressed == i && current->pressed_part == zone &&
                        event.press_origin() == row.view.interaction &&
                        contains(graphics::intersect_effect_bounds(bounds(*services_, row.view.component), clip),
                                 event.event().x, event.event().y);
                    current->pressed.reset();
                    static_cast<void>(event.release_pointer_capture());
                    event.stop_propagation();
                    if (hit && intact(id)) {
                        activate(id, i, zone, current->pointer_additive, current->pointer_range);
                        if (intact(id) && zone == 0 && current->props.directory_ &&
                            current->props.expand_action_ == DirectoryExpandAction::DoubleClick &&
                            event.event().click_count == 2) {
                            activate(id, i, 1);
                        }
                    }
                }
            };
            services.interactions().set_handlers(row.view.interaction, std::move(handlers));
            input::FocusHandlers focus;
            focus.state_changed = [this, id, cell = row.index](input::FocusPresentation value) {
                const auto i = *cell;
                if (intact(id)) {
                    const auto current_state = state(*services_, id);
                    if (i >= current_state->rows.size() || current_state->rows[i].index != cell) {
                        return;
                    }
                    if (value.focused) {
                        const auto current = state(*services_, id);
                        entry(id, i);
                        current->reveal = i;
                        services_->dirty().invalidate(current->node, runtime::DirtyFlags::Layout);
                    }
                    present(id, i);
                }
            };
            services.interactions().set_focus_handlers(row.view.interaction, std::move(focus));
            c.mount_slot(
                row_id, Content{[&] {
                    auto& inner = runtime::require_component_build_context();
                    row.view.switcher = region(true);
                    if (s->props.switcher_) {
                        inner.mount_slot_with_semantic_text_style(
                            row.view.switcher,
                            user_slot(s->props.switcher_({s->model.nodes[i].key, row.expanded, row.loading})),
                            row.color, s->icon_font);
                    } else {
                        inner.mount_slot_with_semantic_text_style(
                            row.view.switcher, Content{[arrow = row.arrow] { Icon(IconProps{}.name(arrow)); }},
                            row.color, s->icon_font);
                    }
                    if (!intact(id)) {
                        return;
                    }
                    remember_generated(remember_generated, row.view.switcher);
                    row.view.checkbox = region(true);
                    inner.mount_slot(row.view.checkbox, Content{[&] { row.check_mark = region(); }});
                    inner.mount_slot_with_semantic_text_style(
                        row.check_mark, Content{[] { Icon(IconProps{}.name(IconName::CheckOutlined)); }},
                        row.mark_color, s->icon_font);
                    if (!intact(id)) {
                        return;
                    }
                    remember_generated(remember_generated, row.check_mark);
                    row.view.icon = region(true);
                    row.custom_icon = s->flat[i].icon_.has_value() || s->props.directory_;
                    if (s->flat[i].icon_) {
                        inner.mount_slot_with_semantic_text_style(row.view.icon, user_slot(*s->flat[i].icon_),
                                                                  row.color, s->font);
                    }
                    if (s->props.directory_ && !s->flat[i].icon_) {
                        inner.mount_slot_with_semantic_text_style(
                            row.view.icon, Content{[name = row.directory_icon] { Icon(IconProps{}.name(name)); }},
                            row.color, s->font);
                        remember_generated(remember_generated, row.view.icon);
                    }
                    if (!intact(id)) {
                        return;
                    }
                    row.view.title = region();
                    inner.mount_slot_with_semantic_text_style(
                        row.view.title,
                        s->flat[i].rich_title_ ? user_slot(*s->flat[i].rich_title_)
                                               : Content{[title = row.title] { Text(TextProps{}.content(title)); }},
                        row.color, s->font);
                    if (intact(id) && !s->flat[i].rich_title_) {
                        remember_generated(remember_generated, row.view.title);
                    }
                }});
            if (!intact(id)) {
                return;
            }
            services.layout().set_layout(
                node, layout::ComponentLayout{
                          [this, id, cell = row.index](layout::LayoutEngine& e, runtime::NodeId,
                                                       layout::Constraints c) { return measure_row(id, *cell, e, c); },
                          [this, id, cell = row.index](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) {
                              place_row(id, *cell, e, r);
                          }});
        }
    }};
    if (active_tree == this) {
        runtime::require_component_build_context().mount_slot_with_theme_scope(
            Content{[&] { runtime::require_component_build_context().mount_slot(id, rows_content); }},
            services.components().theme_scope(id));
    } else {
        services.append_slot(id, rows_content);
    }
    if (!intact(id)) {
        return;
    }
}

void TreeComponentHost::reconcile(runtime::ComponentId id, const std::vector<TreeItem>& items) {
    auto s = state(*services_, id);
    if (!s) {
        return;
    }
    if (s->reconciling) {
        services_->retire_failed_component(id);
        return;
    }
    TreeModel candidate = s->model;
    candidate.nodes.clear();
    candidate.matches.clear();
    candidate.filtering = !s->query.empty();
    std::vector<TreeItem> flat;
    const auto visit = [&](auto&& self, const std::vector<TreeItem>& list, std::size_t parent,
                           std::size_t depth) -> void {
        if (!list.empty() && depth >= 64) {
            throw std::invalid_argument("Tree depth exceeds 64");
        }
        for (const auto& item : list) {
            if (flat.size() >= 100000) {
                throw std::invalid_argument("Tree size exceeds 100000");
            }
            const auto i = flat.size();
            flat.emplace_back(item.key_, item.title_);
            auto& shallow = flat.back();
            shallow.rich_title_ = item.rich_title_;
            shallow.icon_ = item.icon_;
            shallow.disabled_ = item.disabled_;
            shallow.visible_ = item.visible_;
            shallow.selectable_ = item.selectable_;
            shallow.checkable_ = item.checkable_;
            shallow.check_disabled_ = item.check_disabled_;
            shallow.leaf_ = item.leaf_;
            shallow.revision_ = item.revision_;
            shallow.identity_ = item.identity_;
            candidate.nodes.push_back({item.key_, parent, depth, 0, read_prop(item.disabled_), read_prop(item.visible_),
                                       read_prop(item.selectable_), read_prop(item.checkable_),
                                       read_prop(item.check_disabled_), read_prop(item.leaf_)});
            if (candidate.filtering &&
                (s->props.filter_ ? s->props.filter_(item.key_, read_prop(item.title_))
                                  : read_prop(item.title_).bytes().find(s->query.bytes()) != std::string_view::npos)) {
                candidate.matches.push_back(item.key_);
            }
            self(self, item.children_, i, depth + 1);
            candidate.nodes[i].end = flat.size();
        }
    };
    visit(visit, items, tree_no_parent, 0);
    candidate.validate();
    std::vector<bool> data_visible(candidate.nodes.size());
    for (std::size_t i = 0; i < candidate.nodes.size(); ++i) {
        const auto& node = candidate.nodes[i];
        data_visible[i] = node.visible && (node.parent == tree_no_parent || data_visible[node.parent]);
    }
    const auto candidate_shown = candidate.shown();
    std::vector<float> offsets{0};
    for (auto i : candidate_shown) {
        const float height = s->props.item_height_ ? s->props.item_height_(candidate.nodes[i].key) : s->row_height;
        if (!std::isfinite(height) || height < 1 || height > 4096) {
            throw std::invalid_argument("Tree itemHeight must be finite in [1,4096]");
        }
        offsets.push_back(offsets.back() + height);
    }
    if (!intact(id)) {
        return;
    }
    RetireOnFailure failure{*services_, id};
    s->reconciling = true;

    struct Guard {
        State s;

        ~Guard() {
            s->reconciling = false;
        }
    } guard{s};

    const auto preserve = [&](std::optional<std::size_t> i) -> std::optional<String> {
        return i && *i < s->model.nodes.size() ? std::optional{s->model.nodes[*i].key} : std::nullopt;
    };
    const auto old_focus = focused_row(*services_, *s);
    const auto focus_key = preserve(old_focus);
    const auto entry_key = preserve(s->entry);
    const auto anchor_key = preserve(s->anchor);
    if (s->pressed) {
        const auto hit = s->rows[*s->pressed].view.interaction;
        s->pressed.reset();
        s->dragging.reset();
        s->auto_scroll.reset();
        services_->pointer().cancel_pointer_interaction(s->root_interaction);
        services_->pointer().cancel_pointer_interaction(hit);
    }
    std::map<std::string, std::size_t, std::less<>> keys;
    std::vector<TreeRow> rows(flat.size());
    std::vector<runtime::ComponentId> retired;
    for (std::size_t i = 0; i < flat.size(); ++i) {
        const auto key = std::string{flat[i].key_.bytes()};
        keys.emplace(key, i);
        const auto old = s->keys.find(key);
        if (old != s->keys.end()) {
            auto& row = s->rows[old->second];
            if (row.revision == flat[i].revision_ && row.rich == flat[i].rich_title_.has_value() &&
                row.custom_icon == (flat[i].icon_.has_value() || s->props.directory_)) {
                rows[i] = std::move(row);
                row.view.realized = false;
            } else if (row.view.realized) {
                retired.push_back(row.view.component);
                *row.index = tree_no_parent;
                row.view.realized = false;
            }
        }
        rows[i].revision = flat[i].revision_;
        rows[i].rich = flat[i].rich_title_.has_value();
        rows[i].custom_icon = flat[i].icon_.has_value() || s->props.directory_;
        *rows[i].index = i;
        rows[i].view.key = flat[i].key_;
    }
    for (auto& row : s->rows) {
        if (row.view.realized) {
            retired.push_back(row.view.component);
            *row.index = tree_no_parent;
        }
    }
    for (auto it = s->requests.begin(); it != s->requests.end();) {
        if (!keys.contains(it->first) || !candidate.eligible(keys.at(it->first)) || !data_visible[keys.at(it->first)]) {
            it->second.cancelled->store(true);
            if (it->second.cancel) {
                it->second.cancel();
            }
            it = s->requests.erase(it);
        } else {
            ++it;
        }
    }
    if (!candidate.controlled_selected) {
        candidate.selected = candidate.normalize(candidate.selected, !candidate.multiple);
    }
    if (!candidate.controlled_checked) {
        candidate.checked = candidate.normalize(candidate.checked);
        candidate.half = candidate.normalize(candidate.half);
    }
    if (!candidate.controlled_expanded) {
        candidate.expanded = candidate.normalize(candidate.expanded);
    }
    if (!s->props.loaded_) {
        s->loaded = candidate.normalize(s->loaded);
    }
    s->rows = std::move(rows);
    s->keys = std::move(keys);
    s->flat = std::move(flat);
    s->items = items;
    s->model = std::move(candidate);
    const auto restore = [&](const auto& key) -> std::optional<std::size_t> {
        if (!key) {
            return {};
        }
        const auto it = s->keys.find(key->bytes());
        return it == s->keys.end() ? std::nullopt : std::optional{it->second};
    };
    s->entry = restore(entry_key);
    s->anchor = restore(anchor_key);
    s->hover.reset();
    s->reveal.reset();
    s->row_indices.clear();
    s->paint_rows.clear();
    s->paint_pending.assign(s->rows.size(), false);
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        if (s->rows[i].view.realized) {
            s->row_indices.emplace(s->rows[i].view.component.index, i);
        }
        s->rows[i].title.set(read_prop(s->flat[i].title_));
    }
    for (auto child : retired) {
        services_->destroy(child);
        if (!services_->components().contains(id)) {
            return;
        }
    }
    s->shown = candidate_shown;
    s->offsets = std::move(offsets);
    if (s->virtualized) {
        realize(id, std::max(1.F, bounds(*services_, id).height));
    } else {
        std::vector<std::size_t> missing;
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            if (!s->rows[i].view.realized) {
                missing.push_back(i);
            }
        }
        mount_rows(id, missing);
    }
    s->reconciling = false;
    refresh(id, true);
    if (intact(id) && focus_key && !s->keys.contains(focus_key->bytes()) && s->entry) {
        services_->focus().defer_focus(s->rows[*s->entry].view.interaction, input::FocusModality::keyboard);
    }
    failure.complete = intact(id);
}

void TreeComponentHost::realize(runtime::ComponentId id, float height) {
    auto s = state(*services_, id);
    if (!s || !s->virtualized) {
        return;
    }
    const auto& shown = s->shown;
    if (s->offsets.size() != shown.size() + 1) {
        s->offsets.assign(1, 0.F);
        for (auto i : shown) {
            const float height = s->props.item_height_ ? s->props.item_height_(s->model.nodes[i].key) : s->row_height;
            if (!std::isfinite(height) || height < 1 || height > 4096) {
                throw std::invalid_argument("Tree itemHeight must be finite in [1,4096]");
            }
            s->offsets.push_back(s->offsets.back() + height);
        }
    }
    const auto first = std::min(
        shown.size(),
        static_cast<std::size_t>(std::upper_bound(s->offsets.begin(), s->offsets.end(), std::max(0.F, s->scroll)) -
                                 s->offsets.begin() - 1));
    const auto begin = first > 2 ? first - 2 : 0;
    const auto end =
        std::min(shown.size(),
                 static_cast<std::size_t>(std::lower_bound(s->offsets.begin(), s->offsets.end(), s->scroll + height) -
                                          s->offsets.begin()) +
                     3);
    std::unordered_set<std::size_t> wanted;
    std::vector<std::size_t> missing;
    for (auto pos = begin; pos < end; ++pos) {
        wanted.insert(shown[pos]);
        if (!s->rows[shown[pos]].view.realized) {
            missing.push_back(shown[pos]);
        }
    }
    // Rich drafts remain mounted but blocked. Ordinary rows are truly destroyed outside the window.
    std::vector<runtime::ComponentId> retired;
    for (auto it = s->row_indices.begin(); it != s->row_indices.end();) {
        auto& row = s->rows[it->second];
        if (!wanted.contains(it->second)) {
            services_->set_input_blocked(row.view.component, true);
            if (!services_->components().contains(id)) {
                return;
            }
            services_->components().set_branch_active(row.view.component, false);
            row.view.shown = false;
            if (!row.rich && !s->flat[it->second].icon_) {
                retired.push_back(row.view.component);
                row.view.realized = false;
                row.required.clear();
                it = s->row_indices.erase(it);
                continue;
            }
        }
        ++it;
    }
    for (auto child : retired) {
        services_->destroy(child);
        if (!services_->components().contains(id)) {
            return;
        }
    }
    if (!missing.empty()) {
        mount_rows(id, missing);
    }
}

void TreeComponentHost::cancel_loads(runtime::ComponentId id) {
    auto s = state(*services_, id);
    if (!s) {
        return;
    }
    auto requests = std::move(s->requests);
    s->requests.clear();
    ++s->generation;
    for (auto& [key, request] : requests) {
        request.cancelled->store(true);
        if (request.cancel) {
            request.cancel();
        }
    }
    for (auto& row : s->rows) {
        row.view.loading = false;
        row.view.error = {};
    }
}

void TreeComponentHost::load(runtime::ComponentId id, String key) {
    auto s = state(*services_, id);
    if (!s || !s->props.loader_ || s->requests.contains(key.bytes()) ||
        std::ranges::find(s->loaded, key) != s->loaded.end()) {
        return;
    }
    const auto found = s->keys.find(key.bytes());
    if (found == s->keys.end() || s->model.nodes[found->second].leaf) {
        return;
    }
    auto flag = std::make_shared<std::atomic_bool>(false);
    auto once = std::make_shared<std::atomic_bool>(false);
    const auto ticket = ++s->generation;
    s->requests.emplace(std::string{key.bytes()}, TreeState::Request{flag, ticket, {}});
    s->rows[found->second].view.loading = true;
    s->rows[found->second].view.error = {};
    const auto source = s->props.loader_;
    const auto thread = s->thread;
    auto complete = [this, weak = std::weak_ptr{s}, id, key, ticket, flag, once, source,
                     thread](AsyncOptionsResult<TreeItem> result) mutable {
        if (flag->load() || once->exchange(true)) {
            return;
        }
        source.post()([this, weak, id, key, ticket, flag, thread, result = std::move(result)]() mutable {
            if (thread != std::this_thread::get_id() || flag->load()) {
                return;
            }
            auto s = weak.lock();
            if (!s || !intact(id)) {
                return;
            }
            auto request = s->requests.find(key.bytes());
            if (request == s->requests.end() || request->second.ticket != ticket) {
                return;
            }
            s->requests.erase(request);
            flag->store(true);
            auto row = s->keys.find(key.bytes());
            if (row == s->keys.end()) {
                return;
            }
            s->rows[row->second].view.loading = false;
            if (result.error.empty()) {
                auto items = s->items;
                const auto patch = [&](auto&& self, std::vector<TreeItem>& list) -> void {
                    for (auto& item : list) {
                        if (item.key_ == key) {
                            item.children_ = result.options;
                            if (item.children_.empty()) {
                                item.leaf_ = true;
                            }
                        } else {
                            self(self, item.children_);
                        }
                    }
                };
                patch(patch, items);
                try {
                    reconcile(id, items);
                } catch (...) {
                    if (intact(id)) {
                        if (const auto row = s->keys.find(key.bytes()); row != s->keys.end()) {
                            s->rows[row->second].view.error = u8"Invalid loaded topology";
                        }
                    }
                    throw;
                }
                if (!intact(id)) {
                    return;
                }
                if (!s->props.loaded_) {
                    s->loaded.push_back(key);
                }
            } else {
                s->rows[row->second].view.error = result.error;
            }
            const auto callback = s->props.on_load_;
            refresh(id, true);
            if (intact(id) && callback) {
                auto loaded = s->loaded;
                if (result.error.empty() && std::ranges::find(loaded, key) == loaded.end()) {
                    loaded.push_back(key);
                }
                callback({key, loaded, result.error});
            }
        });
    };
    try {
        auto cancel = source.provider()({key, ticket, flag}, std::move(complete));
        if (flag->load()) {
            if (cancel) {
                cancel();
            }
        } else if (auto it = s->requests.find(key.bytes()); it != s->requests.end() && it->second.ticket == ticket) {
            it->second.cancel = std::move(cancel);
        }
    } catch (...) {
        flag->store(true);
        s->requests.erase(std::string{key.bytes()});
        if (intact(id)) {
            if (const auto row = s->keys.find(key.bytes()); row != s->keys.end()) {
                s->rows[row->second].view.loading = false;
                s->rows[row->second].view.error = u8"Loading failed";
            }
            refresh(id, false);
        }
        throw;
    }
}

void TreeComponentHost::drag(runtime::ComponentId id, const input::PointerInputEvent& event) {
    auto s = state(*services_, id);
    if (!s || !s->pressed || !s->draggable) {
        return;
    }
    CallbackFailure failure{*services_, id};
    s->drag_pointer = event.pointer;
    s->drag_point = {event.x, event.y};
    const auto source_index = *s->pressed;
    const auto source_key = s->model.nodes[source_index].key;
    if (!s->dragging && event.action == input::PointerAction::move &&
        std::hypot(event.x - s->press_point.x, event.y - s->press_point.y) >= 4) {
        if (s->props.allow_drag_ && !s->props.allow_drag_(source_key)) {
            return;
        }
        if (!intact(id) || !s->pressed) {
            return;
        }
        TreeKeys keys;
        for (auto i = source_index; i < s->model.nodes[source_index].end; ++i) {
            keys.push_back(s->model.nodes[i].key);
        }
        s->dragging = TreeDragInfo{source_key, {}, keys};
        s->reveal.reset();
        if (s->props.drag_start_) {
            s->props.drag_start_(*s->dragging);
        }
        if (!intact(id) || !s->dragging) {
            return;
        }
    }
    if (!s->dragging) {
        return;
    }
    const auto previous = *s->dragging;
    auto info = previous;
    info.target_key = {};
    info.allowed = false;
    for (const auto& [component, i] : s->row_indices) {
        const auto& row = s->rows[i];
        if (!row.view.shown) {
            continue;
        }
        const auto r = bounds(*services_, row.view.component);
        if (!contains(graphics::intersect_effect_bounds(r, services_->nodes().content_clip(s->node, s->clip)), event.x,
                      event.y)) {
            continue;
        }
        info.target_key = s->model.nodes[i].key;
        const auto fraction = (event.y - r.y) / std::max(1.F, r.height);
        info.position = fraction < .25F   ? TreeDropPosition::Before
                        : fraction > .75F ? TreeDropPosition::After
                                          : TreeDropPosition::Inside;
        info.allowed = s->model.eligible(i) && !(i >= source_index && i < s->model.nodes[source_index].end);
        if (info.allowed && s->props.allow_drop_) {
            info.allowed = s->props.allow_drop_(info);
        }
        break;
    }
    if (!intact(id) || !s->dragging) {
        return;
    }
    s->dragging = info;
    if (previous.target_key != info.target_key) {
        if (!previous.target_key.empty() && s->props.drag_leave_) {
            s->props.drag_leave_(previous);
        }
        if (!intact(id) || !s->dragging) {
            return;
        }
        if (!info.target_key.empty() && s->props.drag_enter_) {
            s->props.drag_enter_(info);
        }
    }
    if (!intact(id) || !s->dragging) {
        return;
    }
    if (event.action == input::PointerAction::up) {
        s->dragging.reset();
        s->auto_scroll.reset();
        s->pressed.reset();
        services_->pointer().cancel_pointer_interaction(s->rows[source_index].view.interaction);
        if (info.allowed && s->props.drop_) {
            s->props.drop_(info);
        }
        if (intact(id) && s->props.drag_end_) {
            s->props.drag_end_(info);
        }
    } else {
        if (s->props.drag_over_) {
            s->props.drag_over_(info);
        }
        if (!intact(id)) {
            return;
        }
        const auto r = bounds(*services_, id);
        s->auto_scroll.reset();
        if (event.y < r.y + 20 || event.y > r.y + r.height - 20) {
            const float direction = event.y < r.y + 20 ? -1.F : 1.F;
            const float next =
                std::clamp(s->scroll + direction * s->row_height, 0.F, std::max(0.F, s->content_height - r.height));
            if (next != s->scroll) {
                s->scroll = next;
                refresh(id, true);
                if (intact(id) && s->dragging) {
                    s->auto_scroll = services_->animation_time() + animation::AnimationDuration::milliseconds(16);
                    services_->request_frame();
                }
            }
        }
    }
    if (intact(id)) {
        s->paint_all = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
}

void TreeComponentHost::activate(runtime::ComponentId id, std::size_t i, int zone, bool additive, bool range) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (i >= s->rows.size() || !s->rows[i].view.shown || !s->model.eligible(i) ||
        !services_->interactions().branch_active(s->rows[i].view.interaction)) {
        return;
    }
    const auto key = s->model.nodes[i].key;
    std::function<void()> callback;
    if (zone == 1) {
        if (s->model.nodes[i].leaf || (s->model.nodes[i].end == i + 1 && !s->props.loader_)) {
            return;
        }
        auto next = s->model.expand(i);
        if (!s->rows[i].view.error.empty() && std::ranges::find(next, key) == next.end()) {
            next.push_back(key);
        }
        const bool expanded = std::ranges::find(next, key) != next.end();
        if (!s->model.controlled_expanded) {
            s->model.expanded = next;
        }
        if (expanded && s->props.loader_) {
            load(id, key);
            if (!intact(id)) {
                return;
            }
        }
        if (s->expand) {
            callback = [f = s->expand, info = TreeExpandInfo{key, next, expanded}] {
                f(info);
            };
        }
    } else if (zone == 2) {
        if (!s->model.checkable || !s->model.nodes[i].checkable || s->model.nodes[i].check_disabled) {
            return;
        }
        auto next = s->model.toggle_check(i);
        const bool checked = std::ranges::find(next.checked, key) != next.checked.end();
        if (!s->model.controlled_checked) {
            s->model.checked = next.checked;
            if (!s->props.half_) {
                s->model.half = next.half_checked;
            }
        }
        if (s->check) {
            callback = [f = s->check, info = TreeCheckInfo{key, next.checked, next.half_checked, checked}] {
                f(info);
            };
        }
    } else {
        if (!s->model.selectable || !s->model.nodes[i].selectable) {
            return;
        }
        auto next = s->model.select(i, additive, range ? s->anchor : std::optional<std::size_t>{});
        if (s->props.directory_ && !additive && !range) {
            next = {key};
        }
        if (!range) {
            s->anchor = i;
        }
        const bool selected = std::ranges::find(next, key) != next.end();
        if (!s->model.controlled_selected) {
            s->model.selected = next;
        }
        if (s->select) {
            callback = [f = s->select, info = TreeSelectInfo{key, next, selected}] {
                f(info);
            };
        }
    }
    s->motion->restart(true);
    refresh(id, true);
    if (zone == 0 && s->props.directory_ && s->props.expand_action_ == DirectoryExpandAction::Click && intact(id)) {
        activate(id, i, 1);
    }
    if (intact(id) && callback) {
        CallbackFailure failure{*services_, id};
        callback();
        static_cast<void>(intact(id));
    }
}

runtime::Size TreeComponentHost::measure_row(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& e,
                                             layout::Constraints c) {
    if (!intact(id)) {
        return c.constrain({});
    }
    const auto s = state(*services_, id);
    auto& row = s->rows[i];
    const auto m = services_->components().theme_scope(id)->snapshot().tree().metrics;
    const auto previous_title = row.title_size;
    row.indent = std::min(c.max_width, static_cast<float>(s->model.nodes[i].depth) * m.indent_size);
    float available = std::max(0.0F, c.max_width - row.indent);
    const auto take = [&](float amount) {
        const float value = std::min(available, amount);
        available -= value;
        return value;
    };
    row.switcher_width = take(m.switcher_size);
    row.checkbox_width = s->model.checkable ? take(m.checkbox_size + m.icon_gap) : 0;
    row.icon_width = s->icons && row.custom_icon ? take(m.title_height + m.icon_gap) : 0;
    const auto measure = [&](runtime::ComponentId child, float width) {
        return e.measure_child(services_->components().root(child), {0, width, 0, infinity});
    };
    static_cast<void>(measure(row.view.switcher, std::min(m.icon_size, row.switcher_width)));
    static_cast<void>(measure(row.view.checkbox, std::min(m.checkbox_size, row.checkbox_width)));
    static_cast<void>(measure(row.view.icon, std::min(m.title_height, row.icon_width)));
    row.title_size = measure(row.view.title, std::max(0.0F, available - 2 * std::min(m.padding_inline, available / 2)));
    if (row.title_size != previous_title) {
        queue_paint(*s, i);
    }
    row.height = std::max({m.title_height, s->model.checkable ? m.checkbox_size : 0, row.title_size.height});
    return c.constrain({row.indent + row.switcher_width + row.checkbox_width + row.icon_width + row.title_size.width +
                            2 * m.padding_inline,
                        row.height});
}

runtime::Size TreeComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e, layout::Constraints c) {
    if (!intact(id)) {
        return c.constrain({});
    }
    const auto s = state(*services_, id);
    const auto m = services_->components().theme_scope(id)->snapshot().tree().metrics;
    const auto& shown = s->shown;
    const float padding = shown.empty() ? 0.0F : focus_padding(m);
    const float inline_padding = std::min(padding, c.max_width / 2);
    const float block_padding = std::min(padding, c.max_height / 2);
    runtime::Size natural;
    if (s->virtualized) {
        natural = {std::isfinite(c.max_width) ? c.max_width - 2 * inline_padding : 180.F, s->offsets.back()};
        for (const auto& [component, i] : s->row_indices) {
            auto& row = s->rows[i];
            if (!row.view.shown) {
                continue;
            }
            auto pos = std::ranges::find(shown, i);
            row.top = s->offsets[static_cast<std::size_t>(pos - shown.begin())];
            const auto height = s->offsets[static_cast<std::size_t>(pos - shown.begin()) + 1] - row.top;
            e.measure_child(services_->components().root(row.view.component),
                            {0, std::max(0.F, c.max_width - 2 * inline_padding), height, height});
            row.height = height;
        }
    } else {
        bool first = true;
        for (auto i : shown) {
            if (!first) {
                natural.height += m.row_gap;
            }
            first = false;
            s->rows[i].top = natural.height;
            const auto size = e.measure_child(services_->components().root(s->rows[i].view.component),
                                              {0, std::max(0.F, c.max_width - 2 * inline_padding), 0, infinity});
            natural.width = std::max(natural.width, size.width);
            natural.height += size.height;
        }
    }
    natural.width += 2 * inline_padding;
    natural.height += 2 * block_padding;
    s->content_height = natural.height;
    return c.constrain(natural);
}

void TreeComponentHost::place_row(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& e, runtime::Rect r) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const auto& row = s->rows[i];
    if (row.painted_bounds != r) {
        queue_paint(*s, i);
    }
    const auto m = services_->components().theme_scope(id)->snapshot().tree().metrics;
    float x = r.x + row.indent;
    const auto place_center = [&](runtime::ComponentId child, float width, float extent) {
        const float side = std::max(0.0F, std::min({width, extent, r.height}));
        e.place_child(services_->components().root(child),
                      {s->rtl ? r.x + r.width - (x - r.x) - width + (width - side) / 2 : x + (width - side) / 2,
                       r.y + (r.height - side) / 2, side, side},
                      true, true);
        x += width;
    };
    place_center(row.view.switcher, row.switcher_width, m.switcher_size);
    place_center(row.view.checkbox, row.checkbox_width, m.checkbox_size);
    place_center(row.view.icon, row.icon_width, m.title_height);
    const float remaining = std::max(0.0F, r.x + r.width - x);
    const float padding = std::min(m.padding_inline, remaining / 2);
    e.place_child(services_->components().root(row.view.title),
                  {s->rtl ? r.x + padding : x + padding,
                   r.y + (r.height - std::min(r.height, row.title_size.height)) / 2,
                   std::max(0.0F, remaining - 2 * padding), std::min(r.height, row.title_size.height)});
}

void TreeComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect r) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const auto& shown = s->shown;
    const float padding =
        shown.empty() ? 0.0F : focus_padding(services_->components().theme_scope(id)->snapshot().tree().metrics);
    const float inline_padding = std::min(padding, r.width / 2);
    const float block_padding = std::min(padding, r.height / 2);
    const runtime::Rect content{r.x + inline_padding, r.y + block_padding, std::max(0.0F, r.width - 2 * inline_padding),
                                std::max(0.0F, r.height - 2 * block_padding)};
    s->content_height = shown.empty()    ? 0.0F
                        : s->virtualized ? s->offsets.back() + 2 * block_padding
                                         : s->rows[shown.back()].top + s->rows[shown.back()].height + 2 * block_padding;
    s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, s->content_height - r.height));
    if (s->reveal && s->rows[*s->reveal].view.shown) {
        const auto& row = s->rows[*s->reveal];
        if (row.top < s->scroll) {
            s->scroll = row.top;
        } else if (row.top + row.height > s->scroll + content.height) {
            s->scroll = std::max(row.top, row.top + row.height - content.height);
        }
        s->reveal.reset();
    }
    if (s->scroll_request) {
        const auto it = s->keys.find(s->scroll_request->key.bytes());
        if (it != s->keys.end()) {
            s->scroll = s->rows[it->second].top + s->scroll_request->offset;
        }
        s->scroll_request.reset();
    }
    s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, s->content_height - r.height));
    for (auto i : shown) {
        const auto& row = s->rows[i];
        if (!row.view.realized || !row.view.shown) {
            continue;
        }
        e.place_child(services_->components().root(row.view.component),
                      {content.x, content.y + row.top - s->scroll, content.width, row.height});
    }
}

bool TreeComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (input::has_modifier(event.modifiers, input::KeyModifier::alt)) {
        return false;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const auto focus = services_->focus().state().focused;
        auto current = std::ranges::find_if(s->rows, [&](const auto& row) { return focus == row.view.interaction; });
        if (current == s->rows.end()) {
            continue;
        }
        const auto index = static_cast<std::size_t>(current - s->rows.begin());
        if (!s->model.eligible(index) || !current->view.shown ||
            !services_->interactions().branch_active(current->view.interaction)) {
            return false;
        }
        const auto command = s->rtl && event.key == input::Key::left    ? input::Key::right
                             : s->rtl && event.key == input::Key::right ? input::Key::left
                                                                        : event.key;
        const bool activation = command == input::Key::enter || command == input::Key::space;
        const bool navigation = command == input::Key::up || command == input::Key::down ||
                                command == input::Key::left || command == input::Key::right ||
                                command == input::Key::home || command == input::Key::end;
        if (!activation && !navigation) {
            return false;
        }
        if (event.action != input::KeyAction::down || (activation && event.repeat)) {
            return true;
        }
        auto target = index;
        auto shown = s->shown;
        std::erase_if(shown, [&](auto i) { return !s->model.eligible(i); });
        auto position = std::ranges::find(shown, index);
        if (command == input::Key::home) {
            target = shown.front();
        } else if (command == input::Key::end) {
            target = shown.back();
        } else if (command == input::Key::up && position != shown.begin()) {
            target = *std::prev(position);
        } else if (command == input::Key::down && std::next(position) != shown.end()) {
            target = *std::next(position);
        } else if (command == input::Key::right) {
            if (!s->model.open(index)) {
                activate(id, index, 1);
                return true;
            }
            if (std::next(position) != shown.end() && *std::next(position) < s->model.nodes[index].end) {
                target = *std::next(position);
            }
        } else if (command == input::Key::left) {
            if (s->model.open(index)) {
                activate(id, index, 1);
                return true;
            }
            auto parent = s->model.nodes[index].parent;
            while (parent != tree_no_parent && !s->model.eligible(parent)) {
                parent = s->model.nodes[parent].parent;
            }
            if (parent != tree_no_parent) {
                target = parent;
            }
        }
        if (s->virtualized && !s->rows[target].view.realized) {
            auto pos = std::ranges::find(s->shown, target);
            s->scroll = s->offsets[static_cast<std::size_t>(pos - s->shown.begin())];
            realize(id, std::max(1.F, bounds(*services_, id).height));
            refresh(id, true);
        }
        entry(id, target);
        if (navigation && input::has_modifier(event.modifiers, input::KeyModifier::shift)) {
            activate(id, target, 0,
                     input::has_modifier(event.modifiers, input::KeyModifier::control) ||
                         input::has_modifier(event.modifiers, input::KeyModifier::meta),
                     true);
        }
        s->reveal = target;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout);
        services_->focus().defer_focus(s->rows[target].view.interaction, input::FocusModality::keyboard);
        if (activation && intact(id)) {
            activate(id, index, command == input::Key::space && s->model.checkable ? 2 : 0,
                     input::has_modifier(event.modifiers, input::KeyModifier::control) ||
                         input::has_modifier(event.modifiers, input::KeyModifier::meta),
                     input::has_modifier(event.modifiers, input::KeyModifier::shift));
        }
        return true;
    }
    return false;
}

void TreeComponentHost::on_pointer_input(const input::PointerInputEvent& event, std::optional<input::InteractionId> hit,
                                         std::optional<input::InteractionId>) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        std::optional<std::size_t> next;
        const auto* target =
            hit && event.action != input::PointerAction::cancel ? services_->interactions().find(*hit) : nullptr;
        if (target) {
            const auto found = s->row_indices.find(target->component.index);
            if (found != s->row_indices.end()) {
                const auto i = found->second;
                if (hit == s->rows[i].view.interaction && s->model.eligible(i)) {
                    next = i;
                }
            }
        }
        if (s->draggable && hit == s->root_interaction && event.action != input::PointerAction::cancel) {
            for (const auto& [component, i] : s->row_indices) {
                if (s->rows[i].view.shown && s->model.eligible(i) &&
                    contains(bounds(*services_, s->rows[i].view.component), event.x, event.y)) {
                    next = i;
                    break;
                }
            }
        }
        if (s->hover != next) {
            const auto previous = s->hover;
            s->hover = next;
            if (previous) {
                present(id, *previous);
            }
            if (next && intact(id)) {
                present(id, *next);
            }
        }
    }
}

void TreeComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (!services_->interactions().component_active(id)) {
            cancel_loads(id);
            if (!intact(id)) {
                continue;
            }
            if (s->motion) {
                s->motion->settle();
            }
        }
        s->paint_all = true;
        if (s->pressed && !s->dragging &&
            !services_->interactions().branch_active(s->rows[*s->pressed].view.interaction)) {
            const auto hit = s->rows[*s->pressed].view.interaction;
            s->pressed.reset();
            services_->pointer().cancel_pointer_interaction(hit);
        }
    }
}

void TreeComponentHost::on_window_active(bool) {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id, false);
    }
}

void TreeComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const auto focus = services_->focus().state().focused;
        if (focus != s->last_focus) {
            s->last_focus = focus;
            if (auto row = focused_row(*services_, *s)) {
                if (s->model.eligible(*row) && s->rows[*row].view.shown) {
                    entry(id, *row);
                }
                if (!s->dragging) {
                    s->reveal = row;
                }
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout);
            }
        }
    }
}

bool TreeComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    if (!record) {
        return false;
    }
    // A Tree at its scroll boundary hands off to a logical Tree ancestor,
    // while the nearest window layer remains an ownership boundary.
    for (auto owner = std::optional{record->component}; owner; owner = services_->components().parent(*owner)) {
        if (std::ranges::find(mounted_, *owner) != mounted_.end() && intact(*owner)) {
            const auto s = state(*services_, *owner);
            if (!services_->interactions().component_active(*owner) ||
                !contains(services_->nodes().content_clip(s->node, s->clip), event.x, event.y)) {
                return false;
            }
            const float height = bounds(*services_, *owner).height;
            const float maximum = std::max(0.0F, s->content_height - height);
            if (maximum > 0) {
                const float next = std::clamp(s->scroll - event.delta_y * 32, 0.0F, maximum);
                if (s->scroll != next) {
                    s->scroll = next;
                    if (s->virtualized) {
                        refresh(*owner, true);
                    }
                    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                               runtime::DirtyFlags::HitTest);
                    return true;
                }
            }
        }
        if (services_->components().is_window_layer_root(*owner)) {
            return false;
        }
    }
    return false;
}

void TreeComponentHost::position_window_layers(runtime::Size, runtime::Rect) {
    // Coordinate committed zero-area bounds before child popup/effect painting.
    // Never invoke the input boundary callbacks from LayoutEngine::place_child.
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const auto& committed = services_->nodes().require(s->node);
        if (s->virtualized && s->viewport_height != committed.bounds.height) {
            s->viewport_height = committed.bounds.height;
            refresh(id, true);
            if (!intact(id)) {
                continue;
            }
        }
        const bool geometry_blocked =
            committed.place_generation != 0 && (committed.bounds.width <= 0 || committed.bounds.height <= 0);
        if (s->geometry_blocked != geometry_blocked) {
            s->geometry_blocked = geometry_blocked;
            refresh(id, false);
        }
    }
}

std::size_t TreeComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed = 0;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (!s->auto_scroll) {
            continue;
        }
        const auto pointer = services_->pointer().state(s->drag_pointer);
        if (!s->draggable || !s->dragging || !s->pressed || !pointer || !pointer->primary_down ||
            pointer->capture != s->root_interaction || !services_->interactions().component_active(id)) {
            s->auto_scroll.reset();
            continue;
        }
        if (time < *s->auto_scroll) {
            continue;
        }
        const float before = s->scroll;
        drag(id, {s->drag_pointer, input::PointerAction::move, input::PointerButton::none, s->drag_point.x,
                  s->drag_point.y});
        if (intact(id) && s->auto_scroll) {
            s->auto_scroll = time + animation::AnimationDuration::milliseconds(16);
        }
        if (s->scroll != before) {
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> TreeComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> next;
    for (auto id : mounted_) {
        const auto s = state(*services_, id);
        if (s && s->auto_scroll && (!next || *s->auto_scroll < *next)) {
            next = s->auto_scroll;
        }
    }
    return next;
}

void TreeComponentHost::synchronize_auxiliary_motion() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (s->motion && (!services_->interactions().component_active(id) ||
                          !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                            services_->motion_preference())
                               .enabled())) {
            s->motion->settle();
        }
    }
}

void TreeComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);

        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto t = services_->components().theme_scope(id)->snapshot().tree();
        const auto& m = t.metrics;
        const auto& root = services_->nodes().require(s->node);
        const auto clip = services_->nodes().content_clip(s->node, s->clip);
        const auto decoration_clip =
            services_->component_clip(id, viewport, document_clip, runtime::ClipPurpose::decoration);
        const float opacity =
            services_->components().branch_active(id) ? services_->nodes().content_opacity(s->node) : 0;
        const bool paint_all = s->paint_all || s->painted_clip != clip ||
                               s->painted_decoration_clip != decoration_clip || s->painted_bounds != root.bounds ||
                               s->painted_translation != root.translation || s->painted_opacity != opacity;
        s->paint_all = paint_all;
        s->painted_clip = clip;
        s->painted_decoration_clip = decoration_clip;
        s->painted_bounds = root.bounds;
        s->painted_translation = root.translation;
        s->painted_opacity = opacity;
        const auto background =
            graphics::make_corner_fill_effects({root.bounds, 0}, {false, false, false, false}, t.colors.background,
                                               opacity, root.translation, graphics::EffectClip{1, clip});
        static_cast<void>(services_->surfaces().update_content_effects(s->background, background));
        if (paint_all) {
            for (std::size_t i = 0; i < s->rows.size(); ++i) {
                queue_paint(*s, i);
            }
        }
        for (auto i : s->paint_rows) {
            ++s->work.presented_rows;
            auto& row = s->rows[i];
            if (!row.view.realized) {
                continue;
            }
            const auto& node = services_->nodes().require(services_->components().root(row.view.component));
            row.painted_bounds = node.bounds;
            std::vector<graphics::RoundedEffectInstance> effects;
            const float alpha = row.view.shown ? opacity * (s->motion ? .5F + .5F * s->motion->value() : 1.F) : 0;
            const auto add = [&](runtime::Rect r, float radius, Color color, graphics::EffectClip c) {
                auto values =
                    graphics::make_corner_fill_effects({r, std::min(radius, std::min(r.width, r.height) / 2)},
                                                       {true, true, true, true}, color, alpha, node.translation, c);
                effects.insert(effects.end(), values.begin(), values.end());
            };
            const auto row_clip =
                services_->nodes().content_clip(services_->components().root(row.view.component), clip);
            const float start = std::min(node.bounds.width, row.indent + row.switcher_width + row.checkbox_width);
            const runtime::Rect content{
                node.bounds.x + start, node.bounds.y,
                std::min(std::max(0.0F, node.bounds.width - start),
                         s->block ? node.bounds.width : row.icon_width + row.title_size.width + 2 * m.padding_inline),
                node.bounds.height};
            {
                // Keep the background slot stable across hover/focus changes.
                const auto background_alpha = row.view.selected || s->hover == i ? 1.0F : 0.0F;
                const auto color =
                    row.view.selected ? t.colors.node_selected_background : t.colors.node_hover_background;
                add(content, m.border_radius,
                    Color{color.red(), color.green(), color.blue(), color.alpha() * background_alpha}, {1, row_clip});
            }
            if (s->lines && s->model.nodes[i].depth > 0) {
                const float line_x =
                    s->rtl ? node.bounds.x + node.bounds.width - row.indent : node.bounds.x + row.indent;
                add({line_x, node.bounds.y, 1.F, node.bounds.height}, 0, t.colors.checkbox_border, {1, row_clip});
                add({s->rtl ? line_x - row.switcher_width / 2 : line_x, node.bounds.y + node.bounds.height / 2,
                     row.switcher_width / 2, 1.F},
                    0, t.colors.checkbox_border, {1, row_clip});
            }
            if (s->dragging && s->dragging->allowed && s->dragging->target_key == s->model.nodes[i].key) {
                if (s->dragging->position == TreeDropPosition::Inside) {
                    add(content, m.border_radius, t.colors.node_hover_background, {1, row_clip});
                } else {
                    add({node.bounds.x,
                         s->dragging->position == TreeDropPosition::Before ? node.bounds.y
                                                                           : node.bounds.y + node.bounds.height - 2,
                         node.bounds.width, 2},
                        0, t.colors.focus, {1, row_clip});
                }
            }
            if (s->model.checkable) {
                const auto checkbox =
                    services_->nodes().require(services_->components().root(row.view.checkbox)).bounds;
                const bool checked = row.view.checked || row.view.indeterminate;
                const bool disabled =
                    s->model.disabled || s->model.nodes[i].disabled || s->model.nodes[i].check_disabled;
                const auto check_clip =
                    graphics::intersect_effect_bounds(bounds(*services_, row.view.checkbox), row_clip);
                add(checkbox, m.border_radius,
                    disabled  ? t.colors.disabled
                    : checked ? t.colors.checkbox_checked
                              : t.colors.checkbox_border,
                    graphics::EffectClip{1, check_clip});
                if (!checked || disabled) {
                    const float border = std::min({m.border_width, checkbox.width / 2, checkbox.height / 2});
                    add({checkbox.x + border, checkbox.y + border, checkbox.width - 2 * border,
                         checkbox.height - 2 * border},
                        std::max(0.0F, m.border_radius - border), t.colors.checkbox_background,
                        graphics::EffectClip{1, check_clip});
                }
                if (row.view.indeterminate) {
                    const float side = std::min(checkbox.width, checkbox.height) / 2;
                    add({checkbox.x + (checkbox.width - side) / 2, checkbox.y + (checkbox.height - side) / 2, side,
                         side},
                        0, disabled ? t.colors.disabled : t.colors.checkbox_mark, graphics::EffectClip{1, check_clip});
                }
            }
            const auto focus = services_->focus().state();
            const bool focused = focus.focused == row.view.interaction && focus.focus_visible && s->model.eligible(i) &&
                                 services_->interactions().branch_active(row.view.interaction) && row.view.shown &&
                                 content.width > 0 && content.height > 0;
            const auto external = services_->component_clip(row.view.component, viewport, document_clip,
                                                            runtime::ClipPurpose::decoration);
            effects.push_back(graphics::make_outline_effect(
                {content, std::min({m.border_radius, content.width / 2, content.height / 2})},
                m.focus_width > 0 ? m.focus_width : 1, m.focus_offset, t.colors.focus,
                focused && m.focus_width > 0 ? alpha : 0, node.translation, graphics::EffectClip{2, external}));
            static_cast<void>(services_->surfaces().update_content_effects(row.surface, effects));
        }
        s->paint_rows.clear();
        std::fill(s->paint_pending.begin(), s->paint_pending.end(), false);
        s->paint_all = false;
    }
}
} // namespace ryn::detail

namespace ryn {
void Tree(TreeProps props, std::vector<TreeItem> items) {
    if (!detail::active_tree) {
        throw std::logic_error("Tree requires window component services");
    }
    detail::active_tree->mount(props, items);
}
} // namespace ryn
