#include "gallery_document_viewport.hpp"

#include "runtime/frame_scheduler.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

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

void* operator new(std::size_t size) { return allocation_probe::allocate(size); }
void* operator new[](std::size_t size) { return allocation_probe::allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(
        size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(
        size, static_cast<std::size_t>(alignment));
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

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_steady_wheel_and_navigation_paths_reuse_capacity() {
    using namespace rynui::example;
    GalleryDocumentViewport document;
    document.set_extents(600.0F, 100000.0F);
    constexpr std::array<float, 6> sections{
        0.0F, 1000.0F, 2000.0F, 3000.0F, 4000.0F, 5000.0F};
    constexpr std::array<float, 7> categories{
        6000.0F, 7000.0F, 8000.0F, 9000.0F,
        10000.0F, 11000.0F, 12000.0F};
    document.replace_anchors(sections);
    document.replace_category_anchors(categories);

    ryn::runtime::FrameRequestState frames;
    ryn::runtime::NodeStore nodes;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    const auto root = nodes.create_root();
    for (std::size_t index = 0; index < 72; ++index) {
        static_cast<void>(nodes.create_child(root));
    }

    document.scroll_to(1.0F);
    require(document.apply_subtree_translation(root, nodes, dirty),
            "Gallery benchmark warmup translation failed");
    dirty.clear();
    static_cast<void>(frames.consume_request());

    std::array<GalleryDocumentAnchorId, 7> category_ids{};
    for (std::size_t index = 0; index < category_ids.size(); ++index) {
        category_ids[index] = *document.category_anchor(
            static_cast<AntDesignGalleryCategory>(index));
    }

    constexpr std::size_t wheel_iterations = 1000;
    constexpr std::size_t navigation_iterations = 1000;
    allocation_probe::count.store(0, std::memory_order_relaxed);
    allocation_probe::tracking.store(true, std::memory_order_relaxed);
    for (std::size_t index = 0; index < wheel_iterations; ++index) {
        static_cast<void>(document.scroll_by(1.0F));
        static_cast<void>(document.apply_subtree_translation(root, nodes, dirty));
        require(dirty.transform_nodes().size() == 73
                    && dirty.hit_test_nodes().size() == 73
                    && dirty.layout_roots().empty()
                    && dirty.material_nodes().empty()
                    && dirty.text_nodes().empty()
                    && dirty.animation_nodes().empty(),
                "steady wheel path dirtied an unrelated phase");
        dirty.clear();
        require(frames.consume_request(),
                "steady wheel path did not request exactly one frame batch");
    }
    for (std::size_t index = 0; index < navigation_iterations; ++index) {
        static_cast<void>(document.jump_to(category_ids[index % category_ids.size()]));
        static_cast<void>(document.apply_subtree_translation(root, nodes, dirty));
        dirty.clear();
        require(frames.consume_request(),
                "steady navigation path did not request a frame");
    }
    allocation_probe::tracking.store(false, std::memory_order_relaxed);

    require(allocation_probe::count.load(std::memory_order_relaxed) == 0,
            "steady Gallery wheel/navigation path allocated");
    const auto before_idle = document.diagnostics();
    require(document.apply_subtree_translation(root, nodes, dirty)
                && dirty.transform_nodes().empty()
                && dirty.hit_test_nodes().empty()
                && !frames.pending()
                && document.diagnostics().translation_passes
                    == before_idle.translation_passes,
            "idle Gallery repeated subtree work or requested another frame");
    require(document.diagnostics().navigation_jumps == navigation_iterations
                && document.diagnostics().translation_passes
                    == wheel_iterations + navigation_iterations + 1
                && document.diagnostics().translated_nodes
                    == (wheel_iterations + navigation_iterations + 1) * 73,
            "Gallery document diagnostics lost steady-state work identity");
}

} // namespace

int main() {
    try {
        test_steady_wheel_and_navigation_paths_reuse_capacity();
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
