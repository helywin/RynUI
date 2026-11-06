#include "component/mentions_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/mentions_model.hpp"
#include "component/option_list_support.hpp"
#include "component/input_component.hpp"
#include "component/popover_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <exception>

namespace ryn::detail {
namespace {
thread_local MentionsComponentHost* active_mentions{};

struct PrivateRegion final {
    runtime::ComponentId id;
    runtime::NodeId node;
    std::optional<runtime::ComponentId> parent;
};

struct RetireOnFailure final {
    WindowComponentServices& services;
    runtime::ComponentId owner;
    bool complete{};

    ~RetireOnFailure() noexcept {
        if (!complete) {
            (void)services.retire_failed_component(owner);
        }
    }
};

struct RetireOnException final {
    WindowComponentServices& services;
    runtime::ComponentId owner;
    int exceptions{std::uncaught_exceptions()};

    ~RetireOnException() noexcept {
        if (std::uncaught_exceptions() > exceptions) {
            (void)services.retire_failed_component(owner);
        }
    }
};

struct Row final {
    runtime::NodeId node;
    runtime::ComponentId label_root;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    String key;
    std::size_t index{};
    MentionsRichLabel rich;
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
};

struct State final {
    explicit State(MentionsModel value) : model(std::move(value)) {}

    MentionsModel model;
    TextAreaProps textarea_props;
    std::optional<Prop<std::size_t>> max_length;
    Signal<bool> clear_disabled{false};
    MentionsConfig config;
    MentionsSearchValidator validator;
    MentionsPlacement placement{MentionsPlacement::Bottom};
    std::optional<MentionQuery> active_query;
    String observed_text;
    input::TextSelection observed_selection;
    bool observed_composing{};
    bool observed{};
    bool reset_query{};
    bool selecting{};
    bool loading{};
    bool shown{true};
    bool caret_offscreen{};
    String not_found_text{u8"No data"};

    runtime::NodeId node;
    runtime::ComponentId input;
    runtime::ComponentId editor_root;
    runtime::ComponentId popup_body;
    runtime::ComponentId empty_root;
    runtime::ComponentId empty_label_root;
    runtime::ComponentId empty_rich_root;
    MentionsRichLabel empty_content;
    Signal<String> not_found{String{}};
    Signal<runtime::SemanticForeground> empty_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> empty_font{runtime::SemanticTypography{}};
    Signal<IconSource> clear_icon{IconSource{IconName::CloseCircleFilled}};
    MentionsEditor editor_render;
    MentionsPopupRenderer popup_render;
    MentionsSource source;
    std::shared_ptr<AsyncOptionsSession<MentionsOption>> async{std::make_shared<AsyncOptionsSession<MentionsOption>>()};
    OptionPopupMotion motion;
    Signal<TextDirection> direction{TextDirection::Auto};
    bool animate{};
    bool virtual_list{true};
    bool match_width{true};
    float popup_width{};
    bool pending{};
    bool auto_focus{};
    std::function<void(String)> on_load_error;
    std::function<void(float)> on_scroll;
    runtime::ComponentId popup;
    runtime::ComponentId list;
    input::InteractionId input_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    std::vector<PrivateRegion> regions;
    runtime::NodeId input_node;
    std::shared_ptr<Scope> bindings{std::make_shared<Scope>()};
    std::uint64_t model_revision{};
    std::uint64_t load_revision{};
    std::uint64_t refresh_generation{};
    bool private_ready{};
    bool mounting{true};
    bool reconciling{};
    bool rows_dirty{};
    bool focus_was_inside{};
    bool zero_geometry{};
    Signal<String> placeholder{String{}};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<InputStatus> status{InputStatus::Default};
    std::vector<runtime::ComponentId> rows;
    std::vector<runtime::ComponentId> parked;
    MentionsRef reference;
    std::optional<Prop<String>> authority;
    Signal<String> display{String{}};
    Signal<bool> clear_visible{false};
    Signal<ThemeConfig> input_theme{*std::make_unique<ThemeConfig>()};
    std::array<theme_runtime::Subscription, 6> subscriptions;
    MentionsFilter filter;
    bool active_first{true};
    bool allow_clear{};
    bool controlled_open{};
    bool open{};
    bool suppressed{};
    bool visible{};
    bool has_been_active{};
    bool closed_on_blur{};
    bool reveal_active{true};
    bool needs_layout{true};
    std::uint64_t layout_generation{};
    float list_height{256};
    float scroll{};
    runtime::Rect anchor;
    runtime::Rect available;
    runtime::Rect bounds;
    runtime::Rect clip;
    std::function<void(String)> on_change;
    std::function<void(String, String)> on_search;
    std::function<void(MentionsOption, String)> on_select;
    std::function<void()> on_clear;
    std::function<void(bool)> on_open;
    std::function<void()> on_focus;
    std::function<void()> on_blur;
};

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

graphics::QuadInstance quad(runtime::Rect r, Color c, float radius = 0) {
    graphics::QuadInstance q;
    q.bounds = {r.x, r.y, r.width, r.height};
    q.color = {c.red(), c.green(), c.blue(), c.alpha()};
    q.corner_radius = radius;
    return q;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

bool inside(runtime::Rect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

std::optional<input::InteractionId> tab_neighbor(WindowComponentServices& services, input::InteractionId anchor,
                                                 bool reverse) {
    std::vector<input::InteractionId> order;
    for (auto hit : services.interactions().declaration_order()) {
        const auto* record = services.interactions().find(hit);
        if (record && record->eligible && record->focusable && record->tab_stop &&
            services.interactions().branch_active(hit)) {
            order.push_back(hit);
        }
    }
    const auto found = std::ranges::find(order, anchor);
    if (found == order.end()) {
        return {};
    }
    const auto index = static_cast<std::size_t>(found - order.begin());
    return order[(index + (reverse ? order.size() - 1 : 1)) % order.size()];
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (*p == parent) {
            return true;
        }
    }
    return false;
}

// A virtual row may own the active native editor. Move focus within the
// compound control before hiding it, so scrolling ends preedit without closing the list.
void restore_row_focus(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId row,
                       input::InteractionId editor) {
    const auto focused = services.focus().state().focused;
    const auto* record = focused ? services.interactions().find(*focused) : nullptr;
    if (record && descendant(services, record->component, row)) {
        RetireOnFailure rollback{services, owner};
        services.focus().defer_focus(editor, input::FocusModality::keyboard);
        rollback.complete = true;
    }
}

// All callbacks return through this exact owner and private-region generation check.
// Retiring a damaged owner never targets a replacement that reused a store index.
State* checked(WindowComponentServices& services, runtime::ComponentId id) {
    auto& components = services.components();
    auto* state = components.state<State>(id);
    if (!state || !components.scope(id).active()) {
        return nullptr;
    }
    bool valid = services.nodes().find(state->node) && components.root(id) == state->node;
    for (const auto& region : state->regions) {
        valid = valid && components.contains(region.id) && components.scope(region.id).active() &&
                components.root(region.id) == region.node && services.nodes().find(region.node) &&
                components.parent(region.id) == region.parent && descendant(services, region.id, id);
    }
    if (state->input.valid()) {
        const auto* record = services.interactions().find(state->input_hit);
        const auto inputs = services.input_runtime()->mounted_inputs();
        valid = valid && record && record->component == state->input && record->node == state->input_node &&
                services.input_runtime()->editors().find(state->editor) &&
                std::ranges::any_of(inputs, [&](const auto& input) {
                    return input.component == state->input && input.node == state->input_node &&
                           input.interaction == state->input_hit && input.editor == state->editor;
                });
    } else if (state->private_ready) {
        valid = false;
    }
    if (state->private_ready) {
        valid = valid && state->popup.valid() && state->list.valid();
    }
    for (auto child : state->rows) {
        const auto* row = components.state<Row>(child);
        valid = valid && row && components.scope(child).active() && components.parent(child) == state->list &&
                components.root(child) == row->node && services.nodes().find(row->node) &&
                components.contains(row->hit);
    }
    if (!valid) {
        services.retire_failed_component(id);
        return nullptr;
    }
    return state;
}

void remember_region(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId child) {
    if (auto* state = checked(services, owner)) {
        state->regions.push_back({child, services.components().root(child), services.components().parent(child)});
    }
}

void forget_region(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId child) {
    if (auto* state = checked(services, owner)) {
        std::erase_if(state->regions, [&](const auto& region) { return descendant(services, region.id, child); });
    }
}

template <class F>
std::optional<bool> change_model(WindowComponentServices& services, runtime::ComponentId id, F operation) {
    const auto* state = checked(services, id);
    if (!state) {
        return {};
    }
    const auto revision = state->model_revision;
    auto candidate = state->model;
    const bool changed = operation(candidate);
    auto* current = checked(services, id);
    if (!current || current->model_revision != revision) {
        return {};
    }
    if (changed) {
        current->model = std::move(candidate);
        ++current->model_revision;
    }
    return changed;
}

bool display_model(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* state = checked(services, id);
    if (!state) {
        return false;
    }
    auto display = state->display;
    auto value = state->model.display();
    display.set(std::move(value));
    return checked(services, id) != nullptr;
}

MentionsFilter guarded_filter(WindowComponentServices& services, runtime::ComponentId id, MentionsFilter filter) {
    return MentionsFilter{[&services, id, filter = std::move(filter)](StringView query, const MentionsOption& option) {
        if (!checked(services, id)) {
            return false;
        }
        const bool result = filter.accepts(query, option);
        return checked(services, id) && result;
    }};
}

void mount_content(WindowComponentServices& services, runtime::ComponentId parent, const Content& content,
                   bool mounting, std::optional<Prop<runtime::SemanticForeground>> color = {},
                   std::optional<Prop<runtime::SemanticTypography>> font = {}) {
    const auto theme = services.components().theme_scope(parent);
    const Content typed{[parent, content, color, font] {
        auto& build = runtime::require_component_build_context();
        if (color && font) {
            build.mount_slot_with_semantic_text_style(parent, content, *color, *font);
        } else {
            build.mount_slot(parent, content);
        }
    }};
    const Content themed{
        [theme, typed] { runtime::require_component_build_context().mount_slot_with_theme_scope(typed, theme); }};
    if (mounting) {
        SlotContentAccess::function(themed)();
    } else {
        services.append_slot(parent, themed);
    }
}

std::optional<runtime::ComponentId> layer(WindowComponentServices& services, runtime::ComponentId child) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (services.components().is_window_layer_root(*p)) {
            return p;
        }
    }
    return {};
}

runtime::ComponentId group(WindowComponentServices& services, bool leaf = false) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    if (leaf) {
        services.layout().set_layout(node, layout::LeafLayout{});
    } else {
        services.layout().set_layout(node, layout::BoxLayout{});
    }
    build.on_resource_cleanup(id, [&services, node, id] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    return id;
}

void validate_height(float height) {
    if (!std::isfinite(height) || height <= 0) {
        throw std::invalid_argument("Mentions listHeight must be finite and positive");
    }
}
} // namespace

