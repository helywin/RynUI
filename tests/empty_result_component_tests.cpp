#include "component/empty_result_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void synchronize(Fixture& f, float width = 480, float height = 900) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}),
          "presentation synchronization failed");
}

bool inside(runtime::Rect child, runtime::Rect parent) {
    constexpr float tolerance = .02F;
    return child.x >= parent.x - tolerance && child.y >= parent.y - tolerance &&
           child.x + child.width <= parent.x + parent.width + tolerance &&
           child.y + child.height <= parent.y + parent.height + tolerance;
}

bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

detail::MountedTextComponent text_child(Fixture& f, runtime::ComponentId region) {
    const auto texts = f.services.text().mounted_texts();
    const auto found = std::ranges::find_if(
        texts, [&](const auto& t) { return f.services.components().parent(t.component) == region; });
    check(found != texts.end(), "presentation label/Icon component absent");
    return *found;
}

void click(Fixture& f, runtime::ComponentId component) {
    const auto r = bounds(f, component);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    synchronize(f);
}

std::size_t visible_glyphs_inside(Fixture& f, runtime::Rect allocated) {
    std::size_t visible = 0;
    for (const auto& glyph : f.scene.glyph_scene().instances().instances()) {
        for (float v : glyph.position_size) {
            check(std::isfinite(v), "nonfinite glyph geometry");
        }
        if (glyph.translation_opacity[2] <= 0 || glyph.color[3] <= 0) {
            continue;
        }
        const auto& p = glyph.position_size;
        const auto& c = glyph.clip_bounds;
        const float left = std::max(p[0] + glyph.translation_opacity[0], c[0]);
        const float top = std::max(p[1] + glyph.translation_opacity[1], c[1]);
        const float right = std::min(p[0] + glyph.translation_opacity[0] + p[2], c[2]);
        const float bottom = std::min(p[1] + glyph.translation_opacity[1] + p[3], c[3]);
        if (right > left && bottom > top) {
            ++visible;
            check(inside({left, top, right - left, bottom - top}, allocated),
                  "actual visible glyph/Icon escaped allocated content");
        }
    }
    return visible;
}

void actual_surface_clip(Fixture& f, runtime::Rect allocated) {
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        const auto r = effect.geometry.shape.rect;
        check(std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) && std::isfinite(r.height) &&
                  r.width >= 0 && r.height >= 0,
              "nonfinite/negative surface geometry");
        for (auto point : {runtime::Point{allocated.x - 1, allocated.y + allocated.height / 2},
                           runtime::Point{allocated.x + allocated.width + 1, allocated.y + allocated.height / 2},
                           runtime::Point{allocated.x + allocated.width / 2, allocated.y - 1},
                           runtime::Point{allocated.x + allocated.width / 2, allocated.y + allocated.height + 1}}) {
            check(effect.material.opacity <= 0 || graphics::rounded_effect_coverage(point, effect) == 0,
                  "actual surface escaped short allocated content");
        }
    }
}

