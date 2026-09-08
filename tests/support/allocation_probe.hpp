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
inline thread_local std::size_t fail_at = std::numeric_limits<std::size_t>::max();
inline void record() {
    if(tracking && count++ == fail_at) { throw std::bad_alloc(); }
}
inline void begin(std::size_t fail = std::numeric_limits<std::size_t>::max()) {
    count = 0; fail_at = fail; tracking = true;
}
inline std::size_t end() { tracking = false; return count; }
inline void* allocate(std::size_t size) {
    record();
    if(void* pointer = std::malloc(size == 0 ? 1 : size)) { return pointer; }
    throw std::bad_alloc();
}
inline void* aligned(std::size_t size, std::size_t alignment) {
    record();
#if defined(_MSC_VER)
    if(void* pointer = _aligned_malloc(size == 0 ? 1 : size, alignment)) { return pointer; }
#else
    void* pointer = nullptr;
    if(posix_memalign(&pointer, alignment, size == 0 ? 1 : size) == 0) { return pointer; }
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
}
void* operator new(std::size_t size) { return ryn_test::allocation::allocate(size); }
void* operator new[](std::size_t size) { return ryn_test::allocation::allocate(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    return ryn_test::allocation::aligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ryn_test::allocation::aligned(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* pointer, std::align_val_t) noexcept { ryn_test::allocation::free_aligned(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { ryn_test::allocation::free_aligned(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { ryn_test::allocation::free_aligned(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { ryn_test::allocation::free_aligned(pointer); }
