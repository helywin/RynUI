#include "component/image_component.hpp"
#include "component/modal_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <ryn/flex.hpp>
#include "component/image_model.hpp"
using namespace ryn;

namespace {
int checks{};

void check(bool v, const char* m) {
    ++checks;
    if (!v) {
        throw std::runtime_error(m);
    }
}

void run() {
    ryn_test::input_component::Fixture f;
    const std::array bytes{std::byte{220}, std::byte{80}, std::byte{100}, std::byte{128}};
    ImageSource source{ImagePixels::copy(1, 1, 4, bytes)};
    ImagePreviewOptions options;
    options.enabled = false;
    Signal<ImagePreviewOptions> preview{options};
    InputRef editor;
    ImageRef image;
    int factories = 0;
    auto actions = std::make_shared<const ImageActionsRenderer>(ImageActionsRenderer{[&](const ImagePreviewActions&) {
        ++factories;
        Input(InputProps{}.defaultValue(u8"toolbar draft").ref(editor));
    }});
    f.services.mount(Content{[&] {
        Image(ImageProps{}
                  .source(source)
                  .shape(ImageShape::RoundedRectangle)
                  .preview(preview)
                  .ref(image)
                  .actionsRender(actions)
                  .layout(LayoutStyle{}.width(dp(64)).height(dp(64))));
    }});
    f.synchronize();
    const auto id = f.services.image().mounted()[0];
    check(f.services.modal().mounted().empty() && factories == 0, "preview(false) mounted a private Modal/actions");
    check(!image.preview() && !editor.bound(), "disabled preview exposed private editor or open action");
    const auto image_node = f.services.components().root(id);
    for (float factor : {.5F, 0.F, 1.F}) {
        f.nodes.set_paint_opacity(image_node, factor);
        f.synchronize();
        std::size_t live = 0;
        for (const auto& slot : f.services.images().slots()) {
            if (slot.live) {
                ++live;
                check(slot.instance.opacity == factor,
                      "rounded Image applied paint envelope twice or lost zero recovery");
            }
        }
        check(live > 0 && live <= 7, "rounded Image piece publication is unbounded");
    }
    options.enabled = true;
    preview.set(options);
    f.synchronize();
    const auto modal = f.services.image().snapshot(id).modal;
    check(modal.valid() && factories == 1 && f.services.modal().mounted().size() == 1,
          "lazy preview did not mount exactly once");
    check(image.preview(), "lazy preview did not open");
    f.synchronize();
    check(f.services.image().snapshot(id).open && editor.focus(), "lazy toolbar could not focus");
    const auto input = f.inputs.mounted_inputs().back();
    const auto stamp = f.inputs.sessions().active();
    static_cast<void>(f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {}, stamp}));
    options.enabled = false;
    preview.set(options);
    f.synchronize();
    check(!f.services.image().snapshot(id).open && !f.inputs.sessions().active().valid() &&
              !f.services.interactions().input_boundary(),
          "preview disable leaked focus/preedit/boundary");
    options.enabled = true;
    preview.set(options);
    f.synchronize();
    check(image.preview(), "re-enabled preview failed to open");
    f.synchronize();
    check(f.services.image().snapshot(id).modal == modal && factories == 1 &&
              f.inputs.editors().require(input.editor).value() == "toolbar draft",
          "preview re-enable rebuilt retained toolbar");
    check(image.preview(false), "preview close request failed");
    f.synchronize();
    check(f.services.destroy(modal), "private preview removal failed");
    f.synchronize();
    check(!image.preview(), "stale Modal accepted open action");
    check(f.services.destroy(id) && !image.bound() && !editor.bound(), "preview retirement leaked refs");
    ryn_test::input_component::Fixture cover;
    const std::array wide_bytes{std::byte{180}, std::byte{50},  std::byte{90},  std::byte{128},
                                std::byte{30},  std::byte{140}, std::byte{210}, std::byte{0}};
    const auto wide = ImagePixels::copy(2, 1, 8, wide_bytes);
    Signal<ImageFit> fit{ImageFit::Contain};
    Signal<ImageShape> shape{ImageShape::Rectangle};
    cover.services.mount(Content{[&] {
        Flex(FlexProps{}, FlexContent{[&] {
                 Flex(FlexProps{}, FlexContent{[&] {
                          Image(ImageProps{}
                                    .source(ImageSource{wide})
                                    .fit(fit)
                                    .shape(shape)
                                    .preview(false)
                                    .layout(LayoutStyle{}.width(dp(65.5F)).height(dp(42.25F))));
                      }});
             }});
    }});
    cover.synchronize();
    const auto cover_image = cover.services.image().mounted().front();
    const auto parent = *cover.services.components().parent(cover_image);
    const auto holder = *cover.services.components().parent(parent);
    for (auto mode : {ImageFit::Cover, ImageFit::Contain, ImageFit::None, ImageFit::ScaleDown}) {
        fit.set(mode);
        for (float radius : {0.F, 9.F, 1000.F}) {
            cover.services.image().set_cover_rounding(parent, radius);
            cover.synchronize();
            const auto canvas = cover.services.image().snapshot(cover_image).bounds;
            const auto drawable = detail::fit_image(canvas, {2, 1}, mode);
            graphics::ImageInstance full{
                wide, drawable, canvas, {drawable.x + drawable.width / 2, drawable.y + drawable.height / 2}};
            const auto expected = graphics::decompose_rounded_image(full, canvas, radius, true);
            check(cover.services.images().live_count() == expected.size(),
                  "cover fit did not intersect drawable bounds");
            for (const auto& piece : expected) {
                bool found = false;
                for (const auto& slot : cover.services.images().slots()) {
                    found |= slot.live && slot.instance.bounds == piece.bounds &&
                             slot.instance.source_uv == piece.source_uv &&
                             slot.instance.circle_clip == piece.circle_clip;
                }
                check(found, "cover geometry/UV differs from full-image fit/canvas policy");
            }
        }
    }
    shape.set(ImageShape::Circle);
    cover.synchronize();
    check(cover.services.images().live_count() == 1, "explicit Image shape did not override Card cover policy");
    check(cover.services.destroy(parent) && cover.services.images().live_count() == 0,
          "cover policy/parts survived parent retirement");
    cover.services.append_slot(holder, Content{[&] {
                                   Flex(FlexProps{}, FlexContent{[&] {
                                            Image(ImageProps{}
                                                      .source(ImageSource{wide})
                                                      .preview(false)
                                                      .layout(LayoutStyle{}.width(dp(64)).height(dp(40))));
                                        }});
                               }});
    cover.synchronize();
    check(cover.services.images().live_count() == 1, "retired cover policy leaked into reused parent generation");
    ryn_test::input_component::Fixture failing;
    Signal<ImagePreviewOptions> enable{ImagePreviewOptions{false}};
    failing.services.mount(Content{[&] {
        Image(ImageProps{}.source(source).preview(enable).actionsRender(
            std::make_shared<const ImageActionsRenderer>([](const ImagePreviewActions&) {
                Input(InputProps{});
                throw std::runtime_error("actions factory");
            })));
    }});
    failing.synchronize();
    bool threw = false;
    try {
        enable.set(ImagePreviewOptions{});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && failing.services.image().mounted().empty() && failing.services.modal().mounted().empty() &&
              failing.inputs.mounted_inputs().empty() && !failing.services.interactions().input_boundary(),
          "failed lazy Modal append retained a partial owner/boundary");
    ryn_test::input_component::Fixture transaction;
    Signal<ImagePreviewOptions> during{ImagePreviewOptions{false}};
    transaction.services.mount(Content{[&] {
        Image(ImageProps{}.preview(during).source(source).onLoad([&] { during.set(ImagePreviewOptions{}); }));
    }});
    transaction.synchronize();
    check(transaction.services.modal().mounted().size() == 1, "mount-transaction preview enable did not lazy mount");
}
} // namespace

int main() {
    try {
        run();
        std::cout << checks << " Image lazy preview checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
