#include <ryn/divider.hpp>

#include <concepts>
#include <type_traits>

template <typename T>
concept VariantValue = requires(ryn::DividerProps props, T value) { props.variant(value); };

static_assert(VariantValue<ryn::DividerVariant>);
static_assert(VariantValue<ryn::Signal<ryn::DividerVariant>>);
static_assert(VariantValue<ryn::Prop<ryn::DividerVariant>>);
static_assert(!VariantValue<int> && !VariantValue<const char*>);
static_assert(!std::constructible_from<ryn::DividerText, ryn::Content>);

int main() {
    ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Small};
    ryn::Signal<ryn::DividerDirection> direction{ryn::DividerDirection::RightToLeft};
    ryn::DividerProps props;
    props.variant(ryn::DividerVariant::Dotted)
        .size(size)
        .direction(direction)
        .orientation(ryn::DividerOrientation::End)
        .orientationMargin(ryn::DividerOrientationMargin::length(ryn::dp(20)));
}
