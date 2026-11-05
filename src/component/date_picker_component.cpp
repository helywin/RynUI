#include "component/date_picker_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/date_picker_model.hpp"
#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "component/table_component.hpp"
#include "component/popover_component.hpp"
#include "component/navigation_transition.hpp"
#include "component/picker_range.hpp"
#include "component/picker_format.hpp"
#include "component/time_picker_component.hpp"
#include <ryn/button.hpp>
#include <set>
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local DatePickerComponentHost* active_dates{};

struct DateItem final {
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    DatePickerValue date;
    int months{};
    Signal<DatePickerCellInfo> info{DatePickerCellInfo{}};
    Signal<String> text{String{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
};

struct DateState final {
    explicit DateState(DatePickerModel value) : model(std::move(value)) {}

    DatePickerModel model;
    DatePickerRules rules;
    std::unique_ptr<NavigationTransition> transition;
    DatePickerMode picker{DatePickerMode::Date};
    DatePickerMode mode{DatePickerMode::Date};
    PickerLocale locale;
    std::optional<PickerEndpoint> range_endpoint;
    String format;
    std::vector<String> input_formats;
    TextDirection direction{TextDirection::LeftToRight};
    PickerPlacement placement{PickerPlacement::BottomLeft};
    bool controlled_mode{};
    bool controlled_panel{};
    DatePickerValue panel_authority;
    bool multiple{};
    bool controlled_multiple{};
    bool need_confirm{};
    bool show_time{};
    String time_format{u8"HH:mm:ss"};
    Prop<TimePickerRules> time_rules{TimePickerRules{}};
    Prop<TimePickerPrecision> time_precision{TimePickerPrecision::Second};
    bool controlled_time{};
    DatePickerMultipleValue accepted_multiple;
    DatePickerMultipleValue pending_multiple;
    DatePickerDateTimeValue accepted_time;
    DatePickerValue pending_date;
    Signal<DatePickerValue> time_date{DatePickerValue{}};
    Signal<TimePickerValue> pending_time{TimePickerValue{TimeOfDay{}}};
    std::function<bool(CivilDate, TimeOfDay)> disabled_time;
    std::function<void(DatePickerValue)> on_ok;
    std::function<void(DatePickerDateTimeValue)> on_time_ok;
    std::function<void(DatePickerMultipleValue)> on_multiple;
    std::function<void(DatePickerDateTimeValue)> on_time;
    std::function<void(CivilDate, DatePickerMode)> on_panel;
    DatePickerCellRenderer cell_render;
    std::optional<DatePickerPanel> panel;
    std::optional<DatePickerHeader> header;
    runtime::ComponentId custom_panel;
    runtime::ComponentId custom_header;
    runtime::ComponentId confirm_group;
    runtime::ComponentId time_picker;
    TimePickerRef time_reference;
    runtime::NodeId node;
    runtime::ComponentId input;
    runtime::ComponentId popup;
    runtime::ComponentId footer;
    std::vector<runtime::ComponentId> items;
    input::InteractionId input_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    DatePickerRef reference;
    Signal<String> display{String{}};
    DatePickerValue hovered;
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<ThemeConfig> input_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    bool dirty_draft{};
    // Pointer focus/blur runs before the outside-pointer observer in the same dispatch.
    bool closed_on_blur{};
    bool disabled{};
    bool allow_clear{true};
    bool controlled_open{};
    bool open{};
    bool suppressed{};
    bool visible{};
    bool needs_layout{true};
    runtime::Rect bounds;
    runtime::Rect clip;
    runtime::Rect anchor;
    std::function<void(DatePickerValue)> on_change;
    std::function<void(DatePickerValue)> on_request;
    std::function<void(DatePickerDateTimeValue)> on_time_request;
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

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (*p == parent) {
            return true;
        }
    }
    return false;
}

std::optional<runtime::ComponentId> layer(WindowComponentServices& services, runtime::ComponentId child) {
    for (auto p = std::optional{child}; p; p = services.components().parent(*p)) {
        if (services.components().is_window_layer_root(*p)) {
            return p;
        }
    }
    return {};
}

std::string_view pattern(const DateState& state) {
    return !state.format.empty()                     ? state.format.bytes()
           : state.picker == DatePickerMode::Week    ? "gggg-[W]ww"
           : state.picker == DatePickerMode::Month   ? "YYYY-MM"
           : state.picker == DatePickerMode::Quarter ? "YYYY-[Q]Q"
           : state.picker == DatePickerMode::Year    ? "YYYY"
                                                     : "YYYY-MM-DD";
}

std::string datetime_pattern(const DateState& state) {
    if (!state.format.empty()) {
        try {
            static_cast<void>(split_date_time_format(state.format.bytes()));
            return std::string{state.format.bytes()};
        } catch (const std::invalid_argument&) {
        }
    }
    return std::string{pattern(state)} + " " + std::string{state.time_format.bytes()};
}

int panel_step(DatePickerMode mode) {
    return mode == DatePickerMode::Date || mode == DatePickerMode::Week       ? 1
           : mode == DatePickerMode::Month || mode == DatePickerMode::Quarter ? 12
           : mode == DatePickerMode::Year                                     ? 120
                                                                              : 1200;
}

String display(const DateState& state, DatePickerValue value) {
    if (state.multiple) {
        std::string text;
        for (const auto& date : state.accepted_multiple) {
            if (!text.empty()) {
                text += ", ";
            }
            text += format_date(date, pattern(state), state.locale).bytes();
        }
        return String::from_utf8(text).value();
    }
    if (state.show_time) {
        return state.accepted_time ? format_date_time(*state.accepted_time, datetime_pattern(state), state.locale)
                                   : String{};
    }
    return value ? format_date(*value, pattern(state), state.locale) : String{};
}

bool selected(const DateState& state, DatePickerValue value) {
    if (!value) {
        return false;
    }
    const auto candidate = state.model.normalized(*value);
    if (state.multiple) {
        return std::ranges::any_of(state.pending_multiple,
                                   [&](CivilDate d) { return state.model.normalized(d) == candidate; });
    }
    const auto current = state.pending_date ? state.pending_date : state.model.value();
    return current && state.model.normalized(*current) == candidate;
}

runtime::ComponentId group(WindowComponentServices& services) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}
} // namespace

DatePickerComponentHost::DatePickerComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

DatePickerComponentHost::~DatePickerComponentHost() {
    services_->detach(*this);
}

void* DatePickerComponentHost::begin_mount() noexcept {
    auto* old = active_dates;
    active_dates = this;
    return old;
}

void DatePickerComponentHost::end_mount(void* old) noexcept {
    active_dates = static_cast<DatePickerComponentHost*>(old);
}

void DatePickerComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

DatePickerSnapshot DatePickerComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<DateState>(id);
    if (!s) {
        throw std::out_of_range("stale DatePicker");
    }
    std::vector<component::RetainedSurfaceId> surfaces{s->background};
    for (auto child : s->items) {
        surfaces.push_back(services_->components().state<DateItem>(child)->surface);
    }
    return {s->input,
            s->popup,
            s->footer,
            {s->items.begin() + 12, s->items.begin() + 12 + static_cast<std::ptrdiff_t>(s->model.cell_count())},
            {s->items.begin(), s->items.begin() + 4},
            s->model.value(),
            s->model.active(),
            s->display.get(),
            s->bounds,
            s->visible,
            std::move(surfaces),
            s->accepted_multiple,
            s->pending_multiple,
            s->accepted_time,
            s->pending_date,
            s->pending_time.get(),
            s->mode};
}

