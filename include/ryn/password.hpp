#pragma once

#include <ryn/input.hpp>

namespace ryn {
enum class PasswordAction { Click, Hover };

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

    PasswordProps& visibilityToggle(Prop<bool> value) {
        visibility_toggle_ = std::move(value);
        return *this;
    }

    PasswordProps& toggleFocusable(Prop<bool> value) {
        toggle_focusable_ = std::move(value);
        return *this;
    }

    PasswordProps& action(Prop<PasswordAction> value) {
        action_ = std::move(value);
        return *this;
    }

    PasswordProps& iconRender(std::function<IconSource(bool)> callback) {
        icon_render_ = std::move(callback);
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
    Prop<bool> visibility_toggle_{true};
    Prop<bool> toggle_focusable_{true};
    Prop<PasswordAction> action_{PasswordAction::Click};
    std::function<IconSource(bool)> icon_render_;
    std::function<void(bool)> on_visible_change_;
};

void Password(PasswordProps props, std::optional<InputPrefix> prefix = {}, std::optional<InputSuffix> suffix = {});

} // namespace ryn
