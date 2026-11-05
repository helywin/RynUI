#include "component/carousel_component.hpp"
#include "component/popover_component.hpp"
#include "component/form_component.hpp"
#include "support/input_fixture.hpp"
#include "../examples/carousel_example.hpp"
#include <iostream>
#include <thread>
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool v, const char* m) {
    if (!v) {
        throw std::runtime_error(m);
    }
}

void sync(Fixture& f, float width = 700, float height = 500) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Carousel layout");
}

void time(Fixture& f, int ms) {
    f.services.tick_animations(animation::AnimationTime::microseconds(std::int64_t(ms) * 1000));
    sync(f);
}

std::vector<CarouselSlide> simple(int count = 3) {
    std::vector<CarouselSlide> result;
    for (int i = 0; i < count; ++i) {
        auto key = String::from_utf8(std::to_string(i)).value();
        result.emplace_back(key, CarouselContent{[key] { Text(TextProps{}.content(key)); }});
    }
    return result;
}

void effects() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    CarouselRef ref;
    Signal<CarouselEffect> effect{CarouselEffect::ScrollX};
    int before{};
    int after{};
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(simple())
                     .effect(effect)
                     .easing(CubicBezier{0, 0, 1, 1})
                     .speed(Duration::milliseconds(500))
                     .beforeChange([&](auto a, auto b) {
                         check(a == 0 && b == 1 || a == 1 && b == 0, "transition pair");
                         ++before;
                     })
                     .afterChange([&](auto) { ++after; }));
    }});
    sync(f);
    auto id = f.services.carousel().mounted().back();
    auto initial = f.services.carousel().snapshot(id);
    check(ref.next(), "next rejected");
    time(f, 250);
    auto middle = f.services.carousel().snapshot(id);
    auto old = f.nodes.require(f.services.components().root(middle.slides[0])).bounds;
    auto next = f.nodes.require(f.services.components().root(middle.slides[1])).bounds;
    check(middle.transitioning && std::abs(middle.progress - .5F) < .001F && old.x < 0 && next.x > 0 &&
              next.x - old.x == middle.viewport.width,
          "actual halfway ScrollX geometry");
    check(!f.services.interactions().component_active(middle.slides[0]) &&
              f.services.interactions().component_active(middle.slides[1]),
          "paint/input gate");
    time(f, 500);
    check(after == 1 && before == 1 && !f.services.carousel().snapshot(id).transitioning, "completion callbacks");
    effect.set(CarouselEffect::Fade);
    sync(f);
    check(ref.prev(), "fade prev");
    time(f, 750);
    middle = f.services.carousel().snapshot(id);
    const auto a = f.nodes.require(f.services.components().root(middle.slides[0]));
    const auto b = f.nodes.require(f.services.components().root(middle.slides[1]));
    check(a.bounds == b.bounds && std::abs(a.paint_opacity - .5F) < .001F && std::abs(b.paint_opacity - .5F) < .001F,
          "actual halfway Fade output");
    bool painted_half{};
    for (const auto& glyph : f.scene.glyph_scene().instances().instances()) {
        painted_half = painted_half || std::abs(glyph.translation_opacity[2] - .5F) < .001F;
    }
    check(painted_half, "Fade did not reach actual native glyph scene");
    time(f, 1000);
    check(after == 2 && before == 2, "fade callback once");
    while (f.frames.consume_request()) {
    }
    const auto requests = f.frames.counters().requests;
    time(f, 2000);
    time(f, 3000);
    check(!f.services.next_frame_deadline() && f.frames.counters().requests == requests, "idle frame loop");
}

