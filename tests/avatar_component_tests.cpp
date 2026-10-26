#include "component/avatar_component.hpp"
#include "component/popover_component.hpp"
#include "component/select_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "support/timeline_fixture.hpp"
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.scene.glyph_scene().instances().size() == 0 &&
              f.services.surfaces().size() == 0 && f.services.interactions().size() == 0 &&
              blocked_roots(f.services.interactions()).empty() && !f.services.next_frame_deadline(),
          "Avatar teardown leaked resources/blocks/deadline");
}

std::size_t ink(Fixture& f, runtime::ComponentId region) {
    const auto allocated = bounds(f, region);
    std::size_t visible = 0;
    const auto& atlas = f.scene.atlas();
    const auto aw = atlas.config().page_width;
    const auto ah = atlas.config().page_height;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (!descendant(f, text.component, region)) {
            continue;
        }
        for (const auto& range : f.scene.primitive(text.scene).draw_ranges) {
            const auto bytes = atlas.page_bytes(range.atlas_page);
            for (auto i = range.instances.first; i < range.instances.first + range.instances.count; ++i) {
                const auto& g = f.scene.glyph_scene().instances().at(i);
                if (g.translation_opacity[2] <= 0 || g.color[3] <= 0) {
                    continue;
                }
                const int x0 = static_cast<int>(std::lround(g.uv_rect[0] * aw));
                const int y0 = static_cast<int>(std::lround(g.uv_rect[1] * ah));
                const int x1 = static_cast<int>(std::lround(g.uv_rect[2] * aw));
                const int y1 = static_cast<int>(std::lround(g.uv_rect[3] * ah));
                for (int y = y0; y < y1; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        if (bytes[static_cast<std::size_t>(y) * aw + x] < 32) {
                            continue;
                        }
                        const auto p =
                            graphics::glyph_vertex(g, {(x - x0 + .5F) / (x1 - x0), (y - y0 + .5F) / (y1 - y0)});
                        if (p.x >= g.clip_bounds[0] && p.x <= g.clip_bounds[2] && p.y >= g.clip_bounds[1] &&
                            p.y <= g.clip_bounds[3]) {
                            check(contains(allocated, p), "actual R8 Avatar ink escaped its viewport");
                            ++visible;
                        }
                    }
                }
            }
        }
    }
    return visible;
}

void text_case() {
    Fixture f;
    Signal<String> text{String{u8"WWWW"}};
    Signal<AvatarSizeValue> size{AvatarSizeValue{AvatarSize::Medium}};
    Signal<AvatarShape> shape{AvatarShape::Circle};
    Signal<LogicalLength> width{dp(80)};
    Signal<LogicalLength> height{dp(80)};
    auto theme = std::make_unique<Signal<ThemeConfig>>(ThemeConfig{});
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(*theme), ThemeContent{[&] {
                  Avatar(AvatarProps{}.text(text).size(size).shape(shape).layout(
                      LayoutStyle{}.width(width).height(height)));
              }});
    }});
    sync(f);
    const auto id = f.services.avatar().mounted()[0];
    const auto first = f.services.avatar().snapshot(id);
    check(first.natural_text_width > first.viewport.width && first.font_size < 14 && first.font_size >= .5F &&
              ink(f, first.content) > 0,
          "Avatar did not auto-fit real shaped text");
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& e) {
                                  return e.material.visible && e.geometry.shape.rect.width == 32 &&
                                         e.geometry.shape.radius == 16;
                              }),
          "Avatar circle actual geometry did not preserve semantic size inside external allocation");
    text.set(u8"iiii");
    sync(f);
    const auto narrow = f.services.avatar().snapshot(id);
    check(narrow.natural_text_width < first.natural_text_width && narrow.font_size > first.font_size,
          "auto-fit used character count instead of font measurements");
    const auto node = f.services.components().root(id);
    const auto measured = f.nodes.require(node).measure_count;
    auto config = std::make_unique<ThemeConfig>();
    config->avatar.tokens.background = Color::rgba8(11, 22, 33);
    theme->set(*config);
    sync(f);
    check(f.nodes.require(node).measure_count == measured, "Avatar color update performed measure");
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [&](const auto& e) {
                                  return e.material.visible && e.material.color == *config->avatar.tokens.background;
                              }),
          "Avatar Theme background missed logical scene");
    for (auto value :
         {AvatarSizeValue{AvatarSize::Small}, AvatarSizeValue{AvatarSize::Large}, AvatarSizeValue{47.0F}}) {
        size.set(value);
        sync(f);
        check(ink(f, first.content) > 0, "Avatar preset/numeric size emitted no ink");
    }
    shape.set(AvatarShape::Square);
    bool recovered = false;
    for (auto extent : {runtime::Size{25, 6}, runtime::Size{0, 0}, runtime::Size{80, 80}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        sync(f);
        const auto part = f.services.avatar().snapshot(id);
        check(inside(bounds(f, part.content), bounds(f, id)), "Avatar finite content frame escaped owner");
        recovered |= ink(f, part.content) > 0;
    }
    check(recovered && f.services.avatar().snapshot(id).content == first.content && !f.services.next_frame_deadline(),
          "Avatar recovery remounted or created idle deadline");
    clean(f);
}