MentionsComponentHost::MentionsComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

MentionsComponentHost::~MentionsComponentHost() {
    services_->detach(*this);
}

void* MentionsComponentHost::begin_mount() noexcept {
    ++mount_depth_;
    auto* old = active_mentions;
    active_mentions = this;
    return old;
}

void MentionsComponentHost::end_mount(void* old) noexcept {
    --mount_depth_;
    active_mentions = static_cast<MentionsComponentHost*>(old);
}

void MentionsComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

MentionsSnapshot MentionsComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = checked(*services_, id);
    if (!s) {
        throw std::out_of_range("stale Mentions");
    }
    MentionsSnapshot result{s->input,         s->popup,          s->rows,      s->model.value(), s->model.query(),
                            s->display.get(), s->model.active(), s->bounds,    s->clip,          s->scroll,
                            s->visible,       s->open,           s->suppressed};
    for (auto child : s->rows) {
        result.row_indices.push_back(services_->components().state<Row>(child)->index);
    }
    result.total_rows = s->model.filtered().size();
    result.pending = s->pending;
    if (s->active_query) {
        result.prefix = s->active_query->prefix;
        result.query_begin = s->active_query->begin;
        result.caret = s->active_query->caret;
    }
    return result;
}

bool MentionsComponentHost::editable(runtime::ComponentId id, bool composition) const {
    const auto* s = checked(*services_, id);
    if (!s || !s->shown || s->zero_geometry || !window_active_ || !services_->components().scope(id).active() ||
        !services_->interactions().component_active(id)) {
        return false;
    }
    const auto* record = services_->interactions().find(s->input_hit);
    const auto* editor = services_->input_runtime()->editors().find(s->editor);
    return record && record->eligible && services_->interactions().component_active(s->input) && editor &&
           !editor->disabled() && !editor->read_only() && (!composition || !editor->composition().active);
}

void MentionsComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = checked(*services_, id);
    if (!s || !s->private_ready || s->mounting) {
        return;
    }
    const auto generation = ++s->refresh_generation;
    auto empty_label = s->not_found;
    empty_label.set(s->loading || s->pending ? String{u8"Loading…"} : s->not_found_text);
    s = checked(*services_, id);
    if (!s || s->refresh_generation != generation) {
        return;
    }
    const auto current = [&]() -> State* {
        auto* live = checked(*services_, id);
        return live && live->refresh_generation == generation ? live : nullptr;
    };
    const bool visible = s->open && s->active_query && !s->caret_offscreen && !s->suppressed &&
                         (s->pending || s->loading || !s->model.filtered().empty() || s->empty_content.content() ||
                          !s->not_found.get().empty()) &&
                         editable(id, false);
    const bool changed = visible != s->visible;
    s->visible = visible;
    if (changed) {
        if (visible) {
            s->motion.begin(services_->components().theme_scope(id)->snapshot(), services_->motion_preference(),
                            services_->animation_time(), s->animate);
            if (s->motion.start) {
                services_->request_frame();
            }
        } else {
            s->motion.stop();
        }
    }
    if (s->empty_rich_root.valid() && (!visible || s->pending || s->loading || !s->model.filtered().empty())) {
        restore_row_focus(*services_, id, s->empty_rich_root, s->input_hit);
        s = current();
        if (!s) {
            return;
        }
    }
    services_->nodes().require(services_->components().root(s->popup)).content_opacity = s->motion.opacity;
    if (services_->components().set_branch_active(s->popup, visible)) {
        services_->mark_scene_structure_dirty();
    }
    if (s->empty_root.valid() && services_->components().set_branch_active(
                                     s->empty_root, (s->loading || s->pending || s->model.filtered().empty()))) {
        services_->mark_scene_structure_dirty();
    }
    const bool rich_empty = !s->pending && !s->loading && s->empty_content.content();
    for (const auto& [child, active] :
         std::array{std::pair{s->empty_label_root, !rich_empty}, std::pair{s->empty_rich_root, rich_empty}}) {
        if (child.valid()) {
            if (services_->components().set_branch_active(child, active)) {
                services_->mark_scene_structure_dirty();
            }
            services_->set_input_blocked(child, !visible || !active || !s->model.filtered().empty());
            s = current();
            if (!s) {
                return;
            }
        }
    }
    auto clear_visible = s->clear_visible;
    clear_visible.set(s->allow_clear && !s->clear_disabled.get() && !s->display.get().empty() && editable(id, false));
    s = current();
    if (!s) {
        return;
    }
    const auto t = services_->components().theme_scope(id)->snapshot().mentions();
    auto empty_color = s->empty_color;
    auto empty_font = s->empty_font;
    empty_color.set(foreground(t.colors.option_disabled_color));
    if (!current()) {
        return;
    }
    empty_font.set(
        {t.typography.font_family, t.typography.font_weight, false, t.typography.font_size, t.typography.line_height});
    s = current();
    if (!s) {
        return;
    }
    std::vector<std::pair<runtime::ComponentId, bool>> boundaries;
    const auto children = s->rows;
    for (auto child : children) {
        s = current();
        if (!s) {
            return;
        }
        auto* row = services_->components().state<Row>(child);
        if (!row) {
            continue;
        }
        const auto found = std::ranges::find(s->model.options(), row->key, &MentionsOption::key);
        if (found == s->model.options().end()) {
            if (services_->components().set_branch_active(child, false)) {
                services_->mark_scene_structure_dirty();
            }
            services_->interactions().set_eligible(row->interaction, false);
            boundaries.emplace_back(child, true);
            continue;
        }
        row->index = static_cast<std::size_t>(found - s->model.options().begin());
        const auto option = *found;
        const bool shown = !s->loading && !s->pending &&
                           std::ranges::find(s->model.filtered(), row->index) != s->model.filtered().end();
        const auto& node = services_->nodes().require(row->node);
        const bool on_screen =
            node.place_count == 0 || s->clip.height <= 0 || intersect(node.bounds, s->clip).height > 0;
        boundaries.emplace_back(child, !visible || !shown || !on_screen || option.disabled || option.group);
        if (services_->components().set_branch_active(child, shown)) {
            services_->mark_scene_structure_dirty();
        }
        services_->interactions().set_eligible(row->interaction, visible && shown && !option.disabled && !option.group);
        const bool selected = false;
        auto label = row->label;
        auto color = row->color;
        auto font = row->font;
        label.set(option.label.empty() ? option.value : option.label);
        if (!current()) {
            return;
        }
        color.set(foreground(option.disabled ? t.colors.option_disabled_color
                             : selected      ? t.colors.option_selected_color
                                             : t.colors.option_color));
        if (!current()) {
            return;
        }
        font.set({t.typography.font_family, selected ? t.typography.selected_font_weight : t.typography.font_weight,
                  false, t.typography.font_size, t.typography.line_height});
    }
    s = current();
    if (!s) {
        return;
    }
    if (geometry || changed) {
        s->needs_layout = true;
        s->reveal_active = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    } else {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    // This can synchronously blur a native child and destroy this entire control.
    const auto popup = s->popup;
    services_->set_input_blocked(popup, !visible);
    if (!current()) {
        return;
    }
    for (const auto& [child, blocked] : boundaries) {
        if (services_->components().contains(child)) {
            services_->set_input_blocked(child, blocked);
            if (!current()) {
                return;
            }
        }
    }
}

void MentionsComponentHost::request_open(runtime::ComponentId id, bool open, bool restore_focus) {
    auto* s = checked(*services_, id);
    if (!s || !services_->components().scope(id).active() || (open && (!editable(id) || !s->active_query))) {
        return;
    }
    RetireOnFailure rollback{*services_, id};
    const bool changed = s->open != open;
    const auto callback = s->on_open;
    const auto input = s->input_hit;
    if (!s->controlled_open) {
        s->open = open;
    }
    if (open) {
        s->suppressed = false;
    } else {
        const auto async = s->async;
        s->pending = false;
        async->cancel();
        if (!checked(*services_, id)) {
            return;
        }
    }
    refresh(id);
    if (changed && checked(*services_, id) && callback) {
        callback(open);
    }
    s = checked(*services_, id);
    if (restore_focus && s && s->input_hit == input && editable(id, false)) {
        services_->focus().defer_focus(input, input::FocusModality::keyboard);
    }
    rollback.complete = true;
}

