#include "component/typography_component.hpp"
#include "component/tooltip_component.hpp"
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

std::vector<InteractionId> focusable(detail::WindowComponentServices& services) {
    std::vector<InteractionId> result;
    for (const auto id : services.interactions().declaration_order()) {
        if (services.interactions().require(id).focusable) {
            result.push_back(id);
        }
    }
    return result;
}

String hint_content(Fixture& f, runtime::ComponentId tip) {
    const auto popup = f.services.components().children(tip)[1];
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.services.components().parent(text.component) == popup) {
            return String::from_utf8(f.scene.text_state(text.scene).display_content().utf8()).value();
        }
    }
    throw std::runtime_error("tooltip text not found");
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
    const auto action = focusable(services).back();
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
    const auto declarations = focusable(f.services);
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
    const auto actions = focusable(f.services);
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
    const auto edit = focusable(f.services).front();
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
    const auto actions = focusable(queued.services);
    pending = actions[1];
    target = queued.services.interactions().require(pending).component;
    queued.synchronize();
    activate(queued.services, actions[0]);
    require(queued.services.focus().state().focused == actions[0], "stale queued target stole focus");
}

void typography_hints() {
    Fixture f;
    Signal<TypographyCopyable> copy_config{TypographyCopyable{}};
    Signal<TypographyEditable> edit_config{TypographyEditable{}};
    Signal<bool> disabled{false};
    Signal<String> content{String{u8"Long original content which must be truncated in the narrow viewport"}};
    f.services.mount(Content{[&] {
        Paragraph(TypographyProps{}
                      .content(content)
                      .disabled(disabled)
                      .ellipsis(TypographyEllipsis{.expandable = true, .tooltip = String{u8"Full content"}})
                      .copyable(copy_config)
                      .editable(edit_config));
        Link(LinkProps{}.content(u8"Sibling"));
    }});
    f.synchronize(180);
    const auto id = f.services.typography().mounted().front();
    const auto tips = f.services.tooltip().mounted();
    const auto text_tip = tips[0];
    const auto copy_tip = tips[1];
    const auto edit_tip = tips[2];
    const auto actions = focusable(f.services);
    const auto expand = actions[0];
    const auto copy_action = actions[1];
    const auto edit_action = actions[2];
    const auto components = f.services.components().component_count();
    const auto runs = f.services.components().mount_runs();
    const auto tick = [&](std::int64_t micros) {
        (void)f.services.tick_animations(animation::AnimationTime::microseconds(micros));
        f.synchronize(180);
    };
    const auto hover = [&](runtime::Rect bounds) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none,
                                       bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    };
    const auto action_bounds = [&](InteractionId action) {
        return f.nodes.require(f.services.interactions().require(action).node).bounds;
    };
    hover(action_bounds(copy_action));
    tick(0);
    tick(100000);
    require(f.services.tooltip().snapshot(copy_tip).visible && !f.services.tooltip().snapshot(text_tip).visible,
            "copy hover opened text hint or failed to show copy hint");
    require(hint_content(f, copy_tip) == String{u8"复制"}, "copy tooltip text missing");
    activate(f.services, copy_action);
    tick(200000);
    require(f.services.typography().snapshot(id).copied && f.services.tooltip().snapshot(copy_tip).visible,
            "copy success lost focused hint");
    require(hint_content(f, copy_tip) == String{u8"已复制"}, "copied tooltip text missing");
    copy_config.set(TypographyCopyable{.tooltip = String{u8"Copy custom"}, .copied_tooltip = String{u8"Done"}});
    f.synchronize(180);
    require(hint_content(f, copy_tip) == String{u8"Done"}, "reactive copied tooltip text missing");
    tick(3200000);
    require(!f.services.typography().snapshot(id).copied && hint_content(f, copy_tip) == String{u8"Copy custom"},
            "feedback timeout did not restore normal tooltip");
    f.platform.clipboard_failure = true;
    activate(f.services, copy_action);
    f.synchronize(180);
    require(f.services.typography().snapshot(id).copy_failed && hint_content(f, copy_tip) == String{u8"Copy custom"},
            "failed copy showed success tooltip");
    f.platform.clipboard_failure = false;
    copy_config.set(TypographyCopyable{.tooltip = String{}, .copied_tooltip = String{}});
    f.synchronize(180);
    require(!f.services.tooltip().snapshot(copy_tip).visible, "empty copy hint remained visible");
    require(f.services.interactions().require(copy_action).eligible, "empty hint disabled copy");
    copy_config.set(TypographyCopyable{});
    activate(f.services, edit_action);
    f.synchronize(180);
    require(!f.services.tooltip().snapshot(copy_tip).visible && !f.services.tooltip().snapshot(edit_tip).visible,
            "editing retained action hints");
    key(f.services, Key::escape);
    tick(3300000);
    require(f.services.tooltip().snapshot(edit_tip).visible, "edit keyboard focus hint missing");
    edit_config.set(TypographyEditable{.tooltip = String{}});
    f.synchronize(180);
    require(!f.services.tooltip().snapshot(edit_tip).visible, "empty edit hint remained visible");
    f.services.focus().clear_focus();
    const auto text_anchor = f.nodes.require(f.services.components().root(text_tip)).bounds;
    hover(text_anchor);
    tick(3400000);
    tick(3500000);
    require(f.services.tooltip().snapshot(text_tip).visible, "truncated text hint missing");
    require(f.services.layout_and_synchronize({900, 600}, {0, 0, 900, 600}), "wide hint layout failed");
    require(!f.services.typography().snapshot(id).truncated && !f.services.tooltip().snapshot(text_tip).visible,
            "resize retained unnecessary text hint");
    f.synchronize(180);
    activate(f.services, expand);
    f.synchronize(180);
    require(!f.services.tooltip().snapshot(text_tip).visible, "expanded text retained hint");
    activate(f.services, expand);
    f.synchronize(180);
    disabled.set(true);
    tick(3600000);
    require(!f.services.tooltip().snapshot(text_tip).visible && !f.services.tooltip().snapshot(copy_tip).visible &&
                !f.services.tooltip().snapshot(edit_tip).visible,
            "disabled typography retained hints");
    disabled.set(false);
    content.set(String{u8"short"});
    tick(4000000);
    f.services.set_window_active(false);
    require(!f.services.next_frame_deadline(), "inactive hint retained a deadline");
    require(f.services.components().component_count() == components && f.services.components().mount_runs() == runs,
            "hint updates remounted components");
    f.services.set_window_active(true);
    require(f.services.destroy(id), "hint owner destroy failed");
    f.synchronize(180);
    require(f.services.tooltip().mounted().empty() && !f.services.next_frame_deadline(),
            "hint owner destroy leaked popups or deadlines");
}

