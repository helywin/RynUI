#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <vector>

namespace ryn::runtime {

struct NodeId {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(NodeId, NodeId) = default;
};

struct Node {
    NodeId id;
    std::optional<NodeId> parent;
    std::vector<NodeId> children;
    float opacity{1.0F};
};

class NodeStore final {
public:
    [[nodiscard]] NodeId create_root();
    [[nodiscard]] NodeId create_child(NodeId parent);

    [[nodiscard]] Node* find(NodeId id) noexcept;
    [[nodiscard]] const Node* find(NodeId id) const noexcept;
    [[nodiscard]] Node& require(NodeId id);
    [[nodiscard]] const Node& require(NodeId id) const;

    bool destroy(NodeId id) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Slot {
        Node node;
        std::uint32_t generation{1};
        bool occupied{false};
    };

    [[nodiscard]] NodeId create(std::optional<NodeId> parent);
    [[nodiscard]] Slot* find_slot(NodeId id) noexcept;
    [[nodiscard]] const Slot* find_slot(NodeId id) const noexcept;
    static void advance_generation(Slot& slot) noexcept;

    std::deque<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::size_t live_nodes_{0};
};

} // namespace ryn::runtime
