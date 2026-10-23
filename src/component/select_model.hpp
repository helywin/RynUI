#pragma once

#include <ryn/select.hpp>

#include <span>
#include <string>

namespace ryn::detail {

// Selection is authoritative; query and keyboard activity never silently select.
class SelectModel final {
public:
    SelectModel(std::vector<SelectOption> options = {}, SelectSelection value = {}, bool controlled = false);
    bool set_options(std::vector<SelectOption> options);
    bool reconcile(SelectSelection value);
    bool search(String query);
    bool activate(std::size_t index);
    bool move(int direction);
    bool boundary(bool last);
    [[nodiscard]] std::optional<SelectSelection> request(std::optional<std::size_t> index);
    [[nodiscard]] String label() const;

    [[nodiscard]] const SelectSelection& value() const noexcept {
        return value_;
    }

    [[nodiscard]] const String& query() const noexcept {
        return query_;
    }

    [[nodiscard]] const std::vector<SelectOption>& options() const noexcept {
        return options_;
    }

    [[nodiscard]] std::span<const std::size_t> filtered() const noexcept {
        return filtered_;
    }

    [[nodiscard]] std::optional<std::size_t> active() const noexcept {
        return active_;
    }

private:
    void reset_active();
    std::vector<SelectOption> options_;
    std::vector<std::string> searchable_;
    std::vector<std::size_t> filtered_;
    SelectSelection value_;
    String query_;
    std::optional<std::size_t> active_;
    bool controlled_{};
};

} // namespace ryn::detail
