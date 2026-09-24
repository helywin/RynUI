#include "input/pressable_behavior.hpp"

#include <atomic>
#include <cstdlib>
#include <new>
#include <stdexcept>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};
}

void* operator new(std::size_t size) {
    if (tracking.load(std::memory_order_relaxed)) {
        allocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* result = std::malloc(size == 0 ? 1 : size)) return result;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

int main() {
    ryn::input::PressableBehavior press;
    const auto pointer = ryn::input::PointerIdentity::mouse();
    const ryn::input::InteractionId target{1, 1};
    constexpr int iterations = 100'000;
    int activations = 0;
    tracking.store(true, std::memory_order_relaxed);
    for (int index = 0; index < iterations; ++index) {
        const auto down = press.begin(pointer, target, true, true);
        const auto up = press.release(pointer, target, true, target, target);
        if (!down.pressed_changed || !up.pressed_changed || !up.activate
                || press.pressed()) {
            tracking.store(false, std::memory_order_relaxed);
            throw std::runtime_error("Pressable steady-state gesture changed behavior");
        }
        ++activations;
    }
    tracking.store(false, std::memory_order_relaxed);
    if (activations != iterations || allocations.load(std::memory_order_relaxed) != 0) {
        throw std::runtime_error("Pressable steady-state gesture allocated memory");
    }
}
