#pragma once
#include <optional>
#include <span>
#include <vector>
#include <ryn/string.hpp>

namespace ryn::detail {
struct TabOption final {
    String key;
    bool disabled{};
};

class TabsModel final {
public:
    TabsModel(std::vector<TabOption> options, std::optional<String> controlled_value = {},
              std::optional<String> default_value = {});

    [[nodiscard]] const String& value() const noexcept {
        return value_;
    }

    [[nodiscard]] std::span<const TabOption> options() const noexcept {
        return options_;
    }

    [[nodiscard]] bool controlled() const noexcept {
        return controlled_;
    }

    [[nodiscard]] std::optional<std::size_t> active() const;
    [[nodiscard]] std::optional<std::size_t> first_enabled() const;
    [[nodiscard]] std::optional<std::size_t> last_enabled() const;
    [[nodiscard]] std::optional<std::size_t> next_enabled(std::optional<std::size_t> from, int direction) const;
    [[nodiscard]] std::optional<String> request(std::size_t index);
    void reconcile(String value);
    void set_disabled(std::size_t index, bool disabled);

private:
    std::vector<TabOption> options_;
    String value_;
    bool controlled_{};
};
} // namespace ryn::detail
