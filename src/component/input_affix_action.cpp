#include "component/input_affix_action.hpp"

#include "input/pressable_behavior.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/text.hpp>

#include <optional>
#include <utility>

namespace ryn::detail {
namespace {
struct InputAffixActionState {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId fragment;
    input::PressableBehavior press;
    std::function<void()> activate;
    bool disabled{}, visible{true};
};

void synchronize(InputAffixActionState& state, WindowComponentServices& host, bool layout_changed) {
    if(layout_changed) {
        layout::BoxLayout box;
        if(state.visible) box.padding.left = box.padding.right = 4.0F;
        host.layout().set_layout(state.node, box);
        host.dirty().invalidate(state.node, runtime::DirtyFlags::Measure
            | runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry
            | runtime::DirtyFlags::HitTest);
    }
    if(state.disabled || !state.visible) {
        static_cast<void>(state.press.reset());
        host.pointer().cancel_interaction(state.interaction);
    }
    static_cast<void>(host.interactions().set_eligible(state.interaction,
        !state.disabled && state.visible));
    host.focus().synchronize();
}
} // namespace

void mount_input_affix_action(WindowComponentServices& host, Prop<String> label,
    Prop<bool> disabled, std::function<void()> activate, Prop<bool> visible) {
    auto& build = runtime::require_component_build_context();
    const auto component = build.mount_component<InputAffixActionState>();
    auto& state = build.state<InputAffixActionState>(component);
    state.component = component;
    state.node = build.root(component);
    state.activate = std::move(activate);
    state.disabled = read_prop(disabled);
    state.visible = read_prop(visible);
    build.on_resource_cleanup(component, [&host, component] {
        if(auto* current = host.components().state<InputAffixActionState>(component)) {
            host.pointer().cancel_interaction(current->interaction);
            host.focus().cancel_interaction(current->interaction);
            static_cast<void>(host.interactions().remove(current->interaction));
            static_cast<void>(host.scene_composer().remove_fragment(current->fragment));
            static_cast<void>(host.layout().remove_layout(current->node));
        }
    });
    layout::BoxLayout box;
    if(state.visible) box.padding.left = box.padding.right = 4.0F;
    host.layout().set_layout(state.node, box);
    std::optional<input::InteractionId> parent;
    for(auto ancestor = host.components().parent(component); ancestor && !parent;
        ancestor = host.components().parent(*ancestor)) {
        for(const auto interaction : host.interactions().declaration_order()) {
            if(const auto* record = host.interactions().find(interaction);
                record && record->component == *ancestor) { parent = interaction; break; }
        }
    }
    state.interaction = host.interactions().create({component, state.node, parent,
        !state.disabled && state.visible, true, {}, false});
    state.fragment = build.register_scene_fragment(component,
        runtime::SceneFragmentPlacement::before_children);
    host.scene_composer().set_fragment(state.fragment, {}, state.interaction);
    host.mark_scene_structure_dirty();
    input::InteractionHandlers pointer;
    pointer.target = [&host, component](input::PointerDispatchContext& context) {
        auto* current = host.components().state<InputAffixActionState>(component);
        if(!current) return;
        const auto result = current->press.dispatch(context, current->interaction,
            host.interactions().require(current->interaction).eligible);
        if(result.activate) {
            auto callback = current->activate;
            callback();
        }
    };
    static_cast<void>(host.interactions().set_handlers(state.interaction, std::move(pointer)));
    input::FocusHandlers focus;
    focus.activation_allowed = [&host, component] {
        if(const auto* current = host.components().state<InputAffixActionState>(component))
            return host.interactions().require(current->interaction).eligible;
        return false;
    };
    focus.activate = [&host, component] {
        if(auto* current = host.components().state<InputAffixActionState>(component)) {
            auto callback = current->activate;
            callback();
        }
    };
    static_cast<void>(host.interactions().set_focus_handlers(state.interaction, std::move(focus)));
    static_cast<void>(connect_prop(build.scope(component), disabled,
        [&host, component](bool value) {
            if(auto* current = host.components().state<InputAffixActionState>(component)) {
                if(current->disabled == value) return;
                current->disabled = value;
                synchronize(*current, host, false);
            }
        }));
    static_cast<void>(connect_prop(build.scope(component), visible,
        [&host, component](bool value) {
            if(auto* current = host.components().state<InputAffixActionState>(component)) {
                if(current->visible == value) return;
                current->visible = value;
                synchronize(*current, host, true);
            }
        }));
    build.mount_slot(component, Content{[label = std::move(label), visible = std::move(visible)] {
        Text(TextProps{}.content(bind([label, visible] {
            return read_prop(visible) ? read_prop(label) : String{};
        })));
    }});
}

} // namespace ryn::detail
