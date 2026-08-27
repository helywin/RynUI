#include <ryn/rynui.hpp>

#include <concepts>
#include <stdexcept>

namespace {

template <typename T>
concept HasBaseline = requires(T props) { props.baseline(true); };

template <typename T>
concept HasSeparator = requires(T props) { props.separator([] {}); };

template <typename T>
concept HasCompact = requires(T props) { props.compact(true); };

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
concept AcceptsStringAlign = requires(T props) { props.align("center"); };

template <typename T>
concept AcceptsStringSize = requires(T props) { props.size("middle"); };

struct ForeignContentSlot final {};
using ForeignContent = ryn::SlotContent<ForeignContentSlot>;

static_assert(!HasBaseline<ryn::SpaceProps>);
static_assert(!HasSeparator<ryn::SpaceProps>);
static_assert(!HasCompact<ryn::SpaceProps>);
static_assert(!HasColor<ryn::SpaceProps>);
static_assert(!HasBackground<ryn::SpaceProps>);
static_assert(!HasBorder<ryn::SpaceProps>);
static_assert(!HasRadius<ryn::SpaceProps>);
static_assert(!HasModifier<ryn::SpaceProps>);
static_assert(!AcceptsStringAlign<ryn::SpaceProps>);
static_assert(!AcceptsStringSize<ryn::SpaceProps>);
static_assert(!std::constructible_from<ryn::SpaceContent, ForeignContent>);
static_assert(!std::constructible_from<ryn::SpaceContent, ryn::Content>);

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        ryn::Signal<bool> vertical{false};
        ryn::Signal<bool> wrap{true};
        ryn::Signal<ryn::SpaceAlign> align{ryn::SpaceAlign::End};
        ryn::Signal<ryn::LayoutGap> size{ryn::LayoutGap{ryn::SpaceSize::Middle}};
        const auto bound_size =
            ryn::bind([] { return ryn::LayoutGap{ryn::dp(3.0F), ryn::dp(5.0F)}; });

        auto declarations = [&] {
            ryn::Space(
                ryn::SpaceProps{}
                    .vertical(vertical)
                    .wrap(wrap)
                    .align(align)
                    .size(size)
                    .layout(ryn::LayoutStyle{}.width(ryn::dp(160.0F))),
                [] { ryn::Text(u8"content"); });
            ryn::Space(
                ryn::SpaceProps{}
                    .size(ryn::SpaceSize::Small)
                    .size(ryn::dp(4.0F))
                    .size(ryn::dp(4.0F), ryn::dp(6.0F))
                    .size(bound_size),
                [] {});
        };
        static_cast<void>(declarations);

        bool missing_host = false;
        try {
            ryn::Space(ryn::SpaceProps{}, [] {});
        } catch (const std::logic_error&) {
            missing_host = true;
        }
        require(missing_host, "linked ryn::Space entry did not diagnose a missing Host");
    } catch (const std::exception&) {
        return 1;
    }
    return 0;
}