void DatePickerComponentHost::refresh(runtime::ComponentId id, bool geometry) try {
    auto* s = services_->components().state<DateState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible =
        s->open && !s->suppressed && !s->disabled && active_ && services_->interactions().component_active(id);
    const bool changed = visible != s->visible;
    s->visible = visible;
    if (s->transition && changed) {
        s->transition->restart(visible);
    }
    if (!visible) {
        s->hovered.reset();
    }
    if (services_->components().set_branch_active(s->popup, visible)) {
        services_->mark_scene_structure_dirty();
    }
    s->clear_visible.set(s->allow_clear && (s->model.value().has_value() || !s->accepted_multiple.empty() ||
                                            s->accepted_time.has_value()));
    s->action_disabled.set(s->disabled);
    if (s->confirm_group.valid() && services_->components().set_branch_active(s->confirm_group, s->need_confirm)) {
        services_->mark_scene_structure_dirty();
    }
    const auto t = services_->components().theme_scope(id)->snapshot().date_picker();
    const auto model = s->model;
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        auto* item = services_->components().state<DateItem>(s->items[i]);
        if (i == 4) {
            item->text.set(
                format_date(model.panel(),
                            s->mode == DatePickerMode::Year || s->mode == DatePickerMode::Decade ? "YYYY" : "MMMM YYYY",
                            s->locale));
        }
        const bool days = s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week;
        if (i < 4) {
            item->months = days     ? std::array{-12, -1, 1, 12}[i]
                           : i == 0 ? -panel_step(s->mode)
                           : i == 3 ? panel_step(s->mode)
                                    : 0;
        }
        if (i < 5 && services_->components().set_branch_active(s->items[i], !s->header && (days || i != 1 && i != 2))) {
            services_->mark_scene_structure_dirty();
        }
        const bool date = i >= 12;
        if (date) {
            item->date = model.cell(i - 12);
            const bool shown = i - 12 < model.cell_count() && !s->panel;
            if (services_->components().set_branch_active(s->items[i], shown)) {
                services_->mark_scene_structure_dirty();
            }
            item->text.set(
                item->date ? format_date(*item->date,
                                         s->mode == DatePickerMode::Month                                       ? "MMM"
                                         : s->mode == DatePickerMode::Quarter                                   ? "[Q]Q"
                                         : s->mode == DatePickerMode::Year || s->mode == DatePickerMode::Decade ? "YYYY"
                                                                                                                : "D",
                                         s->locale)
                           : String{});
        }
        if (i >= 5 && i < 12) {
            item->text.set(s->locale.weekdays[(i - 5 + s->locale.first_day) % 7]);
            if (services_->components().set_branch_active(
                    s->items[i], (s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week) && !s->panel)) {
                services_->mark_scene_structure_dirty();
            }
        }
        const bool enabled = date && item->date && model.eligible(*item->date);
        s = services_->components().state<DateState>(id);
        item = services_->components().state<DateItem>(s ? s->items[i] : runtime::ComponentId{});
        if (!s || !item) {
            return;
        }
        bool navigable = i < 4;
        if (navigable) {
            auto probe = model;
            navigable = item->months && probe.navigate(item->months);
        }
        services_->interactions().set_eligible(item->interaction,
                                               visible && (enabled || ((!s->header) && (navigable || i == 4))));
        const bool is_selected = date && selected(*s, item->date);
        item->info.set({item->date, s->mode, is_selected, !enabled, s->range_endpoint});
        s = services_->components().state<DateState>(id);
        if (!s) {
            return;
        }
        item = services_->components().state<DateItem>(s->items[i]);
        if (!item) {
            return;
        }
        const bool muted = date && item->date && item->date->month() != s->model.panel().month();
        item->color.set(foreground((date && !enabled) || (i < 4 && !navigable) ? t.colors.disabled
                                   : is_selected                               ? t.colors.selected_text
                                   : muted                                     ? t.colors.muted
                                                                               : t.colors.text));
        item->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
    }
    if (geometry || changed) {
        s->needs_layout = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    } else {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    if (changed && !visible) {
        const auto items = s->items;
        const auto hit = s->popup_hit;
        services_->pointer().cancel_interaction(hit);
        for (auto item : items) {
            if (auto* live = services_->components().state<DateItem>(item)) {
                services_->pointer().cancel_interaction(live->interaction);
            }
        }
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void DatePickerComponentHost::request_open(runtime::ComponentId id, bool open, bool restore) try {
    auto* s = services_->components().state<DateState>(id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || !active_ || !services_->interactions().component_active(id)))) {
        return;
    }
    const bool changed = s->open != open;
    const auto callback = s->on_open;
    const auto input = s->input_hit;
    if (!s->controlled_open) {
        s->open = open;
    }
    if (!open) {
        s->pending_date.reset();
        s->pending_multiple = s->accepted_multiple;
        s->pending_time.set(s->accepted_time ? s->accepted_time->time : TimeOfDay{});
        s->dirty_draft = false;
        s->display.set(display(*s, s->model.value()));
    }
    if (open) {
        s->suppressed = false;
    }
    refresh(id);
    if (changed && callback) {
        callback(open);
    }
    if (restore && services_->components().contains(id) && services_->interactions().component_active(id)) {
        services_->focus().defer_focus(input, input::FocusModality::keyboard);
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void DatePickerComponentHost::navigate(runtime::ComponentId id, int months) try {
    auto* s = services_->components().state<DateState>(id);
    if (!s) {
        return;
    }
    auto next = s->model;
    if (!next.navigate(months)) {
        return;
    }
    const auto date = next.panel();
    const auto mode = s->mode;
    const auto callback = s->on_panel;
    if (!s->controlled_panel) {
        s->model = std::move(next);
    }
    refresh(id);
    if (callback) {
        callback(date, mode);
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void DatePickerComponentHost::confirm(runtime::ComponentId id) try {
    const auto* s = services_->components().state<DateState>(id);
    if (s && s->dirty_draft) {
        commit(id, false);
        return;
    }
    if (s) {
        choose(id, s->pending_date ? s->pending_date : DatePickerValue{s->model.active()}, true, true);
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void DatePickerComponentHost::choose(runtime::ComponentId id, DatePickerValue value, bool close, bool confirmed) try {
    auto* s = services_->components().state<DateState>(id);
    if (!s || s->disabled || !active_ || !services_->interactions().component_active(id)) {
        return;
    }
    const auto* input = services_->interactions().find(s->input_hit);
    if (!input || !input->eligible || !services_->interactions().component_active(s->input)) {
        return;
    }
    auto model = s->model;
    if (value && !(s->multiple && confirmed) && !model.eligible(*value)) {
        return;
    }
    s = services_->components().state<DateState>(id);
    if (!s) {
        return;
    }
    if (value && s->mode != s->picker && !(s->picker == DatePickerMode::Week && s->mode == DatePickerMode::Date)) {
        const auto mode = s->mode == DatePickerMode::Decade ? DatePickerMode::Year
                          : s->mode == DatePickerMode::Year ? DatePickerMode::Month
                                                            : DatePickerMode::Date;
        const auto callback = s->on_panel;
        if (!s->controlled_panel) {
            s->model.panel_date(*value);
        }
        if (!s->controlled_mode) {
            s->mode = mode;
            s->model.view(mode, s->locale);
        }
        refresh(id);
        if (callback) {
            callback(*value, mode);
        }
        return;
    }
    if (value) {
        auto normalize = model;
        normalize.view(s->picker, s->locale);
        value = normalize.normalized(*value);
    }
    if (s->multiple && !value) {
        s->pending_multiple.clear();
    }
    if (s->multiple && !confirmed) {
        if (!value) {
            s->pending_multiple.clear();
        } else {
            auto& values = s->pending_multiple;
            const auto found = std::ranges::find_if(
                values, [&](CivilDate d) { return model.normalized(d) == model.normalized(*value); });
            if (found == values.end()) {
                values.push_back(*value);
            } else {
                values.erase(found);
            }
            std::ranges::sort(values);
        }
        if (s->need_confirm && value) {
            refresh(id);
            return;
        }
    }
    if (s->need_confirm && !confirmed && value) {
        s->pending_date = value;
        s->time_date.set(value);
        refresh(id);
        return;
    }
    if ((s->multiple || s->show_time) && !s->need_confirm && value && !confirmed) {
        close = false;
    }
    if (s->multiple) {
        const auto pending = s->pending_multiple;
        for (const auto& date : pending) {
            if (!model.eligible(date)) {
                return;
            }
        }
        s = services_->components().state<DateState>(id);
        if (!s) {
            return;
        }
    }
    const auto before = s->model.value();
    const auto before_multi = s->accepted_multiple;
    const auto before_time = s->accepted_time;
    const auto multi = s->pending_multiple;
    DatePickerDateTimeValue date_time;
    if (s->show_time && value) {
        const auto pending = s->pending_time.get();
        if (!pending) {
            return;
        }
        const auto time = *pending;
        const auto rules = s->time_rules;
        const auto precision = s->time_precision;
        TimePickerModel time_model{read_prop(rules), read_prop(precision), {}, true};
        if (!time_model.allowed(time)) {
            return;
        }
        s = services_->components().state<DateState>(id);
        if (!s) {
            return;
        }
        const auto disabled = s->disabled_time;
        if (disabled && disabled(*value, time)) {
            return;
        }
        s = services_->components().state<DateState>(id);
        if (!s) {
            return;
        }
        date_time = CivilDateTime{*value, time};
    }
    const auto intent = model.request(s->multiple ? DatePickerValue{} : value);
    s = services_->components().state<DateState>(id);
    if (!intent || !s) {
        return;
    }
    if (s->panel_authority) {
        model.panel_date(*s->panel_authority);
    }
    s->model = std::move(model);
    if (!s->controlled_multiple) {
        s->accepted_multiple = multi;
    }
    if (!s->controlled_time) {
        s->accepted_time = date_time;
    }
    const auto callback = s->on_change;
    const auto request = s->on_request;
    const auto time_request = s->on_time_request;
    if (request || time_request) {
        close = false;
    }
    const auto callback_multi = s->on_multiple;
    const auto callback_time = s->on_time;
    const auto callback_ok = s->on_ok;
    const auto callback_time_ok = s->on_time_ok;
    const bool multiple = s->multiple;
    const bool time = s->show_time;
    s->pending_date.reset();
    s->dirty_draft = false;
    s->display.set(display(*s, s->model.value()));
    refresh(id);
    if (!services_->components().contains(id)) {
        return;
    }
    if (time && time_request) {
        time_request(date_time);
    } else if (!time && request) {
        request(value);
    }
    if (!services_->components().contains(id)) {
        return;
    }
    if (multiple) {
        if (before_multi != multi && callback_multi) {
            callback_multi(multi);
        }
    } else if (time) {
        if (before_time != date_time && callback_time) {
            callback_time(date_time);
        }
    } else if (before != *intent && callback) {
        callback(*intent);
    }
    if (close && services_->components().contains(id)) {
        if (time && callback_time_ok) {
            callback_time_ok(date_time);
        } else if (!time && callback_ok) {
            callback_ok(value);
        }
    }
    if (close && services_->components().contains(id)) {
        request_open(id, false, true);
    }
} catch (...) {

    static_cast<void>(services_->retire_failed_component(id));
    throw;
}

void DatePickerComponentHost::commit(runtime::ComponentId id, bool blur) {
    auto* s = services_->components().state<DateState>(id);
    if (!s) {
        return;
    }
    const bool composing = services_->input_runtime()->editors().require(s->editor).composition().active;
    if (s->dirty_draft && !composing) {
        const auto draft = s->display.get();
        DatePickerValue value;
        if (s->show_time) {
            const auto date_time = parse_date_time(draft.bytes(), datetime_pattern(*s), s->locale);
            if (date_time) {
                value = date_time->date;
                s->pending_time.set(date_time->time);
            }
        } else {
            value = parse_date(draft.bytes(), pattern(*s), s->locale);
        }
        if (!value) {
            for (const auto& format : s->input_formats) {
                value = parse_date(draft.bytes(), format.bytes(), s->locale);
                if (value) {
                    break;
                }
            }
        }
        if (draft.empty() || value) {
            s->dirty_draft = false;
            choose(id, value, !blur, !blur);
        }
    }
    s = services_->components().state<DateState>(id);
    if (s && blur) {
        s->dirty_draft = false;
        s->display.set(display(*s, s->model.value()));
    }
}

void DatePickerComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<DateState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().date_picker();
    ThemeConfig config;
    config.input.tokens.background = t.colors.selector_background;
    config.input.tokens.hover_background = t.colors.selector_background;
    config.input.tokens.active_background = t.colors.selector_background;
    config.input.tokens.hover_border_color = t.colors.hover_border;
    config.input.tokens.active_border_color = t.colors.active_border;
    if (geometry) {
        layout::FlexLayout footer_layout;
        footer_layout.direction = layout::FlexDirection::vertical;
        footer_layout.item_policy = layout::FlexItemPolicy::sequential;
        footer_layout.main_gap = t.metrics.gap;
        services_->layout().set_layout(services_->components().root(s->footer), footer_layout);
    }
    s->input_theme.set(std::move(config));
    if (services_->components().set_window_layer(s->popup, t.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
}

void DatePickerComponentHost::mount_items(runtime::ComponentId id) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    auto& s = *services.components().state<DateState>(id);
    constexpr std::array navigation_icons{IconName::DoubleLeftOutlined, IconName::LeftOutlined, IconName::RightOutlined,
                                          IconName::DoubleRightOutlined};
    const std::array labels{u8"", u8"Su", u8"Mo", u8"Tu", u8"We", u8"Th", u8"Fr", u8"Sa"};
    for (std::size_t i = 0; i < 54; ++i) {
        const auto child = build.mount_component<DateItem>();
        auto& item = build.state<DateItem>(child);
        item.node = build.root(child);
        if (i >= 4 && i < 12) {
            item.text.set(String::from_utf8(std::u8string_view{labels[i - 4]}).value());
        }
        if (i < 4) {
            item.months = std::array{-12, -1, 1, 12}[i];
        }
        services.layout().set_layout(item.node, layout::BoxLayout{});
        services.nodes().require(item.node).clip_content = true;
        item.interaction =
            services.interactions().create({child, item.node, s.popup_hit, false, false, {}, false, false});
        input::InteractionHandlers handlers;
        handlers.target = [this, id, child](input::PointerDispatchContext& event) {
            auto* s = services_->components().state<DateState>(id);
            const auto* item = services_->components().state<DateItem>(child);
            if (!s || !item || !s->visible || s->disabled) {
                return;
            }
            if (item->date &&
                (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::move ||
                 event.kind() == input::PointerEventKind::leave)) {
                const auto next = event.kind() == input::PointerEventKind::leave ? DatePickerValue{} : item->date;
                if (s->hovered != next) {
                    s->hovered = next;
                    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
                }
            }
            if (event.event().button != input::PointerButton::primary) {
                return;
            }
            if (event.kind() == input::PointerEventKind::down) {
                static_cast<void>(event.capture_pointer());
                event.stop_propagation();
            }
            if (event.kind() == input::PointerEventKind::up) {
                static_cast<void>(event.release_pointer_capture());
                const bool hit = event.press_origin() == item->interaction &&
                                 inside(intersect(services_->nodes().require(item->node).bounds, s->bounds),
                                        event.event().x, event.event().y);
                const auto date = item->date;
                const bool title = child == s->items[4];
                const int months = item->months;
                event.stop_propagation();
                if (hit && date) {
                    choose(id, date);
                } else if (hit && months) {
                    navigate(id, months);
                } else if (hit && title) {
                    const auto mode = s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week
                                          ? DatePickerMode::Month
                                      : s->mode == DatePickerMode::Month || s->mode == DatePickerMode::Quarter
                                          ? DatePickerMode::Year
                                          : DatePickerMode::Decade;
                    const auto callback = s->on_panel;
                    const auto date = s->model.panel();
                    if (!s->controlled_mode) {
                        s->mode = mode;
                        s->model.view(mode, s->locale);
                    }
                    refresh(id);
                    if (callback) {
                        callback(date, mode);
                    }
                }
            }
        };
        services.interactions().set_handlers(item.interaction, std::move(handlers));
        item.hit = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(item.hit, {}, item.interaction);
        const auto body = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
        graphics::QuadInstance initial;
        initial.opacity = 0;
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        item.surface = services.surfaces().create_surface(child, item.node, body, std::span{&initial, 1}, effects);
        build.on_resource_cleanup(
            child, [&services, node = item.node, hit = item.interaction, fragment = item.hit, surface = item.surface] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
                services.scene_composer().remove_fragment(fragment);
                services.layout().remove_layout(node);
            });
        if (i >= 12) {
            const auto model = s.model;
            const auto date = model.cell(i - 12);
            const bool is_selected = selected(s, date);
            const bool eligible = date && model.eligible(*date);
            if (!services.components().contains(id) || !services.components().contains(child)) {
                throw std::runtime_error("DatePicker predicate retired its owner");
            }
            item.info.set({date, s.mode, is_selected, !eligible, s.range_endpoint});
        }
        if (i < navigation_icons.size()) {
            build.mount_slot_with_semantic_text_style(
                child, Content{[icon = navigation_icons[i]] { Icon(IconProps{}.name(icon)); }}, item.color, item.font);
        } else if (i >= 12 && s.cell_render) {
            const auto rich = s.cell_render(Prop<DatePickerCellInfo>{item.info});
            if (!services.components().contains(id) || !services.components().contains(child)) {
                throw std::runtime_error("DatePicker cell renderer retired its owner");
            }
            build.mount_slot_with_semantic_text_style(child, Content{SlotContentAccess::function(rich)}, item.color,
                                                      item.font);
        } else {
            build.mount_slot_with_semantic_text_style(
                child, Content{[text = item.text] { Text(TextProps{}.content(text)); }}, item.color, item.font);
        }
        services.layout().set_layout(
            item.node,
            layout::ComponentLayout{
                [&services](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                    runtime::Size size;
                    for (auto child : services.nodes().require(node).children) {
                        const auto measured = engine.measure_child(child, {0, limits.max_width, 0, limits.max_height});
                        size.width = std::max(size.width, measured.width);
                        size.height = std::max(size.height, measured.height);
                    }
                    return limits.constrain(size);
                },
                [&services](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect rect) {
                    for (auto child : services.nodes().require(node).children) {
                        const auto size = engine.measure_child(child, {0, rect.width, 0, rect.height});
                        engine.place_child(child, {rect.x + (rect.width - size.width) / 2,
                                                   rect.y + (rect.height - size.height) / 2, size.width, size.height});
                    }
                }});
        if (!services.components().contains(id) || !services.components().contains(child)) {
            throw std::runtime_error("DatePicker cell content retired its owner");
        }
        s.items.push_back(child);
    }
}

void DatePickerComponentHost::mount(const DatePickerProps& props, const std::optional<DatePickerFooter>& footer) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("DatePicker requires a window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("DatePicker controlled/default props conflict");
    }
    if ((props.multiple_value_ && props.default_multiple_) || (props.date_time_value_ && props.default_date_time_)) {
        throw std::invalid_argument("DatePicker typed controlled/default conflict");
    }
    if (read_prop(props.multiple_) && props.time_picker_) {
        throw std::invalid_argument("DatePicker multiple cannot be combined with showTime");
    }
    if (read_prop(props.show_now_) && !props.clock_) {
        throw std::invalid_argument("DatePicker showNow requires an explicit business clock");
    }
    std::set<std::string> keys;
    for (const auto& p : props.presets_) {
        if (p.key.empty() || !p.value || !keys.insert(std::string{p.key.bytes()}).second) {
            throw std::invalid_argument("DatePicker presets require unique keys and value callbacks");
        }
    }
    if (read_prop(props.picker_) == DatePickerMode::Decade) {
        throw std::invalid_argument("Decade is a panel mode, not a selected value mode");
    }
    for (const auto& p : props.multiple_presets_) {
        if (!read_prop(props.multiple_) || p.key.empty() || !p.value ||
            !keys.insert(std::string{p.key.bytes()}).second) {
            throw std::invalid_argument("Invalid multiple preset");
        }
    }
    for (const auto& p : props.date_time_presets_) {
        if (!props.time_picker_ || p.key.empty() || !p.value || !keys.insert(std::string{p.key.bytes()}).second) {
            throw std::invalid_argument("Invalid date-time preset");
        }
    }
    const auto locale = read_prop(props.locale_);
    locale.validate();
    const auto format = read_prop(props.format_);
    if (!format.empty()) {
        if (props.time_picker_ && (format.bytes().find('H') != std::string_view::npos ||
                                   format.bytes().find('h') != std::string_view::npos)) {
            validate_date_time_format(format.bytes());
        } else {
            validate_date_format(format.bytes());
        }
    }
    for (const auto& f : read_prop(props.input_formats_)) {
        validate_date_format(f.bytes());
    }
    auto value = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(DatePickerValue{});
    const auto date_time = props.date_time_value_ ? read_prop(*props.date_time_value_)
                                                  : props.default_date_time_.value_or(DatePickerDateTimeValue{});
    if (date_time) {
        value = date_time->date;
    }
    DatePickerRules rules{read_prop(props.min_), read_prop(props.max_), read_prop(props.disabled_date_)};
    const auto panel = props.controlled_panel_ ? read_prop(*props.controlled_panel_)
                       : value                 ? *value
                       : props.panel_date_     ? *props.panel_date_
                       : props.clock_          ? props.clock_().date
                                               : DatePickerModel::utc_date();
    rules.info = props.disabled_info_;
    rules.type = read_prop(props.picker_);
    DatePickerModel model{
        value, props.value_.has_value() || props.date_time_value_.has_value() || props.multiple_value_.has_value(),
        panel, rules};
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<DateState>(std::move(model));
    auto& s = build.state<DateState>(id);
    s.node = build.root(id);
    s.model.owner_guard(
        [this, id] { return services_->components().contains(id) && services_->components().scope(id).active(); });
    s.rules = rules;
    s.transition = std::make_unique<NavigationTransition>(services, id);
    s.picker = read_prop(props.picker_);
    s.mode = props.mode_ ? read_prop(*props.mode_) : s.picker;
    s.locale = locale;
    s.range_endpoint = props.range_endpoint_;
    s.format = format;
    s.input_formats = read_prop(props.input_formats_);
    s.model.view(s.mode, s.locale);
    s.controlled_mode = props.mode_.has_value();
    s.controlled_panel = props.controlled_panel_.has_value();
    s.panel_authority =
        props.controlled_panel_ ? DatePickerValue{read_prop(*props.controlled_panel_)} : DatePickerValue{};
    if (s.panel_authority) {
        s.model.panel_date(*s.panel_authority);
    }
    s.direction = read_prop(props.direction_);
    s.placement = read_prop(props.placement_);
    s.multiple = read_prop(props.multiple_);
    s.controlled_multiple = props.multiple_value_.has_value();
    s.accepted_multiple = props.multiple_value_ ? read_prop(*props.multiple_value_)
                                                : props.default_multiple_.value_or(DatePickerMultipleValue{});
    s.pending_multiple = s.accepted_multiple;
    s.show_time = props.time_picker_.has_value();
    s.controlled_time = props.date_time_value_.has_value();
    if (props.time_picker_) {
        s.time_reference = props.time_picker_->ref_.value_or(TimePickerRef{});
        s.time_rules = props.time_picker_->rules_;
        s.time_precision = props.time_picker_->precision_;
        const auto tf = read_prop(props.time_picker_->format_);
        const auto precision = read_prop(props.time_picker_->precision_);
        s.time_format = !tf.empty() ? tf
                                    : String::from_utf8(precision == TimePickerPrecision::Minute        ? "HH:mm"
                                                        : precision == TimePickerPrecision::Millisecond ? "HH:mm:ss.SSS"
                                                                                                        : "HH:mm:ss")
                                          .value();
    }
    s.accepted_time = date_time;
    s.pending_time.set(date_time            ? date_time->time
                       : props.time_picker_ ? props.time_picker_->panel_value_
                                            : TimeOfDay{});
    s.need_confirm = props.need_confirm_ ? read_prop(*props.need_confirm_) : s.multiple || s.show_time;
    s.on_ok = props.on_ok_;
    s.on_time_ok = props.on_time_ok_;
    s.on_multiple = props.on_multiple_;
    s.on_time = props.on_date_time_;
    s.disabled_time = props.disabled_time_;
    s.on_panel = props.on_panel_;
    s.cell_render = props.cell_render_;
    s.panel = props.panel_;
    s.header = props.header_;
    s.reference = props.ref_.value_or(DatePickerRef{});
    s.disabled = read_prop(props.disabled_);
    s.allow_clear = read_prop(props.allow_clear_);
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.on_change = props.on_change_;
    s.on_request = props.on_request_;
    s.on_time_request = props.on_time_request_;
    s.on_open = props.on_open_;
    s.on_focus = props.on_focus_;
    s.on_blur = props.on_blur_;
    s.display.set(display(s, value));
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.input_theme), ThemeContent{[&] {
                      FormPrivateEditorScope private_editor;
                      Input(InputProps{}
                                .value(s.display)
                                .placeholder(props.placeholder_)
                                .readOnly(bind([read_only = props.read_only_, multiple = props.multiple_] {
                                    return read_prop(read_only) || read_prop(multiple);
                                }))
                                .disabled(props.disabled_)
                                .size(props.size_)
                                .variant(props.variant_)
                                .status(props.status_)
                                .ref(s.reference)
                                .autoFocus(props.auto_focus_)
                                .onChange([this, id](String text) {
                                    if (auto* s = services_->components().state<DateState>(id)) {
                                        s->dirty_draft = true;
                                        s->display.set(std::move(text));
                                    }
                                })
                                .onSubmit([this, id](String) { commit(id, false); })
                                .onFocus([this, id] {
                                    if (auto* s = services_->components().state<DateState>(id)) {
                                        s->closed_on_blur = false;
                                        const auto callback = s->on_focus;
                                        if (callback) {
                                            picker_callback(*services_, id, callback);
                                        }
                                    }
                                })
                                .onBlur([this, id] {
                                    auto* s = services_->components().state<DateState>(id);
                                    if (!s || !services_->components().scope(id).active()) {
                                        return;
                                    }
                                    const auto callback = s->on_blur;
                                    const auto focused = services_->focus().state().focused;
                                    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
                                    const bool internal = record && descendant(*services_, record->component, s->popup);
                                    const auto* input = services_->interactions().find(s->input_hit);
                                    const bool input_active = input && input->eligible &&
                                                              services_->interactions().component_active(s->input) &&
                                                              services_->focus().state().window_active;
                                    commit(id, true);
                                    if (!internal && input_active && services_->components().contains(id)) {
                                        services_->components().state<DateState>(id)->closed_on_blur = true;
                                        request_open(id, false);
                                    }
                                    if (services_->components().contains(id) && callback) {
                                        picker_callback(*services_, id, callback);
                                    }
                                }),
                            {}, InputSuffix{[&] {
                                mount_input_affix_action(
                                    services, IconSource{IconName::CloseCircleFilled}, s.action_disabled,
                                    [this, id] { choose(id, {}, true, true); }, s.clear_visible, false, false);
                                if (props.suffix_icon_) {
                                    SlotContentAccess::function (*props.suffix_icon_)();
                                } else {
                                    Icon(IconProps{}.name(IconName::CalendarOutlined).tone(TextTone::Secondary));
                                }
                            }});
                      const auto input = inputs->mounted_inputs().back();
                      s.input = input.component;
                      s.input_hit = input.interaction;
                      s.editor = input.editor;
                      inputs->configure_form_reset(input.component, [this, id, editor = input.editor,
                                                                     controlled = props.value_] {
                          auto* current = services_->components().state<DateState>(id);
                          if (!current || current->editor != editor || !services_->components().scope(id).active()) {
                              return;
                          }
                          const auto accepted = controlled ? read_prop(*controlled) : current->model.value();
                          current = services_->components().state<DateState>(id);
                          if (!current || current->editor != editor || !services_->components().scope(id).active()) {
                              return;
                          }
                          current->model.reconcile(accepted);
                          current->dirty_draft = false;
                          current->pending_date.reset();
                          current->pending_multiple = current->accepted_multiple;
                          current->pending_time.set(current->accepted_time ? current->accepted_time->time
                                                                           : TimeOfDay{});
                          current->display.set(display(*current, current->model.value()));
                          const auto reset_time = capture_form_input(current->time_reference).second;
                          if (reset_time) {
                              reset_time();
                          }
                          refresh(id);
                      });
                  }});
            s.popup = group(services);
            const auto popup_node = build.root(s.popup);
            services.layout().set_layout(popup_node, layout::LeafLayout{});
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
            build.on_resource_cleanup(s.popup, [&services, hit = s.popup_hit, surface = s.background] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            build.mount_slot(
                s.popup, Content{[&] {
                    mount_items(id);
                    if (props.panel_) {
                        s.custom_panel = group(services);
                        build.mount_slot(s.custom_panel, Content{SlotContentAccess::function(*props.panel_)});
                    }
                    if (props.header_) {
                        s.custom_header = group(services);
                        build.mount_slot(s.custom_header, Content{SlotContentAccess::function(*props.header_)});
                    }
                    s.footer = group(services);
                    layout::FlexLayout footer_layout;
                    footer_layout.direction = layout::FlexDirection::vertical;
                    footer_layout.item_policy = layout::FlexItemPolicy::sequential;
                    footer_layout.main_gap = build.theme_scope()->snapshot().date_picker().metrics.gap;
                    services.layout().set_layout(build.root(s.footer), footer_layout);
                    services.nodes().require(build.root(s.footer)).clip_content = true;
                    build.mount_slot(
                        s.footer, Content{[&] {
                            if (footer) {
                                SlotContentAccess::function (*footer)();
                            }
                            if (props.time_picker_) {
                                auto time = *props.time_picker_;
                                time.ref(s.time_reference);
                                time.range_endpoint_ = props.range_endpoint_;
                                const auto base = time.rules_;
                                time.rules(bind([this, id, base] {
                                    auto rules = read_prop(base);
                                    const auto* live = services_->components().state<DateState>(id);
                                    if (!live) {
                                        return rules;
                                    }
                                    const auto date = live->time_date.get().value_or(live->model.active());
                                    const auto disabled = live->disabled_time;
                                    const auto existing = rules.disabled;
                                    rules.disabled = [date, disabled, existing](TimeOfDay value) {
                                        return (existing && existing(value)) || (disabled && disabled(date, value));
                                    };
                                    return rules;
                                }));
                                time.value(s.pending_time).needConfirm(false).onChange([this, id](TimePickerValue v) {
                                    if (auto* live = services_->components().state<DateState>(id)) {
                                        live->pending_time.set(v);
                                        live = services_->components().state<DateState>(id);
                                        if (live && !live->need_confirm && v) {
                                            const auto date =
                                                live->pending_date ? live->pending_date : live->model.value();
                                            if (date) {
                                                choose(id, date, false, true);
                                            }
                                        }
                                    }
                                });
                                TimePicker(std::move(time));
                                s.time_picker = services.time_picker().mounted().back();
                            }
                            for (const auto& preset : props.multiple_presets_) {
                                Button(ButtonProps{}.size(ControlSize::Small).onClick([this, id, fn = preset.value] {
                                    picker_callback(*services_, id, [&] {
                                        auto values = fn();
                                        if (auto* live = services_->components().state<DateState>(id)) {
                                            auto normalize = live->model;
                                            for (auto& value : values) {
                                                value = normalize.normalized(value);
                                            }
                                            std::ranges::sort(values);
                                            values.erase(std::unique(values.begin(), values.end()), values.end());
                                            live->pending_multiple = std::move(values);
                                            confirm(id);
                                        }
                                    });
                                }),
                                       ButtonContent{[label = preset.label] { Text(label); }});
                            }
                            for (const auto& preset : props.date_time_presets_) {
                                Button(ButtonProps{}.size(ControlSize::Small).onClick([this, id, fn = preset.value] {
                                    picker_callback(*services_, id, [&] {
                                        const auto value = fn();
                                        if (auto* live = services_->components().state<DateState>(id)) {
                                            live->pending_time.set(value ? value->time : TimeOfDay{});
                                            choose(id, value ? DatePickerValue{value->date} : DatePickerValue{}, true,
                                                   true);
                                        }
                                    });
                                }),
                                       ButtonContent{[label = preset.label] { Text(label); }});
                            }
                            for (const auto& preset : props.presets_) {
                                Button(ButtonProps{}.size(ControlSize::Small).onClick([this, id, fn = preset.value] {
                                    try {
                                        const auto value = fn();
                                        if (services_->components().contains(id)) {
                                            const auto* live = services_->components().state<DateState>(id);
                                            if (live && live->multiple && value) {
                                                choose(id, value, false);
                                                if (services_->components().contains(id)) {
                                                    confirm(id);
                                                }
                                            } else {
                                                choose(id, value, true, true);
                                            }
                                        }
                                    } catch (...) {
                                        static_cast<void>(services_->retire_failed_component(id));
                                        throw;
                                    }
                                }),
                                       ButtonContent{[label = preset.label] { Text(label); }});
                            }
                            if (props.clock_) {
                                Button(ButtonProps{}
                                           .size(ControlSize::Small)
                                           .disabled(bind([show = props.show_now_] { return !read_prop(show); }))
                                           .onClick([this, id, clock = props.clock_] {
                                               try {
                                                   const auto now = clock();
                                                   if (auto* live = services_->components().state<DateState>(id)) {
                                                       live->pending_time.set(now.time);
                                                       choose(id, now.date, true, true);
                                                   }
                                               } catch (...) {
                                                   static_cast<void>(services_->retire_failed_component(id));
                                                   throw;
                                               }
                                           }),
                                       ButtonContent{[locale = props.locale_] {
                                           Text(
                                               TextProps{}.content(bind([locale] { return read_prop(locale).today; })));
                                       }});
                            }
                            s.confirm_group = group(services);
                            build.mount_slot(
                                s.confirm_group, Content{[&] {
                                    Button(ButtonProps{}.size(ControlSize::Small).onClick([this, id] { confirm(id); }),
                                           ButtonContent{[locale = props.locale_] {
                                               Text(TextProps{}.content(
                                                   bind([locale] { return read_prop(locale).confirm; })));
                                           }});
                                }});
                        }});
                }});
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                    const auto& s = *services_->components().state<DateState>(id);
                                    return engine.measure_child(services_->components().root(s.input), limits);
                                },
                                [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                                    const auto& s = *services_->components().state<DateState>(id);
                                    engine.place_child(services_->components().root(s.input), rect);
                                }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](DatePickerValue value) {
            if (auto* s = services_->components().state<DateState>(id)) {
                if (s->model.value() != value) {
                    s->model.reconcile(value);
                    if (s->panel_authority) {
                        s->model.panel_date(*s->panel_authority);
                    }
                    s->dirty_draft = false;
                    s->display.set(display(*s, value));
                    refresh(id);
                }
            }
        });
    }
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool open) {
            if (auto* s = services_->components().state<DateState>(id)) {
                const bool changed = s->open != open;
                s->open = open;
                if (changed && open) {
                    s->suppressed = false;
                }
                if (!open) {
                    s->pending_date.reset();
                    s->pending_multiple = s->accepted_multiple;
                    s->dirty_draft = false;
                    const auto time = s->accepted_time ? s->accepted_time->time : TimeOfDay{};
                    picker_callback(*services_, id, [&] { s->pending_time.set(time); });
                    s = services_->components().state<DateState>(id);
                    if (!s) {
                        return;
                    }
                    s->display.set(display(*s, s->model.value()));
                }
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.disabled_, [this, id](bool disabled) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->disabled = disabled;
            if (disabled) {
                s->suppressed = true;
                s->dirty_draft = false;
                s->display.set(display(*s, s->model.value()));
                request_open(id, false);
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.allow_clear_, [this, id](bool value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->allow_clear = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.min_, [this, id](DatePickerValue value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            auto rules = s->rules;
            rules.min = value;
            s->model.configure(rules);
            s->rules = std::move(rules);
            refresh(id);
        }
    });
    connect_prop(scope, props.max_, [this, id](DatePickerValue value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            auto rules = s->rules;
            rules.max = value;
            s->model.configure(rules);
            s->rules = std::move(rules);
            refresh(id);
        }
    });
    connect_prop(scope, props.disabled_date_, [this, id](DisabledDate value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            auto rules = s->rules;
            rules.disabled = std::move(value);
            s->model.configure(rules);
            s->rules = std::move(rules);
            refresh(id);
        }
    });
    if (props.need_confirm_) {
        connect_prop(scope, *props.need_confirm_, [this, id](bool v) {
            if (auto* s = services_->components().state<DateState>(id)) {
                s->need_confirm = v;
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.multiple_, [this, id, confirm = props.need_confirm_](bool v) {
        if (auto* s = services_->components().state<DateState>(id)) {
            if (v && s->show_time) {
                throw std::invalid_argument("multiple/showTime conflict");
            }
            s->multiple = v;
            if (!confirm) {
                s->need_confirm = v || s->show_time;
            }
            s->display.set(display(*s, s->model.value()));
            refresh(id);
        }
    });
    connect_prop(scope, props.picker_, [this, id](DatePickerMode v) {
        if (v < DatePickerMode::Date || v > DatePickerMode::Year) {
            throw std::invalid_argument("Invalid selected picker mode");
        }
        if (auto* s = services_->components().state<DateState>(id)) {
            auto model = s->model;
            auto rules = s->rules;
            rules.type = v;
            model.configure(rules);
            if (!s->controlled_mode) {
                model.view(v, s->locale);
            }
            s->picker = v;
            s->model = std::move(model);
            s->rules = std::move(rules);
            if (!s->controlled_mode) {
                s->mode = v;
            }
            refresh(id);
        }
    });
    if (props.mode_) {
        connect_prop(scope, *props.mode_, [this, id](DatePickerMode v) {
            if (auto* s = services_->components().state<DateState>(id)) {
                s->model.view(v, s->locale);
                s->mode = v;
                refresh(id);
            }
        });
    }
    if (props.controlled_panel_) {
        connect_prop(scope, *props.controlled_panel_, [this, id](CivilDate v) {
            if (auto* s = services_->components().state<DateState>(id)) {
                s->model.panel_date(v);
                s->panel_authority = v;
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.locale_, [this, id](PickerLocale v) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->model.view(s->mode, v);
            s->locale = std::move(v);
            if (!s->dirty_draft) {
                s->display.set(display(*s, s->model.value()));
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.format_, [this, id](String v) {
        if (!v.empty()) {
            if (v.bytes().find('H') != std::string_view::npos || v.bytes().find('h') != std::string_view::npos) {
                validate_date_time_format(v.bytes());
            } else {
                validate_date_format(v.bytes());
            }
        }
        if (auto* s = services_->components().state<DateState>(id)) {
            s->format = std::move(v);
            if (!s->dirty_draft) {
                s->display.set(display(*s, s->model.value()));
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.input_formats_, [this, id](std::vector<String> v) {
        for (const auto& f : v) {
            validate_date_format(f.bytes());
        }
        if (auto* s = services_->components().state<DateState>(id)) {
            s->input_formats = std::move(v);
        }
    });
    connect_prop(scope, props.direction_, [this, id](TextDirection v) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->direction = v;
            refresh(id);
        }
    });
    connect_prop(scope, props.placement_, [this, id](PickerPlacement v) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->placement = v;
            refresh(id);
        }
    });
    if (props.multiple_value_) {
        connect_prop(scope, *props.multiple_value_, [this, id](DatePickerMultipleValue v) {
            if (auto* s = services_->components().state<DateState>(id)) {
                s->accepted_multiple = v;
                s->pending_multiple = std::move(v);
                s->display.set(display(*s, s->model.value()));
                refresh(id);
            }
        });
    }
    if (props.date_time_value_) {
        connect_prop(scope, *props.date_time_value_, [this, id](DatePickerDateTimeValue v) {
            if (auto* s = services_->components().state<DateState>(id)) {
                s->accepted_time = v;
                picker_callback(*services_, id, [&] { s->pending_time.set(v ? v->time : TimeOfDay{}); });
                s = services_->components().state<DateState>(id);
                if (!s) {
                    return;
                }
                s->pending_date.reset();
                s->model.reconcile(v ? DatePickerValue{v->date} : DatePickerValue{});
                if (s->panel_authority) {
                    s->model.panel_date(*s->panel_authority);
                }
                s->display.set(display(*s, s->model.value()));
                refresh(id);
            }
        });
    }
    update_theme(id, true);
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::date_picker_colors, &theme_runtime::ThemeScope::date_picker_metrics,
        &theme_runtime::ThemeScope::date_picker_effects, &theme_runtime::ThemeScope::date_picker_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
}

void DatePickerComponentHost::mount_range(const DateRangePickerProps& props,
                                          const std::optional<DatePickerFooter>& footer) {
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("DateRangePicker controlled/default conflict");
    }
    validate_range_presets(props.presets_);
    auto state = std::make_shared<PickerRangeState<CivilDate>>();
    state->controlled = props.value_.has_value();
    state->controlled_open = props.open_.has_value();
    state->controlled_endpoint = props.endpoint_.has_value();
    state->policy = read_prop(props.policy_);
    state->policy_source.set(state->policy);
    state->accepted.set(props.value_ ? read_prop(*props.value_)
                                     : props.default_value_.value_or(DatePickerRangeValue{}));
    state->pending.set(state->accepted.get());
    state->open.set(props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false));
    state->reconcile_endpoint(props.endpoint_
                                  ? read_prop(*props.endpoint_)
                                  : (state->policy.disabled[0] ? PickerEndpoint::End : PickerEndpoint::Start));
    if (props.picker_.time_picker_) {
        throw std::invalid_argument("DateRangePicker showTime requires DateTimeRangePickerProps");
    }
    state->disabled = [weak = std::weak_ptr{state}, custom = props.disabled_date_, min = props.picker_.min_,
                       max = props.picker_.max_, disabled = props.picker_.disabled_date_,
                       info = props.picker_.disabled_info_, type = props.picker_.picker_,
                       locale = props.picker_.locale_](CivilDate date, PickerEndpoint endpoint,
                                                       const DatePickerRangeValue& range) {
        DatePickerRules rules{read_prop(min), read_prop(max), read_prop(disabled), info, range.start, read_prop(type)};
        DatePickerModel model{{}, true, date, std::move(rules)};
        model.owner_guard([weak] {
            const auto live = weak.lock();
            return live && live->alive;
        });
        model.view(read_prop(type), read_prop(locale));
        return !model.eligible(date) || (custom && custom(date, endpoint, range));
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
    const auto owner = group(*services_);
    services_->layout().set_layout(build.root(owner), layout);
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
        connect_prop(build.scope(owner), *props.value_, [state](DatePickerRangeValue v) {
            state->accepted.set(v);
            state->pending.set(v);
        });
    }
    if (props.open_) {
        connect_prop(build.scope(owner), *props.open_, [state](bool v) {
            state->open.set(v);
            if (!v) {
                state->pending.set(state->accepted.get());
            }
        });
    }
    if (props.endpoint_) {
        connect_prop(build.scope(owner), *props.endpoint_, [state](PickerEndpoint v) { state->reconcile_endpoint(v); });
    }
    connect_prop(build.scope(owner), props.policy_, [state](PickerRangePolicy v) {
        state->policy = v;
        state->policy_source.set(v);
    });
    build.mount_slot(
        owner, Content{[&, state] {
            for (auto endpoint : {PickerEndpoint::Start, PickerEndpoint::End}) {
                auto picker = props.picker_;
                picker.range_endpoint_ = endpoint;
                picker.autoFocus(picker.auto_focus_ && state->endpoint.get() == endpoint);
                const auto base = picker.disabled_date_;
                const auto info = picker.disabled_info_;
                const auto type = picker.picker_;
                picker.disabled_info_ = {};
                picker.disabledDate(bind([state, base, info, type, endpoint] {
                    const auto draft = state->pending.get();
                    const auto original = read_prop(base);
                    const auto picker_mode = read_prop(type);
                    return DisabledDate{[state, original, info, endpoint, draft, picker_mode](CivilDate v) {
                        return (original && original(v)) || !state->alive ||
                               (info && info(v, {picker_mode, draft.start})) || !state->alive ||
                               (state->disabled && state->disabled(v, endpoint, draft));
                    }};
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
                picker.on_date_time_ = {};
                picker.on_time_ok_ = {};
                picker.on_request_ = [state, endpoint](DatePickerValue v) {
                    state->choose(endpoint, v);
                };
                picker.onOpenChange([state, endpoint](bool v) {
                    if (v) {
                        state->activate(endpoint);
                        state->request_open(true);
                    } else if (state->alive && state->open.get() && state->endpoint.get() == endpoint) {
                        state->request_open(false);
                    }
                });
                DatePicker(
                    std::move(picker), DatePickerFooter{[state, footer, presets = props.presets_] {
                        if (footer) {
                            SlotContentAccess::function (*footer)();
                        }
                        for (const auto& preset : presets) {
                            Button(ButtonProps{}.size(ControlSize::Small).onClick([state, preset] {
                                state->preset(preset);
                            }),
                                   ButtonContent{[label = preset.label] { Text(label); }});
                        }
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([state] { state->clear(); }),
                               ButtonContent{[] { Text(u8"Clear range"); }});
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([state] {
                            static_cast<void>(state->commit());
                        }),
                               ButtonContent{[] { Text(u8"Confirm range"); }});
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([state] { state->request_open(false); }),
                               ButtonContent{[] { Text(u8"Cancel range"); }});
                    }});
                const auto child = mounted_.back();
                state->owners.push_back(child);
                state->inputs.push_back(services_->components().state<DateState>(child)->input);
                state->references.push_back(services_->components().state<DateState>(child)->reference);
            }
        }});
    connect_range_reset(*services_, state, [this, state] {
        const auto accepted = state->accepted.get();
        for (std::size_t i = 0; i < state->owners.size(); ++i) {
            if (auto* live = services_->components().state<DateState>(state->owners[i])) {
                live->model.reconcile(i == 0 ? accepted.start : accepted.end);
                live->pending_date.reset();
                live->dirty_draft = false;
                live->display.set(display(*live, live->model.value()));
                refresh(state->owners[i]);
            }
        }
    });
}

void DatePickerComponentHost::mount_range(const DateTimeRangePickerProps& props,
                                          const std::optional<DatePickerFooter>& footer) {
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("DateRangePicker controlled/default conflict");
    }
    validate_range_presets(props.presets_);
    auto state = std::make_shared<PickerRangeState<CivilDateTime>>();
    state->controlled = props.value_.has_value();
    state->controlled_open = props.open_.has_value();
    state->controlled_endpoint = props.endpoint_.has_value();
    state->policy = read_prop(props.policy_);
    state->policy_source.set(state->policy);
    state->accepted.set(props.value_ ? read_prop(*props.value_)
                                     : props.default_value_.value_or(DateTimePickerRangeValue{}));
    state->pending.set(state->accepted.get());
    state->open.set(props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false));
    state->reconcile_endpoint(props.endpoint_
                                  ? read_prop(*props.endpoint_)
                                  : (state->policy.disabled[0] ? PickerEndpoint::End : PickerEndpoint::Start));
    state->disabled = [weak = std::weak_ptr{state}, custom = props.disabled_date_, min = props.picker_.min_,
                       max = props.picker_.max_, disabled = props.picker_.disabled_date_,
                       info = props.picker_.disabled_info_, type = props.picker_.picker_,
                       locale = props.picker_.locale_, time = props.picker_.time_picker_,
                       disabled_time = props.picker_.disabled_time_](CivilDateTime date, PickerEndpoint endpoint,
                                                                     const DateTimePickerRangeValue& range) {
        DatePickerRules rules{read_prop(min),
                              read_prop(max),
                              read_prop(disabled),
                              info,
                              range.start ? DatePickerValue{range.start->date} : DatePickerValue{},
                              read_prop(type)};
        DatePickerModel model{{}, true, date.date, std::move(rules)};
        model.owner_guard([weak] {
            const auto live = weak.lock();
            return live && live->alive;
        });
        model.view(read_prop(type), read_prop(locale));
        TimePickerModel tm{time ? read_prop(time->rules_) : TimePickerRules{},
                           time ? read_prop(time->precision_) : TimePickerPrecision::Second,
                           {},
                           true};
        tm.owner_guard([weak] {
            const auto live = weak.lock();
            return live && live->alive;
        });
        return !model.eligible(date.date) || !tm.allowed(date.time) ||
               (disabled_time && disabled_time(date.date, date.time)) || (custom && custom(date, endpoint, range));
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
    const auto owner = group(*services_);
    services_->layout().set_layout(build.root(owner), layout);
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
        connect_prop(build.scope(owner), *props.value_, [state](DateTimePickerRangeValue v) {
            state->accepted.set(v);
            state->pending.set(v);
        });
    }
    if (props.open_) {
        connect_prop(build.scope(owner), *props.open_, [state](bool v) {
            state->open.set(v);
            if (!v) {
                state->pending.set(state->accepted.get());
            }
        });
    }
    if (props.endpoint_) {
        connect_prop(build.scope(owner), *props.endpoint_, [state](PickerEndpoint v) { state->reconcile_endpoint(v); });
    }
    connect_prop(build.scope(owner), props.policy_, [state](PickerRangePolicy v) {
        state->policy = v;
        state->policy_source.set(v);
    });
    build.mount_slot(
        owner, Content{[&, state] {
            for (auto endpoint : {PickerEndpoint::Start, PickerEndpoint::End}) {
                auto picker = props.picker_;
                picker.range_endpoint_ = endpoint;
                picker.autoFocus(picker.auto_focus_ && state->endpoint.get() == endpoint);
                if (!picker.time_picker_) {
                    picker.showTime();
                }
                const auto original_disabled = picker.disabled_time_;
                picker.disabledTime([state, endpoint, original_disabled](CivilDate date, TimeOfDay time) {
                    const auto draft = state->pending.get();
                    return (original_disabled && original_disabled(date, time)) || !state->alive ||
                           (state->disabled && state->disabled(CivilDateTime{date, time}, endpoint, draft));
                });
                picker.default_value_.reset();
                picker.default_date_time_.reset();
                picker.value_.reset();
                picker.default_open_.reset();
                picker.dateTimeValue(bind([state, endpoint] {
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
                picker.on_date_time_ = {};
                picker.on_time_ok_ = {};
                picker.on_time_request_ = [state, endpoint](DatePickerDateTimeValue v) {
                    state->choose(endpoint, v);
                };
                picker.onOpenChange([state, endpoint](bool v) {
                    if (v) {
                        state->activate(endpoint);
                        state->request_open(true);
                    } else if (state->alive && state->open.get() && state->endpoint.get() == endpoint) {
                        state->request_open(false);
                    }
                });
                DatePicker(
                    std::move(picker), DatePickerFooter{[state, footer, presets = props.presets_] {
                        if (footer) {
                            SlotContentAccess::function (*footer)();
                        }
                        for (const auto& preset : presets) {
                            Button(ButtonProps{}.size(ControlSize::Small).onClick([state, preset] {
                                state->preset(preset);
                            }),
                                   ButtonContent{[label = preset.label] { Text(label); }});
                        }
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([state] { state->clear(); }),
                               ButtonContent{[] { Text(u8"Clear range"); }});
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([state] {
                            static_cast<void>(state->commit());
                        }),
                               ButtonContent{[] { Text(u8"Confirm range"); }});
                        Button(ButtonProps{}.size(ControlSize::Small).onClick([state] { state->request_open(false); }),
                               ButtonContent{[] { Text(u8"Cancel range"); }});
                    }});
                const auto child = mounted_.back();
                state->owners.push_back(child);
                state->inputs.push_back(services_->components().state<DateState>(child)->input);
                state->references.push_back(services_->components().state<DateState>(child)->reference);
            }
        }});
    connect_range_reset(*services_, state, [this, state] {
        const auto accepted = state->accepted.get();
        for (std::size_t i = 0; i < state->owners.size(); ++i) {
            if (auto* live = services_->components().state<DateState>(state->owners[i])) {
                const auto value = i == 0 ? accepted.start : accepted.end;
                live->accepted_time = value;
                live->model.reconcile(value ? DatePickerValue{value->date} : DatePickerValue{});
                live->pending_date.reset();
                live->dirty_draft = false;
                live->pending_time.set(value ? value->time : TimeOfDay{});
                live->display.set(display(*live, live->model.value()));
                if (live->time_picker.valid()) {
                    const auto reset = capture_form_input(live->time_reference).second;
                    if (reset) {
                        reset();
                    }
                }
                refresh(state->owners[i]);
            }
        }
    });
}

void DatePickerComponentHost::on_window_active(bool active) {
    active_ = active;
    on_input_boundary_changed();
}

void DatePickerComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DateState>(id);
        if (!s) {
            continue;
        }
        if ((!active_ || !services_->interactions().component_active(id)) && !s->suppressed) {
            s->suppressed = true;
            s->dirty_draft = false;
            s->display.set(display(*s, s->model.value()));
            request_open(id, false);
        }
        refresh(id, false);
    }
}

