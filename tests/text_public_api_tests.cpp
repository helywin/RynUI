#include <ryn/rynui.hpp>

#include <concepts>
#include <type_traits>
#include <utility>

namespace {

template <typename T>
concept AcceptsTextValue = requires(T value) {
    ryn::Text(value);
};

template <typename T>
concept HasColor = requires(T props) {
    props.color(0.5F);
};

template <typename T>
concept HasFont = requires(T props) {
    props.font(14);
};

template <typename T>
concept HasFontSize = requires(T props) {
    props.font_size(14);
};

template <typename T>
concept HasPrimitiveStyle = requires(T props) {
    props.primitive_style(ryn::LayoutStyle{});
};

static_assert(AcceptsTextValue<ryn::TextProps>);
static_assert(AcceptsTextValue<ryn::String>);
static_assert(!AcceptsTextValue<ryn::StringView>);
static_assert(!AcceptsTextValue<const char*>);
static_assert(!HasColor<ryn::TextProps>);
static_assert(!HasFont<ryn::TextProps>);
static_assert(!HasFontSize<ryn::TextProps>);
static_assert(!HasPrimitiveStyle<ryn::TextProps>);

} // namespace

int main() {
    ryn::Signal<ryn::String> content{ryn::String{u8"设备监控"}};
    ryn::Signal<ryn::TextTone> tone{ryn::TextTone::Secondary};
    auto bound_content = ryn::bind([] { return ryn::String{u8"绑定中文"}; });
    auto declarations = [content, tone, bound_content] {
        ryn::Text(u8"静态中文");
        ryn::Text(ryn::String{u8"拥有内容"});
        ryn::Text(
            ryn::TextProps{}
                .content(content)
                .tone(tone)
                .layout(ryn::LayoutStyle{}.max_width(ryn::dp(320.0F))));
        ryn::Text(ryn::TextProps{}.content(bound_content));
    };
    static_cast<void>(declarations);
    return 0;
}
