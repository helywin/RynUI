#include "component/empty_result_component.hpp"
#include "component/localized_prop.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "component/retained_slot.hpp"
#include <ryn/icon.hpp>
#include <ryn/image.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local EmptyResultComponentHost* active_presentations{};
#include "icons/empty_illustration.inc"

#include "icons/result_illustrations.inc"

ImageSource exception_image(ResultException exception) {
    const auto decode = [](auto& bytes) {
        const auto result = decode_image(*EncodedImage::copy(std::as_bytes(std::span{bytes})));
        if (!result) {
            throw std::runtime_error("Bundled Result illustration cannot be decoded");
        }
        return ImageSource{result.pixels};
    };
    static const auto forbidden = decode(result_png_403);
    static const auto missing = decode(result_png_404);
    static const auto server = decode(result_png_500);
    return exception == ResultException::Forbidden  ? forbidden
           : exception == ResultException::NotFound ? missing
                                                    : server;
}

struct PresentationState final {
    runtime::NodeId node;
    EmptyResultSnapshot snapshot;
    component::RetainedSurfaceId background;
    bool custom_visual{};
    bool has_extra{};
    bool has_content{};
    bool custom_description{};
    bool built_in_visual{};
    std::optional<EmptyDynamicContent> dynamic_pending;
    EmptyDynamicContent dynamic_current;
    bool dynamic_updating{};
    std::uint64_t dynamic_revision{};
    std::uint64_t dynamic_committed{};
    std::optional<Content> default_visual;
    bool rich_title{};
    bool rich_subtitle{};
    bool reconciling{};
    bool animated{true};
    bool in_view{};
    runtime::ComponentId default_icon;
    runtime::ComponentId illustration;
    runtime::ComponentId custom_icon;
    std::array<std::shared_ptr<const void>, 5> renderers;
    std::array<bool, 5> connected{};
    std::array<std::optional<Content>, 5> desired;
    std::array<bool, 5> pending{};
    std::optional<animation::AnimationTime> motion_start;
    animation::AnimationTime observed;
    Signal<ImageSource> image_source{ImageSource{}};
    Signal<float> visual_width{1};
    Signal<float> visual_height{1};
    std::array<bool, 5> shown{};
    std::array<runtime::Size, 5> sizes{};
    std::array<float, 5> gaps{};
    Signal<String> label{String{}};
    std::uint64_t label_revision{};
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
    Signal<float> visual_size{1};
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

auto regions(const PresentationState& s) {
    return std::array{s.snapshot.image, s.snapshot.label, s.snapshot.subtitle, s.snapshot.extra, s.snapshot.content};
}

bool result_motion(WindowComponentServices& services, runtime::ComponentId id) {
    return services.focus().state().window_active && services.components().branch_active(id) &&
           animation::resolve_motion_policy(services.components().theme_scope(id)->snapshot(),
                                            services.motion_preference())
               .enabled();
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
        s->visual_size.set(m.icon_size);
        s->snapshot.illustration = std::holds_alternative<ResultException>(s->snapshot.status);
        s->visual_width.set(m.icon_size / 72 * (s->snapshot.illustration ? 250 : 72));
        s->visual_height.set(m.icon_size / 72 * (s->snapshot.illustration ? 294 : 72));
        if (s->snapshot.illustration) {
            s->image_source.set(exception_image(std::get<ResultException>(s->snapshot.status)));
        }
        for (const auto [part, shown] : {std::pair{s->default_icon, !s->snapshot.illustration && !s->custom_visual},
                                         std::pair{s->illustration, s->snapshot.illustration},
                                         std::pair{s->custom_icon, !s->snapshot.illustration && s->custom_visual}}) {
            if (part.valid() && services_->components().set_branch_active(part, shown)) {
                services_->mark_scene_structure_dirty();
            }
        }
        // Icon fonts require a positive integral logical pixel size. Keep a valid
        // retained font even while the built-in visual is hidden or subpixel.
        const float font_size = std::max(1.0F, m.icon_size);
        s->icon_font.set({m.font_family, 400, false, font_size, font_size});
        s->gaps = {0, m.icon_gap, m.title_gap, m.extra_gap, m.content_gap};
        s->shown = {s->custom_visual || m.icon_size > 0, s->rich_title || !s->label.get().empty(),
                    s->rich_subtitle || !s->subtitle.get().empty(), s->has_extra, s->has_content};
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
        s->visual_size.set(size);
        const float font_size = std::max(1.0F, size);
        s->icon_font.set({m.font_family, 400, false, font_size, font_size});
        s->gaps = {0, m.image_gap, 0, m.extra_gap, 0};
        s->shown = {s->snapshot.empty_image != EmptyImage::None && (s->custom_visual || size > 0),
                    s->custom_description || !s->label.get().empty(), false, s->has_extra, false};
        action_gap = m.action_gap;
        decoration = m.decoration_padding;
    }
    const auto parts = regions(*s);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (services_->components().set_branch_active(parts[i], s->shown[i])) {
            services_->mark_scene_structure_dirty();
        }
    }
    if (s->snapshot.is_result) {
        const auto node = services_->components().root(s->snapshot.image);
        if (services_->nodes().set_paint_opacity(node, s->snapshot.visual_motion)) {
            services_->dirty().invalidate(node, runtime::DirtyFlags::Material);
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
                                                     std::optional<Content> body, std::optional<Content> description) {
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<PresentationState>();
    auto& s = build.state<PresentationState>(id);
    try {
        s.node = build.root(id);
        s.snapshot.is_result = result;
        s.snapshot.empty_image = image;
        s.snapshot.status = status;
        s.label.set(std::move(label));
        s.subtitle.set(std::move(subtitle));
        s.custom_visual = visual.has_value();
        s.has_extra = extra.has_value();
        s.has_content = body.has_value();
        s.custom_description = description.has_value();
        s.default_visual = visual;
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
                            [&services](layout::LayoutEngine& engine, runtime::NodeId node,
                                        layout::Constraints limits) {
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
                                    const auto size = services.nodes().require(child).layout_size;
                                    const float width = std::min(size.width, bounds.width);
                                    const float height = std::min(size.height, bounds.height);
                                    engine.place_child(child, {bounds.x + (bounds.width - width) / 2,
                                                               bounds.y + (bounds.height - height) / 2, width, height});
                                }
                            }});
                    context.on_resource_cleanup(part, [&services, node] { services.layout().remove_layout(node); });
                    context.mount_slot_with_semantic_text_style(part, contents,
                                                                Prop<runtime::SemanticForeground>{color},
                                                                Prop<runtime::SemanticTypography>{font});
                    return part;
                };
                const auto image_part = region(
                    Content{[&s, visual, result, &services] {
                        if (result) {
                            auto& b = runtime::require_component_build_context();
                            const auto child = [&](Content body) {
                                const auto part = b.mount_component<int>(0);
                                const auto node = b.root(part);
                                services.layout().set_layout(node, layout::BoxLayout{});
                                b.on_resource_cleanup(part,
                                                      [&services, node] { services.layout().remove_layout(node); });
                                b.mount_slot(part, body);
                                return part;
                            };
                            s.default_icon = child(Content{[&s] {
                                const auto size = bind([v = s.visual_size] { return dp(v.get()); });
                                Icon(IconProps{}.name(s.icon).layout(LayoutStyle{}.width(size).height(size)));
                            }});
                            s.illustration = child(Content{[&s] {
                                Image(ImageProps{}
                                          .source(s.image_source)
                                          .preview(false)
                                          .fit(ImageFit::Contain)
                                          .layout(LayoutStyle{}
                                                      .width(bind([v = s.visual_width] { return dp(v.get()); }))
                                                      .height(bind([v = s.visual_height] { return dp(v.get()); }))));
                            }});
                            // Install the container identity before the application slot can reenter or retire it.
                            s.custom_icon = b.mount_component<int>(0);
                            const auto node = b.root(s.custom_icon);
                            services.layout().set_layout(node, layout::BoxLayout{});
                            b.on_resource_cleanup(s.custom_icon,
                                                  [&services, node] { services.layout().remove_layout(node); });
                            if (visual) {
                                b.mount_slot(s.custom_icon, *visual);
                            }
                        } else if (visual) {
                            SlotContentAccess::function (*visual)();
                        } else {
                            const auto size = bind([size = s.visual_size] { return dp(size.get()); });
                            Icon(IconProps{}.name(s.icon).layout(LayoutStyle{}.width(size).height(size)));
                        }
                    }},
                    s.visual_color, s.icon_font);
                if (!services.components().contains(id) || !services.components().contains(image_part)) {
                    return;
                }
                s.snapshot.image = image_part;
                const auto label_part = region(Content{[label = s.label, description] {
                                                   if (description) {
                                                       SlotContentAccess::function (*description)();
                                                   } else {
                                                       Text(TextProps{}.content(label));
                                                   }
                                               }},
                                               s.label_color, s.title_font);
                if (!services.components().contains(id) || !services.components().contains(label_part)) {
                    return;
                }
                s.snapshot.label = label_part;
                const auto subtitle_part = region(Content{[label = s.subtitle] { Text(TextProps{}.content(label)); }},
                                                  s.subtitle_color, s.font);
                if (!services.components().contains(id) || !services.components().contains(subtitle_part)) {
                    return;
                }
                s.snapshot.subtitle = subtitle_part;
                const auto extra_part = region(Content{[extra] {
                                                   if (extra) {
                                                       SlotContentAccess::function (*extra)();
                                                   }
                                               }},
                                               s.extra_color, s.font);
                if (!services.components().contains(id) || !services.components().contains(extra_part)) {
                    return;
                }
                s.snapshot.extra = extra_part;
                const auto content_part = region(Content{[body] {
                                                     if (body) {
                                                         SlotContentAccess::function (*body)();
                                                     }
                                                 }},
                                                 s.content_color, s.font);
                if (!services.components().contains(id) || !services.components().contains(content_part)) {
                    return;
                }
                s.snapshot.content = content_part;
            }});
        if (!services.components().contains(id)) {
            return id;
        }
        const auto parts = regions(s);
        if (std::ranges::any_of(parts, [&services](auto part) { return !services.components().contains(part); }) ||
            (result &&
             (!services.components().contains(s.default_icon) || !services.components().contains(s.illustration) ||
              !services.components().contains(s.custom_icon)))) {
            services.retire_failed_component(id);
            return id;
        }
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
                        {std::isfinite(limits.max_width) ? limits.max_width : measured_width + px * 2,
                         height + py * 2});
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
    } catch (...) {
        services.retire_failed_component(id);
        throw;
    }
}