void group() {
    Fixture f;
    Signal<AvatarGroupMax> max{AvatarGroupMax{2, PopoverTriggerMode::Click, PopoverPlacement::Top}};
    Signal<bool> visible{true};
    Signal<bool> third{true};
    Signal<String> label{String{u8"C"}};
    Signal<std::optional<IconName>> icon{std::optional{IconName::UserOutlined}};
    Signal<LogicalLength> width{dp(104)};
    Signal<LogicalLength> height{dp(32)};
    f.services.mount(Content{[&] {
        AvatarGroup(AvatarGroupProps{}.max(max).visible(visible).layout(LayoutStyle{}.width(width).height(height)),
                    {AvatarGroupItem{u8"a", u8"A"}, AvatarGroupItem{u8"b", u8"B"}.icon(icon),
                     AvatarGroupItem{u8"c", label}.visible(third), AvatarGroupItem{u8"d", u8"D"}});
    }});
    sync(f);
    const auto id = f.services.avatar().groups()[0];
    const auto first = f.services.avatar().group_snapshot(id);
    const auto pop = first.popover;
    const auto original_count = f.services.components().component_count();
    check(first.overflow_count == 2 && first.items[0].inline_visible && first.items[2].overflow_visible,
          "AvatarGroup overflow count/order wrong");
    check(std::abs(bounds(f, first.items[1].inline_avatar).x - bounds(f, first.items[0].inline_avatar).x - 24) < .01F,
          "AvatarGroup actual overlap geometry wrong");
    click(f, first.overflow);
    check(f.services.popover().snapshot(pop).geometry.visible &&
              ink(f, f.services.avatar().snapshot(first.items[2].overflow_avatar).content) > 0,
          "overflow Popover has no real hidden Avatar ink");
    const auto popup = f.services.popover().snapshot(pop).popup;
    check(f.services.components().in_window_layer(popup), "overflow missed native window layer");
    third.set(false);
    label.set(u8"Changed");
    icon.set(std::nullopt);
    sync(f);
    auto current = f.services.avatar().group_snapshot(id);
    check(current.overflow_count == 1 && !current.items[2].inline_visible && !current.items[2].overflow_visible,
          "hidden Group item still counted");
    max.set(AvatarGroupMax{4, PopoverTriggerMode::Click, PopoverPlacement::Top});
    sync(f);
    current = f.services.avatar().group_snapshot(id);
    check(current.overflow_count == 0 && !f.services.popover().snapshot(pop).geometry.visible &&
              !f.services.components().branch_active(pop),
          "nonoverflow Group retained trigger/popup");
    for (auto extent : {runtime::Size{42, 6}, runtime::Size{0, 0}, runtime::Size{104, 32}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        sync(f);
        const auto allocated = bounds(f, id);
        for (const auto& text : f.services.text().mounted_texts()) {
            if (!descendant(f, text.component, first.items[0].inline_avatar)) {
                continue;
            }
            const auto range = f.scene.primitive(text.scene).instances;
            for (auto i = range.first; i < range.first + range.count; ++i) {
                const auto& g = f.scene.glyph_scene().instances().at(i);
                check(inside({g.clip_bounds[0], g.clip_bounds[1], g.clip_bounds[2] - g.clip_bounds[0],
                              g.clip_bounds[3] - g.clip_bounds[1]},
                             allocated),
                      "Group actual glyph clip escaped finite bounds");
            }
        }
        for (const auto& hit : f.services.hit_test().records()) {
            const auto* record = f.services.interactions().find(hit.interaction);
            if (record && descendant(f, record->component, first.popover)) {
                check(inside(hit.effective_clip, allocated), "closed Group actual hit escaped finite bounds");
            }
        }
    }
    max.set(AvatarGroupMax{0, PopoverTriggerMode::Click, PopoverPlacement::Bottom});
    sync(f);
    current = f.services.avatar().group_snapshot(id);
    check(current.overflow_count == 3 &&
              std::ranges::none_of(current.items, [](const auto& i) { return i.inline_visible; }),
          "count=0 should display only +N");
    click(f, current.overflow);
    check(f.services.popover().snapshot(pop).geometry.visible, "count=0 overflow could not reopen");
    visible.set(false);
    sync(f);
    check(!f.services.popover().snapshot(pop).geometry.visible, "hidden Group retained popup");
    visible.set(true);
    third.set(true);
    sync(f);
    check(f.services.components().component_count() == original_count &&
              f.services.avatar().group_snapshot(id).items[2].overflow_avatar == first.items[2].overflow_avatar,
          "Group update rebuilt retained identities");
    max.set(AvatarGroupMax{1, PopoverTriggerMode::Hover, PopoverPlacement::Top});
    sync(f);
    const auto trigger = bounds(f, first.overflow);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none,
                                   trigger.x + trigger.width / 2, trigger.y + trigger.height / 2});
    check(f.services.next_frame_deadline().has_value(), "hover Group did not arm native Popover delay");
    visible.set(false);
    sync(f);
    check(!f.services.next_frame_deadline(), "hidden Group retained hover deadline");
    clean(f);
}

