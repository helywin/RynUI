#include "component/config_defaults.hpp"
#include "component/time_picker_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/popover_component.hpp"
#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "component/table_component.hpp"
#include "component/navigation_transition.hpp"
#include "component/picker_range.hpp"
#include "component/picker_i18n.hpp"
#include "component/date_picker_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"
#include <ryn/button.hpp>
#include <algorithm>
#include <cmath>
#include <set>

namespace ryn::detail {
namespace {
thread_local TimePickerComponentHost* active_time_pickers{};

struct TimeRow final {
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<String> text{String{}};
    Signal<TimePickerCellInfo> info{TimePickerCellInfo{}};
    bool enabled{};
    bool hovered{};
};

struct TimeDisplayFormat final {
    bool twelve{};
    String pattern;
    PickerLocale locale;
    friend bool operator==(const TimeDisplayFormat&, const TimeDisplayFormat&) = default;
};

struct TimePickerState final {
    explicit TimePickerState(TimePickerModel value) : model(std::move(value)) {}

    TimePickerModel model;
    TimePickerSnapshot view;
    runtime::NodeId node;
    runtime::ComponentId actions;
    input::InteractionId selector_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    TimePickerRef reference;
    Signal<String> display{String{}};
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<bool> confirm_disabled{false};
    Signal<ThemeConfig> input_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    std::array<bool, 5> reveal{true, true, true, true, true};
    std::unique_ptr<NavigationTransition> transition;
    PickerLocale locale;
    PickerLocale format_locale;
    std::optional<PickerEndpoint> range_endpoint;
    String format;
    bool twelve{};
    bool change_on_scroll{};
    bool show_now{};
    TextDirection direction{TextDirection::LeftToRight};
    PickerPlacement placement{PickerPlacement::BottomLeft};
    std::function<TimeOfDay()> clock;
    std::function<void(TimeOfDay)> on_select;
    std::function<void(TimePickerValue)> on_ok;
    TimePickerCellRenderer cell_render;
    std::optional<TimePickerPanel> panel;
    runtime::ComponentId custom_panel;
    bool open{};
    bool controlled_open{};
    bool suppressed{};
    bool disabled{};
    bool allow_clear{true};
    bool need_confirm{true};
    bool needs_layout{true};
    bool focus_within{};
    bool updating_rows{};
    bool rows_pending{};
    std::uint64_t revision{};
    std::uint64_t edit_session{};
    std::optional<std::uint64_t> submitting;
    std::optional<std::pair<TimePickerValue, std::uint64_t>> echo;
    runtime::Rect anchor;
    runtime::Rect clip;
    std::function<void(TimePickerValue)> on_change;
    std::function<PickerRequestDisposition(TimePickerValue, const PickerCandidate&)> on_request;
    std::function<void(bool)> on_open;
};

PickerCandidate range_candidate(WindowComponentServices& services,
                                const std::shared_ptr<PickerRangeState<TimeOfDay>>& range,
                                const PickerCandidate& source = {}) {
    std::vector<std::pair<runtime::ComponentId, PickerEditorSession>> editors;
    for (const auto id : range->owners) {
        if (const auto* state = services.components().state<TimePickerState>(id)) {
            const auto* editor = services.input_runtime()->editors().find(state->editor);
            if (editor) {
                editors.emplace_back(id, PickerEditorSession{state->edit_session, editor->composition().active});
            }
        }
    }
    return {source.semantics, [&services, editors = std::move(editors), source] {
                if (!source.active()) {
                    return false;
                }
                for (const auto& [id, session] : editors) {
                    const auto* state = services.components().state<TimePickerState>(id);
                    const auto* editor = state ? services.input_runtime()->editors().find(state->editor) : nullptr;
                    if (!state || state->edit_session != session.edit || !services.components().scope(id).active() ||
                        !editor || editor->composition().active != session.composing) {
                        return false;
                    }
                }
                return true;
            }};
}

bool active_edit(WindowComponentServices& services, const TimePickerState& state) {
    const auto* editor = services.input_runtime()->editors().find(state.editor);
    return state.model.text_dirty() || (editor && services.input_runtime()->focused(state.view.selector) &&
                                        (editor->composition().active || !editor->selection().empty()));
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

bool range_sibling(WindowComponentServices& services, runtime::ComponentId id, const TimePickerState& state,
                   runtime::ComponentId target) {
    if (!state.on_request || !state.range_endpoint) {
        return false;
    }
    const auto parent = services.components().parent(id);
    if (!parent) {
        return false;
    }
    for (auto current = std::optional{target}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        const auto* sibling = services.components().state<TimePickerState>(*current);
        if (sibling && sibling->on_request && sibling->range_endpoint &&
            sibling->range_endpoint != state.range_endpoint && services.components().parent(*current) == parent) {
            return true;
        }
        if (*current == *parent) {
            break;
        }
    }
    return false;
}

std::optional<runtime::ComponentId> nearest_layer(WindowComponentServices& services, runtime::ComponentId id) {
    for (auto current = std::optional{id}; current; current = services.components().parent(*current)) {
        if (services.components().is_window_layer_root(*current)) {
            return current;
        }
    }
    return {};
}

bool inside(runtime::Rect b, float x, float y) {
    return x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

graphics::QuadInstance quad(runtime::Rect bounds, Color color, float radius = 0) {
    graphics::QuadInstance result;
    result.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
    result.color = foreground(color);
    result.corner_radius = std::min(radius, std::min(bounds.width, bounds.height) / 2);
    return result;
}

runtime::ComponentId group(WindowComponentServices& services, layout::LayoutModel model, bool clip = false) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, std::move(model));
    services.nodes().require(node).clip_content = clip;
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}

int step(const TimePickerModel& model, int column) {
    const auto& r = model.rules();
    return column == 0   ? 1
           : column == 1 ? r.minute_step
           : column == 2 ? r.second_step
           : column == 3 ? r.millisecond_step
                         : 1;
}

int visible_columns(const TimePickerModel& model) {
    int count{};
    for (int c = 0; c < 5; ++c) {
        count += model.column_visible(static_cast<TimeColumn>(c));
    }
    return count;
}
} // namespace

TimePickerComponentHost::TimePickerComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TimePickerComponentHost::~TimePickerComponentHost() {
    services_->detach(*this);
}

void* TimePickerComponentHost::begin_mount() noexcept {
    auto* previous = active_time_pickers;
    active_time_pickers = this;
    return previous;
}

void TimePickerComponentHost::end_mount(void* previous) noexcept {
    active_time_pickers = static_cast<TimePickerComponentHost*>(previous);
}

void TimePickerComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

std::optional<PickerEditorSession> TimePickerComponentHost::editor_session(runtime::ComponentId id) const {
    const auto* state = services_->components().state<TimePickerState>(id);
    if (!state || !services_->components().scope(id).active()) {
        return std::nullopt;
    }
    const auto* editor = services_->input_runtime()->editors().find(state->editor);
    return editor ? std::optional{PickerEditorSession{state->edit_session, editor->composition().active}}
                  : std::nullopt;
}

TimePickerSnapshot TimePickerComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<TimePickerState>(id);
    if (!s) {
        throw std::out_of_range("TimePicker component is stale");
    }
    auto result = s->view;
    result.value = s->model.value();
    result.pending = s->model.panel();
    result.draft = s->model.draft();
    const auto model = s->model;
    result.confirmable = model.text_dirty() ? model.text_value().has_value() : model.confirmable();
    return result;
}

