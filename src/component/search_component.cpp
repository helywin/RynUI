#include <ryn/search.hpp>

#include "runtime/component_host.hpp"
#include "runtime/prop_connection.hpp"
#include "component/space_compact.hpp"

#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/icon.hpp>
#include <ryn/text.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

namespace ryn::detail {

struct SearchPropsAccess final {
    [[nodiscard]] static InputProps input(const SearchProps& props) {
        InputProps result;
        result.common_ = props.common_;
        result.common_.value_.reset();
        result.common_.default_value_.reset();
        result.common_.layout_ = {};
        return result;
    }

    [[nodiscard]] static bool has_conflicting_value(const SearchProps& props) noexcept {
        return props.common_.value_.has_value() && props.common_.default_value_.has_value();
    }

    [[nodiscard]] static const std::optional<Prop<String>>& value(const SearchProps& props) noexcept {
        return props.common_.value_;
    }

    [[nodiscard]] static const std::optional<String>& default_value(const SearchProps& props) noexcept {
        return props.common_.default_value_;
    }

    [[nodiscard]] static const Prop<ControlSize>& size(const SearchProps& props) noexcept {
        return props.common_.size_;
    }

    [[nodiscard]] static bool explicit_size(const SearchProps& props) noexcept {
        return props.common_.explicit_size_;
    }

    [[nodiscard]] static const Prop<InputStatus>& status(const SearchProps& props) noexcept {
        return props.common_.status_;
    }

    [[nodiscard]] static const Prop<InputVariant>& variant(const SearchProps& props) noexcept {
        return props.common_.variant_;
    }

    [[nodiscard]] static const Prop<String>& placeholder(const SearchProps& props) noexcept {
        return props.common_.placeholder_;
    }

    [[nodiscard]] static const Prop<bool>& disabled(const SearchProps& props) noexcept {
        return props.common_.disabled_;
    }

    [[nodiscard]] static const Prop<bool>& read_only(const SearchProps& props) noexcept {
        return props.common_.read_only_;
    }

    [[nodiscard]] static const Prop<bool>& loading(const SearchProps& props) noexcept {
        return props.loading_;
    }

    [[nodiscard]] static const Prop<bool>& enter_button(const SearchProps& props) noexcept {
        return props.enter_button_;
    }

    [[nodiscard]] static const std::optional<Prop<std::size_t>>& max_length(const SearchProps& props) noexcept {
        return props.common_.max_length_;
    }

    [[nodiscard]] static const std::function<void(String)>& on_change(const SearchProps& props) noexcept {
        return props.common_.on_change_;
    }

    [[nodiscard]] static const std::function<void(String)>& on_submit(const SearchProps& props) noexcept {
        return props.common_.on_submit_;
    }

    [[nodiscard]] static const std::function<void(String, SearchSource)>& on_search(const SearchProps& props) noexcept {
        return props.on_search_;
    }

    [[nodiscard]] static const LayoutStyle& layout(const SearchProps& props) noexcept {
        return props.common_.layout_;
    }
};

namespace {

struct SearchValueBridge final {
    explicit SearchValueBridge(String initial) : committed(std::move(initial)) {}

    Signal<String> committed;
    Scope scope;
};

void validate(ControlSize size) {
    if (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large) {
        throw std::invalid_argument("Invalid Search size");
    }
}

void validate(InputStatus status) {
    if (status != InputStatus::Default && status != InputStatus::Warning && status != InputStatus::Error) {
        throw std::invalid_argument("Invalid Search status");
    }
}

} // namespace

} // namespace ryn::detail

