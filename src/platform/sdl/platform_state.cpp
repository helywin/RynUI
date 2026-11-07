#include "platform/sdl/platform_state.hpp"
#include "platform/sdl/sdl_event_adapter.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ryn::detail {

std::string windows_html_clipboard_data(StringView fragment) {
    std::string header = "Version:1.0\r\nStartHTML:0000000000\r\nEndHTML:0000000000\r\n"
                         "StartFragment:0000000000\r\nEndFragment:0000000000\r\n";
    constexpr std::string_view prefix{"<html><body><!--StartFragment-->"};
    constexpr std::string_view suffix{"<!--EndFragment--></body></html>"};
    const auto start_html = header.size();
    const auto start_fragment = start_html + prefix.size();
    const auto end_fragment = start_fragment + fragment.size_bytes();
    const auto end_html = end_fragment + suffix.size();
    const auto set_offset = [&](std::string_view field, std::size_t value) {
        const auto number = std::to_string(value);
        if (number.size() > 10) {
            throw std::length_error("HTML clipboard offset exceeds ten digits");
        }
        header.replace(header.find(field) + field.size(), 10, std::string(10 - number.size(), '0') + number);
    };
    set_offset("StartHTML:", start_html);
    set_offset("EndHTML:", end_html);
    set_offset("StartFragment:", start_fragment);
    set_offset("EndFragment:", end_fragment);
    return header + std::string(prefix) + std::string(fragment.bytes()) + std::string(suffix);
}

namespace {

#if defined(_WIN32)
bool publish_windows_html(PlatformWindowHandle window, StringView fragment) noexcept {
    try {
        const auto bytes = windows_html_clipboard_data(fragment);
        const auto format = RegisterClipboardFormatW(L"HTML Format");
        if (!format || !window) {
            return false;
        }
        const auto properties = SDL_GetWindowProperties(static_cast<SDL_Window*>(window));
        const auto owner =
            static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!owner) {
            return false;
        }
        const auto memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size() + 1);
        if (!memory) {
            return false;
        }
        const auto destination = GlobalLock(memory);
        if (!destination) {
            GlobalFree(memory);
            return false;
        }
        std::memcpy(destination, bytes.c_str(), bytes.size() + 1);
        GlobalUnlock(memory);
        if (!OpenClipboard(owner)) {
            GlobalFree(memory);
            return false;
        }
        // SDL has already set CF_UNICODETEXT and owns the clipboard. Append the
        // standard HTML format without emptying the simultaneous plain format.
        const bool success = SetClipboardData(format, memory) != nullptr;
        CloseClipboard();
        if (!success) {
            GlobalFree(memory);
        }
        return success;
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }
}
#endif

class SdlPlatformApi final : public PlatformApi {
public:
    bool has_clipboard_text() const noexcept override {
        return SDL_HasClipboardText();
    }

    char* clipboard_text() noexcept override {
        return SDL_GetClipboardText();
    }

    void free_clipboard_text(char* value) noexcept override {
        SDL_free(value);
    }

    bool set_clipboard_text(const char* value) noexcept override {
        return SDL_SetClipboardText(value);
    }

    bool set_clipboard_content(PlatformWindowHandle window,
                               std::shared_ptr<const PlatformClipboardContent> content) noexcept override {
#if defined(_WIN32)
        const auto native_content = content;
#else
        (void)window;
#endif
        struct Offer final {
            std::shared_ptr<const PlatformClipboardContent> content;
            bool* released_during_set{};
        };

        bool released = false;
        auto* offer = new (std::nothrow) Offer{std::move(content), &released};
        if (!offer) {
            return false;
        }
        const auto data = [](void* userdata, const char* mime, std::size_t* size) -> const void* {
            if (!mime || !size) {
                if (size) {
                    *size = 0;
                }
                return nullptr;
            }
            const auto& content = *static_cast<Offer*>(userdata)->content;
            const String* value =
                std::strcmp(mime, "text/html") == 0 ? &content.html
                : std::strcmp(mime, "text/plain;charset=utf-8") == 0 || std::strcmp(mime, "text/plain") == 0
                    ? &content.plain_text
                    : nullptr;
            *size = value ? value->size_bytes() : 0;
            return value ? value->data() : nullptr;
        };
        const auto cleanup = [](void* userdata) {
            auto* offer = static_cast<Offer*>(userdata);
            if (offer->released_during_set) {
                *offer->released_during_set = true;
            }
            delete offer;
        };
        const char* types[]{"text/plain;charset=utf-8", "text/plain", "text/html"};
        bool success = SDL_SetClipboardData(data, cleanup, offer, types, std::size(types));
#if defined(_WIN32)
        success = success && !released && publish_windows_html(window, native_content->html.view());
#endif
        if (!success) {
            // SDL may retain the callbacks after a backend rejects an offer,
            // or run cleanup synchronously after an allocation failure.
            if (!released) {
                static_cast<void>(SDL_ClearClipboardData());
            }
            if (!released) {
                cleanup(offer);
            }
            return false;
        }
        if (!released) {
            offer->released_during_set = nullptr;
        }
        return !released;
    }