void TimePickerComponentHost::refresh(runtime::ComponentId id, bool geometry) try {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto revision = ++s->revision;
    if (s->updating_rows) {
        s->rows_pending = true;
        s->needs_layout = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        return;
    }
    const auto live = [&] {
        s = services_->components().state<TimePickerState>(id);
        return s && services_->components().scope(id).active() && s->revision == revision;
    };
    const auto publish = [&](auto signal, auto value) {
        signal.set(std::move(value));
        return live();
    };
    const auto row_for = [&](runtime::ComponentId child) {
        auto* row = services_->components().state<TimeRow>(child);
        if (!row) {
            static_cast<void>(services_->retire_failed_component(id));
        }
        return row;
    };
    const bool visible =
        s->open && !s->suppressed && !s->disabled && window_active_ && services_->interactions().component_active(id);
    if (!s->model.column_visible(static_cast<TimeColumn>(s->view.active_column))) {
        s->view.active_column = 0;
    }
    const bool changed = visible != s->view.visible;
    s->view.visible = visible;
    if (changed || !visible) {
        const auto focused = services_->focus().state().focused;
        const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
        s->focus_within =
            visible && target &&
            (descendant(*services_, target->component, id) || range_sibling(*services_, id, *s, target->component));
    }
    bool structure = services_->components().set_branch_active(s->view.popup, visible);
    structure |= services_->components().set_branch_active(s->actions, s->need_confirm);
    s->rows_pending = false;
    for (int c = 0; c < 5; ++c) {
        if (s->model.column_visible(static_cast<TimeColumn>(c)) && s->view.rows[c].empty()) {
            s->rows_pending = true;
        }
        structure |= services_->components().set_branch_active(
            s->view.columns[c], !s->panel && s->model.column_visible(static_cast<TimeColumn>(c)));
    }
    if (s->transition && changed) {
        s->transition->restart(visible);
    }
    if (structure) {
        services_->mark_scene_structure_dirty();
    }
    if (!publish(s->display, s->model.draft()) ||
        !publish(s->clear_visible, s->allow_clear && s->model.value().has_value() && !s->disabled) ||
        !publish(s->action_disabled, s->disabled)) {
        return;
    }
    const auto validation_model = s->model;
    const bool confirmable =
        validation_model.text_dirty() ? validation_model.text_value().has_value() : validation_model.confirmable();
    if (!live()) {
        return;
    }
    if (!publish(s->confirm_disabled, !confirmable)) {
        return;
    }
    const auto token = services_->components().theme_scope(id)->snapshot().time_picker();
    const auto model = s->model;
    const auto rows = s->view.rows;
    for (int column = 0; column < 5; ++column) {
        for (std::size_t index = 0; index < rows[column].size(); ++index) {
            const auto row_id = rows[column][index];
            auto* row = row_for(row_id);
            if (!row) {
                return;
            }
            const bool column_shown = model.column_visible(static_cast<TimeColumn>(column));
            const bool shown = column_shown &&
                               static_cast<int>(index) < model.row_count(static_cast<TimeColumn>(column)) &&
                               static_cast<int>(index) % step(model, column) == 0;
            structure |= services_->components().set_branch_active(row_id, shown);
            const bool enabled =
                shown && model.candidate(static_cast<TimeColumn>(column), static_cast<int>(index)).has_value();
            if (!live()) {
                return;
            }
            row = row_for(row_id);
            if (!row) {
                return;
            }
            row->enabled = enabled;
            const int n = static_cast<int>(index);
            if (!publish(row->text, column == 4
                                        ? (n == 0 ? s->locale.am : s->locale.pm)
                                        : String::from_utf8(column == 0 && s->twelve ? std::to_string((n + 11) % 12 + 1)
                                                            : column == 3 ? (n < 10    ? "00"
                                                                             : n < 100 ? "0"
                                                                                       : "") +
                                                                                std::to_string(n)
                                                                          : (n < 10 ? "0" : "") + std::to_string(n))
                                              .value())) {
                return;
            }
            row = row_for(row_id);
            if (!row) {
                return;
            }
            services_->interactions().set_eligible(row->interaction, visible && row->enabled);
            const bool selected = s->model.row_value(static_cast<TimeColumn>(column)) == static_cast<int>(index);
            if (!publish(row->color, foreground(!row->enabled ? token.colors.disabled
                                                : selected    ? token.colors.selected
                                                              : token.colors.text))) {
                return;
            }
            row = row_for(row_id);
            if (!row || !publish(row->info, TimePickerCellInfo{static_cast<TimePickerColumn>(column),
                                                               column == 0 && s->twelve ? (n + 11) % 12 + 1 : n,
                                                               selected, !enabled, s->range_endpoint})) {
                return;
            }
            row = row_for(row_id);
            if (!row ||
                !publish(row->font, runtime::SemanticTypography{token.metrics.font_family, 400, false,
                                                                token.metrics.font_size, token.metrics.line_height})) {
                return;
            }
        }
    }
    if (structure) {
        services_->mark_scene_structure_dirty();
    }
    if (geometry || changed || structure || s->rows_pending) {
        s->needs_layout = true;
        if (geometry || changed) {
            s->reveal.fill(true);
        }
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    if (changed && !visible) {
        const auto popup = s->view.popup;
        const auto order = services_->interactions().declaration_order();
        const std::vector<input::InteractionId> hits{order.begin(), order.end()};
        for (auto hit : hits) {
            const auto* record = services_->interactions().find(hit);
            if (record && services_->components().contains(popup) && descendant(*services_, record->component, popup)) {
                // FocusManager must deliver blur/sanitize and deferred focus
                // after the current command; only pointer state is cancelled.
                services_->pointer().cancel_pointer_interaction(hit);
            }
        }
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void TimePickerComponentHost::request_open(runtime::ComponentId id, bool open, bool restore) try {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || !window_active_ || !services_->interactions().component_active(id)))) {
        return;
    }
    const bool changed = open ? (!s->open || s->suppressed) : (s->open && !s->suppressed);
    const auto callback = s->on_open;
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    const bool in_popup = record && descendant(*services_, record->component, s->view.popup);
    const auto selector = s->selector_hit;
    if (!s->controlled_open) {
        s->open = open;
    }
    if (open) {
        s->suppressed = false;
    }
    if (!open || changed) {
        s->model.cancel();
        s->focus_within = false;
    }
    refresh(id);
    if (changed && callback) {
        callback(open);
    }
    // A controlled parent may accept, reject, destroy or reopen synchronously.
    // Restore only for an actual close of this same component/focus owner.
    s = services_->components().state<TimePickerState>(id);
    if (!open && in_popup && s && !s->view.visible && services_->focus().state().focused == focused) {
        services_->focus().defer_focus(restore ? std::optional{selector} : std::nullopt,
                                       input::FocusModality::keyboard);
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void TimePickerComponentHost::suppress(runtime::ComponentId id) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !s->open || s->suppressed) {
        return;
    }
    const auto callback = s->on_open;
    s->suppressed = true;
    s->focus_within = false;
    if (!s->controlled_open) {
        s->open = false;
    }
    s->model.cancel();
    refresh(id);
    if (callback) {
        callback(false);
    }
}

