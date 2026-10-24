#include <ryn/rynui.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::SkeletonContent, ryn::SpinContent>);
static_assert(std::is_same_v<decltype(ryn::SkeletonProps{}.loading(true)), ryn::SkeletonProps&>);
static_assert(std::is_same_v<decltype(ryn::SkeletonAvatarProps{}.shape(ryn::SkeletonAvatarShape::Square)),
                             ryn::SkeletonAvatarProps&>);

int main() {
    ryn::Signal<bool> loading{true};
    ryn::Signal<ryn::SkeletonTitle> title{ryn::SkeletonTitle{}};
    ryn::Signal<ryn::SkeletonParagraph> paragraph{ryn::SkeletonParagraph{}};
    ryn::Signal<ryn::SkeletonAvatarOptions> avatar{ryn::SkeletonAvatarOptions{true}};
    auto props = ryn::SkeletonProps{}.loading(loading).title(title).paragraph(paragraph).avatar(avatar).round(true);
    auto hidden = ryn::SkeletonProps{}.title(false).paragraph(false).avatar(true);
    auto button = ryn::SkeletonButtonProps{}.block(loading).shape(ryn::SkeletonButtonShape::Round);
    auto input = ryn::SkeletonInputProps{}.block(true).size(ryn::ControlSize::Small);
    static_cast<void>(props);
    static_cast<void>(hidden);
    static_cast<void>(button);
    static_cast<void>(input);
    const auto example = [] {
        ryn::Signal<bool> loading{true};
        ryn::Skeleton(ryn::SkeletonProps{}.loading(loading).avatar(true),
                      ryn::SkeletonContent{[] { ryn::Input(ryn::InputProps{}.defaultValue(u8"Retained draft")); }});
        ryn::SkeletonButton(ryn::SkeletonButtonProps{}.shape(ryn::SkeletonButtonShape::Round));
        ryn::SkeletonAvatar(ryn::SkeletonAvatarProps{}.size(ryn::ControlSize::Large));
        ryn::SkeletonInput(ryn::SkeletonInputProps{}.block(true));
    };
    static_cast<void>(example);
}
