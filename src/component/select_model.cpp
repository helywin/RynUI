#include "component/select_model.hpp"

#include "input/text_boundary.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ryn::detail {
namespace {

std::string fold(StringView text) {
    return input::text_search_key(text.bytes());
}

std::vector<std::size_t> filter(const std::vector<std::string>& labels, const std::string& query) {
    std::vector<std::size_t> result;
    result.reserve(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (labels[i].find(query) != std::string::npos) {
            result.push_back(i);
        }
    }
    return result;
}

} // namespace

SelectModel::SelectModel(std::vector<SelectOption> options, SelectSelection value, bool controlled)
    : value_(std::move(value)), controlled_(controlled) {
    set_options(std::move(options));
}

bool SelectModel::set_options(std::vector<SelectOption> options) {
    if (options == options_) {
        return false;
    }
    std::unordered_set<std::string> strings;
    std::unordered_set<std::int64_t> numbers;
    std::vector<std::string> searchable;
    searchable.reserve(options.size());
    for (const auto& option : options) {
        const auto& value = option.value.storage();
        const bool unique = std::holds_alternative<String>(value)
                                ? strings.emplace(std::get<String>(value).bytes()).second
                                : numbers.emplace(std::get<std::int64_t>(value)).second;
        if (!unique) {
            throw std::invalid_argument("Select options must have unique typed values");
        }
        searchable.push_back(fold(option.label.view()));
    }
    auto filtered = filter(searchable, fold(query_.view()));
    options_.swap(options);
    searchable_.swap(searchable);
    filtered_.swap(filtered);
    active_.reset();
    reset_active();
    return true;
}

bool SelectModel::reconcile(SelectSelection value) {
    if (value == value_) {
        return false;
    }
    value_ = std::move(value);
    active_.reset();
    reset_active();
    return true;
}

bool SelectModel::search(String query) {
    if (query == query_) {
        return false;
    }
    auto filtered = filter(searchable_, fold(query.view()));
    query_ = std::move(query);
    filtered_.swap(filtered);
    reset_active();
    return true;
}

void SelectModel::reset_active() {
    if (active_ && activate(*active_)) {
        return;
    }
    active_.reset();
    for (const auto index : filtered_) {
        if (!options_[index].disabled && value_ == SelectSelection{options_[index].value}) {
            active_ = index;
            return;
        }
    }
    static_cast<void>(boundary(false));
}

bool SelectModel::activate(std::size_t index) {
    if (index >= options_.size() || options_[index].disabled ||
        std::ranges::find(filtered_, index) == filtered_.end()) {
        return false;
    }
    active_ = index;
    return true;
}

bool SelectModel::move(int direction) {
    if (direction == 0 || filtered_.empty()) {
        return false;
    }
    auto found = active_ ? std::ranges::find(filtered_, *active_) : filtered_.end();
    std::size_t position = found == filtered_.end() ? (direction > 0 ? filtered_.size() - 1 : 0)
                                                    : static_cast<std::size_t>(found - filtered_.begin());
    for (std::size_t visited = 0; visited < filtered_.size(); ++visited) {
        position =
            direction > 0 ? (position + 1) % filtered_.size() : (position + filtered_.size() - 1) % filtered_.size();
        if (activate(filtered_[position])) {
            return true;
        }
    }
    return false;
}

bool SelectModel::boundary(bool last) {
    for (std::size_t i = 0; i < filtered_.size(); ++i) {
        const auto index = filtered_[last ? filtered_.size() - i - 1 : i];
        if (activate(index)) {
            return true;
        }
    }
    return false;
}

std::optional<SelectSelection> SelectModel::request(std::optional<std::size_t> index) {
    if (index && !activate(*index)) {
        return std::nullopt;
    }
    SelectSelection requested = index ? SelectSelection{options_[*index].value} : SelectSelection{};
    if (!controlled_) {
        value_ = requested;
    }
    return std::optional<SelectSelection>{std::in_place, std::move(requested)};
}

String SelectModel::label() const {
    if (!value_) {
        return {};
    }
    for (const auto& option : options_) {
        if (option.value == *value_) {
            return option.label;
        }
    }
    const auto& value = value_->storage();
    return std::holds_alternative<String>(value)
               ? std::get<String>(value)
               : String::from_utf8(std::to_string(std::get<std::int64_t>(value))).value();
}

} // namespace ryn::detail
