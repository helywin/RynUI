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

} // namespace

SelectModel::SelectModel(std::vector<SelectOption> options, SelectSelection value, bool controlled)
    : value_(std::move(value)), controlled_(controlled) {
    set_options(std::move(options));
}

bool SelectModel::set_options(std::vector<SelectOption> options) {
    if (options == source_options_) {
        return false;
    }
    std::unordered_set<std::string> strings;
    std::unordered_set<std::int64_t> numbers;
    std::unordered_set<std::string> keys;
    std::vector<std::string> searchable;
    searchable.reserve(options.size());
    for (const auto& option : options) {
        const auto& value = option.value.storage();
        if (!keys.emplace(key(option).bytes()).second) {
            throw std::invalid_argument("Select option keys must be unique");
        }
        if (option.group) {
            if (option.key.empty()) {
                throw std::invalid_argument("Select group keys must be nonempty");
            }
            searchable.push_back({});
            continue;
        }
        const bool unique = std::holds_alternative<String>(value)
                                ? strings.emplace(std::get<String>(value).bytes()).second
                                : numbers.emplace(std::get<std::int64_t>(value)).second;
        if (!unique) {
            throw std::invalid_argument("Select options must have unique typed values");
        }
        searchable.push_back(fold(option.label.view()));
    }
    const auto active_key = active_ ? std::optional{key(options_[*active_])} : std::nullopt;
    auto candidate = *this;
    candidate.source_options_ = options;
    candidate.options_ = std::move(options);
    candidate.searchable_ = std::move(searchable);
    candidate.active_.reset();
    if (active_key) {
        for (std::size_t i = 0; i < candidate.options_.size(); ++i) {
            if (key(candidate.options_[i]) == *active_key) {
                candidate.active_ = i;
                break;
            }
        }
    }
    candidate.search(candidate.query_);
    *this = std::move(candidate);
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
    const auto active_key = active_ ? std::optional{key(options_[*active_])} : std::nullopt;
    options_ = source_options_;
    if (mode_ == SelectMode::Tags && !query.empty() && std::ranges::none_of(options_, [&](const auto& option) {
            return !option.group && (option.label == query || (std::holds_alternative<String>(option.value.storage()) &&
                                                               std::get<String>(option.value.storage()) == query));
        })) {
        options_.insert(options_.begin(), SelectOption{SelectValue{query}, query});
    }
    active_.reset();
    if (active_key) {
        for (std::size_t i = 0; i < options_.size(); ++i) {
            if (key(options_[i]) == *active_key) {
                active_ = i;
                break;
            }
        }
    }

    key_indices_.clear();
    key_indices_.reserve(options_.size());
    for (std::size_t i = 0; i < options_.size(); ++i) {
        key_indices_.emplace(std::string{key(options_[i]).bytes()}, i);
    }
    std::vector<std::size_t> filtered;
    std::optional<std::size_t> group;
    bool added = false;
    for (std::size_t i = 0; i < options_.size(); ++i) {
        if (options_[i].group) {
            group = i;
            added = false;
            continue;
        }
        if ((!hide_selected_ || !selected(options_[i].value)) &&
            filter_.accepts(query.view(), options_[i], filter_field_)) {
            if (group && !added) {
                filtered.push_back(*group);
                added = true;
            }
            filtered.push_back(i);
        }
    }
    if (sort_) {
        auto begin = filtered.begin();
        while (begin != filtered.end()) {
            if (options_[*begin].group) {
                ++begin;
            }
            auto end = std::find_if(begin, filtered.end(), [&](auto i) { return options_[i].group; });
            std::stable_sort(begin, end,
                             [&](auto a, auto b) { return sort_.before(options_[a], options_[b], query.view()); });
            begin = end;
        }
    }
    const bool changed = query != query_ || filtered != filtered_;
    query_ = std::move(query);
    filtered_ = std::move(filtered);
    reset_active();
    return changed;
}

void SelectModel::reset_active() {
    if (active_ && activate(*active_)) {
        return;
    }
    active_.reset();
    for (const auto index : filtered_) {
        if (!options_[index].disabled && !options_[index].group && selected(options_[index].value)) {
            active_ = index;
            return;
        }
    }
    if (active_first_) {
        static_cast<void>(boundary(false));
    }
}