void MentionsComponentHost::suppress(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s || s->suppressed) {
        return;
    }
    s->suppressed = true;
    const auto async = s->async;
    s->pending = false;
    async->cancel();
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    s->scroll = 0;
    if (!change_model(*services_, id,
                      [](auto& model) {
                          model.restore();
                          return true;
                      }) ||
        !display_model(*services_, id)) {
        return;
    }
    request_open(id, false);
}

void MentionsComponentHost::publish(runtime::ComponentId id, String value, std::optional<MentionsOption>, bool clear) {
    if (!editable(id)) {
        return;
    }
    auto* state = checked(*services_, id);
    if (!state) {
        return;
    }
    RetireOnException rollback{*services_, id};
    const auto previous = state->model.value();
    const auto callback = state->on_change;
    const auto on_clear = state->on_clear;
    const bool selecting = state->selecting;
    if (!change_model(*services_, id, [&](auto& model) {
            model.edit(value);
            return true;
        })) {
        return;
    }
    state = checked(*services_, id);
    if (!state) {
        return;
    }
    auto display = state->display;
    display.set(value);
    if (!checked(*services_, id)) {
        return;
    }
    if (previous != value && callback) {
        callback(value);
    }
    state = checked(*services_, id);
    if (!state) {
        return;
    }
    const auto authority = state->authority;
    if (authority) {
        auto accepted = read_prop(*authority);
        if (!change_model(*services_, id, [&](auto& model) { return model.reconcile(std::move(accepted)); })) {
            return;
        }
    }
    if (!display_model(*services_, id)) {
        return;
    }
    if (clear) {
        request_open(id, false);
        if (checked(*services_, id) && on_clear) {
            on_clear();
        }
    } else if (!selecting) {
        refresh_query(id, true);
    }
}