bool TimePickerComponentHost::publish(runtime::ComponentId id, TimePickerValue value, bool close, bool from_text) try {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || s->disabled || !services_->interactions().component_active(id)) {
        return false;
    }
    const auto before = s->model.value();
    const auto revision = s->revision;
    const auto session = s->edit_session;
    const auto editor_id = s->editor;
    const bool composing = services_->input_runtime()->editors().require(editor_id).composition().active;
    if (s->submitting == session) {
        return false;
    }
    auto model = s->model;
    model.owner_guard([this, id, revision, session, editor_id, composing] {
        const auto* live = services_->components().state<TimePickerState>(id);
        const auto* editor = services_->input_runtime()->editors().find(editor_id);
        return live && !live->disabled && live->revision == revision && live->edit_session == session &&
               services_->components().scope(id).active() && editor && (composing || !editor->composition().active);
    });
    const auto precision = model.request_precision(from_text);
    const auto intent = model.request(value, from_text);
    s = services_->components().state<TimePickerState>(id);
    if (!intent || !s || s->revision != revision || s->edit_session != session) {
        return false;
    }
    const auto callback = s->on_change;
    const auto request = s->on_request;
    const auto current = [this, id, session, editor_id, composing] {
        const auto* live = services_->components().state<TimePickerState>(id);
        const auto* editor = services_->input_runtime()->editors().find(editor_id);
        return live && !live->disabled && live->edit_session == session && services_->components().scope(id).active() &&
               editor && (composing || !editor->composition().active);
    };
    if (request) {
        close = false;
        s->submitting = session;
        const auto result = request(*intent, PickerCandidate{{{}, precision}, current});
        s = services_->components().state<TimePickerState>(id);
        if (s && s->submitting == session) {
            s->submitting.reset();
        }
        if (result != PickerRequestDisposition::Consumed || !current()) {
            return false;
        }
        // The parent has reconciled its actual pending/authority, including refusal.
        s->model.cancel();
    } else {
        model.owner_guard(
            [this, id] { return services_->components().contains(id) && services_->components().scope(id).active(); });
        s->model = std::move(model);
        s->model.finish_edit();
    }
    if (close) {
        s->model.cancel();
    }
    refresh(id);
    s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return true;
    }
    if (!current()) {
        return true;
    }
    if (before != *intent && callback) {
        s->echo = std::pair{*intent, session};
        callback(*intent);
    }
    s = services_->components().state<TimePickerState>(id);
    if (s && s->echo && s->echo->second == session) {
        s->echo.reset();
    }
    if (close && s && s->edit_session == session && !s->model.text_dirty() && !active_edit(*services_, *s)) {
        request_open(id, false, true);
    }
    return true;
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void TimePickerComponentHost::confirm(runtime::ComponentId id) try {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return;
    }
    if (services_->input_runtime()->editors().require(s->editor).composition().active) {
        return;
    }
    const bool composite = bool(s->on_request);
    const auto session = s->edit_session;
    const auto revision = s->revision;
    const auto model = s->model;
    const auto value = model.text_dirty()    ? model.text_value()
                       : model.confirmable() ? std::optional<TimePickerValue>{model.panel()}
                                             : std::nullopt;
    s = services_->components().state<TimePickerState>(id);
    if (!value || !s || s->revision != revision || s->edit_session != session) {
        return;
    }
    if (!publish(id, *value, false, model.text_dirty())) {
        return;
    }
    s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return;
    }
    if (s->edit_session == session && !s->model.text_dirty()) {
        s->model.finish_edit();
        refresh(id, false);
        s = services_->components().state<TimePickerState>(id);
        if (!s) {
            return;
        }
    }
    const auto callback = s->on_ok;
    if (callback) {
        callback(*value);
    }
    s = services_->components().state<TimePickerState>(id);
    if (!composite && s && s->edit_session == session && !s->model.text_dirty() && !active_edit(*services_, *s)) {
        request_open(id, false, true);
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void TimePickerComponentHost::choose(runtime::ComponentId id, TimeColumn column, int value) try {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !s->view.visible) {
        return;
    }
    const auto revision = s->revision;
    const auto session = s->edit_session;
    auto model = s->model;
    if (!model.candidate(column, value)) {
        return;
    }
    s = services_->components().state<TimePickerState>(id);
    if (!s || s->revision != revision) {
        return;
    }
    model.select(column, value);
    s = services_->components().state<TimePickerState>(id);
    if (!s || s->revision != revision) {
        return;
    }
    s->model = std::move(model);
    s->view.active_column = static_cast<int>(column);
    refresh(id);
    s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return;
    }
    services_->focus().defer_focus(s->selector_hit, input::FocusModality::pointer);
    // Pointer focus may synchronously run footer onBlur, including unmounting
    // or updating this picker. Continue only with the same live generation.
    s = services_->components().state<TimePickerState>(id);
    if (!s || !services_->components().scope(id).active() || !s->view.visible || s->disabled || !window_active_ ||
        !services_->interactions().component_active(id)) {
        return;
    }
    const auto* selector = services_->interactions().find(s->selector_hit);
    if (!selector || !selector->eligible || !services_->interactions().component_active(selector->component)) {
        return;
    }
    const auto selected = s->model.panel();
    const auto notify = s->on_select;
    if (notify) {
        notify(selected);
    }
    s = services_->components().state<TimePickerState>(id);
    if (s && !s->need_confirm && s->edit_session == session && !s->model.text_dirty() && s->model.panel() == selected) {
        publish(id, selected, false);
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void TimePickerComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& token = theme.time_picker();
    ThemeConfig config;
    config.input.tokens.background = token.colors.input_background;
    config.input.tokens.hover_background = token.colors.input_background;
    config.input.tokens.active_background = token.colors.input_background;
    config.input.tokens.hover_border_color = token.colors.hover_border;
    config.input.tokens.active_border_color = token.colors.active_border;
    const auto& original = InputTokenAccess::get(theme).active_shadow;
    std::array<ShadowLayer, ShadowList::capacity> shadows{};
    std::copy(original.layers().begin(), original.layers().end(), shadows.begin());
    for (std::size_t i = 0; i < original.size(); ++i) {
        shadows[i].color = token.colors.active_outline;
    }
    config.input.tokens.active_shadow = ShadowList{std::span<const ShadowLayer>{shadows.data(), original.size()}};
    if (geometry) {
        layout::FlexLayout footer_layout;
        footer_layout.direction = layout::FlexDirection::vertical;
        footer_layout.item_policy = layout::FlexItemPolicy::sequential;
        footer_layout.main_gap = token.metrics.footer_padding;
        services_->layout().set_layout(services_->components().root(s->view.footer), footer_layout);
    }
    s->input_theme.set(std::move(config));
    if (services_->components().set_window_layer(s->view.popup, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
    s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return;
    }
    // Panel-only colors and order may leave every child Prop unchanged.
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
}

void TimePickerComponentHost::mount_rows(runtime::ComponentId id, int column) {
    auto& services = *services_;
    auto* s = services.components().state<TimePickerState>(id);
    if (!s || s->updating_rows || !s->view.rows[column].empty()) {
        return;
    }
    const auto revision = s->revision;
    const auto model = s->model;
    const auto popup_hit = s->popup_hit;
    const auto endpoint = s->range_endpoint;
    const auto render = s->cell_render;
    const bool twelve = s->twelve;
    std::vector<runtime::ComponentId> created;
    const auto live = [&] {
        s = services.components().state<TimePickerState>(id);
        return s && services.components().scope(id).active() && s->revision == revision;
    };
    const auto cancel = [&] {
        for (auto child : created) {
            static_cast<void>(services.retire_failed_component(child));
        }
        if (auto* current = services.components().state<TimePickerState>(id)) {
            current->updating_rows = false;
            current->rows_pending = true;
            services.request_frame();
        }
    };
    auto& build = runtime::require_component_build_context();
    const int count = column == 0 ? 24 : column == 3 ? 1000 : column == 4 ? 2 : 60;
    s->updating_rows = true;
    try {
        for (int value = 0; value < count; ++value) {
            const auto row_id = build.mount_component<TimeRow>();
            created.push_back(row_id);
            services.components().set_branch_active(row_id, false);
            auto& row = build.state<TimeRow>(row_id);
            row.node = build.root(row_id);
            services.layout().set_layout(row.node, layout::BoxLayout{});
            services.nodes().require(row.node).clip_content = true;
            row.interaction =
                services.interactions().create({row_id, row.node, popup_hit, false, false, {}, false, false});
            input::InteractionHandlers handlers;
            handlers.target = [this, id, row_id, column, value](input::PointerDispatchContext& event) {
                const auto* s = services_->components().state<TimePickerState>(id);
                auto* row = services_->components().state<TimeRow>(row_id);
                if (!s || !row || !row->enabled || !s->view.visible) {
                    return;
                }
                if (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::leave) {
                    row->hovered = event.kind() == input::PointerEventKind::enter;
                    services_->dirty().invalidate(row->node, runtime::DirtyFlags::Material);
                }
                if (event.kind() == input::PointerEventKind::down &&
                    event.event().button == input::PointerButton::primary) {
                    static_cast<void>(event.capture_pointer());
                    event.stop_propagation();
                }
                if (event.kind() == input::PointerEventKind::up &&
                    event.event().button == input::PointerButton::primary) {
                    static_cast<void>(event.release_pointer_capture());
                    const auto bounds =
                        intersect(services_->nodes().require(row->node).bounds, s->view.column_clips[column]);
                    const bool hit =
                        event.press_origin() == row->interaction && inside(bounds, event.event().x, event.event().y) &&
                        services_->hit_test().hit_test({event.event().x, event.event().y}) == row->interaction;
                    event.stop_propagation();
                    if (hit) {
                        choose(id, static_cast<TimeColumn>(column), value);
                    }
                }
            };
            services.interactions().set_handlers(row.interaction, std::move(handlers));
            row.hit = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
            services.scene_composer().set_fragment(row.hit, {}, row.interaction);
            const auto fragment =
                build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            row.surface =
                services.surfaces().create_surface(row_id, row.node, fragment, std::span{&initial, 1}, effects);
            build.on_resource_cleanup(row_id, [&services, node = row.node, interaction = row.interaction, hit = row.hit,
                                               surface = row.surface] {
                services.pointer().cancel_interaction(interaction);
                services.interactions().remove(interaction);
                services.surfaces().destroy(surface);
                services.scene_composer().remove_fragment(hit);
                services.layout().remove_layout(node);
            });
            const bool eligible = model.candidate(static_cast<TimeColumn>(column), value).has_value();
            if (!live()) {
                cancel();
                return;
            }
            auto* current = services.components().state<TimeRow>(row_id);
            if (!current) {
                throw std::runtime_error("TimePicker predicate retired its unpublished row");
            }
            current->info.set({static_cast<TimePickerColumn>(column),
                               column == 0 && twelve ? (value + 11) % 12 + 1 : value,
                               model.row_value(static_cast<TimeColumn>(column)) == value, !eligible, endpoint});
            const auto info = current->info;
            const auto color = current->color;
            const auto font = current->font;
            const auto text = current->text;
            if (render) {
                const auto rich = render(Prop<TimePickerCellInfo>{info});
                if (!live()) {
                    cancel();
                    return;
                }
                if (!services.components().contains(row_id)) {
                    throw std::runtime_error("TimePicker renderer retired its unpublished row");
                }
                build.mount_slot_with_semantic_text_style(row_id, Content{SlotContentAccess::function(rich)}, color,
                                                          font);
            } else {
                build.mount_slot_with_semantic_text_style(row_id, Content{[text] { Text(TextProps{}.content(text)); }},
                                                          color, font);
            }
            if (!live()) {
                cancel();
                return;
            }
            if (!services.components().contains(row_id)) {
                throw std::runtime_error("TimePicker slot retired its unpublished row");
            }
        }
        s->view.rows[column] = std::move(created);
        s->updating_rows = false;
    } catch (...) {
        for (auto child : created) {
            static_cast<void>(services.retire_failed_component(child));
        }
        static_cast<void>(services.retire_failed_component(id));
        throw;
    }
}

void TimePickerComponentHost::mount(const TimePickerProps& props, const std::optional<TimePickerFooter>& footer) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("TimePicker requires a window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("TimePicker cannot combine controlled and default values");
    }
    const auto locale_source =
        picker_locale_prop(props.locale_, runtime::require_component_build_context().config_scope());
    const auto format_locale_source =
        picker_format_locale_prop(props.locale_, runtime::require_component_build_context().config_scope());
    const Prop<TimeDisplayFormat> display_format_source = bind(
        [format_locale_source, format = props.format_, precision = props.precision_, twelve = props.use_12_hours_] {
            auto locale = read_prop(format_locale_source);
            locale.validate();
            const auto use_twelve = read_prop(twelve);
            auto pattern = picker_time_pattern(read_prop(precision), use_twelve, read_prop(format), locale);
            validate_time_format(pattern.bytes());
            return TimeDisplayFormat{use_twelve, std::move(pattern), std::move(locale)};
        });
    const auto initial_format = read_prop(display_format_source);
    TimePickerModel model{read_prop(props.rules_), read_prop(props.precision_),
                          props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(TimePickerValue{}),
                          props.value_.has_value(), props.panel_value_};
    model.configure_display(initial_format.twelve, initial_format.pattern, initial_format.locale);
    if (read_prop(props.show_now_) && !props.clock_) {
        throw std::invalid_argument("TimePicker showNow requires an explicit clock");
    }
    std::set<std::string> preset_keys;
    for (const auto& p : props.presets_) {
        if (p.key.empty() || !p.value || !preset_keys.insert(std::string{p.key.bytes()}).second) {
            throw std::invalid_argument("TimePicker presets require unique keys and a value callback");
        }
    }
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TimePickerState>(std::move(model));
    auto& s = build.state<TimePickerState>(id);
    s.node = build.root(id);
    s.model.owner_guard(
        [this, id] { return services_->components().contains(id) && services_->components().scope(id).active(); });
    s.transition = std::make_unique<NavigationTransition>(services, id);
    s.locale = read_prop(locale_source);
    s.format_locale = initial_format.locale;
    s.range_endpoint = props.range_endpoint_;
    s.format = initial_format.pattern;
    s.twelve = initial_format.twelve;
    s.direction = read_prop(props.direction_);
    s.placement = read_prop(props.placement_);
    s.change_on_scroll = read_prop(props.change_on_scroll_);
    s.show_now = read_prop(props.show_now_);
    s.clock = props.clock_;
    s.on_select = props.on_select_;
    s.on_ok = props.on_ok_;
    s.cell_render = props.cell_render_;
    s.panel = props.panel_;
    s.reference = props.ref_.value_or(TimePickerRef{});
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.disabled = read_prop(props.disabled_);
    s.allow_clear = read_prop(props.allow_clear_);
    s.need_confirm = read_prop(props.need_confirm_);
    s.on_change = props.on_change_;
    s.on_request = props.on_request_;
    s.on_open = props.on_open_;
    s.display.set(s.model.draft());
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.input_theme), ThemeContent{[&] {
                      FormPrivateEditorScope private_editor;
                      Input(InputProps{}
                                .value(s.display)
                                .placeholder(props.placeholder_.value_or(
                                    Prop<String>{bind([locale_source, endpoint = props.range_endpoint_] {
                                        const auto locale = read_prop(locale_source);
                                        return endpoint
                                                   ? locale.time_range_placeholder[static_cast<std::size_t>(*endpoint)]
                                                   : locale.time_placeholder;
                                    })}))
                                .disabled(props.disabled_)
                                .readOnly(props.read_only_)
                                .size(props.size_)
                                .variant(props.variant_)
                                .status(props.status_)
                                .ref(s.reference)
                                .autoFocus(props.auto_focus_)
                                .onChange([this, id](String value) {
                                    auto* s = services_->components().state<TimePickerState>(id);
                                    if (!s || s->disabled) {
                                        return;
                                    }
                                    if (s->submitting == s->edit_session) {
                                        s->model.finish_edit();
                                    }
                                    const auto revision = s->revision;
                                    const auto session = ++s->edit_session;
                                    s->model.stage_edit(value);
                                    auto model = s->model;
                                    model.edit(std::move(value));
                                    s = services_->components().state<TimePickerState>(id);
                                    if (!s || s->revision != revision || s->edit_session != session) {
                                        return;
                                    }
                                    s->model = std::move(model);
                                    refresh(id);
                                })
                                .onSubmit([this, id](String) { confirm(id); })
                                .onBlur([this, id] {
                                    auto* s = services_->components().state<TimePickerState>(id);
                                    if (!s || !services_->components().scope(id).active()) {
                                        return;
                                    }
                                    const auto focused = services_->focus().state().focused;
                                    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
                                    if (record && (descendant(*services_, record->component, s->view.popup) ||
                                                   range_sibling(*services_, id, *s, record->component))) {
                                        return;
                                    }
                                    if (s->submitting == s->edit_session) {
                                        return;
                                    }
                                    s->model.cancel();
                                    // Pointer outside and prepare's actual focus leave
                                    // issue the close request once, after focus transfer.
                                    refresh(id, false);
                                }),
                            {}, InputSuffix{[&] {
                                s.view.clear = group(services, layout::BoxLayout{});
                                runtime::require_component_build_context().mount_slot(
                                    s.view.clear, Content{[&] {
                                        mount_input_affix_action(
                                            services, IconSource{IconName::CloseCircleFilled}, s.action_disabled,
                                            [this, id] { publish(id, {}, true); }, s.clear_visible, false, false);
                                    }});
                                if (props.suffix_icon_) {
                                    SlotContentAccess::function (*props.suffix_icon_)();
                                } else {
                                    Icon(IconProps{}
                                             .source(IconSource{IconName::ClockCircleOutlined})
                                             .tone(TextTone::Secondary));
                                }
                            }});
                      const auto input = inputs->mounted_inputs().back();
                      s.view.selector = input.component;
                      s.selector_hit = input.interaction;
                      s.editor = input.editor;
                      inputs->configure_form_reset(
                          input.component, [this, id, editor = input.editor, value = props.value_] {
                              // Reading a Binding invokes user code and may destroy
                              // this owner. Never keep its state across that read.
                              const auto accepted = value ? read_prop(*value) : TimePickerValue{};
                              auto* live = services_->components().state<TimePickerState>(id);
                              if (!live || live->editor != editor || !services_->components().scope(id).active()) {
                                  return;
                              }
                              if (value) {
                                  live->model.reconcile(accepted);
                              } else {
                                  live->model.cancel();
                              }
                              ++live->edit_session;
                              refresh(id);
                          });
                  }});
            s.view.popup = group(services, layout::LeafLayout{}, true);
            const auto popup_node = build.root(s.view.popup);
            services.components().set_window_layer(s.view.popup,
                                                   build.theme_scope()->snapshot().time_picker().z_index_popup);
            s.popup_hit = services.interactions().create({s.view.popup, popup_node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(s.view.popup, runtime::SceneFragmentPlacement::before_children);
            std::array<graphics::QuadInstance, 6> initial{};
            for (auto& layer : initial) {
                layer.opacity = 0;
            }
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.background =
                services.surfaces().create(s.view.popup, popup_node, fragment, s.popup_hit, initial, effects);
            build.on_resource_cleanup(s.view.popup, [&services, hit = s.popup_hit, surface = s.background] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            build.mount_slot(
                s.view.popup, Content{[&] {
                    for (int column = 0; column < 5; ++column) {
                        s.view.columns[column] = group(services, layout::LeafLayout{}, true);
                        if (s.model.column_visible(static_cast<TimeColumn>(column))) {
                            build.mount_slot(s.view.columns[column],
                                             Content{[this, id, column] { mount_rows(id, column); }});
                        }
                    }
                    if (props.panel_) {
                        s.custom_panel = group(services, layout::BoxLayout{}, true);
                        build.mount_slot(s.custom_panel, Content{SlotContentAccess::function(*props.panel_)});
                    }
                    layout::FlexLayout footer_layout;
                    footer_layout.direction = layout::FlexDirection::vertical;
                    footer_layout.item_policy = layout::FlexItemPolicy::sequential;
                    footer_layout.main_gap = build.theme_scope()->snapshot().time_picker().metrics.footer_padding;
                    s.view.footer = group(services, footer_layout, true);
                    build.mount_slot(
                        s.view.footer, Content{[&] {
                            if (footer) {
                                SlotContentAccess::function (*footer)();
                            }
                            for (const auto& preset : props.presets_) {
                                Button(ButtonProps{}.size(ControlSize::Small).onClick([this, id, fn = preset.value] {
                                    try {
                                        const auto value = fn();
                                        if (services_->components().contains(id)) {
                                            publish(id, value, true);
                                        }
                                    } catch (...) {
                                        static_cast<void>(services_->retire_failed_component(id));
                                        throw;
                                    }
                                }),
                                       ButtonContent{[label = preset.label, message = preset.label_message,
                                                      args = preset.label_args] {
                                           Text(TextProps{}.content(message
                                                                        ? Prop<String>{useI18n().text(*message, args)}
                                                                        : Prop<String>{label}));
                                       }});
                            }
                            if (props.clock_) {
                                Button(ButtonProps{}
                                           .size(ControlSize::Small)
                                           .disabled(bind([show = props.show_now_] { return !read_prop(show); }))
                                           .onClick([this, id, clock = props.clock_] {
                                               try {
                                                   const auto value = clock();
                                                   if (services_->components().contains(id)) {
                                                       publish(id, value, true);
                                                   }
                                               } catch (...) {
                                                   static_cast<void>(services_->retire_failed_component(id));
                                                   throw;
                                               }
                                           }),
                                       ButtonContent{[label = bind([locale = locale_source] {
                                                          return read_prop(locale).now;
                                                      })] { Text(TextProps{}.content(label)); }});
                            }
                        }});
                    layout::FlexLayout actions;
                    actions.main_gap = 8;
                    actions.item_policy = layout::FlexItemPolicy::sequential;
                    s.actions = group(services, actions, true);
                    build.mount_slot(
                        s.actions, Content{[&] {
                            s.view.cancel = group(services, layout::BoxLayout{}, true);
                            build.mount_slot(s.view.cancel, Content{[&] {
                                                 Button(ButtonProps{}.size(ControlSize::Small).onClick([this, id] {
                                                     request_open(id, false, true);
                                                 }),
                                                        ButtonContent{[locale = locale_source] {
                                                            Text(TextProps{}.content(
                                                                bind([locale] { return read_prop(locale).cancel; })));
                                                        }});
                                             }});
                            s.view.confirm = group(services, layout::BoxLayout{}, true);
                            build.mount_slot(s.view.confirm, Content{[&] {
                                                 Button(ButtonProps{}
                                                            .type(ButtonType::Primary)
                                                            .size(ControlSize::Small)
                                                            .disabled(s.confirm_disabled)
                                                            .onClick([this, id] { confirm(id); }),
                                                        ButtonContent{[locale = locale_source] {
                                                            Text(TextProps{}.content(
                                                                bind([locale] { return read_prop(locale).confirm; })));
                                                        }});
                                             }});
                        }});
                }});
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                        const auto* s = services_->components().state<TimePickerState>(id);
                        return engine.measure_child(services_->components().root(s->view.selector), constraints);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        const auto* s = services_->components().state<TimePickerState>(id);
                        engine.place_child(services_->components().root(s->view.selector), bounds);
                    }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id);
    update_theme(id, true);
    auto& scope = build.scope(id);
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](TimePickerValue value) {
            if (auto* s = services_->components().state<TimePickerState>(id)) {
                if (s->on_request) {
                    s->model.reconcile_candidate(value);
                    if (s->submitting != s->edit_session) {
                        s->model.finish_edit();
                    }
                } else {
                    const bool echoed = s->echo && s->echo->second == s->edit_session && s->echo->first == value;
                    if (!echoed) {
                        ++s->edit_session;
                    }
                    s->model.reconcile(value);
                }
                refresh(id);
            }
        });
    }
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool value) {
            if (auto* s = services_->components().state<TimePickerState>(id)) {
                s->open = value;
                s->suppressed = false;
                if (s->submitting != s->edit_session && (!value || !active_edit(*services_, *s))) {
                    if (!value && !s->on_request) {
                        ++s->edit_session;
                    }
                    s->model.cancel();
                }
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.rules_, [this, id](TimePickerRules rules) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->model.configure(std::move(rules), s->model.precision());
            refresh(id);
        }
    });
    connect_prop(scope, props.precision_, [this, id](TimePickerPrecision precision) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            if (active_edit(*services_, *s)) {
                s->model.freeze_edit();
            }
            s->model.configure(s->model.rules(), precision);
            if (precision == TimePickerPrecision::Minute && s->view.active_column == 2) {
                s->view.active_column = 1;
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->disabled = value;
            if (value) {
                ++s->edit_session;
                suppress(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.allow_clear_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->allow_clear = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.need_confirm_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->need_confirm = value;
            refresh(id);
        }
    });
    connect_prop(scope, locale_source, [this, id](PickerLocale v) {
        v.validate();
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->locale = std::move(v);
            refresh(id);
        }
    });
    connect_prop(scope, display_format_source, [this, id](TimeDisplayFormat v) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->model.configure_display(v.twelve, v.pattern, v.locale, active_edit(*services_, *s));
            s->twelve = v.twelve;
            s->format = std::move(v.pattern);
            s->format_locale = std::move(v.locale);
            refresh(id);
        }
    });
    connect_prop(scope, props.change_on_scroll_, [this, id](bool v) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->change_on_scroll = v;
        }
    });
    connect_prop(scope, props.direction_, [this, id](TextDirection v) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->direction = v;
            refresh(id);
        }
    });
    connect_prop(scope, props.placement_, [this, id](PickerPlacement v) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->placement = v;
            refresh(id);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::time_picker_colors, &theme_runtime::ThemeScope::time_picker_metrics,
        &theme_runtime::ThemeScope::time_picker_effects, &theme_runtime::ThemeScope::time_picker_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
                           [theme, accessor = accessors[i], input_effects = i == 0] {
                               static_cast<void>((theme.get()->*accessor)());
                               if (input_effects) {
                                   static_cast<void>(theme->input_shadows());
                               }
                           });
    }
}

