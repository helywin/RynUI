#pragma once

// Include in exactly one translation unit of an allocation-test executable.
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace ryn_test::allocation {
inline thread_local bool tracking = false;
inline thread_local std::size_t count = 0;
inline thread_local std::size_t requested_bytes = 0;
inline thread_local std::size_t largest_request = 0;
inline thread_local std::size_t fail_at = std::numeric_limits<std::size_t>::max();
inline thread_local std::size_t minimum_failure_size = 0;

inline void record(std::size_t size) {
    if (tracking) {
        requested_bytes += size;
        if (size > largest_request) {
            largest_request = size;
        }
        if (count++ == fail_at && size >= minimum_failure_size) {
            throw std::bad_alloc();
        }
    }
}

inline void begin(std::size_t fail = std::numeric_limits<std::size_t>::max(), std::size_t minimum_size = 0) {
    count = 0;
    requested_bytes = 0;
    largest_request = 0;
    fail_at = fail;
    minimum_failure_size = minimum_size;
    tracking = true;
}

inline std::size_t end() {
    tracking = false;
    return count;
}

inline void* allocate(std::size_t size) {
    record(size);
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) {
        return pointer;
    }
    throw std::bad_alloc();
}

inline void* aligned(std::size_t size, std::size_t alignment) {
    record(size);
#if defined(_MSC_VER)
    if (void* pointer = _aligned_malloc(size == 0 ? 1 : size, alignment)) {
        return pointer;
    }
#else
    void* pointer = nullptr;
    if (posix_memalign(&pointer, alignment, size == 0 ? 1 : size) == 0) {
        return pointer;
    }
#endif
    throw std::bad_alloc();
}

inline void free_aligned(void* pointer) {
#if defined(_MSC_VER)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}
} // namespace ryn_test::allocation

void* operator new(std::size_t size) {
    return ryn_test::allocation::allocate(size);
}

void* operator new[](std::size_t size) {
    return ryn_test::allocation::allocate(size);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return ryn_test::allocation::aligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ryn_test::allocation::aligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* pointer, std::align_val_t) noexcept {
    ryn_test::allocation::free_aligned(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept {
    ryn_test::allocation::free_aligned(pointer);
}

void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    ryn_test::allocation::free_aligned(pointer);
}

void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    ryn_test::allocation::free_aligned(pointer);
}
