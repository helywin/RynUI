#include <ryn/rynui.hpp>

#include <concepts>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

template <typename T>
concept HasColor = requires(T props) { props.color(0x1677FF); };

template <typename T>
concept HasBackground = requires(T props) { props.background(0xFFFFFF); };

template <typename T>
concept HasBorder = requires(T props) { props.border(1); };

template <typename T>
concept HasRadius = requires(T props) { props.radius(6); };

template <typename T>
concept HasModifier = requires(T props) { props.modifier([] {}); };

template <typename T>
concept AcceptsStringJustify = requires(T props) { props.justify("center"); };

template <typename T>
concept AcceptsStringAlign = requires(T props) { props.align("stretch"); };

template <typename T>
concept AcceptsStringGap = requires(T props) { props.gap("middle"); };

struct ForeignContentSlot final {};
using ForeignContent = ryn::SlotContent<ForeignContentSlot>;

static_assert(!HasColor<ryn::FlexProps>);
static_assert(!HasBackground<ryn::FlexProps>);
static_assert(!HasBorder<ryn::FlexProps>);
static_assert(!HasRadius<ryn::FlexProps>);
static_assert(!HasModifier<ryn::FlexProps>);
static_assert(!AcceptsStringJustify<ryn::FlexProps>);
static_assert(!AcceptsStringAlign<ryn::FlexProps>);
static_assert(!AcceptsStringGap<ryn::FlexProps>);
static_assert(!std::constructible_from<ryn::FlexContent, ForeignContent>);
static_assert(!std::constructible_from<ryn::FlexContent, ryn::Content>);

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function> void require_invalid(Function&& function, const char* message) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, message);
}

} // namespace

int main() {
    try {
        ryn::Signal<bool> vertical{false};
        ryn::Signal<bool> wrap{true};
        ryn::Signal<ryn::FlexJustify> justify{ryn::FlexJustify::Center};
        ryn::Signal<ryn::FlexAlign> align{ryn::FlexAlign::Stretch};
        ryn::Signal<ryn::SpaceAlign> space_align{ryn::SpaceAlign::End};
        ryn::Signal<ryn::LayoutGap> gap{ryn::LayoutGap{ryn::SpaceSize::Middle}};
        const auto bound_gap =
            ryn::bind([] { return ryn::LayoutGap{ryn::dp(3.0F), ryn::dp(5.0F)}; });

        auto declarations = [&] {
            ryn::Flex(ryn::FlexProps{}
                          .vertical(vertical)
                          .wrap(wrap)
                          .justify(justify)
                          .align(align)
                          .gap(gap)
                          .layout(ryn::LayoutStyle{}.width(ryn::dp(160.0F))),
                      [] { ryn::Text(u8"content"); });
            ryn::Flex(ryn::FlexProps{}
                          .gap(ryn::SpaceSize::Small)
                          .gap(ryn::dp(4.0F))
                          .gap(ryn::dp(4.0F), ryn::dp(6.0F))
                          .gap(bound_gap),
                      [] {});
        };
        static_cast<void>(declarations);
        static_cast<void>(space_align);

        require(ryn::LayoutGap{} == ryn::LayoutGap{ryn::dp(0.0F)},
                "zero LayoutGap equality is inconsistent");
        require(ryn::LayoutGap{ryn::SpaceSize::Small} == ryn::LayoutGap{ryn::SpaceSize::Small} &&
                    ryn::LayoutGap{ryn::SpaceSize::Small} != ryn::LayoutGap{ryn::dp(8.0F)} &&
                    ryn::LayoutGap{ryn::dp(3.0F), ryn::dp(5.0F)} !=
                        ryn::LayoutGap{ryn::dp(5.0F), ryn::dp(3.0F)},
                "LayoutGap source or dual-axis equality is ambiguous");
        require_invalid([] { static_cast<void>(ryn::LayoutGap{ryn::auto_length}); },
                        "auto LayoutGap was accepted");
        require_invalid([] { static_cast<void>(ryn::LayoutGap{ryn::dp(-1.0F)}); },
                        "negative LayoutGap was accepted");
        require_invalid(
            [] {
                static_cast<void>(ryn::LayoutGap{ryn::dp(std::numeric_limits<float>::quiet_NaN())});
            },
            "NaN LayoutGap was accepted");
        require_invalid(
            [] {
                static_cast<void>(ryn::LayoutGap{ryn::dp(std::numeric_limits<float>::infinity())});
            },
            "infinite LayoutGap was accepted");
        require_invalid([] { static_cast<void>(ryn::LayoutGap{static_cast<ryn::SpaceSize>(255)}); },
                        "invalid SpaceSize was accepted");

        bool missing_host = false;
        try {
            ryn::Flex(ryn::FlexProps{}, [] {});
        } catch (const std::logic_error&) {
            missing_host = true;
        }
        require(missing_host, "linked ryn::Flex entry did not diagnose a missing Host");
    } catch (const std::exception&) {
        return 1;
    }
    return 0;
}
