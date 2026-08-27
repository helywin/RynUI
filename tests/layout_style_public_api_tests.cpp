#include <ryn/layout_style.hpp>

#include <concepts>
#include <type_traits>

namespace {

template <typename T>
concept AcceptsRawWidth = requires(T style) {
    style.width(12.0F);
};

template <typename T>
concept HasColor = requires(T style) {
    style.color(ryn::dp(1.0F));
};

template <typename T>
concept HasPadding = requires(T style) {
    style.padding(ryn::dp(1.0F));
};

template <typename T>
concept HasBackground = requires(T style) {
    style.background(ryn::dp(1.0F));
};

template <typename T>
concept HasFont = requires(T style) {
    style.font(ryn::dp(1.0F));
};

template <typename T>
concept HasModifier = requires(T style) {
    style.modifier(ryn::dp(1.0F));
};

static_assert(!std::is_convertible_v<float, ryn::LogicalLength>);
static_assert(!AcceptsRawWidth<ryn::LayoutStyle>);
static_assert(!HasColor<ryn::LayoutStyle>);
static_assert(!HasPadding<ryn::LayoutStyle>);
static_assert(!HasBackground<ryn::LayoutStyle>);
static_assert(!HasFont<ryn::LayoutStyle>);
static_assert(!HasModifier<ryn::LayoutStyle>);

} // namespace

int main() {
    ryn::Signal<ryn::LogicalLength> width{ryn::dp(320.0F)};
    auto height = ryn::bind([] { return ryn::dp(48.0F); });
    ryn::LayoutStyle style;
    style.width(width)
        .height(height)
        .min_width(ryn::dp(120.0F))
        .max_width(ryn::auto_length)
        .min_height(ryn::dp(24.0F))
        .max_height(ryn::dp(96.0F))
        .margin_left(ryn::dp(8.0F))
        .margin_top(ryn::dp(4.0F))
        .margin_right(ryn::dp(8.0F))
        .margin_bottom(ryn::dp(4.0F));
    static_cast<void>(style);
    return 0;
}
