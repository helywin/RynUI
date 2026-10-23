#include "component/empty_result_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local EmptyResultComponentHost* active_presentations{};

struct PresentationState final {
    runtime::NodeId node;
    EmptyResultSnapshot snapshot;
    component::RetainedSurfaceId background;
    bool custom_visual{};
    bool has_extra{};
    bool has_content{};
    std::array<bool, 5> shown{};
    std::array<runtime::Size, 5> sizes{};
    std::array<float, 5> gaps{};
    Signal<String> label{String{}};
    Signal<String> subtitle{String{}};
    Signal<IconName> icon{IconName::InboxOutlined};
    Signal<runtime::SemanticForeground> visual_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> label_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> subtitle_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> content_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> extra_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

auto regions(const PresentationState& s) {
    return std::array{s.snapshot.image, s.snapshot.label, s.snapshot.subtitle, s.snapshot.extra, s.snapshot.content};
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

void validate(EmptyImage image) {
    if (image != EmptyImage::Default && image != EmptyImage::Compact && image != EmptyImage::None) {
        throw std::invalid_argument("Empty image is invalid");
    }
}

void validate(const ResultStatus& value) {
    if (value.valueless_by_exception()) {
        throw std::invalid_argument("Result status is invalid");
    }
    if (const auto* status = std::get_if<Status>(&value)) {
        if (*status != Status::None && *status != Status::Default && *status != Status::Success &&
            *status != Status::Processing && *status != Status::Error && *status != Status::Warning) {
            throw std::invalid_argument("Result status is invalid");
        }
    } else {
        const auto exception = std::get<ResultException>(value);
        if (exception != ResultException::Forbidden && exception != ResultException::NotFound &&
            exception != ResultException::ServerError) {
            throw std::invalid_argument("Result exception is invalid");
        }
    }
}

std::pair<IconName, Color> result_visual(const ResultStatus& value, const ResultColorToken& colors) {
    if (const auto* status = std::get_if<Status>(&value)) {
        switch (*status) {
        case Status::Success:
            return {IconName::CheckCircleFilled, colors.success};
        case Status::Warning:
            return {IconName::WarningFilled, colors.warning};
        case Status::Error:
            return {IconName::CloseCircleFilled, colors.error};
        default:
            return {IconName::ExclamationCircleFilled, colors.info};
        }
    }
    switch (std::get<ResultException>(value)) {
    case ResultException::Forbidden:
        return {IconName::LockOutlined, colors.warning};
    case ResultException::NotFound:
        return {IconName::FileSearchOutlined, colors.info};
    case ResultException::ServerError:
        return {IconName::CloudServerOutlined, colors.error};
    }
    throw std::invalid_argument("Result exception is invalid");
}

template <class Slot> std::optional<Content> content(const std::optional<Slot>& slot) {
    if (!slot) {
        return {};
    }
    return Content{[slot = *slot] { SlotContentAccess::function(slot)(); }};
}

runtime::Size padding(const PresentationState& s, const ThemeSnapshot& theme) {
    if (s.snapshot.is_result) {
        return {theme.result().metrics.padding_inline, theme.result().metrics.padding_block};
    }
    const auto& m = theme.empty().metrics;
    return {m.padding_inline,
            s.snapshot.empty_image == EmptyImage::Compact ? m.compact_padding_block : m.padding_block};
}
} // namespace

EmptyResultComponentHost::EmptyResultComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

EmptyResultComponentHost::~EmptyResultComponentHost() {
    services_->detach(*this);
}

void* EmptyResultComponentHost::begin_mount() noexcept {
    auto* previous = active_presentations;
    active_presentations = this;
    return previous;
}

void EmptyResultComponentHost::end_mount(void* previous) noexcept {
    active_presentations = static_cast<EmptyResultComponentHost*>(previous);
}

void EmptyResultComponentHost::on_destroy() noexcept {
    const auto stale = [this](auto id) {
        return !services_->components().contains(id);
    };
    std::erase_if(empties_, stale);
    std::erase_if(results_, stale);
}

void EmptyResultComponentHost::on_dispose() noexcept {
    empties_.clear();
    results_.clear();
}

EmptyResultSnapshot EmptyResultComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<PresentationState>(id);
    if (!s) {
        throw std::out_of_range("Empty/Result component is stale");
    }
    return s->snapshot;
}

void EmptyResultComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<PresentationState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    float action_gap{};
    float decoration{};
    if (s->snapshot.is_result) {
        const auto& t = theme.result();
        const auto& m = t.metrics;
        const auto [icon, color] = result_visual(s->snapshot.status, t.colors);
        s->icon.set(icon);
        s->visual_color.set(foreground(color));
        s->label_color.set(foreground(t.colors.title));
        s->subtitle_color.set(foreground(t.colors.subtitle));
        s->content_color.set(foreground(t.colors.content));
        s->extra_color.set(foreground(t.colors.extra));
        s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
        s->title_font.set({m.font_family, 400, false, m.title_font_size, m.title_line_height});
        s->icon_font.set({m.font_family, 400, false, std::max(0.01F, m.icon_size), std::max(0.01F, m.icon_size)});
        s->gaps = {0, m.icon_gap, m.title_gap, m.extra_gap, m.content_gap};
        s->shown = {true, !s->label.get().empty(), !s->subtitle.get().empty(), s->has_extra, s->has_content};
        if (!s->shown[1]) {
            s->gaps[2] = m.icon_gap;
        }
        action_gap = m.action_gap;
        decoration = m.decoration_padding;
        if (geometry) {
            services_->layout().set_layout(services_->components().root(s->snapshot.content),
                                           layout::FlexLayout{layout::FlexDirection::vertical,
                                                              action_gap,
                                                              {m.content_padding_inline, m.content_padding_block,
                                                               m.content_padding_inline, m.content_padding_block},
                                                              true,
                                                              false,
                                                              layout::FlexWrap::no_wrap,
                                                              layout::FlexJustify::start,
                                                              layout::FlexAlign::start});
        }
    } else {
        const auto& t = theme.empty();
        const auto& m = t.metrics;
        const float size = s->snapshot.empty_image == EmptyImage::Compact ? m.compact_image_size : m.image_size;
        s->visual_color.set(foreground(t.colors.image));
        s->label_color.set(foreground(t.colors.description));
        s->extra_color.set(foreground(t.colors.extra));
        s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
        s->title_font.set({m.font_family, 400, false, m.font_size, m.line_height});
        s->icon_font.set({m.font_family, 400, false, std::max(0.01F, size), std::max(0.01F, size)});
        s->gaps = {0, m.image_gap, 0, m.extra_gap, 0};
        s->shown = {s->snapshot.empty_image != EmptyImage::None, !s->label.get().empty(), false, s->has_extra, false};
        action_gap = m.action_gap;
        decoration = m.decoration_padding;
    }
    const auto parts = regions(*s);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (services_->components().set_branch_active(parts[i], s->shown[i])) {
            services_->mark_scene_structure_dirty();
        }
    }
    if (geometry) {
        services_->layout().set_layout(services_->components().root(s->snapshot.extra),
                                       layout::FlexLayout{layout::FlexDirection::horizontal,
                                                          action_gap,
                                                          {decoration, decoration, decoration, decoration},
                                                          true,
                                                          false,
                                                          layout::FlexWrap::wrap,
                                                          layout::FlexJustify::center,
                                                          layout::FlexAlign::center,
                                                          action_gap});
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
}

