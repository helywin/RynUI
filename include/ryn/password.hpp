#pragma once

#include <ryn/input.hpp>

namespace ryn {
namespace detail {
struct PasswordPropsAccess;
}

class PasswordProps final : public InputPropsBase<PasswordProps> {
public:
    PasswordProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    PasswordProps& defaultVisible(bool value) {
        default_visible_ = value;
        return *this;
    }

    PasswordProps& visibilityToggle(bool value) {
        visibility_toggle_ = value;
        return *this;
    }

    PasswordProps& onVisibleChange(std::function<void(bool)> callback) {
        on_visible_change_ = std::move(callback);
        return *this;
    }

private:
    friend struct detail::PasswordPropsAccess;
    std::optional<Prop<bool>> visible_;
    std::optional<bool> default_visible_;
    bool visibility_toggle_{true};
    std::function<void(bool)> on_visible_change_;
};

void Password(PasswordProps props);

} // namespace ryn
