#pragma once

#include "runtime/geometry.hpp"
#include "runtime/node_store.hpp"

#include <cstdint>
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
};

struct Padding {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};
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

struct FlexLayout {
    FlexDirection direction{FlexDirection::horizontal};
    float gap{0.0F};
    Padding padding;
    bool fill_width{false};
    bool fill_height{false};
};

using LayoutModel = std::variant<LeafLayout, BoxLayout, FlexLayout>;

class LayoutEngine final {
public:
    explicit LayoutEngine(runtime::NodeStore& nodes) noexcept;

    void set_layout(runtime::NodeId id, LayoutModel layout);
    [[nodiscard]] runtime::Size measure(runtime::NodeId root, Constraints constraints);
    void place(runtime::NodeId root, runtime::Point origin = {});
    [[nodiscard]] runtime::Size layout(
        runtime::NodeId root,
        Constraints constraints,
        runtime::Point origin = {});

    [[nodiscard]] std::uint64_t generation() const noexcept;

private:
    struct LayoutSlot {
        std::uint32_t generation{0};
        LayoutModel model{LeafLayout{}};
    };

    [[nodiscard]] const LayoutModel& require_layout(runtime::NodeId id) const;
    [[nodiscard]] runtime::Size measure_node(runtime::NodeId id, Constraints constraints);
    void place_node(runtime::NodeId id, runtime::Rect bounds);

    runtime::NodeStore* nodes_;
    std::vector<LayoutSlot> layouts_;
    std::uint64_t generation_{0};
};

} // namespace ryn::layout
