#include "component/carousel_component.hpp"
#include "component/modal_component.hpp"
#include "support/timeline_fixture.hpp"
#include "support/allocation_probe.hpp"
#include <iostream>
using namespace ryn;
using namespace timeline_test;

namespace {
int checks{};

void expect(bool value, const char* message) {
    ++checks;
    check(value, message);
}

struct Consumer final : detail::WindowComponentParticipant {
    detail::WindowComponentServices& services;
    runtime::ComponentId target;
    bool stable{};
    bool active{};
    std::size_t calls{};
    std::function<void()> publication;
    bool publishing{};

    explicit Consumer(detail::WindowComponentServices& s) : services(s) {
        s.attach(*this);
    }

    ~Consumer() {
        services.detach(*this);
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

    void on_input_boundary_changed() override {
        if (publishing && publication) {
            auto callback = std::exchange(publication, {});
            callback();
        }
    }

    void position_window_layers(runtime::Size size, runtime::Rect) override {
        ++calls;
        publishing = true;
        stable = services.carousel().refresh_target_admission(target, size, document);
        publishing = false;
        active = services.interactions().subtree_intersects_input_domain(target);
    }

    runtime::Rect document{0, 0, 600, 800};
};

struct Slides final : Fixture {
    Signal<std::size_t> current{0};
    Signal<bool> visible{true};
    Signal<bool> disabled{false};
    Signal<std::vector<CarouselSlide>> items{std::vector<CarouselSlide>{}};
    std::array<InputRef, 2> refs;
    Consumer consumer{services};
    runtime::ComponentId owner;
    std::array<runtime::ComponentId, 2> editors;

    void mount(bool animated = false, CarouselEffect effect = CarouselEffect::ScrollX) {
        items.set(
            std::vector{CarouselSlide{u8"first", CarouselContent{[this] { Input(InputProps{}.ref(refs[0])); }}},
                        CarouselSlide{u8"second", CarouselContent{[this] { Input(InputProps{}.ref(refs[1])); }}}});
        services.mount(Content{[&] {
            Carousel(CarouselProps{}
                         .slides(items)
                         .current(current)
                         .visible(visible)
                         .disabled(disabled)
                         .animated(animated)
                         .effect(effect)
                         .easing(CubicBezier{0, 0, 1, 1})
                         .arrows(true)
                         .layout(LayoutStyle{}.width(dp(300)).height(dp(100))));
        }});
        owner = services.carousel().mounted().front();
        for (std::size_t i = 0; i < 2; ++i) {
            editors[i] = inputs.mounted_inputs()[i].component;
        }
        consumer.target = editors[0];
    }

