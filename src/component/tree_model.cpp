#include "component/tree_model.hpp"
#include <algorithm>
#include <stdexcept>
#include <set>
#include <unordered_set>

namespace ryn::detail {
void TreeModel::validate() const {
    if (nodes.size() > 100000) {
        throw std::invalid_argument("Tree supports at most 100000 nodes");
    }
    std::unordered_set<std::string_view> keys;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        if (n.key.empty() || !keys.insert(n.key.bytes()).second || n.depth >= 64 || n.end <= i ||
            n.end > nodes.size() ||
            (n.parent == tree_no_parent
                 ? n.depth != 0
                 : n.parent >= i || nodes[n.parent].end < n.end || n.depth != nodes[n.parent].depth + 1)) {
            throw std::invalid_argument("Tree requires unique nonempty keys and valid preorder topology of depth <=64");
        }
    }
}

TreeKeys TreeModel::normalize(const TreeKeys& input, bool single) const {
    TreeKeys result;
    std::set<std::string_view> keys;
    for (const auto& node : nodes) {
        keys.insert(node.key.bytes());
    }
    for (const auto& key : input) {
        if (keys.erase(key.bytes()) != 0) {
            result.push_back(key);
            if (single) {
                break;
            }
        }
    }
    return result;
}

std::vector<bool> TreeModel::opened() const {
    std::unordered_set<std::string_view> keys;
    for (const auto& key : expanded) {
        keys.insert(key.bytes());
    }
    std::vector<bool> result(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        result[i] = keys.contains(nodes[i].key.bytes());
    }
    if (filtering) {
        std::unordered_set<std::string_view> matching;
        for (const auto& key : matches) {
            matching.insert(key.bytes());
        }
        std::vector<bool> visible_branch(nodes.size());
        std::vector<bool> descendant(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            visible_branch[i] =
                nodes[i].visible && (nodes[i].parent == tree_no_parent || visible_branch[nodes[i].parent]);
            descendant[i] = visible_branch[i] && matching.contains(nodes[i].key.bytes());
        }
        for (std::size_t i = nodes.size(); i-- > 0;) {
            if (descendant[i] && nodes[i].parent != tree_no_parent) {
                result[nodes[i].parent] = true;
                descendant[nodes[i].parent] = true;
            }
        }
    }
    if (auto_expand) {
        for (std::size_t i = nodes.size(); i-- > 0;) {
            if (result[i] && nodes[i].parent != tree_no_parent) {
                result[nodes[i].parent] = true;
            }
        }
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        result[i] = result[i] && !nodes[i].leaf && (nodes[i].end > i + 1 || loadable);
    }
    return result;
}

bool TreeModel::open(std::size_t i) const {
    if (i >= nodes.size() || nodes[i].leaf || (nodes[i].end <= i + 1 && !loadable)) {
        return false;
    }
    if (auto_expand || filtering) {
        return opened()[i];
    }
    return std::ranges::find(expanded, nodes[i].key) != expanded.end();
}

