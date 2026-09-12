#include "platform/sdl/sdl_event_adapter.hpp"
#include "support/allocation_probe.hpp"

#include <SDL3/SDL.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn::input;
using namespace ryn::detail;
void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
struct Api final : PlatformApi {
    int token{}, reads{}, releases{}, writes{};
    bool available{true}, fail_read{}, fail_write{};
    std::string source{"clipboard"}, written;
    bool init_video() override { return true; }
    void quit() noexcept override {}
    PlatformWindowHandle create_window(const char*, int, int, bool) override { return &token; }
    void destroy_window(PlatformWindowHandle) noexcept override {}
    PlatformGpuDeviceHandle create_gpu_device(bool) override { return &token; }
    void destroy_gpu_device(PlatformGpuDeviceHandle) noexcept override {}
    bool claim_window(PlatformGpuDeviceHandle, PlatformWindowHandle) override { return true; }
    void release_window(PlatformGpuDeviceHandle, PlatformWindowHandle) noexcept override {}
    const char* last_error() const noexcept override { return "injected"; }
    const char* gpu_driver(PlatformGpuDeviceHandle) const noexcept override { return "fake"; }
    PlatformWindowMetrics window_metrics(PlatformWindowHandle) const noexcept override { return {800, 600}; }
    void delay(std::uint32_t) noexcept override {}
    bool has_clipboard_text() const noexcept override { return available; }
    char* clipboard_text() noexcept override {
        ++reads;
        if(fail_read) return nullptr;
        auto* result = static_cast<char*>(std::malloc(source.size() + 1));
        if(result) std::memcpy(result, source.c_str(), source.size() + 1);
        return result;
    }
    void free_clipboard_text(char* text) noexcept override { ++releases; std::free(text); }
    bool set_clipboard_text(const char* text) noexcept override {
        ++writes;
        if(fail_write) return false;
        try { written = text; return true; } catch(const std::bad_alloc&) { return false; }
    }
};
void bridge() {
    Api api;
    auto result = PlatformState::create(api, {});
    check(bool(result), "platform create");
    auto& platform = *result.state;
    auto read = platform.read_text();
    check(bool(read) && read.text->bytes() == "clipboard" && api.releases == 1, "owned clipboard read");
    api.source = "changed";
    check(read.text->bytes() == "clipboard", "clipboard text was borrowed");
    api.available = false;
    check(!platform.has_text().available && platform.read_text().error == ClipboardError::no_text, "missing clipboard text");
    api.available = true;
    api.fail_read = true;
    check(platform.read_text().error == ClipboardError::platform_failure && api.releases == 1, "read error release");
    api.fail_read = false;
    api.source.clear();
    auto empty = platform.read_text();
    check(bool(empty) && empty.text->empty(), "empty text not distinguished from failure");
    api.source = "\xC0\xAF";
    check(platform.read_text().error == ClipboardError::invalid_utf8 && api.releases == 3, "invalid text leak");
    api.source.assign(clipboard_max_bytes + 1, 'x');
    check(platform.read_text().error == ClipboardError::capacity_exceeded && api.releases == 4, "large clipboard leak");
    const ryn::String text(u8"你好😀");
    check(platform.write_text(text.view()) == ClipboardError::none && api.written == text.bytes(), "write lost UTF8");
    api.fail_write = true;
    check(platform.write_text(text.view()) == ClipboardError::platform_failure, "write error hidden");
    const auto embedded = ryn::String::from_utf8(std::string_view("a\0b", 3));
    const auto writes = api.writes;
    check(platform.write_text(embedded.value().view()) == ClipboardError::embedded_null
        && api.writes == writes, "C-string clipboard silently truncated NUL");
    bool rejected = false;
    std::thread worker([&] {
        rejected = platform.read_text().error == ClipboardError::wrong_thread
            && platform.write_text(text.view()) == ClipboardError::wrong_thread
            && platform.has_text().error == ClipboardError::wrong_thread;
    });
    worker.join();
    check(rejected && api.writes == writes, "clipboard operation off owner thread");
    result.state.reset();
    check(read.text->bytes() == "clipboard", "clipboard snapshot depended on platform lifetime");
}
void failures() {
    Api api;
    auto platform = PlatformState::create(api, {});
    api.source.assign(4096, 'x');
    std::size_t failure_points = 0;
    for(std::size_t point = 0; point < 100; ++point) {
        const auto before = api.releases;
        ryn_test::allocation::begin(point);
        const auto read = platform.state->read_text();
        ryn_test::allocation::end();
        check(api.releases == before + 1, "failed snapshot leaked platform allocation");
        if(read) break;
        check(read.error == ClipboardError::allocation_failure && !read.text, "copy failure not atomic");
        ++failure_points;
    }
    check(failure_points > 0 && failure_points < 100, "clipboard failure points not tested");
    const auto text = ryn::String::from_utf8(api.source).value();
    ryn_test::allocation::begin(0);
    const auto error = platform.state->write_text(text.view());
    ryn_test::allocation::end();
    check(error == ClipboardError::allocation_failure && api.writes == 0, "write preparation not atomic");
    std::cout << "clipboard_read_atomic_points=" << failure_points << '\n';
}
void metadata() {
    PlatformEvents events;
    SdlWindowMetrics metrics;
    const char* types[]{"text/plain", "text/plain;charset=utf-8"};
    SDL_Event event{};
    event.type = SDL_EVENT_CLIPBOARD_UPDATE;
    event.clipboard.owner = true;
    event.clipboard.num_mime_types = 2;
    event.clipboard.mime_types = types;
    SdlEventAdapter::merge(events, event, metrics);
    event.clipboard.owner = false;
    event.clipboard.num_mime_types = 0;
    event.clipboard.mime_types = nullptr;
    SdlEventAdapter::merge(events, event, metrics);
    check(std::get<ClipboardChanged>(events.input.events()[0]) == ClipboardChanged{true, 2}
        && std::get<ClipboardChanged>(events.input.events()[1]) == ClipboardChanged{false, 0}, "metadata snapshot/order");
    event.clipboard.num_mime_types = -1;
    bool rejected = false;
    try { SdlEventAdapter::merge(events, event, metrics); } catch(const std::invalid_argument&) { rejected = true; }
    check(rejected && events.input.size() == 2, "invalid format count accepted");
}
}
int main() {
    try { bridge(); failures(); metadata(); }
    catch(const std::exception& error) {
        ryn_test::allocation::end(); std::cerr << error.what() << '\n'; return 1;
    }
}