    std::uint64_t ticks_ns() const noexcept override {
        return SDL_GetTicksNS();
    }

    std::uint32_t window_id(PlatformWindowHandle window) const noexcept override {
        return SDL_GetWindowID(static_cast<SDL_Window*>(window));
    }

    bool start_text_input(PlatformWindowHandle window, const input::TextInputProperties& value) noexcept override {
        SDL_TextInputType type;
        switch (value.type) {
        case input::TextInputType::text:
            type = SDL_TEXTINPUT_TYPE_TEXT;
            break;
        case input::TextInputType::name:
            type = SDL_TEXTINPUT_TYPE_TEXT_NAME;
            break;
        case input::TextInputType::email:
            type = SDL_TEXTINPUT_TYPE_TEXT_EMAIL;
            break;
        case input::TextInputType::username:
            type = SDL_TEXTINPUT_TYPE_TEXT_USERNAME;
            break;
        case input::TextInputType::number:
            type = SDL_TEXTINPUT_TYPE_NUMBER;
            break;
        case input::TextInputType::password_hidden:
            type = SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN;
            break;
        case input::TextInputType::password_visible:
            type = SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE;
            break;
        default:
            return false;
        }
        SDL_Capitalization capitalization;
        switch (value.capitalization) {
        case input::TextCapitalization::none:
            capitalization = SDL_CAPITALIZE_NONE;
            break;
        case input::TextCapitalization::sentences:
            capitalization = SDL_CAPITALIZE_SENTENCES;
            break;
        case input::TextCapitalization::words:
            capitalization = SDL_CAPITALIZE_WORDS;
            break;
        case input::TextCapitalization::letters:
            capitalization = SDL_CAPITALIZE_LETTERS;
            break;
        default:
            return false;
        }
        const auto props = SDL_CreateProperties();
        if (!props) {
            return false;
        }
        const bool result = SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, type) &&
                            SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, capitalization) &&
                            SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, value.autocorrect) &&
                            SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, false) &&
                            SDL_StartTextInputWithProperties(static_cast<SDL_Window*>(window), props);
        SDL_DestroyProperties(props);
        return result;
    }

    bool stop_text_input(PlatformWindowHandle window) noexcept override {
        return SDL_StopTextInput(static_cast<SDL_Window*>(window));
    }

    bool cancel_composition(PlatformWindowHandle window) noexcept override {
        return SDL_ClearComposition(static_cast<SDL_Window*>(window));
    }

    bool set_text_input_area(PlatformWindowHandle window, const input::WindowTextInputArea& value) noexcept override {
        const SDL_Rect rect{value.x, value.y, value.width, value.height};
        return SDL_SetTextInputArea(static_cast<SDL_Window*>(window), &rect, value.cursor);
    }

    std::optional<runtime::Point> mouse_position(PlatformWindowHandle window) const noexcept override {
        if (SDL_GetMouseFocus() != static_cast<SDL_Window*>(window)) {
            return {};
        }
        runtime::Point point;
        static_cast<void>(SDL_GetMouseState(&point.x, &point.y));
        return point;
    }

    PlatformCursorHandle create_text_cursor() noexcept override {
        return SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    }

    bool set_cursor(PlatformCursorHandle cursor) noexcept override {
        return SDL_SetCursor(cursor ? static_cast<SDL_Cursor*>(cursor) : SDL_GetDefaultCursor());
    }

    PlatformCursorHandle current_cursor() const noexcept override {
        const auto cursor = SDL_GetCursor();
        return cursor == SDL_GetDefaultCursor() ? nullptr : cursor;
    }

    void destroy_cursor(PlatformCursorHandle cursor) noexcept override {
        SDL_DestroyCursor(static_cast<SDL_Cursor*>(cursor));
    }

    bool init_video() override {
        return SDL_InitSubSystem(SDL_INIT_VIDEO);
    }

    void quit() noexcept override {
        // Each PlatformState owns one video reference, not global SDL lifetime.
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    PlatformWindowHandle create_window(const char* title, int width, int height, bool high_pixel_density,
                                       bool hidden) override {
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
        if (hidden) {
            flags |= SDL_WINDOW_HIDDEN;
        }
        if (high_pixel_density) {
            flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        }
        return SDL_CreateWindow(title, width, height, flags);
    }

    void destroy_window(PlatformWindowHandle window) noexcept override {
        SDL_DestroyWindow(static_cast<SDL_Window*>(window));
    }

    [[nodiscard]] const char* last_error() const noexcept override {
        return SDL_GetError();
    }

    [[nodiscard]] PlatformWindowMetrics window_metrics(PlatformWindowHandle window) const noexcept override {
        return query_window_metrics(window);
    }

    void poll_events(PlatformWindowHandle window, PlatformEvents& result) override {
        auto metrics = window_metrics(window);
        const auto modifiers = SDL_GetModState();
        result.pointer_shift = (modifiers & SDL_KMOD_SHIFT) != 0;
        result.pointer_control = (modifiers & SDL_KMOD_CTRL) != 0;
        result.pointer_meta = (modifiers & SDL_KMOD_GUI) != 0;
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            refresh_window_metrics(window, event, metrics);
            SdlEventAdapter::merge(result, event, metrics);
        }
    }

    void delay(std::uint32_t milliseconds) noexcept override {
        SDL_Delay(milliseconds);
    }

    void wait_events(PlatformWindowHandle window, std::uint32_t timeout_milliseconds, PlatformEvents& result) override {
        auto metrics = window_metrics(window);
        const auto modifiers = SDL_GetModState();
        result.pointer_shift = (modifiers & SDL_KMOD_SHIFT) != 0;
        result.pointer_control = (modifiers & SDL_KMOD_CTRL) != 0;
        result.pointer_meta = (modifiers & SDL_KMOD_GUI) != 0;
        SDL_Event event{};
        if (SDL_WaitEventTimeout(&event, static_cast<Sint32>(timeout_milliseconds))) {
            refresh_window_metrics(window, event, metrics);
            SdlEventAdapter::merge(result, event, metrics);
            SDL_Event queued{};
            while (SDL_PollEvent(&queued)) {
                refresh_window_metrics(window, queued, metrics);
                SdlEventAdapter::merge(result, queued, metrics);
            }
        }
    }

