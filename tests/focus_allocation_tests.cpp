#include "input/focus_manager.hpp"

#include <ryn/component.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <utility>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace allocation_probe {

std::atomic<bool> tracking{false};
std::atomic<std::size_t> count{0};

void record() noexcept {
    if (tracking.load(std::memory_order_relaxed)) {
        count.fetch_add(1, std::memory_order_relaxed);
    }
}

void* allocate(std::size_t size) {
    record();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* allocate_aligned(std::size_t size, std::size_t alignment) {
    record();
#if defined(_MSC_VER)
    if (void* memory = _aligned_malloc(size == 0 ? 1 : size, alignment)) {
        return memory;
    }
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size == 0 ? 1 : size) == 0) {
        return memory;
    }
#endif
    throw std::bad_alloc();
}

void deallocate_aligned(void* memory) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

} // namespace allocation_probe

void* operator new(std::size_t size) {
    return allocation_probe::allocate(size);
}

void* operator new[](std::size_t size) {
    return allocation_probe::allocate(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

void operator delete[](void* memory, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

namespace {

struct TestState final {};

ryn::runtime::ComponentId mount_leaf() {
    return ryn::runtime::require_component_build_context()
        .mount_component<TestState>();
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class IdleEvents final : public ryn::runtime::FrameEventSource {
public:
    ryn::animation::AnimationTime now() const noexcept override {
        return ryn::animation::AnimationTime::microseconds(
            static_cast<std::int64_t>(now_milliseconds_) * 1000);
    }
    bool poll_frame_event() noexcept override { return false; }
    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now_milliseconds_ += timeout;
        return false;
    }

    std::uint64_t now_milliseconds_{0};
};

class CountingSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime) override {
        ++submissions;
        return ryn::runtime::FrameSubmissionResult::submitted;
    }

    int submissions{0};
};

ryn::input::KeyboardInputEvent key(
    ryn::input::Key value,
    ryn::input::KeyAction action,
    bool repeat = false) {
    return {value, action, ryn::input::KeyModifier::none, repeat};
}

} // namespace

int main() {
    try {
        constexpr std::size_t measured_events = 10'000;
        ryn::runtime::NodeStore nodes;
        ryn::runtime::ComponentHost components(nodes);
        std::array<ryn::runtime::ComponentId, 3> component_ids;
        components.mount(ryn::Content{[&] {
            for (auto& component : component_ids) {
                component = mount_leaf();
            }
        }});
        ryn::input::InteractionRegistry registry(components, nodes);
        registry.reserve(component_ids.size());
        std::array<ryn::input::InteractionId, 3> interactions;
        int activations = 0;
        for (std::size_t index = 0; index < interactions.size(); ++index) {
            interactions[index] = registry.create({
                component_ids[index],
                components.root(component_ids[index]),
                std::nullopt,
                true,
                true,
                {},
            });
            ryn::input::FocusHandlers handlers;
            handlers.state_changed = [](ryn::input::FocusPresentation) {};
            handlers.activate = [&] { ++activations; };
            registry.set_focus_handlers(interactions[index], std::move(handlers));
        }

        ryn::runtime::FrameRequestState frames;
        ryn::input::FocusManager focus(registry, &frames);
        focus.reserve(interactions.size());
        focus.dispatch(key(ryn::input::Key::tab, ryn::input::KeyAction::down));
        static_cast<void>(frames.consume_request());

        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (std::size_t index = 0; index < measured_events; ++index) {
            focus.dispatch(key(ryn::input::Key::tab, ryn::input::KeyAction::down));
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        const auto traversal_allocations = allocation_probe::count.load(
            std::memory_order_relaxed);
        require(traversal_allocations == 0,
                "steady-state focus traversal allocated");
        require(frames.consume_request(),
                "focus traversal did not request a coalesced frame");

        const auto requests_before = frames.counters().requests;
        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (std::size_t index = 0; index < measured_events; ++index) {
            focus.dispatch(key(
                ryn::input::Key::enter,
                ryn::input::KeyAction::down,
                true));
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        const auto repeat_allocations = allocation_probe::count.load(
            std::memory_order_relaxed);
        require(repeat_allocations == 0,
                "steady-state repeated keyboard input allocated");
        require(frames.counters().requests == requests_before
                    && !frames.pending() && activations == 0,
                "ignored key repeat requested a frame or activation");

        focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::down));
        focus.dispatch(key(ryn::input::Key::space, ryn::input::KeyAction::up));
        require(frames.pending() && activations == 1,
                "Space activation did not request its necessary frame");

        IdleEvents events;
        CountingSubmitter submitter;
        ryn::runtime::OnDemandFrameLoop loop(frames, events, submitter, 1);
        require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
                "keyboard state frame was not submitted");
        for (int index = 0; index < 120; ++index) {
            require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                    "stable keyboard state did not return to idle");
        }
        require(submitter.submissions == 1,
                "stable keyboard state submitted continuously");

        std::cout
            << "focus_events=" << measured_events
            << " traversal_allocations=" << traversal_allocations
            << " repeat_allocations=" << repeat_allocations
            << " activations=" << activations
            << " idle_submissions=" << submitter.submissions
            << '\n';
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
