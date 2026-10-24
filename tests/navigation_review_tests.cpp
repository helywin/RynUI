#include "component/breadcrumb_component.hpp"
#include "component/steps_component.hpp"
#include "support/navigation_fixture.hpp"
#include <array>
#include <iostream>
#include <string_view>

namespace {
using namespace ryn_test::navigation;

runtime::Point center(Fixture& f, runtime::ComponentId id) {
    const auto b = bounds(f, id);
    return {b.x + b.width / 2, b.y + b.height / 2};
}

void child_input_eligibility(bool steps, bool callback) {
    Fixture f;
    Signal<bool> disabled{false};
    Signal<bool> shown{true};
    Signal<bool> tail{false};
    Signal<bool> spinning{false};
    Signal<bool> open{false};
    int child_clicks = 0;
    int requests = 0;
    int slot_runs = 0;
    const auto children = [&] {
        ++slot_runs;
        Flex(FlexProps{}.gap(dp(8)), FlexContent{[&] {
                 Button(ButtonProps{}.onClick([&] { ++child_clicks; }), ButtonContent{[] { Text(u8"Child"); }});
                 Input(InputProps{}.layout(LayoutStyle{}.width(dp(100))));
             }});
    };
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                 if (steps) {
                     StepsProps props;
                     if (callback) {
                         props.onChange([&](std::size_t) { ++requests; });
                     }
                     Steps(props,
                           {StepsItem{u8"only", u8"Display step"}.disabled(disabled).visible(shown).richDescription(
                               StepsDescription{children})});
                 } else {
                     BreadcrumbProps props;
                     if (callback) {
                         props.onActivate([&](BreadcrumbActivation) { ++requests; });
                     }
                     Breadcrumb(props,
                                {BreadcrumbItem{u8"rich", u8"Current"}.disabled(disabled).visible(shown).richTitle(
                                     BreadcrumbTitle{children}),
                                 BreadcrumbItem{u8"tail", u8"Tail"}.visible(tail)});
                 }
             }});
        Modal(ModalProps{}.open(open).footer(ModalFooterMode::None), ModalContent{[] { Text(u8"Modal"); }});
    }});
    sync(f);
    const auto owner = steps ? f.services.steps().mounted()[0] : f.services.breadcrumb().mounted()[0];
    const auto item =
        steps ? f.services.steps().snapshot(owner).items[0] : f.services.breadcrumb().snapshot(owner).items[0];
    const auto button = f.buttons.mounted_buttons()[0];
    const auto input = f.inputs.mounted_inputs()[0];
    const auto input_hit = [&](bool expected) {
        const auto hit = f.services.hit_test().hit_test(center(f, input.component));
        std::cout << "child hit steps=" << steps << " callback=" << callback << " expected=" << expected
                  << " actual=" << (hit == input.interaction) << '\n';
        check((hit == input.interaction) == expected, "native child Input excluded from actual pointer HitTest");
        check((f.services.hit_test().hit_test(center(f, button.component)) == button.interaction) == expected,
              "native child Button pointer eligibility wrong");
    };
    const auto exercise = [&] {
        input_hit(true);
        const auto before = f.inputs.editors().require(input.editor).value().size();
        click(f, input.component);
        check(f.services.focus().state().focused == input.interaction && f.inputs.sessions().active().valid(),
              "non-navigation wrapper prevented native Input focus/session");
        const auto stamp = f.inputs.sessions().active();
        check(static_cast<bool>(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})),
              "child preedit rejected");
        key(f, Key::enter);
        check(f.inputs.editors().require(input.editor).composition().active && requests == 0,
              "wrapper stole composing Input Enter");
        check(static_cast<bool>(f.inputs.dispatch(TextCommitted{String{u8"你"}, stamp})),
              "child TextCommitted rejected");
        check(f.inputs.editors().require(input.editor).value().size() > before &&
                  !f.inputs.editors().require(input.editor).composition().active,
              "child commit did not edit native authority");
        const auto clicks_before = child_clicks;
        click(f, button.component);
        key(f, Key::space);
        check(child_clicks == clicks_before + 2 && requests == 0,
              "wrapper stole child Button pointer/keyboard activation");
        bool shadow_visible = false;
        const auto child_bounds = bounds(f, button.component);
        const auto allowed = bounds(f, owner);
        for (const auto& effect : f.services.rounded_effects().packed_instances()) {
            if (effect.geometry.kind != graphics::RoundedEffectKind::outer_shadow ||
                effect.geometry.shape.rect != child_bounds || effect.material.opacity <= 0 ||
                effect.material.color.alpha() <= 0) {
                continue;
            }
            const auto visible = graphics::rounded_effect_bounds(effect);
            if (visible.width <= 0 || visible.height <= 0) {
                continue;
            }
            shadow_visible = true;
            check(visible.x >= allowed.x - .01F && visible.y >= allowed.y - .01F &&
                      visible.x + visible.width <= allowed.x + allowed.width + .01F &&
                      visible.y + visible.height <= allowed.y + allowed.height + .01F,
                  "native child shadow escaped content allocation");
        }
        check(shadow_visible, "native child shadow was culled by wrapper input/clip changes");
        check(slot_runs == 1 && f.services.components().contains(item), "eligibility update remounted rich children");
    };
    exercise();
    check(!f.services.interactions().require(interaction(f, item)).focusable || steps && callback,
          "display/current wrapper became focusable navigation");
    if (!steps) {
        tail.set(true);
        sync(f);
        exercise();
        click(f, input.component);
        const auto stamp = f.inputs.sessions().active();
        tail.set(false);
        sync(f);
        check(f.services.focus().state().focused == input.interaction && f.inputs.sessions().active() == stamp,
              "becoming current cancelled descendant focus/session");
        exercise();
    }
    click(f, input.component);
    check(
        static_cast<bool>(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, f.inputs.sessions().active()})),
        "disable fixture preedit failed");
    disabled.set(true);
    sync(f);
    input_hit(false);
    check(!f.services.focus().state().focused && !f.inputs.sessions().active().valid(),
          "true disabled item retained descendant focus/IME session");
    check(!f.services.focus().request_focus(input.interaction, FocusModality::keyboard),
          "disabled branch allowed child programmatic focus");
    const auto disabled_clicks = child_clicks;
    click(f, button.component);
    click(f, item);
    check(child_clicks == disabled_clicks && requests == 0, "disabled wrapper/child activated");
    disabled.set(false);
    sync(f);
    exercise();
    click(f, input.component);
    shown.set(false);
    sync(f);
    check(!f.services.interactions().branch_active(input.interaction) && !f.services.focus().state().focused &&
              !f.inputs.sessions().active().valid(),
          "hidden item retained child input");
    shown.set(true);
    sync(f);
    exercise();
    spinning.set(true);
    sync(f);
    input_hit(false);
    const auto blocked_clicks = child_clicks;
    click(f, button.component);
    check(child_clicks == blocked_clicks && !f.services.focus().state().focused, "Spin allowed child activation");
    spinning.set(false);
    sync(f);
    exercise();
    open.set(true);
    sync(f);
    input_hit(false);
    click(f, button.component);
    check(child_clicks == blocked_clicks + 2, "Modal allowed underlying child activation");
    open.set(false);
    sync(f);
    exercise();
    f.services.set_window_active(false);
    const auto inactive_clicks = child_clicks;
    click(f, button.component);
    check(child_clicks == inactive_clicks && !f.inputs.sessions().active().valid(),
          "inactive window allowed child activation");
    f.services.set_window_active(true);
    sync(f);
    exercise();
    check(requests == 0 && slot_runs == 1, "native child exercise caused accidental navigation/remount");
}