void retained_actions_and_editors(bool result) {
    Signal<String> label{String{u8"Saved / 完成"}};
    Signal<String> subtitle{String{u8"Details / 详情"}};
    Signal<ResultStatus> status{ResultStatus{Status::Success}};
    Signal<EmptyImage> image{EmptyImage::Default};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int root_runs = 0;
    int extra_runs = 0;
    int content_runs = 0;
    int clicks = 0;
    Fixture f;
    f.services.mount(Content{[&] {
        ++root_runs;
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  const auto extra = [&] {
                      ++extra_runs;
                      Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Retry"); });
                      Input(InputProps{}.defaultValue(u8"Draft").layout(LayoutStyle{}.width(dp(140))));
                      Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Save"); });
                  };
                  if (result) {
                      Result(ResultProps{}.status(status).title(label).subtitle(subtitle),
                             {.content = ResultContent{[&] {
                                  ++content_runs;
                                  Text(u8"Retained result details");
                              }},
                              .extra = ResultExtra{extra}});
                  } else {
                      Empty(EmptyProps{}.description(label).image(image), {.extra = EmptyExtra{extra}});
                  }
              }});
    }});
    synchronize(f);
    const auto id = result ? f.services.empty_result().results()[0] : f.services.empty_result().empties()[0];
    const auto original = f.services.empty_result().snapshot(id);
    check(original.image_bounds.width == (result ? 72 : 100) && original.image_bounds.height == (result ? 72 : 100),
          "default native icon dimensions drifted");
    const auto visual = text_child(f, original.image);
    check(f.scene.primitive(visual.scene).instances.count > 0, "default Icon produced no actual glyph");
    if (result) {
        check(std::ranges::any_of(
                  f.services.rounded_effects().packed_instances(),
                  [&](const auto& effect) {
                      return effect.geometry.shape.rect == original.content_bounds && effect.material.opacity > 0 &&
                             effect.material.color ==
                                 f.services.components().theme_scope(id)->snapshot().result().colors.content_background;
                  }),
              "Result body background did not reach logical surface");
    }
    const auto button = f.buttons.mounted_buttons()[0];
    const auto input = f.inputs.mounted_inputs()[0];
    const auto first_bounds = bounds(f, button.component);
    const auto last_bounds = bounds(f, f.buttons.mounted_buttons()[1].component);
    const auto extra = original.extra_bounds;
    check(first_bounds.y >= extra.y + 4 && inside(first_bounds, extra) && inside(last_bounds, extra) &&
              std::abs((first_bounds.x + last_bounds.x + last_bounds.width) / 2 - (extra.x + extra.width / 2)) < .1F,
          "native extra actions are not spaced and centered");
    check(original.label_bounds.y >= original.image_bounds.y + original.image_bounds.height + (result ? 24 : 8),
          "presentation image/label gap missing");
    if (result) {
        check(original.subtitle_bounds.y >= original.label_bounds.y + original.label_bounds.height + 8 &&
                  original.extra_bounds.y >= original.subtitle_bounds.y + original.subtitle_bounds.height + 24 &&
                  original.content_bounds.y >= original.extra_bounds.y + original.extra_bounds.height + 24,
              "Result region order/gaps drifted");
    }
    click(f, button.component);
    check(clicks == 1, "native extra Button did not activate");
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard), "extra Button focus failed");
    synchronize(f);
    const runtime::Point focus_point{first_bounds.x + first_bounds.width / 2, first_bounds.y - 2};
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [&](const auto& effect) {
                                  return effect.geometry.kind == graphics::RoundedEffectKind::outline &&
                                         effect.material.opacity > 0 &&
                                         graphics::rounded_effect_coverage(focus_point, effect) > .2F;
                              }),
          "extra viewport clipped native focus decoration");
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard), "extra Input focus failed");
    (void)f.inputs.editors().require(input.editor).select_all();
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Retained / 保留"}, f.inputs.sessions().active()})),
          "extra Input edit failed");
    synchronize(f);
    const auto session = f.inputs.sessions().active();
    const auto component_count = f.services.components().component_count();
    label.set(String{u8"A longer reactive label that wraps and remains retained across multiple lines while "
                     u8"preserving the actions and the native editor / 更新后的多行说明"});
    subtitle.set(String{u8"Updated subtitle / 新详情"});
    status.set(ResultException::NotFound);
    image.set(EmptyImage::Compact);
    synchronize(f);
    auto after = f.services.empty_result().snapshot(id);
    if (!result) {
        check(after.image_bounds.width == 40 && after.image_bounds.height == 40,
              "compact Empty did not resize its native Icon");
    }
    check(after.image == original.image && after.label == original.label && after.extra == original.extra &&
              after.content == original.content && f.services.components().component_count() == component_count &&
              f.inputs.mounted_inputs()[0].editor == input.editor && f.inputs.sessions().active() == session &&
              f.inputs.editors().require(input.editor).value() == String{u8"Retained / 保留"}.bytes(),
          "ordinary presentation update replaced slots/editor/focus/session");
    const auto label_text = text_child(f, after.label);
    check(f.scene.text_state(label_text.scene).content().bytes() == label.get().bytes() &&
              f.scene.text_state(label_text.scene).measurement().lines.size() > 1,
          "reactive label failed to wrap");
    const auto node = f.services.components().root(id);
    const auto measures = f.nodes.require(node).measure_count;
    ThemeConfig config;
    config.empty.tokens.description = Color::rgba8(20, 70, 90);
    config.result.tokens.title = Color::rgba8(20, 70, 90);
    theme.set(config);
    synchronize(f);
    check(f.nodes.require(node).measure_count == measures, "presentation color-only update remeasured");
    const auto primitive = f.scene.primitive(label_text.scene);
    check(primitive.instances.count > 0, "label produced no actual glyphs");
    const auto glyph = f.scene.glyph_scene().instances().at(primitive.instances.first);
    check(glyph.color == std::array{20 / 255.0F, 70 / 255.0F, 90 / 255.0F, 1.0F},
          "presentation Theme color did not reach glyph material");
    config.empty.tokens.extra_gap = dp(26);
    config.result.tokens.extra_gap = dp(30);
    theme.set(config);
    synchronize(f);
    check(f.nodes.require(node).measure_count > measures && root_runs == 1 && extra_runs == 1 &&
              content_runs == (result ? 1 : 0),
          "metric update missed layout or reran retained slot");
    label.set(String{});
    subtitle.set(String{});
    image.set(EmptyImage::None);
    synchronize(f);
    check(!f.services.components().branch_active(original.label) &&
              (result || !f.services.components().branch_active(original.image)) &&
              f.inputs.sessions().active() == session,
          "empty label/image changed unrelated active editor");
    label.set(String{u8"Restored"});
    image.set(EmptyImage::Default);
    synchronize(f);
    click(f, f.buttons.mounted_buttons()[1].component);
    check(clicks == 2 && extra_runs == 1, "restored presentation action failed");
    (void)f.frames.consume_request();
    synchronize(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "static presentation scheduled idle frames");
    check(f.services.destroy(id), "presentation destroy failed");
    synchronize(f);
    check(f.services.empty_result().empties().empty() && f.services.empty_result().results().empty() &&
              f.inputs.mounted_inputs().empty() && f.buttons.mounted_buttons().empty() &&
              f.services.interactions().size() == 0 && f.services.rounded_effects().live_count() == 0 &&
              f.scene.size() == 0,
          "presentation teardown leaked resources");
}

