#include "input/pointer_router.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/component.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>
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
    std::uint64_t now_milliseconds() const noexcept override { return now; }
    bool poll_frame_event() noexcept override { return false; }
    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now += timeout;
        return false;
    }

    std::uint64_t now{0};
};

class CountingSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    ryn::runtime::FrameSubmissionResult submit_frame() override {
        ++submissions;
        return ryn::runtime::FrameSubmissionResult::submitted;
    }

    int submissions{0};
};

ryn::input::PointerInputEvent event(
    ryn::input::PointerAction action,
    ryn::input::PointerButton button = ryn::input::PointerButton::none,
    float x = 20.0F) {
    return {
        ryn::input::PointerIdentity::mouse(),
        action,
        button,
        x,
        20.0F,
    };
}

} // namespace

int main() {
    try {
        constexpr std::size_t measured_moves = 10'000;
        ryn::runtime::NodeStore nodes;
        ryn::runtime::ComponentHost components(nodes);
        ryn::runtime::ComponentId component;
        components.mount(ryn::Content{[&] { component = mount_leaf(); }});
        const auto node = components.root(component);
        nodes.require(node).bounds = {0.0F, 0.0F, 100.0F, 100.0F};
        nodes.require(node).place_generation = 1;

        ryn::runtime::FrameRequestState frames;
        ryn::runtime::DirtyQueues dirty(nodes, &frames);
        ryn::runtime::NodePropertyWriter properties(nodes, dirty);
        ryn::input::InteractionRegistry registry(components, nodes);
        registry.reserve(1);
        ryn::input::InteractionHandlers handlers;
        handlers.target = [&](ryn::input::PointerDispatchContext& pointer) {
            switch (pointer.kind()) {
            case ryn::input::PointerEventKind::enter:
                static_cast<void>(properties.set_color(
                    node, {0.2F, 0.4F, 0.8F, 1.0F}));
                break;
            case ryn::input::PointerEventKind::down:
                static_cast<void>(properties.set_color(
                    node, {0.1F, 0.2F, 0.5F, 1.0F}));
                static_cast<void>(pointer.capture_pointer());
                break;
            case ryn::input::PointerEventKind::up:
                static_cast<void>(properties.set_color(
                    node, {0.2F, 0.4F, 0.8F, 1.0F}));
                break;
            case ryn::input::PointerEventKind::leave:
                static_cast<void>(properties.set_color(
                    node, {1.0F, 1.0F, 1.0F, 1.0F}));
                break;
            case ryn::input::PointerEventKind::move:
            case ryn::input::PointerEventKind::cancel:
                break;
            }
        };
        const auto interaction = registry.create({
            component,
            node,
            std::nullopt,
            true,
            true,
            std::move(handlers),
        });
        ryn::input::HitTestSnapshot hit_test(registry, nodes);
        hit_test.reserve(1);
        const ryn::input::HitTestPaintEntry paint_entry{interaction, std::nullopt};
        hit_test.rebuild(
            std::span<const ryn::input::HitTestPaintEntry>{&paint_entry, 1},
            {0.0F, 0.0F, 100.0F, 100.0F});
        ryn::input::PointerRouter router(registry, hit_test, &frames);
        router.reserve(1, 4);

        router.dispatch(event(ryn::input::PointerAction::move));
        require(frames.consume_request(), "initial hover did not request a frame");
        require(dirty.material_nodes().size() == 1,
                "hover did not invalidate the Material phase");
        dirty.clear();
        const auto requests_before = frames.counters().requests;
        const auto route_count_before = router.diagnostics().routes_dispatched;

        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (std::size_t index = 0; index < measured_moves; ++index) {
            router.dispatch(event(
                ryn::input::PointerAction::move,
                ryn::input::PointerButton::none,
                20.0F + static_cast<float>(index % 10)));
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);

        const auto allocations = allocation_probe::count.load(std::memory_order_relaxed);
        require(allocations == 0, "steady-state pointer move allocated");
        require(frames.counters().requests == requests_before && !frames.pending(),
                "stable pointer move requested continuous frames");
        require(router.diagnostics().routes_dispatched
                    == route_count_before + measured_moves,
                "steady-state pointer move skipped routes");

        router.dispatch(event(
            ryn::input::PointerAction::down,
            ryn::input::PointerButton::primary));
        require(frames.pending(), "pointer press did not request a frame");
        require(router.state(ryn::input::PointerIdentity::mouse())->capture
                    == interaction,
                "pointer press handler did not capture");
        require(dirty.material_nodes().size() == 1,
                "press did not invalidate the Material phase");
        static_cast<void>(frames.consume_request());
        dirty.clear();
        router.dispatch(event(ryn::input::PointerAction::move));
        require(!frames.pending(),
                "stable captured move requested an unnecessary frame");
        router.dispatch(event(
            ryn::input::PointerAction::up,
            ryn::input::PointerButton::primary));
        require(frames.pending(), "pointer release did not request a frame");
        require(dirty.material_nodes().size() == 1,
                "release did not invalidate the Material phase");

        IdleEvents idle_events;
        CountingSubmitter submitter;
        ryn::runtime::OnDemandFrameLoop loop(frames, idle_events, submitter, 1);
        require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
                "pointer release frame was not submitted");
        for (int index = 0; index < 120; ++index) {
            require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                    "stable pointer state did not return to idle");
        }
        require(submitter.submissions == 1,
                "stable pointer state submitted continuously");

        std::cout
            << "pointer_moves=" << measured_moves
            << " allocations=" << allocations
            << " routes=" << router.diagnostics().routes_dispatched
            << " frame_requests=" << router.diagnostics().frame_requests
            << " idle_submissions=" << submitter.submissions
            << '\n';
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