void TimePickerComponentHost::mount_range(const TimeRangePickerProps& props,
                                          const std::optional<TimePickerFooter>& footer) {
    const auto locale_source =
        picker_locale_prop(props.picker_.locale_, runtime::require_component_build_context().config_scope());
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("TimeRangePicker controlled/default conflict");
    }
    validate_range_presets(props.presets_);
    auto state = std::make_shared<PickerRangeState<TimeOfDay>>();
    state->controlled = props.value_.has_value();
    state->controlled_open = props.open_.has_value();
    state->controlled_endpoint = props.endpoint_.has_value();
    state->policy = read_prop(props.policy_);
    state->policy_source.set(state->policy);
    state->accepted.set(props.value_ ? read_prop(*props.value_)
                                     : props.default_value_.value_or(TimePickerRangeValue{}));
    state->pending.set(state->accepted.get());
    state->open.set(props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false));
    state->reconcile_endpoint(props.endpoint_
                                  ? read_prop(*props.endpoint_)
                                  : (state->policy.disabled[0] ? PickerEndpoint::End : PickerEndpoint::Start));
    state->disabled = [weak = std::weak_ptr{state}, custom = props.disabled_time_, rules = props.picker_.rules_,
                       precision = props.picker_.precision_](TimeOfDay time, PickerEndpoint endpoint,
                                                             const TimePickerRangeValue& range,
                                                             const PickerCandidateSemantics& semantics) {
        const auto owner = weak.lock();
        if (!owner || !owner->alive) {
            return true;
        }
        const auto ticket = owner->revision;
        TimePickerModel model{read_prop(rules), semantics.time.value_or(read_prop(precision)), {}, true};
        model.owner_guard([weak, ticket] {
            const auto live = weak.lock();
            return live && live->current(ticket);
        });
        return !model.allowed(time) || !owner->current(ticket) || (custom && custom(time, endpoint, range));
    };
    state->on_change = props.on_change_;
    state->on_calendar = props.on_calendar_;
    state->on_open = props.on_open_;
    state->on_endpoint = props.on_endpoint_;
    auto& build = runtime::require_component_build_context();
    layout::FlexLayout layout;
    layout.main_gap = 8;
    layout.wrap = layout::FlexWrap::wrap;
    layout.right_to_left = read_prop(props.picker_.direction_) == TextDirection::RightToLeft;
    layout.item_policy = layout::FlexItemPolicy::sequential;
    const auto owner = group(*services_, layout);
    const auto node = build.root(owner);
    runtime::connect_layout_style(build.scope(owner), props.layout_, node, services_->nodes(), services_->dirty());
    connect_prop(build.scope(owner), props.picker_.direction_, [this, node, layout](TextDirection direction) mutable {
        layout.right_to_left = direction == TextDirection::RightToLeft;
        services_->layout().set_layout(node, layout);
        services_->dirty().invalidate(node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    });
    build.on_resource_cleanup(owner, [state] { state->alive = false; });
    state->retire = [this, owner] {
        static_cast<void>(services_->retire_failed_component(owner));
    };
    if (props.value_) {
        connect_prop(build.scope(owner), *props.value_,
                     [state](TimePickerRangeValue v) { state->reconcile_authority(v); });
    }
    if (props.open_) {
        connect_prop(build.scope(owner), *props.open_, [state](bool v) {
            state->invalidate();
            const auto ticket = state->revision;
            state->open.set(v);
            if (!state->current(ticket)) {
                return;
            }
            if (!v) {
                const bool restored = state->restore_pending();
                if (restored && state->end_editors) {
                    state->end_editors();
                }
            }
        });
    }
    if (props.endpoint_) {
        connect_prop(build.scope(owner), *props.endpoint_, [state](PickerEndpoint v) { state->reconcile_endpoint(v); });
    }
    connect_prop(build.scope(owner), props.policy_, [state](PickerRangePolicy v) {
        const bool disabling =
            (!state->policy.disabled[0] && v.disabled[0]) || (!state->policy.disabled[1] && v.disabled[1]);
        state->policy = v;
        state->invalidate();
        if (disabling) {
            if (!state->restore_pending()) {
                return;
            }
            const auto ticket = state->revision;
            if (state->end_editors) {
                state->end_editors();
            }
            if (!state->current(ticket)) {
                return;
            }
        }
        state->policy_source.set(v);
    });
    connect_range_business(build.scope(owner), state, props.disabled_);
    connect_range_business(build.scope(owner), state, props.picker_.disabled_);
    connect_range_business(build.scope(owner), state, props.picker_.rules_);
    build.mount_slot(
        owner, Content{[&, state] {
            for (auto endpoint : {PickerEndpoint::Start, PickerEndpoint::End}) {
                auto picker = props.picker_;
                picker.range_endpoint_ = endpoint;
                picker.autoFocus(picker.auto_focus_ && state->endpoint.get() == endpoint);
                const auto base_rules = picker.rules_;
                picker.rules(bind([state, base_rules, endpoint, custom = props.disabled_time_] {
                    auto rules = read_prop(base_rules);
                    const auto existing = rules.disabled;
                    static_cast<void>(state->pending.get());
                    rules.disabled = [state, existing, endpoint, custom](TimeOfDay v) {
                        auto draft = state->pending.get();
                        (endpoint == PickerEndpoint::Start ? draft.start : draft.end) = v;
                        return (existing && existing(v)) || !state->alive || (custom && custom(v, endpoint, draft));
                    };
                    return rules;
                }));
                picker.default_value_.reset();
                picker.default_open_.reset();
                picker.value(bind([state, endpoint] {
                    const auto v = state->pending.get();
                    return endpoint == PickerEndpoint::Start ? v.start : v.end;
                }));
                picker.open(bind([state, endpoint] { return state->open.get() && state->endpoint.get() == endpoint; }));
                picker.disabled(bind([state, endpoint, disabled = props.disabled_, common = picker.disabled_] {
                    return read_prop(disabled) || read_prop(common) ||
                           state->policy_source.get().disabled[static_cast<std::size_t>(endpoint)];
                }));
                picker.ref(endpoint == PickerEndpoint::Start ? props.ref_.start : props.ref_.end);
                picker.on_change_ = {};
                picker.on_ok_ = {};
                picker.on_request_ = [this, state, endpoint](TimePickerValue v, const PickerCandidate& candidate) {
                    return state->choose(endpoint, v, range_candidate(*services_, state, candidate));
                };
                picker.onOpenChange([this, state, endpoint](bool v) {
                    const auto candidate = range_candidate(*services_, state);
                    if (v) {
                        if (state->activate(endpoint, candidate) == PickerRequestDisposition::Consumed) {
                            state->request_open(true, candidate);
                        }
                    } else if (state->alive && state->open.get() && state->endpoint.get() == endpoint) {
                        state->request_open(false, candidate);
                    }
                });
                TimePicker(
                    std::move(picker), TimePickerFooter{[this, state, footer, presets = props.presets_, locale_source] {
                        if (footer) {
                            SlotContentAccess::function (*footer)();
                        }
                        for (const auto& preset : presets) {
                            Button(ButtonProps{}.size(ControlSize::Small).onClick([this, state, preset] {
                                state->preset(preset, range_candidate(*services_, state));
                            }),
                                   ButtonContent{[label = preset.label, message = preset.label_message,
                                                  args = preset.label_args] {
                                       Text(TextProps{}.content(message ? Prop<String>{useI18n().text(*message, args)}
                                                                        : Prop<String>{label}));
                                   }});
                        }
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([this, state] {
                            state->clear(range_candidate(*services_, state));
                        }),
                               ButtonContent{[locale_source] {
                                   Text(TextProps{}.content(
                                       bind([locale_source] { return read_prop(locale_source).clear_range; })));
                               }});
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([this, state] {
                            static_cast<void>(state->commit(range_candidate(*services_, state)));
                        }),
                               ButtonContent{[locale_source] {
                                   Text(TextProps{}.content(
                                       bind([locale_source] { return read_prop(locale_source).confirm_range; })));
                               }});
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([this, state] {
                            const auto result = state->request_open(false, range_candidate(*services_, state));
                            if (result == PickerRequestDisposition::Consumed && state->end_editors) {
                                state->end_editors();
                            }
                        }),
                               ButtonContent{[locale_source] {
                                   Text(TextProps{}.content(
                                       bind([locale_source] { return read_prop(locale_source).cancel_range; })));
                               }});
                    }});
                const auto child = mounted_.back();
                state->owners.push_back(child);
                state->inputs.push_back(services_->components().state<TimePickerState>(child)->view.selector);
                state->references.push_back(services_->components().state<TimePickerState>(child)->reference);
            }
        }});
    connect_range_reset(*services_, state, [this, weak = std::weak_ptr{state}] {
        const auto state = weak.lock();
        if (!state || !state->alive) {
            return;
        }
        const auto accepted = state->accepted.get();
        const auto ticket = state->revision;
        for (std::size_t i = 0; i < state->owners.size(); ++i) {
            if (!state->current(ticket)) {
                return;
            }
            if (auto* live = services_->components().state<TimePickerState>(state->owners[i])) {
                live->model.reconcile(i == 0 ? accepted.start : accepted.end);
                ++live->edit_session;
                refresh(state->owners[i]);
            }
        }
    });
}

