#include "platform/sdl/sdl_event_adapter.hpp"
#include "input/text_input_session.hpp"

#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn::input;
using namespace ryn::detail;
using ryn::String;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
template<class E, class F> void rejects(F action) {
    try { action(); } catch(const E&) { return; }
    throw std::runtime_error("Expected rejection");
}
void adapter() {
    PlatformEvents result;
    result.text_session = {{1, 2}, 3};
    result.window_id = 7;
    result.text_started_at = 100;
    SdlWindowMetrics metrics{800, 600, 1600, 1200, 2, 2};
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_TAB;
    event.key.repeat = true;
    SdlEventAdapter::merge(result, event, metrics);
    char borrowed[] = "hello";
    event = {};
    event.type = SDL_EVENT_TEXT_EDITING;
    event.edit.windowID = 7;
    event.edit.text = borrowed;
    event.edit.start = 1;
    event.edit.length = 2;
    SdlEventAdapter::merge(result, event, metrics);
    borrowed[0] = 'X';
    event.edit.text = reinterpret_cast<const char*>(u8"中😀");
    event.edit.start = 1;
    event.edit.length = 1;
    SdlEventAdapter::merge(result, event, metrics);
    event.edit.start = -1;
    SdlEventAdapter::merge(result, event, metrics);
    event.edit.start = -2;
    rejects<std::invalid_argument>([&] { SdlEventAdapter::merge(result, event, metrics); });
    event.edit.start = 3;
    rejects<std::invalid_argument>([&] { SdlEventAdapter::merge(result, event, metrics); });
    const char* candidates[]{borrowed, reinterpret_cast<const char*>(u8"你好")};
    event = {};
    event.type = SDL_EVENT_TEXT_EDITING_CANDIDATES;
    event.edit_candidates.windowID = 7;
    event.edit_candidates.candidates = candidates;
    event.edit_candidates.num_candidates = 2;
    event.edit_candidates.selected_candidate = 1;
    event.edit_candidates.horizontal = true;
    SdlEventAdapter::merge(result, event, metrics);
    candidates[1] = "mutated";
    borrowed[1] = 'Y';
    event = {};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = 7;
    event.text.text = reinterpret_cast<const char*>(u8"你好");
    SdlEventAdapter::merge(result, event, metrics);
    const auto size = result.input.size();
    event.text.windowID = 8;
    SdlEventAdapter::merge(result, event, metrics);
    event.text.windowID = 7;
    event.text.timestamp = 99;
    SdlEventAdapter::merge(result, event, metrics);
    require(result.input.size() == size, "foreign or old native event was routed");
    event.text.timestamp = 101;
    event.text.text = "\xC0\xAF";
    rejects<std::invalid_argument>([&] { SdlEventAdapter::merge(result, event, metrics); });
    event.text.text = nullptr;
    rejects<std::invalid_argument>([&] { SdlEventAdapter::merge(result, event, metrics); });
    require(result.input.size() == size, "malformed event partially appended");
    require(result.rejected_text_events == 4, "malformed text error diagnostics missing");
    const auto values = result.input.events();
    require(std::get<KeyboardInputEvent>(values[0]).repeat, "repeat lost");
    require(std::get<CompositionChanged>(values[1]).text == String(u8"hello"), "borrowed preedit");
    require(std::get<CompositionChanged>(values[2]).selection == TextScalarRange{1, 1}, "range not scalar");
    require(!std::get<CompositionChanged>(values[3]).selection.known, "unset range lost");
    const auto& snapshot = std::get<CandidatesChanged>(values[4]);
    require(snapshot.candidates[1] == String(u8"你好") && snapshot.selected == 1
        && snapshot.orientation == CandidateOrientation::horizontal, "candidate snapshot lost");
    require(std::get<TextCommitted>(values[5]).session == result.text_session, "owner stamp lost");
    PlatformEvents bounded;
    bounded.input = PlatformInputBatch{1, 2};
    event.text.text = "abc";
    rejects<std::length_error>([&] { SdlEventAdapter::merge(bounded, event, metrics); });
    require(bounded.input.empty(), "capacity error mutated batch");
    event = {};
    event.type = SDL_EVENT_TEXT_EDITING_CANDIDATES;
    event.edit_candidates.num_candidates = 0;
    event.edit_candidates.selected_candidate = -1;
    SdlEventAdapter::merge(bounded, event, metrics);
    require(std::get<CandidatesChanged>(bounded.input.events()[0]).candidates.empty(), "empty candidates rejected");
    event.edit_candidates.num_candidates = 129;
    event.edit_candidates.windowID = 7;
    rejects<std::invalid_argument>([&] { SdlEventAdapter::merge(result, event, metrics); });
}

