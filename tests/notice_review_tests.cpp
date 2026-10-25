#include "component/notice_component.hpp"
#include "component/slider_component.hpp"
#include "component/selection_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
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

void sync(Fixture& f, float height = 200) {
    check(f.services.layout_and_synchronize({720, height}, {0, 0, 720, height}), "notice review layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

bool contains(runtime::Rect r, runtime::Point p) {
    return p.x >= r.x && p.y >= r.y && p.x < r.x + r.width && p.y < r.y + r.height;
}

// Sample the actual published draw commands. Rounded-effect coverage follows
// the shader's ancestor-clip rejection before evaluating the shape coverage.
float fill_coverage(Fixture& f, Color color, runtime::Point point) {
    float coverage = 0;
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
            if (command.kind == graphics::SceneDrawKind::quad) {
                const auto& q = f.services.surfaces().instances().at(i);
                if (q.color != std::array{color.red(), color.green(), color.blue(), color.alpha()}) {
                    continue;
                }
                const graphics::LogicalRoundedRect shape{
                    {q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]},
                    q.corner_radius};
                if (graphics::rounded_rect_signed_distance(point, shape) < 0) {
                    coverage = std::max(coverage, q.opacity * q.color[3]);
                }
            } else if (command.kind == graphics::SceneDrawKind::rounded_effect) {
                const auto& effect = f.services.rounded_effects().packed_instances()[i];
                if (effect.material.color != color || !effect.material.visible ||
                    (effect.geometry.ancestor_clip && !contains(effect.geometry.ancestor_clip->bounds, point))) {
                    continue;
                }
                coverage = std::max(coverage, graphics::rounded_effect_coverage(point, effect) *
                                                  effect.material.opacity * effect.material.color.alpha());
            }
        }
    }
    return coverage;
}

void fills() {
    Fixture f;
    NotificationRef notices;
    std::vector<detail::MountedButtonComponent> rows;
    f.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
    sync(f);
    check(notices.open(u8"overflow", NotificationProps{}.title(u8"Overflow").duration(0),
                       {.description = NotificationDescription{[&] {
                            for (int i = 0; i < 4; ++i) {
                                Button(ButtonProps{}
                                           .color(ButtonColor::Purple)
                                           .variant(ButtonVariant::Solid)
                                           .layout(LayoutStyle{}.height(dp(40)).flex_shrink(0)),
                                       [] { Text(u8"Real retained button"); });
                                rows.push_back(f.buttons.mounted_buttons().back());
                            }
                        }}}),
          "overflow notice failed");
    sync(f);
    const auto owner = f.services.notices().holders()[0];
    const auto id = f.services.notices().entries(owner)[0].id;
    const auto description = f.services.notices().snapshot(id).description;
    const auto clip = bounds(f, description);
    check(!f.services.components().in_window_layer_stack(rows[0].component), "fixture unexpectedly uses Modal stack");
    const auto color = f.buttons.snapshot(rows[0].component).presentation_background;
    const auto first = bounds(f, rows[0].component);
    check(fill_coverage(f, color, {first.x + first.width / 2, first.y + first.height / 2}) > .99F,
          "visible button fill fixture is empty");
    bool sampled = false;
    for (const auto& row : rows) {
        const auto r = bounds(f, row.component);
        check(r.height == 40, "overflow fixture shrank a genuine child height");
        const runtime::Point point{r.x + r.width / 2, std::max(clip.y + clip.height + 3, r.y + r.height / 2)};
        if (!contains(r, point) || point.y >= 199) {
            continue;
        }
        sampled = true;
        const auto hit = f.services.hit_test().hit_test(point);
        check(!hit || f.services.interactions().require(*hit).component != row.component,
              "overflow button hit escaped its description clip");
        const auto coverage = fill_coverage(f, color, point);
        std::cout << "overflow ordered fill coverage=" << coverage << '\n';
        check(coverage == 0, "ordinary notice layer published an unclipped Button fill outside its description");
    }
    check(sampled, "overflow fixture has no in-window sample outside the description");
}

void trim() {
    Fixture f;
    NotificationRef notices;
    Signal<std::size_t> maximum{std::size_t{3}};
    std::vector<String> closed;
    f.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices).maxCount(maximum)); }});
    const auto owner = f.services.notices().holders()[0];
    check(notices.open(u8"A", NotificationProps{}.duration(0).onClose([&] {
        closed.push_back(u8"A");
        check(notices.close(u8"C"), "A callback could not close C");
    })),
          "A failed");
    check(notices.open(u8"B", NotificationProps{}.duration(0).onClose([&] { closed.push_back(u8"B"); })), "B failed");
    check(notices.open(u8"C", NotificationProps{}.duration(0).onClose([&] { closed.push_back(u8"C"); })), "C failed");
    const auto b = f.services.notices().entries(owner)[1].id;
    maximum.set(1);
    const auto remaining = f.services.notices().entries(owner);
    std::cout << "trim remaining=" << remaining.size() << ", close callbacks=" << closed.size() << '\n';
    check(remaining.size() == 1 && remaining[0].key == String{u8"B"} && remaining[0].id == b &&
              closed == std::vector<String>{u8"A", u8"C"},
          "trim continued deleting after callback had already satisfied maxCount");
}