void EmptyResultComponentHost::mount(const EmptyProps& props, const EmptySlots& slots) {
    validate(read_prop(props.image_));
    const auto locale = localized_prop(props.locale_, runtime::require_component_build_context().config_scope(),
                                       [](const LocaleData& data) { return data.common; });
    const auto label = props.description_ ? *props.description_ : bind([locale] { return read_prop(locale).empty; });
    auto visual = content(slots.image);
    const bool built_in_illustration = !visual && !props.source_ && props.illustration_.has_value();
    if (built_in_illustration) {
        visual = Content{[enabled = *props.illustration_, mode = props.image_] {
            const auto theme = runtime::require_component_build_context().theme_scope();
            static const auto sources = empty_illustration_sources();
            const auto size = bind([theme, mode] {
                const auto& m = theme->empty_metrics().metrics;
                return dp(read_prop(mode) == EmptyImage::Compact ? m.compact_image_size : m.image_size);
            });
            for (std::size_t i = 0; i < sources.size(); ++i) {
                const auto color = bind([theme, i] {
                    const auto base = theme->empty_colors().colors.image;
                    const auto bg = theme->alias().color_background_container;
                    const auto text = theme->map().color_text_base;
                    const bool dark = text.red() + text.green() + text.blue() > 1.5F;
                    const std::array opacity =
                        dark ? std::array{0.48F, 1.0F, 0.32F, 0.72F, 0.0F} : std::array{0.24F, 1.0F, 0.16F, 0.6F, 0.0F};
                    const float alpha = base.alpha() * opacity[i];
                    return IconTwoToneColor{Color(base.red() * alpha + bg.red() * (1 - alpha),
                                                  base.green() * alpha + bg.green() * (1 - alpha),
                                                  base.blue() * alpha + bg.blue() * (1 - alpha), 1),
                                            {}};
                });
                Icon(IconProps{}
                         .source(sources[i])
                         .twoToneColor(color)
                         .visible(enabled)
                         .layout(LayoutStyle{}.width(size).height(size)));
            }
            Icon(IconProps{}
                     .name(IconName::InboxOutlined)
                     .visible(bind([enabled] { return !read_prop(enabled); }))
                     .layout(LayoutStyle{}.width(size).height(size)));
        }};
    }
    if (!visual && props.source_) {
        visual = Content{[source = *props.source_, label] {
            const auto theme = runtime::require_component_build_context().theme_scope();
            Image(ImageProps{}.source(source).alt(label).preview(false).layout(
                LayoutStyle{}
                    .width(bind([theme] { return dp(theme->empty_metrics().metrics.image_size); }))
                    .height(bind([theme] { return dp(theme->empty_metrics().metrics.image_size); }))));
        }};
    }
    const auto fallback_visual = visual;
    auto extra = content(slots.extra);
    auto description = content(slots.description);
    const auto initial_dynamic = props.dynamic_ ? std::optional{read_prop(*props.dynamic_)} : std::nullopt;
    if (initial_dynamic) {
        if (initial_dynamic->image) {
            visual = Content{[slot = initial_dynamic->image] { SlotContentAccess::function (*slot)(); }};
        }
        extra = initial_dynamic->extra
                    ? std::optional{Content{[slot = initial_dynamic->extra] { SlotContentAccess::function (*slot)(); }}}
                    : std::nullopt;
        description = initial_dynamic->description ? std::optional{Content{[slot = initial_dynamic->description] {
            SlotContentAccess::function (*slot)();
        }}}
                                                   : std::nullopt;
    }
    const auto id = mount(false, read_prop(label), {}, read_prop(props.image_), Status::Default, props.layout_, visual,
                          extra, {}, description);
    if (auto* state = services_->components().state<PresentationState>(id)) {
        state->built_in_visual = built_in_illustration;
        if (built_in_illustration && (!initial_dynamic || !initial_dynamic->image)) {
            state->custom_visual = false;
        }
        if (initial_dynamic) {
            state->default_visual = fallback_visual;
            state->dynamic_current = *initial_dynamic;
        }
    }
    if (!services_->components().contains(id)) {
        return;
    }
    auto& scope = services_->components().scope(id);
    if (!slots.description || props.dynamic_) {
        connect_prop(scope, label, [this, id](String label) {
            if (auto* s = services_->components().state<PresentationState>(id)) {
                const auto revision = ++s->label_revision;
                const auto target = s->label;
                target.set(std::move(label));
                s = services_->components().state<PresentationState>(id);
                if (!s || s->label_revision != revision) {
                    return;
                }
                refresh(id, true);
            }
        });
    }
    connect_prop(scope, props.image_, [this, id](EmptyImage image) {
        validate(image);
        if (auto* s = services_->components().state<PresentationState>(id)) {
            s->snapshot.empty_image = image;
            refresh(id, true);
        }
    });
    if (props.dynamic_) {
        connect_prop(scope, *props.dynamic_, [this, id](EmptyDynamicContent value) {
            if (auto* s = services_->components().state<PresentationState>(id)) {
                s->dynamic_pending = std::move(value);
                ++s->dynamic_revision;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                           runtime::DirtyFlags::Geometry |
                                                           runtime::DirtyFlags::HitTest);
            }
        });
        auto* state = services_->components().state<PresentationState>(id);
        if (state && state->dynamic_pending && *state->dynamic_pending == state->dynamic_current) {
            state->dynamic_committed = state->dynamic_revision;
        }
    }
}

