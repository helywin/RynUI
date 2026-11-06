#include "component/decoration_component.hpp"
#include "support/timeline_fixture.hpp"
#include <limits>
#include <thread>

using namespace timeline_test;

namespace {
std::shared_ptr<const ImagePixels> pixels() {
    const std::array<std::byte, 16> rgba{std::byte{255}, std::byte{0},   std::byte{0},   std::byte{255},
                                         std::byte{0},   std::byte{255}, std::byte{0},   std::byte{255},
                                         std::byte{0},   std::byte{0},   std::byte{255}, std::byte{255},
                                         std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    return ImagePixels::copy(2, 2, 8, rgba);
}

void watermark_text() {
    Fixture f;
    Signal<float> rotate{-22};
    Signal<String> text{String{u8"Native 水印\nConfidential"}};
    Signal<WatermarkGap> gap{WatermarkGap{30, 40}};
    Signal<WatermarkOffset> offset{WatermarkOffset{10, 20}};
    Signal<LogicalLength> height{dp(240)};
    int mounts{};
    f.services.mount(Content{[&] {
        Watermark(WatermarkProps{}.content(text).rotate(rotate).gap(gap).offset(offset).layout(
                      LayoutStyle{}.width(dp(400)).height(height)),
                  WatermarkContent{[&] {
                      ++mounts;
                      Input(InputProps{}.defaultValue(u8"retained draft"));
                  }});
    }});
    sync(f);
    const auto id = f.services.decorations().mounted().front();
    const auto original = f.services.decorations().snapshot(id);
    check(!original.texts.empty(), "Watermark has no real text views");
    const auto input = f.inputs.mounted_inputs().front().component;
    const auto tile = original.texts.front();
    const auto range = f.scene.primitive(tile).instances;
    check(range.count > 0, "Watermark text view has no raster glyphs");
    const auto glyph = f.scene.glyph_scene().instances().at(range.first);
    check(glyph.transform.angle_degrees == -22, "Watermark did not rotate actual glyphs");
    check(glyph.color[3] == .15F, "Watermark Theme alpha did not reach real glyph material");
    const auto p0 = graphics::glyph_vertex(glyph, {0, 0});
    const auto p1 = graphics::glyph_vertex(glyph, {1, 0});
    check(std::abs(p0.y - p1.y) > .1F, "Watermark rotation is only metadata");
    rotate.set(37);
    offset.set({35, 15});
    text.set(u8"Updated\n原生");
    sync(f);
    auto view = f.services.decorations().snapshot(id);
    check(view.texts.front() == tile, "Watermark property update discarded tile identity");
    const auto updated = f.scene.glyph_scene().instances().at(f.scene.primitive(tile).instances.first);
    check(updated.transform.angle_degrees == 37 && updated.position_size != glyph.position_size,
          "Watermark content/offset update did not reach shared glyph scene");
    ThemeConfig theme;
    theme.watermark.tokens.color = Color::rgba8(22, 119, 255, 90);
    theme.watermark.tokens.font_size = 20;
    f.services.components().theme_scope(id)->update(theme);
    sync(f);
    const auto themed = f.scene.glyph_scene().instances().at(f.scene.primitive(tile).instances.first);
    check(themed.color[2] == 1 && std::abs(themed.color[3] - 90.F / 255) < .0001F,
          "Reactive Watermark Theme color did not reach shared glyph view");
    const auto count = view.texts.size();
    height.set(dp(420));
    gap.set({10, 10});
    sync(f);
    view = f.services.decorations().snapshot(id);
    check(view.texts.size() > count && mounts == 1 && f.inputs.mounted_inputs().front().component == input,
          "Watermark resize/gap remounted retained children or failed to repeat");
    click(f, input);
    check(f.inputs.sessions().active().valid(), "Watermark decoration intercepted native input");
    check(!f.services.decorations().next_auxiliary_deadline(), "Static Watermark polls frames");
    f.services.dispose();
    check(f.scene.size() == 0 && f.services.images().live_count() == 0, "Watermark leaked text/image resources");
}

void watermark_image() {
    Fixture f;
    Signal<ImageSource> source{ImageSource{pixels()}};
    Signal<bool> visible{true};
    int loaded{};
    f.services.mount(Content{[&] {
        Watermark(WatermarkProps{}
                      .image(source)
                      .content(u8"Must not replace image")
                      .rotate(61)
                      .visible(visible)
                      .onLoad([&] { ++loaded; })
                      .layout(LayoutStyle{}.width(dp(400)).height(dp(250))),
                  WatermarkContent{[] { Text(u8"Real RGBA source"); }});
    }});
    sync(f);
    const auto id = f.services.decorations().mounted().front();
    auto view = f.services.decorations().snapshot(id);
    check(view.image_status == ImageStatus::Ready && !view.images.empty() && view.texts.empty() && loaded == 1,
          "Watermark image source was replaced by text or not loaded");
    const auto original = view.images.front();
    const auto* image = f.services.images().find(original);
    check(image && image->instance.angle_degrees == 61 && image->instance.opacity == .15F,
          "Watermark image lost rotation/theme opacity");
    sync(f);
    check(f.services.decorations().snapshot(id).images.front() == original && loaded == 1,
          "Watermark idle recreated resources or repeated onLoad");
    visible.set(false);
    sync(f);
    check(f.services.images().live_count() == 0, "Hidden Watermark retained visible image tiles");
    visible.set(true);
    source.set(ImageSource{});
    sync(f);
    view = f.services.decorations().snapshot(id);
    check(view.images.empty() && !view.texts.empty(), "Watermark did not restore text after image removal");
}

void watermark_async() {
    Fixture f;
    std::vector<ImageCompletion> pending;
    int cancelled{};
    int loaded{};
    int failed{};
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion done) {
        pending.push_back(std::move(done));
        return [&] {
            ++cancelled;
        };
    };
    Signal<ImageSource> source{ImageSource{loader}};
    Signal<bool> visible{true};
    f.services.mount(Content{[&] {
        Watermark(WatermarkProps{}
                      .image(source)
                      .visible(visible)
                      .onLoad([&] { ++loaded; })
                      .onError([&](const ImageError&) { ++failed; })
                      .layout(LayoutStyle{}.width(dp(300)).height(dp(180))),
                  WatermarkContent{[] { Text(u8"Pending"); }});
    }});
    sync(f);
    const auto id = f.services.decorations().mounted().front();
    check(pending.size() == 1, "Watermark loader not started");
    bool foreign_accepted{true};
    std::thread worker([&] { foreign_accepted = pending.front()({pixels(), {}}); });
    worker.join();
    check(!foreign_accepted, "Watermark accepted worker-thread mutation");
    visible.set(false);
    sync(f);
    check(cancelled == 1 && !pending.front()({pixels(), {}}), "Watermark hide did not invalidate async ticket");
    visible.set(true);
    sync(f);
    check(pending.size() == 2, "Watermark show failed to restart cancelled source");
    check(pending[1]({{}, {ImageErrorCode::loader_failure, "injected failure"}}), "Watermark failure rejected");
    sync(f);
    check(failed == 1 && loaded == 0 && f.services.decorations().snapshot(id).images.empty(),
          "Watermark image failure silently published stale content");
    source.set(ImageSource{std::make_shared<ImageLoader>(*loader)});
    sync(f);
    check(pending.size() == 3 && pending[2]({pixels(), {}}) && !pending[2]({pixels(), {}}),
          "Watermark retry or duplicate-result guard failed");
    sync(f);
    check(loaded == 1, "Watermark retry did not report load");
    source.set(ImageSource{std::make_shared<ImageLoader>(*loader)});
    sync(f);
    const auto stale = pending.back();
    const auto before_destroy = cancelled;
    f.services.destroy(id);
    check(!stale({pixels(), {}}) && f.services.images().live_count() == 0 && cancelled == before_destroy + 1,
          "Watermark unmount leaked loader or image state");
}

void watermark_lifetime() {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        Signal<ImageSource> source{ImageSource{}};
        const auto final_pixels = pixels();
        const ImageSource authoritative{final_pixels};
        runtime::ComponentId id;
        int errors{};
        auto loader = std::make_shared<ImageLoader>();
        loader->start = [&](ImageCompletion) -> ImageCancel {
            if (mode == 3) {
                throw std::runtime_error("loader failure");
            }
            return [&] {
                source.set(authoritative);
            };
        };
        source.set(mode == 0 || mode == 3 ? ImageSource{loader} : authoritative);
        f.services.mount(Content{[&] {
            Watermark(WatermarkProps{}
                          .image(source)
                          .onLoad([&] {
                              if (mode == 1) {
                                  f.services.destroy(id);
                              }
                              if (mode == 2) {
                                  throw std::runtime_error("load observer failure");
                              }
                          })
                          .onError([&](const ImageError&) { ++errors; })
                          .layout(LayoutStyle{}.width(dp(260)).height(dp(160))),
                      WatermarkContent{[] { Text(u8"Owner"); }});
        }});
        id = f.services.decorations().mounted().front();
        bool threw{};
        try {
            sync(f);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        if (mode == 0) {
            // Cancelling the pending old loader synchronously publishes a newer source.
            source.set(ImageSource{pixels()});
            sync(f);
            const auto view = f.services.decorations().snapshot(id);
            check(!view.images.empty() &&
                      f.services.images().find(view.images.front())->instance.pixels == final_pixels,
                  "Watermark source setter overwrote reentrant cancellation authority");
        } else if (mode == 1 || mode == 2) {
            check(threw == (mode == 2) && !f.services.components().contains(id) &&
                      f.services.images().live_count() == 0,
                  "Watermark load callback destruction/failure retained old resources");
        } else {
            check(!threw && errors == 1 && f.services.decorations().snapshot(id).image_status == ImageStatus::Error,
                  "Throwing Watermark loader did not become an observable source error");
        }
        f.services.dispose();
    }
}

void affix_top() {
    Fixture f;
    Signal<AffixTarget> target{AffixTarget{1, 0, 100, 400, 300}};
    Signal<float> top{12};
    std::vector<bool> changes;
    int mounts{};
    f.services.mount(Content{[&] {
        Affix(AffixProps{}
                  .target(target)
                  .offsetTop(top)
                  .onChange([&](bool fixed) { changes.push_back(fixed); })
                  .layout(LayoutStyle{}.width(dp(240)).height(dp(60))),
              AffixContent{[&] {
                  ++mounts;
                  Input(InputProps{}.defaultValue(u8"native draft"));
              }});
        // A following independent root advances the engine generation before
        // the retained translation phase positions the earlier Affix child.
        Text(u8"Later independent root");
    }});
    sync(f);
    const auto id = f.services.decorations().mounted().front();
    const auto original = f.services.decorations().snapshot(id);
    const auto input = f.inputs.mounted_inputs().front().component;
    check(original.affixed && original.bounds.y == 112 && changes == std::vector<bool>{true},
          "Affix top target/offset/onChange failed");
    click(f, input);
    check(f.inputs.sessions().active().valid(), "Affix real retained Input is not hittable");
    runtime::NodePropertyWriter writer(f.nodes, f.dirty);
    writer.set_translation(f.services.components().root(id), {0, -200});
    top.set(25);
    sync(f);
    auto view = f.services.decorations().snapshot(id);
    check(view.bounds.y == 125 && view.content == original.content && mounts == 1 &&
              f.inputs.mounted_inputs().front().component == input,
          "Affix scrolled/offset update rebuilt real child");
    const auto content_clip = f.services.component_clip(view.content, {600, 800}, {0, 0, 600, 800});
    check(content_clip.y >= 100 && content_clip.y + content_clip.height <= 400,
          "Affix window layer escaped the explicit target clip");
    auto next = target.get();
    next.identity = 2;
    next.y = 180;
    target.set(next);
    sync(f);
    check(f.services.decorations().snapshot(id).bounds.y == 205, "Affix target rebinding retained old viewport");
    next.attached = false;
    target.set(next);
    sync(f);
    view = f.services.decorations().snapshot(id);
    check(!view.affixed && view.bounds.y == bounds(f, id).y && !f.services.components().in_window_layer(view.layer) &&
              changes == std::vector<bool>({true, false}),
          "Affix detach failed to restore natural flow/layer");
    sync(f);
    check(changes.size() == 2 && !f.services.decorations().next_auxiliary_deadline(),
          "Affix idle emits changes or polls");
    // Rebinding must keep working after the following root was laid out.
    next.attached = true;
    target.set(next);
    sync(f);
    check(f.services.decorations().snapshot(id).content == original.content,
          "Affix failed retained placement after another root advanced the layout generation");
}

void affix_bottom() {
    Fixture f;
    Signal<bool> disabled{false};
    f.services.mount(Content{[&] {
        Affix(AffixProps{}
                  .target(AffixTarget{1, 0, 0, 400, 40})
                  .offsetBottom(5)
                  .disabled(disabled)
                  .layout(LayoutStyle{}.width(dp(240)).height(dp(60))),
              AffixContent{[] { Button(ButtonProps{}, [] { Text(u8"Bottom"); }); }});
    }});
    sync(f);
    const auto id = f.services.decorations().mounted().front();
    check(f.services.decorations().snapshot(id).bounds.y == -25 && f.services.decorations().snapshot(id).affixed,
          "Affix bottom did not pin measured child to target bottom");
    disabled.set(true);
    sync(f);
    const auto view = f.services.decorations().snapshot(id);
    check(!view.affixed && view.bounds.y == bounds(f, id).y &&
              !f.services.interactions().component_active(view.content),
          "Disabled Affix did not restore natural placement and block input");
    disabled.set(false);
    sync(f);
    check(f.services.decorations().snapshot(id).affixed, "Affix did not restore after enable");
}

void affix_lifetime() {
    for (int mode = 0; mode < 2; ++mode) {
        Fixture f;
        runtime::ComponentId id;
        bool armed{};
        Signal<AffixTarget> target{AffixTarget{1, 0, 0, 400, 400}};
        f.services.mount(Content{[&] {
            Affix(AffixProps{}
                      .target(target)
                      .onChange([&](bool) {
                          if (armed && mode == 0) {
                              f.services.destroy(id);
                          }
                          if (armed && mode == 1) {
                              throw std::runtime_error("callback failure");
                          }
                      })
                      .layout(LayoutStyle{}.width(dp(200)).height(dp(60))),
                  AffixContent{[] { Input(InputProps{}); }});
        }});
        sync(f);
        id = f.services.decorations().mounted().front();
        armed = true;
        target.set({1, 0, 100, 400, 400});
        bool threw{};
        try {
            sync(f);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw == (mode == 1) && !f.services.components().contains(id) && f.inputs.mounted_inputs().empty(),
              "Affix callback destruction/failure did not retire complete retained owner");
    }
}

void beam() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> shown{true};
    Signal<BorderBeamDirection> direction{BorderBeamDirection::Clockwise};
    f.services.mount(Content{[&] {
        BorderBeam(BorderBeamProps{}
                       .duration(Duration::seconds(4))
                       .direction(direction)
                       .visible(shown)
                       .layout(LayoutStyle{}.width(dp(260)).height(dp(100))),
                   BorderBeamContent{[] { Text(u8"Traveling beam"); }});
    }});
    sync(f);
    const auto id = f.services.decorations().mounted().front();
    check(f.services.decorations().snapshot(id).animating && f.services.next_frame_deadline(),
          "Beam has no live animation deadline");
    const auto effects = f.services.rounded_effects().packed_instances();
    const auto found = std::ranges::find_if(effects, [](const auto& effect) {
        return effect.geometry.kind == graphics::RoundedEffectKind::dashed_outline;
    });
    check(found != effects.end(), "Beam has no genuine rounded-perimeter effect");
    const auto initial = *found;
    f.services.tick_animations(animation::AnimationTime::microseconds(1000000));
    sync(f);
    check(std::abs(f.services.decorations().snapshot(id).phase - .25) < .001,
          "Beam duration did not map to quarter perimeter");
    const auto advanced = f.services.rounded_effects().packed_instances();
    const auto moved = std::ranges::find_if(advanced, [](const auto& effect) {
        return effect.geometry.kind == graphics::RoundedEffectKind::dashed_outline;
    });
    check(moved != advanced.end() && initial.geometry.path != moved->geometry.path &&
              initial.material == moved->material,
          "Beam flashes material instead of moving along path");
    direction.set(BorderBeamDirection::Counterclockwise);
    f.services.tick_animations(animation::AnimationTime::microseconds(1500000));
    sync(f);
    check(std::abs(f.services.decorations().snapshot(id).phase - .125) < .001, "Beam direction not reversed");
    shown.set(false);
    sync(f);
    check(!f.services.next_frame_deadline(), "Hidden Beam polls");
    shown.set(true);
    sync(f);
    f.services.set_window_active(false);
    check(!f.services.next_frame_deadline(), "Inactive Beam polls");
    const auto paused_phase = f.services.decorations().snapshot(id).phase;
    f.services.set_window_active(true);
    f.services.tick_animations(animation::AnimationTime::microseconds(9000000));
    check(f.services.decorations().snapshot(id).phase == paused_phase,
          "Beam replayed the paused interval when the host clock resumed");
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    sync(f);
    check(!f.services.next_frame_deadline(), "Reduced motion Beam polls");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    sync(f);
    runtime::NodePropertyWriter writer(f.nodes, f.dirty);
    writer.set_translation(f.services.components().root(id), {0, -2000});
    sync(f);
    check(!f.services.next_frame_deadline(), "Offscreen Beam polls");
    f.services.destroy(id);
    check(f.services.rounded_effects().packed_instances().empty() || !f.services.next_frame_deadline(),
          "Beam unmount leaked scheduling");
}

