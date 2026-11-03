#include "gallery_source_panel.hpp"
#include "component/collapse_component.hpp"
#include "support/input_fixture.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void test_source_panel() {
    ryn_test::input_component::Fixture f;
    Signal<LogicalLength> width{dp(720)};
    String copied;
    const auto* source = rynui::example::gallery_sample_source("gallery.sample.button.basics");
    require(source, "compiled sample source is missing");
    f.services.mount(Content{
        [&] { rynui::example::gallery_source_panel(*source, width, [&](const String& text) { copied = text; }); }});
    const auto sync = [&] {
        require(f.buttons.layout_and_synchronize({800, 900}, {0, 0, 800, 900}, {}, 0, true),
                "source panel layout failed");
    };
    sync();
    const auto collapse = f.services.collapse().mounted().front();
    const auto original = f.services.collapse().snapshot(collapse);
    require(original.active_keys.empty() && original.panels.size() == 1, "source panel did not start collapsed");
    const auto editor = f.inputs.mounted_inputs().front();
    require(!f.services.focus().request_focus(editor.interaction, input::FocusModality::keyboard),
            "collapsed source editor remained focusable");
    const auto header = original.panels.front().header;
    bool focused{};
    for (const auto id : f.services.interactions().declaration_order()) {
        const auto& hit = f.services.interactions().require(id);
        if (hit.component == header && hit.focusable) {
            focused = f.services.focus().request_focus(id, input::FocusModality::keyboard);
            break;
        }
    }
    require(focused, "source header has no native keyboard target");
    f.services.focus().dispatch({input::Key::enter, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::enter, input::KeyAction::up});
    sync();
    require(f.services.collapse().snapshot(collapse).active_keys == CollapseKeys{u8"cpp"},
            "Enter did not expand actual C++");
    require(f.inputs.editors().require(editor.editor).read_only() &&
                f.inputs.editors().require(editor.editor).value() == source->cpp,
            "source editor is editable or contains different code");
    require(f.services.focus().request_focus(editor.interaction, input::FocusModality::keyboard),
            "expanded source editor is unreachable");
    width.set(dp(328));
    sync();
    // The second source line is the long actual function signature. A short
    // first-line location comment need not overflow a narrow code viewport.
    f.services.focus().dispatch({input::Key::home, input::KeyAction::down, input::KeyModifier::control});
    f.services.focus().dispatch({input::Key::home, input::KeyAction::up, input::KeyModifier::control});
    f.services.focus().dispatch({input::Key::down, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::down, input::KeyAction::up});
    f.services.focus().dispatch({input::Key::end, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::end, input::KeyAction::up});
    sync();
    const auto layout = f.inputs.layout_snapshot(editor.component);
    require(layout.text_width > layout.viewport.width && layout.scroll_offset > 0 &&
                f.nodes.require(editor.node).bounds.width <= width.get().value() &&
                f.inputs.mounted_inputs().front().editor == editor.editor,
            "narrow C++ did not scroll horizontally in its retained editor");
    const auto copy = f.buttons.mounted_buttons().front();
    require(f.services.focus().request_focus(copy.interaction, input::FocusModality::keyboard),
            "copy code is unreachable");
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::space, input::KeyAction::up});
    sync();
    require(copied.bytes() == source->cpp, "copy code did not use the actual compiled excerpt");
}
} // namespace

int main() {
    try {
        test_source_panel();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
