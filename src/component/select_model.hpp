#pragma once

#include <ryn/select.hpp>

#include <span>
#include <string>
#include <unordered_map>

namespace ryn::detail {

// Selection is authoritative; query and keyboard activity never silently select.
class SelectModel final {
public:
    SelectModel(std::vector<SelectOption> options = {}, SelectSelection value = {}, bool controlled = false);
    void configure(SelectMode mode, SelectFilter filter = {}, SelectSort sort = {}, bool active_first = true,
                   bool hide_selected = false, std::size_t max_count = std::numeric_limits<std::size_t>::max(),
                   SelectOptionField filter_field = SelectOptionField::Label,
                   SelectOptionField label_field = SelectOptionField::Label);
    bool reconcile_values(SelectValues values);
    bool reconcile_labels(std::vector<SelectLabeledValue> labels);
    [[nodiscard]] std::optional<SelectValues> request_values(std::optional<std::size_t> index);
    [[nodiscard]] std::optional<SelectValues> request_tokens(const std::vector<String>& tokens);
    void accept_values(SelectValues values);

    [[nodiscard]] const SelectValues& values() const noexcept {
        return values_;
    }

    [[nodiscard]] bool selected(const SelectValue& value) const;

    [[nodiscard]] SelectMode mode() const noexcept {
        return mode_;
    }

    [[nodiscard]] String label(const SelectValue& value) const;
    [[nodiscard]] SelectOption option(const SelectValue& value) const;
    [[nodiscard]] static String key(const SelectOption& option);
    [[nodiscard]] std::optional<std::size_t> index(StringView key) const;
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
    std::unordered_map<std::string, std::size_t> key_indices_;
    std::vector<SelectOption> source_options_;
    std::vector<SelectOption> options_;
    std::vector<std::string> searchable_;
    std::vector<std::size_t> filtered_;
    SelectSelection value_;
    String query_;
    std::optional<std::size_t> active_;
    bool controlled_{};
    SelectMode mode_{SelectMode::Single};
    SelectValues values_;
    std::vector<SelectLabeledValue> labels_;
    SelectFilter filter_;
    SelectSort sort_;
    bool active_first_{true};
    bool hide_selected_{};
    std::size_t max_count_{std::numeric_limits<std::size_t>::max()};
    SelectOptionField filter_field_{SelectOptionField::Label};
    SelectOptionField label_field_{SelectOptionField::Label};
};

} // namespace ryn::detail