    void clean() {
        services.dispose();
        const auto policy = services.interactions().input_constraint_diagnostics();
        expect(blocked_roots(services.interactions()).empty() && policy.leases == 0 && policy.companions == 0 &&
                   services.interactions().size() == 0 && services.surfaces().size() == 0 &&
                   !services.next_frame_deadline(),
               "Carousel retired exact private block/resource generations leaked");
    }
};

void same_frame() {
    for (const bool reverse : {false, true}) {
        Slides f;
        f.mount();
        sync(f);
        expect(f.consumer.stable && f.consumer.active && f.consumer.calls == 1 && f.refs[0].focus(),
               "First single synchronization consumed stale slide admission");
        if (reverse) {
            f.current.set(1);
            f.consumer.target = f.editors[1];
        } else {
            f.consumer.target = f.editors[1];
            f.current.set(1);
        }
        sync(f);
        expect(f.consumer.stable && f.consumer.active && f.consumer.calls == 2 && f.refs[1].focus() &&
                   !f.refs[0].focus(),
               "Same-frame accepted current/input target consumed stale gate");
        f.consumer.target = f.editors[0];
        sync(f);
        expect(!f.consumer.active && !f.refs[0].focus(), "Changing only target authorized an inactive slide");
        f.current.set(0);
        sync(f);
        expect(f.consumer.active && f.refs[0].focus(), "Changing only current did not refresh target admission");
        f.clean();
    }
}

void policies_and_clip() {
    Slides f;
    f.mount();
    sync(f);
    f.services.set_input_blocked(f.editors[0], true);
    sync(f);
    expect(!f.consumer.active && !f.refs[0].focus(), "Producer erased an independent target hard block");
    f.services.set_input_blocked(f.editors[0], false);
    auto lease = f.services.create_input_constraint(f.services.interactions().input_root(f.owner),
                                                    input::InputConstraintKind::BlockSubtrees);
    auto batch = f.services.interactions().begin_input_constraint_update();
    batch.replace(lease, {{f.services.interactions().input_root(f.editors[0])}});
    f.services.publish_input_constraints(batch);
    sync(f);
    expect(!f.consumer.active && !f.refs[0].focus(), "Producer bypassed an independent owned block");
    f.services.release_input_constraint(lease);
    f.consumer.document = {500.25F, 0, 40.5F, 40.25F};
    expect(f.services.layout_and_synchronize({600, 800}, f.consumer.document), "Clipped layout failed");
    expect(!f.consumer.active && !f.refs[0].focus(), "Offscreen slide was authorized with a window rectangle");
    f.consumer.document = {20.25F, 1.5F, 80.5F, 50.25F};
    expect(f.services.layout_and_synchronize({600, 800}, f.consumer.document), "Partial fractional clip failed");
    expect(f.consumer.active, "Actually visible fractional slide was rejected");
    f.disabled.set(true);
    sync(f);
    expect(!f.consumer.active, "Disabled Carousel target admitted");
    f.disabled.set(false);
    f.visible.set(false);
    sync(f);
    expect(!f.consumer.active, "Hidden Carousel target admitted");
    f.visible.set(true);
    f.services.set_window_active(false);
    sync(f);
    expect(!f.consumer.active, "Inactive window target admitted");
    f.services.set_window_active(true);
    sync(f);
    expect(f.consumer.active, "Resumed target did not recover");
    f.clean();
}

void real_motion() {
    for (const auto effect : {CarouselEffect::ScrollX, CarouselEffect::Fade}) {
        Slides f;
        f.buttons.set_motion_preference(animation::MotionPreference::normal);
        f.mount(true, effect);
        sync(f);
        f.consumer.target = f.editors[1];
        f.current.set(1);
        sync(f);
        expect(!f.consumer.active && !f.refs[1].focus() && f.services.carousel().snapshot(f.owner).transitioning,
               "Real zero-opacity/offscreen start was bypassed or animation advanced");
        f.services.tick_animations(animation::AnimationTime::microseconds(250000));
        sync(f);
        expect(f.consumer.active && f.refs[1].focus() && !f.refs[0].focus() &&
                   f.services.carousel().snapshot(f.owner).transitioning,
               "Real visible halfway current slide was not admitted");
        f.services.tick_animations(animation::AnimationTime::microseconds(500000));
        sync(f);
        expect(f.consumer.active && !f.services.carousel().snapshot(f.owner).transitioning,
               "Producer refresh changed completion/accepted current");
        f.clean();
    }
}

void nested() {
    Fixture f;
    Consumer consumer{f.services};
    Signal<std::size_t> outer{0};
    Signal<std::size_t> inner{0};
    std::array<InputRef, 3> refs;
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .animated(false)
                     .current(outer)
                     .slides(std::vector{
                         CarouselSlide{u8"outer-first", CarouselContent{[&] {
                                           Carousel(CarouselProps{}
                                                        .animated(false)
                                                        .current(inner)
                                                        .slides(std::vector{
                                                            CarouselSlide{u8"inner-first", CarouselContent{[&] {
                                                                              Input(InputProps{}.ref(refs[0]));
                                                                          }}},
                                                            CarouselSlide{u8"inner-second", CarouselContent{[&] {
                                                                              Input(InputProps{}.ref(refs[1]));
                                                                          }}}})
                                                        .layout(LayoutStyle{}.width(dp(200)).height(dp(80))));
                                       }}},
                         CarouselSlide{u8"outer-second", CarouselContent{[&] { Input(InputProps{}.ref(refs[2])); }}}})
                     .layout(LayoutStyle{}.width(dp(300)).height(dp(100))));
    }});
    consumer.target = f.inputs.mounted_inputs()[0].component;
    sync(f);
    expect(consumer.active && refs[0].focus(), "Nested initial producer path failed");
    consumer.target = f.inputs.mounted_inputs()[1].component;
    inner.set(1);
    expect(f.services.layout_and_synchronize({600, 800}, {0, 0, 600, 800}, {11.25F, 7.5F}),
           "Translated nested layout failed");
    expect(consumer.active && refs[1].focus() && !refs[0].focus(),
           "Nested producer path did not refresh outer to inner");
    outer.set(1);
    sync(f);
    expect(!consumer.active && !refs[1].focus(), "Nested helper bypassed inactive outer slide");
    outer.set(0);
    sync(f);
    expect(consumer.active && refs[1].focus(), "Nested target failed to recover in the same frame");
    f.services.dispose();
    expect(blocked_roots(f.services.interactions()).empty() && f.services.interactions().size() == 0 &&
               f.services.surfaces().size() == 0 && !f.services.next_frame_deadline(),
           "Nested private blocks leaked");
}

