#include "component/pagination_component.hpp"
#include "component/responsive_layout.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/icon.hpp>
#include <ryn/input.hpp>
#include <ryn/select.hpp>
#include <charconv>
#include <limits>
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local PaginationComponentHost* active_pagination{};

struct PageItem final {
    runtime::ComponentId component;
    Signal<String> label{String{}};
    Signal<IconName> icon{IconName::EllipsisOutlined};
    Signal<bool> icon_visible{false};
    Signal<bool> disabled{false};
    Signal<LogicalLength> extent{dp(32)};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<std::size_t> page{1};
    Signal<PaginationItemType> type{PaginationItemType::Page};
    Signal<bool> active{false};
    std::size_t target{};
    bool shown{};
};

struct PaginationState final {
    runtime::NodeId node;
    std::array<PageItem, 11> items;
    std::size_t total{};
    std::size_t current{1};
    std::size_t pages{1};
    std::size_t page_size{10};
    PaginationLocale locale;
    TextDirection direction{TextDirection::LeftToRight};
    bool less{};
    Signal<TextDirection> child_direction{TextDirection::LeftToRight};
    bool controlled{};
    bool controlled_size{};
    bool simple{};
    bool read_only{};
    bool responsive{};
    float window_width{};
    bool compact{};
    bool quick{};
    std::optional<bool> show_size_changer;
    PaginationAlign align{PaginationAlign::Start};
    runtime::ComponentId size_changer;
    runtime::ComponentId jumper;
    runtime::ComponentId total_component;
    runtime::ComponentId simple_total;
    Signal<String> draft{String{}};
    Signal<String> total_text{String{}};
    Signal<String> simple_text{String{}};
    Signal<bool> controls_disabled{false};
    Signal<bool> editor_read_only{false};
    Signal<ControlSize> control_size{ControlSize::Middle};
    Signal<SelectSelection> selected_size{std::nullopt};
    Signal<std::vector<SelectOption>> options{std::vector<SelectOption>{}};
    std::vector<std::size_t> size_options;
    std::vector<std::pair<runtime::ComponentId, float>> flow;
    std::function<String(std::size_t, PaginationRange)> total_render;
    std::function<void(std::size_t, std::size_t)> on_size_change;
    bool disabled{};
    bool hide_single{};
    bool hidden{};
    ControlSize size{ControlSize::Middle};
    float extent{32};
    float simple_count_width{72};
    float gap{8};
    std::array<theme_runtime::Subscription, 2> subscriptions;
    std::function<void(std::size_t, std::size_t)> on_change;
};

void valid_size(std::size_t size) {
    if (size == 0) {
        throw std::invalid_argument("Pagination pageSize must be positive");
    }
}

void valid_control_size(ControlSize value) {
    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
        throw std::invalid_argument("Pagination size is invalid");
    }
}
} // namespace

PaginationComponentHost::PaginationComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

PaginationComponentHost::~PaginationComponentHost() {
    services_->detach(*this);
}

void* PaginationComponentHost::begin_mount() noexcept {
    auto* previous = active_pagination;
    active_pagination = this;
    return previous;
}

void PaginationComponentHost::end_mount(void* previous) noexcept {
    active_pagination = static_cast<PaginationComponentHost*>(previous);
}

void PaginationComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

PaginationSnapshot PaginationComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<PaginationState>(id);
    if (!s) {
        throw std::out_of_range("Pagination is stale");
    }
    PaginationSnapshot result{s->current,      s->pages,  s->page_size,       {},        {},        s->hidden,
                              s->size_changer, s->jumper, s->total_component, s->simple, s->compact};
    for (const auto& item : s->items) {
        if (item.shown && !s->hidden) {
            result.items.push_back(item.component);
            result.targets.push_back(item.target);
        }
    }
    return result;
}

void PaginationComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<PaginationState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    s->pages = std::max(std::size_t{1}, s->total / s->page_size + (s->total % s->page_size != 0));
    s->current = std::clamp(s->current, std::size_t{1}, s->pages);
    s->hidden = s->hide_single && s->pages == 1;
    auto& root = services_->nodes().require(s->node);
    if (root.out_of_flow != s->hidden) {
        root.out_of_flow = s->hidden;
        if (s->hidden) {
            // Box/Flex skip out-of-flow children, including placement, so clear
            // retained geometry here while preserving the external layout style.
            root.measured_size = {};
            root.layout_size = {};
            root.bounds.width = 0;
            root.bounds.height = 0;
        }
        if (root.parent) {
            services_->dirty().invalidate(*root.parent, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout);
        }
        geometry = true;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().pagination();
    s->extent = (s->size == ControlSize::Small || s->compact) ? token.metrics.item_size_small
                : s->size == ControlSize::Large               ? token.metrics.item_size_large
                                                              : token.metrics.item_size;
    s->gap = token.metrics.gap;
    s->control_size.set(s->compact ? ControlSize::Small : s->size);
    s->controls_disabled.set(s->disabled);
    s->editor_read_only.set(s->read_only);
    s->selected_size.set(SelectValue{String::from_utf8(std::to_string(s->page_size)).value()});
    std::vector<SelectOption> options;
    auto sizes = s->size_options;
    sizes.push_back(s->page_size);
    std::ranges::sort(sizes);
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
    for (auto size : sizes) {
        options.push_back(
            {SelectValue{String::from_utf8(std::to_string(size)).value()},
             String::from_utf8(std::to_string(size) + std::string(s->locale.items_per_page.bytes())).value(), false});
    }
    s->options.set(std::move(options));
    s->simple_text.set(
        String::from_utf8(std::string(s->locale.page_separator.bytes()) + std::to_string(s->pages)).value());
    if (s->simple) {
        s->draft.set(String::from_utf8(std::to_string(s->current)).value());
    }
    if (s->total_render) {
        // Copy the callback and its arguments: it may destroy this Pagination.
        const auto render = s->total_render;
        const auto total = s->total;
        const PaginationRange range{total ? (s->current - 1) * s->page_size + 1 : 0,
                                    total ? total - std::min(total, (s->pages - s->current) * s->page_size) : 0};
        const auto first = range.first;
        const auto last = total ? first + std::min(s->page_size, total - first + 1) - 1 : 0;
        auto text = render(total, {first, last});
        s = services_->components().state<PaginationState>(id);
        if (!s) {
            return;
        }
        s->total_text.set(std::move(text));
    }
    s->flow.clear();
    const auto auxiliary = [&](runtime::ComponentId child, bool shown, float width) {
        if (services_->components().set_branch_active(child, shown && !s->hidden)) {
            services_->mark_scene_structure_dirty();
        }
        services_->nodes().require(services_->components().root(child)).out_of_flow = !shown || s->hidden;
        if (shown) {
            s->flow.emplace_back(child, width);
        }
    };
    auxiliary(s->total_component, static_cast<bool>(s->total_render), 0);
    std::vector<std::pair<std::size_t, bool>> targets;
    const std::size_t radius = s->less ? 1 : 2;
    const std::size_t jump_step = radius * 2 + 1;
    if (s->pages <= (s->less ? 5 : 9)) {
        for (std::size_t page = 1; page <= s->pages; ++page) {
            targets.emplace_back(page, false);
        }
    } else {
        const auto start = s->current > radius + 1 ? s->current - radius : std::size_t{2};
        const auto end = s->current >= s->pages - radius ? s->pages - 1 : s->current + radius;
        targets.emplace_back(1, false);
        if (start > 2) {
            targets.emplace_back(s->current > jump_step ? s->current - jump_step : 1, true);
        }
        for (auto page = start; page <= end; ++page) {
            targets.emplace_back(page, false);
        }
        if (end < s->pages - 1) {
            targets.emplace_back(s->current > s->pages - jump_step ? s->pages : s->current + jump_step, true);
        }
        targets.emplace_back(s->pages, false);
    }
    for (std::size_t index = 0; index < s->items.size(); ++index) {
        auto& item = s->items[index];
        item.shown = index == 0 || index == 10 || (!s->simple && index <= targets.size());
        const bool jump = index > 0 && index <= targets.size() && targets[index - 1].second;
        item.target = index == 0                ? (s->current > 1 ? s->current - 1 : 1)
                      : index == 10             ? (s->current < s->pages ? s->current + 1 : s->pages)
                      : index <= targets.size() ? targets[index - 1].first
                                                : 0;
        const bool active = index > 0 && index < 10 && !jump && item.target == s->current;
        item.page.set(item.target);
        item.active.set(active);
        item.type.set(index == 0    ? PaginationItemType::Previous
                      : index == 10 ? PaginationItemType::Next
                      : jump
                          ? (item.target < s->current ? PaginationItemType::JumpPrevious : PaginationItemType::JumpNext)
                          : PaginationItemType::Page);
        const bool navigation_icon = index == 0 || index == 10 || jump;
        item.icon.set(
            index == 0 ? (s->direction == TextDirection::RightToLeft ? IconName::RightOutlined : IconName::LeftOutlined)
            : index == 10
                ? (s->direction == TextDirection::RightToLeft ? IconName::LeftOutlined : IconName::RightOutlined)
                : IconName::EllipsisOutlined);
        item.icon_visible.set(navigation_icon);
        item.label.set(navigation_icon ? String{} : String::from_utf8(std::to_string(item.target)).value());
        item.disabled.set(s->disabled || (index == 0 && s->current == 1) || (index == 10 && s->current == s->pages));
        item.extent.set(dp(s->extent));
        ThemeConfig config;
        config.alias.color_text_disabled = token.colors.disabled;
        auto& b = config.button.tokens;
        b.default_color = s->disabled ? token.colors.disabled : active ? token.colors.active : token.colors.item;
        b.default_background = active ? token.colors.active_background : token.colors.item_background;
        b.default_border_color = active ? token.colors.border : Color{0, 0, 0, 0};
        b.border_radius = dp(token.metrics.border_radius);
        b.default_shadow = ShadowList{};
        b.padding_inline = dp(0);
        b.icon_gap = dp(0);
        item.theme.set(std::move(config));
        if (services_->components().set_branch_active(item.component, item.shown && !s->hidden)) {
            services_->mark_scene_structure_dirty();
            geometry = true;
        }
    }
    if (s->simple) {
        // Prev, editor, page count, next; all are retained across mode changes.
        s->flow.emplace_back(s->items[0].component, s->extent);
        auxiliary(s->jumper, true, 64);
        auxiliary(s->simple_total, true, s->simple_count_width);
        s->flow.emplace_back(s->items[10].component, s->extent);
    } else {
        for (const auto& item : s->items) {
            if (item.shown) {
                s->flow.emplace_back(item.component, s->extent);
            }
        }
        auxiliary(s->simple_total, false, 0);
        auxiliary(s->jumper, s->quick && !s->compact, 80);
    }
    auxiliary(s->size_changer, s->show_size_changer.value_or(s->total > 50), 112);
    if (geometry) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
}