void controlled() {
    Fixture f;
    CarouselRef ref;
    Signal<std::size_t> current{0};
    int requests{};
    int before{};
    int after{};
    bool accept{};
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(simple())
                     .current(current)
                     .onCurrentChange([&](auto i, auto) {
                         ++requests;
                         if (accept) {
                             current.set(i);
                         }
                     })
                     .beforeChange([&](auto, auto) { ++before; })
                     .afterChange([&](auto) { ++after; }));
    }});
    sync(f);
    auto id = f.services.carousel().mounted().back();
    check(!ref.next() && f.services.carousel().snapshot(id).current == 0 && requests == 1 && before == 0 && after == 0,
          "controlled refusal silently accepted");
    accept = true;
    check(ref.next(), "accepted controlled ref");
    sync(f);
    check(f.services.carousel().snapshot(id).current == 1 && before == 1 && after == 1,
          "controlled callback exactly once");
    current.set(2);
    sync(f);
    check(before == 2 && after == 2, "external accepted property");
    f.services.destroy(id);
    check(!ref.next() && !ref.pause() && !ref.focus(), "stale ref");
    bool rejected{};
    std::thread t([&] {
        try {
            static_cast<void>(ref.next());
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    t.join();
    check(rejected, "foreign ref");
    Fixture g;
    g.buttons.set_motion_preference(animation::MotionPreference::normal);
    Signal<std::size_t> live{0};
    g.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(simple())
                     .current(live)
                     .easing(CubicBezier{0, 0, 1, 1})
                     .onCurrentChange([&](auto target, auto) { live.set(target); }));
    }});
    sync(g);
    const auto controlled_id = g.services.carousel().mounted().back();
    check(ref.goTo(2, true) && !g.services.carousel().snapshot(controlled_id).transitioning,
          "controlled dontAnimate ignored");
    sync(g);
    check(ref.next(), "controlled infinite next");
    time(g, 250);
    const auto wrap = g.services.carousel().snapshot(controlled_id);
    check(g.nodes.require(g.services.components().root(wrap.slides[2])).bounds.x < wrap.viewport.x &&
              g.nodes.require(g.services.components().root(wrap.slides[0])).bounds.x > wrap.viewport.x,
          "controlled wrap lost requested direction");
}

void retained() {
    Fixture f;
    CarouselRef ref;
    InputRef first;
    int mounts{};
    int events{};
    auto original = simple();
    original[0] = CarouselSlide{u8"draft", CarouselContent{[&] {
                                    ++mounts;
                                    TextArea(TextAreaProps{}.ref(first).defaultValue(u8"draft"));
                                }}};
    Signal<std::vector<CarouselSlide>> slides{original};
    f.services.mount(
        Content{[&] { Carousel(CarouselProps{}.ref(ref).slides(slides).afterChange([&](auto) { ++events; })); }});
    sync(f);
    auto id = f.services.carousel().mounted().back();
    auto initial = f.services.carousel().snapshot(id);
    check(first.focus(), "active editor");
    auto stamp = f.inputs.sessions().active();
    static_cast<void>(f.inputs.editors().require(stamp.owner).select_all());
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8"preserved"}, stamp})), "edit draft");
    check(ref.next(), "retained next");
    sync(f);
    check(!first.focus() && !f.inputs.sessions().active().valid(), "hidden input session survived");
    check(ref.prev(), "retained prev");
    sync(f);
    check(first.focus() && mounts == 1 &&
              f.inputs.editors().require(stamp.owner).value() == String{u8"preserved"}.bytes(),
          "draft lost after round trip");
    const auto before_replacement = f.services.carousel().snapshot(id).slides;
    original[2] =
        CarouselSlide{original[2].key(), CarouselContent{[] { Input(InputProps{}.defaultValue(u8"replacement")); }}};
    slides.set(original);
    sync(f);
    const auto replaced = f.services.carousel().snapshot(id).slides;
    check(replaced[0] == before_replacement[0] && replaced[1] == before_replacement[1] &&
              replaced[2] != before_replacement[2] && mounts == 1,
          "same key factory replacement was not local");
    std::swap(original[0], original[2]);
    slides.set(original);
    sync(f);
    auto reordered = f.services.carousel().snapshot(id);
    check(reordered.current == 2 && reordered.slides[2] == initial.slides[0] && mounts == 1 && events == 2,
          "stable active key reorder");
    original.pop_back();
    slides.set(original);
    sync(f);
    check(!first.focus() && f.services.carousel().snapshot(id).current == 1 && events == 3, "removed active fallback");
    slides.set(std::vector<CarouselSlide>{});
    sync(f);
    check(!f.services.carousel().snapshot(id).current && !ref.next(), "empty slides");
}

