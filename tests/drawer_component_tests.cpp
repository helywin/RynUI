#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f, float width = 700, float height = 600) {
    for (int frame = 0; frame < 3; ++frame) {
        check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Drawer layout failed");
    }
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

void click_at(Fixture& f, float x, float y) {
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, x, y});
    }
    sync(f);
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = bounds(f, id);
    click_at(f, rect.x + rect.width / 2, rect.y + rect.height / 2);
}

void controlled_retention_and_cancellation() {
    Signal<bool> open{false};
    Signal<bool> closable{true};
    Signal<bool> keyboard{true};
    Signal<DrawerMask> mask{DrawerMask{}};
    InputRef outside;
    InputRef inside;
    int body_runs{};
    int closed{};
    std::vector<bool> changes;
    std::vector<DrawerCancelReason> requests;
    Fixture f;
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(outside));
        Drawer(DrawerProps{}
                   .open(open)
                   .title(u8"Account")
                   .closable(closable)
                   .keyboard(keyboard)
                   .mask(mask)
                   .onCancel([&](DrawerCancelReason reason) { requests.push_back(reason); })
                   .afterClose([&] { ++closed; })
                   .afterOpenChange([&](bool value) { changes.push_back(value); }),
               DrawerContent{[&] {
                   ++body_runs;
                   Input(InputProps{}.defaultValue(u8"draft").autoFocus().ref(inside));
               }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted()[0];
    const auto initial = f.services.modal().snapshot(id);
    const auto editor = f.inputs.mounted_inputs()[1];
    const auto count = f.services.components().component_count();
    check(initial.drawer && !initial.visible && body_runs == 1 && !inside.focus() && changes.empty() && !closed &&
              !f.inputs.sessions().active().valid(),
          "hidden Drawer started focus/lifecycle");
    check(outside.focus(), "outside initial focus failed");
    open.set(true);
    sync(f);
    auto shown = f.services.modal().snapshot(id);
    check(shown.visible && shown.topmost && shown.bounds == runtime::Rect{322, 0, 378, 600} &&
              f.services.focus().state().focused == editor.interaction && !outside.focus(),
          "Drawer open isolation failed");
    check(bounds(f, shown.close).x < bounds(f, shown.title).x, "Drawer close is not at the header start");
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"!"}, session})), "Drawer editing failed");
    const auto draft = f.inputs.editors().require(editor.editor).value();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, session})), "Drawer preedit failed");
    key(f, Key::escape);
    check(requests.empty(), "Drawer consumed editor composition Escape");
    click(f, shown.close);
    click_at(f, 2, 2);
    key(f, Key::escape);
    check(requests ==
                  std::vector{DrawerCancelReason::CloseButton, DrawerCancelReason::Mask, DrawerCancelReason::Escape} &&
              f.services.modal().snapshot(id).visible && changes == std::vector{true},
          "controlled close was committed or repeated");
    closable.set(false);
    keyboard.set(false);
    mask.set({false, false});
    sync(f);
    click_at(f, 2, 2);
    key(f, Key::escape);
    check(requests.size() == 3 && !outside.focus(), "transparent/disabled cancellation leaked input");
    for (int index = 0; index < 8; ++index) {
        key(f, Key::tab, index % 2 ? KeyModifier::shift : KeyModifier::none);
        check(f.services.focus().state().focused == editor.interaction, "single-entry Drawer Tab escaped");
    }
    open.set(false);
    sync(f);
    check(changes == std::vector({true, false}) && closed == 1 &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction && !inside.focus() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"stale"}, session})),
          "Drawer close/session restoration failed");
    open.set(true);
    sync(f);
    check(body_runs == 1 && f.services.components().component_count() == count &&
              f.services.modal().snapshot(id).body == initial.body &&
              f.inputs.editors().require(editor.editor).value() == draft,
          "Drawer reopened with rebuilt/lost contents");
    f.services.dispose();
    check(!inside.bound() && !outside.bound() && !f.inputs.sessions().active().valid() &&
              f.services.interactions().size() == 0 && f.services.surfaces().size() == 0 &&
              f.services.rounded_effects().live_count() == 0,
          "Drawer disposal leaked resources");
}