private:
    static PlatformWindowMetrics query_window_metrics(PlatformWindowHandle window) noexcept {
        PlatformWindowMetrics metrics;
        if (window != nullptr) {
            static_cast<void>(SDL_GetWindowSize(static_cast<SDL_Window*>(window), &metrics.coordinate_width,
                                                &metrics.coordinate_height));
            static_cast<void>(SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(window), &metrics.pixel_width,
                                                        &metrics.pixel_height));
            metrics.pixel_density = SDL_GetWindowPixelDensity(static_cast<SDL_Window*>(window));
            metrics.display_scale = SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(window));
        }
        return metrics;
    }

    static void refresh_window_metrics(PlatformWindowHandle window, const SDL_Event& event,
                                       PlatformWindowMetrics& metrics) noexcept {
        if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
            event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
            metrics = query_window_metrics(window);
        }
    }
};

PlatformCreateResult failed_create(std::unique_ptr<PlatformState> state, PlatformApi& api, PlatformStage stage) {
    const char* error = api.last_error();
    PlatformError failure{
        stage,
        error != nullptr && error[0] != '\0' ? error : "Unknown SDL error",
    };
    state.reset();
    return PlatformCreateResult{nullptr, std::move(failure)};
}

PlatformApi& real_platform_api() {
    static SdlPlatformApi api;
    return api;
}

} // namespace

PlatformState::PlatformState(PlatformApi& api) : api_(&api), owner_thread_(std::this_thread::get_id()) {
    events_.input.reserve(64);
}

PlatformState::~PlatformState() {
    if (text_cursor_) {
        if (api_->current_cursor() == text_cursor_) {
            static_cast<void>(api_->set_cursor(nullptr));
        }
        api_->destroy_cursor(text_cursor_);
    }
    if (text_session_.valid() || text_stop_pending_) {
        static_cast<void>(stop());
    }
    if (window_ != nullptr) {
        api_->destroy_window(window_);
    }
    if (sdl_initialized_) {
        api_->quit();
    }
}

PlatformCreateResult PlatformState::create(const PlatformConfig& config) {
    return create(real_platform_api(), config);
}

PlatformCreateResult PlatformState::create(PlatformApi& api, const PlatformConfig& config) {
    auto state = std::unique_ptr<PlatformState>(new PlatformState(api));

    if (!api.init_video()) {
        return failed_create(std::move(state), api, PlatformStage::sdl_init);
    }
    state->sdl_initialized_ = true;

    state->window_ =
        api.create_window(config.title.c_str(), config.width, config.height, config.high_pixel_density, config.hidden);
    if (state->window_ == nullptr) {
        return failed_create(std::move(state), api, PlatformStage::window);
    }

    return PlatformCreateResult{std::move(state), std::nullopt};
}