void autoplay() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    CarouselRef ref;
    Signal<bool> visible{true};
    int changes{};
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(simple())
                     .autoplay()
                     .animated(false)
                     .visible(visible)
                     .autoplaySpeed(Duration::milliseconds(1000))
                     .afterChange([&](auto) { ++changes; }));
    }});
    sync(f);
    auto id = f.services.carousel().mounted().back();
    check(f.services.next_frame_deadline() == animation::AnimationTime::microseconds(1000000),
          "autoplay first deadline");
    time(f, 999);
    check(changes == 0, "early autoplay");
    time(f, 1000);
    check(changes == 1, "deadline autoplay");
    check(ref.pause(), "manual pause");
    time(f, 5000);
    check(changes == 1 && !f.services.next_frame_deadline(), "paused autoplay polling");
    check(ref.play(), "resume autoplay");
    time(f, 5999);
    check(changes == 1, "resume deadline");
    time(f, 6000);
    check(changes == 2, "resume advance");
    visible.set(false);
    sync(f);
    time(f, 10000);
    check(!f.services.next_frame_deadline() && changes == 2, "hidden autoplay");
    visible.set(true);
    sync(f);
    f.services.set_window_active(false);
    time(f, 20000);
    check(changes == 2 && !f.services.next_frame_deadline(), "inactive autoplay");
    f.services.set_window_active(true);
    f.buttons.set_motion_preference(animation::MotionPreference::reduced);
    sync(f);
    time(f, 30000);
    check(changes == 2 && !f.services.next_frame_deadline(), "reduced autoplay");
    f.services.destroy(id);
    check(!f.services.next_frame_deadline(), "timer unmount");
}

void gesture() {
    using namespace input;
    Fixture f;
    CarouselRef ref;
    InputRef editor;
    std::vector<CarouselSlide> slides{
        {u8"editor", CarouselContent{[&] { Input(InputProps{}.ref(editor).defaultValue(u8"do not steal")); }}},
        {u8"other", CarouselContent{[] { Text(u8"second"); }}}};
    f.services.mount(Content{[&] { Carousel(CarouselProps{}.ref(ref).slides(slides).draggable().infinite(false)); }});
    sync(f);
    auto id = f.services.carousel().mounted().back();
    auto snap = f.services.carousel().snapshot(id);
    auto r = snap.viewport;
    const auto pointer = [&](PointerAction a, float x, float y, PointerIdentity p = PointerIdentity::mouse()) {
        f.services.pointer().dispatch(
            {p, a,
             (a == PointerAction::move || a == PointerAction::cancel) ? PointerButton::none : PointerButton::primary, x,
             y});
        sync(f);
    };
    auto input = f.inputs.mounted_inputs().back();
    auto box = f.nodes.require(input.node).bounds;
    pointer(PointerAction::down, box.x + 20, box.y + 10);
    pointer(PointerAction::move, box.x + 180, box.y + 10);
    pointer(PointerAction::up, box.x + 180, box.y + 10);
    check(f.services.carousel().snapshot(id).current == 0, "parent stole Input selection");
    const auto y = r.y + r.height - 40;
    pointer(PointerAction::down, r.x + r.width * .8F, y);
    pointer(PointerAction::move, r.x + r.width * .2F, y);
    pointer(PointerAction::up, r.x + r.width * .2F, y);
    check(f.services.carousel().snapshot(id).current == 1, "drag threshold transition");
    check(ref.focus(), "keyboard root");
    f.services.focus().dispatch({Key::left, KeyAction::down});
    sync(f);
    check(f.services.carousel().snapshot(id).current == 0, "root key");
    pointer(PointerAction::down, r.x + 50, y);
    pointer(PointerAction::move, r.x + 300, y);
    pointer(PointerAction::cancel, r.x + 300, y);
    check(f.services.carousel().snapshot(id).current == 0 && !f.services.carousel().snapshot(id).dragging,
          "edge cancel");
    const auto first_touch = PointerIdentity::touch(1, 1);
    const auto second_touch = PointerIdentity::touch(1, 2);
    pointer(PointerAction::down, r.x + 400, y, first_touch);
    pointer(PointerAction::move, r.x + 350, y, first_touch);
    pointer(PointerAction::down, r.x + 300, y, second_touch);
    check(!f.services.pointer().state(first_touch)->capture && f.services.carousel().snapshot(id).current == 0,
          "second pointer retained old capture");
    pointer(PointerAction::cancel, r.x + 300, y, second_touch);
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    sync(f);
    pointer(PointerAction::down, r.x + 400, y);
    pointer(PointerAction::move, r.x + 350, y);
    f.buttons.set_motion_preference(animation::MotionPreference::reduced);
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
              !f.services.carousel().snapshot(id).dragging,
          "reduced motion retained gesture capture");
}

