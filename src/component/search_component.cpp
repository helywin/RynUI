#include <ryn/search.hpp>

#include "runtime/component_host.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/text.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

namespace ryn::detail {

struct SearchPropsAccess final {
    [[nodiscard]] static bool has_conflicting_value(const SearchProps& props) noexcept {
        return props.value_.has_value() && props.default_value_.has_value();
    }

    [[nodiscard]] static const std::optional<Prop<String>>& value(const SearchProps& props) noexcept {
        return props.value_;
    }

    [[nodiscard]] static const std::optional<String>& default_value(const SearchProps& props) noexcept {
        return props.default_value_;
    }

    [[nodiscard]] static const Prop<ControlSize>& size(const SearchProps& props) noexcept {
        return props.size_;
    }

    [[nodiscard]] static const Prop<InputStatus>& status(const SearchProps& props) noexcept {
        return props.status_;
    }

    [[nodiscard]] static const Prop<String>& placeholder(const SearchProps& props) noexcept {
        return props.placeholder_;
    }

    [[nodiscard]] static const Prop<bool>& disabled(const SearchProps& props) noexcept {
        return props.disabled_;
    }

    [[nodiscard]] static const Prop<bool>& read_only(const SearchProps& props) noexcept {
        return props.read_only_;
    }

    [[nodiscard]] static const Prop<bool>& loading(const SearchProps& props) noexcept {
        return props.loading_;
    }

    [[nodiscard]] static const Prop<bool>& enter_button(const SearchProps& props) noexcept {
        return props.enter_button_;
    }

    [[nodiscard]] static const std::optional<Prop<std::size_t>>& max_length(const SearchProps& props) noexcept {
        return props.max_length_;
    }

    [[nodiscard]] static const std::function<void(String)>& on_change(const SearchProps& props) noexcept {
        return props.on_change_;
    }

    [[nodiscard]] static const std::function<void(String, SearchSource)>& on_search(const SearchProps& props) noexcept {
        return props.on_search_;
    }

    [[nodiscard]] static const LayoutStyle& layout(const SearchProps& props) noexcept {
        return props.layout_;
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
    if (status != InputStatus::Default && status != InputStatus::Warning
            && status != InputStatus::Error) {
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
    String initial = controlled
        ? detail::read_prop(*detail::SearchPropsAccess::value(props))
        : detail::SearchPropsAccess::default_value(props).value_or(String{});
    auto bridge = std::make_shared<detail::SearchValueBridge>(std::move(initial));
    if (controlled) {
        const std::weak_ptr<detail::SearchValueBridge> weak = bridge;
        static_cast<void>(detail::connect_prop(
            bridge->scope, *detail::SearchPropsAccess::value(props),
            [weak](String next) {
                if (const auto live = weak.lock()) {
                    live->committed.set(std::move(next));
                }
            }));
    }

    auto can_submit = [disabled = detail::SearchPropsAccess::disabled(props),
                       read_only = detail::SearchPropsAccess::read_only(props),
                       loading = detail::SearchPropsAccess::loading(props)] {
        return !detail::read_prop(disabled) && !detail::read_prop(read_only)
            && !detail::read_prop(loading);
    };
    auto on_change = detail::SearchPropsAccess::on_change(props);
    auto on_search = detail::SearchPropsAccess::on_search(props);

    InputProps input;
    input.value(bridge->committed)
        .placeholder(detail::SearchPropsAccess::placeholder(props))
        .size(detail::SearchPropsAccess::size(props))
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
        .onSubmit([bridge, can_submit, on_search](String) {
            if (can_submit() && on_search) {
                auto value = bridge->committed.get();
                on_search(std::move(value), SearchSource::Input);
            }
        });
    if (detail::SearchPropsAccess::max_length(props)) {
        input.maxLength(*detail::SearchPropsAccess::max_length(props));
    }

    ButtonProps action;
    action.type(bind([enter = detail::SearchPropsAccess::enter_button(props)] {
            return detail::read_prop(enter) ? ButtonType::Primary : ButtonType::Default;
        }))
        .size(detail::SearchPropsAccess::size(props))
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

    Flex(FlexProps{}.gap(dp(0.0F)).align(FlexAlign::Center)
             .layout(detail::SearchPropsAccess::layout(props)),
         FlexContent{[bridge, input = std::move(input), action = std::move(action),
                      button = std::move(button)]() mutable {
             Input(std::move(input));
             Button(std::move(action), ButtonContent{[button = std::move(button)] {
                 if (button) {
                     detail::SlotContentAccess::function(*button)();
                 } else {
                     Text(u8"搜索");
                 }
             }});
         }});
}

} // namespace ryn
