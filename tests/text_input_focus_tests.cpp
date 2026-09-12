#include "input/text_input_session.hpp"
#include "input/focus_manager.hpp"
#include <ryn/component.hpp>

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn::input;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
struct State {};
struct Platform final : TextInputPlatform {
    int starts{}, stops{};
    bool start(TextInputSessionStamp, const TextInputProperties&) noexcept override { ++starts; return true; }
    bool stop() noexcept override { ++stops; return true; }
    bool cancel() noexcept override { return true; }
    bool set_area(const WindowTextInputArea&) noexcept override { return true; }
};
void journey() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components{nodes};
    InteractionRegistry registry{components, nodes};
    FocusManager focus{registry};
    TextEditorStore editors;
    Platform platform;
    TextInputSessionHost sessions{editors, platform};
    std::array<ryn::runtime::ComponentId, 2> component_ids;
    std::array<InteractionId, 2> interactions;
    std::array<TextInputOwnerId, 2> owners;
    components.mount(ryn::Content{[&] {
        auto& build = ryn::runtime::require_component_build_context();
        for(std::size_t index = 0; index < 2; ++index) {
            const auto component = build.mount_component<State>();
            const auto owner = editors.create();
            const auto interaction = registry.create({component, build.root(component), {}, true, true, {}});
            component_ids[index] = component;
            interactions[index] = interaction;
            owners[index] = owner;
            FocusHandlers handlers;
            handlers.state_changed = [&, owner](FocusPresentation value) {
                if(value.focused) static_cast<void>(sessions.focus(owner));
                else if(sessions.active().owner == owner) static_cast<void>(sessions.blur());
            };
            registry.set_focus_handlers(interaction, std::move(handlers));
            build.on_resource_cleanup(component, [&, owner, interaction] {
                focus.cancel_interaction(interaction);
                static_cast<void>(editors.destroy(owner));
                static_cast<void>(registry.remove(interaction));
            });
        }
    }});
    require(focus.request_focus(interactions[0], FocusModality::keyboard), "focus request rejected");
    const auto first = sessions.active();
    require(first.owner == owners[0], "Focus callback did not start session");
    require(bool(sessions.dispatch(CompositionChanged{ryn::String(u8"ni"), {2, 0}, first})), "preedit rejected");
    focus.dispatch({Key::tab, KeyAction::down});
    require(sessions.active().owner == owners[1], "Tab did not transfer session");
    require(!editors.require(owners[0]).composition().active, "Tab retained old preedit");
    require(!sessions.dispatch(TextCommitted{ryn::String(u8"你"), first}), "Tab routed late commit");
    require(sessions.set_window_active(false), "window session stop failed");
    focus.set_window_active(false);
    require(!sessions.active().valid(), "window loss retained native session");
    focus.set_window_active(true);
    require(sessions.set_window_active(true), "window session restore failed");
    const auto removed = sessions.active();
    require(components.destroy(component_ids[1]), "conditional unmount failed");
    require(!sessions.active().valid() && !editors.find(owners[1]), "unmount leaked owner/session");
    require(!sessions.dispatch(TextCommitted{ryn::String(u8"late"), removed}), "unmounted commit accepted");
    require(focus.focus_from_pointer(interactions[0]), "pointer focus failed");
    require(sessions.active().owner == owners[0], "pointer focus did not start session");
    components.dispose();
    require(editors.size() == 0 && !sessions.active().valid(), "dispose retained editor resources");
    require(platform.starts == platform.stops, "native session lifetime imbalance");
}
}
int main() {
    try { journey(); std::cout << "Focus, Tab, pointer focus and conditional unmount text session passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
