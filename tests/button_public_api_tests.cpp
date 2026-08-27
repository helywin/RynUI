#include <ryn/rynui.hpp>

#include <concepts>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

template <typename T>
concept HasColor = requires(T props) {
    props.color(0x1677FF);
};

template <typename T>
concept HasFont = requires(T props) {
    props.font(14);
};

template <typename T>
concept HasBorder = requires(T props) {
    props.border(1);
};

template <typename T>
concept HasRadius = requires(T props) {
    props.radius(6);
};

template <typename T>
concept HasShader = requires(T props) {
    props.shader("button");
};

template <typename T>
concept HasPrefix = requires(T props) {
    props.prefix([] {});
};

template <typename T>
concept HasSuffix = requires(T props) {
    props.suffix([] {});
};

template <typename T>
concept HasFooter = requires(T props) {
    props.footer([] {});
};

template <typename Callback>
concept AcceptsClick = requires(ryn::ButtonProps props, Callback callback) {
    props.onClick(callback);
};

struct ForeignContentSlot final {};
using ForeignContent = ryn::SlotContent<ForeignContentSlot>;

static_assert(!HasColor<ryn::ButtonProps>);
static_assert(!HasFont<ryn::ButtonProps>);
static_assert(!HasBorder<ryn::ButtonProps>);
static_assert(!HasRadius<ryn::ButtonProps>);
static_assert(!HasShader<ryn::ButtonProps>);
static_assert(!HasPrefix<ryn::ButtonProps>);
static_assert(!HasSuffix<ryn::ButtonProps>);
static_assert(!HasFooter<ryn::ButtonProps>);
static_assert(AcceptsClick<std::function<void()>>);
static_assert(!AcceptsClick<std::function<void(int)>>);
static_assert(!std::constructible_from<ryn::ButtonContent, ForeignContent>);
static_assert(!std::constructible_from<ryn::ButtonContent, ryn::Content>);

} // namespace

int main() {
    ryn::Signal<ryn::ButtonType> type{ryn::ButtonType::Primary};
    ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Middle};
    ryn::Signal<bool> disabled{false};
    ryn::Signal<bool> loading{false};
    auto bound_type = ryn::bind([] { return ryn::ButtonType::Default; });
    int clicks = 0;

    auto declarations = [&] {
        ryn::Button(
            ryn::ButtonProps{}
                .type(type)
                .size(size)
                .disabled(disabled)
                .loading(loading)
                .onClick([&] { ++clicks; })
                .layout(ryn::LayoutStyle{}.width(ryn::dp(160.0F))),
            [] { ryn::Text(u8"确定"); });
        ryn::Button(
            ryn::ButtonProps{}
                .type(bound_type)
                .size(ryn::ControlSize::Small),
            [] {});
    };
    static_cast<void>(declarations);

    bool linked_public_entry_diagnosed_missing_host = false;
    try {
        ryn::Button(ryn::ButtonProps{}, [] {});
    } catch (const std::logic_error&) {
        linked_public_entry_diagnosed_missing_host = true;
    }
    return linked_public_entry_diagnosed_missing_host ? clicks : 1;
}
