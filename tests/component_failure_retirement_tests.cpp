#include "support/input_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

struct MountFailure final {
    std::shared_ptr<int> identity;
};

struct RetireOnFailure final {
    detail::WindowComponentServices& services;
    runtime::ComponentId owner;

    ~RetireOnFailure() {
        (void)services.retire_failed_component(owner);
    }
};

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

runtime::ComponentId region(Fixture& f) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
    return id;
}

void failure_retirement(int mode, bool replace) {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    runtime::ComponentId safe;
    runtime::ComponentId owner;
    InputRef reference;
    Signal<String> value{String{u8"Old"}};
    int blurs = 0;
    int cancels = 0;
    std::shared_ptr<theme_runtime::ThemeScope> theme;
    std::size_t subscribers = 0;
    f.services.mount(Content{[&] {
        safe = region(f);
        theme = runtime::require_component_build_context().theme_scope();
        subscribers = theme->diagnostics().subscriber_count;
        owner = region(f);
        runtime::require_component_build_context().mount_slot(
            owner, Content{[&] {
                Input(InputProps{}.ref(reference).value(value).onBlur([&] {
                    ++blurs;
                    if (mode == 2) {
                        throw std::runtime_error("blur must not run during failure retirement");
                    }
                    (void)f.services.destroy(owner);
                    if (mode == 1) {
                        f.services.append_slot(safe, Content{[&] { Input(InputProps{}.ref(reference)); }});
                    }
                }));
                Button(ButtonProps{}, [] { Text(u8"Capture"); });
            }});
    }});
    f.synchronize();
    const auto old = f.inputs.mounted_inputs().front();
    const auto old_root = f.services.components().root(owner);
    const auto reset = detail::capture_form_input(reference).second;
    const auto button = f.buttons.mounted_buttons().front();
    auto handlers = *f.services.interactions().require(button.interaction).handlers;
    const auto native = handlers.target;
    handlers.target = [&, native](PointerDispatchContext& event) {
        if (event.kind() == PointerEventKind::cancel || event.kind() == PointerEventKind::leave) {
            ++cancels;
            throw std::runtime_error("pointer cancellation must not deliver during failure retirement");
        }
        if (native) {
            native(event);
        }
        if (event.kind() == PointerEventKind::down) {
            (void)event.capture_pointer();
        }
    };
    f.services.interactions().set_handlers(button.interaction, std::move(handlers));
    f.services.interactions().set_pointer_focus_predicate(button.interaction, [] { return false; });
    require(reference.focus(), "plain descendant focus failed");
    const auto stale = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stale})), "preedit failed");
    const auto bounds = f.nodes.require(f.services.components().root(button.component)).bounds;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    const auto pressed = f.services.pointer().state(PointerIdentity::mouse());
    require(pressed && pressed->capture == button.interaction && pressed->primary_down &&
                f.services.focus().state().focused == old.interaction,
            "plain descendant capture/focus fixture failed");
    const auto identity = std::make_shared<int>(73);
    bool original = false;
    try {
        f.services.append_slot(
            owner, Content{[&] {
                RetireOnFailure rollback{f.services, owner};
                if (replace) {
                    require(f.services.retire_failed_component(owner), "failed generation retirement failed");
                    runtime::require_component_build_context().mount_slot(
                        safe, Content{[&] { Input(InputProps{}.ref(reference).defaultValue(u8"New")); }});
                }
                throw MountFailure{identity};
            }});
    } catch (const MountFailure& error) {
        original = error.identity == identity;
    }
    require(original && blurs == 0 && cancels == 0 && !f.services.components().contains(owner) &&
                !f.inputs.editors().find(old.editor) && !f.services.interactions().find(old.interaction) &&
                !f.nodes.find(old_root) && !f.inputs.sessions().active().valid(),
            "quiet failure retirement lost original exception or retained old resources");
    const auto canceled = f.services.pointer().state(PointerIdentity::mouse());
    require(canceled && !canceled->capture && !canceled->press_origin && !canceled->primary_down &&
                !canceled->hover_target,
            "quiet failure retirement retained pointer state");
    value.set(String{u8"Late old signal"});
    if (replace) {
        require(reference.focus(), "replacement ref was damaged");
        const auto fresh = f.inputs.sessions().active();
        require(fresh != stale && bool(f.inputs.dispatch(CompositionChanged{String{u8"new preedit"}, {}, fresh})),
                "replacement session failed");
        reset();
        require(f.inputs.editors().require(fresh.owner).composition().active &&
                    !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stale})),
                "old reset or input event crossed generation");
        require(f.services.destroy(f.inputs.mounted_inputs().front().component), "replacement cleanup failed");
    } else {
        require(!reference.focus(), "retired reference remained bound");
    }
    f.synchronize();
    require(f.nodes.size() == 1 && f.inputs.mounted_inputs().empty() && f.scene.size() == 0 &&
                f.scene.glyph_scene().instances().size() == 0 && f.services.interactions().size() == 0 &&
                f.services.surfaces().size() == 0 && !f.services.focus().state().focused &&
                !f.inputs.sessions().active().valid() && theme->diagnostics().subscriber_count == subscribers,
            "plain failed subtree retained nodes/editors/hits/surfaces/subscriptions");
    f.services.dispose();
    require(f.nodes.size() == 0 && !f.services.next_frame_deadline(), "plain retirement retained work");
}
} // namespace

int main() {
    try {
        for (int mode = 0; mode < 3; ++mode) {
            for (bool replace : {false, true}) {
                failure_retirement(mode, replace);
            }
        }
        std::cout << "quiet component failure retirement: 6 fixtures passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
