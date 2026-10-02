#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/gpu_binding.hpp"
#include "graphics/quad_primitive.hpp"
#include "renderer/common/scene_resources.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ryn::detail::PlatformApi;
using ryn::detail::PlatformConfig;
using ryn::detail::GpuDeviceHandle;
using ryn::detail::GpuBindingApi;
using ryn::detail::SdlGpuBinding;
using ryn::detail::PlatformStage;
using ryn::detail::PlatformState;
using ryn::detail::PlatformWindowHandle;
using ryn::detail::PlatformWindowMetrics;

enum class FailurePoint {
    none,
    init,
    window,
    device,
    claim,
};

class FakePlatformApi final : public PlatformApi, public GpuBindingApi {
public:
    explicit FakePlatformApi(FailurePoint failure) : failure_(failure) {}

    bool init_video() override {
        calls.emplace_back("init");
        return failure_ != FailurePoint::init;
    }

    void quit() noexcept override {
        calls.emplace_back("quit");
    }

    PlatformWindowHandle create_window(const char*, int, int, bool high_pixel_density) override {
        calls.emplace_back("create_window");
        high_pixel_density_requested = high_pixel_density;
        return failure_ == FailurePoint::window ? nullptr : &window_token_;
    }

    void destroy_window(PlatformWindowHandle) noexcept override {
        calls.emplace_back("destroy_window");
    }

    GpuDeviceHandle create_gpu_device(bool) override {
        calls.emplace_back("create_device");
        device_live = failure_ != FailurePoint::device;
        return failure_ == FailurePoint::device ? nullptr : &device_token_;
    }

    void destroy_gpu_device(GpuDeviceHandle) noexcept override {
        calls.emplace_back("destroy_device");
        device_live = false;
    }

    bool claim_window(GpuDeviceHandle, PlatformWindowHandle) override {
        calls.emplace_back("claim_window");
        return failure_ != FailurePoint::claim;
    }

    void release_window(GpuDeviceHandle, PlatformWindowHandle) noexcept override {
        calls.emplace_back("release_window");
    }

    [[nodiscard]] const char* last_error() const noexcept override {
        return "injected failure";
    }

    [[nodiscard]] const char* gpu_driver(GpuDeviceHandle) const noexcept override {
        return "fake-gpu";
    }

    [[nodiscard]] PlatformWindowMetrics window_metrics(
        PlatformWindowHandle) const noexcept override {
        return {960, 720, 1200, 900, 1.25F, 1.25F};
    }

    void delay(std::uint32_t) noexcept override {}

    std::vector<std::string> calls;
    bool high_pixel_density_requested{false};
    bool device_live{};

private:
    FailurePoint failure_;
    int window_token_{0};
    int device_token_{0};
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_calls(
    const std::vector<std::string>& actual,
    const std::vector<std::string>& expected) {
    if (actual != expected) {
        std::cerr << "Expected:";
        for (const auto& call : expected) {
            std::cerr << ' ' << call;
        }
        std::cerr << "\nActual:";
        for (const auto& call : actual) {
            std::cerr << ' ' << call;
        }
        std::cerr << '\n';
        throw std::runtime_error("lifecycle call order differs");
    }
}

void test_failure(
    FailurePoint point,
    PlatformStage expected_stage,
    const std::vector<std::string>& expected_calls) {
    FakePlatformApi api(point);
    const auto result = PlatformState::create(api, PlatformConfig{});
    require(!result, "injected failure unexpectedly succeeded");
    require(result.error.has_value(), "failure did not include an error");
    require(result.error->stage == expected_stage, "failure stage differs");
    require(result.error->message == "injected failure", "SDL error was not preserved");
    require_calls(api.calls, expected_calls);
}

void test_success_cleanup() {
    FakePlatformApi api(FailurePoint::none);
    {
        auto result = PlatformState::create(api, PlatformConfig{});
        require(static_cast<bool>(result), "valid lifecycle failed");
        require(!result.error.has_value(), "successful lifecycle included an error");
        require(result.state->window() != nullptr, "window handle is missing");
        require(result.state->display_scale() == 1.25F, "display scale differs");
        require(result.state->window_metrics().logical_width() == 960.0F,
                "logical viewport width differs");
        require(api.high_pixel_density_requested,
                "platform did not request a high-pixel-density window");
        require(result.state->is_owner_thread(), "lifecycle lost its owner thread");
        require_calls(
            api.calls,
            {"init", "create_window"});
    }
    require_calls(
        api.calls,
        {"init",
         "create_window",
         "destroy_window",
         "quit"});
}

void test_gpu_failure_preserves_host(FailurePoint point) {
    FakePlatformApi api(point);
    auto host = PlatformState::create(api, {});
    require(bool(host), "GPU failure prevented host creation");
    try {
        SdlGpuBinding binding(*host.state, api);
        throw std::logic_error("GPU binding unexpectedly succeeded");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("injected failure") != std::string::npos,
                "binding lost GPU error");
    }
    static_cast<void>(host.state->poll_events());
    require(host.state->window_metrics().pixel_width == 1200, "host service lost after GPU failure");
    require(!api.device_live, "failed binding leaked its GPU device");
    if (point == FailurePoint::device)
        require_calls(api.calls, {"init", "create_window", "create_device"});
    else
        require_calls(api.calls, {"init", "create_window", "create_device", "claim_window", "destroy_device"});
    host.state.reset();
    require(api.calls[api.calls.size() - 2] == "destroy_window" && api.calls.back() == "quit",
            "host did not independently clean up");
}

