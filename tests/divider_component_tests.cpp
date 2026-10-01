#include "component/divider_component.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>
namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < 0.002F; }
void forms() {
  Fixture f;
  f.services.mount(Content{[] {
    Divider();
    Divider(DividerProps{}.content(u8"Center"));
    Divider(
        DividerProps{}.content(u8"Left").orientation(DividerOrientation::Left));
    Divider(DividerProps{}.content(u8"Right").orientation(
        DividerOrientation::Right));
    Divider(DividerProps{}.type(DividerType::Vertical).content(u8"Ignored"));
    Divider(DividerProps{}.dashed(true));
    Divider(DividerProps{}.content(u8"Plain").plain(true));
    Divider(DividerProps{}.orientation(DividerOrientation::Left),
            DividerText{[] { Text(u8"Typed slot"); }});
  }});
  f.synchronize(300, {0, 0, 300, 1000});
  const auto ids = f.services.divider().mounted();
  require(ids.size() == 8, "Divider forms did not mount");
  const auto token = resolve_theme().divider();
  const auto blank = f.services.divider().snapshot(ids[0]);
  require(!blank.has_label && near(blank.left.width, 300) &&
              near(blank.margin, token.metrics.horizontal_margin),
          "empty Divider geometry drifted");
  const auto center = f.services.divider().snapshot(ids[1]);
  require(center.has_label && near(center.left.width, center.right.width) &&
              near(center.label.x,
                   center.left.width + token.metrics.text_padding_inline),
          "center rails/label spacing wrong");
  const auto left = f.services.divider().snapshot(ids[2]),
             right = f.services.divider().snapshot(ids[3]);
  require(near(left.left.width, 15) && near(right.right.width, 15),
          "Theme orientation ratio not applied");
  const auto vertical = f.services.divider().snapshot(ids[4]);
  require(vertical.vertical && !vertical.has_label &&
              near(vertical.left.height, 19.8F) &&
              near(vertical.left.y, -1.32F) &&
              near(f.nodes.require(f.services.components().root(ids[4]))
                       .measured_size.width,
                   17),
          "vertical fixed line-height geometry wrong");
  require(f.services.divider().snapshot(ids[7]).has_label,
          "typed Divider slot missing");
  require(f.services.interactions().size() == 0 &&
              !f.inputs.sessions().active().valid(),
          "Divider acquired interaction or IME state");
  const auto scene = f.services.text().mounted_texts()[2].scene;
  const auto text_node = f.scene.node(scene);
  const auto root =
      f.nodes.require(f.services.components().root(ids[2])).bounds;
  const auto glyph = f.scene.primitive(scene).instances;
  require(glyph.count > 0 &&
              near(f.nodes.require(text_node).bounds.x, root.x + left.label.x),
          "label placement missed same-frame geometry");
  require((f.scene.glyph_scene().instances().at(glyph.first).position_size[0] +
           1) * 150 >=
              root.x + left.label.x - 4,
          "glyphs remained at previous position");
  bool found_dash = false;
  for (const auto &quad : f.services.surfaces().instances().instances())
    if (near(quad.clip_rect[2] * 150, 3))
      found_dash = true;
  require(found_dash, "dashed Divider emitted no dash segments");
}
void reactive() {
  Fixture f;
  Signal<String> content{String{}};
  Signal<DividerOrientation> orientation{DividerOrientation::Left};
  Signal<DividerOrientationMargin> margin{DividerOrientationMargin::theme()};
  Signal<bool> plain{false}, dashed{false};
  Signal<DividerType> type{DividerType::Horizontal};
  Signal<ThemeConfig> theme{ThemeConfig{}};
  f.services.mount(Content{[&] {
    Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
            Divider(DividerProps{}
                        .content(content)
                        .orientation(orientation)
                        .orientationMargin(margin)
                        .plain(plain)
                        .dashed(dashed)
                        .type(type));
          }});
    Text(u8"Sibling");
  }});
  f.synchronize(300);
  const auto id = f.services.divider().mounted().front();
  const auto component_count = f.services.components().component_count(),
             runs = f.services.components().mount_runs();
  const auto label = f.services.text().mounted_texts().front();
  const auto sibling = f.services.text().mounted_texts().back();
  const auto sibling_shapes =
      f.scene.text_state(sibling.scene).counters().shape_count;
  content.set(String{u8"Label"});
  f.synchronize(300);
  require(f.services.divider().snapshot(id).has_label,
          "empty->label update failed");
  margin.set(DividerOrientationMargin::fraction(.2F));
  f.synchronize(300);
  require(near(f.services.divider().snapshot(id).left.width, 60),
          "explicit ratio did not override Theme");
  margin.set(DividerOrientationMargin::fraction(0));
  f.synchronize(300);
  auto geometry = f.services.divider().snapshot(id);
  require(near(geometry.left.width, 0) && near(geometry.label.x, 16),
          "zero ratio was confused with None");
  margin.set(DividerOrientationMargin::none());
  f.synchronize(300);
  geometry = f.services.divider().snapshot(id);
  require(near(geometry.left.width, 0) && near(geometry.label.x, 0),
          "None margin failed no-default padding");
  orientation.set(DividerOrientation::Right);
  f.synchronize(300);
  require(near(f.services.divider().snapshot(id).right.width, 0),
          "right None did not suppress edge rail");
  margin.set(DividerOrientationMargin::theme());
  plain.set(true);
  f.synchronize(300);
  require(f.scene.text_state(label.scene)
                  .shaped()
                  .default_metrics.logical_pixel_size == 14,
          "plain label typography not inherited");
  type.set(DividerType::Vertical);
  f.synchronize(300);
  require(!f.services.divider().snapshot(id).has_label,
          "vertical did not suspend label");
  type.set(DividerType::Horizontal);
  f.synchronize(300);
  content.set(String{});
  f.synchronize(300);
  require(!f.services.divider().snapshot(id).has_label &&
              near(f.services.divider().snapshot(id).left.width, 300),
          "label->empty failed full rail");
  content.set(String{u8"A very long label with Chinese 中文和很多内容"});
  f.synchronize(18, {0, 0, 18, 240});
  geometry = f.services.divider().snapshot(id);
  require(geometry.left.width == 0 && geometry.right.width == 0 &&
              geometry.label.x + geometry.label.width <= 18.001F,
          "oversized label overflowed rails");
  f.synchronize(300);
  const auto node = f.services.components().root(id);
  const auto measures = f.nodes.require(node).measure_count,
             places = f.nodes.require(node).place_count;
  const auto shapes = f.scene.text_state(label.scene).counters().shape_count;
  const auto surface = f.services.surfaces().diagnostics();
  auto changed = ThemeConfig{};
  changed.divider.tokens.line = Color::rgba8(255, 0, 0);
  f.dirty.clear();
  theme.set(changed);
  require(f.dirty.layout_roots().empty(),
          "Divider line color requested layout");
  f.synchronize(300);
  require(f.nodes.require(node).measure_count == measures &&
              f.nodes.require(node).place_count == places &&
              f.scene.text_state(label.scene).counters().shape_count == shapes,
          "color update reshaped or laid out Divider");
  require(f.services.surfaces().diagnostics().geometry_updates ==
                  surface.geometry_updates &&
              f.services.surfaces().diagnostics().material_updates >
                  surface.material_updates,
          "line color did not use material path");
  require(f.services.components().component_count() == component_count &&
              f.services.components().mount_runs() == runs &&
              f.scene.text_state(sibling.scene).counters().shape_count ==
                  sibling_shapes,
          "Divider update remounted or reshaped sibling");
  require(f.services.destroy(id), "Divider destroy failed");
  require(f.services.divider().mounted().empty() &&
              f.services.surfaces().size() == 0,
          "Divider teardown leaked ranges");
}
} // namespace
int main() {
  try {
    forms();
    reactive();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
