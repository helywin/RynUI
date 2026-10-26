#include "component/auto_complete_model.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ryn {
bool AutoCompleteFilter::accepts(StringView query, const AutoCompleteOption& option) const {
    if (predicate_) {
        return (*predicate_)(query, option);
    }
    return !substring_ || option.value.bytes().find(query.bytes()) != std::string_view::npos;
}
} // namespace ryn

namespace ryn::detail {
AutoCompleteModel::AutoCompleteModel(std::vector<AutoCompleteOption> options, String value, bool controlled,
                                     AutoCompleteFilter filter, bool active_first)
    : value_(std::move(value)), query_(value_), filter_(std::move(filter)), controlled_(controlled),
      active_first_(active_first) {
    set_options(std::move(options));
    refilter();
}

bool AutoCompleteModel::set_options(std::vector<AutoCompleteOption> options) {
    std::unordered_set<std::string> keys;
    for (const auto& option : options) {
        if (option.key.empty() || !keys.emplace(option.key.bytes()).second) {
            throw std::invalid_argument("AutoComplete option keys must be nonempty and unique");
        }
    }
    if (options_ == options) {
        return false;
    }
    const auto key = active_ ? std::optional{options_[*active_].key} : std::nullopt;
    // Evaluate user predicates on a candidate first, retaining the old model on failure.
    auto candidate = *this;
    candidate.options_ = std::move(options);
    candidate.refilter(key);
    if (candidate.backfill_) {
        candidate.backfill_ = candidate.active_ && key == std::optional{candidate.options_[*candidate.active_].key}
                                  ? std::optional{candidate.options_[*candidate.active_].value}
                                  : std::nullopt;
    }
    *this = std::move(candidate);
    return true;
}

bool AutoCompleteModel::reconcile(String value) {
    if (value_ == value) {
        return false;
    }
    value_ = std::move(value);
    restore();
    return true;
}

void AutoCompleteModel::edit(String value) {
    query_ = value;
    backfill_.reset();
    if (!controlled_) {
        value_ = std::move(value);
    }
    refilter();
}

void AutoCompleteModel::request(String value) {
    if (!controlled_) {
        value_ = std::move(value);
    }
    restore();
}

void AutoCompleteModel::restore() {
    query_ = value_;
    backfill_.reset();
    refilter();
}

void AutoCompleteModel::configure(AutoCompleteFilter filter, bool active_first) {
    auto candidate = *this;
    candidate.filter_ = std::move(filter);
    candidate.active_first_ = active_first;
    candidate.refilter();
    candidate.backfill_.reset();
    *this = std::move(candidate);
}

void AutoCompleteModel::refilter(std::optional<String> active_key) {
    filtered_.clear();
    active_.reset();
    for (std::size_t i = 0; i < options_.size(); ++i) {
        if (filter_.accepts(query_.view(), options_[i])) {
            filtered_.push_back(i);
            if (!options_[i].disabled && active_key == std::optional{options_[i].key}) {
                active_ = i;
            }
        }
    }
    if (!active_ && active_first_) {
        for (auto i : filtered_) {
            if (!options_[i].disabled) {
                active_ = i;
                break;
            }
        }
    }
}

bool AutoCompleteModel::activate(std::size_t index) {
    if (index >= options_.size() || options_[index].disabled ||
        std::ranges::find(filtered_, index) == filtered_.end() || active_ == index) {
        return false;
    }
    active_ = index;
    return true;
}

bool AutoCompleteModel::move(int direction) {
    if (filtered_.empty()) {
        return false;
    }
    auto it = active_ ? std::ranges::find(filtered_, *active_) : filtered_.end();
    auto index = it == filtered_.end() ? (direction > 0 ? -1 : static_cast<int>(filtered_.size()))
                                       : static_cast<int>(it - filtered_.begin());
    for (std::size_t n = 0; n < filtered_.size(); ++n) {
        index = (index + direction + static_cast<int>(filtered_.size())) % static_cast<int>(filtered_.size());
        if (!options_[filtered_[index]].disabled) {
            return activate(filtered_[index]);
        }
    }
    return false;
}

void AutoCompleteModel::backfill() {
    if (active_) {
        backfill_ = options_[*active_].value;
    }
}
} // namespace ryn::detail