void TimePickerComponentHost::on_window_active(bool active) {
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!active) {
            suppress(id);
        }
        refresh(id);
    }
}

void TimePickerComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<TimePickerState>(id);
        if (s && s->open && !s->suppressed && !services_->interactions().component_active(id)) {
            suppress(id);
        } else {
            refresh(id, false);
        }
    }
}

void TimePickerComponentHost::reorder_rows(runtime::ComponentId id) {
    const auto* state = services_->components().state<TimePickerState>(id);
    if (!state) {
        return;
    }
    std::vector<runtime::ComponentId> rows;
    for (const auto& column : state->view.rows) {
        rows.insert(rows.end(), column.begin(), column.end());
    }
    std::vector<std::vector<input::InteractionId>> blocks(rows.size());
    for (auto interaction : services_->interactions().declaration_order()) {
        const auto* record = services_->interactions().find(interaction);
        if (!record) {
            continue;
        }
        for (auto ancestor = std::optional{record->component}; ancestor && *ancestor != id;
             ancestor = services_->components().parent(*ancestor)) {
            const auto found = std::ranges::find(rows, *ancestor);
            if (found != rows.end()) {
                blocks[static_cast<std::size_t>(found - rows.begin())].push_back(interaction);
                break;
            }
        }
    }
    std::vector<input::InteractionId> order;
    for (const auto& block : blocks) {
        order.insert(order.end(), block.begin(), block.end());
    }
    services_->interactions().reorder_after(state->popup_hit, order);
}

void TimePickerComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    bool mounting = false;
    try {
        mounting = &runtime::require_component_build_context().host() == &services_->components();
    } catch (const std::logic_error&) {
    }
    for (auto id : ids) {
        auto* s = services_->components().state<TimePickerState>(id);
        if (s && s->open && !s->suppressed && !services_->interactions().component_active(id)) {
            suppress(id);
        } else if (s && s->view.visible) {
            const auto focused = services_->focus().state().focused;
            const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
            const bool within = target && (descendant(*services_, target->component, id) ||
                                           range_sibling(*services_, id, *s, target->component));
            const bool left = s->focus_within && !within;
            s->focus_within = within;
            if (left) {
                request_open(id, false);
            }
        }
        s = services_->components().state<TimePickerState>(id);
        if (!s || !s->rows_pending || s->updating_rows) {
            continue;
        }
        if (mounting) {
            services_->request_frame();
            continue;
        }
        bool created = false;
        for (int column = 0; column < 5; ++column) {
            s = services_->components().state<TimePickerState>(id);
            if (!s) {
                break;
            }
            if (s->model.column_visible(static_cast<TimeColumn>(column)) && s->view.rows[column].empty()) {
                const auto parent = s->view.columns[column];
                const auto context = services_->components().capture_context(parent);
                services_->append_slot(parent, Content{[this, id, column] { mount_rows(id, column); }}, context);
                s = services_->components().state<TimePickerState>(id);
                created |= s && !s->view.rows[column].empty();
            }
        }
        if (services_->components().contains(id)) {
            if (created) {
                reorder_rows(id);
            }
            refresh(id);
        }
    }
}

