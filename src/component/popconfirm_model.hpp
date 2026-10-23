#pragma once

#include <optional>

namespace ryn::detail {
class PopconfirmModel final {
public:
    PopconfirmModel(bool controlled, bool open) : controlled_(controlled), open_(open) {}

    void reconcile(bool open);
    [[nodiscard]] bool request(bool open);
    [[nodiscard]] bool suppress();

    [[nodiscard]] bool open() const noexcept {
        return open_ && !suppressed_;
    }

private:
    bool controlled_{};
    bool open_{};
    bool suppressed_{};
    std::optional<bool> requested_;
};
} // namespace ryn::detail
