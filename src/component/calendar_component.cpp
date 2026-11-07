#include "component/config_defaults.hpp"
#include "component/calendar_component.hpp"
#include "component/picker_i18n.hpp"
#include "component/date_picker_model.hpp"
#include "component/form_control_binding.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/select.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

namespace ryn::detail {
struct CalendarRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t generation{};
    std::function<bool()> focus;
    std::function<bool(CivilDate, CalendarSelectSource)> select;
    std::function<bool(CalendarMode)> mode;

    void check() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("CalendarRef requires its owner thread");
        }
    }
};

namespace {
thread_local CalendarComponentHost* active_calendar{};

struct Cell final {
    runtime::NodeId node;
    runtime::ComponentId component;
    runtime::ComponentId body;
    runtime::NodeId label_node;
    runtime::NodeId body_node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    Signal<CalendarCellInfo> info{CalendarCellInfo{}};
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    CalendarCellRenderer renderer;
    bool full{};
    bool hovered{};
};

struct CalendarState final {
    runtime::NodeId node;
    CalendarProps props;
    CalendarHeaderRenderer header_renderer;
    CivilDate value;
    CivilDate panel;
    std::optional<CivilDate> today;
    CalendarMode mode{CalendarMode::Month};
    PickerLocale locale;
    PickerLocale format_locale;
    std::optional<CalendarRange> range;
    DisabledDate predicate;
    CalendarCellRenderer renderer;
    CalendarCellRenderer full_renderer;
    CalendarCellRenderer date_renderer;
    CalendarCellRenderer month_renderer;
    bool fullscreen{true};
    bool show_week{};
    bool disabled{};
    bool visible{true};
    bool rtl{};
    bool show_today{};
    bool updating{};
    bool evaluating_rule{};
    bool dispatching{};
    bool pending{true};
    std::uint64_t revision{};
    runtime::ComponentId header;
    std::vector<runtime::ComponentId> rows;
    std::vector<runtime::ComponentId> labels;
    std::vector<Signal<String>> label_text;
    component::RetainedSurfaceId surface;
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<CalendarHeaderInfo> header_info{CalendarHeaderInfo{}};
    Signal<SelectValue> year{SelectValue{1}};
    Signal<SelectValue> month{SelectValue{1}};
    Signal<std::vector<SelectOption>> years{std::vector<SelectOption>{}};
    Signal<std::vector<SelectOption>> months{std::vector<SelectOption>{}};
    Signal<bool> private_disabled{false};
    Signal<bool> previous_disabled{false};
    Signal<bool> next_disabled{false};
    Signal<String> today_label{String{u8"Today"}};
    Signal<String> month_label{String{u8"Month"}};
    Signal<String> year_label{String{u8"Year"}};
    Signal<FlexDirection> header_direction{FlexDirection::LeftToRight};
    Signal<ControlSize> header_size{ControlSize::Middle};
    Signal<LogicalLength> year_width{dp(80)};
    Signal<LogicalLength> month_width{dp(70)};
    std::optional<runtime::ComponentId> month_control;
    std::optional<runtime::ComponentId> today_control;
    float header_height{};
    float label_height{};
    float cell_height{};
    float padding{};
    float cell_width{};
    runtime::Rect clip;
    std::array<theme_runtime::Subscription, 3> subscriptions;
};

struct CalendarDispatchGuard final {
    WindowComponentServices& services;
    runtime::ComponentId id;

    CalendarDispatchGuard(WindowComponentServices& s, runtime::ComponentId component) : services(s), id(component) {
        services.components().state<CalendarState>(id)->dispatching = true;
    }

    ~CalendarDispatchGuard() {
        if (auto* current = services.components().state<CalendarState>(id)) {
            current->dispatching = false;
        }
    }
};

bool disabled_rule(WindowComponentServices& services, runtime::ComponentId id, CivilDate date) {
    auto* state = services.components().state<CalendarState>(id);
    if (!state || state->evaluating_rule) {
        return true;
    }
    const auto predicate = state->predicate;
    if (!predicate) {
        return false;
    }
    state->evaluating_rule = true;
    try {
        const auto result = predicate(date);
        if (auto* current = services.components().state<CalendarState>(id)) {
            current->evaluating_rule = false;
        }
        return result;
    } catch (...) {
        if (auto* current = services.components().state<CalendarState>(id)) {
            current->evaluating_rule = false;
        }
        throw;
    }
}

std::chrono::sys_days days(CivilDate d) {
    return std::chrono::year{d.year()} / d.month() / d.day();
}

CivilDate civil(std::chrono::sys_days d) {
    auto ymd = std::chrono::year_month_day{d};
    return {int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day())};
}

CivilDate add_months(CivilDate d, int n) {
    const auto i = std::clamp((d.year() - 1) * 12 + int(d.month()) - 1 + n, 0, 9999 * 12 - 1);
    unsigned day = d.day();
    while (!CivilDate::valid(i / 12 + 1, unsigned(i % 12 + 1), day)) {
        --day;
    }
    return {i / 12 + 1, unsigned(i % 12 + 1), day};
}

CivilDate clamp(CivilDate d, const std::optional<CalendarRange>& range) {
    return range ? std::clamp(d, range->start, range->end) : d;
}

void validate(CalendarMode m) {
    if (m != CalendarMode::Month && m != CalendarMode::Year) {
        throw std::invalid_argument("Invalid Calendar mode");
    }
}

runtime::SemanticForeground fg(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

String number(int n) {
    return String::from_utf8(std::to_string(n)).value();
}

bool same_panel(CivilDate a, CivilDate b, CalendarMode m) {
    return a.year() == b.year() && (m == CalendarMode::Year || a.month() == b.month());
}

bool hit(runtime::Rect b, float x, float y) {
    return b.width > 0 && b.height > 0 && x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height;
}

runtime::ComponentId box(WindowComponentServices& services, runtime::ComponentBuildContext& b, Content content,
                         Prop<runtime::SemanticForeground> color, Prop<runtime::SemanticTypography> font) {
    auto id = b.mount_component<int>(0);
    auto node = b.root(id);
    services.layout().set_layout(node, layout::BoxLayout{});
    services.nodes().require(node).clip_content = true;
    b.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    b.mount_slot_with_semantic_text_style(id, content, std::move(color), std::move(font));
    if (!services.components().contains(id)) {
        throw std::runtime_error("Calendar slot factory retired its owner");
    }
    return id;
}
} // namespace

CalendarComponentHost::CalendarComponentHost(WindowComponentServices& s) : services_(&s) {
    s.attach(*this);
}