void multiline_editing() {
    Fixture f;
    const String original{u8"第一行 / First line\n第二行 / Second line"};
    Signal<TypographyEditable> config{TypographyEditable{}};
    Signal<TypographyLevel> level{TypographyLevel::H4};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    String committed;
    int commits{};
    f.services.mount(Content{[&] {
        Title(
            TitleProps{}.level(level).direction(direction).content(original).editable(config).onEdit([&](String value) {
                ++commits;
                committed = std::move(value);
            }));
        TextArea(TextAreaProps{}.defaultValue(u8"ordinary"));
    }});
    f.synchronize(180);
    const auto id = f.services.typography().mounted().front();
    const auto editor = f.inputs.mounted_inputs()[0];
    const auto ordinary = f.inputs.mounted_inputs()[1];
    const auto edit_action = focusable(f.services)[0];
    const auto count = f.services.components().component_count();
    const auto runs = f.services.components().mount_runs();
    activate(f.services, edit_action);
    f.synchronize(180);
    const auto shell_visible = [&] {
        const auto bounds = f.nodes.require(editor.node).bounds;
        return std::ranges::any_of(f.services.rounded_effects().packed_instances(), [&](const auto& effect) {
            return effect.geometry.shape.rect == bounds && effect.material.visible && effect.material.opacity > 0;
        });
    };
    require(shell_visible(), "initial Typography editor shell missing");
    require(f.inputs.editors().require(editor.editor).value() == original.bytes(),
            "Typography editor discarded original newlines");
    const auto initial_height = f.nodes.require(editor.node).bounds.height;
    (void)f.inputs.editors().require(editor.editor).select_all();
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"first"}, f.inputs.sessions().active()})),
            "multiline replacement failed");
    f.services.focus().dispatch({Key::enter, KeyAction::down, KeyModifier::shift});
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"second"}, f.inputs.sessions().active()})),
            "second line insertion failed");
    f.synchronize(180);
    require(commits == 0 && f.services.typography().snapshot(id).draft == String{u8"first\nsecond"},
            "Shift+Enter submitted or lost newline");
    config.set(TypographyEditable{.auto_size = TextAreaAutoSize{true, 2, 2}});
    f.synchronize(180);
    const auto capped_height = f.nodes.require(editor.node).bounds.height;
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"\nthird\nfourth\nfifth"}, f.inputs.sessions().active()})),
            "multiline text commit failed");
    f.synchronize(180);
    require(f.nodes.require(editor.node).bounds.height == capped_height && initial_height > 30,
            "Typography autoSize row bounds not applied");
    direction.set(TextDirection::RightToLeft);
    level.set(TypographyLevel::H2);
    f.synchronize(180);
    require(f.inputs.layout_snapshot(editor.component).baseline > 20, "multiline editor lost heading typography");
    key(f.services, Key::enter);
    f.synchronize(180);
    require(commits == 1 && committed == String{u8"first\nsecond\nthird\nfourth\nfifth"} &&
                !f.services.typography().snapshot(id).editing && f.services.focus().state().focused == edit_action,
            "multiline Enter failed submit and focus return");
    activate(f.services, edit_action);
    f.synchronize(180);
    require(shell_visible(), "reactivated Typography editor shell remained hidden");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"cancel me"}, f.inputs.sessions().active()})),
            "multiline cancel draft insertion failed");
    key(f.services, Key::escape);
    f.synchronize(180);
    require(commits == 1 && f.services.typography().snapshot(id).original == committed &&
                f.services.typography().snapshot(id).draft == committed,
            "multiline cancel changed original content");
    activate(f.services, edit_action);
    require(f.services.focus().request_focus(ordinary.interaction, FocusModality::keyboard),
            "ordinary TextArea focus failed");
    require(commits == 2 && !f.services.typography().snapshot(id).editing,
            "multiline blur did not preserve existing commit behavior");
    (void)f.inputs.editors().require(ordinary.editor).select({8, 8});
    key(f.services, Key::enter);
    require(f.inputs.editors().require(ordinary.editor).value() == "ordinary\n" && commits == 2,
            "Typography Enter override changed ordinary TextArea");
    require(f.services.components().component_count() == count && f.services.components().mount_runs() == runs,
            "multiline editor remounted subtree");
}

