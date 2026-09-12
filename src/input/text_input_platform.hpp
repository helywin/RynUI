#pragma once

#include "input/text_input_events.hpp"

namespace ryn::input {

enum class TextInputType : std::uint8_t { text, name, email, username, number };
enum class TextCapitalization : std::uint8_t { none, sentences, words, letters };
struct TextInputProperties {
    TextInputType type{TextInputType::text};
    TextCapitalization capitalization{TextCapitalization::none};
    bool autocorrect{true};
    friend bool operator==(const TextInputProperties&, const TextInputProperties&) = default;
};
struct WindowTextInputArea {
    int x{}, y{}, width{}, height{}, cursor{};
    friend bool operator==(const WindowTextInputArea&, const WindowTextInputArea&) = default;
};
struct TextInputRect { double x{}, y{}, width{}, height{}; };
struct TextInputAreaGeometry {
    TextInputRect bounds;
    TextInputRect clip;
    double translation_x{}, translation_y{}, caret_x{};
    double logical_to_window_scale{1};
    int window_width{}, window_height{};
};
[[nodiscard]] std::optional<WindowTextInputArea> map_text_input_area(const TextInputAreaGeometry&) noexcept;

// One port instance represents one window. Implementations report failure, do
// not throw or call back into the host, and clear their routing stamp on stop
// even if the native operation fails. Store and port must outlive the host.
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
private:
    friend class TextInputSessionHost;
    const void* session_host_{};
};

} // namespace ryn::input
