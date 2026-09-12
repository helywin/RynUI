#pragma once
#include "input/text_editor.hpp"
#include "text/text_caret_map.hpp"

namespace ryn::detail {
struct InputDisplaySnapshot {
    std::string_view text;
    input::TextSelection selection, composition;
    std::size_t caret{};
    bool placeholder{}, composing{};
};
struct InputDisplayUpdate { bool text_changed{}, geometry_changed{}; };

class InputDisplayState final {
public:
    void reserve(std::size_t bytes);
    [[nodiscard]] InputDisplayUpdate update(const input::TextEditorState&, StringView placeholder);
    [[nodiscard]] InputDisplaySnapshot snapshot() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::size_t committed_to_display(std::size_t byte, bool trailing = false) const noexcept;
    [[nodiscard]] std::size_t display_to_committed(std::size_t byte, bool trailing = false) const noexcept;
    [[nodiscard]] std::optional<float> scroll_for_caret(const text::TextCaretMap&, std::uint64_t revision,
        float viewport_width, float previous_offset, float caret_width = 1) const noexcept;
private:
    std::string text_, pending_;
    input::TextBoundaryMap boundaries_, pending_boundaries_;
    input::TextBoundaryMap composition_boundaries_, pending_composition_boundaries_;
    input::TextSelection selection_, composition_, replacement_;
    std::size_t caret_{}, committed_size_{}, inserted_size_{};
    bool placeholder_{}, composing_{};
    std::uint64_t revision_{};
};
} // namespace ryn::detail
