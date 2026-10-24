#include "component/descriptions_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f, float width = 600, float height = 800) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Descriptions layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    sync(f);
}

DescriptionsItem plain(String key, String label = u8"Name", String content = u8"Value") {
    return {key, DescriptionsLabel{[label] { Text(label); }}, DescriptionsContent{[content] { Text(content); }}};
}

void layout_and_retention() {
    Fixture f;
    Signal<int> columns{3};
    Signal<int> span{2};
    Signal<bool> shown{true};
    Signal<bool> bordered{true};
    Signal<DescriptionsLabelPlacement> placement{DescriptionsLabelPlacement::Horizontal};
    Signal<DescriptionsSize> size{DescriptionsSize::Default};
    Signal<String> name{String{u8"Name"}};
    int labels = 0;
    int contents = 0;
    int clicks = 0;
    InputRef ref;
    f.services.mount(Content{[&] {
        Descriptions(DescriptionsProps{}
                         .column(columns)
                         .bordered(bordered)
                         .labelPlacement(placement)
                         .size(size)
                         .title(DescriptionsTitle{[] { Text(u8"Account details"); }})
                         .extra(DescriptionsExtra{
                             [&] { Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Save"); }); }}),
                     {DescriptionsItem{u8"draft", DescriptionsLabel{[&] {
                                           ++labels;
                                           Text(TextProps{}.content(name));
                                       }},
                                       DescriptionsContent{[&] {
                                           ++contents;
                                           Input(InputProps{}.defaultValue(u8"Original").ref(ref));
                                       }}}
                          .span(span)
                          .visible(shown),
                      plain(u8"b").span(2), plain(u8"c")});
    }});
    sync(f);
    const auto id = f.services.descriptions().mounted()[0];
    const auto initial = f.services.descriptions().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    check(initial.rows == detail::DescriptionsRows{{{0, 0, 2}, {1, 2, 1}}, {{2, 0, 3}}}, "host packing drifted");
    check(bounds(f, initial.items[0].label).x < bounds(f, initial.items[0].content).x &&
              bounds(f, initial.items[0].label).y == bounds(f, initial.items[0].content).y &&
              bounds(f, initial.items[2].viewport).y > bounds(f, initial.items[0].viewport).y,
          "actual horizontal row geometry missing");
    check(bounds(f, initial.extra).x > bounds(f, initial.title).x &&
              bounds(f, initial.items[0].viewport).y > bounds(f, initial.header).y,
          "title/extra layout missing");
    check(f.services.surfaces().instances().size() > 0 && f.scene.glyph_scene().instances().size() > 0,
          "Descriptions did not publish actual surfaces and glyphs");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(clicks == 1, "noninteractive wrapper blocked Button");
    check(ref.focus({InputFocusCursor::All}), "rich Input cannot focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained draft"}, f.inputs.sessions().active()})),
          "draft input failed");
    const auto count = f.services.components().component_count();
    columns.set(2);
    span.set(1);
    placement.set(DescriptionsLabelPlacement::Vertical);
    size.set(DescriptionsSize::Small);
    name.set(u8"Renamed");
    sync(f);
    const auto vertical = f.services.descriptions().snapshot(id);
    check(bounds(f, vertical.items[0].label).y < bounds(f, vertical.items[0].content).y &&
              bounds(f, vertical.items[0].content).y == bounds(f, vertical.items[1].content).y,
          "actual vertical label/value rows are not aligned");
    shown.set(false);
    sync(f);
    check(f.services.focus().state().focused != input.interaction, "hidden content retained input focus");
    check(!f.inputs.sessions().active().valid(), "hidden content retained input session");
    check(!ref.focus(), "hidden content ref accepted focus");
    check(f.services.descriptions().snapshot(id).rows == detail::DescriptionsRows{{{1, 0, 2}}, {{2, 0, 2}}},
          "hidden content retained flow");
    shown.set(true);
    bordered.set(false);
    sync(f);
    check(f.services.components().component_count() == count && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "retained draft" && labels == 1 && contents == 1,
          "reactive props remounted rich slots or lost editor");
    check(f.services.destroy(id), "Descriptions teardown failed");
    sync(f);
    check(f.services.components().component_count() == 0 && f.services.surfaces().size() == 0 &&
              f.services.interactions().declaration_order().empty(),
          "Descriptions teardown leaked resources");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        layout_and_retention();
        std::cout << "Descriptions retained geometry contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