PlatformWindowHandle PlatformState::window() const noexcept {
    return window_;
}

float PlatformState::display_scale() const noexcept {
    return window_metrics().display_scale;
}

PlatformWindowMetrics PlatformState::window_metrics() const noexcept {
    return api_->window_metrics(window_);
}

bool PlatformState::is_owner_thread() const noexcept {
    return owner_thread_ == std::this_thread::get_id();
}

bool PlatformState::poll_quit_requested() {
    return poll_events().quit_requested;
}

const PlatformEvents& PlatformState::poll_events() {
    require_owner_thread();
    events_.clear();
    events_.text_session = text_session_;
    events_.text_started_at = text_started_at_;
    events_.window_id = api_->window_id(window_);
    ++event_diagnostics_.poll_calls;
    try {
        api_->poll_events(window_, events_);
    } catch (...) {
        record_event_pump();
        throw;
    }
    record_event_pump();
    static_cast<void>(synchronize_pointer_cursor());
    return events_;
}

const PlatformEvents& PlatformState::wait_events(std::uint32_t timeout_milliseconds) {
    require_owner_thread();
    events_.clear();
    events_.text_session = text_session_;
    events_.text_started_at = text_started_at_;
    events_.window_id = api_->window_id(window_);
    ++event_diagnostics_.wait_calls;
    try {
        api_->wait_events(window_, timeout_milliseconds, events_);
    } catch (...) {
        record_event_pump();
        throw;
    }
    record_event_pump();
    static_cast<void>(synchronize_pointer_cursor());
    return events_;
}

PlatformEventDiagnostics PlatformState::event_diagnostics() const {
    require_owner_thread();
    return event_diagnostics_;
}

void PlatformState::delay(std::uint32_t milliseconds) noexcept {
    api_->delay(milliseconds);
}

bool PlatformState::start(input::TextInputSessionStamp stamp, const input::TextInputProperties& props) noexcept {
    if (!is_owner_thread() || !stamp.valid() || text_session_.valid() || text_stop_pending_) {
        return false;
    }
    const auto started_at = api_->ticks_ns();
    if (!api_->start_text_input(window_, props)) {
        text_stop_pending_ = !api_->stop_text_input(window_);
        return false;
    }
    text_session_ = stamp;
    text_started_at_ = started_at;
    return true;
}

bool PlatformState::stop() noexcept {
    if (!is_owner_thread()) {
        return false;
    }
    if (!text_session_.valid() && !text_stop_pending_) {
        return true;
    }
    text_session_ = {};
    text_stop_pending_ = !api_->stop_text_input(window_);
    return !text_stop_pending_;
}

bool PlatformState::cancel() noexcept {
    return is_owner_thread() && api_->cancel_composition(window_);
}

bool PlatformState::set_area(const input::WindowTextInputArea& area) noexcept {
    if (!is_owner_thread() || !text_session_.valid() || area.x < 0 || area.y < 0 || area.width < 0 || area.height < 0 ||
        area.cursor < 0 || area.cursor > area.width) {
        return false;
    }
    return api_->set_text_input_area(window_, area);
}

void PlatformState::bind_text_pointer_query(input::TextPointerQuery query) noexcept {
    text_pointer_query_ = std::move(query);
    if (!text_pointer_query_) {
        static_cast<void>(synchronize_pointer_cursor());
    }
}

bool PlatformState::set_pointer_display_scale(float scale) noexcept {
    if (!is_owner_thread() || !std::isfinite(scale) || scale <= 0) {
        return false;
    }
    pointer_display_scale_ = scale;
    return true;
}

bool PlatformState::synchronize_pointer_cursor() noexcept {
    if (!is_owner_thread()) {
        return false;
    }
    const auto point = api_->mouse_position(window_);
    if (!point) {
        // SDL's current cursor is shared by its windows. An inactive window
        // must not reset a cursor already selected by another window.
        if (text_cursor_ && api_->current_cursor() == text_cursor_) {
            return api_->set_cursor(nullptr);
        }
        return true;
    }
    bool text = false;
    if (text_pointer_query_) {
        if (point && std::isfinite(point->x) && std::isfinite(point->y)) {
            const auto metrics = window_metrics();
            const auto scale = pointer_display_scale_ > 0 ? metrics.pixel_density / pointer_display_scale_
                                                          : metrics.coordinate_to_logical_scale();
            try {
                text = text_pointer_query_({point->x * scale, point->y * scale});
            } catch (...) {
                text = false;
            }
        }
    }
    if (text && !text_cursor_) {
        text_cursor_ = api_->create_text_cursor();
        if (!text_cursor_) {
            return false;
        }
    }
    const auto desired = text ? text_cursor_ : nullptr;
    if (api_->current_cursor() == desired) {
        return true;
    }
    return api_->set_cursor(desired);
}