void MentionsComponentHost::refresh_query(runtime::ComponentId id, bool force) {
    auto* s = checked(*services_, id);
    if (!s || s->mounting || !s->private_ready || s->selecting) {
        return;
    }
    auto* editor = services_->input_runtime()->editors().find(s->editor);
    if (!editor) {
        return;
    }
    const auto text = String::from_utf8(editor->value()).value();
    const auto selection = editor->selection();
    const bool composing = editor->composition().active;
    const bool changed = !s->observed || text != s->observed_text || selection != s->observed_selection ||
                         composing != s->observed_composing;
    if (!changed && !force) {
        return;
    }
    s->observed = true;
    s->observed_text = text;
    s->observed_selection = selection;
    s->observed_composing = composing;
    if (s->reset_query) {
        s->reset_query = false;
        return;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (!record || !descendant(*services_, record->component, id) || !editable(id, false)) {
        return;
    }
    RetireOnException rollback{*services_, id};
    const auto generation = ++s->refresh_generation;
    const auto config = s->config;
    const auto validator = s->validator;
    auto query = composing ? std::optional<MentionQuery>{}
                           : mention_query(text.view(), selection.anchor, selection.caret, config);
    if (query && !validator.accepts(query->query.view(), config)) {
        query.reset();
    }
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    editor = services_->input_runtime()->editors().find(s->editor);
    if (s->refresh_generation != generation || s->config != config || !editor || editor->value() != text.bytes() ||
        editor->selection() != selection) {
        s->observed = false;
        services_->request_frame();
        return;
    }
    const bool reopen = force && !s->open;
    const bool new_query = query != s->active_query;
    s->active_query = query;
    if (!query) {
        request_open(id, false);
        return;
    }
    if (!new_query && !force) {
        return;
    }
    if (!change_model(*services_, id, [&](auto& model) {
            model.search(query->query);
            return true;
        })) {
        return;
    }
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto load_revision = s->load_revision;
    const auto on_search = s->on_search;
    if ((new_query || reopen) && on_search) {
        on_search(query->query, query->prefix);
    }
    s = checked(*services_, id);
    // onSearch may synchronously replace options; the editor/query, rather than an option revision, owns this intent.
    if (!s) {
        return;
    }
    editor = services_->input_runtime()->editors().find(s->editor);
    if (s->active_query != query || s->config != config || !editor || editor->value() != text.bytes() ||
        editor->selection() != selection) {
        s->observed = false;
        request_open(id, false);
        services_->request_frame();
        return;
    }
    reconcile_rows(id);
    request_open(id, true);
    s = checked(*services_, id);
    if (s && s->active_query == query && (new_query || reopen) && s->load_revision == load_revision) {
        load(id, query->query);
    }
    refresh(id);
}

void MentionsComponentHost::choose(runtime::ComponentId id, std::size_t index) {
    auto* s = checked(*services_, id);
    if (!s || !s->visible || s->pending || s->loading || !s->active_query || !editable(id) ||
        index >= s->model.options().size() || s->model.options()[index].disabled || s->model.options()[index].group ||
        std::ranges::find(s->model.filtered(), index) == s->model.filtered().end()) {
        return;
    }
    RetireOnException rollback{*services_, id};
    const auto key = s->model.options()[index].key;
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (record && descendant(*services_, record->component, s->popup)) {
        services_->focus().defer_focus(s->input_hit, input::FocusModality::keyboard);
    }
    s = checked(*services_, id);
    if (!s || !s->active_query || !editable(id)) {
        return;
    }
    const auto found = std::ranges::find(s->model.options(), key, &MentionsOption::key);
    if (found == s->model.options().end() || found->disabled || found->group) {
        return;
    }
    const auto option = *found;
    const auto query = *s->active_query;
    auto* editor = services_->input_runtime()->editors().find(s->editor);
    if (!editor || editor->selection() != input::TextSelection{query.caret, query.caret} ||
        mention_query(String::from_utf8(editor->value()).value().view(), query.caret, query.caret, s->config) !=
            s->active_query) {
        return;
    }
    const auto callback = s->on_select;
    const auto input = s->input;
    const auto input_hit = s->input_hit;
    const auto prefix = query.prefix;
    const auto split = s->config.split;
    const auto document = String::from_utf8(editor->value()).value();
    const auto replacement = mention_replacement(document.view(), query, option.value.view(), split.view());
    s->selecting = true;
    const auto result =
        services_->input_runtime()->replace_range(input, {replacement.begin, replacement.end}, replacement.text.view());
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    s->selecting = false;
    s->active_query.reset();
    s->reset_query = true;
    request_open(id, false);
    if (checked(*services_, id) && result) {
        services_->focus().defer_focus(input_hit, input::FocusModality::keyboard);
        if (callback) {
            callback(option, prefix);
        }
    }
}

void MentionsComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    RetireOnException failure{*services_, id};
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& t = theme.mentions();
    auto config = std::make_unique<ThemeConfig>();
    config->seed = t.input_seed;
    config->alias = t.input_alias;
    config->input.tokens = t.input;
    config->text.tokens.font_family = t.typography.font_family;
    config->text.tokens.font_weight = t.typography.font_weight;
    auto input_theme = s->input_theme;
    input_theme.set(std::move(*config));
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    if (services_->components().set_window_layer(s->popup, t.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
}

void MentionsComponentHost::mount_label(runtime::ComponentId id, runtime::ComponentId child) {
    if (!checked(*services_, id)) {
        return;
    }
    auto* row = services_->components().state<Row>(child);
    if (!row) {
        return;
    }
    RetireOnFailure rollback{*services_, id};
    auto& build = runtime::require_component_build_context();
    row->label_root = group(*services_);
    const auto label_root = row->label_root;
    remember_region(*services_, id, label_root);
    build.on_resource_cleanup(label_root, [services = services_, label_root] {
        services->interactions().set_input_blocked(label_root, false);
    });
    const auto rich = row->rich;
    const auto label_signal = row->label;
    const auto color = row->color;
    const auto font = row->font;
    const auto direction = checked(*services_, id)->direction;
    build.mount_slot_with_semantic_text_style(label_root, Content{[rich, label_signal, direction] {
                                                  if (const auto* label = rich.content()) {
                                                      SlotContentAccess::function (*label)();
                                                  } else {
                                                      Text(TextProps{}.content(label_signal).direction(direction));
                                                  }
                                              }},
                                              color, font);
    if (!checked(*services_, id)) {
        return;
    }
    // Rich native children remain pointer operable, while the compound control has one Tab entry.
    for (auto hit : services_->interactions().declaration_order()) {
        const auto* r = services_->interactions().find(hit);
        if (r && descendant(*services_, r->component, child)) {
            services_->interactions().set_tab_stop(hit, false);
        }
    }
    rollback.complete = true;
}

void MentionsComponentHost::mount_row(runtime::ComponentId id, std::size_t index) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    auto* live = checked(services, id);
    if (!live || index >= live->model.options().size()) {
        return;
    }
    const auto option = live->model.options()[index];
    const auto popup_hit = live->popup_hit;
    const auto child = build.mount_component<Row>();
    auto& row = build.state<Row>(child);
    row.node = build.root(child);
    row.index = index;
    row.key = option.key;
    row.rich = option.rich_label;
    services.layout().set_layout(row.node, layout::BoxLayout{});
    services.nodes().require(row.node).clip_content = true;
    row.interaction = services.interactions().create({child, row.node, popup_hit, false, false, {}, false, false});
    input::InteractionHandlers handlers;
    handlers.target = [this, id, child](input::PointerDispatchContext& e) {
        const auto* s = checked(*services_, id);
        const auto* row = services_->components().state<Row>(child);
        if (!s || !row || !s->visible || !editable(id)) {
            return;
        }
        const auto index = row->index;
        if (e.kind() == input::PointerEventKind::enter || e.kind() == input::PointerEventKind::move) {
            auto* live = checked(*services_, id);
            if (live->model.activate(index)) {
                refresh(id, false);
            }
            return;
        }
        if (e.event().button != input::PointerButton::primary) {
            return;
        }
        if (e.kind() == input::PointerEventKind::down) {
            (void)e.capture_pointer();
            e.stop_propagation();
        }
        if (e.kind() == input::PointerEventKind::up) {
            (void)e.release_pointer_capture();
            const bool hit =
                e.press_origin() == row->interaction &&
                inside(intersect(services_->nodes().require(row->node).bounds, s->clip), e.event().x, e.event().y);
            e.stop_propagation();
            if (hit) {
                choose(id, index);
            }
        }
    };
    services.interactions().set_handlers(row.interaction, std::move(handlers));
    row.hit = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(row.hit, {}, row.interaction);
    const auto body = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
    graphics::QuadInstance initial;
    initial.opacity = 0;
    component::RetainedSurfaceEffects effects;
    effects.focus_enabled = false;
    row.surface = services.surfaces().create_surface(child, row.node, body, std::span{&initial, 1}, effects);
    build.on_resource_cleanup(
        child, [&services, child, node = row.node, hit = row.interaction, fragment = row.hit, surface = row.surface] {
            services.interactions().set_input_blocked(child, false);
            services.pointer().cancel_interaction(hit);
            services.interactions().remove(hit);
            services.surfaces().destroy(surface);
            services.scene_composer().remove_fragment(fragment);
            services.layout().remove_layout(node);
        });
    live = checked(services, id);
    if (!live) {
        return;
    }
    live->rows.push_back(child);
    remember_region(services, id, child);
    build.mount_slot(child, Content{[this, id, child] { mount_label(id, child); }});
    (void)checked(services, id);
}

void MentionsComponentHost::reconcile_rows(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    if (s->mounting || s->reconciling) {
        s->rows_dirty = true;
        return;
    }
    RetireOnFailure rollback{*services_, id};
    s->reconciling = true;
    do {
        s->rows_dirty = false;
        const auto revision = s->model_revision;
        const auto options = s->model.options();
        auto previous = s->rows;
        previous.insert(previous.end(), s->parked.begin(), s->parked.end());
        s->parked.clear();
        std::vector<std::size_t> desired;
        if (!s->virtual_list || !s->match_width || options.size() <= 32) {
            for (std::size_t i = 0; i < options.size(); ++i) {
                desired.push_back(i);
            }
        } else {
            const auto& metrics = services_->components().theme_scope(id)->snapshot().mentions().metrics;
            const auto filtered = s->model.filtered();
            const auto range =
                windowed_range(filtered.size(), s->scroll, s->clip.height > 0 ? s->clip.height : s->list_height,
                               metrics.option_height, true);
            for (auto p = range.first; p < range.end; ++p) {
                desired.push_back(filtered[p]);
            }
        }
        std::vector<runtime::ComponentId> next;
        bool restart = false;
        for (auto i : desired) {
            const auto option = options[i];
            const auto found = std::ranges::find_if(previous, [&](auto child) {
                const auto* row = services_->components().state<Row>(child);
                return row && row->key == option.key;
            });
            if (found == previous.end()) {
                mount_content(*services_, s->list, Content{[this, id, i] { mount_row(id, i); }}, mount_depth_ != 0);
                s = checked(*services_, id);
                if (!s) {
                    return;
                }
                next.push_back(s->rows.back());
            } else {
                const auto child = *found;
                auto* row = services_->components().state<Row>(child);
                row->index = i;
                if (row->rich != option.rich_label) {
                    const auto label = row->label_root;
                    row->rich = option.rich_label;
                    row->label_root = {};
                    forget_region(*services_, id, label);
                    services_->set_input_blocked(label, true);
                    if (!checked(*services_, id)) {
                        return;
                    }
                    services_->destroy(label);
                    if (!checked(*services_, id)) {
                        return;
                    }
                    mount_content(*services_, child, Content{[this, id, child] { mount_label(id, child); }},
                                  mount_depth_ != 0);
                    s = checked(*services_, id);
                    if (!s) {
                        return;
                    }
                }
                next.push_back(child);
                previous.erase(found);
            }
            s = checked(*services_, id);
            if (!s) {
                return;
            }
            if (s->model_revision != revision) {
                restart = true;
                break;
            }
        }
        if (restart) {
            s->rows_dirty = true;
            continue;
        }
        s->rows = std::move(next);
        for (auto child : previous) {
            auto* row = services_->components().state<Row>(child);
            if (row && row->rich.content() &&
                std::ranges::find(options, row->key, &MentionsOption::key) != options.end()) {
                s->parked.push_back(child);
                restore_row_focus(*services_, id, child, s->input_hit);
                s = checked(*services_, id);
                if (!s) {
                    return;
                }
                row = services_->components().state<Row>(child);
                if (!row) {
                    return;
                }
                services_->components().set_branch_active(child, false);
                services_->interactions().set_eligible(row->interaction, false);
                services_->set_input_blocked(child, true);
                if (!checked(*services_, id)) {
                    return;
                }
                continue;
            }
            forget_region(*services_, id, child);
            services_->set_input_blocked(child, true);
            if (!checked(*services_, id)) {
                return;
            }
            services_->destroy(child);
            if (!checked(*services_, id)) {
                return;
            }
        }
        refresh(id);
        s = checked(*services_, id);
        if (!s) {
            return;
        }
    } while (s->rows_dirty);
    s->reconciling = false;
    rollback.complete = true;
}

void MentionsComponentHost::mount_popup(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    auto& build = runtime::require_component_build_context();
    s->popup_body = group(*services_);
    remember_region(*services_, id, s->popup_body);
    build.mount_slot(
        s->popup_body, Content{[this, id] {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            const auto render = s->popup_render;
            render([this, id] {
                auto* s = checked(*services_, id);
                if (!s) {
                    return;
                }
                if (s->list.valid()) {
                    throw std::invalid_argument("Mentions popupRender must mount origin exactly once");
                }
                s->list = group(*services_, true);
                remember_region(*services_, id, s->list);
                services_->nodes().require(services_->components().root(s->list)).clip_content = true;
                runtime::require_component_build_context().mount_slot(
                    s->list, Content{[this, id] {
                        auto* s = checked(*services_, id);
                        if (!s) {
                            return;
                        }
                        s->empty_root = group(*services_);
                        remember_region(*services_, id, s->empty_root);
                        runtime::require_component_build_context().mount_slot(
                            s->empty_root, Content{[this, id] {
                                auto* s = checked(*services_, id);
                                if (!s) {
                                    return;
                                }
                                s->empty_label_root = group(*services_);
                                remember_region(*services_, id, s->empty_label_root);
                                runtime::require_component_build_context().mount_slot_with_semantic_text_style(
                                    s->empty_label_root, Content{[label = s->not_found, direction = s->direction] {
                                        Text(TextProps{}.content(label).direction(direction));
                                    }},
                                    s->empty_color, s->empty_font);
                                s->empty_rich_root = group(*services_);
                                remember_region(*services_, id, s->empty_rich_root);
                                runtime::require_component_build_context().mount_slot(
                                    s->empty_rich_root, Content{[this, id] { mount_empty_content(id); }});
                            }});
                    }});
            });
            if (const auto* live = checked(*services_, id); live && !live->list.valid()) {
                throw std::invalid_argument("Mentions popupRender must mount origin exactly once");
            }
        }});
}

void MentionsComponentHost::mount_empty_content(runtime::ComponentId id) {
    const auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto render = s->empty_content;
    if (const auto* content = render.content()) {
        SlotContentAccess::function (*content)();
    }
}

void MentionsComponentHost::rebuild_empty_content(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s || s->mounting || !s->empty_rich_root.valid()) {
        return;
    }
    RetireOnFailure rollback{*services_, id};
    const auto root = s->empty_rich_root;
    const auto span = services_->components().children(root);
    const std::vector<runtime::ComponentId> children{span.begin(), span.end()};
    for (auto child : children) {
        restore_row_focus(*services_, id, child, s->input_hit);
        if (!checked(*services_, id)) {
            return;
        }
        services_->components().scope(child).on_cleanup(
            [services = services_, child] { services->interactions().set_input_blocked(child, false); });
        services_->set_input_blocked(child, true);
        services_->destroy(child);
        if (!checked(*services_, id)) {
            return;
        }
    }
    services_->append_slot(root, Content{[this, id] { mount_empty_content(id); }});
    refresh(id);
    rollback.complete = true;
}

void MentionsComponentHost::rebuild_popup(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s || s->mounting) {
        return;
    }
    RetireOnFailure rollback{*services_, id};
    const auto body = s->popup_body;
    const auto popup = s->popup;
    // A changed popup renderer retires exactly its owned popup body, including the origin.
    // The editor and its IME/draft ownership remain untouched.
    s->private_ready = false;
    forget_region(*services_, id, body);
    s->rows.clear();
    s->parked.clear();
    s->empty_root = {};
    s->empty_label_root = {};
    s->empty_rich_root = {};
    s->list = {};
    s->popup_body = {};
    services_->set_input_blocked(body, true);
    services_->destroy(body);
    if (!checked(*services_, id)) {
        return;
    }
    services_->append_slot(popup, Content{[this, id] { mount_popup(id); }});
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    s->private_ready = true;
    reconcile_rows(id);
    refresh(id);
    rollback.complete = true;
}

