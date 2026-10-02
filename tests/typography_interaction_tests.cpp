#include "component/typography_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void key(detail::WindowComponentServices& services, Key key, KeyAction action = KeyAction::down) {
    services.focus().dispatch({key, action, KeyModifier::none, false, KeyModifier::control});
}

void activate(detail::WindowComponentServices& services, InteractionId target) {
    (void)services.focus().request_focus(target, FocusModality::keyboard);
    require(services.focus().state().focused == target, "action could not focus");
    key(services, Key::enter);
    key(services, Key::enter, KeyAction::up);
}

void copy_without_input() {
    runtime::NodeStore nodes;
    runtime::FrameRequestState frames;
    runtime::DirtyQueues dirty{nodes, &frames};
    layout::LayoutEngine layout{nodes};
    auto fonts = std::move(font::FontRuntime::create().runtime);
    text::TextEngine engine{*fonts};
    detail::TextSceneService scene{*fonts, engine, frames};
    auto latin = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, font::FontRasterConfig{14, 1});
    auto cjk = fonts->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, font::FontRasterConfig{14, 1});
    detail::WindowComponentServices services{nodes, layout, dirty, scene, {latin.font, cjk.font}, frames};
    const String full{u8"完整文本与显示省略内容互相独立"};
    services.mount(Content{
        [&] { Text(TypographyProps{}.content(full).ellipsis(TypographyEllipsis{}).copyable(TypographyCopyable{})); }});
    const auto id = services.typography().mounted().front();
    require(services.layout_and_synchronize({100, 200}, {0, 0, 100, 200}), "copy layout failed");
    const auto action = services.interactions().declaration_order().back();
    require(!services.interactions().require(action).eligible && services.text_edit() == nullptr,
            "unbound copy enabled or created Input service");
    ryn_test::input_component::Platform clipboard;
    services.bind_clipboard(clipboard);
    require(services.interactions().require(action).eligible, "late-bound clipboard did not enable copy");
    activate(services, action);
    require(clipboard.clipboard == full && clipboard.writes == 1,
            "empty clipboard rejected copy or copied display string");
    require(services.typography().snapshot(id).copied && services.next_frame_deadline().has_value(),
            "copy feedback missing");
    (void)services.tick_animations(animation::AnimationTime::microseconds(3'000'000));
    require(!services.typography().snapshot(id).copied && !services.next_frame_deadline(),
            "copy feedback timer did not settle");
    activate(services, action);
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    services.components().theme_scope(id)->update(dark);
    require(!services.typography().snapshot(id).copied && !services.next_frame_deadline(),
            "theme update retained copy feedback");
    clipboard.clipboard_failure = true;
    activate(services, action);
    require(services.typography().snapshot(id).copy_failed && !services.typography().snapshot(id).copied,
            "failed copy reported success");
    clipboard.clipboard_failure = false;
    activate(services, action);
    services.set_window_active(false);
    require(!services.typography().snapshot(id).copied && !services.next_frame_deadline(),
            "inactive window retained timer");
    services.set_window_active(true);
    clipboard.on_clipboard = [&] {
        services.destroy(id);
    };
    activate(services, action);
    require(services.typography().mounted().empty() && services.surfaces().size() == 0,
            "destroy during copy leaked resources");
    services.dispose();
}

void expand_and_edit() {
    Fixture f;
    int commits{};
    String committed;
    f.inputs.mount(Content{[&] {
        Title(TitleProps{}
                  .level(TypographyLevel::H2)
                  .content(u8"Original content long enough to truncate")
                  .ellipsis(TypographyEllipsis{.expandable = true})
                  .editable(TypographyEditable{})
                  .onEdit([&](String value) {
                      ++commits;
                      committed = std::move(value);
                  }));
    }});
    const auto id = f.services.typography().mounted().front();
    const auto input = f.inputs.mounted_inputs().front();
    const auto declarations = f.services.interactions().declaration_order();
    const auto expand = declarations[0];
    const auto edit = declarations[1];
    f.synchronize(190);
    const auto components = f.services.components().component_count();
    const auto runs = f.services.components().mount_runs();
    require(!f.services.interactions().require(input.interaction).eligible && !f.inputs.sessions().active().valid(),
            "prebuilt editor active in display branch");
    require(f.services.typography().snapshot(id).truncated, "ellipsis entry did not reserve width");
    activate(f.services, expand);
    f.synchronize(190);
    require(f.services.typography().snapshot(id).expanded && !f.services.typography().snapshot(id).truncated,
            "expand failed");
    activate(f.services, edit);
    f.synchronize(190);
    require(f.services.typography().snapshot(id).editing && f.services.focus().state().focused == input.interaction &&
                f.inputs.sessions().active().valid(),
            "edit entry failed focus transaction");
    require(f.inputs.layout_snapshot(input.component).baseline > 20, "heading typography did not reach actual Input");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"输入"}, {2, 0}, stamp})), "editing preedit failed");
    key(f.services, Key::escape);
    require(f.services.typography().snapshot(id).editing &&
                !f.inputs.editors().require(input.editor).composition().active,
            "Esc cancelled draft before IME");
    key(f.services, Key::escape);
    f.synchronize(190);
    require(!f.services.typography().snapshot(id).editing && !f.inputs.sessions().active().valid() &&
                f.services.focus().state().focused == edit,
            "second Esc failed cancel/focus");
    activate(f.services, edit);
    auto& editor = f.inputs.editors().require(input.editor);
    (void)editor.select({0, editor.value().size()});
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"Edited"}, f.inputs.sessions().active()})),
            "draft commit failed");
    key(f.services, Key::enter);
    f.synchronize(190);
    require(commits == 1 && committed == String{u8"Edited"} &&
                f.services.typography().snapshot(id).original == committed &&
                !f.services.typography().snapshot(id).editing,
            "uncontrolled submit failed");
    require(f.services.components().component_count() == components && f.services.components().mount_runs() == runs,
            "edit/expand remounted subtree");
    require(f.services.focus().diagnostics().reentrant_rejections == 0, "editor used nested focus transaction");
}

