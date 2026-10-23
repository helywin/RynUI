#include "component/tabs_model.hpp"
#include <stdexcept>
#include <unordered_set>

namespace ryn::detail {
TabsModel::TabsModel(std::vector<TabOption> options, std::optional<String> controlled_value,
                     std::optional<String> default_value)
    : options_(std::move(options)), controlled_(controlled_value.has_value()) {
    if (controlled_value && default_value) {
        throw std::invalid_argument("Tabs activeKey and defaultActiveKey are mutually exclusive");
    }
    std::unordered_set<std::string_view> keys;
    for (const auto& option : options_) {
        if (option.key.empty() || !keys.insert(option.key.bytes()).second) {
            throw std::invalid_argument("Tabs keys must be nonempty and unique");
        }
    }
    if (controlled_value) {
        value_ = std::move(*controlled_value);
    } else if (default_value) {
        value_ = std::move(*default_value);
    } else if (const auto first = first_enabled()) {
        value_ = options_[*first].key;
    }
}

std::optional<std::size_t> TabsModel::active() const {
    for (std::size_t index = 0; index < options_.size(); ++index) {
        if (options_[index].key == value_) {
            return index;
        }
    }
    return {};
}

std::optional<std::size_t> TabsModel::first_enabled() const {
    for (std::size_t index = 0; index < options_.size(); ++index) {
        if (!options_[index].disabled) {
            return index;
        }
    }
    return {};
}

std::optional<std::size_t> TabsModel::last_enabled() const {
    for (std::size_t index = options_.size(); index > 0; --index) {
        if (!options_[index - 1].disabled) {
            return index - 1;
        }
    }
    return {};
}

std::optional<std::size_t> TabsModel::next_enabled(std::optional<std::size_t> from, int direction) const {
    if (direction != 1 && direction != -1) {
        throw std::invalid_argument("Tabs navigation direction must be -1 or 1");
    }
    if (!from) {
        return direction == 1 ? first_enabled() : last_enabled();
    }
    if (*from >= options_.size()) {
        throw std::out_of_range("Tabs navigation index is invalid");
    }
    auto index = *from;
    for (std::size_t count = 0; count < options_.size(); ++count) {
        index = direction == 1 ? (index + 1) % options_.size() : (index + options_.size() - 1) % options_.size();
        if (!options_[index].disabled) {
            return index;
        }
    }
    return {};
}

std::optional<String> TabsModel::request(std::size_t index) {
    if (index >= options_.size() || options_[index].disabled || options_[index].key == value_) {
        return {};
    }
    auto requested = options_[index].key;
    if (!controlled_) {
        value_ = requested;
    }
    return requested;
}

void TabsModel::reconcile(String value) {
    if (!controlled_) {
        throw std::logic_error("Uncontrolled Tabs cannot reconcile an external activeKey");
    }
    value_ = std::move(value);
}

void TabsModel::set_disabled(std::size_t index, bool disabled) {
    if (index >= options_.size()) {
        throw std::out_of_range("Tabs disabled index is invalid");
    }
    options_[index].disabled = disabled;
}
} // namespace ryn::detail