void PaginationComponentHost::request(runtime::ComponentId id, std::size_t index) {
    auto* s = services_->components().state<PaginationState>(id);
    if (!s || s->disabled || s->hidden || !s->items[index].shown) {
        return;
    }
    change(id, s->items[index].target, s->page_size);
}

void PaginationComponentHost::change(runtime::ComponentId id, std::size_t page, std::size_t size) {
    auto* s = services_->components().state<PaginationState>(id);
    if (!s || s->disabled || s->hidden || !services_->components().scope(id).active() || size == 0) {
        return;
    }
    const auto pages = std::max(std::size_t{1}, s->total / size + (s->total % size != 0));
    page = std::clamp(page, std::size_t{1}, pages);
    const bool size_changed = size != s->page_size;
    if (!size_changed && page == s->current) {
        refresh(id, true);
        return;
    }
    const auto callback = s->on_change;
    const auto size_callback = s->on_size_change;
    if (!s->controlled) {
        s->current = page;
    }
    if (!s->controlled_size) {
        s->page_size = size;
    }
    refresh(id, true);
    if (size_changed && size_callback) {
        size_callback(page, size);
    }
    if (callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(page, size);
    }
}

void PaginationComponentHost::jump(runtime::ComponentId id, String text) {
    auto* s = services_->components().state<PaginationState>(id);
    if (!s || s->read_only || s->disabled || s->hidden) {
        return;
    }
    auto bytes = text.bytes();
    while (!bytes.empty() && bytes.front() == ' ') {
        bytes.remove_prefix(1);
    }
    while (!bytes.empty() && bytes.back() == ' ') {
        bytes.remove_suffix(1);
    }
    std::size_t value{};
    const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), value);
    if (!bytes.empty() && parsed.ec == std::errc{} && parsed.ptr == bytes.data() + bytes.size()) {
        const auto size = s->page_size;
        change(id, value, size);
    }
    s = services_->components().state<PaginationState>(id);
    if (s) {
        s->draft.set(s->simple ? String::from_utf8(std::to_string(s->current)).value() : String{});
    }
}