void content_ranges() {
    Fixture f;
    NotificationRef notices;
    f.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
    sync(f);
    check(notices.open(u8"dots", NotificationProps{}.title(u8"Slider dots").duration(0),
                       {.description = NotificationDescription{[] {
                            for (int i = 0; i < 4; ++i) {
                                Slider(SliderProps{}
                                           .limits(SliderLimits{0, 100, 25})
                                           .dots(true)
                                           .hint(SliderHintOptions{SliderHintMode::Hidden})
                                           .layout(LayoutStyle{}.height(dp(40)).flex_shrink(0)));
                            }
                        }}}),
          "slider notice failed");
    sync(f);
    const auto owner = f.services.notices().holders()[0];
    const auto id = f.services.notices().entries(owner)[0].id;
    const auto clip = bounds(f, f.services.notices().snapshot(id).description);
    const auto slider = f.services.slider().mounted()[0].component;
    const auto& token = f.services.components().theme_scope(slider)->snapshot().slider();
    bool inside = false;
    bool outside = false;
    for (const auto& q : f.services.surfaces().instances().instances()) {
        if (q.bounds[2] != token.metrics.dot_size || q.bounds[3] != token.metrics.dot_size ||
            q.color != std::array{token.colors.dot_border.red(), token.colors.dot_border.green(),
                                  token.colors.dot_border.blue(), token.colors.dot_border.alpha()}) {
            continue;
        }
        const runtime::Point point{q.bounds[0] + q.bounds[2] / 2, q.bounds[1] + q.bounds[3] / 2};
        const auto coverage = fill_coverage(f, token.colors.dot_border, point);
        if (contains(clip, point)) {
            inside = true;
            check(coverage > .9F, "visible Slider dot content-range fill disappeared");
        } else if (point.y > clip.y + clip.height + 2 && point.y < 199) {
            outside = true;
            std::cout << "overflow ordered Slider dot coverage=" << coverage << '\n';
            check(coverage == 0, "ordinary notice layer published an unclipped Slider content-range fill");
        }
    }
    check(inside && outside, "Slider content-range fixture did not sample both sides of the description clip");
}

void trim_callbacks() {
    for (int action = 0; action < 5; ++action) {
        Fixture f;
        NotificationRef notices;
        Signal<std::size_t> maximum{std::size_t{3}};
        runtime::ComponentId parent;
        runtime::ComponentId owner;
        bool retire_on_read = false;
        std::array<int, 3> closed{};
        f.services.mount(Content{[&] {
            auto& build = runtime::require_component_build_context();
            parent = build.mount_component<int>(0);
            f.layout.set_layout(build.root(parent), layout::BoxLayout{});
            build.mount_slot(parent, Content{[&] {
                                 NotificationHolder(NotificationHolderProps{}.ref(notices).maxCount(bind([&] {
                                     const auto value = maximum.get();
                                     if (retire_on_read) {
                                         retire_on_read = false;
                                         f.services.destroy(owner);
                                         f.services.append_slot(
                                             parent, Content{[&] {
                                                 NotificationHolder(
                                                     NotificationHolderProps{}.ref(notices).maxCount(maximum));
                                             }});
                                         check(notices.open(u8"new", NotificationProps{}.duration(0)),
                                               "authority getter rebind failed");
                                     }
                                     return value;
                                 })));
                             }});
        }});
        owner = f.services.notices().holders()[0];
        check(notices.open(u8"A", NotificationProps{}.duration(0).onClose([&] {
            ++closed[0];
            if (action == 0) {
                maximum.set(3);
            } else if (action == 1) {
                check(notices.clear(), "trim callback clear failed");
            } else if (action == 2) {
                check(notices.open(u8"A", NotificationProps{}.title(u8"new generation").duration(0)),
                      "trim callback same-key reopen failed");
            } else if (action == 3) {
                f.services.destroy(owner);
                f.services.append_slot(parent, Content{[&] {
                                           NotificationHolder(NotificationHolderProps{}.ref(notices).maxCount(maximum));
                                       }});
                check(notices.open(u8"new", NotificationProps{}.duration(0)), "trim callback Ref rebind failed");
            } else {
                retire_on_read = true;
            }
        })),
              "callback A failed");
        check(notices.open(u8"B", NotificationProps{}.duration(0).onClose([&] { ++closed[1]; })), "callback B failed");
        check(notices.open(u8"C", NotificationProps{}.duration(0).onClose([&] { ++closed[2]; })), "callback C failed");
        const auto old_a = f.services.notices().entries(owner)[0].id;
        maximum.set(1);
        const auto current_owner = f.services.notices().holders()[0];
        const auto remaining = f.services.notices().entries(current_owner);
        if (action == 0) {
            std::cout << "raised-limit remaining=" << remaining.size() << '\n';
            check(remaining.size() == 2 && remaining[0].key == String{u8"B"} && remaining[1].key == String{u8"C"} &&
                      closed == std::array{1, 0, 0},
                  "trim used a deferred stale maxCount after callback raised authority");
        } else if (action == 1) {
            check(remaining.empty() && closed == std::array{1, 1, 1}, "trim callback clear repeated a close callback");
        } else if (action == 2) {
            check(remaining.size() == 1 && remaining[0].key == String{u8"A"} && remaining[0].id != old_a &&
                      closed == std::array{1, 1, 1},
                  "trim retired a same-key replacement or repeated callbacks");
        } else {
            check(current_owner != owner && remaining.size() == 1 && remaining[0].key == String{u8"new"} &&
                      closed == std::array{1, 0, 0},
                  "old trim continued against a rebound holder generation");
        }
    }
}

