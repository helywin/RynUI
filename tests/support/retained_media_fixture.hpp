#pragma once
#include "paint_opacity_fixture.hpp"
#include "component/collapse_component.hpp"
#include "component/descriptions_component.hpp"
#include "component/form_component.hpp"
#include "component/popover_component.hpp"
#include "component/select_component.hpp"

namespace retained_media_test {
using namespace ryn;

struct Scene final {
    paint_test::Fixture fixture;
    CarouselRef carousel;
    FormRef form;
    FormField<String> field{String{u8"initial draft"}};
    InputRef input;
    Signal<CollapseKeys> active{CollapseKeys{}};
    Signal<int> revision{0};
    runtime::NodeId root;
    int submissions{};
    int factories{};

    void mount_controls(int version) {
        Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), [this, version] {
            Image(ImageProps{}
                      .source(paint_test::pixels(version != 0))
                      .preview(false)
                      .fit(ImageFit::Fill)
                      .layout(LayoutStyle{}.width(dp(96)).height(dp(48))));
            FormItem(FormItemProps{}.field(u8"draft", field),
                     FormItemContent{[this] { Input(InputProps{}.ref(input).layout(LayoutStyle{}.width(dp(240)))); }});
            Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click),
                    PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Nested choice"); }); }},
                    PopoverSlots{.content = PopoverContent{
                                     [] { Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}})); }}});
        });
    }

    void mount_details() {
        Descriptions(DescriptionsProps{}.column(1).labelPlacement(DescriptionsLabelPlacement::Vertical), bind([this] {
                         const int version = revision.get();
                         return std::vector<DescriptionsItem>{DescriptionsItem{
                             u8"media", DescriptionsLabel{[] { Text(u8"Retained media"); }},
                             DescriptionsContent{[this, version] {
                                 ++factories;
                                 mount_controls(version);
                             }}}.contentRevision(static_cast<std::uint64_t>(version))};
                     }));
    }

    void mount_panel() {
        Collapse(CollapseProps{}.activeKeys(active).lazy().ghost().animated(false),
                 {CollapsePanel{u8"body", CollapseTitle{[] { Text(u8"Media form"); }},
                                CollapseBody{[this] { mount_details(); }}}});
    }

    Scene() {
        ThemeConfig config;
        config.image.tokens.background = Color::rgba8(0, 0, 0, 0);
        config.image.tokens.border_width = dp(0);
        config.carousel.tokens.background = Color::rgba8(0, 0, 0, 0);
        fixture.services.mount(Content{[this, config] {
            Theme(ThemeProps{}.config(config), ThemeContent{[this] {
                      Form(FormProps{}.ref(form).onFinish([this](FormResult) { ++submissions; }), FormContent{[this] {
                               Carousel(
                                   CarouselProps{}
                                       .ref(carousel)
                                       .animated(false)
                                       .dots(false)
                                       .slides({{u8"rich", CarouselContent{[this] { mount_panel(); }}},
                                                {u8"other", CarouselContent{[] { Text(u8"Other retained panel"); }}}})
                                       .layout(LayoutStyle{}.width(dp(400)).height(dp(400))));
                           }});
                  }});
        }});
        sync();
        root = fixture.services.components().root(fixture.services.components().root_components().front());
    }

    void sync() {
        paint_test::sync(fixture);
    }

    void opacity(float value) {
        fixture.nodes.set_paint_opacity(root, value);
        sync();
    }

    void open() {
        active.set({String{u8"body"}});
        sync();
        sync();
    }

    runtime::ComponentId image_owner() {
        paint_test::check(fixture.services.image().mounted().size() == 1, "retained media image count");
        return fixture.services.image().mounted().front();
    }

    const graphics::ImageInstance& image() {
        const auto id = fixture.services.image().snapshot(image_owner()).image;
        paint_test::check(id.has_value(), "retained rich image resource absent");
        return fixture.services.images().find(*id)->instance;
    }
};
} // namespace retained_media_test
