#pragma once
#include <ryn/mentions.hpp>
#include <span>

namespace ryn::detail {
struct MentionQuery final {
    std::size_t begin{};
    std::size_t caret{};
    String prefix;
    String query;
    friend bool operator==(const MentionQuery&, const MentionQuery&) = default;
};

struct MentionReplacement final {
    std::size_t begin{};
    std::size_t end{};
    String text;
};

[[nodiscard]] MentionReplacement mention_replacement(StringView, const MentionQuery&, StringView value,
                                                     StringView split);

void validate_mentions_config(const MentionsConfig&);
[[nodiscard]] std::optional<MentionQuery> mention_query(StringView, std::size_t anchor, std::size_t caret,
                                                        const MentionsConfig&);

class MentionsModel final {
public:
    MentionsModel(std::vector<MentionsOption> options = {}, String value = {}, bool controlled = false,
                  MentionsFilter filter = {}, bool active_first = true);
    bool set_options(std::vector<MentionsOption>);
    bool reconcile(String);
    void edit(String);
    void search(String);
    void request(String);
    void restore();
    void configure(MentionsFilter, bool active_first);
    bool activate(std::size_t);
    bool move(int direction);

    [[nodiscard]] const String& value() const noexcept {
        return value_;
    }

    [[nodiscard]] const String& query() const noexcept {
        return query_;
    }

    [[nodiscard]] const String& display() const noexcept {
        return value_;
    }

    [[nodiscard]] const std::vector<MentionsOption>& options() const noexcept {
        return options_;
    }

    [[nodiscard]] std::span<const std::size_t> filtered() const noexcept {
        return filtered_;
    }

    [[nodiscard]] std::optional<std::size_t> active() const noexcept {
        return active_;
    }

private:
    void refilter(std::optional<String> active_key = {});
    std::vector<MentionsOption> options_;
    std::vector<std::size_t> filtered_;
    String value_;
    String query_;
    std::optional<std::size_t> active_;
    MentionsFilter filter_;
    bool controlled_{};
    bool active_first_{true};
};
} // namespace ryn::detail
