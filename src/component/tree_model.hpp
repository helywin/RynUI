#pragma once
#include <ryn/tree.hpp>
#include <cstddef>
#include <limits>

namespace ryn::detail {
inline constexpr std::size_t tree_no_parent = std::numeric_limits<std::size_t>::max();

struct TreeModelNode final {
    String key;
    std::size_t parent{tree_no_parent};
    std::size_t depth{};
    std::size_t end{};
    bool disabled{};
    bool visible{true};
};

struct TreeChecks final {
    TreeKeys checked;
    TreeKeys half_checked;
};

struct TreeModel final {
    std::vector<TreeModelNode> nodes;
    TreeKeys expanded;
    TreeKeys selected;
    TreeKeys checked;
    bool disabled{};
    bool visible{true};
    bool checkable{};
    bool controlled_expanded{};
    bool controlled_selected{};
    bool controlled_checked{};

    void validate() const;
    [[nodiscard]] TreeKeys normalize(const TreeKeys&, bool single = false) const;
    [[nodiscard]] std::vector<std::size_t> shown() const;
    [[nodiscard]] bool eligible(std::size_t) const;
    [[nodiscard]] bool open(std::size_t) const;
    [[nodiscard]] TreeKeys expand(std::size_t) const;
    [[nodiscard]] TreeKeys select(std::size_t) const;
    [[nodiscard]] TreeChecks checks(const TreeKeys&) const;
    [[nodiscard]] TreeChecks toggle_check(std::size_t) const;
};
} // namespace ryn::detail