CalendarComponentHost::~CalendarComponentHost() {
    services_->detach(*this);
}

void* CalendarComponentHost::begin_mount() noexcept {
    return std::exchange(active_calendar, this);
}

void CalendarComponentHost::end_mount(void* old) noexcept {
    active_calendar = static_cast<CalendarComponentHost*>(old);
}

void CalendarComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

CalendarSnapshot CalendarComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<CalendarState>(id);
    if (!s) {
        throw std::out_of_range("Calendar is stale");
    }
    CalendarSnapshot result{s->value, s->panel, s->mode, {}, s->rows, s->header};
    for (auto child : s->rows) {
        const auto* row = services_->components().state<Cell>(child);
        result.cells.push_back(row ? row->info.get()
                                   : CalendarCellInfo{CivilDate{}, s->mode, false, false, true, false, {}});
    }
    return result;
}

bool CalendarComponentHost::select(runtime::ComponentId id, CivilDate date, CalendarSelectSource source) {
    auto* s = services_->components().state<CalendarState>(id);
    if (!s || s->dispatching || s->disabled || !s->visible || !services_->focus().state().window_active ||
        !services_->components().scope(id).active() || !services_->interactions().component_active(id)) {
        return false;
    }
    if (s->mode == CalendarMode::Year && source == CalendarSelectSource::Month) {
        date = clamp(
            add_months(s->value, (date.year() - s->value.year()) * 12 + int(date.month()) - int(s->value.month())),
            s->range);
    }
    if (s->range && (date < s->range->start || date > s->range->end)) {
        return false;
    }
    CalendarDispatchGuard dispatch{*services_, id};
    const auto revision = ++s->revision;
    bool disabled{};
    try {
        disabled = disabled_rule(*services_, id, date);
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    s = services_->components().state<CalendarState>(id);
    if (disabled || !s || s->revision != revision) {
        return false;
    }
    const auto previous = s->value;
    const auto previous_mode = s->mode;
    const auto panel_callback = s->props.on_panel_;
    const auto change = s->props.on_change_;
    const auto selected = s->props.on_select_;
    const bool panel_changed = !same_panel(date, previous, previous_mode);
    if (!s->props.value_) {
        s->value = date;
        s->panel = date;
        s->pending = true;
    }
    refresh(id);
    const auto live = [&] {
        const auto* current = services_->components().state<CalendarState>(id);
        return current && services_->components().scope(id).active() &&
               (current->revision == revision ||
                ((current->value == date || current->value == previous) && current->mode == previous_mode));
    };
    try {
        if (date != previous && panel_changed && panel_callback) {
            panel_callback(date, previous_mode);
        }
        if (!live()) {
            return true;
        }
        if (date != previous && change) {
            change(date);
        }
        // Controlled setters may synchronously reconcile the requested date.
        if (!live()) {
            return true;
        }
        if (selected) {
            selected(date, {source});
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    return true;
}

bool CalendarComponentHost::mode(runtime::ComponentId id, CalendarMode value) {
    validate(value);
    auto* s = services_->components().state<CalendarState>(id);
    if (!s || s->dispatching || s->disabled || !s->visible || !services_->interactions().component_active(id)) {
        return false;
    }
    if (value == s->mode) {
        return true;
    }
    CalendarDispatchGuard dispatch{*services_, id};
    const auto callback = s->props.on_panel_;
    const auto date = s->value;
    ++s->revision;
    if (!s->props.mode_) {
        s->mode = value;
        s->pending = true;
    }
    refresh(id);
    if (callback) {
        try {
            callback(date, value);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
    return true;
}

bool CalendarComponentHost::focus(runtime::ComponentId id) {
    const auto* s = services_->components().state<CalendarState>(id);
    if (!s || !services_->interactions().component_active(id) ||
        services_->nodes().require(s->node).bounds.width <= 0 ||
        services_->nodes().require(s->node).bounds.height <= 0) {
        return false;
    }
    for (auto child : s->rows) {
        const auto* row = services_->components().state<Cell>(child);
        if (row && row->info.get().selected && !row->info.get().disabled) {
            services_->focus().defer_focus(row->interaction, input::FocusModality::keyboard);
            return services_->components().contains(id);
        }
    }
    for (auto child : s->rows) {
        const auto* row = services_->components().state<Cell>(child);
        if (row && !row->info.get().disabled) {
            services_->focus().defer_focus(row->interaction, input::FocusModality::keyboard);
            return services_->components().contains(id);
        }
    }
    return false;
}

void CalendarComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<CalendarState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    try {
        const auto t = services_->components().theme_scope(id)->snapshot().calendar();
        s->header_size.set(s->fullscreen ? ControlSize::Middle : ControlSize::Small);
        s->header_direction.set(s->rtl ? FlexDirection::RightToLeft : FlexDirection::LeftToRight);
        s->year_width.set(dp(t.metrics.year_control_width));
        s->month_width.set(dp(t.metrics.month_control_width));
        s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
        s->color.set(fg(t.colors.text));
        s->year.set(SelectValue{s->value.year()});
        s->month.set(SelectValue{int(s->value.month())});
        s->today_label.set(s->locale.today);
        s->month_label.set(s->locale.month);
        s->year_label.set(s->locale.year);
        s->private_disabled.set(s->disabled || !s->visible);
        const auto navigation = clamp(s->value, s->range);
        s->previous_disabled.set(
            s->disabled || add_months(navigation, s->mode == CalendarMode::Month ? -1 : -12) == navigation ||
            clamp(add_months(navigation, s->mode == CalendarMode::Month ? -1 : -12), s->range) == navigation);
        s->next_disabled.set(
            s->disabled || add_months(navigation, s->mode == CalendarMode::Month ? 1 : 12) == navigation ||
            clamp(add_months(navigation, s->mode == CalendarMode::Month ? 1 : 12), s->range) == navigation);
        std::vector<SelectOption> years;
        const int first = std::max(s->range ? s->range->start.year() : 1, s->value.year() - 10);
        const int last = std::min(s->range ? s->range->end.year() : 9999, s->value.year() + 10);
        for (int y = first; y <= last; ++y) {
            years.push_back({y, number(y + s->format_locale.year_offset)});
        }
        s->years.set(std::move(years));
        std::vector<SelectOption> months;
        for (unsigned m = 1; m <= 12; ++m) {
            if (s->range && ((s->value.year() == s->range->start.year() && m < s->range->start.month()) ||
                             (s->value.year() == s->range->end.year() && m > s->range->end.month()))) {
                continue;
            }
            months.push_back({int(m), s->format_locale.short_months[m - 1]});
        }
        s->months.set(std::move(months));
        if (s->month_control &&
            services_->components().set_branch_active(*s->month_control, s->mode == CalendarMode::Month)) {
            services_->mark_scene_structure_dirty();
        }
        if (s->month_control) {
            services_->nodes().require(services_->components().root(*s->month_control)).out_of_flow =
                s->mode != CalendarMode::Month;
        }
        if (s->today_control && services_->components().set_branch_active(*s->today_control, s->show_today)) {
            services_->mark_scene_structure_dirty();
        }
        if (s->today_control) {
            services_->nodes().require(services_->components().root(*s->today_control)).out_of_flow = !s->show_today;
        }
        if (services_->components().set_branch_active(id, s->visible)) {
            services_->mark_scene_structure_dirty();
        }
        services_->nodes().require(s->node).out_of_flow = !s->visible;
        s->pending = true;
        services_->dirty().invalidate(s->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material |
                                                         runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material | runtime::DirtyFlags::Geometry);
        const auto info = s->header_info;
        const CalendarHeaderInfo meta{s->value, s->mode, s->fullscreen, s->locale};
        const bool blocked = s->disabled || !s->visible;
        info.set(meta);
        if (services_->components().contains(id)) {
            services_->set_input_blocked(id, blocked);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

runtime::ComponentId CalendarComponentHost::cell(runtime::ComponentId id, CalendarCellInfo info,
                                                 CalendarCellRenderer renderer, bool full,
                                                 runtime::ComponentBuildContext* initial) {
    runtime::ComponentId result;
    auto content = Content{[&, info, renderer, full] {
        auto& b = runtime::require_component_build_context();
        result = b.mount_component<Cell>();
        auto& row = b.state<Cell>(result);
        row.node = b.root(result);
        row.component = result;
        row.renderer = renderer;
        row.full = full;
        row.info.set(info);
        auto& s = *services_->components().state<CalendarState>(id);
        row.label.set(info.mode == CalendarMode::Year ? s.locale.short_months[info.date.month() - 1]
                                                      : number(int(info.date.day())));
        services_->nodes().require(row.node).clip_content = true;
        row.interaction = services_->interactions().create({result, row.node, {}, true, true, {}, false, false});
        input::InteractionHandlers handlers;
        handlers.target = [this, id, child = result](input::PointerDispatchContext& e) {
            auto* row = services_->components().state<Cell>(child);
            if (!row || row->info.get().disabled) {
                return;
            }
            if (e.kind() == input::PointerEventKind::enter || e.kind() == input::PointerEventKind::leave) {
                row->hovered = e.kind() == input::PointerEventKind::enter;
                services_->dirty().invalidate(row->node, runtime::DirtyFlags::Material);
            }
            if (e.kind() == input::PointerEventKind::down && e.event().button == input::PointerButton::primary) {
                static_cast<void>(e.capture_pointer());
                e.stop_propagation();
            }
            if (e.kind() == input::PointerEventKind::up && e.event().button == input::PointerButton::primary) {
                const auto date = row->info.get().date;
                const auto mode = row->info.get().mode;
                const auto node = services_->nodes().require(row->node);
                const auto clip =
                    services_->nodes().content_clip(row->node, services_->components().state<CalendarState>(id)->clip);
                const bool accept = e.press_origin() == row->interaction &&
                                    hit(graphics::intersect_effect_bounds({node.bounds.x + node.translation.x,
                                                                           node.bounds.y + node.translation.y,
                                                                           node.bounds.width, node.bounds.height},
                                                                          clip),
                                        e.event().x, e.event().y);
                static_cast<void>(e.release_pointer_capture());
                e.stop_propagation();
                if (accept) {
                    select(id, date,
                           mode == CalendarMode::Month ? CalendarSelectSource::Date : CalendarSelectSource::Month);
                }
            }
        };
        services_->interactions().set_handlers(row.interaction, std::move(handlers));
        const auto fragment = b.register_scene_fragment(result, runtime::SceneFragmentPlacement::before_children);
        services_->scene_composer().set_fragment(fragment, {}, row.interaction);
        const auto decoration = b.register_scene_fragment(result, runtime::SceneFragmentPlacement::before_children);
        row.surface = services_->surfaces().create_content_range(decoration, {});
        b.on_resource_cleanup(result,
                              [this, node = row.node, interaction = row.interaction, fragment, surface = row.surface] {
                                  services_->pointer().cancel_interaction(interaction);
                                  services_->interactions().remove(interaction);
                                  services_->scene_composer().remove_fragment(fragment);
                                  services_->surfaces().destroy_content_range(surface);
                                  services_->layout().remove_layout(node);
                              });
        b.mount_slot(result, Content{[&] {
                         auto& c = runtime::require_component_build_context();
                         const auto font = s.font;
                         if (!full) {
                             const auto label =
                                 box(*services_, c, Content{[label = row.label] { Text(TextProps{}.content(label)); }},
                                     row.color, font);
                             row.label_node = services_->components().root(label);
                         }
                         row.body = box(*services_, c, Content{[renderer, info = row.info] {
                             if (renderer) {
                                 SlotContentAccess::function(renderer(bind([info] { return info.get(); })))();
                             }
                         }},
                                        row.color, font);
                         row.body_node = services_->components().root(row.body);
                     }});
        services_->layout().set_layout(
            row.node,
            layout::ComponentLayout{
                [this, child = result, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                    const auto& row = *services_->components().state<Cell>(child);
                    const auto& s = *services_->components().state<CalendarState>(id);
                    const auto& m = services_->components().theme_scope(id)->snapshot().calendar().metrics;
                    const auto padding = s.fullscreen ? m.cell_padding : std::min(m.cell_padding, m.gap / 4);
                    const auto width = std::max(0.0F, limits.max_width - padding * 2);
                    const auto height = std::max(0.0F, limits.max_height - padding * 2);
                    if (row.label_node.valid()) {
                        static_cast<void>(
                            engine.measure_child(row.label_node, {0, width, 0, std::min(height, m.line_height)}));
                    }
                    static_cast<void>(engine.measure_child(
                        row.body_node, {0, width, 0, std::max(0.0F, height - (row.full ? 0 : m.line_height))}));
                    return limits.constrain({limits.max_width, s.cell_height});
                },
                [this, child = result, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                    const auto& row = *services_->components().state<Cell>(child);
                    const auto& m = services_->components().theme_scope(id)->snapshot().calendar().metrics;
                    const auto& state = *services_->components().state<CalendarState>(id);
                    const auto padding = state.fullscreen ? m.cell_padding : std::min(m.cell_padding, m.gap / 4);
                    const auto p = std::min({padding, bounds.width / 2, bounds.height / 2});
                    auto body = runtime::Rect{bounds.x + p, bounds.y + p, std::max(0.0F, bounds.width - 2 * p),
                                              std::max(0.0F, bounds.height - 2 * p)};
                    if (row.label_node.valid()) {
                        const float h = std::min(body.height, m.line_height);
                        engine.place_child(row.label_node, {body.x, body.y, body.width, h});
                        body.y += h;
                        body.height -= h;
                    }
                    engine.place_child(row.body_node, body);
                }});
    }};
    if (initial) {
        initial->mount_slot(id, content);
    } else {
        services_->append_slot(id, content);
    }
    return result;
}

void CalendarComponentHost::cells(runtime::ComponentId id, runtime::ComponentBuildContext* initial) {
    auto* s = services_->components().state<CalendarState>(id);
    if (!s || s->updating || !s->pending) {
        return;
    }
    const auto revision = s->revision;
    const auto previous = s->rows;
    std::vector<runtime::ComponentId> next;
    std::vector<runtime::ComponentId> created;
    std::vector<runtime::ComponentId> retired;
    bool metadata_delivery{};
    bool published{};
    const auto cancel = [&] {
        for (auto child : created) {
            services_->retire_failed_component(child);
        }
        if (auto* current = services_->components().state<CalendarState>(id)) {
            if (std::ranges::any_of(
                    previous, [&](auto child) { return child.valid() && !services_->components().contains(child); })) {
                services_->retire_failed_component(id);
                return;
            }
            current->updating = false;
            current->pending = true;
            services_->request_frame();
        }
    };
    const auto range = s->range;
    DatePickerModel grid{{},
                         false,
                         clamp(s->panel, range),
                         {range ? DatePickerValue{range->start} : DatePickerValue{},
                          range ? DatePickerValue{range->end} : DatePickerValue{},
                          {},
                          {},
                          {},
                          DatePickerMode::Date}};
    grid.view(s->mode == CalendarMode::Month ? DatePickerMode::Date : DatePickerMode::Month, s->locale);
    s->updating = true;
    s->pending = false;
    try {
        bool tab = false;
        for (std::size_t i = 0; i < grid.cell_count(); ++i) {
            const auto date = grid.cell(i);
            if (!date) {
                next.push_back({});
                continue;
            }
            s = services_->components().state<CalendarState>(id);
            if (!s || s->revision != revision) {
                cancel();
                return;
            }
            const auto candidate = s->mode == CalendarMode::Year
                                       ? clamp(add_months(s->value, (date->year() - s->value.year()) * 12 +
                                                                        int(date->month()) - int(s->value.month())),
                                               range)
                                       : *date;
            const bool month_outside =
                s->mode == CalendarMode::Year && range &&
                (date->year() * 12 + int(date->month()) < range->start.year() * 12 + int(range->start.month()) ||
                 date->year() * 12 + int(date->month()) > range->end.year() * 12 + int(range->end.month()));
            const bool disabled =
                s->disabled || month_outside ||
                (s->mode == CalendarMode::Month && range && (*date < range->start || *date > range->end)) ||
                disabled_rule(*services_, id, candidate);
            s = services_->components().state<CalendarState>(id);
            if (!s || s->revision != revision) {
                cancel();
                return;
            }
            CalendarCellInfo info{
                *date,
                s->mode,
                s->mode == CalendarMode::Month ? *date == s->value
                                               : (date->year() == s->value.year() && date->month() == s->value.month()),
                s->today && (s->mode == CalendarMode::Month
                                 ? *date == *s->today
                                 : (date->year() == s->today->year() && date->month() == s->today->month())),
                disabled,
                same_panel(*date, s->panel, s->mode),
                calendar_week(*date, s->locale)};
            auto renderer = s->full_renderer                 ? s->full_renderer
                            : s->renderer                    ? s->renderer
                            : s->mode == CalendarMode::Month ? s->date_renderer
                                                             : s->month_renderer;
            const bool full = bool(s->full_renderer);
            auto found = std::ranges::find_if(previous, [&](auto child) {
                const auto* row = services_->components().state<Cell>(child);
                return row && row->info.get().date == *date && row->info.get().mode == s->mode &&
                       row->renderer == renderer && row->full == full;
            });
            const auto child = found == previous.end() ? cell(id, info, renderer, full, initial) : *found;
            if (found == previous.end()) {
                created.push_back(child);
            }
            s = services_->components().state<CalendarState>(id);
            if (!s || s->revision != revision) {
                cancel();
                return;
            }
            auto* row = services_->components().state<Cell>(child);
            row->label.set(info.mode == CalendarMode::Month ? number(int(date->day()))
                                                            : s->locale.short_months[date->month() - 1]);
            const auto& t = services_->components().theme_scope(id)->snapshot().calendar();
            row->color.set(fg(disabled        ? t.colors.disabled
                              : info.selected ? (s->fullscreen ? t.colors.selected : t.colors.selected_text)
                              : info.in_view  ? t.colors.text
                                              : t.colors.muted));
            services_->interactions().set_eligible(row->interaction, !disabled);
            const bool stop = !disabled && (info.selected || (!tab && i == grid.cell_count() - 1));
            services_->interactions().set_tab_stop(row->interaction, stop);
            tab |= stop;
            next.push_back(child);
            const auto meta = row->info;
            metadata_delivery = true;
            meta.set(info);
            metadata_delivery = false;
            s = services_->components().state<CalendarState>(id);
            if (!s || s->revision != revision) {
                cancel();
                return;
            }
            if (!services_->components().contains(child)) {
                services_->retire_failed_component(id);
                cancel();
                return;
            }
        }
        s = services_->components().state<CalendarState>(id);
        if (!s || s->revision != revision) {
            cancel();
            return;
        }
        for (auto child : previous) {
            if (child.valid() && std::ranges::find(next, child) == next.end()) {
                retired.push_back(child);
            }
        }
        s->rows = next;
        published = true;
        created.clear();
        s->updating = false;
        std::optional<input::InteractionId> tab_stop;
        for (auto child : next) {
            const auto* row = services_->components().state<Cell>(child);
            if (row && !row->info.get().disabled && (!tab_stop || row->info.get().selected)) {
                tab_stop = row->interaction;
            }
        }
        for (auto child : next) {
            const auto* row = services_->components().state<Cell>(child);
            if (row) {
                services_->interactions().set_tab_stop(row->interaction, tab_stop == row->interaction);
            }
        }
        for (auto child : retired) {
            if (services_->components().contains(child)) {
                services_->destroy(child);
            }
        }
        retired.clear();
        for (auto child : next) {
            const auto* row = services_->components().state<Cell>(child);
            if (row) {
                services_->set_input_blocked(child, row->info.get().disabled);
            }
        }
        if (const auto* current = services_->components().state<CalendarState>(id); current && current->pending) {
            services_->request_frame();
        }
    } catch (...) {
        if (metadata_delivery || published) {
            services_->retire_failed_component(id);
        }
        for (auto child : created) {
            services_->retire_failed_component(child);
        }
        for (auto child : retired) {
            services_->retire_failed_component(child);
        }
        if (auto* current = services_->components().state<CalendarState>(id)) {
            current->updating = false;
            current->pending = false;
        }
        throw;
    }
}

runtime::ComponentId CalendarComponentHost::header(runtime::ComponentId id, CalendarHeaderRenderer renderer,
                                                   runtime::ComponentBuildContext* initial) {
    runtime::ComponentId result;
    auto* previous = services_->components().state<CalendarState>(id);
    const auto old_month = previous->month_control;
    const auto old_today = previous->today_control;
    const auto content = Content{[&] {
        auto& c = runtime::require_component_build_context();
        auto& s = *services_->components().state<CalendarState>(id);
        s.month_control.reset();
        s.today_control.reset();
        result = box(*services_, c, Content{[&] {
            FormPrivateEditorScope private_controls;
            if (renderer) {
                SlotContentAccess::function(
                    renderer({bind([info = s.header_info] { return info.get(); }), s.props.ref_}))();
                return;
            }
            const auto navigate = [this, id](int delta) {
                if (const auto* s = services_->components().state<CalendarState>(id)) {
                    select(id, clamp(add_months(s->value, delta * (s->mode == CalendarMode::Month ? 1 : 12)), s->range),
                           CalendarSelectSource::Customize);
                }
            };
            Flex(
                FlexProps{}
                    .wrap(true)
                    .direction(s.header_direction)
                    .gap(dp(4))
                    .align(FlexAlign::Center)
                    .justify(FlexJustify::End),
                [&, navigate] {
                    Button(
                        ButtonProps{}.size(s.header_size).disabled(s.previous_disabled).wave(false).onClick([navigate] {
                            navigate(-1);
                        }),
                        [] { Text(u8"‹"); });
                    Select(SelectProps{}
                               .size(s.header_size)
                               .value(bind([year = s.year] { return SelectSelection{year.get()}; }))
                               .options(s.years)
                               .disabled(s.private_disabled)
                               .layout(LayoutStyle{}.width(s.year_width))
                               .onChange([this, id](SelectSelection v) {
                                   if (!v) {
                                       return;
                                   }
                                   const auto y = std::get_if<std::int64_t>(&v->storage());
                                   const auto* s = services_->components().state<CalendarState>(id);
                                   if (y && s) {
                                       select(id,
                                              clamp(add_months(s->value, (int(*y) - s->value.year()) * 12), s->range),
                                              CalendarSelectSource::Year);
                                   }
                               }));
                    auto& build = runtime::require_component_build_context();
                    s.month_control = box(*services_, build, Content{[&] {
                        Select(SelectProps{}
                                   .size(s.header_size)
                                   .value(bind([month = s.month] { return SelectSelection{month.get()}; }))
                                   .options(s.months)
                                   .disabled(s.private_disabled)
                                   .layout(LayoutStyle{}.width(s.month_width))
                                   .onChange([this, id](SelectSelection v) {
                                       if (!v) {
                                           return;
                                       }
                                       const auto m = std::get_if<std::int64_t>(&v->storage());
                                       const auto* s = services_->components().state<CalendarState>(id);
                                       if (m && s) {
                                           select(
                                               id,
                                               clamp(add_months(s->value, int(*m) - int(s->value.month())), s->range),
                                               CalendarSelectSource::Month);
                                       }
                                   }));
                    }},
                                          s.color, s.font);
                    Button(ButtonProps{}.size(s.header_size).disabled(s.next_disabled).wave(false).onClick([navigate] {
                        navigate(1);
                    }),
                           [] { Text(u8"›"); });
                    Button(
                        ButtonProps{}.size(s.header_size).disabled(s.private_disabled).wave(false).onClick([this, id] {
                            this->mode(id, CalendarMode::Month);
                        }),
                        [label = s.month_label] { Text(TextProps{}.content(label)); });
                    Button(
                        ButtonProps{}.size(s.header_size).disabled(s.private_disabled).wave(false).onClick([this, id] {
                            this->mode(id, CalendarMode::Year);
                        }),
                        [label = s.year_label] { Text(TextProps{}.content(label)); });
                    s.today_control = box(*services_, build, Content{[&] {
                        Button(ButtonProps{}
                                   .size(s.header_size)
                                   .disabled(s.private_disabled)
                                   .wave(false)
                                   .onClick([this, id] {
                                       const auto* s = services_->components().state<CalendarState>(id);
                                       if (!s || !s->props.clock_) {
                                           return;
                                       }
                                       const auto clock = s->props.clock_;
                                       const auto now = clock();
                                       if (services_->components().contains(id)) {
                                           select(id, now, CalendarSelectSource::Customize);
                                       }
                                   }),
                               [label = s.today_label] { Text(TextProps{}.content(label)); });
                    }},
                                          s.color, s.font);
                });
        }},
                     s.color, s.font);
    }};
    try {
        if (initial) {
            initial->mount_slot(id, content);
        } else {
            services_->append_slot(id, content);
        }
    } catch (...) {
        if (auto* current = services_->components().state<CalendarState>(id)) {
            current->month_control = old_month;
            current->today_control = old_today;
        }
        throw;
    }
    if (!services_->components().contains(id) || !services_->components().contains(result)) {
        throw std::runtime_error("Calendar header factory retired its owner");
    }
    services_->layout().set_layout(services_->components().root(result), layout::BoxLayout{{}, true});
    return result;
}

void CalendarComponentHost::mount(const CalendarProps& props) {
    const auto locale_source =
        picker_locale_prop(props.locale_, runtime::require_component_build_context().config_scope());
    const auto format_locale_source =
        picker_format_locale_prop(props.locale_, runtime::require_component_build_context().config_scope());
    const auto locale = read_prop(locale_source);
    locale.validate();
    const auto range = read_prop(props.range_);
    if (range && range->start > range->end) {
        throw std::invalid_argument("Calendar validRange is reversed");
    }
    if (read_prop(props.show_today_) && !props.clock_) {
        throw std::invalid_argument("Calendar Today requires an explicit clock");
    }
    const auto today = props.clock_ ? std::optional{props.clock_()} : std::nullopt;
    const auto value =
        props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(today.value_or(CivilDate{}));
    const auto mode = props.mode_ ? read_prop(*props.mode_) : props.default_mode_;
    validate(mode);
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<CalendarState>();
    auto& s = b.state<CalendarState>(id);
    s.node = b.root(id);
    s.props = props;
    s.header_renderer = read_prop(props.header_);
    s.value = value;
    s.panel = clamp(value, range);
    s.today = today;
    s.mode = mode;
    s.locale = locale;
    s.format_locale = read_prop(format_locale_source);
    s.range = range;
    const auto reference = props.ref_.state_;
    reference->check();
    const auto generation = ++reference->generation;
    reference->focus = [this, id] {
        return focus(id);
    };
    reference->select = [this, id](auto date, auto source) {
        return select(id, date, source);
    };
    reference->mode = [this, id](auto mode) {
        return this->mode(id, mode);
    };
    s.predicate = read_prop(props.disabled_date_);
    s.renderer = read_prop(props.cell_);
    s.full_renderer = read_prop(props.full_cell_);
    s.date_renderer = read_prop(props.date_cell_);
    s.month_renderer = read_prop(props.month_cell_);
    s.fullscreen = read_prop(props.fullscreen_);
    s.show_week = read_prop(props.show_week_);
    s.disabled = read_prop(props.disabled_);
    s.visible = read_prop(props.visible_);
    s.show_today = read_prop(props.show_today_);
    s.rtl = read_prop(props.direction_) == TextDirection::RightToLeft;
    runtime::connect_layout_style(b.scope(id), props.layout_, s.node, services_->nodes(), services_->dirty());
    services_->nodes().require(s.node).clip_content = true;
    const auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.surface = services_->surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(id, [this, node = s.node, surface = s.surface, reference, generation] {
        if (reference->generation == generation) {
            ++reference->generation;
            reference->focus = {};
            reference->select = {};
            reference->mode = {};
        }
        services_->surfaces().destroy_content_range(surface);
        services_->layout().remove_layout(node);
    });
    refresh(id);
    b.mount_slot(id, Content{[&] {
                     auto& c = runtime::require_component_build_context();
                     s.header = header(id, read_prop(props.header_), &c);
                     for (int i = 0; i < 13; ++i) {
                         s.label_text.emplace_back(String{});
                         const auto label = s.label_text.back();
                         s.labels.push_back(box(*services_, c, Content{[label] { Text(TextProps{}.content(label)); }},
                                                s.color, s.font));
                     }
                 }});
    cells(id, &b);
    if (!services_->components().contains(id)) {
        throw std::runtime_error("Calendar cell metadata retired its owner");
    }
    services_->layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](auto& e, auto, auto c) { return measure(id, e, c); },
                                        [this, id](auto& e, auto, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    const auto connect = [&]<typename T>(const Prop<T>& prop, auto field, auto validate_value) {
        connect_prop(b.scope(id), prop, [this, id, field, validate_value](T value) {
            validate_value(value);
            if (auto* s = services_->components().state<CalendarState>(id)) {
                s->*field = std::move(value);
                ++s->revision;
                refresh(id);
            }
        });
    };
    const auto noop = [](const auto&) {
    };
    connect(props.fullscreen_, &CalendarState::fullscreen, noop);
    connect(props.show_week_, &CalendarState::show_week, noop);
    connect(props.disabled_, &CalendarState::disabled, noop);
    connect(props.visible_, &CalendarState::visible, noop);
    connect(props.show_today_, &CalendarState::show_today, [clock = props.clock_](bool v) {
        if (v && !clock) {
            throw std::invalid_argument("Calendar Today requires a clock");
        }
    });
    connect(locale_source, &CalendarState::locale, [](const auto& v) { v.validate(); });
    connect(format_locale_source, &CalendarState::format_locale, [](const auto& v) { v.validate(); });
    connect(props.range_, &CalendarState::range, [](const auto& v) {
        if (v && v->start > v->end) {
            throw std::invalid_argument("Calendar range reversed");
        }
    });
    connect(props.disabled_date_, &CalendarState::predicate, noop);
    connect(props.cell_, &CalendarState::renderer, noop);
    connect(props.full_cell_, &CalendarState::full_renderer, noop);
    connect(props.date_cell_, &CalendarState::date_renderer, noop);
    connect(props.month_cell_, &CalendarState::month_renderer, noop);
    connect_prop(b.scope(id), props.direction_, [this, id](TextDirection d) {
        if (auto* s = services_->components().state<CalendarState>(id)) {
            s->rtl = d == TextDirection::RightToLeft;
            refresh(id);
        }
    });
    if (props.value_) {
        connect_prop(b.scope(id), *props.value_, [this, id](CivilDate v) {
            if (auto* s = services_->components().state<CalendarState>(id)) {
                ++s->revision;
                s->value = v;
                s->panel = clamp(v, s->range);
                refresh(id);
            }
        });
    }
    if (props.mode_) {
        connect(*props.mode_, &CalendarState::mode, [](auto v) { validate(v); });
    }
    connect_prop(b.scope(id), props.header_, [this, id](CalendarHeaderRenderer renderer) {
        auto* current = services_->components().state<CalendarState>(id);
        if (!current || current->header_renderer == renderer) {
            return;
        }
        const auto replacement = header(id, renderer);
        current = services_->components().state<CalendarState>(id);
        if (!current) {
            return;
        }
        const auto previous = current->header;
        current->header = replacement;
        current->header_renderer = std::move(renderer);
        services_->destroy(previous);
        refresh(id);
    });
    const auto theme = b.theme_scope();
    for (std::size_t i = 0; i < 3; ++i) {
        s.subscriptions[i] = theme->capture([this, id, geometry = i == 1](auto) { refresh(id, geometry); },
                                            [theme, i] {
                                                if (i == 0) {
                                                    static_cast<void>(theme->calendar_colors());
                                                } else if (i == 1) {
                                                    static_cast<void>(theme->calendar_metrics());
                                                } else {
                                                    static_cast<void>(theme->calendar_effects());
                                                }
                                            });
    }
    refresh(id);
}

void CalendarComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        cells(id);
    }
}

runtime::Size CalendarComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                             layout::Constraints c) {
    auto& s = *services_->components().state<CalendarState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().calendar().metrics;
    const auto width = std::clamp(s.fullscreen ? m.panel_width : m.mini_width, c.min_width, c.max_width);
    s.padding = std::min(m.padding, width / 2);
    const auto inside = std::max(0.0F, width - 2 * s.padding);
    const auto header = engine.measure_child(services_->components().root(s.header), {0, inside, 0, c.max_height});
    s.header_height = std::min(c.max_height, std::max(m.header_height, header.height));
    const auto columns = s.mode == CalendarMode::Year ? 3 : (s.show_week ? 8 : 7);
    s.cell_width = inside / float(columns);
    s.label_height = s.mode == CalendarMode::Month ? m.line_height + m.gap : 0;
    const auto row_count = s.mode == CalendarMode::Year ? 4 : 6;
    const auto available = std::max(0.0F, c.max_height - s.header_height - s.label_height - 2 * s.padding);
    const auto cell_padding = s.fullscreen ? m.cell_padding : std::min(m.cell_padding, m.gap / 4);
    float rich_height{};
    for (auto child : s.rows) {
        const auto* row = services_->components().state<Cell>(child);
        if (row && row->renderer) {
            const auto natural = engine.measure_child(
                row->body_node, {0, std::max(0.0F, s.cell_width - 2 * cell_padding), 0, available / float(row_count)});
            rich_height = std::max(rich_height, natural.height + (row->full ? 0 : m.line_height) + 2 * cell_padding);
        }
    }
    const auto natural = s.fullscreen ? std::max(m.cell_height, m.line_height + 2 * cell_padding)
                                      : std::max(m.mini_cell_height, m.line_height + 2 * cell_padding);
    s.cell_height = std::min(
        available / float(row_count),
        s.fullscreen ? std::max(natural, rich_height)
                     : std::max({natural, rich_height, (m.mini_content_height - s.label_height) / float(row_count)}));
    for (auto child : s.rows) {
        if (child.valid()) {
            static_cast<void>(engine.measure_child(services_->components().root(child),
                                                   layout::Constraints::fixed(s.cell_width, s.cell_height)));
        }
    }
    for (std::size_t i = 0; i < s.labels.size(); ++i) {
        if (i < 7) {
            s.label_text[i].set(s.locale.weekdays[(unsigned(i) + s.locale.first_day) % 7]);
        } else {
            const auto first = std::min((i - 7) * 7, s.rows.empty() ? std::size_t{} : s.rows.size() - 1);
            const auto* row = s.rows.empty() ? nullptr : services_->components().state<Cell>(s.rows[first]);
            s.label_text[i].set(row ? number(int(row->info.get().week.week)) : String{});
        }
        const bool shown = s.mode == CalendarMode::Month && (i < 7 || s.show_week);
        if (services_->components().set_branch_active(s.labels[i], shown)) {
            services_->mark_scene_structure_dirty();
        }
        static_cast<void>(
            engine.measure_child(services_->components().root(s.labels[i]),
                                 layout::Constraints::fixed(shown ? s.cell_width : 0,
                                                            shown ? (i < 7 ? s.label_height : s.cell_height) : 0)));
    }
    return c.constrain({width, s.header_height + s.label_height + s.cell_height * float(row_count) + 2 * s.padding});
}

void CalendarComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<CalendarState>(id);
    const auto width = std::max(0.0F, bounds.width - 2 * s.padding);
    engine.place_child(services_->components().root(s.header),
                       {bounds.x + s.padding, bounds.y + s.padding, width, s.header_height});
    const auto origin = runtime::Point{bounds.x + s.padding, bounds.y + s.padding + s.header_height};
    const auto columns = s.mode == CalendarMode::Year ? 3 : 7;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (!s.rows[i].valid()) {
            continue;
        }
        int column = int(i % std::size_t(columns)) + (s.mode == CalendarMode::Month && s.show_week ? 1 : 0);
        const auto total = columns + (s.mode == CalendarMode::Month && s.show_week ? 1 : 0);
        if (s.rtl) {
            column = total - 1 - column;
        }
        engine.place_child(services_->components().root(s.rows[i]),
                           {origin.x + float(column) * s.cell_width,
                            origin.y + s.label_height + float(i / std::size_t(columns)) * s.cell_height, s.cell_width,
                            s.cell_height});
    }
    for (std::size_t i = 0; i < s.labels.size(); ++i) {
        auto column = i < 7 ? int(i) + (s.show_week ? 1 : 0) : 0;
        if (s.rtl) {
            column = (s.show_week ? 8 : 7) - 1 - column;
        }
        const auto y = i < 7 ? origin.y : origin.y + s.label_height + float(i - 7) * s.cell_height;
        engine.place_child(
            services_->components().root(s.labels[i]),
            {origin.x + float(column) * s.cell_width, y, s.cell_width, i < 7 ? s.label_height : s.cell_height});
    }
}

bool CalendarComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || input::has_modifier(event.modifiers, input::KeyModifier::alt) ||
        input::has_modifier(event.modifiers, input::KeyModifier::control) ||
        input::has_modifier(event.modifiers, input::KeyModifier::meta)) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    if (!focused) {
        return false;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<CalendarState>(id);
        if (!s || s->disabled || !services_->interactions().component_active(id)) {
            continue;
        }
        const auto found = std::ranges::find_if(s->rows, [&](auto child) {
            auto* row = services_->components().state<Cell>(child);
            return row && row->interaction == *focused;
        });
        if (found == s->rows.end()) {
            continue;
        }
        const auto* row = services_->components().state<Cell>(*found);
        const auto date = row->info.get().date;
        auto next = date;
        if (event.key == input::Key::enter || event.key == input::Key::space) {
            select(id, date, s->mode == CalendarMode::Month ? CalendarSelectSource::Date : CalendarSelectSource::Month);
            return true;
        }
        int count{};
        if (event.key == input::Key::left) {
            count = s->rtl ? 1 : -1;
        } else if (event.key == input::Key::right) {
            count = s->rtl ? -1 : 1;
        } else if (event.key == input::Key::up) {
            count = s->mode == CalendarMode::Month ? -7 : -3;
        } else if (event.key == input::Key::down) {
            count = s->mode == CalendarMode::Month ? 7 : 3;
        } else if (event.key == input::Key::page_up || event.key == input::Key::page_down) {
            next = add_months(date, (event.key == input::Key::page_up ? -1 : 1) *
                                        (s->mode == CalendarMode::Year ||
                                                 input::has_modifier(event.modifiers, input::KeyModifier::shift)
                                             ? 12
                                             : 1));
        } else if (event.key == input::Key::home || event.key == input::Key::end) {
            if (s->mode == CalendarMode::Year) {
                next = CivilDate{date.year(), event.key == input::Key::home ? 1U : 12U, 1};
            } else {
                const int offset = int((std::chrono::weekday{days(date)}.c_encoding() + 7 - s->locale.first_day) % 7);
                next = civil(
                    std::clamp(days(date) + std::chrono::days{event.key == input::Key::home ? -offset : 6 - offset},
                               days(CivilDate{1, 1, 1}), days(CivilDate{9999, 12, 31})));
            }
        } else {
            return false;
        }
        if (count) {
            next = s->mode == CalendarMode::Year
                       ? add_months(date, count)
                       : civil(std::clamp(days(date) + std::chrono::days{count}, days(CivilDate{1, 1, 1}),
                                          days(CivilDate{9999, 12, 31})));
        }
        next = clamp(next, s->range);
        s->panel = next;
        s->pending = true;
        cells(id);
        s = services_->components().state<CalendarState>(id);
        if (!s) {
            return true;
        }
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        for (auto child : s->rows) {
            const auto* candidate = services_->components().state<Cell>(child);
            if (candidate && candidate->info.get().date == next && !candidate->info.get().disabled) {
                services_->focus().defer_focus(candidate->interaction, input::FocusModality::keyboard);
                break;
            }
        }
        return true;
    }
    return false;
}