void geometry_scrolling_theme_and_slots() {
    Signal<DrawerPlacement> edge{DrawerPlacement::Right};
    Signal<float> width{360};
    Signal<float> height{300};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    InputRef last;
    int body_runs{};
    int title_runs{};
    int footer_runs{};
    Fixture f;
    f.font_scale = 1.25F;
    f.chains.clear(); // The shared fixture primes its 14px chain at construction.
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Drawer(DrawerProps{}.open().placement(edge).width(width).height(height), DrawerContent{[&] {
                             ++body_runs;
                             for (int index = 0; index < 30; ++index) {
                                 Text(u8"Retained scrolling document.");
                             }
                             Input(InputProps{}.ref(last));
                         }},
                         DrawerSlots{.title = DrawerTitle{[&] {
                                         ++title_runs;
                                         Text(u8"Details");
                                     }},
                                     .footer = DrawerFooter{[&] {
                                         ++footer_runs;
                                         Button(ButtonProps{}, ButtonContent{[] { Text(u8"Save"); }});
                                     }}});
              }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted()[0];
    const auto count = f.services.components().component_count();
    for (auto placement :
         {DrawerPlacement::Top, DrawerPlacement::Bottom, DrawerPlacement::Left, DrawerPlacement::Right}) {
        edge.set(placement);
        sync(f);
        const auto s = f.services.modal().snapshot(id);
        const bool side = placement == DrawerPlacement::Left || placement == DrawerPlacement::Right;
        check(s.bounds.width == (side ? 360 : 700) && s.bounds.height == (side ? 600 : 300) &&
                  s.bounds.x == (placement == DrawerPlacement::Right ? 340 : 0) &&
                  s.bounds.y == (placement == DrawerPlacement::Bottom ? 300 : 0) && s.maximum_scroll > 0,
              "Drawer four-edge layout failed");
        const auto header = bounds(f, s.title);
        const auto footer = bounds(f, s.footer);
        check(s.body_bounds.y > header.y && s.body_bounds.y + s.body_bounds.height <= footer.y,
              "Drawer fixed regions overlapped");
        check(last.focus(), "Drawer final input focus failed");
        sync(f);
        const auto shown = f.services.modal().snapshot(id);
        const auto input = bounds(f, f.inputs.mounted_inputs()[0].component);
        if (!(shown.scroll > 0 && input.y >= shown.body_bounds.y - .01F &&
              input.y + input.height <= shown.body_bounds.y + shown.body_bounds.height + .01F &&
              std::abs(shown.scroll * 1.25F - std::round(shown.scroll * 1.25F)) < .01F)) {
            std::cerr << "placement=" << static_cast<int>(placement) << " body=" << shown.body_bounds.y << ','
                      << shown.body_bounds.height << " input=" << input.y << ',' << input.height
                      << " scroll=" << shown.scroll << " max=" << shown.maximum_scroll << '\n';
        }
        check(shown.scroll > 0 && input.y >= shown.body_bounds.y - .01F &&
                  input.y + input.height <= shown.body_bounds.y + shown.body_bounds.height + .01F &&
                  std::abs(shown.scroll * 1.25F - std::round(shown.scroll * 1.25F)) < .01F,
              "Drawer focus reveal or scroll phase failed");
        const auto node = f.services.components().root(shown.body);
        const auto measures = f.nodes.require(node).measure_count;
        check(f.inputs.dispatch(ScrollInputEvent{0, 1, shown.body_bounds.x + 3, shown.body_bounds.y + 3}),
              "Drawer wheel unhandled");
        sync(f);
        check(f.services.modal().snapshot(id).scroll < shown.scroll && bounds(f, shown.title) == header &&
                  bounds(f, shown.footer) == footer && f.nodes.require(node).measure_count == measures,
              "Drawer scroll moved fixed regions or remeasured");
    }
    const auto shown = f.services.modal().snapshot(id);
    const auto body = f.services.components().root(shown.body);
    const auto measures = f.nodes.require(body).measure_count;
    ThemeConfig custom;
    custom.drawer.tokens.background = Color::rgba8(50, 70, 90);
    custom.drawer.tokens.border = Color::rgba8(80, 110, 150);
    theme.set(custom);
    sync(f);
    check(f.nodes.require(body).measure_count == measures && body_runs == 1 && title_runs == 1 && footer_runs == 1,
          "Drawer color update remeasured/remounted");
    width.set(400);
    sync(f);
    check(f.services.modal().snapshot(id).bounds.width == 400 && body_runs == 1, "Drawer width was not reactive");
    height.set(250);
    edge.set(DrawerPlacement::Bottom);
    sync(f);
    check(f.services.modal().snapshot(id).bounds == runtime::Rect{0, 350, 700, 250}, "Drawer height was not reactive");
    for (auto placement :
         {DrawerPlacement::Left, DrawerPlacement::Right, DrawerPlacement::Top, DrawerPlacement::Bottom}) {
        edge.set(placement);
        sync(f, 1, 1);
        check(f.services.modal().snapshot(id).bounds == runtime::Rect{0, 0, 1, 1}, "Drawer tiny window overflowed");
    }
    sync(f);
    check(f.services.components().component_count() == count && body_runs == 1, "Drawer resize rebuilt slots");
}