void MentionsComponentHost::mount_editor(runtime::ComponentId id) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    auto* s = checked(services, id);
    if (!s) {
        return;
    }
    const auto config = s->input_theme;
    Theme(ThemeProps{}.config(config), ThemeContent{[&, id] {
              FormPrivateEditorScope private_editor;
              auto* state = checked(services, id);
              if (!state) {
                  return;
              }
              const auto count = inputs->mounted_inputs().size();
              auto text_area = state->textarea_props;
              text_area.value(state->display)
                  .placeholder(state->placeholder)
                  .disabled(state->disabled)
                  .readOnly(state->read_only)
                  .direction(state->direction)
                  .size(state->size)
                  .variant(state->variant)
                  .status(state->status)
                  .ref(state->reference)
                  .autoFocus(state->auto_focus)
                  .clearIcon(state->clear_icon)
                  .clearDisabled(state->clear_disabled)
                  .allowClear(state->clear_visible)
                  .onChange([this, id](String value) { publish(id, std::move(value), {}); })
                  .onClear([this, id] { publish(id, {}, {}, true); })
                  .onFocus([this, id] {
                      auto* s = checked(*services_, id);
                      if (!s) {
                          return;
                      }
                      RetireOnException rollback{*services_, id};
                      if (!s->active_query || s->open) {
                          refresh_query(id, true);
                      }
                      s = checked(*services_, id);
                      if (!s) {
                          return;
                      }
                      const auto callback = s->on_focus;
                      if (callback) {
                          callback();
                      }
                  })
                  .onBlur([this, id] {
                      auto* s = checked(*services_, id);
                      if (!s || !services_->components().scope(id).active()) {
                          return;
                      }
                      RetireOnException rollback{*services_, id};
                      const auto callback = s->on_blur;
                      const auto focus = services_->focus().state().focused;
                      const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
                      if (!record || !descendant(*services_, record->component, s->popup)) {
                          s->closed_on_blur = true;
                          request_open(id, false);
                      }
                      if (checked(*services_, id) && callback) {
                          callback();
                      }
                  });
              if (state->max_length) {
                  text_area.maxLength(*state->max_length);
              }
              const auto render = state->editor_render;
              render({std::move(text_area)});
              state = checked(services, id);
              if (!state) {
                  return;
              }
              if (inputs->mounted_inputs().size() != count + 1 || !state->reference.bound()) {
                  throw std::invalid_argument("Mentions editor must mount exactly one bound native TextArea");
              }
              const auto mounted = inputs->mounted_inputs()[count];
              if (inputs->editors().require(mounted.editor).mode() != input::TextEditMode::MultiLine) {
                  throw std::invalid_argument("Mentions editor must preserve a native multiline TextArea");
              }
              state->input = mounted.component;
              state->input_node = mounted.node;
              state->input_hit = mounted.interaction;
              state->editor = mounted.editor;
              remember_region(services, id, state->input);
              auto& native_state = *state;
              inputs->configure_form_reset(native_state.input, [this, id, editor = native_state.editor] {
                  RetireOnException failure{*services_, id};
                  auto* s = checked(*services_, id);
                  if (!s || s->editor != editor || !services_->components().scope(id).active()) {
                      return;
                  }
                  const auto async = s->async;
                  s->pending = false;
                  async->cancel();
                  s = checked(*services_, id);
                  if (!s) {
                      return;
                  }
                  const auto authority = s->authority;
                  if (authority) {
                      auto accepted = read_prop(*authority);
                      if (!change_model(*services_, id,
                                        [&](auto& model) { return model.reconcile(std::move(accepted)); })) {
                          return;
                      }
                  }
                  if (!change_model(*services_, id,
                                    [](auto& model) {
                                        model.restore();
                                        return true;
                                    }) ||
                      !display_model(*services_, id)) {
                      return;
                  }
                  s = checked(*services_, id);
                  if (!s || s->editor != editor) {
                      return;
                  }
                  s->scroll = 0;
                  s->active_query.reset();
                  s->reset_query = true;
                  s->observed = false;
                  s->open = false;
                  s->suppressed = true;
                  refresh(id);
              });
          }});
}

void MentionsComponentHost::load(runtime::ComponentId id, String query) {
    auto* s = checked(*services_, id);
    if (!s || !s->source || !s->open || !s->active_query || !editable(id, false)) {
        return;
    }
    const auto source = s->source;
    const auto async = s->async;
    ++s->load_revision;
    s->pending = true;
    refresh(id);
    RetireOnFailure rollback{*services_, id};
    async->start(source, std::move(query), [this, id](AsyncOptionsResult<MentionsOption> result) {
        auto* s = checked(*services_, id);
        if (!s || !editable(id, false) || s->suppressed) {
            return;
        }
        RetireOnFailure rollback{*services_, id};
        s->pending = false;
        if (!result.error.empty()) {
            const auto callback = s->on_load_error;
            if (callback) {
                callback(std::move(result.error));
            }
        } else {
            if (change_model(*services_, id,
                             [&](auto& model) { return model.set_options(std::move(result.options)); })) {
                reconcile_rows(id);
                refresh(id);
            }
        }
        if (checked(*services_, id)) {
            services_->request_frame();
        }
        rollback.complete = true;
    });
    rollback.complete = true;
}

void MentionsComponentHost::rebuild_editor(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s || s->mounting) {
        return;
    }
    RetireOnFailure rollback{*services_, id};
    const auto async = s->async;
    s->pending = false;
    async->cancel();
    if (!change_model(*services_, id,
                      [](auto& model) {
                          model.restore();
                          return true;
                      }) ||
        !display_model(*services_, id)) {
        return;
    }
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto root = s->editor_root;
    const bool focused = services_->focus().state().focused == s->input_hit;
    const auto children_span = services_->components().children(root);
    const std::vector<runtime::ComponentId> children{children_span.begin(), children_span.end()};
    for (auto child : std::vector<runtime::ComponentId>{children.begin(), children.end()}) {
        forget_region(*services_, id, child);
        services_->components().scope(child).on_cleanup(
            [services = services_, child] { services->interactions().set_input_blocked(child, false); });
        services_->set_input_blocked(child, true);
        if (!checked(*services_, id)) {
            return;
        }
    }
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    s->private_ready = false;
    s->input = {};
    s->input_node = {};
    s->input_hit = {};
    s->editor = {};
    for (auto child : std::vector<runtime::ComponentId>{children.begin(), children.end()}) {
        services_->destroy(child);
    }
    services_->append_slot(root, Content{[this, id] { mount_editor(id); }});
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    s->private_ready = true;
    if (focused) {
        services_->focus().defer_focus(s->input_hit, input::FocusModality::keyboard);
    }
    update_theme(id, true);
    rollback.complete = true;
}