runtime::Size PaginationComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                               layout::Constraints limits) {
    auto& s = *services_->components().state<PaginationState>(id);
    const bool compact = s.responsive && !Breakpoints{}.matches(Breakpoint::Sm, s.window_width);
    if (compact != s.compact) {
        s.compact = compact;
        refresh(id, false);
    }
    if (s.hidden) {
        return {};
    }
    float width{};
    for (auto& [child, extent] : s.flow) {
        auto node = services_->components().root(child);
        const bool natural = child == s.simple_total;
        const auto measured = engine.measure_child(
            node, {natural ? 0 : extent, natural || extent == 0 ? limits.max_width : extent, s.extent, s.extent});
        if (extent == 0 || natural) {
            extent = measured.width;
        }
        if (natural) {
            s.simple_count_width = extent;
        }
        width += extent + s.gap;
    }
    return limits.constrain({std::max(0.0F, width - s.gap), s.extent});
}

void PaginationComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<PaginationState>(id);
    if (s.hidden) {
        return;
    }
    float width = -s.gap;
    for (const auto& [child, extent] : s.flow) {
        width += extent + s.gap;
    }
    const bool wrap_size =
        s.show_size_changer.value_or(s.total > 50) && width > bounds.width && bounds.height >= s.extent * 2 + s.gap;
    if (wrap_size) {
        for (const auto& [child, extent] : s.flow) {
            if (child == s.size_changer) {
                width -= extent + s.gap;
            }
        }
    }
    float x = bounds.x + std::max(0.0F, bounds.width - width) * (s.align == PaginationAlign::Center ? .5F
                                                                 : s.align == PaginationAlign::End  ? 1.0F
                                                                                                    : 0.0F);
    if (s.direction == TextDirection::RightToLeft) {
        x = bounds.x + bounds.width - (x - bounds.x);
    }
    for (const auto& [child, extent] : s.flow) {
        if (wrap_size && child == s.size_changer) {
            const float actual = std::min(extent, bounds.width);
            engine.place_child(services_->components().root(child),
                               {s.direction == TextDirection::RightToLeft ? bounds.x + bounds.width - actual : bounds.x,
                                bounds.y + s.extent + s.gap, actual, s.extent});
            continue;
        }
        engine.place_child(services_->components().root(child),
                           {s.direction == TextDirection::RightToLeft ? x - extent : x, bounds.y, extent, s.extent});
        x += (s.direction == TextDirection::RightToLeft ? -1 : 1) * (extent + s.gap);
    }
}