void failure() {
    Fixture f;
    CarouselRef ref;
    Signal<std::vector<CarouselSlide>> slides{simple()};
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}.ref(ref).slides(slides));
        Button(ButtonProps{}, [] { Text(u8"unrelated"); });
    }});
    sync(f);
    auto id = f.services.carousel().mounted().back();
    auto before = f.services.carousel().snapshot(id).slides;
    auto bad = simple();
    bad.emplace_back(u8"failure", CarouselContent{[] {
                         Input(InputProps{});
                         throw std::runtime_error("slide factory");
                     }});
    slides.set(bad);
    bool caught{};
    try {
        sync(f);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught && f.services.carousel().snapshot(id).slides == before, "slide factory rollback");
    auto duplicate = simple();
    duplicate.push_back(duplicate.front());
    slides.set(duplicate);
    bool invalid{};
    try {
        sync(f);
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    check(invalid && f.services.carousel().snapshot(id).slides == before, "invalid keys replaced accepted topology");
    sync(f); // A rejected topology is not retried on every host pump.
    slides.set(simple());
    sync(f);
    f.services.dispose();
    check(!ref.next() && !f.services.next_frame_deadline() && f.nodes.size() == 0, "window destroy");
    Fixture g;
    runtime::ComponentId owner;
    g.services.mount(Content{[&] {
        Carousel(CarouselProps{}.ref(ref).slides(simple()).afterChange([&](auto) { g.services.destroy(owner); }));
    }});
    sync(g);
    owner = g.services.carousel().mounted().back();
    check(ref.next() && !ref.focus(), "callback self destroy");
}

void theme() {
    const auto base = resolve_theme();
    check(base.carousel().metrics.dot_offset == 12 && base.carousel().metrics.arrow_offset == 8 &&
              base.carousel().metrics.arrow_size == 16 && base.carousel().metrics.dot_gap == 4,
          "pinned Carousel metrics");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    const auto small = resolve_theme(compact).carousel().metrics;
    check(small.dot_gap == 4 && small.arrow_offset == 4 && small.arrow_size == 16 && small.dot_offset == 12,
          "pinned Compact Carousel metrics");
    ThemeConfig config;
    config.carousel.tokens.dot_width = dp(30);
    config.carousel.tokens.arrow = Color::rgba8(22, 44, 66);
    const auto changed = resolve_theme(config);
    check(changed.carousel().metrics.dot_width == 30 && changed.calendar() == base.calendar() &&
              changed.tabs() == base.tabs(),
          "independent carousel Theme");
    check(resolve_theme({}, &changed).carousel() == changed.carousel(), "carousel inherit");
    config.carousel.algorithm = true;
    config.carousel.seed.color_primary = Color::rgba8(200, 10, 20);
    check(resolve_theme(config).carousel().colors.focus != base.carousel().colors.focus, "carousel scoped algorithm");
    Fixture f;
    f.services.mount(Content{[] { CarouselExample(); }});
    sync(f);
    check(theme_runtime::token_identity_name(theme_runtime::TokenIdentity::calendar_colors) == "Calendar.colors" &&
              theme_runtime::token_identity_name(theme_runtime::TokenIdentity::carousel_colors) == "Carousel.colors" &&
              theme_runtime::token_identity_name(theme_runtime::TokenIdentity::carousel_metrics) ==
                  "Carousel.metrics" &&
              theme_runtime::token_identity_name(theme_runtime::TokenIdentity::carousel_effects) == "Carousel.effects",
          "new TokenIdentity names/order");
    auto scope = theme_runtime::ThemeScope::create_default();
    int colors{};
    int metrics{};
    int effects{};
    auto c = scope->capture([&](auto) { ++colors; }, [&] { static_cast<void>(scope->carousel_colors()); });
    auto m = scope->capture([&](auto) { ++metrics; }, [&] { static_cast<void>(scope->carousel_metrics()); });
    auto e = scope->capture([&](auto) { ++effects; }, [&] { static_cast<void>(scope->carousel_effects()); });
    ThemeConfig local;
    local.carousel.tokens.dot = Color::rgba8(1, 2, 3);
    scope->update(local);
    check(colors == 1 && metrics == 0 && effects == 0, "Carousel token invalidation domains");
    local.carousel.tokens.disabled_opacity = .4F;
    local.carousel.tokens.shadow = ShadowList{{ShadowKind::outer, {}, 2, 0, Color::rgba8(0, 0, 0)}};
    scope->update(local);
    check(effects == 1, "Carousel effects notified twice");
    const auto identity = scope->snapshot().identity();
    local.carousel.tokens.dot_width = dp(-1);
    bool invalid{};
    try {
        scope->update(local);
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    check(invalid && scope->snapshot().identity() == identity, "invalid Carousel Theme was not atomic");
    Fixture g;
    Signal<ThemeConfig> live{ThemeConfig{}};
    int unrelated{};
    g.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(live), ThemeContent{[&] {
                  Carousel(CarouselProps{}.slides(simple()));
                  ++unrelated;
                  Button(ButtonProps{}, [] { Text(u8"stable"); });
              }});
    }});
    sync(g);
    const auto owner = g.services.carousel().mounted().back();
    const auto node = g.services.components().root(owner);
    const auto measured = g.nodes.require(node).measure_count;
    const auto children = g.services.carousel().snapshot(owner).slides;
    ThemeConfig color;
    color.carousel.tokens.arrow = Color::rgba8(12, 34, 56);
    live.set(color);
    sync(g);
    check(g.nodes.require(node).measure_count == measured && children == g.services.carousel().snapshot(owner).slides &&
              unrelated == 1,
          "Carousel Theme remounted or remeasured unrelated content");
}

