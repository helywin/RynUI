#include "support/input_fixture.hpp"
#include <ryn/flex.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void paint_identity_and_readonly() {
    ryn_test::input_component::Fixture fixture;
    fixture.inputs.mount(Content{[] { Input(InputProps{}.defaultValue(u8"keep editor identity")); }});
    const auto mounted = fixture.inputs.mounted_inputs().front();
    require(fixture.inputs.paint_snapshot(mounted.component).status == detail::InputPaintStatus::unsynchronized,
            "new Input must not invent a published paint");
    fixture.synchronize();
    const auto text = fixture.inputs.text_layers(mounted.component);
    const auto selection = fixture.inputs.editors().require(mounted.editor).selection();
    const auto revision = fixture.frames.revision();
    const auto requests = fixture.frames.counters().requests;
    const auto before = fixture.services.successful_sync_revision();
    const auto paint = fixture.inputs.paint_snapshot(mounted.component);
    require(paint.status == detail::InputPaintStatus::published && paint.branch_active &&
                paint.owner == mounted.component && paint.editor == mounted.editor && paint.container_fragment.valid(),
            "Input paint must bind actual retained owner/generation/editor/fragment");
    require(paint.selection_surface.valid() && paint.overlay_surface.valid() && !paint.layers.empty(),
            "Input retained surface and primitive identities are absent");
    bool background{};
    bool border{};
    bool focus{};
    for (const auto& layer : paint.layers) {
        require(layer.exists && layer.retained.has_value() &&
                    fixture.services.rounded_effects().contains(layer.primitive),
                "snapshot invented a primitive");
        require(*layer.retained == fixture.services.rounded_effects().at(layer.primitive),
                "snapshot differs from actual retained geometry/material");
        background |= layer.role == detail::InputPaintLayerRole::background;
        border |= layer.role == detail::InputPaintLayerRole::border;
        focus |= layer.role == detail::InputPaintLayerRole::focus;
    }
    require(background && border && focus, "actual background/border/focus layers must remain distinct");
    for (int index = 0; index < 5; ++index) {
        static_cast<void>(fixture.inputs.paint_snapshot(mounted.component));
    }
    require(fixture.frames.revision() == revision && fixture.frames.counters().requests == requests &&
                fixture.services.successful_sync_revision() == before,
            "read-only paint observation woke or refreshed the host");
    require(fixture.inputs.text_layers(mounted.component).base == text.base &&
                fixture.inputs.text_layers(mounted.component).selected == text.selected &&
                fixture.inputs.editors().require(mounted.editor).selection() == selection,
            "paint observation changed editor/selection/text identity");
    fixture.synchronize(320, {1000, 1000, 5, 5});
    const auto clipped = fixture.inputs.paint_snapshot(mounted.component);
    require(std::ranges::any_of(clipped.layers, [](const auto& layer) { return layer.clipped && !layer.hidden; }),
            "clipped paint must not be reported as absent or visible");
    require(fixture.services.destroy(mounted.component), "Input retirement failed");
    const auto stale = fixture.inputs.paint_snapshot(mounted.component);
    require(stale.status == detail::InputPaintStatus::absent && stale.layers.empty() &&
                stale.owner == mounted.component,
            "retired owner returned another generation's paint");
}

void paint_query_preserves_preedit() {
    ryn_test::input_component::Fixture fixture;
    InputRef reference;
    fixture.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"keep editor identity").ref(reference)); }});
    fixture.synchronize();
    const auto mounted = fixture.inputs.mounted_inputs().front();
    require(reference.focus({InputFocusCursor::All}), "preedit focus failed");
    const auto session = fixture.inputs.sessions().active();
    require(bool(fixture.inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, session})),
            "preedit dispatch failed");
    fixture.synchronize();
    const auto scene = fixture.inputs.text_scene(mounted.component);
    const auto shapes = fixture.scene.text_state(scene).counters().shape_count;
    const auto selection = fixture.inputs.editors().require(mounted.editor).selection();
    const auto preedit = fixture.inputs.editors().require(mounted.editor).composition();
    const auto value = std::string{fixture.inputs.editors().require(mounted.editor).value()};
    const auto revision = fixture.frames.revision();
    const auto requests = fixture.frames.counters().requests;
    const auto synchronized = fixture.services.successful_sync_revision();
    for (int index = 0; index < 5; ++index) {
        const auto paint = fixture.inputs.paint_snapshot(mounted.component);
        require(paint.owner == mounted.component && paint.editor == mounted.editor && paint.branch_active,
                "preedit paint owner/editor generation changed");
    }
    const auto after = fixture.inputs.editors().require(mounted.editor).composition();
    require(after.active && after.text == preedit.text && after.selection == preedit.selection &&
                after.replacement == preedit.replacement && fixture.inputs.sessions().active() == session &&
                fixture.inputs.editors().require(mounted.editor).selection() == selection &&
                fixture.inputs.editors().require(mounted.editor).value() == value &&
                fixture.inputs.text_scene(mounted.component) == scene &&
                fixture.scene.text_state(scene).counters().shape_count == shapes &&
                fixture.frames.revision() == revision && fixture.frames.counters().requests == requests &&
                fixture.services.successful_sync_revision() == synchronized,
            "read-only paint query changed editing/selection/composition or scheduled work");
}