void PaginationComponentHost::mount(const PaginationProps& props) {
    if (props.page_size_ && props.default_page_size_) {
        throw std::invalid_argument("Pagination pageSize and defaultPageSize conflict");
    }
    valid_size(props.page_size_ ? read_prop(*props.page_size_) : props.default_page_size_.value_or(10));
    for (auto size : read_prop(props.page_size_options_)) {
        valid_size(size);
    }
    valid_control_size(read_prop(props.size_));
    const auto direction = read_prop(props.direction_);
    if (direction != TextDirection::Auto && direction != TextDirection::LeftToRight &&
        direction != TextDirection::RightToLeft) {
        throw std::invalid_argument("Pagination direction is invalid");
    }
    if (props.current_ && props.default_current_) {
        throw std::invalid_argument("Pagination current and defaultCurrent conflict");
    }
    if ((props.current_ && read_prop(*props.current_) == 0) || props.default_current_ == 0) {
        throw std::invalid_argument("Pagination current must be positive");
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<PaginationState>();
    auto& s = build.state<PaginationState>(id);
    s.node = build.root(id);
    s.controlled = props.current_.has_value();
    s.locale = read_prop(props.locale_);
    s.direction = read_prop(props.direction_);
    s.child_direction.set(s.direction);
    s.less = read_prop(props.less_);
    s.current = props.current_ ? read_prop(*props.current_) : props.default_current_.value_or(1);
    s.total = read_prop(props.total_);
    s.page_size = props.page_size_ ? read_prop(*props.page_size_) : props.default_page_size_.value_or(10);
    s.controlled_size = props.page_size_.has_value();
    s.simple = read_prop(props.simple_);
    s.read_only = read_prop(props.read_only_);
    s.quick = read_prop(props.quick_);
    s.responsive = read_prop(props.responsive_);
    s.align = read_prop(props.align_);
    s.size_options = read_prop(props.page_size_options_);
    s.show_size_changer = props.show_size_changer_ ? std::optional{read_prop(*props.show_size_changer_)} : std::nullopt;
    s.total_render = props.total_render_;
    s.on_size_change = props.on_size_change_;
    s.size = read_prop(props.size_);
    s.disabled = read_prop(props.disabled_);
    s.hide_single = read_prop(props.hide_single_);
    s.on_change = props.on_change_;
    auto initial_sizes = s.size_options;
    initial_sizes.push_back(s.page_size);
    std::ranges::sort(initial_sizes);
    initial_sizes.erase(std::unique(initial_sizes.begin(), initial_sizes.end()), initial_sizes.end());
    std::vector<SelectOption> initial_options;
    for (auto size : initial_sizes) {
        initial_options.push_back(
            {SelectValue{String::from_utf8(std::to_string(size)).value()},
             String::from_utf8(std::to_string(size) + std::string(s.locale.items_per_page.bytes())).value(), false});
    }
    s.options.set(std::move(initial_options));
    s.selected_size.set(SelectValue{String::from_utf8(std::to_string(s.page_size)).value()});
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            for (std::size_t index = 0; index < s.items.size(); ++index) {
                auto& item = s.items[index];
                item.component = context.mount_component<int>(0);
                const auto node = context.root(item.component);
                services.layout().set_layout(node, layout::BoxLayout{{}, true});
                context.on_resource_cleanup(item.component,
                                            [&services, node] { services.layout().remove_layout(node); });
                context.mount_slot(
                    item.component, Content{[&, index] {
                        Theme(ThemeProps{}.config(item.theme), ThemeContent{[&, index] {
                                  Button(ButtonProps{}
                                             .disabled(item.disabled)
                                             .size(s.control_size)
                                             .wave(false)
                                             .layout(LayoutStyle{}.width(item.extent).height(item.extent))
                                             .onClick([this, id, index] { request(id, index); }),
                                         ButtonContent{[render = props.item_render_, page = item.page, type = item.type,
                                                        label = item.label, active = item.active] {
                                             if (render) {
                                                 render({bind([page] { return page.get(); }),
                                                         bind([type] { return type.get(); }),
                                                         bind([label] { return label.get(); }),
                                                         bind([active] { return active.get(); })});
                                             } else {
                                                 Text(TextProps{}.content(label));
                                             }
                                         }},
                                         ButtonIcon{[icon = item.icon, visible = item.icon_visible,
                                                     custom = static_cast<bool>(props.item_render_)] {
                                             if (!custom) {
                                                 Icon(IconProps{}.name(icon).visible(visible));
                                             }
                                         }});
                              }});
                    }});
            }
            const auto wrap = [&](auto content) {
                const auto child = context.mount_component<int>(0);
                const auto node = context.root(child);
                services.layout().set_layout(node, layout::BoxLayout{{}, true});
                context.on_resource_cleanup(child, [&services, node] { services.layout().remove_layout(node); });
                context.mount_slot(child, Content{content});
                return child;
            };
            s.total_component = wrap([label = s.total_text] { Text(TextProps{}.content(label)); });
            services.layout().set_layout(services.components().root(s.total_component), layout::BoxLayout{});
            s.simple_total = wrap([label = s.simple_text] { Text(TextProps{}.content(label)); });
            services.layout().set_layout(services.components().root(s.simple_total), layout::BoxLayout{});
            s.jumper = wrap([this, id, &s, locale = props.locale_] {
                Input(InputProps{}
                          .direction(s.child_direction)
                          .value(s.draft)
                          .disabled(s.controls_disabled)
                          .readOnly(s.editor_read_only)
                          .size(s.control_size)
                          .placeholder(bind([locale] { return read_prop(locale).jump_placeholder; }))
                          .onChange([this, id](String value) {
                              if (auto* current = services_->components().state<PaginationState>(id)) {
                                  current->draft.set(std::move(value));
                              }
                          })
                          .onSubmit([this, id](String value) { jump(id, std::move(value)); })
                          .onBlur([this, id] {
                              if (auto* current = services_->components().state<PaginationState>(id)) {
                                  jump(id, current->draft.get());
                              }
                          }));
            });
            s.size_changer = wrap([this, id, &s] {
                Select(SelectProps{}
                           .direction(s.child_direction)
                           .options(s.options)
                           .value(s.selected_size)
                           .disabled(s.controls_disabled)
                           .size(s.control_size)
                           .onChange([this, id](SelectSelection value) {
                               if (!value) {
                                   return;
                               }
                               const auto* text = std::get_if<String>(&value->storage());
                               auto* current = services_->components().state<PaginationState>(id);
                               if (!text || !current) {
                                   return;
                               }
                               std::size_t size{};
                               auto bytes = text->bytes();
                               const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), size);
                               if (parsed.ec == std::errc{} && size > 0) {
                                   change(id, current->current, size);
                               }
                           }));
            });
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect b) { place(id, e, b); }});
    services.nodes().require(s.node).clip_content = true;
    mounted_.push_back(id);
    refresh(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, Prop<float>{services.responsive_layout().window_width()}, [this, id](float width) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->window_width = width;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.total_, [this, id](std::size_t value) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->total = value;
            refresh(id, true);
        }
    });
    if (props.page_size_) {
        connect_prop(scope, *props.page_size_, [this, id](std::size_t value) {
            valid_size(value);
            if (auto* s = services_->components().state<PaginationState>(id)) {
                s->page_size = value;
                refresh(id, true);
            }
        });
    }
    const auto boolean = [&](const Prop<bool>& prop, bool PaginationState::* field) {
        connect_prop(scope, prop, [this, id, field](bool value) {
            if (auto* s = services_->components().state<PaginationState>(id)) {
                s->*field = value;
                refresh(id, true);
            }
        });
    };
    boolean(props.simple_, &PaginationState::simple);
    boolean(props.quick_, &PaginationState::quick);
    boolean(props.read_only_, &PaginationState::read_only);
    boolean(props.responsive_, &PaginationState::responsive);
    connect_prop(scope, props.align_, [this, id](PaginationAlign value) {
        if (value != PaginationAlign::Start && value != PaginationAlign::Center && value != PaginationAlign::End) {
            throw std::invalid_argument("Pagination align is invalid");
        }
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->align = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.locale_, [this, id](PaginationLocale value) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->locale = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.direction_, [this, id](TextDirection value) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            if (value != TextDirection::Auto && value != TextDirection::LeftToRight &&
                value != TextDirection::RightToLeft) {
                throw std::invalid_argument("Pagination direction is invalid");
            }
            s->direction = value;
            s->child_direction.set(value);
            refresh(id, true);
        }
    });
    connect_prop(scope, props.less_, [this, id](bool value) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->less = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.page_size_options_, [this, id](std::vector<std::size_t> value) {
        for (auto size : value) {
            valid_size(size);
        }
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->size_options = std::move(value);
            refresh(id, true);
        }
    });
    if (props.show_size_changer_) {
        connect_prop(scope, *props.show_size_changer_, [this, id](bool value) {
            if (auto* s = services_->components().state<PaginationState>(id)) {
                s->show_size_changer = value;
                refresh(id, true);
            }
        });
    }
    if (props.current_) {
        connect_prop(scope, *props.current_, [this, id](std::size_t value) {
            if (value == 0) {
                throw std::invalid_argument("Pagination current must be positive");
            }
            if (auto* s = services_->components().state<PaginationState>(id)) {
                s->current = value;
                refresh(id, true);
            }
        });
    }
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->disabled = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.hide_single_, [this, id](bool value) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->hide_single = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        valid_control_size(value);
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->size = value;
            refresh(id, true);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::pagination_colors,
                               &theme_runtime::ThemeScope::pagination_metrics};
    for (std::size_t index = 0; index < accessors.size(); ++index) {
        s.subscriptions[index] =
            theme->capture([this, id, geometry = index == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[index]] { static_cast<void>((theme.get()->*accessor)()); });
    }
}
} // namespace ryn::detail

namespace ryn {
void Pagination(PaginationProps props) {
    if (!detail::active_pagination) {
        throw std::logic_error("Pagination requires window component services");
    }
    detail::active_pagination->mount(props);
}
} // namespace ryn
