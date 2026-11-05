#pragma once
#include "input_fixture.hpp"
#include "component/image_component.hpp"
#include "component/carousel_component.hpp"
#include "component/upload_component.hpp"

namespace paint_test {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

inline void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

inline ImageSource pixels(bool blue = false) {
    const std::array<unsigned char, 8> bytes{
        static_cast<unsigned char>(blue ? 0 : 255), 0, static_cast<unsigned char>(blue ? 255 : 0), 255, 0, 255, 0, 128};
    return ImagePixels::copy(2, 1, 8, std::as_bytes(std::span{bytes}));
}

inline void sync(Fixture& fixture) {
    check(fixture.services.layout_and_synchronize({640, 600}, {0, 0, 640, 600}), "opacity scene layout");
}

struct RichScene final {
    Fixture fixture;
    ImageRef image;
    Signal<ImageSource> source{pixels()};
    Signal<bool> visible{true};
    runtime::NodeId root;
    runtime::ComponentId image_owner;
    runtime::ComponentId text_owner;

    RichScene() {
        ThemeConfig theme;
        theme.image.tokens.background = Color::rgba8(0, 0, 0, 0);
        theme.image.tokens.border_width = dp(0);
        fixture.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(12)), [&] {
                          Button(ButtonProps{}.type(ButtonType::Primary).wave(false), [] { Text(u8"Button"); });
                          Input(InputProps{}.defaultValue(u8"Input").layout(LayoutStyle{}.width(dp(240))));
                          TextArea(
                              TextAreaProps{}.defaultValue(u8"TextArea").rows(2).layout(LayoutStyle{}.width(dp(240))));
                          Icon(IconProps{}.name(IconName::CalendarOutlined));
                          Text(u8"Opacity glyph");
                          Image(ImageProps{}
                                    .ref(image)
                                    .source(source)
                                    .visible(visible)
                                    .fit(ImageFit::Fill)
                                    .layout(LayoutStyle{}.width(dp(96)).height(dp(48))));
                      });
                  }});
        }});
        sync(fixture);
        root = fixture.services.components().root(fixture.services.components().root_components().front());
        image_owner = fixture.services.image().mounted().front();
        for (const auto& mounted : fixture.services.text().mounted_texts()) {
            if (fixture.scene.text_state(mounted.scene).content().bytes() == "Opacity glyph") {
                text_owner = mounted.component;
            }
        }
        check(text_owner.valid(), "opacity glyph component absent");
    }

    void opacity(float value) {
        fixture.nodes.set_paint_opacity(root, value);
        sync(fixture);
    }

    const graphics::ImageInstance& instance() {
        const auto id = fixture.services.image().snapshot(image_owner).image;
        check(id.has_value(), "native Image lost its resource");
        return fixture.services.images().find(*id)->instance;
    }
};
} // namespace paint_test