void clips() {
    Fixture f;
    Signal<LogicalLength> width{dp(34)};
    Signal<LogicalLength> height{dp(28)};
    f.services.mount(Content{[&] {
        Avatar(AvatarProps{}.size(72).shape(AvatarShape::Square).layout(LayoutStyle{}.width(width).height(height)),
               {.content = AvatarContent{[] {
                    Text(TypographyProps{}.content(u8"汉汉AVATAR").mark(true));
                    Button(ButtonProps{}.wave(false), [] { Text(u8"Action"); });
                }}});
    }});
    sync(f);
    const auto id = f.services.avatar().mounted()[0];
    const auto content = f.services.avatar().snapshot(id).content;
    bool glyph = false;
    bool quad = false;
    for (auto extent : {runtime::Size{34, 28}, runtime::Size{13, 5}, runtime::Size{0, 0}, runtime::Size{100, 72}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        sync(f);
        const auto allocated = bounds(f, content);
        glyph |= ink(f, content) > 0;
        const auto marker = f.services.components().theme_scope(id)->snapshot().typography().colors.mark_background;
        for (const auto& q : f.services.surfaces().instances().instances()) {
            if (q.opacity <= 0 || q.color[3] <= 0) {
                continue;
            }
            const runtime::Rect r{q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2],
                                  q.bounds[3]};
            if (q.color == std::array{marker.red(), marker.green(), marker.blue(), marker.alpha()}) {
                quad = true;
                check(inside(r, allocated), "actual Avatar mark quad escaped content clip");
            }
        }
        for (const auto& hit : f.services.hit_test().records()) {
            const auto* record = f.services.interactions().find(hit.interaction);
            if (record && descendant(f, record->component, content)) {
                check(inside(hit.effective_clip, allocated), "Avatar actual hit clip escaped viewport");
            }
        }
    }
    check(glyph && quad, "Avatar clipping tests had no actual glyph/quad");
    check(f.services.layout_and_synchronize({600, 800}, {0, 0, 12, 10}), "Avatar document clip layout failed");
    for (const auto& hit : f.services.hit_test().records()) {
        const auto* record = f.services.interactions().find(hit.interaction);
        if (record && descendant(f, record->component, content)) {
            check(inside(hit.effective_clip, {0, 0, 12, 10}), "Avatar document interaction clip leaked");
        }
    }
    sync(f);
    const auto button = f.buttons.mounted_buttons()[0];
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard), "Avatar rich focus failed");
    sync(f);
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [&](const auto& e) {
                                  return e.material.visible &&
                                         e.geometry.kind == graphics::RoundedEffectKind::outline &&
                                         (e.geometry.shape.rect.x < bounds(f, content).x ||
                                          e.geometry.shape.rect.y < bounds(f, content).y);
                              }),
          "Avatar outward focus decoration was cropped to content viewport");
    clean(f);
}