bool TimePickerComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !window_active_) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (!record) {
        return false;
    }
    std::optional<runtime::ComponentId> owner;
    for (auto current = std::optional{record->component}; current; current = services_->components().parent(*current)) {
        if (services_->components().state<TimePickerState>(*current)) {
            owner = current;
            break;
        }
        // A Date selector lives in the parent's layer, but its open picker
        // owns Escape before the enclosing Time footer does.
        const auto dates = services_->date_picker().mounted();
        if (std::ranges::find(dates, *current) != dates.end() && services_->date_picker().snapshot(*current).visible) {
            return false;
        }
    }
    if (!owner) {
        return false;
    }
    auto* s = services_->components().state<TimePickerState>(*owner);
    if (!s || s->disabled || !services_->interactions().component_active(*owner)) {
        return false;
    }
    const auto layer = nearest_layer(*services_, record->component);
    const bool selector = *focused == s->selector_hit;
    if (!selector && layer != s->view.popup) {
        return false;
    }
    for (const auto& input : services_->input_runtime()->mounted_inputs()) {
        if (input.interaction == focused &&
            services_->input_runtime()->editors().require(input.editor).composition().active) {
            return false;
        }
    }
    using input::Key;
    if (event.key == Key::escape && s->view.visible) {
        if (services_->popover().owns_escape(*owner, record->component)) {
            return false;
        }
        request_open(*owner, false, true);
        return true;
    }
    // Footer content keeps native editor/navigation/activation behavior.
    if (!selector && *focused != s->popup_hit) {
        return false;
    }
    if (event.key == Key::tab) {
        return false;
    }
    if (event.modifiers != input::KeyModifier::none) {
        return false;
    }
    if (event.key == Key::enter) {
        if (!event.repeat) {
            if (s->view.visible || s->model.text_dirty()) {
                confirm(*owner);
            } else {
                request_open(*owner, true);
            }
        }
        return true;
    }
    if (event.key != Key::up && event.key != Key::down && event.key != Key::left && event.key != Key::right &&
        event.key != Key::home && event.key != Key::end) {
        return false;
    }
    if (!s->view.visible && event.key != Key::up && event.key != Key::down) {
        return false;
    }
    if (!s->view.visible) {
        request_open(*owner, true);
        return true;
    }
    if (event.key == Key::left || event.key == Key::right) {
        const int columns = visible_columns(s->model);
        static_cast<void>(columns);
        const int delta = (event.key == Key::left ? -1 : 1) * (s->direction == TextDirection::RightToLeft ? -1 : 1);
        for (int c = s->view.active_column + delta; c >= 0 && c < 5; c += delta) {
            if (s->model.column_visible(static_cast<TimeColumn>(c))) {
                s->view.active_column = c;
                break;
            }
        }
        refresh(*owner);
        return true;
    }
    const auto revision = s->revision;
    const auto session = s->edit_session;
    auto model = s->model;
    bool changed{};
    try {
        changed = model.navigate(static_cast<TimeColumn>(s->view.active_column),
                                 event.key == Key::up || event.key == Key::end ? -1 : 1,
                                 event.key == Key::home || event.key == Key::end);
    } catch (...) {
        static_cast<void>(services_->retire_failed_component(*owner));
        throw;
    }
    s = services_->components().state<TimePickerState>(*owner);
    if (!s || s->revision != revision) {
        return true;
    }
    s->model = std::move(model);
    const auto value = s->model.panel();
    const auto notify = s->on_select;
    refresh(*owner);
    if (!services_->components().contains(*owner)) {
        return true;
    }
    if (changed && notify) {
        picker_callback(*services_, *owner, notify, value);
    }
    s = services_->components().state<TimePickerState>(*owner);
    if (changed && s && !s->need_confirm && s->edit_session == session && !s->model.text_dirty() &&
        s->model.panel() == value) {
        publish(*owner, value, false);
    }
    return true;
}

void TimePickerComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<TimePickerState>(id);
        if (!s || s->disabled) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool within = record && (descendant(*services_, record->component, id) ||
                                       range_sibling(*services_, id, *s, record->component));
        if (event.action == input::PointerAction::down && !within) {
            request_open(id, false);
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary &&
                   hit == s->selector_hit && origin == hit) {
            request_open(id, true);
        }
    }
}

bool TimePickerComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (!std::isfinite(event.delta_y) || !std::ranges::any_of(mounted_, [&](auto id) {
            const auto* s = services_->components().state<TimePickerState>(id);
            return s && s->view.visible && inside(s->view.bounds, event.x, event.y);
        })) {
        return false;
    }
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    const auto layer = nearest_layer(*services_, target->component);
    for (auto id : mounted_) {
        auto* s = services_->components().state<TimePickerState>(id);
        if (!s || !s->view.visible || !services_->interactions().component_active(s->view.popup) ||
            layer != s->view.popup) {
            continue;
        }
        for (int column = 0; column < 5; ++column) {
            const auto clip = s->view.column_clips[column];
            if (!inside(clip, event.x, event.y)) {
                continue;
            }
            const auto& metrics = services_->components().theme_scope(id)->snapshot().time_picker().metrics;
            const int count = s->model.row_count(static_cast<TimeColumn>(column));
            const float content = static_cast<float>((count - 1) / step(s->model, column) + 1) * metrics.cell_height;
            const float next = std::clamp(s->view.scroll[column] - event.delta_y * metrics.cell_height * 3, 0.0F,
                                          std::max(0.0F, content - clip.height));
            if (s->change_on_scroll && event.delta_y != 0) {
                auto candidate = s->model;
                if (candidate.navigate(static_cast<TimeColumn>(column), event.delta_y > 0 ? -1 : 1)) {
                    choose(id, static_cast<TimeColumn>(column), candidate.row_value(static_cast<TimeColumn>(column)));
                }
                return true;
            }
            if (next != s->view.scroll[column]) {
                s->view.scroll[column] = next;
                s->reveal[column] = false;
                s->needs_layout = true;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            }
            return true;
        }
    }
    return false;
}

bool TimePickerComponentHost::consume_scroll_boundary(const input::ScrollInputEvent& event) const {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    const auto layer = nearest_layer(*services_, target->component);
    return std::ranges::any_of(mounted_, [&](auto id) {
        const auto* s = services_->components().state<TimePickerState>(id);
        return s && s->view.visible && layer == s->view.popup;
    });
}

void TimePickerComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        position_window_layer(id, viewport, clip);
    }
}

