#include "graphics/image_scene.hpp"
#include "renderer/common/image_gpu_resources.hpp"
#include <iostream>
#include <set>
using namespace ryn;

namespace {
int checks{};

void check(bool v, const char* m) {
    ++checks;
    if (!v) {
        throw std::runtime_error(m);
    }
}

struct Api final : detail::ImageGpuApi {
    int creates{};
    int uploads{};
    int releases{};
    int rejected{};
    bool fail_texture{};
    bool fail_buffer{};
    std::set<void*> textures;
    std::set<void*> buffers;
    std::vector<char> order;

    ~Api() {
        reset();
    }

    void reset() {
        for (auto p : textures) {
            delete static_cast<int*>(p);
        }
        for (auto p : buffers) {
            delete static_cast<int*>(p);
        }
        textures.clear();
        buffers.clear();
    }

    void* create_image_texture(std::uint32_t, std::uint32_t) override {
        auto p = new int(++creates);
        textures.insert(p);
        return p;
    }

    void* create_image_buffer(std::size_t) override {
        auto p = new int(0);
        buffers.insert(p);
        return p;
    }

    bool upload_image_texture(void*, const detail::ImageTextureUpload&) override {
        ++uploads;
        if (fail_texture) {
            fail_texture = false;
            return false;
        }
        return true;
    }

    bool upload_image_buffer(void*, std::size_t, std::span<const std::byte>) override {
        if (fail_buffer) {
            fail_buffer = false;
            return false;
        }
        return true;
    }

    void release_image_texture(void* p) noexcept override {
        order.push_back('t');
        if (textures.erase(p)) {
            ++releases;
            delete static_cast<int*>(p);
        } else {
            ++rejected;
        }
    }