void typed_slots_and_statuses() {
    Fixture f;
    Signal<ResultStatus> status{ResultStatus{Status::Default}};
    int image_runs = 0;
    int icon_runs = 0;
    f.services.mount(Content{[&] {
        Empty(EmptyProps{}.image(EmptyImage::Compact).description(u8"Custom"),
              {.image = EmptyImageContent{[&] {
                   ++image_runs;
                   Icon(IconProps{}.name(IconName::SearchOutlined));
               }}});
        Result(ResultProps{}.status(status).title(u8"Custom result"),
               {.icon = ResultIcon{[&] {
                    ++icon_runs;
                    Icon(IconProps{}.name(IconName::SmileOutlined));
                }}});
        Result(ResultProps{}.status(status).title(u8"Default result"));
    }});
    synchronize(f);
    const auto custom = f.services.empty_result().results()[0];
    const auto plain = f.services.empty_result().results()[1];
    const auto custom_icon = text_child(f, f.services.empty_result().snapshot(custom).image).component;
    const auto plain_icon = text_child(f, f.services.empty_result().snapshot(plain).image).component;
    for (const auto& [value, icon] :
         std::array{std::pair{ResultStatus{Status::Success}, IconName::CheckCircleFilled},
                    std::pair{ResultStatus{Status::Default}, IconName::ExclamationCircleFilled},
                    std::pair{ResultStatus{Status::Warning}, IconName::WarningFilled},
                    std::pair{ResultStatus{Status::Error}, IconName::CloseCircleFilled},
                    std::pair{ResultStatus{Status::Processing}, IconName::ExclamationCircleFilled},
                    std::pair{ResultStatus{Status::None}, IconName::ExclamationCircleFilled},
                    std::pair{ResultStatus{ResultException::Forbidden}, IconName::LockOutlined},
                    std::pair{ResultStatus{ResultException::NotFound}, IconName::FileSearchOutlined},
                    std::pair{ResultStatus{ResultException::ServerError}, IconName::CloudServerOutlined}}) {
        status.set(value);
        synchronize(f, 480, 1600);
        check(f.services.text().icon_snapshot(custom_icon).source == IconSource{IconName::SmileOutlined} &&
                  f.services.text().icon_snapshot(plain_icon).source == IconSource{icon} && image_runs == 1 &&
                  icon_runs == 1,
              "status update replaced custom icon or mapped incorrectly");
    }
}

