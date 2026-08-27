#include "platform/sdl/platform_state.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

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

using ryn::detail::PlatformApi;
using ryn::detail::PlatformConfig;
using ryn::detail::PlatformEvents;
using ryn::detail::PlatformGpuDeviceHandle;
using ryn::detail::PlatformState;
using ryn::detail::PlatformWindowHandle;
using ryn::detail::PlatformWindowMetrics;
using ryn::input::Key;
using ryn::input::KeyAction;
using ryn::input::KeyModifier;
using ryn::input::KeyboardInputEvent;
using ryn::input::PointerAction;
using ryn::input::PointerButton;
using ryn::input::PointerIdentity;
using ryn::input::PointerInputEvent;
using ryn::input::WindowInputAction;
using ryn::input::WindowInputEvent;

class FakePlatformApi final : public PlatformApi {
public:
    bool init_video() override { return true; }
    void quit() noexcept override {}

    PlatformWindowHandle create_window(const char*, int, int, bool) override {
        return &window_token_;
    }

    void destroy_window(PlatformWindowHandle) noexcept override {}

    PlatformGpuDeviceHandle create_gpu_device(bool) override {
        return &device_token_;
    }

    void destroy_gpu_device(PlatformGpuDeviceHandle) noexcept override {}

    bool claim_window(PlatformGpuDeviceHandle, PlatformWindowHandle) override {
        return true;
    }

    void release_window(PlatformGpuDeviceHandle, PlatformWindowHandle) noexcept override {}
    [[nodiscard]] const char* last_error() const noexcept override { return ""; }
    [[nodiscard]] const char* gpu_driver(PlatformGpuDeviceHandle) const noexcept override {
        return "fake";
    }
    [[nodiscard]] PlatformWindowMetrics window_metrics(
        PlatformWindowHandle) const noexcept override {
        return {960, 640, 960, 640, 1.0F, 1.0F};
    }
    void delay(std::uint32_t) noexcept override {}

    void poll_events(PlatformWindowHandle, PlatformEvents& result) override {
        ++poll_calls;
        const auto coordinate = static_cast<float>(poll_calls);
        static_cast<void>(result.input.append(PointerInputEvent{
            PointerIdentity::mouse(),
            PointerAction::move,
            PointerButton::none,
            coordinate,
            coordinate,
        }));
        static_cast<void>(result.input.append(PointerInputEvent{
            PointerIdentity::mouse(),
            PointerAction::move,
            PointerButton::none,
            coordinate + 1.0F,
            coordinate + 1.0F,
        }));
        static_cast<void>(result.input.append(KeyboardInputEvent{
            Key::space,
            KeyAction::down,
            KeyModifier::none,
            false,
        }));
        result.frame_requested = true;
        result.suppressed_compatibility_mouse_events = 1;
    }

    void wait_events(
        PlatformWindowHandle,
        std::uint32_t,
        PlatformEvents& result) override {
        ++wait_calls;
        static_cast<void>(result.input.append(WindowInputEvent{
            WindowInputAction::focus_lost,
            0,
            0,
        }));
        result.frame_requested = true;
    }

    std::uint64_t poll_calls{0};
    std::uint64_t wait_calls{0};

private:
    int window_token_{0};
    int device_token_{0};
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_owner_thread_pump_reuses_storage_without_allocation() {
    FakePlatformApi api;
    auto created = PlatformState::create(api, PlatformConfig{});
    require(static_cast<bool>(created), "platform creation failed");
    auto& state = *created.state;

    const auto& warmup = state.poll_events();
    require(warmup.input.size() == 2, "pump did not preserve key/move boundary");
    require(warmup.input.coalesced_move_count() == 1,
            "pump did not diagnose consecutive move coalescing");
    const auto stable_capacity = warmup.input.capacity();

    constexpr std::uint64_t measured_polls = 1'000;
    allocation_probe::count.store(0, std::memory_order_relaxed);
    allocation_probe::tracking.store(true, std::memory_order_relaxed);
    for (std::uint64_t index = 0; index < measured_polls; ++index) {
        static_cast<void>(state.poll_events());
    }
    allocation_probe::tracking.store(false, std::memory_order_relaxed);

    require(allocation_probe::count.load(std::memory_order_relaxed) == 0,
            "steady-state event pump allocated");
    require(state.poll_events().input.capacity() == stable_capacity,
            "event pump did not retain stable batch capacity");

    const auto diagnostics = state.event_diagnostics();
    const auto expected_polls = measured_polls + 2;
    require(diagnostics.poll_calls == expected_polls, "poll diagnostic count differs");
    require(diagnostics.normalized_input_events == expected_polls * 3,
            "normalized input diagnostic count differs");
    require(diagnostics.coalesced_pointer_moves == expected_polls,
            "move coalescing diagnostic count differs");
    require(diagnostics.suppressed_compatibility_mouse_events == expected_polls,
            "compatibility suppression diagnostic count differs");
    require(diagnostics.frame_requested_pumps == expected_polls,
            "frame request diagnostic count differs");
}

void test_wrong_thread_is_rejected_without_mutation() {
    FakePlatformApi api;
    auto created = PlatformState::create(api, PlatformConfig{});
    require(static_cast<bool>(created), "platform creation failed");
    auto& state = *created.state;
    static_cast<void>(state.poll_events());
    const auto before = state.event_diagnostics();
    const auto api_calls_before = api.poll_calls;

    std::atomic<bool> rejected{false};
    std::thread worker([&] {
        try {
            static_cast<void>(state.poll_events());
        } catch (const std::logic_error&) {
            rejected.store(true, std::memory_order_relaxed);
        }
    });
    worker.join();

    require(rejected.load(std::memory_order_relaxed),
            "non-owner event pump was not rejected");
    require(api.poll_calls == api_calls_before,
            "non-owner event pump reached the platform API");
    require(state.event_diagnostics() == before,
            "non-owner event pump mutated diagnostics");
}

void test_wait_pump_uses_the_same_batch_and_diagnostics() {
    FakePlatformApi api;
    auto created = PlatformState::create(api, PlatformConfig{});
    require(static_cast<bool>(created), "platform creation failed");
    auto& state = *created.state;

    const auto& waited = state.wait_events(5);
    require(waited.input.size() == 1, "wait pump lost normalized input");
    require(std::get<WindowInputEvent>(waited.input.events().front()).action
                == WindowInputAction::focus_lost,
            "wait pump event differs");
    const auto diagnostics = state.event_diagnostics();
    require(diagnostics.wait_calls == 1 && diagnostics.poll_calls == 0,
            "wait diagnostics were counted as poll calls");
    require(diagnostics.normalized_input_events == 1,
            "wait normalized input count differs");
}

} // namespace

int main() {
    try {
        test_owner_thread_pump_reuses_storage_without_allocation();
        test_wrong_thread_is_rejected_without_mutation();
        test_wait_pump_uses_the_same_batch_and_diagnostics();
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