void EmptyResultComponentHost::prepare_auxiliary_layout() {
    const auto ids = empties_;
    auto& services = *services_;
    for (auto id : ids) {
        auto* s = services.components().state<PresentationState>(id);
        if (!s || !s->dynamic_pending || s->dynamic_updating || s->dynamic_revision == s->dynamic_committed) {
            continue;
        }
        s->dynamic_updating = true;
        const auto next = *s->dynamic_pending;
        const auto previous = s->dynamic_current;
        const auto revision = s->dynamic_revision;
        const std::array parts{s->snapshot.image, s->snapshot.label, s->snapshot.extra};
        const auto label = s->label;
        const auto icon = s->icon;
        const auto size = s->visual_size;
        const auto fallback = s->default_visual;
        try {
            for (std::size_t i = 0; i < parts.size(); ++i) {
                const bool changed = i == 0   ? next.image != previous.image
                                     : i == 1 ? next.description != previous.description
                                              : next.extra != previous.extra;
                if (!changed) {
                    continue;
                }
                const auto children = services.components().children(parts[i]);
                for (auto child : children) {
                    services.destroy(child);
                    if (!services.components().contains(id)) {
                        break;
                    }
                }
                if (!services.components().contains(id)) {
                    break;
                }
                s = services.components().state<PresentationState>(id);
                const auto color = i == 0 ? s->visual_color : i == 1 ? s->label_color : s->extra_color;
                const auto font = i == 0 ? s->icon_font : i == 1 ? s->title_font : s->font;
                services.append_slot(parts[i], Content{[&, i] {
                                         auto& build = runtime::require_component_build_context();
                                         build.mount_slot_with_semantic_text_style(
                                             parts[i], Content{[=] {
                                                 if (i == 0) {
                                                     if (next.image) {
                                                         SlotContentAccess::function (*next.image)();
                                                     } else if (fallback) {
                                                         SlotContentAccess::function (*fallback)();
                                                     } else {
                                                         const auto dimension = bind([size] { return dp(size.get()); });
                                                         Icon(IconProps{}.name(icon).layout(
                                                             LayoutStyle{}.width(dimension).height(dimension)));
                                                     }
                                                 } else if (i == 1) {
                                                     if (next.description) {
                                                         SlotContentAccess::function (*next.description)();
                                                     } else {
                                                         Text(TextProps{}.content(label));
                                                     }
                                                 } else if (next.extra) {
                                                     SlotContentAccess::function (*next.extra)();
                                                 }
                                             }},
                                             Prop<runtime::SemanticForeground>{color},
                                             Prop<runtime::SemanticTypography>{font});
                                     }});
                if (!services.components().contains(id)) {
                    break;
                }
            }
            s = services.components().state<PresentationState>(id);
            if (!s) {
                continue;
            }
            s->custom_visual = bool(next.image) || (s->default_visual.has_value() && !s->built_in_visual);
            s->custom_description = bool(next.description);
            s->has_extra = bool(next.extra);
            s->dynamic_current = next;
            s->dynamic_committed = revision;
            s->dynamic_updating = false;
            refresh(id, true);
        } catch (...) {
            services.retire_failed_component(id);
            throw;
        }
    }
}