void theme_and_errors() {
    ThemeConfig config;
    config.watermark.tokens.color = Color::rgba8(200, 20, 40, 80);
    config.watermark.tokens.font_size = 23;
    config.border_beam.tokens.length = 63;
    config.border_beam.tokens.width = 3;
    config.affix.tokens.z_index = 42;
    const auto resolved = resolve_theme(config);
    check(resolved.watermark().font_size == 23 && resolved.border_beam().length == 63 && resolved.affix().z_index == 42,
          "Decoration Theme tokens not resolved");
    auto scope = theme_runtime::ThemeScope::create_default();
    int changed{};
    auto subscription = scope->capture([&](auto) { ++changed; },
                                       [&] {
                                           static_cast<void>(scope->watermark());
                                           static_cast<void>(scope->border_beam());
                                           static_cast<void>(scope->affix());
                                       });
    scope->update(config);
    check(changed == 1, "Decoration token overrides not reactive");
    const auto nested = theme_runtime::ThemeScope::create(scope, ThemeConfig{});
    check(nested->snapshot().watermark() == resolved.watermark() && nested->snapshot().affix() == resolved.affix(),
          "Nested Theme did not inherit decoration overrides");
    for (int mode = 0; mode < 4; ++mode) {
        bool failed{};
        try {
            auto invalid = config;
            if (mode == 0) {
                invalid.watermark.tokens.image_opacity = 2;
            }
            if (mode == 1) {
                invalid.watermark.tokens.font_size = std::numeric_limits<float>::infinity();
            }
            if (mode == 2) {
                invalid.border_beam.tokens.length = 0;
            }
            if (mode == 3) {
                invalid.border_beam.tokens.radius = -1;
            }
            static_cast<void>(resolve_theme(invalid));
        } catch (const std::invalid_argument&) {
            failed = true;
        }
        check(failed, "Decoration Theme silently accepted invalid geometry/material");
    }
    for (int mode = 0; mode < 3; ++mode) {
        Fixture f;
        bool failed{};
        try {
            f.services.mount(Content{[&] {
                if (mode == 0) {
                    Affix(AffixProps{}, AffixContent{[] {}});
                }
                if (mode == 1) {
                    BorderBeam(BorderBeamProps{}.duration(Duration{}), BorderBeamContent{[] {}});
                }
                if (mode == 2) {
                    Watermark(WatermarkProps{}.size(WatermarkSize{0, 1}), WatermarkContent{[] {}});
                }
            }});
        } catch (const std::invalid_argument&) {
            failed = true;
        }
        check(failed && f.services.decorations().mounted().empty(),
              "Invalid decoration declaration leaked retained owner");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "watermark_text";
        if (name == "watermark_text") {
            watermark_text();
        } else if (name == "watermark_image") {
            watermark_image();
        } else if (name == "watermark_async") {
            watermark_async();
        } else if (name == "watermark_lifetime") {
            watermark_lifetime();
        } else if (name == "affix_top") {
            affix_top();
        } else if (name == "affix_bottom") {
            affix_bottom();
        } else if (name == "affix_lifetime") {
            affix_lifetime();
        } else if (name == "beam") {
            beam();
        } else if (name == "theme") {
            theme_and_errors();
        } else {
            throw std::runtime_error("Unknown decoration test");
        }
        std::cout << name << " source contract checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