void MentionsComponentHost::mount(const MentionsProps& props) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("Mentions requires window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("Mentions controlled/default props conflict");
    }
    const auto editor_render = read_prop(props.editor_);
    const auto popup_render = read_prop(props.popup_render_);
    const auto empty_content = read_prop(props.empty_content_);
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<State>(
        MentionsModel{{}, props.default_value_.value_or(String{}), props.value_.has_value(), MentionsFilter{}, true});
    RetireOnFailure rollback{services, id};
    auto& s = build.state<State>(id);
    s.node = build.root(id);
    const auto node = s.node;
    // connect_binding owns an observer only after its first synchronous read/apply.
    // Keep this Scope alive even if that read or apply retires the component record.
    const auto bindings = s.bindings;
    build.on_resource_cleanup(id, [&services, node, bindings] {
        bindings->dispose();
        services.layout().remove_layout(node);
    });
    build.on_resource_cleanup(id, [&services, id] { services.interactions().set_input_blocked(id, false); });
    s.reference = props.ref_.value_or(MentionsRef{});
    s.textarea_props.rows(props.rows_)
        .autoSize(props.auto_size_)
        .wrap(props.wrap_)
        .resize(props.resize_)
        .onResize([this, id, callback = props.on_resize_](TextAreaSize size) {
            RetireOnException rollback{*services_, id};
            if (checked(*services_, id) && callback) {
                callback(size);
            }
        });
    s.max_length = props.max_length_;
    s.auto_focus = props.auto_focus_;
    s.editor_render = editor_render;
    s.popup_render = popup_render;
    s.empty_content = empty_content;
    s.on_load_error = props.on_load_error_;
    s.on_scroll = props.on_scroll_;
    const auto async = s.async;
    build.on_resource_cleanup(id, [async] { async->cancel(); });
    s.authority = props.value_;
    s.open = props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.on_change = props.on_change_;
    s.on_search = props.on_search_;
    s.on_select = props.on_select_;
    s.on_clear = props.on_clear_;
    s.on_open = props.on_open_;
    s.on_focus = props.on_focus_;
    s.on_blur = props.on_blur_;
    s.display.set(s.model.display());
    runtime::connect_layout_style(*bindings, props.layout_, node, services.nodes(), services.dirty(),
                                  [&services, id] { return checked(services, id) != nullptr; });
    if (!checked(services, id)) {
        return;
    }
    build.mount_slot(
        id, Content{[&] {
            s.editor_root = group(services);
            remember_region(services, id, s.editor_root);
            build.mount_slot(s.editor_root, Content{[this, id] { mount_editor(id); }});
            if (!checked(services, id)) {
                return;
            }
            s.popup = group(services, true);
            remember_region(services, id, s.popup);
            const auto popup_node = build.root(s.popup);
            services.nodes().require(popup_node).clip_content = true;
            services.components().set_window_layer(s.popup, 1050);
            s.popup_hit = services.interactions().create({s.popup, popup_node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(s.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.background =
                services.surfaces().create(s.popup, popup_node, fragment, s.popup_hit, std::span{&initial, 1}, effects);
            build.on_resource_cleanup(s.popup, [&services, popup = s.popup, hit = s.popup_hit, surface = s.background] {
                services.interactions().set_input_blocked(popup, false);
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            build.mount_slot(s.popup, Content{[this, id] { mount_popup(id); }});
        }});
    if (!checked(services, id)) {
        return;
    }
    s.private_ready = true;
    services.layout().set_layout(
        node, layout::ComponentLayout{
                  [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                      const auto* live = checked(*services_, id);
                      return live ? engine.measure_child(services_->components().root(live->editor_root), limits)
                                  : runtime::Size{};
                  },
                  [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                      if (const auto* live = checked(*services_, id)) {
                          engine.place_child(services_->components().root(live->editor_root), rect);
                      }
                  }});
    mounted_.push_back(id);
    auto& scope = *bindings;
    const auto connect = [&](const auto& prop, auto apply) {
        if (!checked(services, id)) {
            return false;
        }
        connect_prop(scope, prop, [&services, id, apply = std::move(apply)](auto value) {
            RetireOnFailure rollback{services, id};
            if (checked(services, id)) {
                apply(std::move(value));
            }
            (void)checked(services, id);
            rollback.complete = true;
        });
        return checked(services, id) != nullptr;
    };
    if (!connect(props.prefixes_,
                 [this, id](std::vector<String> value) {
                     if (auto* s = checked(*services_, id)) {
                         auto config = s->config;
                         config.prefix = std::move(value);
                         validate_mentions_config(config);
                         s->config = std::move(config);
                         s->observed = false;
                         services_->request_frame();
                     }
                 }) ||
        !connect(props.split_,
                 [this, id](String value) {
                     if (auto* s = checked(*services_, id)) {
                         s->config.split = std::move(value);
                         s->observed = false;
                         services_->request_frame();
                     }
                 }) ||
        !connect(props.validate_search_,
                 [this, id](MentionsSearchValidator value) {
                     if (auto* s = checked(*services_, id)) {
                         s->validator = std::move(value);
                         s->observed = false;
                         services_->request_frame();
                     }
                 }) ||
        !connect(props.placement_,
                 [this, id](MentionsPlacement value) {
                     if (value != MentionsPlacement::Top && value != MentionsPlacement::Bottom) {
                         throw std::invalid_argument("Invalid Mentions placement");
                     }
                     if (auto* s = checked(*services_, id)) {
                         s->placement = value;
                         refresh(id);
                     }
                 }) ||
        !connect(props.clear_disabled_,
                 [this, id](bool value) {
                     if (auto* s = checked(*services_, id)) {
                         auto signal = s->clear_disabled;
                         signal.set(value);
                         refresh(id, false);
                     }
                 }) ||
        !connect(props.visible_,
                 [this, id](bool value) {
                     if (auto* s = checked(*services_, id)) {
                         s->shown = value;
                         if (!value) {
                             suppress(id);
                         }
                         if (!checked(*services_, id)) {
                             return;
                         }
                         if (services_->components().set_branch_active(id, value)) {
                             services_->mark_scene_structure_dirty();
                         }
                         services_->set_input_blocked(id, !value);
                         refresh(id);
                     }
                 }) ||
        !connect(props.loading_, [this, id](bool value) {
            if (auto* s = checked(*services_, id)) {
                s->loading = value;
                refresh(id);
            }
        })) {
        return;
    }
    if (props.single_prefix_ && !connect(*props.single_prefix_, [this, id](String value) {
            if (auto* s = checked(*services_, id)) {
                auto config = s->config;
                config.prefix = {std::move(value)};
                validate_mentions_config(config);
                s->config = std::move(config);
                s->observed = false;
                services_->request_frame();
            }
        })) {
        return;
    }
    if (!connect(props.empty_content_, [this, id](MentionsRichLabel value) {
            if (auto* s = checked(*services_, id); s && s->empty_content != value) {
                s->empty_content = std::move(value);
                rebuild_empty_content(id);
            }
        })) {
        return;
    }
    if (!connect(props.clear_icon_,
                 [this, id](IconSource icon) {
                     if (const auto* s = checked(*services_, id)) {
                         auto signal = s->clear_icon;
                         signal.set(std::move(icon));
                     }
                 }) ||
        !connect(props.not_found_, [this, id](String text) {
            if (auto* s = checked(*services_, id)) {
                s->not_found_text = text;
                auto signal = s->not_found;
                signal.set(std::move(text));
                refresh(id);
            }
        })) {
        return;
    }
    const auto simple = [&](const auto& prop, auto member) {
        return connect(prop, [this, id, member](auto value) {
            if (auto* s = checked(*services_, id)) {
                s->*member = std::move(value);
                refresh(id);
            }
        });
    };
    if (!simple(props.virtual_, &State::virtual_list) || !simple(props.motion_, &State::animate) ||
        !simple(props.match_width_, &State::match_width)) {
        return;
    }
    if (!connect(props.popup_width_, [this, id](float width) {
            if (!std::isfinite(width) || width < 0) {
                throw std::invalid_argument("Mentions popupWidth must be finite and nonnegative");
            }
            if (auto* s = checked(*services_, id)) {
                s->popup_width = width;
                refresh(id);
            }
        })) {
        return;
    }
    if (!connect(props.direction_, [this, id](TextDirection direction) {
            if (direction != TextDirection::Auto && direction != TextDirection::LeftToRight &&
                direction != TextDirection::RightToLeft) {
                throw std::invalid_argument("Invalid Mentions direction");
            }
            if (auto* s = checked(*services_, id)) {
                auto signal = s->direction;
                signal.set(direction);
                refresh(id);
            }
        })) {
        return;
    }
    if (!connect(props.editor_, [this, id](MentionsEditor render) {
            if (auto* s = checked(*services_, id); s && s->editor_render != render) {
                s->editor_render = std::move(render);
                rebuild_editor(id);
            }
        })) {
        return;
    }
    if (!connect(props.popup_render_, [this, id](MentionsPopupRenderer render) {
            if (auto* s = checked(*services_, id); s && s->popup_render != render) {
                s->popup_render = std::move(render);
                rebuild_popup(id);
            }
        })) {
        return;
    }
    if (!connect(props.source_, [this, id](MentionsSource source) {
            if (auto* s = checked(*services_, id); s && s->source != source) {
                auto async = s->async;
                s->source = source;
                const auto query = s->active_query;
                s->pending = false;
                async->cancel();
                s = checked(*services_, id);
                if (s && !s->mounting && s->source == source && s->active_query == query && query) {
                    load(id, query->query);
                }
                refresh(id);
            }
        })) {
        return;
    }
    if (props.groups_ && !connect(*props.groups_, [this, id](std::vector<MentionsOptionGroup> groups) {
            std::vector<MentionsOption> options;
            for (auto& group : groups) {
                options.push_back({group.key, {}, group.label, true, group.rich_label, true});
                options.insert(options.end(), group.options.begin(), group.options.end());
            }
            if (change_model(*services_, id, [&](auto& model) { return model.set_options(std::move(options)); })) {
                reconcile_rows(id);
            }
        })) {
        return;
    }
    if (!props.groups_ && !connect(props.options_, [this, id](std::vector<MentionsOption> value) {
            const auto changed =
                change_model(*services_, id, [&](auto& model) { return model.set_options(std::move(value)); });
            if (changed && *changed && display_model(*services_, id)) {
                reconcile_rows(id);
            }
        })) {
        return;
    }
    if (props.value_) {
        if (!connect(*props.value_, [this, id](String value) {
                const auto changed =
                    change_model(*services_, id, [&](auto& model) { return model.reconcile(std::move(value)); });
                if (changed && *changed && display_model(*services_, id)) {
                    if (auto* s = checked(*services_, id)) {
                        s->observed = false;
                    }
                    refresh(id);
                }
            })) {
            return;
        }
    }
    if (props.open_) {
        if (!connect(*props.open_, [this, id](bool open) {
                auto* s = checked(*services_, id);
                if (!s) {
                    return;
                }
                const bool changed = s->open != open;
                s->open = open;
                if (changed && open) {
                    s->suppressed = false;
                }
                if (!open) {
                    auto async = s->async;
                    s->pending = false;
                    async->cancel();
                    if (!checked(*services_, id)) {
                        return;
                    }
                }
                refresh(id);
            })) {
            return;
        }
    }
    if (!connect(props.filter_, [this, id](MentionsFilter filter) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            const bool active_first = s->active_first;
            const auto guarded = guarded_filter(*services_, id, filter);
            if (!change_model(*services_, id, [&](auto& model) {
                    model.configure(guarded, active_first);
                    return true;
                })) {
                return;
            }
            s = checked(*services_, id);
            if (!s) {
                return;
            }
            s->filter = std::move(filter);
            if (display_model(*services_, id)) {
                refresh(id);
            }
        })) {
        return;
    }
    if (!connect(props.active_first_, [this, id](bool value) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            const auto filter = guarded_filter(*services_, id, s->filter);
            if (!change_model(*services_, id, [&](auto& model) {
                    model.configure(filter, value);
                    return true;
                })) {
                return;
            }
            s = checked(*services_, id);
            if (!s) {
                return;
            }
            s->active_first = value;
            if (display_model(*services_, id)) {
                refresh(id);
            }
        })) {
        return;
    }
    if (!connect(props.allow_clear_, [this, id](bool value) {
            if (auto* s = checked(*services_, id)) {
                s->allow_clear = value;
                refresh(id, false);
            }
        })) {
        return;
    }
    if (!connect(props.disabled_, [this, id](bool value) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            auto signal = s->disabled;
            signal.set(value);
            if (!checked(*services_, id)) {
                return;
            }
            if (value) {
                suppress(id);
            }
            refresh(id);
        })) {
        return;
    }
    if (!connect(props.read_only_, [this, id](bool value) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            auto signal = s->read_only;
            signal.set(value);
            if (!checked(*services_, id)) {
                return;
            }
            if (value) {
                suppress(id);
            }
            refresh(id);
        })) {
        return;
    }
    if (!connect(props.list_height_, [this, id](float value) {
            validate_height(value);
            if (auto* s = checked(*services_, id)) {
                s->list_height = value;
                refresh(id);
            }
        })) {
        return;
    }
    const auto mirror = [&](const auto& prop, auto member) {
        return connect(prop, [&services, id, member](auto value) {
            if (const auto* state = checked(services, id)) {
                auto signal = state->*member;
                signal.set(std::move(value));
            }
        });
    };
    if (!mirror(props.placeholder_, &State::placeholder) || !mirror(props.size_, &State::size) ||
        !mirror(props.variant_, &State::variant) || !mirror(props.status_, &State::status)) {
        return;
    }
    update_theme(id, true);
    if (!checked(services, id)) {
        return;
    }
    const auto theme = services.components().theme_scope(id);
    const std::array accessors{
        &theme_runtime::ThemeScope::mentions_colors,     &theme_runtime::ThemeScope::mentions_metrics,
        &theme_runtime::ThemeScope::mentions_typography, &theme_runtime::ThemeScope::mentions_effects,
        &theme_runtime::ThemeScope::mentions_order,      &theme_runtime::ThemeScope::mentions_input};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        auto subscription = theme->capture([this, id, geometry = i == 1 || i == 2 || i == 5](
                                               theme_runtime::DirtyPhase) { update_theme(id, geometry); },
                                           [theme, accessor = accessors[i], shadow = i == 0] {
                                               (void)(theme.get()->*accessor)();
                                               (void)shadow;
                                           });
        auto* live = checked(services, id);
        if (!live) {
            return;
        }
        live->subscriptions[i] = std::move(subscription);
    }
    auto* live = checked(services, id);
    if (!live) {
        return;
    }
    live->mounting = false;
    reconcile_rows(id);
    rollback.complete = checked(services, id) != nullptr;
}

