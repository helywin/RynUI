#pragma once

#include "graphics/glyph_scene.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/node_store.hpp"
#include "text/text_engine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace ryn::detail {

struct TextSceneId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(TextSceneId, TextSceneId) = default;
};

struct TextSceneRevisions final {
    std::uint64_t content{1};
    std::uint64_t tone{1};
    std::uint64_t layout{1};
    std::uint64_t placement{0};

    friend constexpr bool operator==(
        TextSceneRevisions,
        TextSceneRevisions) = default;
};

struct TextSceneRecordCounters final {
    std::uint64_t measurement_synchronizations{};
    std::uint64_t synchronizations{};
    std::uint64_t instance_rebuilds{};
    std::uint64_t material_updates{};
    std::uint64_t geometry_updates{};
};

struct TextSceneServiceCounters final {
    std::uint64_t creates{};
    std::uint64_t destroys{};
    std::uint64_t range_compactions{};
    std::uint64_t ordered_scene_rebuilds{};
};

class TextSceneService final {
public:
    TextSceneService(
        font::FontRuntime& fonts,
        text::TextEngine& engine,
        runtime::FrameRequestState& frame_requests) noexcept;
    TextSceneService(const TextSceneService&) = delete;
    TextSceneService& operator=(const TextSceneService&) = delete;
    TextSceneService(TextSceneService&&) = delete;
    TextSceneService& operator=(TextSceneService&&) = delete;
    ~TextSceneService();

    [[nodiscard]] TextSceneId create(
        runtime::NodeId node,
        String content,
        std::vector<font::FontIdentity> fallback_chain,
        std::uint32_t pixel_size,
        text::TextLayoutConfig layout);
    bool destroy(TextSceneId id);

    bool set_content(TextSceneId id, String content);
    bool set_font_chain(
        TextSceneId id,
        std::vector<font::FontIdentity> fallback_chain);
    bool set_pixel_size(TextSceneId id, std::uint32_t pixel_size);
    bool set_line_height(TextSceneId id, float line_height);
    void request_reshape(TextSceneId id);
    bool set_width_constraint(TextSceneId id, float width);
    bool set_color(TextSceneId id, std::array<float, 4> color);
    bool set_opacity(TextSceneId id, float opacity);
    bool set_placement(TextSceneId id, graphics::GlyphPlacement placement);

    [[nodiscard]] bool synchronize(TextSceneId id);
    [[nodiscard]] bool synchronize_measurement(TextSceneId id);
    [[nodiscard]] bool synchronize_measurement(
        TextSceneId id,
        float width_constraint);
    [[nodiscard]] bool synchronize(
        TextSceneId id,
        graphics::GlyphPlacement placement);
    [[nodiscard]] bool synchronize_all();

    [[nodiscard]] bool contains(TextSceneId id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] runtime::NodeId node(TextSceneId id) const;
    [[nodiscard]] std::size_t declaration_order(TextSceneId id) const;
    [[nodiscard]] text::TextState& text_state(TextSceneId id);
    [[nodiscard]] const text::TextState& text_state(TextSceneId id) const;
    [[nodiscard]] const graphics::GlyphPrimitive& primitive(TextSceneId id) const;
    [[nodiscard]] const TextSceneRevisions& revisions(TextSceneId id) const;
    [[nodiscard]] const TextSceneRecordCounters& record_counters(
        TextSceneId id) const;
    [[nodiscard]] const graphics::GlyphAtlasError& last_error(
        TextSceneId id) const;

    [[nodiscard]] graphics::GlyphAtlas& atlas() noexcept;
    [[nodiscard]] const graphics::GlyphAtlas& atlas() const noexcept;
    [[nodiscard]] graphics::GlyphScene& glyph_scene() noexcept;
    [[nodiscard]] const graphics::GlyphScene& glyph_scene() const noexcept;
    [[nodiscard]] const graphics::OrderedScene& ordered_scene() const noexcept;
    [[nodiscard]] const TextSceneServiceCounters& counters() const noexcept;

private:
    struct Record;
    struct Slot;

    [[nodiscard]] Record* find_record(TextSceneId id) noexcept;
    [[nodiscard]] const Record* find_record(TextSceneId id) const noexcept;
    [[nodiscard]] Record& require_record(TextSceneId id);
    [[nodiscard]] const Record& require_record(TextSceneId id) const;
    [[nodiscard]] std::uint32_t acquire_slot();
    void release_slot(TextSceneId id) noexcept;
    void ensure_owner_thread() const;
    bool update_placement(
        TextSceneId id,
        graphics::GlyphPlacement placement,
        bool request_frame);
    void remap_following(TextSceneId id, std::int64_t offset);
    void rebuild_ordered_scene();
    static void shift_primitive(
        graphics::GlyphPrimitive& primitive,
        std::int64_t offset);
    static void advance_generation(Slot& slot) noexcept;

    font::FontRuntime* fonts_;
    text::TextEngine* engine_;
    runtime::FrameRequestState* frame_requests_;
    std::thread::id owner_thread_;
    graphics::GlyphAtlas atlas_;
    graphics::GlyphScene glyph_scene_;
    graphics::OrderedScene ordered_scene_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<TextSceneId> ordered_ids_;
    std::size_t live_records_{0};
    std::size_t next_declaration_order_{0};
    TextSceneServiceCounters counters_;
};

} // namespace ryn::detail