void actual_command_order_and_surface_generation() {
    ryn_test::input_component::Fixture fixture;
    fixture.inputs.mount(Content{[] {
        Flex(FlexProps{}.vertical(true),
             FlexContent{[] { Input(InputProps{}.variant(InputVariant::Filled).defaultValue(u8"order")); }});
    }});
    fixture.synchronize();
    const auto mounted = fixture.inputs.mounted_inputs().front();
    const auto paint = fixture.inputs.paint_snapshot(mounted.component);
    const auto binding = fixture.services.scene_composer().paint_snapshot(paint.container_fragment);
    require(binding && binding->commands == paint.commands &&
                binding->publication_revision == paint.publication_revision,
            "Input snapshot must copy the actual fragment publication");
    std::optional<std::size_t> background;
    std::optional<std::size_t> border;
    for (const auto& layer : paint.layers) {
        if (layer.in_fragment) {
            require(layer.command_ordinal && *layer.command_ordinal < paint.commands.size(),
                    "published Input primitive has no actual command ordinal");
            const auto& command = paint.commands[*layer.command_ordinal];
            require(layer.packed_index && command.first_instance <= *layer.packed_index &&
                        *layer.packed_index - command.first_instance < command.instance_count,
                    "Input ordinal does not address its actual primitive");
            if (layer.role == detail::InputPaintLayerRole::background) {
                background = layer.command_ordinal;
            }
            if (layer.role == detail::InputPaintLayerRole::border) {
                border = layer.command_ordinal;
            }
        }
    }
    require(background && border && *background < *border, "Filled Input actual order differs from role enum order");
    const auto surface = fixture.services.surfaces().paint_snapshot(paint.selection_surface);
    require(surface && surface->id == paint.selection_surface && surface->binding &&
                surface->owner == fixture.services.components().fragment_component(surface->fragment) &&
                surface->binding->owner == surface->owner,
            "content surface snapshot lost its actual owner/fragment generation");
    bool input_owns_surface{};
    for (auto owner = surface->owner; owner.valid();) {
        if (owner == mounted.component) {
            input_owns_surface = true;
            break;
        }
        const auto parent = fixture.services.components().parent(owner);
        if (!parent) {
            break;
        }
        owner = *parent;
    }
    require(input_owns_surface, "selection surface owner escaped its retained Input subtree");
    const auto revision = fixture.frames.revision();
    require(fixture.services.surfaces().paint_snapshot(paint.overlay_surface).has_value(), "overlay query failed");
    require(fixture.frames.revision() == revision, "const surface query requested work");
    const auto parent = fixture.services.components().parent(mounted.component);
    require(parent.has_value(), "retained Input fixture has no append owner");
    require(fixture.services.destroy(mounted.component), "owner retirement failed");
    require(!fixture.services.surfaces().paint_snapshot(paint.selection_surface) &&
                !fixture.services.scene_composer().paint_snapshot(paint.container_fragment),
            "retired source survived readonly generation guards");
    fixture.services.append_slot(*parent, Content{[] { Input(InputProps{}.defaultValue(u8"new generation")); }});
    fixture.synchronize();
    require(fixture.inputs.mounted_inputs().size() == 1 &&
                fixture.inputs.mounted_inputs().front().component != mounted.component,
            "replacement Input did not receive a new retained generation");
    require(!fixture.services.surfaces().paint_snapshot(paint.selection_surface),
            "reused content range accepted an old generation");
}
} // namespace

int main() {
    try {
        paint_identity_and_readonly();
        paint_query_preserves_preedit();
        actual_command_order_and_surface_generation();
        std::cout << "Input retained paint snapshot contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