float focus_coverage(Fixture& f, runtime::Rect shape, runtime::Point point) {
    float result = 0;
    for (const auto& e : f.services.rounded_effects().packed_instances()) {
        if (e.geometry.kind != graphics::RoundedEffectKind::outline || e.geometry.shape.rect != shape ||
            e.geometry.outline_width != 3 || e.geometry.outline_offset != 1 || !e.material.visible) {
            continue;
        }
        if (e.geometry.ancestor_clip) {
            const auto c = e.geometry.ancestor_clip->bounds;
            if (point.x < c.x || point.y < c.y || point.x >= c.x + c.width || point.y >= c.y + c.height) {
                continue;
            }
        }
        result = std::max(result,
                          graphics::rounded_effect_coverage(point, e) * e.material.opacity * e.material.color.alpha());
    }
    return result;
}

std::array<float, 4> outward_edges(Fixture& f, runtime::ComponentId item) {
    const auto b = bounds(f, item);
    const float offset = 2.5F;
    return {focus_coverage(f, b, {b.x - offset, b.y + b.height / 2}),
            focus_coverage(f, b, {b.x + b.width / 2, b.y - offset}),
            focus_coverage(f, b, {b.x + b.width + offset, b.y + b.height / 2}),
            focus_coverage(f, b, {b.x + b.width / 2, b.y + b.height + offset})};
}