void EmptyResultComponentHost::mount(const ResultProps& props, const ResultSlots& slots) {
    validate(read_prop(props.status_));
    const auto id =
        mount(true, read_prop(props.title_), read_prop(props.subtitle_), EmptyImage::Default, read_prop(props.status_),
              props.layout_, content(slots.icon), content(slots.extra), content(slots.content));
    if (!services_->components().contains(id)) {
        return;
    }
    try {
        if (slots.title) {
            replace_result_region(id, 1, content(slots.title));
        }
        if (!services_->components().contains(id)) {
            return;
        }
        if (slots.subtitle) {
            replace_result_region(id, 2, content(slots.subtitle));
        }
        if (!services_->components().contains(id)) {
            return;
        }
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
                if (s->snapshot.status == status) {
                    return;
                }
                s->snapshot.status = status;
                if (s->animated && s->in_view && result_motion(*services_, id)) {
                    s->motion_start = std::max(s->observed, services_->animation_time());
                    s->snapshot.visual_motion = 0;
                }
                refresh(id, true);
            }
        });
        if (!services_->components().contains(id)) {
            return;
        }
        connect_prop(services_->components().scope(id), props.animated_, [this, id](bool value) {
            if (auto* s = services_->components().state<PresentationState>(id)) {
                s->animated = value;
            }
            synchronize_auxiliary_motion();
        });
        connect_result_slot(id, 0, props.icon_);
        connect_result_slot(id, 1, props.title_slot_);
        connect_result_slot(id, 2, props.subtitle_slot_);
        connect_result_slot(id, 3, props.extra_);
        connect_result_slot(id, 4, props.content_);
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