void MentionsComponentHost::on_window_active(bool value) {
    window_active_ = value;
    on_input_boundary_changed();
}

void MentionsComponentHost::on_input_boundary_changed() {
    if (mount_depth_) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = checked(*services_, id);
        if (s && s->has_been_active && !editable(id, false)) {
            suppress(id);
        }
        refresh(id, false);
    }
}

void MentionsComponentHost::prepare_auxiliary_layout() {
    // Also reconcile focus cancellation suppressed while an exception was unwinding.
    on_focus_changed();
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = checked(*services_, id);
        if (!s) {
            continue;
        }
        refresh_query(id);
        s = checked(*services_, id);
        if (!s) {
            continue;
        }
        s->closed_on_blur = false;
        const bool active = editable(id, false);
        if (active) {
            s->has_been_active = true;
        }
        if (s->has_been_active && !active) {
            suppress(id);
        }
        s = checked(*services_, id);
        if (s) {
            const bool visible = s->open && s->active_query && !s->caret_offscreen && !s->suppressed &&
                                 (s->pending || s->loading || !s->model.filtered().empty() ||
                                  s->empty_content.content() || !s->not_found.get().empty()) &&
                                 editable(id, false);
            if (visible != s->visible) {
                refresh(id);
            }
        }
    }
}

void MentionsComponentHost::validate_private_regions() {
    const auto ids = mounted_;
    for (auto id : ids) {
        (void)checked(*services_, id);
    }
}

void MentionsComponentHost::on_focus_changed() {
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto id = *it;
        auto* state = checked(*services_, id);
        if (!state) {
            continue;
        }
        const auto focus = services_->focus().state().focused;
        const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
        const bool inside = record && descendant(*services_, record->component, id);
        const bool exited = state->focus_was_inside && !inside;
        state->focus_was_inside = inside;
        if (inside) {
            state->closed_on_blur = false;
        }
        if (exited && !state->closed_on_blur) {
            state->closed_on_blur = true;
            request_open(id, false);
        }
    }
}

bool MentionsComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !window_active_) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (!record) {
        return false;
    }
    const auto component = record->component;
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto id = *it;
        auto* s = checked(*services_, id);
        if (!s || !editable(id) || !descendant(*services_, component, id)) {
            continue;
        }
        const bool main = focused == s->input_hit;
        const bool was_visible = s->visible;
        const auto editor_hit = s->input_hit;
        refresh_query(id, main && !was_visible && (event.key == input::Key::up || event.key == input::Key::down));
        s = checked(*services_, id);
        if (!s) {
            continue;
        }
        if (s->input_hit != editor_hit || !services_->interactions().find(*focused)) {
            return true;
        }
        s->focus_was_inside = true;
        const auto session = services_->input_runtime()->sessions().active();
        const auto* editor = services_->input_runtime()->editors().find(session.owner);
        if (editor && editor->composition().active) {
            return false;
        }
        using input::Key;
        if (event.key == Key::tab) {
            const auto target =
                tab_neighbor(*services_, s->input_hit, input::has_modifier(event.modifiers, input::KeyModifier::shift));
            request_open(id, false);
            if (checked(*services_, id) && target) {
                services_->focus().defer_focus(target, input::FocusModality::keyboard);
            }
            return true;
        }
        if (!main && layer(*services_, component) != std::optional{s->popup}) {
            continue;
        }
        if (event.key == Key::escape && s->visible) {
            for (const auto& entry : services_->components().paint_traversal()) {
                const auto child = entry.component;
                if (child == s->popup || !services_->components().is_window_layer_root(child) ||
                    !descendant(*services_, child, id) || !services_->interactions().component_active(child)) {
                    continue;
                }
                const auto owner = services_->components().parent(child);
                if (owner && descendant(*services_, component, *owner)) {
                    return false;
                }
            }
            if (services_->popover().owns_escape(id, component)) {
                return false;
            }
            services_->focus().defer_focus(focused, input::FocusModality::keyboard);
            request_open(id, false, !main);
            return true;
        }
        if (!main || event.modifiers != input::KeyModifier::none) {
            return false;
        }
        if (event.key == Key::enter) {
            if (s->visible && !s->loading && !s->pending && s->model.active()) {
                services_->focus().defer_focus(focused, input::FocusModality::keyboard);
                if (!event.repeat) {
                    choose(id, *s->model.active());
                }
                return true;
            }
            return false;
        }
        if (event.key != Key::up && event.key != Key::down) {
            return false;
        }
        if (!s->active_query || s->loading || s->pending) {
            return false;
        }
        const bool visible = was_visible;
        const auto input = s->input_hit;
        services_->focus().defer_focus(focused, input::FocusModality::keyboard);
        request_open(id, true);
        s = checked(*services_, id);
        if (!s || s->input_hit != input || !editable(id) || !s->visible) {
            return true;
        }
        if (visible || !s->model.active()) {
            s->model.move(event.key == Key::up ? -1 : 1);
        }
        refresh(id);
        return true;
    }
    return false;
}

void MentionsComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                             std::optional<input::InteractionId> hit,
                                             std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = checked(*services_, id);
        if (!s) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool own = record && descendant(*services_, record->component, id);
        if (event.action == input::PointerAction::down) {
            const bool closed = s->closed_on_blur;
            s->closed_on_blur = false;
            if (!own && s->visible && !closed) {
                request_open(id, false);
            }
        } else if (event.button == input::PointerButton::primary && hit == s->input_hit && hit == origin) {
            refresh_query(id, true);
        }
    }
}

bool MentionsComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    if (!record) {
        return false;
    }
    const auto owner = layer(*services_, record->component);
    for (auto id : mounted_) {
        auto* s = checked(*services_, id);
        if (!s || !s->visible || !services_->interactions().component_active(s->popup) ||
            owner != std::optional{s->popup} || !inside(s->clip, event.x, event.y)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().mentions();
        const float content = static_cast<float>(s->model.filtered().size()) * t.metrics.option_height;
        const float next = std::clamp(s->scroll - event.delta_y * t.metrics.option_height * 3, 0.0F,
                                      std::max(0.0F, content - s->clip.height));
        if (next != s->scroll) {
            s->scroll = next;
            s->reveal_active = false;
            s->needs_layout = true;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            RetireOnFailure rollback{*services_, id};
            const auto callback = s->on_scroll;
            if (callback) {
                callback(next);
            }
            rollback.complete = true;
        }
        return true;
    }
    return false;
}

void MentionsComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        position_window_layer(id, viewport, clip);
    }
}