runtime::ComponentId EmptyResultComponentHost::mount(bool result, String label, String subtitle, EmptyImage image,
                                                     ResultStatus status, const LayoutStyle& style,
                                                     std::optional<Content> visual, std::optional<Content> extra,
                                                     std::optional<Content> body) {
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<PresentationState>();
    auto& s = build.state<PresentationState>(id);
    s.node = build.root(id);
    s.snapshot.is_result = result;
    s.snapshot.empty_image = image;
    s.snapshot.status = status;
    s.label.set(std::move(label));
    s.subtitle.set(std::move(subtitle));
    s.custom_visual = visual.has_value();
    s.has_extra = extra.has_value();
    s.has_content = body.has_value();
    runtime::connect_layout_style(build.scope(id), style, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    if (result) {
        const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
        s.background = services.surfaces().create_content_range(fragment, {});
        build.on_resource_cleanup(
            id, [&services, surface = s.background] { services.surfaces().destroy_content_range(surface); });
    }
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](Content contents, const Signal<runtime::SemanticForeground>& color,
                                    const Signal<runtime::SemanticTypography>& font) {
                const auto part = context.mount_component<int>(0);
                const auto node = context.root(part);
                services.nodes().require(node).clip_content = true;
                services.layout().set_layout(
                    node,
                    layout::ComponentLayout{
                        [&services](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                            runtime::Size size;
                            for (auto child : services.nodes().require(node).children) {
                                const auto measured =
                                    engine.measure_child(child, {0, limits.max_width, 0, limits.max_height});
                                size.width = std::max(size.width, measured.width);
                                size.height = std::max(size.height, measured.height);
                            }
                            return limits.constrain(size);
                        },
                        [&services](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect bounds) {
                            for (auto child : services.nodes().require(node).children) {
                                const auto size = services.nodes().require(child).measured_size;
                                const float width = std::min(size.width, bounds.width);
                                const float height = std::min(size.height, bounds.height);
                                engine.place_child(child, {bounds.x + (bounds.width - width) / 2,
                                                           bounds.y + (bounds.height - height) / 2, width, height});
                            }
                        }});
                context.on_resource_cleanup(part, [&services, node] { services.layout().remove_layout(node); });
                context.mount_slot_with_semantic_text_style(part, contents, Prop<runtime::SemanticForeground>{color},
                                                            Prop<runtime::SemanticTypography>{font});
                return part;
            };
            s.snapshot.image = region(Content{[&s, visual] {
                                          if (visual) {
                                              SlotContentAccess::function (*visual)();
                                          } else {
                                              Icon(IconProps{}.name(s.icon));
                                          }
                                      }},
                                      s.visual_color, s.icon_font);
            s.snapshot.label =
                region(Content{[label = s.label] { Text(TextProps{}.content(label)); }}, s.label_color, s.title_font);
            s.snapshot.subtitle =
                region(Content{[label = s.subtitle] { Text(TextProps{}.content(label)); }}, s.subtitle_color, s.font);
            s.snapshot.extra = region(Content{[extra] {
                                          if (extra) {
                                              SlotContentAccess::function (*extra)();
                                          }
                                      }},
                                      s.extra_color, s.font);
            s.snapshot.content = region(Content{[body] {
                                            if (body) {
                                                SlotContentAccess::function (*body)();
                                            }
                                        }},
                                        s.content_color, s.font);
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<PresentationState>(id);
                const auto pad = padding(s, services_->components().theme_scope(id)->snapshot());
                const float px = std::min(pad.width, limits.max_width / 2);
                const float py = std::min(pad.height, limits.max_height / 2);
                const float width = std::max(0.0F, limits.max_width - px * 2);
                const float available = std::max(0.0F, limits.max_height - py * 2);
                float height = 0;
                float measured_width = 0;
                bool previous = false;
                const auto parts = regions(s);
                for (std::size_t i = 0; i < parts.size(); ++i) {
                    s.sizes[i] = {};
                    if (!s.shown[i]) {
                        continue;
                    }
                    const float gap = previous ? std::min(s.gaps[i], std::max(0.0F, available - height)) : 0;
                    height += gap;
                    s.sizes[i] = engine.measure_child(services_->components().root(parts[i]),
                                                      {0, width, 0, std::max(0.0F, available - height)});
                    height += s.sizes[i].height;
                    measured_width = std::max(measured_width, s.sizes[i].width);
                    previous = true;
                }
                return limits.constrain(
                    {std::isfinite(limits.max_width) ? limits.max_width : measured_width + px * 2, height + py * 2});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<PresentationState>(id);
                const auto pad = padding(s, services_->components().theme_scope(id)->snapshot());
                const float px = std::min(pad.width, bounds.width / 2);
                const float py = std::min(pad.height, bounds.height / 2);
                const float width = std::max(0.0F, bounds.width - px * 2);
                const float bottom = bounds.y + bounds.height - py;
                float y = bounds.y + py;
                bool previous = false;
                const auto parts = regions(s);
                for (std::size_t i = 0; i < parts.size(); ++i) {
                    if (!s.shown[i]) {
                        continue;
                    }
                    y += previous ? std::min(s.gaps[i], std::max(0.0F, bottom - y)) : 0;
                    const float height = std::min(s.sizes[i].height, std::max(0.0F, bottom - y));
                    const float region_width = i >= 3 ? width : std::min(width, s.sizes[i].width);
                    engine.place_child(services_->components().root(parts[i]),
                                       {bounds.x + px + (width - region_width) / 2, y, region_width, height});
                    y += height;
                    previous = true;
                }
            }});
    (result ? results_ : empties_).push_back(id);
    refresh(id, true);
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](theme_runtime::DirtyPhase) { refresh(id, false); },
                                        [theme, result] {
                                            if (result) {
                                                (void)theme->result_colors();
                                            } else {
                                                (void)theme->empty_colors();
                                            }
                                        });
    s.subscriptions[1] = theme->capture([this, id](theme_runtime::DirtyPhase) { refresh(id, true); },
                                        [theme, result] {
                                            if (result) {
                                                (void)theme->result_metrics();
                                            } else {
                                                (void)theme->empty_metrics();
                                            }
                                        });
    return id;
}

