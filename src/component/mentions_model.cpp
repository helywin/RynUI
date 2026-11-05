#include "component/mentions_model.hpp"
#include <algorithm>
#include "input/text_boundary.hpp"
#include <stdexcept>
#include <unordered_set>

namespace ryn {
bool MentionsFilter::accepts(StringView query, const MentionsOption& option) const {
    if (predicate_) {
        return (*predicate_)(query, option);
    }
    return query.empty() || !substring_ ||
           input::text_search_key(option.value.bytes()).find(input::text_search_key(query.bytes())) !=
               std::string_view::npos;
}

bool MentionsSearchValidator::accepts(StringView query, const MentionsConfig& config) const {
    if (callback_) {
        return (*callback_)(query, config);
    }
    const auto bytes = query.bytes();
    return bytes.find_first_of("\r\n\t") == std::string_view::npos &&
           (config.split.empty() || bytes.find(config.split.bytes()) == std::string_view::npos);
}

std::vector<MentionEntity> getMentions(StringView value, MentionsConfig config) {
    detail::validate_mentions_config(config);
    std::vector<MentionEntity> result;
    const auto text = value.bytes();
    input::TextBoundaryMap boundaries;
    if (!boundaries.assign(text)) {
        return result;
    }
    const auto add = [&](std::size_t begin, std::size_t end) {
        const auto token = text.substr(begin, end - begin);
        const String* chosen{};
        for (const auto& prefix : config.prefix) {
            if (token.starts_with(prefix.bytes()) && boundaries.is_boundary(begin + prefix.bytes().size()) &&
                (!chosen || prefix.bytes().size() > chosen->size_bytes())) {
                chosen = &prefix;
            }
        }
        if (chosen && token.size() > chosen->size_bytes()) {
            result.push_back({*chosen, String::from_utf8(token.substr(chosen->size_bytes())).value()});
        }
    };
    if (config.split.empty()) {
        const auto graphemes = boundaries.grapheme_bytes();
        for (std::size_t i = 1; i < graphemes.size(); ++i) {
            add(graphemes[i - 1], graphemes[i]);
        }
        return result;
    }
    std::size_t begin{};
    while (begin <= text.size()) {
        auto end = text.find(config.split.bytes(), begin);
        while (end != std::string_view::npos &&
               (!boundaries.is_boundary(end) || !boundaries.is_boundary(end + config.split.bytes().size()))) {
            end = text.find(config.split.bytes(), end + 1);
        }
        add(begin, end == std::string_view::npos ? text.size() : end);
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + config.split.bytes().size();
    }
    return result;
}
} // namespace ryn