input::ClipboardReadResult PlatformState::read_text() {
    using input::ClipboardError;
    if (!is_owner_thread()) {
        return {ClipboardError::wrong_thread, {}};
    }
    if (!api_->has_clipboard_text()) {
        return {ClipboardError::no_text, {}};
    }
    char* text = api_->clipboard_text();
    if (!text) {
        return {ClipboardError::platform_failure, {}};
    }

    struct Release {
        PlatformApi* api;
        char* text;

        ~Release() {
            api->free_clipboard_text(text);
        }
    } release{api_, text};

    std::size_t size = 0;
    while (size <= input::clipboard_max_bytes && text[size] != '\0') {
        ++size;
    }
    if (size > input::clipboard_max_bytes) {
        return {ClipboardError::capacity_exceeded, {}};
    }
    try {
        auto parsed = String::from_utf8(std::string_view(text, size));
        if (!parsed) {
            return {ClipboardError::invalid_utf8, {}};
        }
        return {ClipboardError::none, std::move(parsed).value()};
    } catch (const std::bad_alloc&) {
        return {ClipboardError::allocation_failure, {}};
    } catch (const std::length_error&) {
        return {ClipboardError::capacity_exceeded, {}};
    }
}

input::ClipboardError PlatformState::write_text(StringView text) {
    using input::ClipboardError;
    if (!is_owner_thread()) {
        return ClipboardError::wrong_thread;
    }
    if (text.size_bytes() > input::clipboard_max_bytes) {
        return ClipboardError::capacity_exceeded;
    }
    if (text.bytes().find('\0') != std::string_view::npos) {
        return ClipboardError::embedded_null;
    }
    try {
        const std::string terminated(text.bytes());
        return api_->set_clipboard_text(terminated.c_str()) ? ClipboardError::none : ClipboardError::platform_failure;
    } catch (const std::bad_alloc&) {
        return ClipboardError::allocation_failure;
    } catch (const std::length_error&) {
        return ClipboardError::capacity_exceeded;
    }
}

input::ClipboardError PlatformState::write_html(StringView plain_text, StringView html) {
    using input::ClipboardError;
    if (!is_owner_thread()) {
        return ClipboardError::wrong_thread;
    }
    if (plain_text.size_bytes() > input::clipboard_max_bytes || html.size_bytes() > input::clipboard_max_bytes) {
        return ClipboardError::capacity_exceeded;
    }
    if (plain_text.bytes().find('\0') != std::string_view::npos || html.bytes().find('\0') != std::string_view::npos) {
        return ClipboardError::embedded_null;
    }
    try {
        auto content = std::make_shared<PlatformClipboardContent>(PlatformClipboardContent{
            String::from_utf8(plain_text.bytes()).value(), String::from_utf8(html.bytes()).value()});
        return api_->set_clipboard_content(window_, std::move(content)) ? ClipboardError::none
                                                                        : ClipboardError::platform_failure;
    } catch (const std::bad_alloc&) {
        return ClipboardError::allocation_failure;
    } catch (const std::length_error&) {
        return ClipboardError::capacity_exceeded;
    }
}

input::ClipboardAvailability PlatformState::has_text() const noexcept {
    if (!is_owner_thread()) {
        return {input::ClipboardError::wrong_thread, false};
    }
    return {input::ClipboardError::none, api_->has_clipboard_text()};
}

void PlatformState::require_owner_thread() const {
    if (!is_owner_thread()) {
        throw std::logic_error("Platform event pump must run on its owner thread");
    }
}

void PlatformState::record_event_pump() {
    event_diagnostics_.rejected_text_events += events_.rejected_text_events;
    event_diagnostics_.normalized_input_events += events_.input.size() + events_.input.coalesced_move_count();
    event_diagnostics_.coalesced_pointer_moves += events_.input.coalesced_move_count();
    event_diagnostics_.suppressed_compatibility_mouse_events += events_.suppressed_compatibility_mouse_events;
    if (events_.frame_requested) {
        ++event_diagnostics_.frame_requested_pumps;
    }
    if (events_.quit_requested) {
        ++event_diagnostics_.quit_requested_pumps;
    }
}

} // namespace ryn::detail