namespace ryn {

void Search(SearchProps props, std::optional<SearchButtonContent> button) {
    if (detail::SearchPropsAccess::has_conflicting_value(props)) {
        throw std::invalid_argument("Search value and defaultValue are mutually exclusive");
    }
    detail::validate(detail::read_prop(detail::SearchPropsAccess::size(props)));
    detail::validate(detail::read_prop(detail::SearchPropsAccess::status(props)));

    const bool controlled = detail::SearchPropsAccess::value(props).has_value();
    String initial = controlled ? detail::read_prop(*detail::SearchPropsAccess::value(props))
                                : detail::SearchPropsAccess::default_value(props).value_or(String{});
    auto bridge = std::make_shared<detail::SearchValueBridge>(std::move(initial));
    if (controlled) {
        const std::weak_ptr<detail::SearchValueBridge> weak = bridge;
        static_cast<void>(
            detail::connect_prop(bridge->scope, *detail::SearchPropsAccess::value(props), [weak](String next) {
                if (const auto live = weak.lock()) {
                    live->committed.set(std::move(next));
                }
            }));
    }

    auto can_submit = [disabled = detail::SearchPropsAccess::disabled(props),
                       read_only = detail::SearchPropsAccess::read_only(props),
                       loading = detail::SearchPropsAccess::loading(props)] {
        return !detail::read_prop(disabled) && !detail::read_prop(read_only) && !detail::read_prop(loading);
    };
    auto on_change = detail::SearchPropsAccess::on_change(props);
    auto on_search = detail::SearchPropsAccess::on_search(props);

    InputProps input = detail::SearchPropsAccess::input(props);
    input.value(bridge->committed)
        .placeholder(detail::SearchPropsAccess::placeholder(props))
        .status(detail::SearchPropsAccess::status(props))
        .disabled(detail::SearchPropsAccess::disabled(props))
        .readOnly(detail::SearchPropsAccess::read_only(props))
        .layout(LayoutStyle{}.flex_grow(1.0F).flex_shrink(1.0F).min_width(dp(0.0F)))
        .onChange([bridge, controlled, on_change](String next) {
            if (!controlled) {
                bridge->committed.set(next);
            }
            if (on_change) {
                on_change(std::move(next));
            }
        })
        .onSubmit([bridge, can_submit, on_search, on_submit = detail::SearchPropsAccess::on_submit(props)](String) {
            if (can_submit()) {
                auto value = bridge->committed.get();
                if (on_submit) {
                    on_submit(value);
                }
                if (on_search) {
                    on_search(std::move(value), SearchSource::Input);
                }
            }
        });
    if (detail::SearchPropsAccess::max_length(props)) {
        input.maxLength(*detail::SearchPropsAccess::max_length(props));
    }

    ButtonProps action;
    action
        .type(bind([enter = detail::SearchPropsAccess::enter_button(props)] {
            return detail::read_prop(enter) ? ButtonType::Primary : ButtonType::Default;
        }))
        .variant(bind([enter = detail::SearchPropsAccess::enter_button(props),
                       variant = detail::SearchPropsAccess::variant(props)] {
            if (detail::read_prop(variant) != InputVariant::Outlined) {
                return ButtonVariant::Text;
            }
            return detail::read_prop(enter) ? ButtonVariant::Solid : ButtonVariant::Outlined;
        }))
        .disabled(bind([disabled = detail::SearchPropsAccess::disabled(props),
                        read_only = detail::SearchPropsAccess::read_only(props)] {
            return detail::read_prop(disabled) || detail::read_prop(read_only);
        }))
        .loading(detail::SearchPropsAccess::loading(props))
        .layout(LayoutStyle{}.flex_shrink(0.0F))
        .onClick([bridge, can_submit, on_search] {
            if (can_submit() && on_search) {
                auto value = bridge->committed.get();
                on_search(std::move(value), SearchSource::Input);
            }
        });

    if (detail::SearchPropsAccess::explicit_size(props)) {
        input.size(detail::SearchPropsAccess::size(props));
        action.size(detail::SearchPropsAccess::size(props));
    }
    auto content = [bridge, input = std::move(input), action = std::move(action),
                    button = std::move(button)]() mutable {
        Input(std::move(input));
        Button(std::move(action), ButtonContent{[button = std::move(button)] {
                   if (button) {
                       detail::SlotContentAccess::function (*button)();
                   } else {
                       Icon(IconProps{}.name(IconName::SearchOutlined));
                   }
               }});
    };
    SpaceCompact(SpaceCompactProps{}.layout(detail::SearchPropsAccess::layout(props)),
                 SpaceCompactContent{std::move(content)});
}

} // namespace ryn