namespace ryn::detail {
void validate_mentions_config(const MentionsConfig& config) {
    for (const auto& prefix : config.prefix) {
        if (prefix.empty() || prefix.bytes().find_first_of("\r\n") != std::string_view::npos) {
            throw std::invalid_argument("Mentions prefixes must be nonempty and single-line");
        }
    }
}

std::optional<MentionQuery> mention_query(StringView value, std::size_t anchor, std::size_t caret,
                                          const MentionsConfig& config) {
    validate_mentions_config(config);
    input::TextBoundaryMap boundaries;
    const auto bytes = value.bytes();
    if (!boundaries.assign(bytes) || anchor != caret || !boundaries.is_boundary(caret)) {
        return {};
    }
    std::optional<MentionQuery> result;
    std::size_t latest_end{};
    for (const auto begin : boundaries.grapheme_bytes()) {
        if (begin >= caret) {
            break;
        }
        for (const auto& prefix : config.prefix) {
            const auto end = begin + prefix.bytes().size();
            if (end > caret || !boundaries.is_boundary(end) ||
                bytes.substr(begin, prefix.bytes().size()) != prefix.bytes()) {
                continue;
            }
            if (!result || end > latest_end ||
                (end == latest_end && prefix.bytes().size() > result->prefix.bytes().size())) {
                latest_end = end;
                result = MentionQuery{begin, caret, prefix, String::from_utf8(bytes.substr(end, caret - end)).value()};
            }
        }
    }
    return result;
}

MentionReplacement mention_replacement(StringView value, const MentionQuery& query, StringView chosen,
                                       StringView split) {
    const auto bytes = value.bytes();
    input::TextBoundaryMap text_boundaries;
    input::TextBoundaryMap chosen_boundaries;
    input::TextBoundaryMap query_boundaries;
    if (!text_boundaries.assign(bytes) || !chosen_boundaries.assign(chosen.bytes()) ||
        !query_boundaries.assign(query.query.bytes()) || query.begin > query.caret ||
        !text_boundaries.is_boundary(query.begin) || !text_boundaries.is_boundary(query.caret)) {
        throw std::invalid_argument("Mentions replacement requires legal native grapheme boundaries");
    }
    std::string inserted;
    if (query.begin && !split.empty() && !bytes.substr(0, query.begin).ends_with(split.bytes()) &&
        bytes[query.begin - 1] != '\n') {
        inserted += split.bytes();
    }
    inserted += query.prefix.bytes();
    inserted += chosen.bytes();
    inserted += split.bytes();
    auto end = query.caret;
    const auto stops = chosen_boundaries.grapheme_bytes();
    // Reuse the matching suffix after the caret, without ever cutting a code point or grapheme.
    auto index = std::min(query_boundaries.grapheme_count(), chosen_boundaries.grapheme_count());
    while (index + 1 < stops.size() && end < bytes.size() &&
           (split.empty() || !bytes.substr(end).starts_with(split.bytes()))) {
        const auto next = text_boundaries.next(end);
        const auto target = chosen.bytes().substr(stops[index], stops[index + 1] - stops[index]);
        if (input::text_search_key(bytes.substr(end, next - end)) != input::text_search_key(target)) {
            break;
        }
        end = next;
        ++index;
    }
    if (!split.empty() && bytes.substr(end).starts_with(split.bytes()) &&
        text_boundaries.is_boundary(end + split.size_bytes())) {
        end += split.size_bytes();
    }
    return {query.begin, end, String::from_utf8(inserted).value()};
}

MentionsModel::MentionsModel(std::vector<MentionsOption> options, String value, bool controlled, MentionsFilter filter,
                             bool active_first)
    : value_(std::move(value)), query_(), filter_(std::move(filter)), controlled_(controlled),
      active_first_(active_first) {
    set_options(std::move(options));
    refilter();
}

bool MentionsModel::set_options(std::vector<MentionsOption> options) {
    std::unordered_set<std::string> keys;
    for (const auto& option : options) {
        if (option.key.empty() || !keys.emplace(option.key.bytes()).second) {
            throw std::invalid_argument("Mentions option keys must be nonempty and unique");
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
    *this = std::move(candidate);
    return true;
}

bool MentionsModel::reconcile(String value) {
    if (value_ == value) {
        return false;
    }
    value_ = std::move(value);
    restore();
    return true;
}

void MentionsModel::edit(String value) {
    if (!controlled_) {
        value_ = std::move(value);
    }
    refilter();
}

void MentionsModel::search(String query) {
    if (query_ == query) {
        return;
    }
    query_ = std::move(query);
    refilter();
}

void MentionsModel::request(String value) {
    if (!controlled_) {
        value_ = std::move(value);
    }
    restore();
}

void MentionsModel::restore() {
    query_ = String{};
    refilter();
}

void MentionsModel::configure(MentionsFilter filter, bool active_first) {
    auto candidate = *this;
    candidate.filter_ = std::move(filter);
    candidate.active_first_ = active_first;
    candidate.refilter();
    *this = std::move(candidate);
}

void MentionsModel::refilter(std::optional<String> active_key) {
    filtered_.clear();
    active_.reset();
    std::optional<std::size_t> group;
    bool group_added = false;
    for (std::size_t i = 0; i < options_.size(); ++i) {
        if (options_[i].group) {
            group = i;
            group_added = false;
            continue;
        }
        if (filter_.accepts(query_.view(), options_[i])) {
            if (group && !group_added) {
                filtered_.push_back(*group);
                group_added = true;
            }
            filtered_.push_back(i);
            if (!options_[i].disabled && !options_[i].group && active_key == std::optional{options_[i].key}) {
                active_ = i;
            }
        }
    }
    if (!active_ && active_first_) {
        for (auto i : filtered_) {
            if (!options_[i].disabled && !options_[i].group) {
                active_ = i;
                break;
            }
        }
    }
}

bool MentionsModel::activate(std::size_t index) {
    if (index >= options_.size() || (options_[index].disabled || options_[index].group) ||
        std::ranges::find(filtered_, index) == filtered_.end() || active_ == index) {
        return false;
    }
    active_ = index;
    return true;
}

bool MentionsModel::move(int direction) {
    if (filtered_.empty()) {
        return false;
    }
    auto it = active_ ? std::ranges::find(filtered_, *active_) : filtered_.end();
    auto index = it == filtered_.end() ? (direction > 0 ? -1 : static_cast<int>(filtered_.size()))
                                       : static_cast<int>(it - filtered_.begin());
    for (std::size_t n = 0; n < filtered_.size(); ++n) {
        index = (index + direction + static_cast<int>(filtered_.size())) % static_cast<int>(filtered_.size());
        if (!options_[filtered_[index]].disabled && !options_[filtered_[index]].group) {
            return activate(filtered_[index]);
        }
    }
    return false;
}

} // namespace ryn::detail