void content_stays_clipped(Fixture& f, runtime::ComponentId owner, runtime::Rect external_clip) {
    const auto allowed = graphics::intersect_effect_bounds(bounds(f, owner), external_clip);
    glyphs_inside(f, owner, allowed);
    const auto contained = [&](runtime::Rect r) {
        return r.width <= 0 || r.height <= 0 ||
               (r.x >= allowed.x - .01F && r.y >= allowed.y - .01F &&
                r.x + r.width <= allowed.x + allowed.width + .01F &&
                r.y + r.height <= allowed.y + allowed.height + .01F);
    };
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        if (effect.geometry.kind == graphics::RoundedEffectKind::outline || effect.material.opacity <= 0 ||
            effect.material.color.alpha() <= 0) {
            continue;
        }
        check(contained(graphics::rounded_effect_bounds(effect)),
              "navigation body surface escaped strict content clip");
    }
    for (const auto& hit : f.services.hit_test().records()) {
        if (!hit.effective_eligible || !f.services.interactions().branch_active(hit.interaction)) {
            continue;
        }
        check(contained(graphics::intersect_effect_bounds(hit.translated_bounds, hit.effective_clip)),
              "navigation hit escaped strict content clip");
    }
}

void focus_decorations(bool steps) {
    for (bool vertical : {false, true}) {
        if (!steps && vertical) {
            continue;
        }
        for (std::size_t count : {std::size_t{1}, std::size_t{3}}) {
            Fixture f;
            Signal<LogicalLength> height{dp(steps && vertical ? 160 : 36)};
            f.services.mount(Content{[&] {
                Flex(FlexProps{}.vertical(true).layout(LayoutStyle{}.width(dp(500)).height(dp(400))), FlexContent{[&] {
                         if (steps) {
                             std::vector<StepsItem> items;
                             for (std::size_t i = 0; i < count; ++i) {
                                 items.emplace_back(String::from_utf8(std::to_string(i)).value(),
                                                    String{u8"Title\nOther line"});
                             }
                             Steps(StepsProps{}
                                       .vertical(vertical)
                                       .onChange([](std::size_t) {})
                                       .layout(LayoutStyle{}.width(dp(320)).height(height)),
                                   items);
                         } else {
                             std::vector<BreadcrumbItem> items;
                             for (std::size_t i = 0; i < count + 1; ++i) {
                                 items.emplace_back(String::from_utf8(std::to_string(i)).value(),
                                                    String{u8"Title\nOther line"});
                             }
                             Breadcrumb(BreadcrumbProps{}
                                            .onActivate([](BreadcrumbActivation) {})
                                            .layout(LayoutStyle{}.width(dp(320)).height(height)),
                                        items);
                         }
                     }});
            }});
            const auto synchronize = [&](runtime::Rect clip = {0, 0, 800, 800}) {
                check(f.services.layout_and_synchronize({800, 800}, clip, {40, 40}), "review focus layout failed");
            };
            synchronize();
            const auto owner = steps ? f.services.steps().mounted()[0] : f.services.breadcrumb().mounted()[0];
            const auto items =
                steps ? f.services.steps().snapshot(owner).items : f.services.breadcrumb().snapshot(owner).items;
            const auto external = *f.nodes.require(f.services.components().root(owner)).parent;
            f.services.focus().dispatch({Key::tab, KeyAction::down});
            f.services.focus().dispatch({Key::tab, KeyAction::up});
            synchronize();
            check(f.services.focus().state().focused == interaction(f, items[0]) &&
                      f.services.focus().state().focus_visible,
                  "Tab did not focus navigation wrapper");
            std::vector<float> heights{steps && vertical ? 160.0F : 36.0F, 8.0F};
            if (steps && vertical && count > 1) {
                heights.insert(heights.begin() + 1, 40.0F);
            }
            for (auto h : heights) {
                height.set(dp(h));
                synchronize();
                for (std::size_t i = 0; i < count; ++i) {
                    static_cast<void>(
                        f.services.focus().request_focus(interaction(f, items[i]), FocusModality::keyboard));
                    synchronize();
                    const auto coverage = outward_edges(f, items[i]);
                    std::cout << "focus coverage steps=" << steps << " vertical=" << vertical << " count=" << count
                              << " height=" << h << " item=" << i << " edges=" << coverage[0] << ',' << coverage[1]
                              << ',' << coverage[2] << ',' << coverage[3] << '\n';
                    const auto allocation = bounds(f, items[i]);
                    check(std::ranges::all_of(coverage,
                                              [&](float c) {
                                                  return allocation.width > 0 && allocation.height > 0 ? c > .9F
                                                                                                       : c == 0;
                                              }),
                          "outward focus edge coverage does not match its nonempty/empty allocation in roomy parent");
                    glyphs_inside(f, items[i], bounds(f, items[i]));
                    content_stays_clipped(f, owner, {0, 0, 800, 800});
                }
            }
            height.set(dp(36));
            synchronize();
            static_cast<void>(f.services.focus().request_focus(interaction(f, items[0]), FocusModality::keyboard));
            synchronize();
            f.nodes.require(external).clip_content = true;
            f.dirty.invalidate(external, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            synchronize();
            const auto clipped = outward_edges(f, items[0]);
            check(clipped[0] == 0 && clipped[1] == 0 && clipped[2] > .9F && clipped[3] > .9F,
                  "focus decoration escaped real external ancestor clip or lost available edges");
            content_stays_clipped(f, owner, f.nodes.require(external).bounds);
            f.nodes.require(external).clip_content = false;
            const auto b = bounds(f, items[0]);
            const runtime::Rect document_clip{b.x + 1, b.y + 1, std::max(0.0F, b.width - 2),
                                              std::max(0.0F, b.height - 2)};
            synchronize(document_clip);
            const auto all_clipped = outward_edges(f, items[0]);
            check(std::ranges::all_of(all_clipped, [](float c) { return c == 0; }),
                  "outline ignored external document clip");
            content_stays_clipped(f, owner, document_clip);
            height.set(dp(0));
            synchronize();
            const auto zero = outward_edges(f, items[0]);
            check(std::ranges::all_of(zero, [](float c) { return c == 0; }) &&
                      glyphs_inside(f, owner, bounds(f, owner)) == 0,
                  "zero allocation retained focus decoration/content");
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        if (mode == "children_steps") {
            child_input_eligibility(true, false);
        } else if (mode == "children_breadcrumb_current") {
            child_input_eligibility(false, true);
        } else if (mode == "children_breadcrumb_display") {
            child_input_eligibility(false, false);
        } else if (mode == "focus_steps") {
            focus_decorations(true);
        } else if (mode == "focus_breadcrumb") {
            focus_decorations(false);
        } else {
            throw std::invalid_argument("unknown navigation review case");
        }
        std::cout << "Navigation review " << mode << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
