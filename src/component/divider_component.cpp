#include "component/divider_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local DividerComponentHost *active_divider{};
struct DividerState {
  runtime::ComponentId component, label;
  runtime::NodeId node;
  component::RetainedSurfaceId range;
  DividerType type{DividerType::Horizontal};
  DividerOrientation orientation{DividerOrientation::Center};
  DividerOrientationMargin orientation_margin;
  bool dashed{}, plain{}, slot{}, content_nonempty{};
  bool inherited_line_height{};
  float current_line_height{22};
  Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
  Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
  DividerSnapshot geometry;
  runtime::Size label_size;
  theme_runtime::Subscription colors, metrics, fonts;
};
void validate(DividerOrientationMargin value) {
  if (value.source != DividerOrientationMargin::Source::Theme &&
      value.source != DividerOrientationMargin::Source::None &&
      value.source != DividerOrientationMargin::Source::Ratio)
    throw std::invalid_argument("Divider orientation margin source is invalid");
  if (value.source == DividerOrientationMargin::Source::Ratio &&
      (!std::isfinite(value.ratio) || value.ratio < 0 || value.ratio > 1))
    throw std::invalid_argument(
        "Divider orientation margin ratio must be in [0,1]");
}
} // namespace
DividerComponentHost::DividerComponentHost(WindowComponentServices &services)
    : services_(&services) {
  services.attach(*this);
}
DividerComponentHost::~DividerComponentHost() { services_->detach(*this); }
void *DividerComponentHost::begin_mount() noexcept {
  auto *previous = active_divider;
  active_divider = this;
  return previous;
}
void DividerComponentHost::end_mount(void *previous) noexcept {
  active_divider = static_cast<DividerComponentHost *>(previous);
}
void DividerComponentHost::on_destroy() noexcept {
  std::erase_if(mounted_, [this](auto id) {
    return !services_->components().contains(id);
  });
}
DividerSnapshot DividerComponentHost::snapshot(runtime::ComponentId id) const {
  const auto *s = services_->components().state<DividerState>(id);
  if (!s)
    throw std::out_of_range("Divider component is stale");
  return s->geometry;
}
void DividerComponentHost::update(runtime::ComponentId id, bool geometry) {
  auto *s = services_->components().state<DividerState>(id);
  if (!s)
    return;
  const auto &theme = services_->components().theme_scope(id)->snapshot();
  const auto &token = theme.divider();
  const auto color = s->plain ? token.colors.plain_text : token.colors.text;
  s->foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
  const auto &typography = theme.typography();
  const float size = s->plain ? token.typography.plain_font_size
                              : token.typography.text_font_size;
  s->typography.set(
      {typography.font_family,
       s->plain ? token.typography.plain_font_weight
                : token.typography.text_font_weight,
       false, size,
       size * typography.base_line_height / typography.base_font_size});
  services_->dirty().invalidate(s->node, geometry
                                             ? runtime::DirtyFlags::Measure |
                                                   runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry
                                             : runtime::DirtyFlags::Material);
}
void DividerComponentHost::mount(const DividerProps &props,
                                 const std::optional<DividerText> &text) {
  auto &build = runtime::require_component_build_context();
  auto &services = *services_;
  const auto id = build.mount_component<DividerState>();
  auto &s = build.state<DividerState>(id);
  s.component = id;
  s.node = build.root(id);
  s.slot = text.has_value();
  s.type = read_prop(props.type_);
  s.orientation = read_prop(props.orientation_);
  s.orientation_margin = read_prop(props.margin_);
  validate(s.orientation_margin);
  s.plain = read_prop(props.plain_);
  s.dashed = read_prop(props.dashed_);
  s.content_nonempty = !read_prop(props.content_).empty();
  s.current_line_height =
      build.semantic_typography()
          ? read_prop(*build.semantic_typography()).line_height
          : build.theme_scope()->snapshot().typography().base_line_height;
  s.inherited_line_height = build.semantic_typography().has_value();
  runtime::connect_layout_style(build.scope(id), props.layout_, s.node,
                                services.nodes(), services.dirty());
  const auto fragment = build.register_scene_fragment(
      id, runtime::SceneFragmentPlacement::before_children);
  s.range = services.surfaces().create_content_range(fragment, {});
  build.on_resource_cleanup(id, [&services, range = s.range, node = s.node] {
    services.surfaces().destroy_content_range(range);
    services.layout().remove_layout(node);
  });
  update(id, true);
  build.mount_slot(id, Content{[&services, &s, text, content = props.content_] {
                     auto &context = runtime::require_component_build_context();
                     s.label = context.mount_component<int>(0);
                     services.layout().set_layout(context.root(s.label),
                                                  layout::BoxLayout{});
                     context.on_resource_cleanup(
                         s.label, [&services, node = context.root(s.label)] {
                           services.layout().remove_layout(node);
                         });
                     context.mount_slot_with_semantic_text_style(
                         s.label, Content{[text, content] {
                           if (text)
                             SlotContentAccess::function (*text)();
                           else
                             ryn::Text(TextProps{}.content(content));
                         }},
                         Prop<runtime::SemanticForeground>{s.foreground},
                         Prop<runtime::SemanticTypography>{s.typography});
                   }});
  if (!text) {
    const auto &mounted = services.text().mounted_texts().back();
    services.text().scene_service().set_ellipsis(mounted.scene,
                                                 {1, String{u8"…"}, false, 0});
    const auto revisions =
        services.text().scene_service().revisions(mounted.scene);
    services.layout().set_intrinsic_revision(
        services.components().root(mounted.component),
        revisions.content + revisions.layout);
  }
  services.layout().set_layout(
      s.node,
      layout::ComponentLayout{
          [this, id](layout::LayoutEngine &engine, runtime::NodeId,
                     layout::Constraints limits) {
            auto &s = *services_->components().state<DividerState>(id);
            const auto &token =
                services_->components().theme_scope(id)->snapshot().divider();
            s.geometry = {};
            s.geometry.vertical = s.type == DividerType::Vertical;
            s.geometry.dashed = s.dashed;
            s.geometry.plain = s.plain;
            s.geometry.line_width = token.metrics.line_width;
            if (s.geometry.vertical) {
              if (services_->components().set_branch_active(s.label, false))
                services_->mark_scene_structure_dirty();
              s.geometry.margin = token.metrics.vertical_margin_inline;
              s.geometry.left = {
                  s.geometry.margin, -0.06F * s.current_line_height,
                  token.metrics.line_width, 0.9F * s.current_line_height};
              return limits.constrain(
                  {2 * s.geometry.margin + token.metrics.line_width,
                   0.9F * s.current_line_height});
            }
            const bool potential_label = s.slot || s.content_nonempty;
            float padding = token.metrics.text_padding_inline;
            const bool no_margin = s.orientation_margin.source ==
                                   DividerOrientationMargin::Source::None;
            if (no_margin && s.orientation != DividerOrientation::Center)
              padding = 0;
            if (std::isfinite(limits.max_width))
              padding = std::min(padding, limits.max_width * 0.5F);
            s.label_size =
                potential_label
                    ? engine.measure_child(
                          services_->components().root(s.label),
                          {0, std::max(0.0F, limits.max_width - 2 * padding), 0,
                           limits.max_height})
                    : runtime::Size{};
            const bool label = potential_label &&
                               (s.content_nonempty || s.label_size.width > 0);
            if (services_->components().set_branch_active(s.label, label))
              services_->mark_scene_structure_dirty();
            s.geometry.has_label = label;
            s.geometry.margin = label
                                    ? token.metrics.horizontal_with_text_margin
                                    : token.metrics.horizontal_margin;
            const float width = std::isfinite(limits.max_width)
                                    ? limits.max_width
                                    : s.label_size.width + 2 * padding +
                                          2 * token.metrics.line_width;
            const float content_height =
                label ? std::max(token.metrics.line_width, s.label_size.height)
                      : token.metrics.line_width;
            const float height = content_height + 2 * s.geometry.margin;
            const float y = s.geometry.margin +
                            (content_height - token.metrics.line_width) * 0.5F;
            if (!label) {
              s.geometry.left = {0, y, width, token.metrics.line_width};
              return limits.constrain({width, height});
            }
            const float occupied =
                std::min(width, s.label_size.width + 2 * padding);
            const float remaining = std::max(0.0F, width - occupied);
            float left = remaining * 0.5F;
            if (s.orientation != DividerOrientation::Center) {
              float ratio = s.orientation_margin.source ==
                                    DividerOrientationMargin::Source::Ratio
                                ? s.orientation_margin.ratio
                                : token.metrics.orientation_margin;
              if (no_margin)
                ratio = 0;
              const float edge = std::clamp(width * ratio, 0.0F, remaining);
              left = s.orientation == DividerOrientation::Left
                         ? edge
                         : remaining - edge;
            }
            s.geometry.left = {0, y, left, token.metrics.line_width};
            s.geometry.right = {left + occupied, y, remaining - left,
                                token.metrics.line_width};
            s.geometry.label = {left + padding, s.geometry.margin,
                                std::min(s.label_size.width,
                                         std::max(0.0F, width - 2 * padding)),
                                s.label_size.height};
            return limits.constrain({width, height});
          },
          [this, id](layout::LayoutEngine &engine, runtime::NodeId,
                     runtime::Rect bounds) {
            auto &s = *services_->components().state<DividerState>(id);
            if (!s.geometry.has_label)
              return;
            const auto label = s.geometry.label;
            engine.place_child(services_->components().root(s.label),
                               {bounds.x + label.x, bounds.y + label.y,
                                label.width, label.height});
          }});
  mounted_.push_back(id);
  auto &scope = build.scope(id);
  connect_prop(scope, props.type_, [this, id](DividerType value) {
    if (value != DividerType::Horizontal && value != DividerType::Vertical)
      throw std::invalid_argument("Divider type is invalid");
    if (auto *s = services_->components().state<DividerState>(id)) {
      s->type = value;
      update(id, true);
    }
  });
  connect_prop(scope, props.orientation_, [this, id](DividerOrientation value) {
    if (value != DividerOrientation::Left &&
        value != DividerOrientation::Center &&
        value != DividerOrientation::Right)
      throw std::invalid_argument("Divider orientation is invalid");
    if (auto *s = services_->components().state<DividerState>(id)) {
      s->orientation = value;
      update(id, true);
    }
  });
  connect_prop(
      scope, props.margin_, [this, id](DividerOrientationMargin value) {
        validate(value);
        if (auto *s = services_->components().state<DividerState>(id)) {
          s->orientation_margin = value;
          update(id, true);
        }
      });
  connect_prop(scope, props.dashed_, [this, id](bool value) {
    if (auto *s = services_->components().state<DividerState>(id)) {
      s->dashed = value;
      s->geometry.dashed = value;
      services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry);
    }
  });
  connect_prop(scope, props.plain_, [this, id](bool value) {
    if (auto *s = services_->components().state<DividerState>(id)) {
      s->plain = value;
      update(id, true);
    }
  });
  connect_prop(scope, props.content_, [this, id](String value) {
    if (auto *s = services_->components().state<DividerState>(id)) {
      s->content_nonempty = !value.empty();
      update(id, true);
    }
  });
  if (build.semantic_typography())
    connect_prop(scope, *build.semantic_typography(),
                 [this, id](runtime::SemanticTypography value) {
                   if (auto *s =
                           services_->components().state<DividerState>(id)) {
                     s->current_line_height = value.line_height;
                     update(id, true);
                   }
                 });
  const auto theme = build.theme_scope();
  s.colors = theme->capture(
      [this, id](theme_runtime::DirtyPhase) { update(id, false); },
      [theme] { (void)theme->divider_colors(); });
  s.metrics = theme->capture(
      [this, id](theme_runtime::DirtyPhase) { update(id, true); },
      [theme] { (void)theme->divider_metrics(); });
  s.fonts = theme->capture(
      [this, id](theme_runtime::DirtyPhase) {
        if (auto *s = services_->components().state<DividerState>(id);
            s && !s->inherited_line_height)
          s->current_line_height = services_->components()
                                       .theme_scope(id)
                                       ->snapshot()
                                       .typography()
                                       .base_line_height;
        update(id, true);
      },
      [theme] {
        (void)theme->divider_typography();
        (void)theme->typography_fonts();
        (void)theme->typography_base_typography();
      });
}
void DividerComponentHost::synchronize_auxiliary_geometry(
    runtime::Size viewport, runtime::Rect clip) {
  for (auto id : mounted_) {
    auto *s = services_->components().state<DividerState>(id);
    if (!s)
      continue;
    const auto &node = services_->nodes().require(s->node);
    const auto color = services_->components()
                           .theme_scope(id)
                           ->snapshot()
                           .divider()
                           .colors.line;
    std::vector<graphics::QuadInstance> quads;
    const auto append = [&](runtime::Rect rect) {
      rect.x += node.bounds.x + node.translation.x;
      rect.y += node.bounds.y + node.translation.y;
      const float right = std::min(rect.x + rect.width, clip.x + clip.width),
                  bottom = std::min(rect.y + rect.height, clip.y + clip.height);
      rect.x = std::max(rect.x, clip.x);
      rect.y = std::max(rect.y, clip.y);
      rect.width = std::max(0.0F, right - rect.x);
      rect.height = std::max(0.0F, bottom - rect.y);
      if (rect.width <= 0 || rect.height <= 0)
        return;
      graphics::QuadInstance quad;
      quad.clip_rect = {
          -1 + 2 * rect.x / viewport.width, 1 - 2 * rect.y / viewport.height,
          2 * rect.width / viewport.width, -2 * rect.height / viewport.height};
      quad.color = {color.red(), color.green(), color.blue(), color.alpha()};
      quad.opacity = node.opacity;
      quads.push_back(quad);
    };
    const auto rail = [&](runtime::Rect rect) {
      if (!s->dashed) {
        append(rect);
        return;
      }
      const float dash = 3 * s->geometry.line_width;
      const float length = s->geometry.vertical ? rect.height : rect.width;
      if (dash <= 0)
        return;
      for (float offset = 0; offset < length; offset += dash * 2) {
        auto segment = rect;
        if (s->geometry.vertical) {
          segment.y += offset;
          segment.height = std::min(dash, length - offset);
        } else {
          segment.x += offset;
          segment.width = std::min(dash, length - offset);
        }
        append(segment);
      }
    };
    rail(s->geometry.left);
    rail(s->geometry.right);
    (void)services_->surfaces().update_content_range(s->range, quads);
  }
}
} // namespace ryn::detail
namespace ryn {
void Divider(DividerProps props) {
  if (!detail::active_divider)
    throw std::logic_error("Divider requires window component services");
  detail::active_divider->mount(props, {});
}
void Divider(DividerProps props, DividerText text) {
  if (!detail::active_divider)
    throw std::logic_error("Divider requires window component services");
  detail::active_divider->mount(props, text);
}
} // namespace ryn