void callback_retirement() {
    Fixture f;
    Consumer consumer{f.services};
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    Signal<std::size_t> current{0};
    Signal<bool> modal_open{false};
    InputRef unrelated;
    runtime::ComponentId carousel;
    runtime::ComponentId modal;
    bool throwing{};
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(unrelated));
        Carousel(CarouselProps{}
                     .current(current)
                     .effect(CarouselEffect::Fade)
                     .easing(CubicBezier{0, 0, 1, 1})
                     .afterChange([&](auto) {
                         if (throwing) {
                             throw std::runtime_error("producer afterChange");
                         }
                     })
                     .slides(std::vector{CarouselSlide{u8"a", CarouselContent{[] { Input(InputProps{}); }}},
                                         CarouselSlide{u8"b", CarouselContent{[] { Input(InputProps{}); }}}})
                     .layout(LayoutStyle{}.width(dp(300)).height(dp(100))));
        Modal(ModalProps{}.open(modal_open), ModalContent{[] { Text(u8"independent Modal"); }});
    }});
    carousel = f.services.carousel().mounted().front();
    modal = f.services.modal().mounted().front();
    consumer.target = f.inputs.mounted_inputs()[1].component;
    sync(f);
    current.set(1);
    consumer.target = f.inputs.mounted_inputs()[2].component;
    sync(f);
    f.services.tick_animations(animation::AnimationTime::microseconds(250000));
    throwing = true;
    consumer.publication = [&] {
        modal_open.set(true);
    };
    bool caught{};
    try {
        sync(f);
    } catch (const std::runtime_error& error) {
        caught = std::string_view(error.what()) == "producer afterChange";
    }
    expect(caught && !f.services.components().contains(carousel) && f.services.components().contains(modal) &&
               f.inputs.mounted_inputs().size() == 1 && f.services.modal().snapshot(modal).visible,
           "Producer callback exception retired the wrong owner or swallowed its exception");
    f.services.dispose();
    const auto policy = f.services.interactions().input_constraint_diagnostics();
    expect(blocked_roots(f.services.interactions()).empty() && policy.leases == 0 && policy.companions == 0 &&
               f.services.interactions().size() == 0 && f.services.surfaces().size() == 0,
           "Producer callback retirement leaked policy/resources");
}

void retired_scope() {
    Slides f;
    f.mount();
    sync(f);
    f.services.components().scope(f.owner).dispose();
    const auto revision = f.services.interactions().input_boundary_revision();
    expect(!f.services.carousel().refresh_target_admission(f.editors[0], {600, 800}, {0, 0, 600, 800}) &&
               revision == f.services.interactions().input_boundary_revision() && !f.refs[0].focus(),
           "Retained storage of a retired producer scope republished admission");
    f.clean();
}

void retirement_and_idle() {
    Slides f;
    f.mount();
    sync(f);
    while (f.frames.consume_request()) {
    }
    const auto revision = f.services.interactions().input_boundary_revision();
    const auto frames = f.services.frame_request_revision();
    ryn_test::allocation::begin();
    bool stable = true;
    for (int i = 0; i < 2000; ++i) {
        stable = f.services.carousel().refresh_target_admission(f.editors[0], {600, 800}, {0, 0, 600, 800}) && stable;
    }
    const auto allocations = ryn_test::allocation::end();
    expect(stable && allocations == 0 && revision == f.services.interactions().input_boundary_revision() &&
               frames == f.services.frame_request_revision() && !f.services.next_frame_deadline(),
           "Stable producer query allocated, republished policy, or requested idle work");
    const auto old = f.services.carousel().snapshot(f.owner).slides[1];
    auto replacement = f.items.get();
    replacement[1] = CarouselSlide{u8"replacement", CarouselContent{[] { Text(u8"replacement"); }}};
    f.items.set(replacement);
    sync(f);
    expect(!f.services.components().contains(old) && std::ranges::find(blocked_roots(f.services.interactions()), old) ==
                                                         blocked_roots(f.services.interactions()).end(),
           "Removed private slide left its generation blocked");
    f.current.set(1);
    f.consumer.target = f.services.carousel().snapshot(f.owner).slides[1];
    f.consumer.publication = [&] {
        f.services.destroy(f.owner);
    };
    sync(f);
    expect(!f.consumer.stable && !f.consumer.active && !f.services.components().contains(f.owner),
           "Publication continued through producer/target retirement");
    f.clean();
}
} // namespace

int main() {
    try {
        same_frame();
        policies_and_clip();
        real_motion();
        nested();
        callback_retirement();
        retired_scope();
        retirement_and_idle();
        std::cout << "Carousel target admission checks=" << checks << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