struct FakeApi final : PlatformApi {
    int token{}, starts{}, stops{}, cancels{}, areas{};
    bool fail_stop{}, fail_start{};
    TextInputProperties properties;
    WindowTextInputArea area;
    bool init_video() override { return true; }
    void quit() noexcept override {}
    PlatformWindowHandle create_window(const char*, int, int, bool) override { return &token; }
    void destroy_window(PlatformWindowHandle) noexcept override {}
    PlatformGpuDeviceHandle create_gpu_device(bool) override { return &token; }
    void destroy_gpu_device(PlatformGpuDeviceHandle) noexcept override {}
    bool claim_window(PlatformGpuDeviceHandle, PlatformWindowHandle) override { return true; }
    void release_window(PlatformGpuDeviceHandle, PlatformWindowHandle) noexcept override {}
    const char* last_error() const noexcept override { return "injected platform error"; }
    const char* gpu_driver(PlatformGpuDeviceHandle) const noexcept override { return "fake"; }
    PlatformWindowMetrics window_metrics(PlatformWindowHandle) const noexcept override { return {800, 600}; }
    void delay(std::uint32_t) noexcept override {}
    std::uint32_t window_id(PlatformWindowHandle) const noexcept override { return 7; }
    std::uint64_t ticks_ns() const noexcept override { return 100; }
    bool start_text_input(PlatformWindowHandle, const TextInputProperties& value) noexcept override {
        ++starts; properties = value; return !fail_start;
    }
    bool stop_text_input(PlatformWindowHandle) noexcept override { ++stops; return !fail_stop; }
    bool cancel_composition(PlatformWindowHandle) noexcept override { ++cancels; return true; }
    bool set_text_input_area(PlatformWindowHandle, const WindowTextInputArea& value) noexcept override {
        ++areas; area = value; return true;
    }
    void poll_events(PlatformWindowHandle, PlatformEvents& result) override {
        SDL_Event event{};
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.windowID = 7;
        event.text.timestamp = 101;
        event.text.text = "commit";
        auto metrics = window_metrics(nullptr);
        SdlEventAdapter::merge(result, event, metrics);
    }
};
void bridge() {
    FakeApi api;
    auto created = PlatformState::create(api, {});
    require(bool(created), "fake platform creation failed");
    auto& platform = *created.state;
    TextEditorStore store;
    TextInputSessionHost host(store, platform);
    const auto id = store.create();
    require(host.focus(id, {TextInputType::email, TextCapitalization::words, false}), "start failed");
    require(api.properties == TextInputProperties{TextInputType::email, TextCapitalization::words, false}, "props lost");
    const auto event = std::get<TextCommitted>(platform.poll_events().input.events()[0]);
    require(event.session == host.active() && bool(host.dispatch(event)), "pump lost owner");
    const auto wait_event = std::get<TextCommitted>(platform.wait_events(1).input.events()[0]);
    require(wait_event.session == host.active(), "wait pump lost owner");
    require(host.set_input_area({{1, 2, 100, 24}, {0, 0, 800, 600}, 0, 0, 40, 1.25, 800, 600}), "area failed");
    require(api.area == WindowTextInputArea{1, 2, 126, 31, 49}, "area conversion mismatch");
    bool rejected = false;
    std::thread worker([&] { rejected = !platform.cancel() && !platform.stop()
        && !platform.start({id, 9}, {}) && !platform.set_area({}); });
    worker.join();
    require(rejected, "native calls allowed off owner thread");
    api.fail_stop = true;
    require(!host.blur() && !host.active().valid(), "stop failure hidden");
    const auto late = std::get<TextCommitted>(platform.poll_events().input.events()[0]);
    require(!late.session.valid(), "failed stop still routes input");
    api.fail_stop = false;
    require(host.synchronize(), "failed stop not retryable");
    require(host.focus(id), "restart failed");
    require(!host.dispatch(event), "previous-session event accepted");
}
}
int main() {
    try { adapter(); bridge(); std::cout << "SDL-shaped text events and platform bridge passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
