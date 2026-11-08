#pragma once

#include "component/drawer_model.hpp"
#include "component/input_component.hpp"
#include "component/modal_component.hpp"
#include "gallery_pages_host.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace rynui::example {
// Hidden native fixture: every theme case exercises ALL physical placements.
// Its clock is controlled by the real Gallery frame loop, never wall sleeps.
inline void accept_drawer_motion(std::size_t case_index, GalleryPagesHost& pages, ryn::detail::ButtonComponentHost& app,
                                 ryn::detail::InputComponentHost& inputs, ryn::detail::SdlSceneRenderer& renderer,
                                 ryn::runtime::Size viewport, float scale, ryn::animation::AnimationTime origin,
                                 const std::filesystem::path& output,
                                 const std::function<void(ryn::animation::AnimationTime)>& render_at,
                                 const std::function<bool(ryn::animation::AnimationTime)>& idle_at,
                                 const std::function<ryn::runtime::Size(float)>& device_scale) {
    using namespace ryn;
    const auto check = [](bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    };
    const auto near = [](float a, float b) {
        return std::abs(a - b) < .03F;
    };
    Signal<bool> open{false};
    Signal<bool> child_open{false};
    Signal<DrawerPlacement> placement{DrawerPlacement::Right};
    Signal<float> width{std::min(300.0F, viewport.width * .3F)};
    const auto height = std::min(260.0F, viewport.height * .3F);
    Signal<bool> rtl{false};
    InputRef retained;
    int mounts{};
    ThemeConfig palette;
    palette.seed.motion = true;
    palette.seed.motion_base = Duration::milliseconds(0);
    palette.seed.motion_unit = Duration::milliseconds(100);
    palette.drawer.tokens.background = Color::rgba8(41, 119, 173);
    palette.drawer.tokens.header_background = Color::rgba8(190, 65, 40);
    palette.drawer.tokens.footer_background = Color::rgba8(65, 155, 80);
    Signal<ThemeConfig> theme{palette};
    auto& services = app.services();
    runtime::ComponentId fixture;

    struct Cleanup final {
        ryn::detail::WindowComponentServices& services;
        runtime::ComponentId& id;

        ~Cleanup() {
            services.retire_failed_component(id);
        }
    } cleanup{services, fixture};

    const auto first = services.modal().mounted().size();
    services.append_slot(
        pages.snapshot().page, Content{[&] {
            auto& build = runtime::require_component_build_context();
            fixture = build.mount_component<int>(0);
            const auto node = build.root(fixture);
            services.nodes().require(node).out_of_flow = true;
            services.layout().set_layout(node, layout::BoxLayout{});
            build.on_resource_cleanup(fixture, [&, node] { services.layout().remove_layout(node); });
            build.mount_slot(
                fixture, Content{[&] {
                    Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                              Drawer(DrawerProps{}
                                         .open(open)
                                         .width(width)
                                         .height(height)
                                         .placement(placement)
                                         .rtl(rtl)
                                         .title(u8"Drawer motion readback")
                                         .resizable(DrawerResize{true}),
                                     DrawerContent{[&] {
                                         ++mounts;
                                         Input(InputProps{}.ref(retained).defaultValue(u8"native retained draft"));
                                         for (int row = 0; row < 40; ++row) {
                                             Text(u8"Clipped retained body");
                                         }
                                         Drawer(DrawerProps{}
                                                    .open(child_open)
                                                    .width(std::min(180.0F, width.get() * .65F))
                                                    .mask(DrawerMask{false, false})
                                                    .push(DrawerPush{true, 80}),
                                                DrawerContent{[] { Text(u8"Nested push"); }});
                                     }},
                                     {.footer = DrawerFooter{[] { Text(u8"Retained footer"); }}});
                          }});
                }});
        }});

    const auto parent = services.modal().mounted()[first];
    const auto child = services.modal().mounted()[first + 1];
    auto time = origin;
    const auto advance = [&](std::int64_t us) {
        time = time + animation::AnimationDuration::microseconds(us);
        render_at(time);
    };
    const auto pixel = [&](const ryn::detail::SceneRgbaReadback& image, runtime::Point point) {
        const auto x = static_cast<std::uint32_t>(std::floor(point.x * scale));
        const auto y = static_cast<std::uint32_t>(std::floor(point.y * scale));
        check(x < image.width && y < image.height, "Drawer readback probe escaped the physical target");
        std::array<float, 4> result;
        for (std::size_t c = 0; c < result.size(); ++c) {
            result[c] = std::to_integer<unsigned char>(image.pixels[y * image.row_pitch + x * 4 + c]) / 255.0F;
        }
        return result;
    };
    const auto read = [&] {
        ryn::detail::SceneRgbaReadback result;
        check(renderer.read_frame_rgba(result), "Drawer native RGBA readback failed");
        return result;
    };
    const auto color = [&](const ryn::detail::SceneRgbaReadback& image, runtime::Point point, Color expected) {
        const auto actual = pixel(image, point);
        if (std::abs(actual[0] - expected.red()) >= .025F || std::abs(actual[1] - expected.green()) >= .025F ||
            std::abs(actual[2] - expected.blue()) >= .025F) {
            std::cerr << "Drawer color probe x=" << point.x << " y=" << point.y << " actual=" << actual[0] << ','
                      << actual[1] << ',' << actual[2] << " expected=" << expected.red() << ',' << expected.green()
                      << ',' << expected.blue() << '\n';
            check(renderer.save_frame_bmp(output / "drawer-color-failure.bmp"), "Drawer failure capture failed");
            throw std::runtime_error("Drawer native panel/header/footer moved incorrectly or faded");
        }
    };
    std::ofstream numeric(output / "drawer-motion.tsv", std::ios::app);
    std::ofstream captures(output / "captures.tsv", std::ios::app);
    numeric.exceptions(std::ios::failbit | std::ios::badbit);
    captures.exceptions(std::ios::failbit | std::ios::badbit);
    render_at(time);
    const auto retained_editor = [&] {
        for (const auto& item : inputs.mounted_inputs()) {
            for (auto owner = std::optional{item.component}; owner; owner = app.components().parent(*owner)) {
                if (*owner == parent) {
                    return item;
                }
            }
        }
        throw std::runtime_error("Drawer native retained editor missing");
    }();
    const auto capture = [&](std::string_view edge, std::string_view phase, std::size_t index) {
        const auto name =
            std::to_string(case_index) + "-drawer-" + std::string(edge) + "-" + std::string(phase) + ".bmp";
        check(renderer.save_frame_bmp(output / name), "Drawer native frame save failed");
        captures << case_index << "\tant.component.drawer\tdrawer.motion." << edge << '\t' << phase << '\t' << index
                 << '\t' << scale << "\t0\t0\t" << std::lround(viewport.width * scale) << '\t'
                 << std::lround(viewport.height * scale) << '\t' << name << '\n';
    };
    const auto verify = [&](DrawerPlacement edge, float p, const ryn::detail::SceneRgbaReadback& background,
                            bool inspect_mask) {
        const auto s = services.modal().snapshot(parent);
        auto expected =
            ryn::detail::drawer_bounds({edge, width.get(), height}, {0, 0, viewport.width, viewport.height});
        switch (edge) {
        case DrawerPlacement::Left:
            expected.x -= (1 - p) * expected.width;
            break;
        case DrawerPlacement::Right:
            expected.x += (1 - p) * expected.width;
            break;
        case DrawerPlacement::Top:
            expected.y -= (1 - p) * expected.height;
            break;
        case DrawerPlacement::Bottom:
            expected.y += (1 - p) * expected.height;
            break;
        }
        check(near(s.progress, p) &&
                  (p == 0 && !s.paint_visible || near(s.bounds.x, expected.x) && near(s.bounds.y, expected.y)),
              "Drawer native clock/placement projection disagrees with CPU bounds");
        const auto image = read();
        const auto& token = app.components().theme_scope(parent)->snapshot().drawer();
        if (p > 0 && s.paint_visible) {
            // Sample padding clear of both translated text and the resize grip.
            const float padding_x = s.bounds.x < 0 ? s.bounds.x + s.bounds.width - 12 : s.bounds.x + 12;
            const auto point =
                runtime::Point{std::clamp(padding_x, 4.0F, viewport.width - 4),
                               std::clamp(s.body_bounds.y + s.body_bounds.height / 2, 8.0F, viewport.height - 8)};
            if (point.x > s.bounds.x + 2 && point.x < s.bounds.x + s.bounds.width - 2 &&
                point.y > s.body_bounds.y + 2 && point.y < s.body_bounds.y + s.body_bounds.height - 2) {
                color(image, point, token.colors.background);
            }
            for (auto region : {s.title, s.footer}) {
                const auto& node = app.nodes().require(app.components().root(region));
                const runtime::Point probe{s.bounds.x + 4, node.bounds.y + node.bounds.height / 2};
                if (probe.x > 2 && probe.y > 2 && probe.x < viewport.width - 2 && probe.y < viewport.height - 2) {
                    color(image, probe,
                          region == s.title ? token.colors.header_background : token.colors.footer_background);
                }
            }
        }
        if (inspect_mask) {
            const runtime::Point probe{viewport.width / 2, viewport.height / 2};
            const auto base = pixel(background, probe);
            const auto actual = pixel(image, probe);
            const auto tint = token.colors.mask;
            const std::array src{tint.red(), tint.green(), tint.blue()};
            const auto alpha = tint.alpha() * p;
            for (std::size_t c = 0; c < 3; ++c) {
                check(std::abs(actual[c] - (src[c] * alpha + base[c] * (1 - alpha))) < .025F,
                      "Drawer native mask did not source-over exactly once");
            }
            if (p > 0 && s.paint_visible) {
                auto shadow = runtime::Point{viewport.width / 2, viewport.height / 2};
                switch (edge) {
                case DrawerPlacement::Left:
                    shadow.x = s.bounds.x + s.bounds.width + 6;
                    break;
                case DrawerPlacement::Right:
                    shadow.x = s.bounds.x - 6;
                    break;
                case DrawerPlacement::Top:
                    shadow.y = s.bounds.y + s.bounds.height + 6;
                    break;
                case DrawerPlacement::Bottom:
                    shadow.y = s.bounds.y - 6;
                    break;
                }
                if (shadow.x > 1 && shadow.y > 1 && shadow.x < viewport.width - 1 && shadow.y < viewport.height - 1) {
                    const auto under = pixel(background, shadow);
                    const auto drawn = pixel(image, shadow);
                    const auto expected_sum =
                        (under[0] + under[1] + under[2]) * (1 - alpha) + (src[0] + src[1] + src[2]) * alpha;
                    if (expected_sum > .3F) {
                        check(drawn[0] + drawn[1] + drawn[2] < expected_sum - .005F,
                              "Drawer native directional shadow missing at inward panel edge");
                    }
                }
            }
        }
        check(mounts == 1 && inputs.editors().find(retained_editor.editor), "Drawer native motion remounted editor");
    };
    constexpr std::array edges{DrawerPlacement::Left, DrawerPlacement::Right, DrawerPlacement::Top,
                               DrawerPlacement::Bottom};
    constexpr std::array names{"left", "right", "top", "bottom"};
    const auto half = animation::ant_easing(animation::AntEasingPreset::ease_in_out).sample(.5F);
    for (std::size_t e = 0; e < edges.size(); ++e) {
        placement.set(edges[e]);
        render_at(time);
        const auto background = read();
        open.set(true);
        for (std::size_t phase = 0; phase < 3; ++phase) {
            advance(phase == 0 ? 0 : 150000);
            const auto p = phase == 0 ? 0.0F : phase == 1 ? half : 1.0F;
            verify(edges[e], p, background, true);
            const auto label = phase == 0 ? "open-t0" : phase == 1 ? "open-half" : "open-end";
            capture(names[e], label, phase);
            numeric << case_index << '\t' << names[e] << '\t' << label << '\t' << p << '\t' << scale << "\tpass\n";
        }
        render_at(time); // Exactly one completion dispatch pass.
        check(retained.focus({InputFocusCursor::All}), "Drawer native editor focus failed");
        const auto selection = inputs.editors().require(retained_editor.editor).selection();
        const auto session = inputs.sessions().active();
        open.set(false);
        check(!retained.focus() && !bool(inputs.dispatch(input::TextCommitted{String{u8"stale"}, session})),
              "Drawer native close did not immediately stop editor input");
        for (std::size_t phase = 0; phase < 3; ++phase) {
            advance(phase == 0 ? 0 : 150000);
            const auto p = phase == 0 ? 1.0F : phase == 1 ? 1 - half : 0.0F;
            verify(edges[e], p, background, true);
            const auto label = phase == 0 ? "close-t0" : phase == 1 ? "close-half" : "close-end";
            capture(names[e], label, phase + 3);
            numeric << case_index << '\t' << names[e] << '\t' << label << '\t' << p << '\t' << scale << "\tpass\n";
        }
        render_at(time);
        check(inputs.editors().require(retained_editor.editor).selection() == selection,
              "Drawer native motion lost retained selection");
    }
    placement.set(DrawerPlacement::Right);
    open.set(true);
    advance(300000);
    render_at(time);
    const auto original = services.modal().snapshot(parent).bounds;
    child_open.set(true);
    advance(150000);
    const auto pushed = services.modal().snapshot(parent);
    const auto child_mid = services.modal().snapshot(child);
    check(near(pushed.bounds.x, original.x - 80 * child_mid.progress), "Drawer native nested midpoint push failed");
    const auto push_frame = read();
    color(push_frame, {pushed.bounds.x + 8, pushed.body_bounds.y + pushed.body_bounds.height / 2},
          app.components().theme_scope(parent)->snapshot().drawer().colors.background);
    capture("right-nested", "open-half", 6);
    child_open.set(false);
    advance(30000);
    const auto reversed = services.modal().snapshot(parent).bounds;
    child_open.set(true);
    render_at(time);
    check(near(services.modal().snapshot(parent).bounds.x, reversed.x), "Drawer native nested reversal jumped");
    advance(300000);
    render_at(time);
    child_open.set(false);
    advance(150000);
    check(services.modal().snapshot(parent).bounds.x < original.x, "Drawer native exiting child lost push");
    capture("right-nested", "close-half", 7);
    advance(150000);
    render_at(time);
    open.set(false);
    advance(300000);
    render_at(time);
    // Exercise real GPU upload/readback at two explicit offscreen device scales.
    // This does not claim that the OS moved the hidden window between monitors.
    for (const auto requested : {1.25F, 1.5F}) {
        const auto background = read();
        open.set(true);
        advance(150000);
        viewport = device_scale(requested);
        scale = requested;
        width.set(std::min(280.0F, viewport.width * .3F));
        rtl.set(true);
        palette.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
        theme.set(palette);
        render_at(time);
        verify(DrawerPlacement::Right, half, background, false);
        capture("right-scaled", requested == 1.25F ? "scale-125" : "scale-150", 8);
        open.set(false);
        advance(300000);
        render_at(time);
    }
    services.destroy(fixture);
    render_at(time);
    render_at(time);
    check(!services.modal().next_auxiliary_deadline(), "Drawer native cleanup retained a motion deadline");
    check(idle_at(time + animation::AnimationDuration::milliseconds(1000)) &&
              idle_at(time + animation::AnimationDuration::milliseconds(2000)),
          "Drawer native cleanup continued on-demand submission/upload");
    numeric << case_index << "\tnested/reversal/scales/cleanup\t" << renderer.gpu_driver() << '\t'
            << renderer.shader_format() << "\tpass\n";
}
} // namespace rynui::example