void controlled_and_link() {
    Fixture f;
    Signal<String> value{String{u8"controlled"}};
    Signal<bool> disabled{false};
    bool echo = false;
    int commits{};
    int clicks{};
    f.inputs.mount(Content{[&] {
        Text(TypographyProps{}.content(value).editable(TypographyEditable{}).onEdit([&](String next) {
            ++commits;
            if (echo) {
                value.set(std::move(next));
            }
        }));
        Link(LinkProps{}.content(u8"Link").disabled(disabled).onClick([&] { ++clicks; }));
    }});
    const auto id = f.services.typography().mounted().front();
    const auto input = f.inputs.mounted_inputs().front();
    const auto actions = f.services.interactions().declaration_order();
    const auto edit = actions[0];
    const auto link = actions.back();
    f.synchronize();
    activate(f.services, edit);
    auto& editor = f.inputs.editors().require(input.editor);
    (void)editor.select({0, editor.value().size()});
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"draft"}, f.inputs.sessions().active()})),
            "controlled input failed");
    value.set(String{u8"external"});
    require(f.services.typography().snapshot(id).draft == String{u8"draft"}, "external content overwrote active draft");
    key(f.services, Key::enter);
    auto state = f.services.typography().snapshot(id);
    require(state.editing && state.edit_pending && state.draft == String{u8"draft"} && commits == 1,
            "controlled rejection hid pending draft");
    echo = true;
    key(f.services, Key::enter);
    f.synchronize();
    require(value.get() == String{u8"draft"} && !f.services.typography().snapshot(id).editing,
            "controlled acceptance did not end edit");
    activate(f.services, link);
    require(clicks == 1, "link keyboard activation failed");
    f.synchronize();
    const auto bounds = f.nodes.require(f.services.interactions().require(link).node).bounds;
    const runtime::Point center{bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F};
    const auto pointer = [&](PointerAction action, PointerButton button = PointerButton::none) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, button, center.x, center.y});
    };
    pointer(PointerAction::move);
    pointer(PointerAction::down, PointerButton::primary);
    disabled.set(true);
    require(!f.services.interactions().require(link).eligible && !f.services.focus().state().focused,
            "disabled link retained focus");
    pointer(PointerAction::up, PointerButton::primary);
    require(clicks == 1, "disabled link retained pointer capture");
    key(f.services, Key::enter);
    key(f.services, Key::enter, KeyAction::up);
    require(clicks == 1, "disabled link activated");
    disabled.set(false);
    f.synchronize();
    pointer(PointerAction::down, PointerButton::primary);
    pointer(PointerAction::up, PointerButton::primary);
    require(clicks == 2, "Link pointer activation failed");
    f.services.focus().clear_focus();
    key(f.services, Key::tab);
    require(f.services.focus().state().focused == edit, "suspended Input changed Tab order");
    key(f.services, Key::tab);
    require(f.services.focus().state().focused == link, "Link missing in Tab traversal");
}

void limits_and_lifecycle() {
    Fixture f;
    Signal<bool> disabled{false};
    runtime::ComponentId id;
    bool destroy = false;
    f.inputs.mount(Content{[&] {
        Text(TypographyProps{}
                 .content(u8"abc")
                 .disabled(disabled)
                 .editable(TypographyEditable{.max_length = 3})
                 .onEdit([&](String) {
                     if (destroy) {
                         f.services.destroy(id);
                     }
                 }));
    }});
    id = f.services.typography().mounted().front();
    const auto input = f.inputs.mounted_inputs().front();
    const auto edit = f.services.interactions().declaration_order().front();
    f.synchronize();
    activate(f.services, edit);
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"longer"}, f.inputs.sessions().active()})),
            "limited insertion failed");
    require(f.services.typography().snapshot(id).draft.size_bytes() == 3, "editor maxLength missing");
    disabled.set(true);
    require(!f.services.typography().snapshot(id).editing && !f.inputs.sessions().active().valid(),
            "disabled Typography retained editing session");
    disabled.set(false);
    activate(f.services, edit);
    destroy = true;
    key(f.services, Key::enter);
    require(f.services.typography().mounted().empty() && f.inputs.mounted_inputs().empty() &&
                !f.services.focus().state().focused && !f.inputs.sessions().active().valid(),
            "destroy in edit callback retained draft/focus/session");
    Fixture queued;
    runtime::ComponentId target;
    InteractionId pending;
    queued.services.mount(Content{[&] {
        Link(LinkProps{}.content(u8"first").onClick([&] {
            queued.services.focus().defer_focus(pending, FocusModality::keyboard);
            queued.services.destroy(target);
        }));
        Link(LinkProps{}.content(u8"second"));
    }});
    const auto actions = queued.services.interactions().declaration_order();
    pending = actions[1];
    target = queued.services.interactions().require(pending).component;
    queued.synchronize();
    activate(queued.services, actions[0]);
    require(queued.services.focus().state().focused == actions[0], "stale queued target stole focus");
}
} // namespace

int main() {
    try {
        copy_without_input();
        expand_and_edit();
        controlled_and_link();
        limits_and_lifecycle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