class ResourceApi final : public ryn::detail::QuadUploadApi {
public:
    explicit ResourceApi(FakePlatformApi& api) : api_(&api) {}
    void* create_vertex_buffer(std::size_t) override { return this; }
    void release_buffer(void*) noexcept override {
        released_on_live_device = api_->device_live;
        api_->calls.emplace_back("release_resource");
    }
    bool upload(void*, std::size_t, std::span<const std::byte>) override { return api_->device_live; }
    const char* last_error() const noexcept override { return "resource device absent"; }
    bool released_on_live_device{};
private:
    FakePlatformApi* api_;
};

void test_resources_retired_before_binding_rebuild() {
    FakePlatformApi api(FailurePoint::none);
    auto host = PlatformState::create(api, {});
    require(bool(host), "host create failed");
    ResourceApi resources(api);
    ryn::graphics::QuadInstanceStore store;
    const std::array<ryn::graphics::QuadInstance, 1> instances{};
    static_cast<void>(store.append(instances));
    std::uint64_t previous_epoch{};
    for (int index = 0; index != 2; ++index) {
        {
            SdlGpuBinding binding(*host.state, api);
            require(binding.device() && std::string(binding.driver()) == "fake-gpu", "binding invalid");
            require(binding.epoch() > previous_epoch, "rebuild reused device epoch");
            previous_epoch = binding.epoch();
            { ryn::detail::QuadGpuBuffer buffer(resources, store, {100, 100, 1}); }
            require(resources.released_on_live_device, "old resource released through destroyed device");
        }
        require(!api.device_live, "binding did not destroy device");
    }
    require_calls(api.calls, {"init", "create_window", "create_device", "claim_window",
        "release_resource", "release_window", "destroy_device", "create_device", "claim_window",
        "release_resource", "release_window", "destroy_device"});
}