void EmptyResultComponentHost::mount(const EmptyProps& props, const EmptySlots& slots) {
    validate(read_prop(props.image_));
    const auto id = mount(false, read_prop(props.description_), {}, read_prop(props.image_), Status::Default,
                          props.layout_, content(slots.image), content(slots.extra), {});
    auto& scope = services_->components().scope(id);
    connect_prop(scope, props.description_, [this, id](String label) {
        if (auto* s = services_->components().state<PresentationState>(id)) {
            s->label.set(std::move(label));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.image_, [this, id](EmptyImage image) {
        validate(image);
        if (auto* s = services_->components().state<PresentationState>(id)) {
            s->snapshot.empty_image = image;
            refresh(id, true);
        }
    });
}

void EmptyResultComponentHost::mount(const ResultProps& props, const ResultSlots& slots) {
    validate(read_prop(props.status_));
    const auto id =
        mount(true, read_prop(props.title_), read_prop(props.subtitle_), EmptyImage::Default, read_prop(props.status_),
              props.layout_, content(slots.icon), content(slots.extra), content(slots.content));
    auto& scope = services_->components().scope(id);
    connect_prop(scope, props.title_, [this, id](String label) {
        if (auto* s = services_->components().state<PresentationState>(id)) {
            s->label.set(std::move(label));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.subtitle_, [this, id](String label) {
        if (auto* s = services_->components().state<PresentationState>(id)) {
            s->subtitle.set(std::move(label));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.status_, [this, id](ResultStatus status) {
        validate(status);
        if (auto* s = services_->components().state<PresentationState>(id)) {
            s->snapshot.status = status;
            refresh(id, false);
        }
    });
}

void EmptyResultComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (const auto* list : {&empties_, &results_}) {
        for (auto id : *list) {
            auto* s = services_->components().state<PresentationState>(id);
            if (!s || !services_->components().branch_active(id)) {
                continue;
            }
            const auto parts = regions(*s);
            std::array<runtime::Rect*, 5> rects{&s->snapshot.image_bounds, &s->snapshot.label_bounds,
                                                &s->snapshot.subtitle_bounds, &s->snapshot.extra_bounds,
                                                &s->snapshot.content_bounds};
            for (std::size_t i = 0; i < parts.size(); ++i) {
                *rects[i] = s->shown[i] ? services_->nodes().require(services_->components().root(parts[i])).bounds
                                        : runtime::Rect{};
            }
            if (s->snapshot.is_result) {
                const auto& node = services_->nodes().require(s->node);
                auto effect = graphics::make_shadow_effect(
                    {s->snapshot.content_bounds, 0},
                    ShadowLayer{ShadowKind::outer,
                                {},
                                0,
                                0,
                                services_->components().theme_scope(id)->snapshot().result().colors.content_background},
                    node.translation,
                    graphics::EffectClip{
                        1, services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, clip))});
                effect.material.opacity = s->has_content ? node.opacity : 0;
                (void)services_->surfaces().update_content_effects(s->background, std::span{&effect, 1});
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Empty(EmptyProps props, EmptySlots slots) {
    if (!detail::active_presentations) {
        throw std::logic_error("Empty requires window component services");
    }
    detail::active_presentations->mount(props, slots);
}

void Result(ResultProps props, ResultSlots slots) {
    if (!detail::active_presentations) {
        throw std::logic_error("Result requires window component services");
    }
    detail::active_presentations->mount(props, slots);
}
} // namespace ryn