void retention() {
    Fixture f;
    Signal<bool> visible{true};
    InputRef ref;
    int mounts = 0;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(11)), FlexContent{[&] {
                 Text(u8"Before");
                 Avatar(AvatarProps{}.visible(visible).size(100).shape(AvatarShape::Square),
                        {.content = AvatarContent{[&] {
                             ++mounts;
                             Input(InputProps{}.ref(ref).defaultValue(u8"Original"));
                         }}});
                 Text(u8"After");
             }});
    }});
    sync(f);
    const auto id = f.services.avatar().mounted()[0];
    const auto input = f.inputs.mounted_inputs()[0];
    check(ref.focus({InputFocusCursor::All}), "Avatar Input ref failed to focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Retained draft"}, f.inputs.sessions().active()})),
          "Avatar draft edit failed");
    visible.set(false);
    sync(f);
    check(!ref.focus() && f.nodes.require(f.services.components().root(id)).out_of_flow,
          "hidden Avatar retained focus/flow");
    const auto texts = f.services.text().mounted_texts();
    check(std::abs(bounds(f, texts.back().component).y - bounds(f, texts.front().component).y -
                   bounds(f, texts.front().component).height - 11) < .01F,
          "hidden Avatar left Flex gap");
    visible.set(true);
    sync(f);
    check(ref.focus() && mounts == 1 && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Retained draft",
          "Avatar lost retained content/ref/draft");
    clean(f);
}

void gates() {
    Fixture f;
    Signal<bool> spinning{false};
    Signal<bool> modal{false};
    Signal<bool> visible{true};
    Signal<bool> open{true};
    int closes = 0;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                 AvatarGroup(
                     AvatarGroupProps{}.max(AvatarGroupMax{0, PopoverTriggerMode::Click, PopoverPlacement::Top}),
                     {AvatarGroupItem{u8"a", u8"A"}});
             }});
        Avatar(AvatarProps{}.size(100).visible(visible),
               {.content = AvatarContent{[&] {
                    Select(
                        SelectProps{}.options({{1, u8"One"}}).open(open).onOpenChange([&](bool v) { closes += !v; }));
                }}});
        Modal(ModalProps{}.open(modal), ModalContent{[] { Text(u8"Modal"); }});
    }});
    sync(f);
    const auto g = f.services.avatar().group_snapshot(f.services.avatar().groups()[0]);
    const auto select = f.services.select().mounted()[0];
    check(f.services.select().snapshot(select).visible, "Avatar controlled Select popup missing");
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible && closes > 0,
          "Avatar ordinary close did not preserve controlled refusal");
    visible.set(false);
    sync(f);
    check(!f.services.select().snapshot(select).visible && closes > 0, "Avatar hide ignored controlled popup refusal");
    visible.set(true);
    sync(f);
    check(!f.services.select().snapshot(select).visible, "Avatar ordinary unhide revived rejected popup");
    click(f, g.overflow);
    check(f.services.popover().snapshot(g.popover).geometry.visible, "group gate fixture failed");
    spinning.set(true);
    sync(f);
    check(!f.services.popover().snapshot(g.popover).geometry.visible, "Spin gate left overflow popup visible");
    click(f, g.overflow);
    check(!f.services.popover().snapshot(g.popover).geometry.visible, "Spin allowed overflow activation");
    spinning.set(false);
    sync(f);
    click(f, g.overflow);
    check(f.services.popover().snapshot(g.popover).geometry.visible, "Spin release failed");
    modal.set(true);
    sync(f);
    check(!f.services.popover().snapshot(g.popover).geometry.visible, "foreign Modal did not close Group popup");
    clean(f);
}