void clip_contract() {
    Fixture f;
    detail::SelectionComponentHost selections{f.services};
    NotificationRef notices;
    InputRef input;
    CheckboxRef checkbox;
    runtime::ComponentId button;
    f.services.mount(Content{[&] {
        Button(ButtonProps{}.layout(LayoutStyle{}.width(dp(100))), [] { Text(u8"Normal flow"); });
        NotificationHolder(NotificationHolderProps{}.ref(notices));
    }});
    sync(f, 600);
    const auto normal = f.buttons.mounted_buttons()[0];
    check(notices.open(u8"focus", NotificationProps{}.title(u8"Focus decoration").duration(0),
                       {.description = NotificationDescription{[&] {
                            Button(
                                ButtonProps{}.layout(
                                    LayoutStyle{}.width(dp(100)).height(dp(32)).margin_left(dp(8)).margin_top(dp(8))),
                                [] { Text(u8"Focus"); });
                            button = f.buttons.mounted_buttons().back().component;
                            Input(InputProps{}
                                      .ref(input)
                                      .variant(InputVariant::Borderless)
                                      .layout(LayoutStyle{}.width(dp(100)).height(dp(32)).margin_left(dp(8))));
                            Checkbox(CheckboxProps{}.ref(checkbox).layout(
                                LayoutStyle{}.width(dp(100)).height(dp(24)).margin_left(dp(8))));
                        }}}),
          "focus notice failed");
    sync(f, 600);
    const auto owner = f.services.notices().holders()[0];
    const auto id = f.services.notices().entries(owner)[0].id;
    const auto clip = bounds(f, f.services.notices().snapshot(id).description);
    const auto raw_drawn = [&](component::RetainedSurfaceId surface) {
        const auto range = f.services.surfaces().visual_range(surface);
        return std::ranges::any_of(f.services.scene_composer().ordered_scene().commands(), [&](const auto& command) {
            return command.kind == graphics::SceneDrawKind::quad &&
                   command.first_instance < range.first + range.count &&
                   command.first_instance + command.instance_count > range.first;
        });
    };
    check(raw_drawn(normal.scene), "notice clipping converted an unrelated normal-flow surface");
    const auto mounted =
        std::ranges::find(f.buttons.mounted_buttons(), button, &detail::MountedButtonComponent::component);
    check(mounted != f.buttons.mounted_buttons().end(), "focus Button missing");
    const auto button_surface = mounted->scene;
    const auto button_hit = mounted->interaction;
    check(!raw_drawn(button_surface), "notice Button retained an unclipped raw fill draw");
    const auto verify_focus = [&](const graphics::RoundedEffectInstance& effect) {
        const auto shape = effect.geometry.shape.rect;
        const float distance = effect.geometry.outline_offset + effect.geometry.outline_width / 2;
        const runtime::Point point{shape.x + shape.width + distance, shape.y + shape.height / 2};
        check(effect.material.opacity > 0 && effect.geometry.ancestor_clip &&
                  effect.geometry.ancestor_clip->bounds == clip && contains(clip, point) &&
                  graphics::rounded_effect_coverage(point, effect) > .5F,
              "child focus decoration lost its valid outward coverage or external clip");
    };
    check(f.services.focus().request_focus(button_hit, FocusModality::keyboard), "Button focus failed");
    sync(f, 600);
    verify_focus(f.services.surfaces().focus_effect(button_surface));
    check(input.focus(), "Input focus failed");
    sync(f, 600);
    const auto input_bounds = bounds(f, f.inputs.mounted_inputs()[0].component);
    bool input_outline = false;
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        if (effect.geometry.kind == graphics::RoundedEffectKind::outline && effect.material.opacity > 0 &&
            effect.geometry.shape.rect.x == input_bounds.x + 1 && effect.geometry.shape.rect.y == input_bounds.y + 1) {
            verify_focus(effect);
            input_outline = true;
        }
    }
    check(input_outline, "borderless Input focus outline was not published");
    check(checkbox.focus(), "selection focus failed");
    sync(f, 600);
    verify_focus(f.services.surfaces().focus_effect(selections.mounted()[0].surface));
    for (const auto child : {button, f.inputs.mounted_inputs()[0].component, selections.mounted()[0].component}) {
        const auto r = bounds(f, child);
        const auto hit = f.services.hit_test().hit_test({r.x + 2, r.y + r.height / 2});
        check(hit && f.services.interactions().require(*hit).component == child,
              "clipped retained child lost its valid content hit");
    }
    check(notices.close(u8"focus"), "focus notice close failed");
    sync(f, 600);
    check(raw_drawn(normal.scene) && !input.bound() && !checkbox.bound(),
          "notice cleanup changed normal content or retained refs");
}

