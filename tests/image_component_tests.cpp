#include "component/image_component.hpp"
#include "component/image_model.hpp"
#include "component/modal_component.hpp"
#include "component/popover_component.hpp"
#include "support/timeline_fixture.hpp"
#include <fstream>
#include <thread>
#include <limits>
using namespace timeline_test;

namespace {
int checks{};

void expect(bool v, const char* text) {
    ++checks;
    check(v, text);
}

ImageSource source() {
    std::ifstream in(std::string(RYNUI_IMAGE_FIXTURES) + "/colors.png", std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(in), {}};
    return EncodedImage::copy(std::as_bytes(std::span(bytes)));
}

void keyboard(Fixture& f, Key key) {
    f.services.focus().dispatch({key, KeyAction::down});
    f.services.focus().dispatch({key, KeyAction::up});
    sync(f);
}

void basic() {
    Fixture f;
    Signal<ImageSource> src{source()};
    Signal<bool> disabled{false};
    ImageRef ref;
    int roots{};
    int errors{};
    f.services.mount(Content{[&] {
        ++roots;
        Image(ImageProps{}
                  .source(src)
                  .alt(u8"Real colors")
                  .ref(ref)
                  .disabled(disabled)
                  .onError([&](const ImageError&) { ++errors; })
                  .layout(LayoutStyle{}.width(dp(120)).height(dp(80))));
    }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    auto v = f.services.image().snapshot(id);
    expect(v.status == ImageStatus::Ready && v.image && f.services.images().live_count() == 1,
           "Image did not decode/publish");
    expect(ref.bound() && ref.focus(), "Image ref failed");
    keyboard(f, Key::enter);
    v = f.services.image().snapshot(id);
    expect(v.open, "Keyboard did not open preview");
    expect(v.canvas_bounds.width > 0 && v.canvas_bounds.height > 0, "Preview canvas has no geometry");
    expect(f.services.images().live_count() == 2, "Preview has no real image resource");
    expect(ref.action(ImageAction::ZoomIn), "Zoom action failed");
    sync(f);
    v = f.services.image().snapshot(id);
    expect(v.transform.scale > 1, "Zoom did not change transform");
    expect(ref.action(ImageAction::RotateRight), "Rotate failed");
    expect(ref.action(ImageAction::FlipX), "Flip failed");
    sync(f);
    v = f.services.image().snapshot(id);
    expect(v.transform.rotate == 90 && v.transform.flip_x, "Rotate/flip state lost");
    expect(ref.action(ImageAction::Reset), "Reset failed");
    sync(f);
    expect(f.services.image().snapshot(id).transform == ImageTransform{}, "Reset is stale");
    keyboard(f, Key::escape);
    expect(!f.services.image().snapshot(id).open, "Escape did not close");
    disabled.set(true);
    sync(f);
    expect(!ref.preview() && !ref.focus(), "Disabled Image accepted input");
    expect(roots == 1 && errors == 0, "Image reran unrelated composition");
    f.services.dispose();
    expect(!ref.bound() && f.services.images().live_count() == 0 && f.services.interactions().size() == 0,
           "Image resources leaked");
}

void async() {
    Fixture f;
    std::vector<ImageCompletion> pending;
    int cancelled{};
    int loaded{};
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion c) {
        pending.push_back(c);
        return [&] {
            ++cancelled;
        };
    };
    Signal<ImageSource> src{ImageSource{loader}};
    Signal<bool> visible{true};
    f.services.mount(Content{[&] { Image(ImageProps{}.source(src).visible(visible).onLoad([&] { ++loaded; })); }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    expect(pending.size() == 1 && f.services.image().snapshot(id).status == ImageStatus::Loading,
           "Loader did not start");
    auto pixels = decode_image(*std::get<std::shared_ptr<const EncodedImage>>(source().value)).pixels;
    bool foreign = true;
    std::thread thread([&] { foreign = pending[0]({pixels, {}}); });
    thread.join();
    expect(!foreign && loaded == 0, "Foreign completion mutated owner");
    visible.set(false);
    sync(f);
    expect(cancelled == 1 && !pending[0]({pixels, {}}), "Hidden Image accepted stale completion");
    visible.set(true);
    sync(f);
    expect(pending.size() == 2, "Visible Image did not resume once");
    expect(pending[1]({pixels, {}}), "Live completion rejected");
    expect(!pending[1]({pixels, {}}), "Duplicate completion accepted");
    sync(f);
    expect(loaded == 1, "Duplicate load callback");
    src.set(ImageSource{loader});
    sync(f);
    f.services.destroy(id);
    expect(!pending.back()({pixels, {}}) && f.services.images().live_count() == 0, "Retired loader mutated generation");
    Fixture h;
    const std::array<std::byte, 8> green{std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
                                         std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255}};
    const ImageSource replacement{ImagePixels::copy(2, 1, 8, green)};
    Signal<ImageSource> current{ImageSource{}};
    auto reentrant = std::make_shared<ImageLoader>();
    reentrant->start = [&](ImageCompletion) {
        return [&] {
            current.set(replacement);
        };
    };
    current.set(ImageSource{reentrant});
    h.services.mount(Content{[&] { Image(ImageProps{}.source(current)); }});
    sync(h);
    current.set(source());
    sync(h);
    const auto image = h.services.image().snapshot(h.services.image().mounted().front()).image;
    expect(image && h.services.images().find(*image)->instance.pixels->width() == 2,
           "Cancel callback's newer source was overwritten");
    Fixture i;
    int cancellations{};
    auto immediate = std::make_shared<ImageLoader>();
    immediate->start = [&](ImageCompletion done) {
        done(decode_image(*std::get<std::shared_ptr<const EncodedImage>>(source().value)));
        return [&] {
            ++cancellations;
        };
    };
    i.services.mount(Content{[&] { Image(ImageProps{}.source(ImageSource{immediate})); }});
    sync(i);
    i.services.dispose();
    expect(cancellations == 0, "Completed synchronous loader was cancelled");
}

void fallback() {
    Fixture f;
    std::array<std::byte, 4> bad{};
    int errors{};
    f.services.mount(Content{[&] {
        Image(ImageProps{}
                  .source(ImageSource{EncodedImage::copy(bad)})
                  .fallback(source())
                  .onError([&](const ImageError& e) {
                      expect(bool(e), "Empty decoder error");
                      ++errors;
                  }));
    }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    expect(f.services.image().snapshot(id).status == ImageStatus::Fallback && errors == 1,
           "Fallback did not load real pixels");
}

void controlled() {
    Fixture f;
    Signal<bool> open{false};
    int requests{};
    ImageRef ref;
    f.services.mount(Content{
        [&] { Image(ImageProps{}.source(source()).open(open).ref(ref).onOpenChange([&](bool) { ++requests; })); }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    expect(ref.preview() && requests == 1 && !f.services.image().snapshot(id).open, "Controlled refusal overwritten");
    open.set(true);
    sync(f);
    expect(f.services.image().snapshot(id).open, "Controlled open not applied");
    keyboard(f, Key::escape);
    expect(requests == 2 && f.services.image().snapshot(id).open, "Escape overrode controlled state");
    open.set(false);
    sync(f);
    expect(!f.services.image().snapshot(id).open, "Controlled close not applied");
}

void group() {
    Fixture f;
    const auto a = source();
    const auto b = source();
    Signal<std::vector<ImagePreviewItem>> items{{{String{u8"a"}, a, String{u8"A"}}, {String{u8"b"}, b, String{u8"B"}}}};
    int changed{};
    f.services.mount(Content{[&] {
        PreviewGroup(
            PreviewGroupProps{}.items(items).defaultOpen(true).onChange([&](std::size_t, std::size_t) { ++changed; }));
    }});
    sync(f);
    runtime::ComponentId id;
    for (auto c : f.services.image().mounted()) {
        if (f.services.image().snapshot(c).total == 2) {
            id = c;
            break;
        }
    }
    auto v = f.services.image().snapshot(id);
    expect(v.open && v.thumbnails.size() == 2, "PreviewGroup failed");
    const auto first = v.thumbnails[0];
    expect(f.services.image().action(id, ImageAction::ZoomIn), "Group zoom failed");
    sync(f);
    const auto transform = f.services.image().snapshot(id).transform;
    auto reordered = items.get();
    std::reverse(reordered.begin(), reordered.end());
    items.set(reordered);
    sync(f);
    v = f.services.image().snapshot(id);
    expect(v.current == 1 && v.thumbnails[1] == first && v.transform == transform,
           "Stable-key reorder reset preview state");
    expect(f.services.image().action(id, ImageAction::Previous), "Group navigation failed");
    sync(f);
    expect(changed == 1 && f.services.image().snapshot(id).current == 0, "Group event/index wrong");
    items.set({});
    sync(f);
    expect(!f.services.image().snapshot(id).open, "Empty group retained preview");
    f.services.dispose();
    expect(f.services.images().live_count() == 0, "Group image leaked");
    Fixture g;
    Signal<std::vector<ImagePreviewItem>> source_items{{{String{u8"a"}, a, {}}}};
    Signal<bool> opened{true};
    g.services.mount(Content{[&] { PreviewGroup(PreviewGroupProps{}.items(source_items).open(opened).rtl(true)); }});
    sync(g);
    const auto owner = g.services.image().mounted().front();
    source_items.set({});
    sync(g);
    source_items.set({{String{u8"a"}, a, {}}, {String{u8"b"}, b, {}}});
    sync(g);
    expect(g.services.image().snapshot(owner).open, "Controlled open did not recover after empty items");
    expect(g.services.image().action(owner, ImageAction::Next), "RTL semantic Next was reversed");
    sync(g);
    expect(g.services.image().snapshot(owner).current == 1, "RTL Next did not select next item");
    keyboard(g, Key::right);
    expect(g.services.image().snapshot(owner).current == 0, "RTL Right did not request Previous");
    auto self_destroy = std::make_shared<ImageLoader>();
    int cancels{};
    self_destroy->start = [&](ImageCompletion) {
        g.services.destroy(owner);
        return [&] {
            ++cancels;
        };
    };
    source_items.set({{String{u8"a"}, ImageSource{self_destroy}, {}}});
    expect(g.services.image().mounted().empty() && g.services.images().live_count() == 0 && cancels == 1,
           "Reentrant group loader leaked or accessed retired rows");
}

void gestures() {
    Fixture f;
    ImageRef ref;
    f.services.mount(Content{[&] { Image(ImageProps{}.source(source()).defaultOpen(true).ref(ref)); }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    ref.action(ImageAction::ZoomIn);
    sync(f);
    auto v = f.services.image().snapshot(id);
    const auto r = v.canvas_bounds;
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, r.x + r.width * 10, r.y + r.height * 10});
    sync(f);
    v = f.services.image().snapshot(id);
    expect(v.dragging && std::isfinite(v.transform.x) && std::abs(v.transform.x) < r.width, "Preview drag not bounded");
    f.services.set_window_active(false);
    sync(f);
    expect(!f.services.image().snapshot(id).dragging && !f.services.image().snapshot(id).open,
           "Inactive window retained gesture");
}

void failures() {
    Fixture f;
    bool threw{};
    try {
        f.services.mount(Content{[] {
            Image(ImageProps{}.source(source()),
                  {.placeholder = ImagePlaceholder{[] { throw std::runtime_error("slot"); }}});
        }});
    } catch (...) {
        threw = true;
    }
    expect(threw && f.services.images().live_count() == 0 && f.services.interactions().size() == 0,
           "Throwing slot leaked resources");
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [](ImageCompletion) -> ImageCancel {
        throw std::runtime_error("loader");
    };
    Fixture g;
    g.services.mount(Content{[&] { Image(ImageProps{}.source(ImageSource{loader})); }});
    sync(g);
    expect(g.services.image().snapshot(g.services.image().mounted().front()).status == ImageStatus::Error,
           "Throwing loader not represented");
}

void idle() {
    Fixture f;
    f.services.mount(Content{[] { Image(ImageProps{}.source(source())); }});
    sync(f);
    f.frames.consume_request();
    sync(f);
    f.frames.consume_request();
    sync(f);
    expect(!f.frames.pending() && !f.services.next_frame_deadline(), "Idle Image requested frames");
    f.services.dispose();
    expect(f.services.images().live_count() == 0 && f.services.rounded_effects().live_count() == 0,
           "Idle Image cleanup leak");
}

void preview_source() {
    Fixture f;
    std::vector<ImageCompletion> pending;
    int cancels{};
    ImageRef ref;
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion done) {
        pending.push_back(done);
        return [&] {
            ++cancels;
        };
    };
    f.services.mount(
        Content{[&] { Image(ImageProps{}.source(source()).previewSource(ImageSource{loader}).ref(ref)); }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    expect(pending.empty(), "Preview source started before open");
    ref.preview();
    sync(f);
    expect(pending.size() == 1, "Preview source did not start");
    ref.preview(false);
    sync(f);
    const auto pixels = decode_image(*std::get<std::shared_ptr<const EncodedImage>>(source().value)).pixels;
    expect(cancels == 1 && !pending[0]({pixels, {}}), "Closed preview accepted stale source");
    ref.preview();
    sync(f);
    expect(pending.size() == 2 && pending[1]({pixels, {}}), "Preview source did not resume");
    sync(f);
    f.services.destroy(id);
    expect(f.services.images().live_count() == 0, "Preview source retained image after destroy");
    Fixture g;
    ImageRef other;
    int errors{};
    auto throwing = std::make_shared<ImageLoader>();
    throwing->start = [](ImageCompletion) -> ImageCancel {
        throw 1;
    };
    g.services.mount(Content{[&] {
        Image(ImageProps{}
                  .source(source())
                  .previewSource(ImageSource{throwing})
                  .ref(other)
                  .onError([&](const ImageError&) { ++errors; }));
    }});
    sync(g);
    expect(other.preview(), "Throwing preview did not open fallback");
    sync(g);
    expect(errors == 1 && g.services.images().live_count() == 2,
           "Unknown preview loader exception was not represented");
}

void theme() {
    Fixture f;
    ThemeConfig config;
    config.image.tokens.background = Color::rgba8(20, 30, 40);
    config.image.tokens.thumbnail_size = dp(64);
    config.image.tokens.font_size = dp(18);
    Signal<ThemeConfig> theme{config};
    Signal<bool> rtl{false};
    const auto src = source();
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  PreviewGroup(
                      PreviewGroupProps{}
                          .items(std::vector<ImagePreviewItem>{{String{u8"a"}, src, {}}, {String{u8"b"}, src, {}}})
                          .rtl(rtl));
              }});
    }});
    sync(f);
    runtime::ComponentId id;
    for (auto c : f.services.image().mounted()) {
        if (f.services.image().snapshot(c).total == 2) {
            id = c;
            break;
        }
    }
    auto v = f.services.image().snapshot(id);
    expect(bounds(f, v.thumbnails[0]).width == 64, "Image thumbnail token ignored");
    const auto first = v.thumbnails[0];
    rtl.set(true);
    sync(f);
    v = f.services.image().snapshot(id);
    expect(bounds(f, v.thumbnails[0]).x > bounds(f, v.thumbnails[1]).x, "Image RTL order wrong");
    config.image.tokens.thumbnail_size = dp(72);
    theme.set(config);
    sync(f);
    v = f.services.image().snapshot(id);
    expect(v.thumbnails[0] == first && bounds(f, first).width == 72, "Image theme remounted or did not resize");
    const auto baseline = resolve_theme();
    ThemeConfig color;
    color.image.tokens.background = Color::rgba8(90, 30, 20);
    const auto result = resolve_theme(color);
    expect(result.image().colors.background == *color.image.tokens.background && result.upload() == baseline.upload(),
           "Image token isolation wrong");
}

void controlled_group() {
    Fixture f;
    const auto src = source();
    Signal<std::size_t> index{0};
    int changes{};
    f.services.mount(Content{[&] {
        PreviewGroup(PreviewGroupProps{}
                         .items(std::vector<ImagePreviewItem>{{String{u8"a"}, src, {}}, {String{u8"b"}, src, {}}})
                         .current(index)
                         .defaultOpen(true)
                         .onChange([&](std::size_t, std::size_t) { ++changes; }));
    }});
    sync(f);
    runtime::ComponentId id;
    for (auto c : f.services.image().mounted()) {
        if (f.services.image().snapshot(c).total == 2) {
            id = c;
            break;
        }
    }
    expect(f.services.image().action(id, ImageAction::Next) && changes == 1 &&
               f.services.image().snapshot(id).current == 0,
           "Controlled group was overwritten");
    index.set(1);
    sync(f);
    expect(f.services.image().snapshot(id).current == 1, "Controlled group index not applied");
}

void rich() {
    Fixture f;
    ImageRef ref;
    int mounts{};
    auto renderer = std::make_shared<ImageActionsRenderer>();
    renderer->render = [&](const ImagePreviewActions&) {
        ++mounts;
        Input(InputProps{}.defaultValue(String{u8"toolbar draft"}));
    };
    Signal<std::shared_ptr<const ImageActionsRenderer>> actions{renderer};
    Signal<ImageSource> src{source()};
    f.services.mount(
        Content{[&] { Image(ImageProps{}.source(src).actionsRender(actions).ref(ref).defaultOpen(true)); }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    const auto action_root = f.services.image().snapshot(id).actions;
    ref.action(ImageAction::ZoomIn);
    src.set(source());
    sync(f);
    expect(mounts == 1 && f.services.image().snapshot(id).actions == action_root, "Image replaced retained toolbar");
    auto bad = std::make_shared<ImageActionsRenderer>();
    bad->render = [](const ImagePreviewActions&) {
        throw std::runtime_error("renderer");
    };
    bool threw{};
    try {
        actions.set(bad);
    } catch (...) {
        threw = true;
    }
    expect(threw && f.services.components().contains(action_root), "Renderer replacement did not roll back");
    std::function<void(ImageAction)> late;
    int transforms{};
    {
        auto g = std::make_unique<Fixture>();
        auto toolbar = std::make_shared<ImageActionsRenderer>();
        toolbar->render = [&](const ImagePreviewActions& actions) {
            late = actions.invoke;
            Text(u8"Retained action");
        };
        g->services.mount(Content{[&] {
            Image(ImageProps{}
                      .source(source())
                      .defaultOpen(true)
                      .actionsRender(std::shared_ptr<const ImageActionsRenderer>{toolbar})
                      .onTransform([&](ImageTransform, ImageAction) { ++transforms; }));
        }});
        sync(*g);
        std::thread foreign([&] { late(ImageAction::ZoomIn); });
        foreign.join();
        expect(transforms == 0, "Foreign public preview action reached Core");
        late(ImageAction::ZoomIn);
        expect(transforms == 1, "Live public preview action failed");
    }
    late(ImageAction::ZoomIn);
    expect(transforms == 1, "Retired preview action reached old owner");
}

void lifetime() {
    Fixture f;
    std::vector<ImageCompletion> pending;
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion c) {
        pending.push_back(c);
        return ImageCancel{};
    };
    f.services.mount(Content{[&] {
        Image(ImageProps{}.source(ImageSource{loader}).onLoad([&] {
            const auto id = f.services.image().mounted().front();
            f.services.destroy(id);
        }));
    }});
    sync(f);
    const auto pixels = decode_image(*std::get<std::shared_ptr<const EncodedImage>>(source().value)).pixels;
    expect(pending[0]({pixels, {}}) && f.services.image().mounted().empty() && f.services.images().live_count() == 0,
           "Self-destroying decoder callback leaked");
    Fixture g;
    ImageCompletion completion;
    auto second = std::make_shared<ImageLoader>();
    second->start = [&](ImageCompletion c) {
        completion = c;
        return ImageCancel{};
    };
    g.services.mount(Content{
        [&] { Image(ImageProps{}.source(ImageSource{second}).onLoad([] { throw std::runtime_error("onLoad"); })); }});
    sync(g);
    bool threw{};
    try {
        completion({pixels, {}});
    } catch (...) {
        threw = true;
    }
    expect(threw && g.services.image().mounted().empty(), "Throwing Image callback was not retired");
    Fixture h;
    ImageRef open_ref;
    h.services.mount(Content{[&] {
        Image(ImageProps{}.source(source()).ref(open_ref).onOpenChange(
            [&](bool) { h.services.destroy(h.services.image().mounted().front()); }));
    }});
    sync(h);
    expect(open_ref.preview() && !open_ref.bound() && h.services.images().live_count() == 0,
           "Self-destroying open callback leaked");
    Fixture i;
    ImageRef transform_ref;
    i.services.mount(Content{[&] {
        Image(ImageProps{}
                  .source(source())
                  .ref(transform_ref)
                  .defaultOpen(true)
                  .onTransform([](ImageTransform, ImageAction) { throw std::runtime_error("transform"); }));
    }});
    sync(i);
    threw = false;
    try {
        static_cast<void>(transform_ref.action(ImageAction::ZoomIn));
    } catch (...) {
        threw = true;
    }
    expect(threw && !transform_ref.bound() && i.services.images().live_count() == 0,
           "Throwing transform callback was not retired");
}

void boundaries() {
    Fixture f;
    ImageRef ref;
    ImagePreviewOptions options;
    options.focus_trap = false;
    Signal<ImagePreviewOptions> config{options};
    f.services.mount(Content{[&] {
        Button(ButtonProps{}, [] { Text(u8"Outside"); });
        Image(ImageProps{}.source(source()).preview(config).ref(ref).defaultOpen(true));
    }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    expect(!f.services.interactions().input_boundary(), "focusTrap false still confined focus");
    options.focus_trap = true;
    config.set(options);
    sync(f);
    expect(f.services.interactions().input_boundary().has_value(), "focusTrap true did not confine");
    const auto block = f.services.image().snapshot(id).canvas;
    f.services.set_input_blocked(block, true);
    sync(f);
    expect(!f.services.image().snapshot(id).dragging, "Blocked Image retained capture");
    Fixture g;
    InputRef draft;
    Signal<bool> disabled{false};
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [](ImageCompletion) {
        return ImageCancel{};
    };
    g.services.mount(Content{[&] {
        Image(ImageProps{}.source(ImageSource{loader}).disabled(disabled),
              {.placeholder =
                   ImagePlaceholder{[&] { Input(InputProps{}.ref(draft).defaultValue(u8"placeholder draft")); }}});
    }});
    sync(g);
    expect(draft.focus(), "Placeholder draft focus failed");
    disabled.set(true);
    sync(g);
    expect(!draft.focus() && !g.inputs.sessions().active().valid(), "Disabled Image retained custom slot IME");
    g.services.dispose();
    expect(blocked_roots(g.services.interactions()).empty(), "Image private input blocks leaked");
}

void circle() {
    Fixture f;
    f.services.mount(Content{[] {
        Image(ImageProps{}
                  .source(source())
                  .shape(ImageShape::Circle)
                  .fit(ImageFit::Cover)
                  .layout(LayoutStyle{}.width(dp(80)).height(dp(80))));
    }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    const auto view = f.services.image().snapshot(id);
    expect(view.image && f.services.images().find(*view.image)->instance.circle_clip.has_value(),
           "Picture circle has no actual clip record");
}

void ratio() {
    Fixture f;
    f.services.mount(Content{[] { Image(ImageProps{}.source(source()).layout(LayoutStyle{}.width(dp(90)))); }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    const auto r = bounds(f, id);
    expect(r.width == 90 && r.height == 60, "Image intrinsic aspect ratio lost");
}

void controller() {
    Fixture f;
    Signal<bool> open{false};
    const auto src = source();
    f.services.mount(Content{[&] {
        PreviewGroup(PreviewGroupProps{}
                         .items(std::vector<ImagePreviewItem>{{String{u8"a"}, src, {}}})
                         .thumbnails(false)
                         .open(open));
    }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    expect(!f.services.components().branch_active(f.services.image().snapshot(id).thumbnails.front()),
           "Controller rendered thumbnails");
    open.set(true);
    sync(f);
    expect(f.services.image().snapshot(id).open && f.services.images().live_count() == 1,
           "Controller did not render preview");
    open.set(false);
    sync(f);
    expect(f.services.images().live_count() == 0, "Controller retained closed image");
}

void stack() {
    Fixture f;
    Signal<bool> parent_open{true};
    Signal<bool> child_open{false};
    InputRef background;
    InputRef popup_editor;
    InputRef child_editor;
    ImageRef ref;
    auto toolbar = std::make_shared<ImageActionsRenderer>();
    toolbar->render = [&](const ImagePreviewActions&) {
        Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click),
                PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Preview details"); }); }},
                {.content = PopoverContent{[&] { Input(InputProps{}.ref(popup_editor)); }}});
    };
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(background));
        Modal(ModalProps{}.open(parent_open), ModalContent{[&] {
                  Image(ImageProps{}.source(source()).ref(ref).actionsRender(
                      std::shared_ptr<const ImageActionsRenderer>{toolbar}));
              }});
        Modal(ModalProps{}.open(child_open).onCancel([&](ModalCancelReason) { child_open.set(false); }),
              ModalContent{[&] { Input(InputProps{}.ref(child_editor)); }});
    }});
    sync(f);
    expect(ref.preview(), "Nested Image did not open");
    sync(f);
    const auto image = f.services.image().mounted().front();
    const auto view = f.services.image().snapshot(image);
    const auto trigger = std::ranges::find_if(
        f.buttons.mounted_buttons(), [&](const auto& button) { return descendant(f, button.component, view.actions); });
    expect(trigger != f.buttons.mounted_buttons().end(), "Preview popup trigger missing");
    click(f, trigger->component);
    sync(f);
    const auto popup = f.services.popover().mounted().front();
    expect(f.services.popover().snapshot(popup).geometry.visible && popup_editor.focus(),
           "Preview rich Popover did not open/focus");
    keyboard(f, Key::escape);
    expect(!f.services.popover().snapshot(popup).geometry.visible && f.services.image().snapshot(image).open,
           "Image consumed Escape before its focused Popover");
    child_open.set(true);
    sync(f);
    expect(child_editor.focus() && !background.focus(), "Top Modal confinement failed above Image");
    keyboard(f, Key::escape);
    expect(!child_open.get() && f.services.image().snapshot(image).open, "Underlying Image consumed top Modal Escape");
    keyboard(f, Key::escape);
    expect(!f.services.image().snapshot(image).open && !background.focus(),
           "Image close released the still-open parent Modal");
    f.services.dispose();
    expect(!f.inputs.sessions().active().valid() && !f.services.interactions().input_boundary() &&
               f.services.images().live_count() == 0,
           "Nested preview retired with input/image resources");
}

void model() {
    auto r = detail::fit_image({0, 0, 100, 100}, {200, 100}, ImageFit::Contain);
    expect(r.width == 100 && r.height == 50 && r.y == 25, "Contain geometry wrong");
    r = detail::fit_image({0, 0, 100, 100}, {200, 100}, ImageFit::Cover);
    expect(r.width == 200 && r.x == -50, "Cover geometry wrong");
    ImageTransform t{10, 90, 10000, -10000};
    t = detail::clamp_image_transform(t, {100, 50}, {100, 100}, {});
    expect(std::abs(t.x - 200) < .01F && std::abs(t.y + 450) < .01F, "Rotated drag clamp wrong");
    bool threw{};
    try {
        detail::validate_image_preview({.min_scale = 2, .max_scale = 1});
    } catch (...) {
        threw = true;
    }
    expect(threw, "Invalid scale range accepted");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "basic";
        if (mode == "basic") {
            basic();
        } else if (mode == "async") {
            async();
        } else if (mode == "fallback") {
            fallback();
        } else if (mode == "controlled") {
            controlled();
        } else if (mode == "group") {
            group();
        } else if (mode == "gestures") {
            gestures();
        } else if (mode == "failures") {
            failures();
        } else if (mode == "idle") {
            idle();
        } else if (mode == "controller") {
            controller();
        } else if (mode == "stack") {
            stack();
        } else if (mode == "model") {
            model();
        } else if (mode == "preview_source") {
            preview_source();
        } else if (mode == "theme") {
            theme();
        } else if (mode == "controlled_group") {
            controlled_group();
        } else if (mode == "rich") {
            rich();
        } else if (mode == "lifetime") {
            lifetime();
        } else if (mode == "boundaries") {
            boundaries();
        } else if (mode == "circle") {
            circle();
        } else if (mode == "ratio") {
            ratio();
        } else {
            throw std::runtime_error("Unknown case");
        }
        std::cout << checks << " Image checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
