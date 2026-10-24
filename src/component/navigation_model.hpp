#pragma once

#include <ryn/breadcrumb.hpp>
#include <ryn/steps.hpp>
#include <algorithm>
#include <stdexcept>

namespace ryn::detail {
struct NavigationItemModel final {
    String key;
    bool disabled{};
    bool visible{true};
    std::optional<StepsStatus> status;
};

inline void validate_navigation_items(const std::vector<NavigationItemModel>& items) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].key.empty() || std::any_of(items.begin(), items.begin() + i,
                                                [&](const auto& other) { return other.key == items[i].key; })) {
            throw std::invalid_argument("Navigation keys must be nonempty and unique");
        }
    }
}

inline void validate_steps_status(StepsStatus status) {
    if (status != StepsStatus::Wait && status != StepsStatus::Process && status != StepsStatus::Finish &&
        status != StepsStatus::Error) {
        throw std::invalid_argument("Steps status is invalid");
    }
}

struct BreadcrumbModel final {
    std::vector<NavigationItemModel> items;

    [[nodiscard]] std::optional<std::size_t> current() const {
        for (std::size_t i = items.size(); i > 0; --i) {
            if (items[i - 1].visible) {
                return i - 1;
            }
        }
        return {};
    }

    [[nodiscard]] bool eligible(std::size_t i) const {
        return i < items.size() && items[i].visible && !items[i].disabled && current() != i;
    }
};

struct StepsModel final {
    std::vector<NavigationItemModel> items;
    std::size_t current{};
    bool controlled{};
    bool interactive{};
    StepsStatus current_status{StepsStatus::Process};

    [[nodiscard]] StepsStatus status(std::size_t i) const {
        return items.at(i).status.value_or(i < current    ? StepsStatus::Finish
                                           : i == current ? current_status
                                                          : StepsStatus::Wait);
    }

    [[nodiscard]] bool eligible(std::size_t i) const {
        return interactive && i < items.size() && items[i].visible && !items[i].disabled;
    }

    bool request(std::size_t i) {
        if (!eligible(i) || i == current) {
            return false;
        }
        if (!controlled) {
            current = i;
        }
        return true;
    }
};
} // namespace ryn::detail