void interruptions() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    CarouselRef ref;
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    Signal<bool> vertical{false};
    int after{};
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(simple())
                     .arrows()
                     .direction(direction)
                     .vertical(vertical)
                     .easing(CubicBezier{0, 0, 1, 1})
                     .afterChange([&](auto) { ++after; }));
    }});
    sync(f);
    const auto id = f.services.carousel().mounted().back();
    const auto original = f.services.carousel().snapshot(id).slides;
    check(ref.prev(), "infinite backward wrap");
    time(f, 250);
    const auto middle = f.services.carousel().snapshot(id);
    check(middle.current == 2 && middle.slides == original, "infinite wrap cloned slides");
    const auto incoming = f.nodes.require(f.services.components().root(original[2])).bounds;
    check(incoming.x > middle.viewport.x, "RTL wrap direction");
    sync(f, 500);
    check(!f.services.carousel().snapshot(id).transitioning && after == 1, "resize failed to finish once");
    vertical.set(true);
    sync(f, 500);
    check(ref.next(), "vertical wrap");
    f.services.tick_animations(animation::AnimationTime::microseconds(500000));
    sync(f, 500);
    const auto a = f.nodes.require(f.services.components().root(original[0])).bounds;
    const auto b = f.nodes.require(f.services.components().root(original[2])).bounds;
    check(a.y > b.y && a.x == b.x, "vertical ScrollX geometry");
    f.buttons.set_motion_preference(animation::MotionPreference::reduced);
    sync(f, 500);
    check(after == 2 && !f.services.carousel().snapshot(id).transitioning, "reduced motion completion");
    time(f, 2000);
    check(after == 2, "interrupted completion repeated");
    f.services.dispose();
    check(!ref.goTo(0) && !f.services.next_frame_deadline(), "destroy retained motion");
    Fixture g;
    g.buttons.set_motion_preference(animation::MotionPreference::normal);
    int completed{};
    std::vector<CarouselSlide> heights;
    for (int height : {64, 128}) {
        heights.emplace_back(String::from_utf8(std::to_string(height)).value(), CarouselContent{[height] {
                                 Flex(FlexProps{}.layout(LayoutStyle{}.height(dp(float(height)))),
                                      [] { Text(u8"height"); });
                             }});
    }
    g.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(heights)
                     .adaptiveHeight()
                     .easing(CubicBezier{0, 0, 1, 1})
                     .afterChange([&](auto) { ++completed; }));
    }});
    sync(g);
    const auto adaptive_id = g.services.carousel().mounted().back();
    check(g.services.carousel().snapshot(adaptive_id).viewport.height == 64 && ref.next(), "adaptive initial height");
    time(g, 250);
    check(std::abs(g.services.carousel().snapshot(adaptive_id).viewport.height - 96) < .01F && completed == 0,
          "adaptive halfway actual geometry");
    time(g, 500);
    check(g.services.carousel().snapshot(adaptive_id).viewport.height == 128 && completed == 1,
          "adaptive final height");
}

