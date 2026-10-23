#include "component/pagination_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local PaginationComponentHost* active_pagination{};

struct PageItem final {
    runtime::ComponentId component;
    Signal<String> label{String{}};
    Signal<bool> disabled{false};
    Signal<LogicalLength> extent{dp(32)};
    Signal<ThemeConfig> theme{ThemeConfig{}};
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
    bool controlled{};
    bool disabled{};
    bool hide_single{};
    bool hidden{};
    ControlSize size{ControlSize::Middle};
    float extent{32};
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
    PaginationSnapshot result{s->current, s->pages, s->page_size, {}, {}, s->hidden};
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
    s->extent = s->size == ControlSize::Small   ? token.metrics.item_size_small
                : s->size == ControlSize::Large ? token.metrics.item_size_large
                                                : token.metrics.item_size;
    s->gap = token.metrics.gap;
    std::vector<std::pair<std::size_t, bool>> targets;
    if (s->pages <= 9) {
        for (std::size_t page = 1; page <= s->pages; ++page) {
            targets.emplace_back(page, false);
        }
    } else {
        const auto start = s->current > 3 ? s->current - 2 : std::size_t{2};
        const auto end = s->current >= s->pages - 2 ? s->pages - 1 : s->current + 2;
        targets.emplace_back(1, false);
        if (start > 2) {
            targets.emplace_back(s->current > 5 ? s->current - 5 : 1, true);
        }
        for (auto page = start; page <= end; ++page) {
            targets.emplace_back(page, false);
        }
        if (end < s->pages - 1) {
            targets.emplace_back(s->current > s->pages - 5 ? s->pages : s->current + 5, true);
        }
        targets.emplace_back(s->pages, false);
    }
    for (std::size_t index = 0; index < s->items.size(); ++index) {
        auto& item = s->items[index];
        item.shown = index == 0 || index == 10 || index <= targets.size();
        const bool jump = index > 0 && index <= targets.size() && targets[index - 1].second;
        item.target = index == 0                ? (s->current > 1 ? s->current - 1 : 1)
                      : index == 10             ? (s->current < s->pages ? s->current + 1 : s->pages)
                      : index <= targets.size() ? targets[index - 1].first
                                                : 0;
        const bool active = index > 0 && index < 10 && !jump && item.target == s->current;
        item.label.set(index == 0    ? String{u8"‹"}
                       : index == 10 ? String{u8"›"}
                       : jump        ? String{u8"•••"}
                                     : String::from_utf8(std::to_string(item.target)).value());
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
        item.theme.set(std::move(config));
        if (services_->components().set_branch_active(item.component, item.shown && !s->hidden)) {
            services_->mark_scene_structure_dirty();
            geometry = true;
        }
    }
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
    const auto target = s->items[index].target;
    if (target == s->current || target == 0) {
        return;
    }
    const auto callback = s->on_change;
    const auto size = s->page_size;
    if (!s->controlled) {
        s->current = target;
        refresh(id, true);
    }
    if (callback) {
        callback(target, size);
    }
}

void PaginationComponentHost::mount(const PaginationProps& props) {
    valid_size(read_prop(props.page_size_));
    valid_control_size(read_prop(props.size_));
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
    s.current = props.current_ ? read_prop(*props.current_) : props.default_current_.value_or(1);
    s.total = read_prop(props.total_);
    s.page_size = read_prop(props.page_size_);
    s.size = read_prop(props.size_);
    s.disabled = read_prop(props.disabled_);
    s.hide_single = read_prop(props.hide_single_);
    s.on_change = props.on_change_;
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
                                             .size(props.size_)
                                             .wave(false)
                                             .layout(LayoutStyle{}.width(item.extent).height(item.extent))
                                             .onClick([this, id, index] { request(id, index); }),
                                         ButtonContent{[label = item.label] { Text(TextProps{}.content(label)); }});
                              }});
                    }});
            }
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *services_->components().state<PaginationState>(id);
                        if (s.hidden) {
                            return runtime::Size{};
                        }
                        std::size_t count = 0;
                        for (const auto& item : s.items) {
                            if (item.shown && !s.hidden) {
                                ++count;
                                static_cast<void>(engine.measure_child(services_->components().root(item.component),
                                                                       {s.extent, s.extent, s.extent, s.extent}));
                            }
                        }
                        return limits.constrain(
                            {count ? static_cast<float>(count) * s.extent + static_cast<float>(count - 1) * s.gap : 0,
                             count ? s.extent : 0});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        auto& s = *services_->components().state<PaginationState>(id);
                        if (s.hidden) {
                            return;
                        }
                        float x = bounds.x;
                        for (const auto& item : s.items) {
                            if (item.shown && !s.hidden) {
                                engine.place_child(services_->components().root(item.component),
                                                   {x, bounds.y, s.extent, s.extent});
                                x += s.extent + s.gap;
                            }
                        }
                    }});
    services.nodes().require(s.node).clip_content = true;
    mounted_.push_back(id);
    refresh(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, props.total_, [this, id](std::size_t value) {
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->total = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.page_size_, [this, id](std::size_t value) {
        valid_size(value);
        if (auto* s = services_->components().state<PaginationState>(id)) {
            s->page_size = value;
            refresh(id, true);
        }
    });
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