bool SelectModel::activate(std::size_t index) {
    if (index >= options_.size() || options_[index].disabled || options_[index].group ||
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
    return value_ ? label(*value_) : String{};
}

String SelectModel::label(const SelectValue& selected) const {
    if (label_field_ == SelectOptionField::Label) {
        for (const auto& label : labels_) {
            if (label.value == selected) {
                return label.label;
            }
        }
        for (const auto& option : options_) {
            if (!option.group && option.value == selected) {
                return option.label;
            }
        }
    }
    const auto& value = selected.storage();
    return std::holds_alternative<String>(value)
               ? std::get<String>(value)
               : String::from_utf8(std::to_string(std::get<std::int64_t>(value))).value();
}

SelectOption SelectModel::option(const SelectValue& value) const {
    for (const auto& option : options_) {
        if (!option.group && option.value == value) {
            return option;
        }
    }
    return {value, label(value)};
}

String SelectModel::key(const SelectOption& option) {
    if (option.group) {
        return String::from_utf8("g:" + std::string{option.key.bytes()}).value();
    }
    if (!option.key.empty()) {
        return String::from_utf8("k:" + std::string{option.key.bytes()}).value();
    }
    const auto& value = option.value.storage();
    return String::from_utf8(std::holds_alternative<String>(value)
                                 ? "s:" + std::string{std::get<String>(value).bytes()}
                                 : "n:" + std::to_string(std::get<std::int64_t>(value)))
        .value();
}

std::optional<std::size_t> SelectModel::index(StringView key) const {
    const auto found = key_indices_.find(std::string{key.bytes()});
    return found == key_indices_.end() ? std::nullopt : std::optional{found->second};
}

void SelectModel::configure(SelectMode mode, SelectFilter filter, SelectSort sort, bool active_first,
                            bool hide_selected, std::size_t max_count, SelectOptionField filter_field,
                            SelectOptionField label_field) {
    if (mode != SelectMode::Single && mode != SelectMode::Multiple && mode != SelectMode::Tags) {
        throw std::invalid_argument("Invalid Select mode");
    }
    if ((filter_field != SelectOptionField::Label && filter_field != SelectOptionField::Value) ||
        (label_field != SelectOptionField::Label && label_field != SelectOptionField::Value)) {
        throw std::invalid_argument("Invalid Select option field");
    }
    auto candidate = *this;
    candidate.mode_ = mode;
    candidate.filter_ = std::move(filter);
    candidate.sort_ = std::move(sort);
    candidate.active_first_ = active_first;
    candidate.hide_selected_ = hide_selected;
    candidate.max_count_ = max_count;
    candidate.filter_field_ = filter_field;
    candidate.label_field_ = label_field;
    candidate.search(candidate.query_);
    *this = std::move(candidate);
}

bool SelectModel::selected(const SelectValue& value) const {
    return mode_ == SelectMode::Single ? value_ == SelectSelection{value}
                                       : std::ranges::find(values_, value) != values_.end();
}

bool SelectModel::reconcile_labels(std::vector<SelectLabeledValue> labels) {
    if (labels_ == labels) {
        return false;
    }
    labels_ = std::move(labels);
    return true;
}

bool SelectModel::reconcile_values(SelectValues values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (std::find(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(i), values[i]) !=
            values.begin() + static_cast<std::ptrdiff_t>(i)) {
            throw std::invalid_argument("Select values must be unique typed values");
        }
    }
    if (values_ == values) {
        return false;
    }
    values_ = std::move(values);
    search(query_);
    return true;
}

void SelectModel::accept_values(SelectValues values) {
    if (!controlled_) {
        reconcile_values(std::move(values));
    }
}

std::optional<SelectValues> SelectModel::request_values(std::optional<std::size_t> index) {
    auto result = values_;
    if (index) {
        if (!activate(*index)) {
            return std::nullopt;
        }
        const auto value = options_[*index].value;
        const auto found = std::ranges::find(result, value);
        if (found != result.end()) {
            result.erase(found);
        } else if (result.size() < max_count_) {
            result.push_back(value);
        } else {
            return std::nullopt;
        }
    } else {
        result.clear();
    }
    accept_values(result);
    return result;
}

std::optional<SelectValues> SelectModel::request_tokens(const std::vector<String>& tokens) {
    auto result = values_;
    for (const auto& token : tokens) {
        if (token.empty()) {
            continue;
        }
        auto found = std::ranges::find_if(options_, [&](const auto& option) {
            return !option.group && (option.label == token || (std::holds_alternative<String>(option.value.storage()) &&
                                                               std::get<String>(option.value.storage()) == token));
        });
        if (found != options_.end() && found->disabled) {
            continue;
        }
        if (found == options_.end() && mode_ != SelectMode::Tags) {
            continue;
        }
        const SelectValue value = found == options_.end() ? SelectValue{token} : found->value;
        if (std::ranges::find(result, value) == result.end() && result.size() < max_count_) {
            result.push_back(value);
        }
    }
    if (result == values_) {
        return std::nullopt;
    }
    accept_values(result);
    return result;
}

} // namespace ryn::detail

namespace ryn {
bool SelectFilter::accepts(StringView query, const SelectOption& option, SelectOptionField field) const {
    if (function_) {
        return (*function_)(query, option);
    }
    if (!enabled_) {
        return true;
    }
    const auto& stored = option.value.storage();
    const auto label = field == SelectOptionField::Label ? option.label
                       : std::holds_alternative<String>(stored)
                           ? std::get<String>(stored)
                           : String::from_utf8(std::to_string(std::get<std::int64_t>(stored))).value();
    return input::text_search_key(label.bytes()).find(input::text_search_key(query.bytes())) != std::string::npos;
}
} // namespace ryn