void pause_progress() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    CarouselRef ref;
    InputRef editor;
    int after{};
    auto slides = simple();
    slides[0] = CarouselSlide{u8"editor", CarouselContent{[&] { Input(InputProps{}.ref(editor)); }}};
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(slides)
                     .autoplay()
                     .dotDuration()
                     .animated(false)
                     .autoplaySpeed(Duration::milliseconds(1000))
                     .afterChange([&](auto) { ++after; }));
    }});
    sync(f);
    const auto id = f.services.carousel().mounted().back();
    time(f, 250);
    check(std::abs(f.services.carousel().snapshot(id).autoplay_progress - .25F) < .001F, "dot progress clock");
    const auto dot = f.nodes.require(f.services.components().root(f.services.carousel().snapshot(id).dots[0])).bounds;
    bool base_painted{};
    bool progress_painted{};
    for (const auto& effect : f.services.surfaces().effects().packed_instances()) {
        const auto r = effect.geometry.shape.rect;
        if (r.x >= dot.x && r.y >= dot.y && r.x + r.width <= dot.x + dot.width &&
            r.y + r.height <= dot.y + dot.height && r.height <= 3) {
            base_painted = base_painted ||
                           (std::abs(r.width - 24) < .001F && std::abs(effect.material.color.alpha() - .2F) < .001F);
            progress_painted =
                progress_painted || (std::abs(r.width - 6) < .001F && effect.material.color.alpha() == 1);
        }
    }
    check(base_painted && progress_painted, "dot progress must remain visible over its translucent base");

    check(editor.focus(), "focus pause editor");
    sync(f);
    time(f, 5000);
    check(after == 0 && f.services.carousel().snapshot(id).paused, "focused descendant did not pause");
    f.services.focus().defer_focus({}, input::FocusModality::keyboard);
    sync(f);
    time(f, 5749);
    check(after == 0, "focus resume lost remainder");
    time(f, 5750);
    check(after == 1, "focus resume deadline");
    const auto r = f.services.carousel().snapshot(id).viewport;
    f.services.pointer().dispatch(
        {input::PointerIdentity::mouse(), input::PointerAction::move, input::PointerButton::none, r.x + 20, r.y + 80});
    sync(f);
    time(f, 10000);
    check(after == 1 && !f.services.next_frame_deadline(), "hover did not pause");
    f.services.pointer().dispatch(
        {input::PointerIdentity::mouse(), input::PointerAction::move, input::PointerButton::none, 699, 499});
    sync(f);
    time(f, 11000);
    check(after == 2, "hover resume");
    check(ref.next(), "manual advance");
    sync(f);
    check(f.services.carousel().snapshot(id).autoplay_progress == 0, "manual interaction did not reset progress");
}

void nested_lifecycle() {
    Fixture f;
    CarouselRef ref;
    InputRef editor;
    Signal<bool> open{false};
    Signal<bool> visible{true};
    Signal<LogicalLength> width{dp(600)};
    auto slides = simple(2);
    slides[0] = CarouselSlide{
        u8"rich", CarouselContent{[&] {
            TextArea(TextAreaProps{}.ref(editor).defaultValue(u8"retained"));
            Popover(PopoverProps{}.open(open).onOpenChange([&](bool v) { open.set(v); }),
                    PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"nested popup"); }); }},
                    PopoverSlots{.content = PopoverContent{[] { Input(InputProps{}.defaultValue(u8"popup draft")); }}});
        }}};
    f.services.mount(Content{[&] {
        Carousel(
            CarouselProps{}.ref(ref).slides(slides).visible(visible).draggable().layout(LayoutStyle{}.width(width)));
    }});
    sync(f);
    check(editor.focus(), "rich editor focus");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"拼音"}, {0, 2, true}, stamp})),
          "Carousel Core preedit");
    f.services.focus().dispatch({input::Key::right, input::KeyAction::down});
    sync(f);
    const auto id = f.services.carousel().mounted().back();
    check(f.services.carousel().snapshot(id).current == 0, "Carousel stole TextArea arrow");
    open.set(true);
    sync(f);
    check(ref.next(), "switch rich slide");
    sync(f);
    check(!f.inputs.sessions().active().valid() && !editor.focus(), "offscreen slide retained preedit/focus");
    for (auto popup : f.services.popover().mounted()) {
        check(!f.services.popover().snapshot(popup).geometry.visible, "old slide popup remained actionable");
    }
    check(!f.inputs.dispatch(input::TextCommitted{String{u8"stale"}, stamp}), "retired IME stamp accepted");
    check(ref.prev(), "restore rich slide");
    sync(f);
    check(editor.focus(), "restored rich editor");
    visible.set(false);
    sync(f);
    check(!editor.focus() && !ref.next() && !f.inputs.sessions().active().valid(), "hidden input authority");
    visible.set(true);
    sync(f);
    check(ref.focus(), "root focus restored");
    width.set(dp(0));
    sync(f);
    check(!ref.focus() && !f.services.next_frame_deadline(), "zero area kept input/timer");
}

