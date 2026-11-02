#pragma once

#include "input/text_input_events.hpp"
#include "runtime/geometry.hpp"

#include <functional>

namespace ryn::input {

enum class TextInputType : std::uint8_t { text, name, email, username, number, password_hidden, password_visible };
enum class TextCapitalization : std::uint8_t { none, sentences, words, letters };

struct TextInputProperties {
    TextInputType type{TextInputType::text};
    TextCapitalization capitalization{TextCapitalization::none};
    bool autocorrect{true};
    friend bool operator==(const TextInputProperties&, const TextInputProperties&) = default;
};

struct WindowTextInputArea {
    int x{};
    int y{};
    int width{};
    int height{};
    int cursor{};
    friend bool operator==(const WindowTextInputArea&, const WindowTextInputArea&) = default;
};

struct TextInputRect {
    double x{};
    double y{};
    double width{};
    double height{};
};

struct TextInputAreaGeometry {
    TextInputRect bounds;
    TextInputRect clip;
    double translation_x{};
    double translation_y{};
    double caret_x{};
    double logical_to_window_scale{1};
    int window_width{};
    int window_height{};
};

[[nodiscard]] std::optional<WindowTextInputArea> map_text_input_area(const TextInputAreaGeometry&) noexcept;

// The query is read-only: true only for the current, visible text selection area.
// Positions use the same logical coordinates as normalized pointer input.
using TextPointerQuery = std::function<bool(runtime::Point)>;

// One port instance represents one window. Implementations report failure, do
// not throw or call user code during text operations, and clear their routing
// stamp on stop even if the native operation fails. Pointer synchronization may
// run the separately bound read-only hit query. Store and port outlive the host.
class TextInputPlatform {
public:
    TextInputPlatform() = default;
    TextInputPlatform(const TextInputPlatform&) = delete;
    TextInputPlatform& operator=(const TextInputPlatform&) = delete;
    virtual ~TextInputPlatform() = default;
    virtual bool start(TextInputSessionStamp, const TextInputProperties&) noexcept = 0;
    virtual bool stop() noexcept = 0;
    virtual bool cancel() noexcept = 0;
    virtual bool set_area(const WindowTextInputArea&) noexcept = 0;

    // Optional native pointer presentation. Binding replaces the previous query;
    // clearing it must also restore the platform's ordinary cursor.
    virtual void bind_text_pointer_query(TextPointerQuery) noexcept {}

    virtual bool synchronize_pointer_cursor() noexcept {
        return true;
    }

private:
    friend class TextInputSessionHost;
    const void* session_host_{};
};

} // namespace ryn::input
