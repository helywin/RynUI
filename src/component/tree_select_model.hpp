#pragma once

#include <ryn/string.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ryn::detail {
inline constexpr std::size_t tree_select_no_parent = std::numeric_limits<std::size_t>::max();

struct TreeSelectNode final {
    String key;
    String title;
    std::size_t parent{tree_select_no_parent};
    bool disabled{};
    bool visible{true};
};

class TreeSelectModel final {
public:
    explicit TreeSelectModel(std::vector<TreeSelectNode> nodes, String value = {}, bool controlled_value = false,
                             bool open = false, bool controlled_open = false)
        : nodes_(std::move(nodes)), value_(std::move(value)), controlled_value_(controlled_value), open_(open),
          controlled_open_(controlled_open) {
        if (nodes_.size() > 1024) {
            throw std::invalid_argument("TreeSelect supports at most 1024 nodes");
        }
        std::vector<std::size_t> depths;
        depths.reserve(nodes_.size());
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const auto& node = nodes_[i];
            if (node.key.empty() || (node.parent != tree_select_no_parent && node.parent >= i)) {
                throw std::invalid_argument("TreeSelect keys and parent order are invalid");
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (nodes_[j].key == node.key) {
                    throw std::invalid_argument("TreeSelect keys must be unique");
                }
            }
            const auto depth = node.parent == tree_select_no_parent ? 0 : depths[node.parent] + 1;
            if (depth >= 64) {
                throw std::invalid_argument("TreeSelect supports at most 64 levels");
            }
            depths.push_back(depth);
        }
    }

    [[nodiscard]] const std::vector<TreeSelectNode>& nodes() const noexcept {
        return nodes_;
    }

    void presentation(std::size_t index, String title, bool disabled, bool visible) {
        auto& node = nodes_.at(index);
        node.title = std::move(title);
        node.disabled = disabled;
        node.visible = visible;
    }

    [[nodiscard]] bool eligible(std::size_t index) const {
        if (index >= nodes_.size() || nodes_[index].disabled) {
            return false;
        }
        for (auto i = index; i != tree_select_no_parent; i = nodes_[i].parent) {
            if (!nodes_[i].visible) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] const String& value() const noexcept {
        return value_;
    }

    [[nodiscard]] String label() const {
        const auto found = std::ranges::find(nodes_, value_, &TreeSelectNode::key);
        return found == nodes_.end() ? value_ : found->title;
    }

    void reconcile_value(String value) {
        value_ = std::move(value);
    }

    [[nodiscard]] bool request_value(const String& value) {
        if (!value.empty()) {
            const auto found = std::ranges::find(nodes_, value, &TreeSelectNode::key);
            if (found == nodes_.end() || !eligible(static_cast<std::size_t>(found - nodes_.begin()))) {
                return false;
            }
        }
        const bool changed = value_ != value;
        if (!controlled_value_) {
            value_ = value;
        }
        return changed;
    }

    [[nodiscard]] bool open() const noexcept {
        return open_ && !suppressed_;
    }

    [[nodiscard]] bool request_open(bool value) {
        const bool changed = open() != value;
        if (value) {
            suppressed_ = false;
        }
        if (!controlled_open_) {
            open_ = value;
        }
        return changed;
    }

    void reconcile_open(bool value) {
        if (value != open_) {
            suppressed_ = false;
        }
        open_ = value;
    }

    [[nodiscard]] bool suppress() noexcept {
        const bool changed = open();
        suppressed_ = true;
        return changed;
    }

private:
    std::vector<TreeSelectNode> nodes_;
    String value_;
    bool controlled_value_{};
    bool open_{};
    bool controlled_open_{};
    bool suppressed_{};
};
} // namespace ryn::detail