    void release_image_buffer(void* p) noexcept override {
        order.push_back('b');
        if (buffers.erase(p)) {
            delete static_cast<int*>(p);
        } else {
            ++rejected;
        }
    }
};

void run() {
    const std::array bytes{std::byte{255}, std::byte{64}, std::byte{128}, std::byte{128}};
    auto pixels = ImagePixels::copy(1, 1, 4, bytes);
    graphics::ImageInstance original{pixels, {5.25F, 7.5F, 100, 60}, {10, 9, 70, 45}, {55.25F, 37.5F}};
    const runtime::Rect canvas{10.25F, 9.75F, 70.5F, 45.5F};
    for (bool top : {false, true}) {
        for (float radius : {0.F, 8.F, 1000.F}) {
            const auto pieces = graphics::decompose_rounded_image(original, canvas, radius, top);
            check(!pieces.empty() && pieces.size() <= 7, "rounded Image piece bounds invalid");
            for (std::size_t i = 0; i < pieces.size(); ++i) {
                const auto& p = pieces[i];
                check(p.pivot == original.pivot && p.pixels == pixels, "rounded Image lost full-image pivot/source");
                if (radius == 0) {
                    continue;
                }
                const float u = (p.bounds.x - original.bounds.x) / original.bounds.width;
                check(std::abs(p.source_uv.x - u) < .00001F, "rounded Image UV is not relative to full image");
                auto flipped = p;
                flipped.flip_x = true;
                const auto packed = detail::pack_image_instance(flipped, {160, 100, 1.25F});
                check(packed.uv_rect[0] == 1 - p.source_uv.x, "rounded Image flip lost UV subrange");
                for (std::size_t j = 0; j < i; ++j) {
                    const auto q = pieces[j].bounds;
                    const auto r = p.bounds;
                    const float w = std::min(q.x + q.width, r.x + r.width) - std::max(q.x, r.x);
                    const float h = std::min(q.y + q.height, r.y + r.height) - std::max(q.y, r.y);
                    check(w <= 0 || h <= 0, "rounded Image geometries overlap");
                }
            }
        }
    }
    for (float width : {.01F, .25F, 1.F, 17.25F, 99.75F, 333.333F, 901.125F, 1440.F}) {
        for (float height : {.01F, 3.5F, 67.25F, 200.F}) {
            const runtime::Rect canvas{184.25F, 130.5F, width, height};
            const float scale = std::max(width / 3.F, height / 2.F);
            auto source = original;
            source.bounds = {canvas.x + (width - 3 * scale) / 2, canvas.y + (height - 2 * scale) / 2, 3 * scale,
                             2 * scale};
            const auto parts = graphics::decompose_rounded_image(source, canvas, 8);
            for (std::size_t i = 0; i < parts.size(); ++i) {
                for (std::size_t j = 0; j < i; ++j) {
                    const auto a = parts[i].bounds;
                    const auto b = parts[j].bounds;
                    check(std::min(a.x + a.width, b.x + b.width) <= std::max(a.x, b.x) ||
                              std::min(a.y + a.height, b.y + b.height) <= std::max(a.y, b.y),
                          "narrow fractional rounded patches overlap");
                }
            }
            for (const auto& piece : parts) {
                graphics::validate_image_instance(piece);
                check(piece.source_uv.x + piece.source_uv.width <= 1 && piece.source_uv.y + piece.source_uv.height <= 1,
                      "fractional/narrow cover source UV escaped immutable source");
            }
        }
    }
    check(graphics::decompose_rounded_image(original, {0, 0, 0, 1}, 8).empty(),
          "zero rounded canvas retained drawable geometry");
    graphics::ImageStore store;
    std::vector<graphics::ImageId> ids;
    const auto pieces = graphics::decompose_rounded_image(original, canvas, 8);
    for (const auto& p : pieces) {
        ids.push_back(store.create(p));
    }
    Api api;
    {
        detail::ImageGpuResources resources{api};
        resources.synchronize(&store, {160, 100, 1});
        check(api.creates == 1 && api.uploads == 1 && api.textures.size() == 1,
              "pieces duplicated GPU texture/source upload");
        const auto texture = resources.texture(ids[1].index);
        check(store.destroy(ids[0]), "piece deletion failed");
        resources.synchronize(&store, {160, 100, 1});
        check(resources.texture(ids[1].index) == texture && api.releases == 0,
              "deleting one lease invalidated remaining pieces");
        resources.invalidate_upload();
        resources.synchronize(&store, {160, 100, 1});
        check(api.uploads == 2, "shared source invalidation uploaded once per piece");
        resources.synchronize(&store, {160, 100, 2});
        check(api.uploads == 2, "DPI-only change reuploaded shared pixels");
        const auto replacement = ImagePixels::copy(1, 1, 4, bytes);
        for (std::size_t i = 1; i < ids.size(); ++i) {
            auto p = store.find(ids[i])->instance;
            p.pixels = replacement;
            store.update(ids[i], p);
        }
        api.fail_texture = true;
        bool failed = false;
        try {
            resources.synchronize(&store, {160, 100, 2});
        } catch (...) {
            failed = true;
        }
        check(failed, "source upload failure hidden");
        resources.synchronize(&store, {160, 100, 2});
        check(api.creates == 2 && api.textures.size() == 1, "failed source retry leaked/duplicated textures");
        api.fail_buffer = true;
        auto p = store.find(ids[1])->instance;
        p.opacity = .5F;
        store.update(ids[1], p);
        failed = false;
        try {
            resources.synchronize(&store, {160, 100, 2});
        } catch (...) {
            failed = true;
        }
        const auto uploads = api.uploads;
        check(failed, "instance upload failure hidden");
        resources.synchronize(&store, {160, 100, 2});
        check(api.uploads == uploads, "buffer retry reuploaded immutable source");
        api.reset();
        resources.abandon_device();
        resources.synchronize(&store, {160, 100, 2});
        check(api.textures.size() == 1 && api.rejected == 0, "device abandon reused/released stale texture");
        for (auto id : ids) {
            store.destroy(id);
            store.destroy(id);
        }
        resources.synchronize(&store, {160, 100, 2});
        check(api.textures.empty(), "retired pieces retained GPU source");
    }
    check(api.buffers.empty() && api.rejected == 0, "resource destructor ownership/order invalid");
    Api order_api;
    {
        graphics::ImageStore live;
        live.create(original);
        detail::ImageGpuResources resources{order_api};
        resources.synchronize(&live, {160, 100, 1});
    }
    check(order_api.order == std::vector<char>{'b', 't'},
          "Image instance buffer must retire before final texture lease");
}
} // namespace

int main() {
    try {
        run();
        std::cout << checks << " Image rounding/resource checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
