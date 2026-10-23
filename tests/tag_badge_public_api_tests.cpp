#include <ryn/tag.hpp>
#include <ryn/badge.hpp>
#include <ryn/theme.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::TagIcon, ryn::TagContent>);
static_assert(!std::is_convertible_v<ryn::BadgeContent, ryn::TagContent>);

int main() {
    using namespace ryn;
    Signal<bool> checked{false};
    Signal<int> count{7};
    Signal<TagColor> color{TagColor{PresetColor::Blue}};
    auto tag =
        TagProps{}.color(color).bordered(false).closable().visible(checked).disabled(false).onClose([] {}).layout(
            LayoutStyle{}.width(dp(120)));
    auto filter =
        CheckableTagProps{}.checked(checked).disabled(false).onChange([checked](bool value) { checked.set(value); });
    auto badge = BadgeProps{}
                     .count(count)
                     .overflowCount(99)
                     .showZero()
                     .dot(false)
                     .status(Status::Success)
                     .text(u8"Ready / 就绪")
                     .offset(LogicalOffset{4, -2})
                     .size(BadgeSize::Small);
    ThemeConfig theme;
    theme.tag.tokens.default_background = Color::rgba8(1, 2, 3);
    theme.badge.tokens.indicator_height = dp(24);
    static_cast<void>(tag);
    static_cast<void>(filter);
    static_cast<void>(badge);
    return resolve_theme(theme).badge().metrics.indicator_height == 24 ? 0 : 1;
}