void MentionsComponentHost::position_window_layer(runtime::ComponentId id, runtime::Size viewport, runtime::Rect clip) {
    if (std::ranges::find(positioning_, id) != positioning_.end()) {
        return;
    }
    positioning_.push_back(id);

    struct Guard {
        std::vector<runtime::ComponentId>& stack;

        ~Guard() {
            stack.pop_back();
        }
    } guard{positioning_};

    services_->popover().position_ancestors(id, viewport, clip);
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto& placed = services_->nodes().require(s->node);
    if (placed.place_count != 0) {
        const bool zero = placed.bounds.width <= 0 || placed.bounds.height <= 0;
        if (zero != s->zero_geometry) {
            s->zero_geometry = zero;
            if (zero) {
                suppress(id);
            }
            if (!checked(*services_, id)) {
                return;
            }
            services_->set_input_blocked(id, zero);
            s = checked(*services_, id);
            if (!s) {
                return;
            }
        }
    }
    if (!s->open || !s->active_query) {
        return;
    }
    RetireOnException failure{*services_, id};
    services_->input_runtime()->synchronize_editor_geometry(s->input, viewport, clip);
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto geometry = services_->input_runtime()->layout_snapshot(s->input);
    const auto anchor = geometry.caret;
    const auto caret_clip = intersect(anchor, geometry.clip);
    const bool offscreen = caret_clip.width <= 0 || caret_clip.height <= 0;
    if (s->caret_offscreen != offscreen) {
        s->caret_offscreen = offscreen;
        refresh(id, false);
    }
    s = checked(*services_, id);
    if (!s || !s->visible) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().mentions();
    const auto& m = t.metrics;
    const auto available = services_->component_clip(s->popup, viewport, clip);
    const auto popup_node = services_->components().root(s->popup);
    auto& engine = services_->layout();
    if (!s->needs_layout && s->anchor == anchor && s->available == available &&
        s->layout_generation == engine.generation()) {
        return;
    }
    const auto revision = s->model_revision;
    const std::vector<std::size_t> filtered =
        (s->loading || s->pending) ? std::vector<std::size_t>{}
                                   : std::vector<std::size_t>{s->model.filtered().begin(), s->model.filtered().end()};
    const float content = static_cast<float>(std::max<std::size_t>(1, filtered.size())) * m.option_height;
    const float below =
        std::clamp(available.y + available.height - anchor.y - anchor.height - m.gap, 0.0F, available.height);
    const float above = std::clamp(anchor.y - m.gap - available.y, 0.0F, available.height);
    const float width = std::min(std::max(100.0F, s->popup_width), available.width);
    const float inset = std::min(m.popup_padding, width / 2);
    const auto list_node = services_->components().root(s->list);
    const auto body_node = services_->components().root(s->popup_body);
    engine.set_layout(list_node,
                      layout::LeafLayout{{std::max(0.0F, width - inset * 2), std::min(s->list_height, content)}});
    const auto body_size =
        engine.measure_child(body_node, {std::max(0.0F, width - inset * 2), std::max(0.0F, width - inset * 2), 0,
                                         std::max(0.0F, std::max(below, above) - inset * 2)});
    s = checked(*services_, id);
    if (!s || s->model_revision != revision) {
        return;
    }
    const float wanted = body_size.height + 2 * inset;
    const bool use_above = s->placement == MentionsPlacement::Top ? (above >= wanted || above >= below)
                                                                  : (below < wanted && above > below);
    const float height = std::min(wanted, use_above ? above : below);
    s->bounds = {
        std::clamp(s->direction.get() == TextDirection::RightToLeft ? anchor.x + anchor.width - width : anchor.x,
                   available.x, available.x + available.width - width),
        std::clamp(use_above ? anchor.y - m.gap - height : anchor.y + anchor.height + m.gap, available.y,
                   available.y + available.height - height),
        width, height};
    (void)engine.measure_child(popup_node, layout::Constraints::fixed(width, height));
    engine.place_child(popup_node, s->bounds);
    const runtime::Rect body_bounds{s->bounds.x + inset, s->bounds.y + inset, std::max(0.0F, width - inset * 2),
                                    std::max(0.0F, height - inset * 2)};
    (void)engine.measure_child(body_node, layout::Constraints::fixed(body_bounds.width, body_bounds.height));
    engine.place_child(body_node, body_bounds);
    s = checked(*services_, id);
    if (!s || s->model_revision != revision) {
        return;
    }
    s->clip = intersect(services_->nodes().require(list_node).bounds, body_bounds);
    s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, content - s->clip.height));
    if (s->reveal_active && s->model.active()) {
        const auto it = std::ranges::find(filtered, *s->model.active());
        if (it != filtered.end()) {
            const float top = static_cast<float>(it - filtered.begin()) * m.option_height;
            s->scroll = s->clip.height < m.option_height
                            ? top
                            : std::clamp(s->scroll, std::max(0.0F, top + m.option_height - s->clip.height), top);
        }
    }
    s->reveal_active = false;
    if (s->model.options().size() > 32 && s->virtual_list && s->match_width) {
        reconcile_rows(id);
        s = checked(*services_, id);
        if (!s || s->model_revision != revision) {
            return;
        }
        s->reveal_active = false;
    }
    const auto empty_node = services_->components().root(s->empty_root);
    if (filtered.empty()) {
        (void)engine.measure_child(empty_node, layout::Constraints::fixed(s->clip.width, m.option_height));
        engine.place_child(empty_node, {s->clip.x, s->clip.y, s->clip.width, m.option_height});
        s = checked(*services_, id);
        if (!s) {
            return;
        }
    }
    const auto rows = s->rows;
    const auto menu_clip = s->clip;
    const auto scroll = s->scroll;
    for (auto child : rows) {
        const auto* row = services_->components().state<Row>(child);
        if (!row) {
            continue;
        }
        const auto it = std::ranges::find(filtered, row->index);
        if (it == filtered.end()) {
            continue;
        }
        const float y = menu_clip.y - scroll + static_cast<float>(it - filtered.begin()) * m.option_height;
        const float px = std::min(m.option_padding_inline, menu_clip.width / 2);
        const auto node = row->node;
        engine.set_layout(node, layout::BoxLayout{{px, m.option_padding_block, px, m.option_padding_block}});
        (void)engine.measure_child(node, layout::Constraints::fixed(menu_clip.width, m.option_height));
        if (!checked(*services_, id)) {
            return;
        }
        engine.place_child(node, {menu_clip.x, y, menu_clip.width, m.option_height});
        s = checked(*services_, id);
        if (!s) {
            return;
        }
        const auto* live_row = services_->components().state<Row>(child);
        if (!live_row) {
            return;
        }
        const auto shown = intersect(services_->nodes().require(node).bounds, menu_clip);
        const bool disabled = s->model.options()[live_row->index].disabled || s->model.options()[live_row->index].group;
        const bool blocked = shown.width <= 0 || shown.height <= 0 || disabled;
        if (blocked) {
            restore_row_focus(*services_, id, child, s->input_hit);
            if (!checked(*services_, id)) {
                return;
            }
        }
        services_->set_input_blocked(child, blocked);
        if (!checked(*services_, id)) {
            return;
        }
    }
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    s->needs_layout = false;
    s->anchor = anchor;
    s->available = available;
    s->layout_generation = engine.generation();
}

void MentionsComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    for (auto id : mounted_) {
        const auto* s = checked(*services_, id);
        if (!s) {
            continue;
        }
        services_->nodes().require(services_->components().root(s->popup)).content_opacity = s->motion.opacity;
        const auto& t = services_->components().theme_scope(id)->snapshot().mentions();
        if (!s->visible) {
            continue;
        }
        const float radius = std::min({t.metrics.border_radius, s->bounds.width / 2, s->bounds.height / 2});
        auto background = quad(s->bounds, t.colors.popup_background, radius);
        background.opacity = s->motion.opacity;
        (void)services_->surfaces().update_surface(s->background, std::span{&background, 1});
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{s->bounds.x, s->bounds.y, s->bounds.width, s->bounds.height}, radius};
        effects.shadows = t.shadow;
        effects.shadow_opacity = s->motion.opacity;
        effects.ancestor_clip = graphics::EffectClip{1, s->available};
        (void)services_->surfaces().update_effects(s->background, effects);
        for (auto child : s->rows) {
            const auto* row = services_->components().state<Row>(child);
            const bool shown = services_->components().branch_active(child);
            const bool selected = false;
            const bool active = s->model.active() == row->index;
            auto fill =
                quad(intersect(services_->nodes().require(row->node).bounds, s->clip),
                     selected ? t.colors.option_selected_background : t.colors.option_active_background, radius);
            fill.opacity = shown && (selected || active) ? s->motion.opacity : 0.0F;
            (void)services_->surfaces().update_surface(row->surface, std::span{&fill, 1});
            services_->scene_composer().set_fragment(row->hit, {}, row->interaction, s->clip);
        }
    }
}

std::size_t MentionsComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed = 0;
    for (auto id : std::vector{mounted_}) {
        auto* s = checked(*services_, id);
        if (!s) {
            continue;
        }
        const auto previous = s->motion.opacity;
        if (!s->visible || !s->animate || services_->motion_preference() == animation::MotionPreference::reduced ||
            !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot()).enabled()) {
            s->motion.stop();
        }
        if (s->motion.tick(time) || previous != s->motion.opacity) {
            services_->nodes().require(services_->components().root(s->popup)).content_opacity = s->motion.opacity;
            ++changed;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> MentionsComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> next;
    for (auto id : mounted_) {
        if (const auto* s = checked(*services_, id)) {
            const auto deadline = s->motion.deadline();
            if (deadline && (!next || *deadline < *next)) {
                next = deadline;
            }
        }
    }
    return next;
}
} // namespace ryn::detail

namespace ryn {
void Mentions(MentionsProps props) {
    if (!detail::active_mentions) {
        throw std::logic_error("Mentions requires window component services");
    }
    detail::active_mentions->mount(props);
}
} // namespace ryn