void DatePickerComponentHost::prepare_auxiliary_layout() {
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->closed_on_blur = false;
        }
    }
    if (std::ranges::any_of(mounted_, [this](auto id) {
            const auto* s = services_->components().state<DateState>(id);
            return s && !s->suppressed && !services_->interactions().component_active(id);
        })) {
        on_input_boundary_changed();
    }
}

bool DatePickerComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !active_) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DateState>(id);
        if (!s || s->disabled || !services_->interactions().component_active(id) || !record) {
            continue;
        }
        const bool input = focused == s->input_hit;
        const bool panel = descendant(*services_, record->component, s->popup) &&
                           layer(*services_, record->component) == std::optional{s->popup};
        if (!input && !panel) {
            continue;
        }
        const auto session = services_->input_runtime()->sessions().active();
        if (session.owner.valid() &&
            services_->input_runtime()->editors().require(session.owner).composition().active) {
            return false;
        }
        using input::Key;
        if (event.key == Key::escape && s->visible) {
            for (auto time : services_->time_picker().mounted()) {
                if (descendant(*services_, time, id) && descendant(*services_, record->component, time) &&
                    services_->time_picker().snapshot(time).visible) {
                    return false;
                }
            }
            if (services_->popover().owns_escape(id, record->component)) {
                return false;
            }
            request_open(id, false, true);
            return true;
        }
        if (!input) {
            return false;
        }
        if (event.key == Key::enter && event.modifiers == input::KeyModifier::none) {
            if (!event.repeat) {
                if (s->dirty_draft) {
                    commit(id, false);
                } else if (s->visible) {
                    if (s->need_confirm) {
                        confirm(id);
                    } else {
                        choose(id, s->model.active());
                    }
                } else {
                    request_open(id, true);
                }
            }
            return true;
        }
        const bool page = event.key == Key::page_up || event.key == Key::page_down;
        if (event.modifiers != input::KeyModifier::none && !(page && event.modifiers == input::KeyModifier::shift)) {
            return false;
        }
        if (!s->visible) {
            if (event.key == Key::down) {
                request_open(id, true);
                return true;
            }
            return false;
        }
        const auto previous_model = s->model;
        switch (event.key) {
        case Key::left:
            if (s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week) {
                s->model.move(s->direction == TextDirection::RightToLeft ? 1 : -1);
            } else {
                s->model.navigate(s->mode == DatePickerMode::Month     ? -1
                                  : s->mode == DatePickerMode::Quarter ? -3
                                                                       : -12);
            }
            break;
        case Key::right:
            if (s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week) {
                s->model.move(s->direction == TextDirection::RightToLeft ? -1 : 1);
            } else {
                s->model.navigate(s->mode == DatePickerMode::Month ? 1 : s->mode == DatePickerMode::Quarter ? 3 : 12);
            }
            break;
        case Key::up:
            if (s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week) {
                s->model.move(-7);
            } else {
                s->model.navigate(s->mode == DatePickerMode::Month     ? -3
                                  : s->mode == DatePickerMode::Quarter ? -6
                                                                       : -36);
            }
            break;
        case Key::down:
            if (s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week) {
                s->model.move(7);
            } else {
                s->model.navigate(s->mode == DatePickerMode::Month ? 3 : s->mode == DatePickerMode::Quarter ? 6 : 36);
            }
            break;
        case Key::home:
            s->model.boundary(false);
            break;
        case Key::end:
            s->model.boundary(true);
            break;
        case Key::page_up:
            navigate(id, -panel_step(s->mode) *
                             (event.modifiers == input::KeyModifier::shift ? (panel_step(s->mode) == 1 ? 12 : 10) : 1));
            return true;
        case Key::page_down:
            navigate(id, panel_step(s->mode) *
                             (event.modifiers == input::KeyModifier::shift ? (panel_step(s->mode) == 1 ? 12 : 10) : 1));
            return true;
        default:
            return false;
        }
        const auto wanted = s->model.panel();
        const auto mode = s->mode;
        const auto callback = s->on_panel;
        const bool changed = wanted != previous_model.panel();
        if (changed && s->controlled_panel) {
            s->model = previous_model;
        }
        refresh(id);
        if (changed && callback) {
            picker_callback(*services_, id, callback, wanted, mode);
        }
        return true;
    }
    return false;
}

void DatePickerComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DateState>(id);
        if (!s || s->disabled) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool own = record && descendant(*services_, record->component, id);
        if (event.action == input::PointerAction::down) {
            const bool already_closed = s->closed_on_blur;
            s->closed_on_blur = false;
            if (!own && s->visible && !already_closed) {
                request_open(id, false);
            }
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary &&
                   hit == s->input_hit && hit == origin) {
            request_open(id, !s->visible);
        }
    }
}

bool DatePickerComponentHost::consume_scroll_boundary(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    if (!record) {
        return false;
    }
    const auto owner = layer(*services_, record->component);
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DateState>(id);
        if (s && s->visible && owner == std::optional{s->popup}) {
            return true;
        }
    }
    return false;
}

void DatePickerComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        position_window_layer(id, viewport, clip);
    }
}

void DatePickerComponentHost::position_window_layer(runtime::ComponentId id, runtime::Size viewport,
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
    auto* s = services_->components().state<DateState>(id);
    if (!s || !s->visible) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().date_picker();
    auto anchor = services_->nodes().require(s->node).bounds;
    const auto translation = services_->nodes().require(s->node).translation;
    anchor.x += translation.x;
    anchor.y += translation.y;
    const auto available = services_->component_clip(s->popup, viewport, clip);
    auto& engine = services_->layout();
    const auto popup_node = services_->components().root(s->popup);
    if (!s->needs_layout && s->anchor == anchor && s->clip == available &&
        services_->nodes().require(popup_node).measure_generation == engine.generation()) {
        return;
    }
    const float width = std::min(t.metrics.panel_width, available.width);
    const float padding = std::min(t.metrics.padding, width / 2);
    const auto footer_node = services_->components().root(s->footer);
    const auto footer_size =
        engine.measure_child(footer_node, {0, std::max(0.0F, width - 2 * padding), 0, available.height});
    const bool days = s->mode == DatePickerMode::Date || s->mode == DatePickerMode::Week;
    const int rows = days ? 7 : s->mode == DatePickerMode::Quarter ? 2 : 4;
    const int columns = days ? 7 : s->mode == DatePickerMode::Quarter ? 2 : 3;
    const float wanted = 2 * padding + t.metrics.header_height + rows * t.metrics.row_height + footer_size.height;
    const float below =
        std::clamp(available.y + available.height - anchor.y - anchor.height - t.metrics.gap, 0.0F, available.height);
    const float above = std::clamp(anchor.y - t.metrics.gap - available.y, 0.0F, available.height);
    const bool top = s->placement == PickerPlacement::TopLeft || s->placement == PickerPlacement::TopRight;
    const bool use_above = top ? !(above < wanted && below > above) : below < wanted && above > below;
    const bool right = (s->placement == PickerPlacement::BottomRight || s->placement == PickerPlacement::TopRight) !=
                       (s->direction == TextDirection::RightToLeft);
    const float height = std::min(wanted, use_above ? above : below);
    s->bounds = {std::clamp(right ? anchor.x + anchor.width - width : anchor.x, available.x,
                            available.x + available.width - width),
                 std::clamp(use_above ? anchor.y - t.metrics.gap - height : anchor.y + anchor.height + t.metrics.gap,
                            available.y, available.y + available.height - height),
                 width, height};
    static_cast<void>(engine.measure_child(popup_node, layout::Constraints::fixed(width, height)));
    engine.place_child(popup_node, s->bounds);
    const float x = s->bounds.x + padding;
    const float y = s->bounds.y + padding;
    const float body = std::max(0.0F, width - 2 * padding);
    const float nav = std::min(t.metrics.header_height, body / 6);
    const auto place = [&](runtime::ComponentId item, runtime::Rect rect) {
        const auto node = services_->components().root(item);
        static_cast<void>(engine.measure_child(node, layout::Constraints::fixed(rect.width, rect.height)));
        engine.place_child(node, rect);
    };
    for (std::size_t i = 0; i < 4; ++i) {
        const float offset = i < 2 ? nav * float(i) : body - nav * float(4 - i);
        place(s->items[i], {x + offset, y, nav, t.metrics.header_height});
    }
    const float title_inset = days ? 2 * nav : nav;
    place(s->items[4], {x + title_inset, y, std::max(0.0F, body - 2 * title_inset), t.metrics.header_height});
    for (std::size_t i = 5; i < s->items.size(); ++i) {
        const std::size_t index = days ? i - 5 : i >= 12 ? i - 12 : 0;
        const int column = static_cast<int>(index % columns);
        const int visual = s->direction == TextDirection::RightToLeft ? columns - 1 - column : column;
        place(s->items[i], {x + visual * body / columns,
                            y + t.metrics.header_height + static_cast<float>(index / columns) * t.metrics.row_height,
                            body / columns, t.metrics.row_height});
    }
    if (s->panel) {
        place(s->custom_panel, {x, y + t.metrics.header_height, body, rows * t.metrics.row_height});
    }
    if (s->header) {
        place(s->custom_header, {x, y, body, t.metrics.header_height});
    }
    place(s->footer, {x, y + t.metrics.header_height + rows * t.metrics.row_height, body, footer_size.height});
    s->needs_layout = false;
    s->anchor = anchor;
    s->clip = available;
    // Footer placement commits after Table's normal pass. Its cached body
    // clip and scroll coordinates must be ready for input in this frame.
    if (std::ranges::any_of(services_->table().mounted(),
                            [&](auto table) { return descendant(*services_, table, s->footer); })) {
        services_->table().position_window_layers(viewport, clip);
    }
}

void DatePickerComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DateState>(id);
        if (!s || !s->visible) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().date_picker();
        const float radius = std::min({t.metrics.radius, s->bounds.width / 2, s->bounds.height / 2});
        auto background = quad(s->bounds, t.colors.background, radius);
        background.opacity = s->transition ? s->transition->value() : 1;
        static_cast<void>(services_->surfaces().update_surface(s->background, std::span{&background, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{s->bounds.x, s->bounds.y, s->bounds.width, s->bounds.height}, radius};
        effects.shadows = t.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, s->clip};
        static_cast<void>(services_->surfaces().update_effects(s->background, effects));
        for (auto child : s->items) {
            const auto* item = services_->components().state<DateItem>(child);
            const bool is_selected = selected(*s, item->date);
            const bool active =
                item->date && s->model.normalized(*item->date) == s->model.normalized(s->model.active());
            const auto r = services_->nodes().require(item->node).bounds;
            const float size = std::min({t.metrics.cell_height, r.width, r.height});
            const runtime::Rect inner{r.x + (r.width - size) / 2, r.y + (r.height - size) / 2, size, size};
            auto q =
                quad(intersect(inner, s->bounds), is_selected ? t.colors.selected : t.colors.hover, t.metrics.radius);
            q.opacity = is_selected || active || (item->date && item->date == s->hovered) ? 1.0F : 0.0F;
            static_cast<void>(services_->surfaces().update_surface(item->surface, std::span{&q, 1}));
            services_->scene_composer().set_fragment(item->hit, {}, item->interaction, s->bounds);
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void DatePicker(DatePickerProps props, std::optional<DatePickerFooter> footer) {
    if (!detail::active_dates) {
        throw std::logic_error("DatePicker requires window component services");
    }
    detail::active_dates->mount(props, footer);
}

void DateRangePicker(DateRangePickerProps props, std::optional<DatePickerFooter> footer) {
    if (!detail::active_dates) {
        throw std::logic_error("DateRangePicker requires window component services");
    }
    detail::active_dates->mount_range(props, footer);
}

void DateRangePicker(DateTimeRangePickerProps props, std::optional<DatePickerFooter> footer) {
    if (!detail::active_dates) {
        throw std::logic_error("DateRangePicker requires window component services");
    }
    detail::active_dates->mount_range(props, footer);
}

} // namespace ryn