class BindingSceneApi final : public ryn::detail::SceneBackend {
public:
    BindingSceneApi(SdlGpuBinding& binding, FakePlatformApi& api) : binding_(&binding), api_(&api) {}
    std::uint64_t device_epoch() const noexcept override { return binding_->epoch(); }
    bool begin_upload_batch() override { return api_->device_live; }
    bool finish_upload_batch() override { return api_->device_live; }
    void cancel_upload_batch() noexcept override {}
    void* create_vertex_buffer(std::size_t) override { return &buffer_; }
    void release_buffer(void*) noexcept override {
        released_live &= api_->device_live;
        api_->calls.emplace_back("release_scene_buffer");
    }
    bool upload(void*, std::size_t, std::span<const std::byte>) override { return api_->device_live; }
    void* create_glyph_sampler() override { return &sampler_; }
    void* create_glyph_texture(std::uint32_t, std::uint32_t) override { return &texture_; }
    void* create_glyph_buffer(std::size_t size) override { return create_vertex_buffer(size); }
    bool upload_glyph_texture(void*, const ryn::detail::GlyphTextureUpload&) override { return api_->device_live; }
    bool upload_glyph_buffer(void* value, std::size_t offset, std::span<const std::byte> bytes) override { return upload(value, offset, bytes); }
    void release_glyph_buffer(void* value) noexcept override { release_buffer(value); }
    void release_glyph_texture(void*) noexcept override { released_live &= api_->device_live; }
    void release_glyph_sampler(void*) noexcept override {
        released_live &= api_->device_live;
        api_->calls.emplace_back("release_scene_sampler");
    }
    void* create_effect_buffer(std::size_t size) override { return create_vertex_buffer(size); }
    bool upload_effect_buffer(void* value, std::size_t offset, std::span<const std::byte> bytes) override { return upload(value, offset, bytes); }
    void release_effect_buffer(void* value) noexcept override { release_buffer(value); }
    const char* last_error() const noexcept override { return "device not alive"; }
    const char* glyph_gpu_error() const noexcept override { return last_error(); }
    const char* effect_gpu_error() const noexcept override { return last_error(); }
    void draw_quad(std::uint32_t, std::uint32_t) override {}
    void draw_glyph(std::uint32_t, std::uint32_t, std::uint32_t) override {}
    void draw_rounded_effect(std::uint32_t, std::uint32_t) override {}
    ryn::runtime::FrameSubmissionResult submit_frame(ryn::animation::AnimationTime) override {
        return attached_scene_ready() ? ryn::runtime::FrameSubmissionResult::submitted : ryn::runtime::FrameSubmissionResult::failed;
    }
    bool released_live{true};
private:
    SdlGpuBinding* binding_;
    FakePlatformApi* api_;
    int buffer_{}, sampler_{}, texture_{};
};

void test_shared_scene_retirement_and_binding_order() {
    FakePlatformApi api(FailurePoint::none);
    auto host = PlatformState::create(api, {});
    ryn::graphics::QuadInstanceStore quads;
    static_cast<void>(quads.append(std::array<ryn::graphics::QuadInstance, 1>{}));
    ryn::graphics::GlyphAtlas atlas;
    ryn::graphics::GlyphInstanceStore glyphs;
    ryn::graphics::OrderedScene scene;
    scene.append_quad(0, 1);
    std::uint64_t old_epoch{};
    for (int index = 0; index != 2; ++index) {
        SdlGpuBinding binding(*host.state, api);
        require(binding.epoch() > old_epoch, "shared rebuild reused epoch");
        old_epoch = binding.epoch();
        BindingSceneApi backend(binding, api);
        ryn::detail::SceneResources resources(backend);
        require(resources.synchronize({&quads, atlas, glyphs, nullptr, {100, 100, 1}}), "shared lifecycle sync failed");
        const auto attached = resources.attach(scene);
        require(backend.attach_scene(attached), "shared lifecycle attach failed");
        resources.retire();
        require(!backend.valid_attachment(attached) && backend.released_live,
                "shared retire left attachment valid or released on dead device");
    }
    require_calls(api.calls, {"init", "create_window", "create_device", "claim_window",
        "release_scene_buffer", "release_scene_sampler", "release_window", "destroy_device",
        "create_device", "claim_window", "release_scene_buffer", "release_scene_sampler",
        "release_window", "destroy_device"});
}

} // namespace

int main() {
    try {
        test_failure(FailurePoint::init, PlatformStage::sdl_init, {"init"});
        test_failure(
            FailurePoint::window,
            PlatformStage::window,
            {"init", "create_window", "quit"});
        test_gpu_failure_preserves_host(FailurePoint::device);
        test_gpu_failure_preserves_host(FailurePoint::claim);
        test_resources_retired_before_binding_rebuild();
        test_shared_scene_retirement_and_binding_order();
        test_success_cleanup();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