void constrained_geometry(bool result) {
    Fixture f;
    Signal<float> width{230};
    Signal<float> height{700};
    const auto style = LayoutStyle{}.width(bind([width] { return dp(width.get()); })).height(bind([height] {
        return dp(height.get());
    }));
    f.services.mount(Content{[&] {
        const auto extra = [] {
            Button(ButtonProps{}.layout(LayoutStyle{}.width(dp(100))), [] { Text(u8"First action"); });
            Button(ButtonProps{}.layout(LayoutStyle{}.width(dp(100))), [] { Text(u8"Second action"); });
        };
        if (result) {
            Result(ResultProps{}
                       .title(u8"Long wrapped title / 标题")
                       .subtitle(u8"Long subtitle that wraps in finite widths")
                       .layout(style),
                   {.content = ResultContent{[] { Text(u8"Long content body that wraps / 详细内容"); }},
                    .extra = ResultExtra{extra}});
        } else {
            Empty(EmptyProps{}.description(u8"Long description that wraps within finite widths / 说明").layout(style),
                  {.extra = EmptyExtra{extra}});
        }
    }});
    synchronize(f);
    const auto id = result ? f.services.empty_result().results()[0] : f.services.empty_result().empties()[0];
    auto snap = f.services.empty_result().snapshot(id);
    if (result) {
        check(bounds(f, f.buttons.mounted_buttons()[1].component).y >
                  bounds(f, f.buttons.mounted_buttons()[0].component).y,
              "extra actions did not wrap under finite width");
    }
    check(visible_glyphs_inside(f, bounds(f, id)) > 0, "geometry test produced no visible glyphs");
    for (auto dimensions :
         {runtime::Size{76, 70}, runtime::Size{12, 5}, runtime::Size{0, 0}, runtime::Size{230, 700}}) {
        width.set(dimensions.width);
        height.set(dimensions.height);
        synchronize(f);
        snap = f.services.empty_result().snapshot(id);
        const auto allocated = bounds(f, id);
        check(allocated.width == dimensions.width && allocated.height == dimensions.height,
              "external constraints ignored");
        for (auto r :
             {snap.image_bounds, snap.label_bounds, snap.subtitle_bounds, snap.extra_bounds, snap.content_bounds}) {
            check(std::isfinite(r.x) && std::isfinite(r.y) && r.width >= 0 && r.height >= 0 && inside(r, allocated),
                  "presentation region escaped short/zero constraints");
        }
        const auto visible = visible_glyphs_inside(f, allocated);
        if (dimensions.width == 0) {
            check(visible == 0, "zero presentation retained visible glyphs");
        }
        actual_surface_clip(f, allocated);
    }
}

void short_text_clips_and_invalid_roll_back() {
    Fixture f;
    f.services.mount(Content{[] {
        Empty(EmptyProps{}
                  .image(EmptyImage::None)
                  .description(u8"Clipped real text")
                  .layout(LayoutStyle{}.width(dp(80)).height(dp(8))));
    }});
    synchronize(f);
    const auto id = f.services.empty_result().empties()[0];
    const auto snap = f.services.empty_result().snapshot(id);
    check(snap.label_bounds.height == 8 && visible_glyphs_inside(f, snap.label_bounds) > 0,
          "short text glyph clip was not exercised");
    for (int mode = 0; mode < 3; ++mode) {
        Fixture invalid;
        bool rejected = false;
        try {
            invalid.services.mount(Content{[&] {
                if (mode == 0) {
                    Empty(EmptyProps{}.image(static_cast<EmptyImage>(99)));
                }
                if (mode == 1) {
                    Result(ResultProps{}.status(static_cast<Status>(99)));
                }
                if (mode == 2) {
                    Result(ResultProps{}.status(static_cast<ResultException>(999)));
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && invalid.nodes.size() == 0 && invalid.services.interactions().size() == 0 &&
                  invalid.services.empty_result().empties().empty() &&
                  invalid.services.empty_result().results().empty(),
              "invalid props partially mounted presentation");
    }
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        retained_actions_and_editors(false);
        retained_actions_and_editors(true);
        typed_slots_and_statuses();
        constrained_geometry(false);
        constrained_geometry(true);
        short_text_clips_and_invalid_roll_back();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
