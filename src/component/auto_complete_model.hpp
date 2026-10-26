#pragma once
#include <ryn/auto_complete.hpp>
#include <span>

namespace ryn::detail {
class AutoCompleteModel final {
public:
    AutoCompleteModel(std::vector<AutoCompleteOption> options = {}, String value = {}, bool controlled = false,
                      AutoCompleteFilter filter = {}, bool active_first = true);
    bool set_options(std::vector<AutoCompleteOption>);
    bool reconcile(String);
    void edit(String);
    void request(String);
    void restore();
    void configure(AutoCompleteFilter, bool active_first);
    bool activate(std::size_t);
    bool move(int direction);
    void backfill();

    [[nodiscard]] const String& value() const noexcept {
        return value_;
    }

    [[nodiscard]] const String& query() const noexcept {
        return query_;
    }

    [[nodiscard]] const String& display() const noexcept {
        return backfill_ ? *backfill_ : value_;
    }

    [[nodiscard]] const std::vector<AutoCompleteOption>& options() const noexcept {
        return options_;
    }

    [[nodiscard]] std::span<const std::size_t> filtered() const noexcept {
        return filtered_;
    }

    [[nodiscard]] std::optional<std::size_t> active() const noexcept {
        return active_;
    }

    [[nodiscard]] bool backfilled() const noexcept {
        return backfill_.has_value();
    }

private:
    void refilter(std::optional<String> active_key = {});
    std::vector<AutoCompleteOption> options_;
    std::vector<std::size_t> filtered_;
    String value_;
    String query_;
    std::optional<String> backfill_;
    std::optional<std::size_t> active_;
    AutoCompleteFilter filter_;
    bool controlled_{};
    bool active_first_{true};
};
} // namespace ryn::detail