void ime_before_other_hints() {
    Fixture f;
    f.services.mount(Content{[] {
        Text(TypographyProps{}.content(u8"Copy").copyable(TypographyCopyable{}));
        Paragraph(TypographyProps{}.content(u8"Edit\nsecond line").editable(TypographyEditable{}));
    }});
    f.synchronize();
    const auto copy_tip = f.services.tooltip().mounted()[0];
    const auto owner = f.services.typography().mounted()[1];
    const auto actions = focusable(f.services);
    const auto editor = f.inputs.mounted_inputs()[0];
    const auto bounds = f.nodes.require(f.services.interactions().require(actions[0]).node).bounds;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none,
                                   bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    (void)f.services.tick_animations(animation::AnimationTime::microseconds(0));
    (void)f.services.tick_animations(animation::AnimationTime::microseconds(100000));
    f.synchronize();
    require(f.services.tooltip().snapshot(copy_tip).visible, "unrelated copy hint did not open");
    activate(f.services, actions[1]);
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"输入"}, {2, 0}, f.inputs.sessions().active()})),
            "IME priority test composition failed");
    key(f.services, Key::escape);
    require(!f.inputs.editors().require(editor.editor).composition().active &&
                f.services.typography().snapshot(owner).editing && f.services.tooltip().snapshot(copy_tip).visible,
            "unrelated Tooltip consumed Escape before active IME");
    key(f.services, Key::escape);
    require(!f.services.typography().snapshot(owner).editing, "unrelated Tooltip consumed draft cancellation");
    (void)f.services.focus().request_focus(actions[0], FocusModality::keyboard);
    key(f.services, Key::escape);
    require(!f.services.tooltip().snapshot(copy_tip).visible, "unhandled Escape no longer dismisses Tooltip");
}
} // namespace

int main() {
    try {
        copy_without_input();
        expand_and_edit();
        controlled_and_link();
        limits_and_lifecycle();
        typography_hints();
        multiline_editing();
        ime_before_other_hints();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