std::vector<std::size_t> TreeModel::shown() const {
    std::vector<std::size_t> result;
    if (!visible) {
        return result;
    }
    const auto expanded = opened();
    std::vector<bool> keep(nodes.size(), !filtering);
    if (filtering) {
        std::unordered_set<std::string_view> match;
        for (const auto& key : matches) {
            match.insert(key.bytes());
        }
        std::vector<bool> visible_branch(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            visible_branch[i] =
                nodes[i].visible && (nodes[i].parent == tree_no_parent || visible_branch[nodes[i].parent]);
        }
        for (std::size_t i = nodes.size(); i-- > 0;) {
            keep[i] = keep[i] || (visible_branch[i] && match.contains(nodes[i].key.bytes()));
            if (keep[i] && nodes[i].parent != tree_no_parent) {
                keep[nodes[i].parent] = true;
            }
        }
    }
    for (std::size_t i = 0; i < nodes.size();) {
        if (nodes[i].visible && keep[i]) {
            result.push_back(i);
            i = filtering || expanded[i] ? i + 1 : nodes[i].end;
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
    if (eligible(i) && !nodes[i].leaf && (nodes[i].end > i + 1 || loadable)) {
        if (open(i)) {
            std::erase(result, nodes[i].key);
        } else {
            result.push_back(nodes[i].key);
        }
    }
    return result;
}

TreeKeys TreeModel::select(std::size_t i, bool additive, std::optional<std::size_t> anchor) const {
    auto result = normalize(selected, !multiple);
    if (!eligible(i) || !selectable || !nodes[i].selectable) {
        return result;
    }
    if (!multiple) {
        return std::ranges::find(result, nodes[i].key) == result.end() ? TreeKeys{nodes[i].key} : TreeKeys{};
    }
    if (anchor) {
        if (!additive) {
            result.clear();
        }
        const auto visible = shown();
        auto a = std::ranges::find(visible, *anchor);
        auto b = std::ranges::find(visible, i);
        if (a != visible.end() && b != visible.end()) {
            if (a > b) {
                std::swap(a, b);
            }
            std::unordered_set<std::string> picked;
            for (const auto& key : result) {
                picked.emplace(key.bytes());
            }
            for (; a <= b; ++a) {
                if (eligible(*a) && nodes[*a].selectable && picked.emplace(nodes[*a].key.bytes()).second) {
                    result.push_back(nodes[*a].key);
                }
            }
        }
    } else if (additive) {
        if (std::ranges::find(result, nodes[i].key) == result.end()) {
            result.push_back(nodes[i].key);
        } else {
            std::erase(result, nodes[i].key);
        }
    } else {
        result = {nodes[i].key};
    }
    return result;
}

TreeChecks TreeModel::checks(const TreeKeys& input) const {
    if (strict) {
        return {normalize(input), normalize(half)};
    }
    std::vector<bool> full(nodes.size());
    std::vector<bool> half(nodes.size());
    std::set<std::string_view> explicit_keys;
    for (const auto& key : input) {
        explicit_keys.insert(key.bytes());
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        const bool explicit_value = explicit_keys.contains(n.key.bytes());
        full[i] = explicit_value || (!n.disabled && !n.check_disabled && n.checkable && n.parent != tree_no_parent &&
                                     !nodes[n.parent].disabled && !nodes[n.parent].check_disabled &&
                                     nodes[n.parent].checkable && full[n.parent]);
    }
    for (std::size_t i = nodes.size(); i-- > 0;) {
        if (nodes[i].disabled || nodes[i].check_disabled || !nodes[i].checkable) {
            continue;
        }
        bool any = false;
        bool all = true;
        bool children = false;
        for (auto j = i + 1; j < nodes[i].end; j = nodes[j].end) {
            if (nodes[j].disabled || nodes[j].check_disabled || !nodes[j].checkable) {
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
    if (!checkable || !eligible(i) || !nodes[i].checkable || nodes[i].check_disabled) {
        return result;
    }
    const bool next = std::ranges::find(result.checked, nodes[i].key) == result.checked.end();
    if (strict) {
        std::erase(result.checked, nodes[i].key);
        std::erase(result.half_checked, nodes[i].key);
        if (next) {
            result.checked.push_back(nodes[i].key);
        }
        return result;
    }
    std::unordered_set<std::string_view> remove;
    TreeKeys added;
    if (!next) {
        for (auto p = nodes[i].parent;
             p != tree_no_parent && !nodes[p].disabled && !nodes[p].check_disabled && nodes[p].checkable;
             p = nodes[p].parent) {
            remove.insert(nodes[p].key.bytes());
        }
    }
    for (auto j = i; j < nodes[i].end;) {
        if (j != i && (nodes[j].disabled || nodes[j].check_disabled || !nodes[j].checkable)) {
            j = nodes[j].end;
            continue;
        }
        remove.insert(nodes[j].key.bytes());
        if (next) {
            added.push_back(nodes[j].key);
        }
        ++j;
    }
    std::erase_if(result.checked, [&](const String& key) { return remove.contains(key.bytes()); });
    result.checked.insert(result.checked.end(), added.begin(), added.end());
    return checks(result.checked);
}
} // namespace ryn::detail