void anchor() {
    Fixture f;
    NotificationRef notices;
    Signal<NotificationPlacement> placement{NotificationPlacement::TopRight};
    Signal<bool> popup_open{true};
    Signal<bool> spinning{false};
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true).footer(ModalFooterMode::None), ModalContent{[&] {
                  Spin(SpinProps{}.spinning(spinning),
                       SpinContent{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
              }});
    }});
    sync(f, 600);
    const auto select = [&] {
        Select(
            SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).open(popup_open).layout(LayoutStyle{}.width(dp(120))));
    };
    check(notices.open(u8"anchor", NotificationProps{}.title(u8"Open in first frame").placement(placement).duration(0),
                       {.description = NotificationDescription{select}, .actions = NotificationActions{select}}),
          "anchor notice failed");
    const auto verify = [&](float width, float height, const char* stage) {
        // Exactly one synchronize after each open/placement/viewport change.
        check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}),
              "single-frame anchor layout failed");
        check(f.services.select().mounted().size() == 2, "description/action Selects were remounted");
        for (auto id : f.services.select().mounted()) {
            const auto popup = f.services.select().snapshot(id);
            const auto r = bounds(f, popup.selector);
            const auto gap = f.services.components().theme_scope(id)->snapshot().select().metrics.gap;
            check(popup.visible && std::abs(popup.popup.x - r.x) < .01F &&
                      std::abs(popup.popup.width - r.width) < .01F &&
                      std::abs(popup.above ? popup.popup.y + popup.popup.height + gap - r.y
                                           : popup.popup.y - r.y - r.height - gap) < .01F,
                  "open Select used an old notice anchor after one synchronize");
            std::cout << stage << " selector=(" << r.x << ',' << r.y << ") popup=(" << popup.popup.x << ','
                      << popup.popup.y << ")\n";
        }
    };
    verify(720, 600, "first");
    const auto ids =
        std::vector<runtime::ComponentId>{f.services.select().mounted().begin(), f.services.select().mounted().end()};
    const auto first = bounds(f, f.services.select().snapshot(ids[0]).selector);
    placement.set(NotificationPlacement::BottomLeft);
    verify(720, 600, "placement");
    const auto moved = bounds(f, f.services.select().snapshot(ids[0]).selector);
    check(first.x != moved.x && first.y != moved.y, "placement fixture did not move the notice anchor");
    verify(900, 500, "viewport");
    check(bounds(f, f.services.select().snapshot(ids[0]).selector).y != moved.y,
          "viewport fixture did not move the anchor");
    spinning.set(true);
    sync(f, 500);
    for (auto id : ids) {
        check(!f.services.select().snapshot(id).visible, "Spin boundary did not suppress child popup");
    }
    spinning.set(false);
    popup_open.set(false);
    popup_open.set(true);
    verify(900, 500, "restored");
    check(std::ranges::equal(ids, f.services.select().mounted()), "anchor changes or suppression remounted Selects");
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const std::string_view name = argc > 1 ? argv[1] : "fills";
        if (name == "fills") {
            fills();
        } else if (name == "trim") {
            trim();
        } else if (name == "content_ranges") {
            content_ranges();
        } else if (name == "trim_callbacks") {
            trim_callbacks();
        } else if (name == "clip_contract") {
            clip_contract();
        } else if (name == "anchor") {
            anchor();
        } else {
            throw std::runtime_error("unknown notice review case");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
