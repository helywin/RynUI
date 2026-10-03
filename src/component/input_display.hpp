#pragma once
#include "input/text_editor.hpp"
#include "text/text_caret_map.hpp"

namespace ryn::detail {
struct InputDisplaySnapshot {
    std::string_view text;
    input::TextSelection selection;
    input::TextSelection composition;
    std::size_t caret{};
    bool placeholder{};
    bool composing{};
};

struct InputDisplayUpdate {
    bool text_changed{};
    bool geometry_changed{};
};

class InputDisplayState final {
public:
    void reserve(std::size_t bytes);
    [[nodiscard]] InputDisplayUpdate update(const input::TextEditorState&, StringView placeholder, bool masked = false,
                                            StringView mask = {});
    [[nodiscard]] InputDisplaySnapshot snapshot() const noexcept;

    [[nodiscard]] std::uint64_t revision() const noexcept {
        return revision_;
    }

    [[nodiscard]] std::size_t committed_to_display(std::size_t byte, bool trailing = false) const noexcept;
    [[nodiscard]] std::size_t display_to_committed(std::size_t byte, bool trailing = false) const noexcept;
    [[nodiscard]] std::optional<float> scroll_for_caret(const text::TextCaretMap&, std::uint64_t revision,
                                                        float viewport_width, float previous_offset,
                                                        float caret_width = 1) const noexcept;

private:
    std::string text_;
    std::string pending_;
    std::string pending_logical_;
    input::TextBoundaryMap boundaries_;
    input::TextBoundaryMap pending_boundaries_;
    input::TextBoundaryMap mask_boundaries_;
    input::TextBoundaryMap pending_mask_boundaries_;
    input::TextBoundaryMap composition_boundaries_;
    input::TextBoundaryMap pending_composition_boundaries_;
    input::TextSelection selection_;
    input::TextSelection composition_;
    input::TextSelection replacement_;
    std::size_t caret_{};
    std::size_t committed_size_{};
    std::size_t inserted_size_{};
    bool placeholder_{};
    bool composing_{};
    bool masked_{};
    std::size_t mask_bytes_{3};
    std::uint64_t revision_{};
};
} // namespace ryn::detail