void mixed_stack_and_child_popup(bool drawer_parent, bool drawer_child) {
    Signal<bool> parent_open{false};
    Signal<bool> child_open{false};
    InputRef outside;
    InputRef parent;
    InputRef child;
    bool reopen{};
    int closed{};
    Fixture f;
    const auto child_body = [&] {
        Input(InputProps{}.ref(child));
    };
    const auto child_closed = [&] {
        ++closed;
        if (reopen) {
            reopen = false;
            child_open.set(true);
        }
    };
    const auto parent_body = [&] {
        Input(InputProps{}.ref(parent));
        Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
        if (!drawer_child) {
            Modal(ModalProps{}
                      .open(child_open)
                      .onCancel([&](ModalCancelReason) { child_open.set(false); })
                      .afterClose(child_closed),
                  ModalContent{child_body});
        } else {
            Drawer(DrawerProps{}
                       .open(child_open)
                       .onCancel([&](DrawerCancelReason) { child_open.set(false); })
                       .afterClose(child_closed),
                   DrawerContent{child_body});
        }
    };
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(outside));
        if (drawer_parent) {
            Drawer(DrawerProps{}.open(parent_open), DrawerContent{parent_body});
        } else {
            Modal(ModalProps{}.open(parent_open), ModalContent{parent_body});
        }
    }});
    sync(f);
    const auto parent_id = f.services.modal().mounted()[0];
    const auto child_id = f.services.modal().mounted()[1];
    check(outside.focus(), "mixed stack initial focus failed");
    parent_open.set(true);
    sync(f);
    check(parent.focus(), "mixed parent focus failed");
    const auto select = f.services.select().mounted()[0];
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "Drawer child Select did not open");
    key(f, Key::escape);
    check(!f.services.select().snapshot(select).visible && f.services.modal().snapshot(parent_id).visible,
          "child popup Escape closed the parent");
    check(parent.focus(), "parent refocus failed");
    child_open.set(true);
    sync(f);
    check(child.focus() && !parent.focus() && !outside.focus() && f.services.modal().snapshot(child_id).topmost &&
              !f.services.modal().snapshot(parent_id).topmost,
          "mixed stack boundary failed");
    key(f, Key::escape);
    check(!child_open.get() && closed == 1 &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[1].interaction,
          "mixed close failed to restore parent");
    child_open.set(true);
    sync(f);
    reopen = true;
    child_open.set(false);
    sync(f);
    check(child_open.get() && child.focus() && closed == 2, "Drawer mixed lifecycle reentry failed");
    check(f.services.destroy(child_id), "mixed child destroy failed");
    sync(f);
    check(!child.bound() && parent.focus() && !outside.focus() && f.services.modal().snapshot(parent_id).topmost,
          "mixed destroy left stale focus/boundary");
    parent_open.set(false);
    sync(f);
    check(f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction &&
              !f.services.interactions().input_boundary(),
          "mixed parent close failed to restore outside");
}

void empty_panel_and_callback_retirement() {
    Signal<bool> open{true};
    Fixture f;
    runtime::ComponentId owner;
    int closed{};
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open(open).closable(false).afterClose([&] { ++closed; }).afterOpenChange([&](bool value) {
            if (!value) {
                f.services.destroy(owner);
            }
        }),
               DrawerContent{[] {}});
    }});
    owner = f.services.modal().mounted()[0];
    sync(f);
    check(f.services.focus().state().focused.has_value(), "empty Drawer has no focus anchor");
    const auto focused = f.services.focus().state().focused;
    key(f, Key::tab);
    check(f.services.focus().state().focused == focused, "empty Drawer Tab escaped");
    open.set(false);
    sync(f);
    check(!closed && f.services.modal().mounted().empty() && !f.services.interactions().input_boundary(),
          "retired Drawer received late lifecycle notification");
}

void document_clip_and_child_scrolling() {
    InputRef area;
    Fixture f;
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open().width(420.0F).title(u8"Document"), DrawerContent{[&] {
                   TextArea(TextAreaProps{}
                                .defaultValue(u8"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten")
                                .rows(3)
                                .ref(area));
                   for (int index = 0; index < 30; ++index) {
                       Text(u8"Drawer body content");
                   }
               }});
    }});
    const auto clipped_sync = [&] {
        for (int frame = 0; frame < 3; ++frame) {
            check(f.services.layout_and_synchronize({700, 600}, {350, 200, 250, 200}),
                  "Drawer document-clip layout failed");
        }
    };
    clipped_sync();
    const auto id = f.services.modal().mounted()[0];
    const auto shown = f.services.modal().snapshot(id);
    check(shown.bounds == runtime::Rect{280, 0, 420, 600} && area.focus({InputFocusCursor::Start}),
          "Drawer inherited the document clip or lost logical focus");
    clipped_sync();
    const auto editor = f.inputs.mounted_inputs()[0];
    const auto input = f.inputs.layout_snapshot(editor.component);
    check(input.viewport.x < 350 && input.clip.x == input.viewport.x && input.clip.width == input.viewport.width,
          "document clip cropped the Drawer editor");
    const auto scroll = f.services.modal().snapshot(id).scroll;
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, input.viewport.x + 3, input.viewport.y + 3}),
          "Drawer TextArea wheel unhandled");
    clipped_sync();
    check(f.inputs.layout_snapshot(editor.component).vertical_scroll > 0 &&
              f.services.modal().snapshot(id).scroll == scroll,
          "Drawer stole its child TextArea wheel");
}
} // namespace

int main() {
    try {
        controlled_retention_and_cancellation();
        geometry_scrolling_theme_and_slots();
        mixed_stack_and_child_popup(true, false);
        mixed_stack_and_child_popup(false, true);
        mixed_stack_and_child_popup(true, true);
        empty_panel_and_callback_retirement();
        document_clip_and_child_scrolling();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