void callbacks() {
    for (int kind = 0; kind < 3; ++kind) {
        Fixture f;
        CarouselRef ref;
        Signal<std::size_t> current{0};
        f.services.mount(Content{[&] {
            auto props = CarouselProps{}.ref(ref).slides(simple());
            if (kind == 0) {
                props.beforeChange([](auto, auto) { throw std::runtime_error("before"); });
            }
            if (kind == 1) {
                props.afterChange([](auto) { throw std::runtime_error("after"); });
            }
            if (kind == 2) {
                props.current(current).onCurrentChange([](auto, auto) { throw std::runtime_error("request"); });
            }
            Carousel(props);
            Button(ButtonProps{}, [] { Text(u8"unrelated"); });
        }});
        sync(f);
        bool caught{};
        try {
            static_cast<void>(ref.next());
        } catch (const std::runtime_error&) {
            caught = true;
        }
        check(caught && !ref.next() && !f.services.next_frame_deadline(), "callback exception failed cleanup");
        sync(f);
        check(!f.buttons.mounted_buttons().empty(), "callback retired unrelated component");
    }
    Fixture f;
    CarouselRef ref;
    Signal<std::size_t> current{0};
    int after{};
    bool reentry{};
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(simple())
                     .current(current)
                     .onCurrentChange([&](auto target, auto) { current.set(target); })
                     .afterChange([&](auto target) {
                         ++after;
                         reentry = ref.next();
                         if (target == 1) {
                             current.set(2);
                         }
                     }));
    }});
    sync(f);
    check(ref.next() && !reentry, "callback ref reentry accepted");
    sync(f);
    check(after == 2 && f.services.carousel().snapshot(f.services.carousel().mounted().back()).current == 2,
          "queued authority update lost");
}

void rich_controls() {
    Fixture f;
    FormField<String> outer{String{u8"outer authority"}};
    FormField<String> nested{String{u8"nested authority"}};
    InputRef public_ref;
    InputRef nested_ref;
    std::vector<InputRef> dots;
    Signal<std::vector<CarouselSlide>> slides{simple(2)};
    CarouselRef ref;
    std::vector<Binding<CarouselDotInfo>> metadata;
    int mounts{};
    f.services.mount(Content{[&] {
        Form(FormProps{}, FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                              Carousel(
                                  CarouselProps{}
                                      .ref(ref)
                                      .slides(slides)
                                      .arrows()
                                      .nextArrow([](Binding<CarouselArrowInfo> info, std::function<void()> action) {
                                          check(info.get().next, "next arrow initial metadata");
                                          return CarouselArrow{[action] {
                                              Button(ButtonProps{}.onClick(action), [] { Text(u8"Next"); });
                                          }};
                                      })
                                      .customPaging([&](Binding<CarouselDotInfo> info, std::function<void()> activate) {
                                          metadata.push_back(info);
                                          return CarouselDot{[&, info, activate] {
                                              ++mounts;
                                              InputRef dot;
                                              dots.push_back(dot);
                                              Flex(FlexProps{}.gap(dp(4)), [&, info, activate, dot] {
                                                  Input(InputProps{}
                                                            .ref(dot)
                                                            .defaultValue(u8"private draft")
                                                            .layout(LayoutStyle{}.width(dp(90))));
                                                  Button(ButtonProps{}.onClick(activate), [] { Text(u8"go"); });
                                                  if (info.get().index == 0) {
                                                      FormItem(FormItemProps{}.field(u8"nested", nested),
                                                               FormItemContent{[&] {
                                                                   Input(InputProps{}
                                                                             .ref(nested_ref)
                                                                             .layout(LayoutStyle{}.width(dp(90))));
                                                               }});
                                                  }
                                              });
                                          }};
                                      }));
                              Input(InputProps{}.ref(public_ref));
                          }});
             }});
    }});
    sync(f);
    check(public_ref.focus(), "public Form control after private Carousel");
    check(f.inputs.editors().require(f.inputs.sessions().active().owner).value() == "outer authority",
          "private dot captured parent field");
    check(nested_ref.focus(), "explicit nested FormItem suppressed by private scope");
    check(f.inputs.editors().require(f.inputs.sessions().active().owner).value() == "nested authority",
          "nested Form authority lost");
    check(metadata.size() == 2 && metadata[1].get().index == 1 && metadata[1].get().key == String{u8"1"},
          "custom dot metadata");
    const auto id = f.services.carousel().mounted().back();
    const auto old = f.services.carousel().snapshot(id);
    check(dots[0].focus(), "custom dot editor");
    const auto stamp = f.inputs.sessions().active();
    static_cast<void>(f.inputs.editors().require(stamp.owner).select_all());
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8"retained dot"}, stamp})), "custom dot draft");
    auto values = slides.get();
    std::reverse(values.begin(), values.end());
    slides.set(values);
    sync(f);
    check(mounts == 2 && f.services.carousel().snapshot(id).dots[1] == old.dots[0] && metadata[0].get().index == 1,
          "dot key reorder remounted");
    check(dots[0].focus() && f.inputs.editors().require(stamp.owner).value() == "retained dot",
          "custom dot draft lost");
}

