#include "component/tree_model.hpp"
#include <algorithm>
#include <stdexcept>

namespace ryn::detail {
void TreeModel::validate() const {
    if (nodes.size() > 1024) {
        throw std::invalid_argument("Tree supports at most 1024 nodes");
    }
    TreeKeys keys;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        if (n.key.empty() || std::ranges::find(keys, n.key) != keys.end() || n.depth >= 64 || n.end <= i ||
            n.end > nodes.size() ||
            (n.parent == tree_no_parent
                 ? n.depth != 0
                 : n.parent >= i || nodes[n.parent].end < n.end || n.depth != nodes[n.parent].depth + 1)) {
            throw std::invalid_argument("Tree requires unique nonempty keys and valid preorder topology of depth <=64");
        }
        keys.push_back(n.key);
    }
}

TreeKeys TreeModel::normalize(const TreeKeys& input, bool single) const {
    TreeKeys result;
    for (const auto& key : input) {
        if (std::ranges::any_of(nodes, [&](const auto& n) { return n.key == key; }) &&
            std::ranges::find(result, key) == result.end()) {
            result.push_back(key);
            if (single) {
                break;
            }
        }
    }
    return result;
}

bool TreeModel::open(std::size_t i) const {
    return i < nodes.size() && nodes[i].end > i + 1 && std::ranges::find(expanded, nodes[i].key) != expanded.end();
}

std::vector<std::size_t> TreeModel::shown() const {
    std::vector<std::size_t> result;
    if (!visible) {
        return result;
    }
    for (std::size_t i = 0; i < nodes.size();) {
        if (nodes[i].visible) {
            result.push_back(i);
            i = open(i) ? i + 1 : nodes[i].end;
        } else {
            i = nodes[i].end;
        }
    }
    return result;
}

bool TreeModel::eligible(std::size_t i) const {
    return visible && !disabled && i < nodes.size() && !nodes[i].disabled;
}

TreeKeys TreeModel::expand(std::size_t i) const {
    auto result = normalize(expanded);
    if (eligible(i) && nodes[i].end > i + 1) {
        if (open(i)) {
            std::erase(result, nodes[i].key);
        } else {
            result.push_back(nodes[i].key);
        }
    }
    return result;
}

TreeKeys TreeModel::select(std::size_t i) const {
    const auto result = normalize(selected, true);
    if (!eligible(i)) {
        return result;
    }
    return std::ranges::find(result, nodes[i].key) == result.end() ? TreeKeys{nodes[i].key} : TreeKeys{};
}

TreeChecks TreeModel::checks(const TreeKeys& input) const {
    std::vector<bool> full(nodes.size());
    std::vector<bool> half(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        const bool explicit_value = std::ranges::find(input, n.key) != input.end();
        full[i] = explicit_value ||
                  (!n.disabled && n.parent != tree_no_parent && !nodes[n.parent].disabled && full[n.parent]);
    }
    for (std::size_t i = nodes.size(); i-- > 0;) {
        if (nodes[i].disabled) {
            continue;
        }
        bool any = false;
        bool all = true;
        bool children = false;
        for (auto j = i + 1; j < nodes[i].end; j = nodes[j].end) {
            if (nodes[j].disabled) {
                continue;
            }
            children = true;
            any |= full[j] || half[j];
            all &= full[j];
        }
        if (children) {
            full[i] = all;
            half[i] = any && !all;
        }
    }
    TreeChecks result;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (full[i]) {
            result.checked.push_back(nodes[i].key);
        }
        if (half[i]) {
            result.half_checked.push_back(nodes[i].key);
        }
    }
    return result;
}

TreeChecks TreeModel::toggle_check(std::size_t i) const {
    auto result = checks(checked);
    if (!checkable || !eligible(i)) {
        return result;
    }
    const bool next = std::ranges::find(result.checked, nodes[i].key) == result.checked.end();
    // Remove ancestor aggregate keys before recomputing conduction on deselection.
    if (!next) {
        for (auto p = nodes[i].parent; p != tree_no_parent && !nodes[p].disabled; p = nodes[p].parent) {
            std::erase(result.checked, nodes[p].key);
        }
    }
    for (auto j = i; j < nodes[i].end;) {
        if (j != i && nodes[j].disabled) {
            j = nodes[j].end;
            continue;
        }
        std::erase(result.checked, nodes[j].key);
        if (next) {
            result.checked.push_back(nodes[j].key);
        }
        ++j;
    }
    return checks(result.checked);
}
} // namespace ryn::detail
