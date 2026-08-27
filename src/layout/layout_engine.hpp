#pragma once

#include "runtime/geometry.hpp"
#include "runtime/node_store.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

namespace ryn::layout {

struct Constraints {
    float min_width{0.0F};
    float max_width{0.0F};
    float min_height{0.0F};
    float max_height{0.0F};

    static Constraints fixed(float width, float height);
    void validate() const;
    [[nodiscard]] runtime::Size constrain(runtime::Size size) const;

    friend constexpr bool operator==(Constraints, Constraints) = default;
};

struct Padding {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};

    friend constexpr bool operator==(Padding, Padding) = default;
};

struct LeafLayout {
    runtime::Size preferred_size;
};

struct BoxLayout {
    Padding padding;
    bool fill_width{false};
    bool fill_height{false};
};

enum class FlexDirection {
    horizontal,
    vertical,
};

enum class FlexWrap {
    no_wrap,
    wrap,
};

enum class FlexJustify {
    start,
    center,
    end,
    space_between,
    space_around,
    space_evenly,
};

enum class FlexAlign {
    start,
    center,
    end,
    stretch,
};

struct FlexLayout {
    FlexDirection direction{FlexDirection::horizontal};
    float main_gap{0.0F};
    Padding padding;
    bool fill_width{false};
    bool fill_height{false};
    FlexWrap wrap{FlexWrap::no_wrap};
    FlexJustify justify{FlexJustify::start};
    FlexAlign align{FlexAlign::start};
    float cross_gap{0.0F};

    friend constexpr bool operator==(FlexLayout, FlexLayout) = default;
};

struct FlexLayoutDiagnostics final {
    std::size_t item_count{0};
    std::size_t line_count{0};
    std::size_t item_capacity{0};
    std::size_t line_capacity{0};

    friend constexpr bool operator==(
        FlexLayoutDiagnostics,
        FlexLayoutDiagnostics) = default;
};

struct HorizontalContentLayout final {
    float control_height{32.0F};
    float padding_inline{15.0F};
    float border_width{1.0F};
    float gap{8.0F};
    bool loading{false};
    float loading_indicator_size{14.0F};

    friend constexpr bool operator==(
        HorizontalContentLayout,
        HorizontalContentLayout) = default;
};

struct HorizontalContentGeometry final {
    runtime::Rect content_bounds;
    std::optional<runtime::Rect> loading_indicator_bounds;

    friend constexpr bool operator==(
        HorizontalContentGeometry,
        HorizontalContentGeometry) = default;
};

using LayoutModel =
    std::variant<LeafLayout, BoxLayout, FlexLayout, HorizontalContentLayout>;

class LayoutEngine final {
public:
    using IntrinsicMeasure = std::function<runtime::Size(Constraints)>;

    explicit LayoutEngine(runtime::NodeStore& nodes) noexcept;

    void set_layout(runtime::NodeId id, LayoutModel layout);
    void set_intrinsic_measure(
        runtime::NodeId id,
        std::uint64_t revision,
        IntrinsicMeasure measure);
    bool set_intrinsic_revision(runtime::NodeId id, std::uint64_t revision);
    bool remove_intrinsic_measure(runtime::NodeId id) noexcept;
    [[nodiscard]] runtime::Size measure(runtime::NodeId root, Constraints constraints);
    void place(runtime::NodeId root, runtime::Point origin = {});
    [[nodiscard]] runtime::Size layout(
        runtime::NodeId root,
        Constraints constraints,
        runtime::Point origin = {});

    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] const HorizontalContentGeometry& horizontal_content_geometry(
        runtime::NodeId id) const;
    [[nodiscard]] FlexLayoutDiagnostics flex_layout_diagnostics(
        runtime::NodeId id) const;

private:
    struct FlexItem final {
        runtime::NodeId id;
        std::size_t declaration_ordinal{0};
        float main_size{0.0F};
        float cross_size{0.0F};
        float base_main_size{0.0F};
        float min_main_size{0.0F};
        float max_main_size{0.0F};
        float grow{0.0F};
        float shrink{1.0F};
        runtime::FlexItemAlign align_self{runtime::FlexItemAlign::automatic};
        bool frozen{false};
    };

    struct FlexLine final {
        std::size_t first_item{0};
        std::size_t item_count{0};
        float main_size{0.0F};
        float cross_size{0.0F};
    };

    struct FlexScratch final {
        std::uint64_t measure_generation{0};
        std::vector<FlexItem> items;
        std::vector<FlexLine> lines;
    };

    struct LayoutSlot {
        std::uint32_t generation{0};
        LayoutModel model{LeafLayout{}};
        std::optional<HorizontalContentGeometry> horizontal_content_geometry;
        FlexScratch flex_scratch;
    };

    struct IntrinsicCache final {
        std::uint64_t revision{0};
        Constraints constraints;
        runtime::Size result;
    };

    struct IntrinsicSlot final {
        std::uint32_t generation{0};
        std::uint64_t revision{0};
        IntrinsicMeasure measure;
        std::optional<IntrinsicCache> cache;
        bool measuring{false};
    };

    [[nodiscard]] const LayoutModel& require_layout(runtime::NodeId id) const;
    [[nodiscard]] IntrinsicSlot* find_intrinsic(runtime::NodeId id) noexcept;
    [[nodiscard]] runtime::Size
    measure_node(runtime::NodeId id, Constraints constraints,
                 std::optional<float> forced_outer_width = std::nullopt,
                 std::optional<float> forced_outer_height = std::nullopt);
    void place_node(
        runtime::NodeId id,
        runtime::Rect bounds,
        bool stretch_width = false,
        bool stretch_height = false);

    runtime::NodeStore* nodes_;
    std::vector<LayoutSlot> layouts_;
    std::vector<IntrinsicSlot> intrinsics_;
    std::uint64_t generation_{0};
};

} // namespace ryn::layout