void pixels() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    CarouselRef ref;
    std::vector<CarouselSlide> slides;
    for (int i = 0; i < 2; ++i) {
        ThemeConfig config;
        config.button.tokens.primary_background = i ? Color::rgba8(0, 0, 255) : Color::rgba8(255, 0, 0);
        slides.emplace_back(i ? String{u8"blue"} : String{u8"red"}, CarouselContent{[config] {
                                Theme(ThemeProps{}.config(config), ThemeContent{[] {
                                          Button(ButtonProps{}.type(ButtonType::Primary).wave(false), [] {});
                                      }});
                            }});
    }
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .slides(slides)
                     .dots(false)
                     .effect(CarouselEffect::Fade)
                     .easing(CubicBezier{0, 0, 1, 1}));
    }});
    sync(f);
    const auto owner = f.services.carousel().mounted().back();
    const auto selected = f.services.carousel().snapshot(owner).slides.front();
    runtime::Rect button;
    for (const auto& candidate : f.buttons.mounted_buttons()) {
        for (auto ancestor = std::optional{candidate.component}; ancestor;
             ancestor = f.services.components().parent(*ancestor)) {
            if (*ancestor == selected) {
                button = f.nodes.require(candidate.node).bounds;
                break;
            }
        }
    }
    check(button.width > 0 && button.height > 0, "native pixel fixture button");
    const auto px = button.x + button.width / 2;
    const auto py = button.y + button.height / 2;
    const auto pixel = [&] {
        std::array<float, 3> rgb{};
        // Sample a solid interior pixel of the actual retained native Button quads.
        // No glyph/curve boundary intersects this point; this is a CPU scene check.
        for (const auto& q : f.services.surfaces().instances().instances()) {
            const auto x = q.bounds[0] + q.translation[0];
            const auto y = q.bounds[1] + q.translation[1];
            if (px > x && px < x + q.bounds[2] && py > y && py < y + q.bounds[3]) {
                const auto alpha = q.opacity * q.color[3];
                for (std::size_t c = 0; c < 3; ++c) {
                    rgb[c] = q.color[c] * alpha + rgb[c] * (1 - alpha);
                }
            }
        }
        return rgb;
    };
    auto initial = pixel();
    check(initial[0] > .9F && initial[2] < .1F, "initial native pixel");
    check(ref.next(), "pixel Fade request");
    time(f, 250);
    auto middle = pixel();
    check(middle[0] > .1F && middle[2] > .1F && middle != initial, "halfway Fade left native pixels unchanged");
    time(f, 500);
    auto final = pixel();
    check(final[0] < .1F && final[2] > .9F, "completed Fade native pixel");
}

int main(int argc, char** argv) {
    try {
        const std::string s = argc > 1 ? argv[1] : "effects";
        if (s == "pixels") {
            pixels();
        } else if (s == "rich_controls") {
            rich_controls();
        } else if (s == "interruptions") {
            interruptions();
        } else if (s == "pause_progress") {
            pause_progress();
        } else if (s == "nested_lifecycle") {
            nested_lifecycle();
        } else if (s == "callbacks") {
            callbacks();
        } else if (s == "effects") {
            effects();
        } else if (s == "controlled") {
            controlled();
        } else if (s == "retained") {
            retained();
        } else if (s == "autoplay") {
            autoplay();
        } else if (s == "gesture") {
            gesture();
        } else if (s == "failure") {
            failure();
        } else if (s == "theme") {
            theme();
        } else {
            throw std::invalid_argument("case");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