void lifecycle() {
    Fixture f;
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        auto& b = runtime::require_component_build_context();
        parent = b.mount_component<int>(0);
        const auto node = b.root(parent);
        f.layout.set_layout(node, layout::BoxLayout{{}, true});
        b.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
    }});
    InputRef ref;
    Signal<bool> visible{true};
    runtime::ComponentId retired;
    bool replace = false;
    f.services.append_slot(
        parent, Content{[&] {
            Avatar(AvatarProps{}.visible(visible).size(100),
                   {.content = AvatarContent{[&] {
                        Input(InputProps{}.ref(ref).onBlur([&] {
                            if (replace) {
                                replace = false;
                                check(f.services.destroy(retired), "Avatar synchronous unmount failed");
                                f.services.append_slot(
                                    parent, Content{[&] {
                                        Avatar(AvatarProps{}.size(100),
                                               {.content = AvatarContent{
                                                    [&] { Input(InputProps{}.ref(ref).defaultValue(u8"New")); }}});
                                    }});
                            }
                        }));
                    }}});
        }});
    sync(f);
    retired = f.services.avatar().mounted()[0];
    check(ref.focus(), "Avatar lifecycle focus failed");
    const auto session = f.inputs.sessions().active();
    replace = true;
    visible.set(false);
    sync(f);
    check(f.services.avatar().mounted().size() == 1 && f.services.avatar().mounted()[0] != retired && ref.bound() &&
              ref.focus() && !bool(f.inputs.dispatch(TextCommitted{String{u8"Late"}, session})) &&
              blocked_roots(f.services.interactions()).empty(),
          "Avatar reentrant hide touched stale generation/ref or leaked block");
    clean(f);
}

void validation() {
    Fixture f;
    Signal<AvatarSizeValue> size{AvatarSizeValue{32.0F}};
    f.services.mount(Content{[&] { Avatar(AvatarProps{}.size(size).text(u8"A")); }});
    sync(f);
    for (float bad : {0.0F, -1.0F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        bool rejected = false;
        try {
            size.set(AvatarSizeValue{bad});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "invalid Avatar size accepted");
        size.set(AvatarSizeValue{32.0F});
        sync(f);
    }
    clean(f);
    for (int mode = 0; mode < 3; ++mode) {
        auto fixture = std::make_unique<Fixture>();
        bool rejected = false;
        try {
            fixture->services.mount(Content{[&] {
                if (mode == 0) {
                    AvatarGroup({}, {AvatarGroupItem{u8"a", u8"A"}, AvatarGroupItem{u8"a", u8"B"}});
                }
                if (mode == 1) {
                    AvatarGroup({}, {AvatarGroupItem{u8"", u8"A"}});
                }
                if (mode == 2) {
                    AvatarGroup(AvatarGroupProps{}.max(AvatarGroupMax{1025}), {});
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && fixture->services.avatar().groups().empty(), "invalid AvatarGroup keys/count accepted");
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        check(argc == 2, "expected Avatar test case");
        const std::string_view name{argv[1]};
        if (name == "text") {
            text_case();
        } else if (name == "group") {
            group();
        } else if (name == "clips") {
            clips();
        } else if (name == "retention") {
            retention();
        } else if (name == "gates") {
            gates();
        } else if (name == "lifecycle") {
            lifecycle();
        } else if (name == "validation") {
            validation();
        } else {
            throw std::invalid_argument("unknown Avatar test");
        }
        std::cout << "Avatar " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