void TimePickerComponentHost::position_window_layer(runtime::ComponentId id, runtime::Size viewport,
                                                    runtime::Rect clip) {
    if (std::ranges::find(positioning_, id) != positioning_.end()) {
        return;
    }
    positioning_.push_back(id);

    struct PositionGuard final {
        std::vector<runtime::ComponentId>& stack;

        ~PositionGuard() {
            stack.pop_back();
        }
    } guard{positioning_};

    services_->popover().position_ancestors(id, viewport, clip);
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !s->view.visible) {
        return;
    }
    auto& engine = services_->layout();
    const auto& node = services_->nodes().require(s->node);
    const runtime::Rect anchor{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                               node.bounds.width, node.bounds.height};
    const auto popup_node = services_->components().root(s->view.popup);
    if (!s->needs_layout && anchor == s->anchor && clip == s->clip &&
        services_->nodes().require(popup_node).measure_generation == engine.generation()) {
        return;
    }
    const auto& m = services_->components().theme_scope(id)->snapshot().time_picker().metrics;
    const int columns = visible_columns(s->model);
    const float width = std::min(clip.width, std::max(anchor.width, m.column_width * columns + m.padding * 2));
    const float inner_width = std::max(0.0F, width - std::min(m.padding * 2, width));
    const auto footer_node = services_->components().root(s->view.footer);
    const auto actions_node = services_->components().root(s->actions);
    const auto footer_size = engine.measure_child(footer_node, {0, inner_width, 0, clip.height});
    const auto actions_size =
        s->need_confirm ? engine.measure_child(actions_node, {0, inner_width, 0, clip.height}) : runtime::Size{};
    const float footer_height = footer_size.height > 0 ? footer_size.height + m.footer_padding * 2 : 0;
    const float actions_height = s->need_confirm ? actions_size.height + m.footer_padding * 2 : 0;
    const float wanted = m.column_height + m.padding * 2 + footer_height + actions_height;
    const float below = std::clamp(clip.y + clip.height - anchor.y - anchor.height - m.gap, 0.0F, clip.height);
    const float above = std::clamp(anchor.y - m.gap - clip.y, 0.0F, clip.height);
    const bool prefer_top = s->placement == PickerPlacement::TopLeft || s->placement == PickerPlacement::TopRight;
    const bool upwards = prefer_top ? !(above < wanted && below > above) : below < wanted && above > below;
    const bool right = (s->placement == PickerPlacement::BottomRight || s->placement == PickerPlacement::TopRight) !=
                       (s->direction == TextDirection::RightToLeft);
    const float height = std::min(wanted, upwards ? above : below);
    s->view.bounds = {
        std::clamp(right ? anchor.x + anchor.width - width : anchor.x, clip.x, clip.x + clip.width - width),
        std::clamp(upwards ? anchor.y - m.gap - height : anchor.y + anchor.height + m.gap, clip.y,
                   clip.y + clip.height - height),
        width, height};
    static_cast<void>(engine.measure_child(popup_node, layout::Constraints::fixed(width, height)));
    engine.place_child(popup_node, s->view.bounds);
    const float inset = std::min(m.padding, std::min(width, height) / 2);
    const runtime::Rect inner{s->view.bounds.x + inset, s->view.bounds.y + inset, std::max(0.0F, width - inset * 2),
                              std::max(0.0F, height - inset * 2)};
    const float rows_height = std::max(0.0F, inner.height - footer_height - actions_height);
    int physical_column{};
    for (int column = 0; column < 5; ++column) {
        const int physical =
            s->direction == TextDirection::RightToLeft ? columns - 1 - physical_column : physical_column;
        auto rect =
            runtime::Rect{inner.x + inner.width * physical / columns, inner.y, inner.width / columns, rows_height};
        if (!s->model.column_visible(static_cast<TimeColumn>(column)) || s->panel) {
            rect = {};
        } else {
            ++physical_column;
        }
        s->view.column_clips[column] = rect;
        const auto column_node = services_->components().root(s->view.columns[column]);
        static_cast<void>(engine.measure_child(column_node, layout::Constraints::fixed(rect.width, rect.height)));
        engine.place_child(column_node, rect);
        const int count = static_cast<int>(s->view.rows[column].size());
        const int stride = step(s->model, column);
        const float content = static_cast<float>((count - 1) / stride + 1) * m.cell_height;
        auto& scroll = s->view.scroll[column];
        const float maximum = std::max(0.0F, content - rows_height);
        scroll = std::clamp(scroll, 0.0F, maximum);
        if (s->reveal[column]) {
            const float top =
                static_cast<float>(s->model.row_value(static_cast<TimeColumn>(column)) / stride) * m.cell_height;
            scroll = std::clamp(rows_height < m.cell_height
                                    ? top
                                    : std::clamp(scroll, std::max(0.0F, top + m.cell_height - rows_height), top),
                                0.0F, maximum);
        }
        s->reveal[column] = false;
        for (int value = 0; value < count; value += stride) {
            const auto* row = services_->components().state<TimeRow>(s->view.rows[column][value]);
            const float padding = std::min(m.padding, rect.width / 2);
            engine.set_layout(row->node, layout::BoxLayout{{padding, 0, padding, 0}});
            static_cast<void>(engine.measure_child(row->node, layout::Constraints::fixed(rect.width, m.cell_height)));
            engine.place_child(row->node,
                               {rect.x, rect.y + value / stride * m.cell_height - scroll, rect.width, m.cell_height});
        }
    }
    if (s->panel) {
        const auto custom = services_->components().root(s->custom_panel);
        static_cast<void>(engine.measure_child(custom, layout::Constraints::fixed(inner.width, rows_height)));
        engine.place_child(custom, {inner.x, inner.y, inner.width, rows_height});
    }
    const float footer_top = inner.y + rows_height;
    const float fp = std::min(m.footer_padding, inner.width / 2);
    const runtime::Rect footer_rect{inner.x + fp, footer_top + std::min(fp, footer_height / 2),
                                    std::max(0.0F, inner.width - fp * 2), std::max(0.0F, footer_height - fp * 2)};
    static_cast<void>(
        engine.measure_child(footer_node, layout::Constraints::fixed(footer_rect.width, footer_rect.height)));
    engine.place_child(footer_node, footer_rect);
    const runtime::Rect actions_rect{inner.x + fp, footer_top + footer_height + std::min(fp, actions_height / 2),
                                     std::max(0.0F, inner.width - fp * 2), std::max(0.0F, actions_height - fp * 2)};
    static_cast<void>(
        engine.measure_child(actions_node, layout::Constraints::fixed(actions_rect.width, actions_rect.height)));
    engine.place_child(actions_node, actions_rect);
    s->needs_layout = false;
    s->anchor = anchor;
    s->clip = clip;

    if (std::ranges::any_of(services_->table().mounted(), [&](auto table) {
            const auto* live = services_->components().state<TimePickerState>(id);
            return live && live->view.visible && descendant(*services_, table, live->view.footer);
        })) {
        services_->table().position_window_layers(viewport, clip);
    }
}

void TimePickerComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect window{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* s = services_->components().state<TimePickerState>(id);
        if (!s || !s->view.visible) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().time_picker();
        auto background = quad(s->view.bounds, t.colors.background, t.metrics.border_radius);
        background.opacity = s->transition ? s->transition->value() : 1;
        std::array<graphics::QuadInstance, 6> panel{};
        panel[0] = background;
        const int count = visible_columns(s->model);
        const auto b = s->view.bounds;
        const float border = std::min(t.metrics.border_width, std::min(b.width, b.height) / 2);
        for (int column = 1; column < count; ++column) {
            const auto c = s->view.column_clips[column];
            panel[column] = quad(intersect({c.x, c.y, border, c.height}, b), t.colors.border);
        }
        const auto c = s->view.column_clips[0];
        panel[5] = quad(intersect({b.x, c.y + c.height, b.width, border}, b), t.colors.border);
        static_cast<void>(services_->surfaces().update_surface(s->background, panel));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{s->view.bounds.x, s->view.bounds.y, s->view.bounds.width, s->view.bounds.height},
                         background.corner_radius};
        effects.shadows = t.effects.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, window};
        static_cast<void>(services_->surfaces().update_effects(s->background, effects));
        for (int column = 0; column < 5; ++column) {
            const auto clip = s->view.column_clips[column];
            for (std::size_t value = 0; value < s->view.rows[column].size(); ++value) {
                const auto* row = services_->components().state<TimeRow>(s->view.rows[column][value]);
                const bool selected = static_cast<int>(value) == s->model.row_value(static_cast<TimeColumn>(column));
                const auto bounds = intersect(services_->nodes().require(row->node).bounds, clip);
                auto item = quad(bounds, selected ? t.colors.selected_background : t.colors.hover_background,
                                 t.metrics.border_radius);
                item.opacity = selected || (row->enabled && row->hovered) ? 1.0F : 0.0F;
                static_cast<void>(services_->surfaces().update_surface(row->surface, std::span{&item, 1}));
                services_->scene_composer().set_fragment(row->hit, {}, row->interaction, clip);
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void TimePicker(TimePickerProps props, std::optional<TimePickerFooter> footer) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_time_pickers) {
        throw std::logic_error("TimePicker requires window component services");
    }
    detail::active_time_pickers->mount(props, footer);
}

void TimeRangePicker(TimeRangePickerProps props, std::optional<TimePickerFooter> footer) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_time_pickers) {
        throw std::logic_error("TimeRangePicker requires window component services");
    }
    detail::active_time_pickers->mount_range(props, footer);
}
} // namespace ryn