void CalendarComponentHost::on_input_boundary_changed() {}

void CalendarComponentHost::on_window_active(bool) {}

void CalendarComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<CalendarState>(id);
        if (!s) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, clip);
        const auto& t = services_->components().theme_scope(id)->snapshot().calendar();
        const auto node = services_->nodes().require(s->node);
        const auto opacity =
            services_->components().branch_active(id) ? services_->nodes().content_opacity(s->node) : 0;
        std::vector<graphics::RoundedEffectInstance> panel;
        if (node.bounds.width > 0 && node.bounds.height > 0) {
            const graphics::LogicalRoundedRect shape{
                node.bounds, std::min(t.metrics.radius, std::min(node.bounds.width, node.bounds.height) / 2)};
            for (std::size_t si = 0; si < t.shadow.size(); ++si) {
                const auto& shadow = t.shadow[si];
                auto effect =
                    graphics::make_shadow_effect(shape, shadow, node.translation, graphics::EffectClip{1, s->clip});
                effect.material.opacity = opacity;
                panel.push_back(effect);
            }
            auto fill = graphics::make_corner_fill_effects(shape, {true, true, true, true}, t.colors.full_background,
                                                           opacity, node.translation, graphics::EffectClip{1, s->clip});
            panel.insert(panel.end(), fill.begin(), fill.end());
            runtime::Rect body{node.bounds.x + s->padding, node.bounds.y + s->padding + s->header_height,
                               std::max(0.0F, node.bounds.width - 2 * s->padding),
                               std::max(0.0F, node.bounds.height - 2 * s->padding - s->header_height)};
            if (body.width > 0 && body.height > 0) {
                auto background =
                    graphics::make_corner_fill_effects({body, 0}, {true, true, true, true}, t.colors.panel_background,
                                                       opacity, node.translation, graphics::EffectClip{1, s->clip});
                panel.insert(panel.end(), background.begin(), background.end());
            }
        }
        services_->surfaces().update_content_effects(s->surface, panel);
        for (auto child : s->rows) {
            const auto* row = services_->components().state<Cell>(child);
            if (!row) {
                continue;
            }
            const auto info = row->info.get();
            const auto n = services_->nodes().require(row->node);
            const auto c = services_->nodes().content_clip(row->node, s->clip);
            const auto p = std::min({t.metrics.gap, n.bounds.width / 2, n.bounds.height / 2});
            runtime::Rect r{n.bounds.x + p, n.bounds.y + p, std::max(0.0F, n.bounds.width - 2 * p),
                            std::max(0.0F, n.bounds.height - 2 * p)};
            std::vector<graphics::RoundedEffectInstance> effects;
            if (r.width > 0 && r.height > 0) {
                auto fill = graphics::make_corner_fill_effects(
                    {r, std::min(t.metrics.radius, std::min(r.width, r.height) / 2)}, {true, true, true, true},
                    info.selected  ? (s->fullscreen ? t.colors.item_active_background : t.colors.selected)
                    : row->hovered ? t.colors.hover
                                   : t.colors.panel_background,
                    opacity, n.translation, graphics::EffectClip{1, c});
                effects.insert(effects.end(), fill.begin(), fill.end());
                const auto focused =
                    services_->focus().state().focused == row->interaction && services_->focus().state().focus_visible;
                if (info.today || focused) {
                    effects.push_back(graphics::make_outline_effect(
                        {r, std::min(t.metrics.radius, std::min(r.width, r.height) / 2)},
                        focused ? t.metrics.focus_width : t.metrics.line_width, 0,
                        focused ? t.colors.focus : t.colors.today, opacity, n.translation, graphics::EffectClip{1, c}));
                } else if (s->fullscreen && !row->full) {
                    auto line = graphics::make_corner_fill_effects(
                        {{r.x, r.y, r.width, std::min(r.height, t.metrics.line_width)}, 0}, {true, true, true, true},
                        t.colors.border, opacity, n.translation, graphics::EffectClip{1, c});
                    effects.insert(effects.end(), line.begin(), line.end());
                }
            }
            services_->surfaces().update_content_effects(row->surface, effects);
        }
    }
}
} // namespace ryn::detail

namespace ryn {
CalendarRef::CalendarRef() : state_(std::make_shared<detail::CalendarRefState>()) {}

bool CalendarRef::focus() const {
    state_->check();
    const auto f = state_->focus;
    return f && f();
}

bool CalendarRef::select(CivilDate date, CalendarSelectSource source) const {
    state_->check();
    const auto f = state_->select;
    return f && f(date, source);
}

bool CalendarRef::changeMode(CalendarMode mode) const {
    state_->check();
    const auto f = state_->mode;
    return f && f(mode);
}

void Calendar(CalendarProps props) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_calendar) {
        throw std::logic_error("Calendar requires an active window component host");
    }
    detail::active_calendar->mount(props);
}
} // namespace ryn