template <typename Slot>
void EmptyResultComponentHost::connect_result_slot(runtime::ComponentId id, std::size_t index,
                                                   const std::optional<Prop<std::shared_ptr<const Slot>>>& prop) {
    if (!prop || !services_->components().contains(id)) {
        return;
    }
    connect_prop(services_->components().scope(id), *prop, [this, id, index](std::shared_ptr<const Slot> slot) {
        auto* s = services_->components().state<PresentationState>(id);
        if (!s || (s->connected[index] && s->renderers[index] == slot)) {
            return;
        }
        s->connected[index] = true;
        s->renderers[index] = slot;
        std::optional<Content> body;
        if (slot) {
            body = Content{[slot] { SlotContentAccess::function (*slot)(); }};
        }
        replace_result_region(id, index, std::move(body));
    });
}

void EmptyResultComponentHost::replace_result_region(runtime::ComponentId id, std::size_t index,
                                                     std::optional<Content> body) {
    auto* s = services_->components().state<PresentationState>(id);
    if (!s) {
        return;
    }
    s->desired[index] = std::move(body);
    s->pending[index] = true;
    if (s->reconciling) {
        return;
    }
    s->reconciling = true;
    try {
        while (true) {
            s = services_->components().state<PresentationState>(id);
            if (!s) {
                return;
            }
            const auto next = std::find(s->pending.begin(), s->pending.end(), true);
            if (next == s->pending.end()) {
                s->reconciling = false;
                break;
            }
            const auto i = static_cast<std::size_t>(next - s->pending.begin());
            s->pending[i] = false;
            auto desired = s->desired[i];
            const auto region = i == 0 ? s->custom_icon : regions(*s)[i];
            const auto children = services_->components().children(region);
            const std::vector<runtime::ComponentId> old(children.begin(), children.end());
            for (auto child : old) {
                if (services_->components().contains(child)) {
                    services_->destroy(child);
                }
                if (!services_->components().contains(id) || !services_->components().contains(region)) {
                    services_->retire_failed_component(id);
                    return;
                }
            }
            s = services_->components().state<PresentationState>(id);
            if (i == 0) {
                s->custom_visual = desired.has_value();
            }
            if (i == 1) {
                s->rich_title = desired.has_value();
            }
            if (i == 2) {
                s->rich_subtitle = desired.has_value();
            }
            if (i == 3) {
                s->has_extra = desired.has_value();
            }
            if (i == 4) {
                s->has_content = desired.has_value();
            }
            if (!desired && (i == 1 || i == 2)) {
                const auto text = i == 1 ? s->label : s->subtitle;
                desired = Content{[text] { Text(TextProps{}.content(text)); }};
            }
            const auto color = i == 0   ? s->visual_color
                               : i == 1 ? s->label_color
                               : i == 2 ? s->subtitle_color
                               : i == 3 ? s->extra_color
                                        : s->content_color;
            const auto font = i == 0 ? s->icon_font : i == 1 ? s->title_font : s->font;
            if (desired) {
                append_retained_slot(*services_, region, Content{[region, color, font, desired] {
                    runtime::require_component_build_context().mount_slot_with_semantic_text_style(
                        region, *desired, Prop<runtime::SemanticForeground>{color},
                        Prop<runtime::SemanticTypography>{font});
                }});
            }
            if (!services_->components().contains(id) || !services_->components().contains(region)) {
                services_->retire_failed_component(id);
                return;
            }
            refresh(id, true);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void EmptyResultComponentHost::on_window_active(bool) {
    synchronize_auxiliary_motion();
}

void EmptyResultComponentHost::synchronize_auxiliary_motion() {
    for (auto id : results_) {
        auto* s = services_->components().state<PresentationState>(id);
        if (!s) {
            continue;
        }
        if (!s->animated || !s->in_view || !result_motion(*services_, id) ||
            services_->nodes().content_opacity(s->node) <= 0) {
            s->motion_start.reset();
            s->snapshot.visual_motion = 1;
        }
        if (s->snapshot.image.valid()) {
            const auto node = services_->components().root(s->snapshot.image);
            if (services_->nodes().set_paint_opacity(node, s->snapshot.visual_motion)) {
                services_->dirty().invalidate(node, runtime::DirtyFlags::Material);
            }
        }
    }
}

std::size_t EmptyResultComponentHost::tick_auxiliary(animation::AnimationTime now) {
    synchronize_auxiliary_motion();
    std::size_t changed{};
    for (auto id : results_) {
        auto* s = services_->components().state<PresentationState>(id);
        if (!s) {
            continue;
        }
        s->observed = std::max(now, s->observed);
        if (!s->motion_start) {
            continue;
        }
        const auto duration = animation::resolve_motion_tokens(services_->components().theme_scope(id)->snapshot())
                                  .fast.count_microseconds();
        const float p = duration <= 0
                            ? 1
                            : std::clamp(static_cast<float>((s->observed - *s->motion_start).count_microseconds()) /
                                             static_cast<float>(duration),
                                         0.0F, 1.0F);
        s->snapshot.visual_motion = p * p * (3 - 2 * p);
        if (p == 1) {
            s->motion_start.reset();
        }
        const auto node = services_->components().root(s->snapshot.image);
        if (services_->nodes().set_paint_opacity(node, s->snapshot.visual_motion)) {
            services_->dirty().invalidate(node, runtime::DirtyFlags::Material);
        }
        ++changed;
    }
    return changed;
}

std::optional<animation::AnimationTime> EmptyResultComponentHost::next_auxiliary_deadline() const {
    for (auto id : results_) {
        const auto* s = services_->components().state<PresentationState>(id);
        if (s && s->motion_start && s->animated && s->in_view && result_motion(*services_, id) &&
            services_->nodes().content_opacity(s->node) > 0) {
            return std::max(s->observed, services_->animation_time()) +
                   animation::AnimationDuration::microseconds(16000);
        }
    }
    return {};
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
                const auto bounds = s->snapshot.image_bounds;
                const auto visible = graphics::intersect_effect_bounds(
                    {bounds.x + node.translation.x, bounds.y + node.translation.y, bounds.width, bounds.height},
                    services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, clip)));
                s->in_view = visible.width > 0 && visible.height > 0;
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
    synchronize_auxiliary_motion();
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
