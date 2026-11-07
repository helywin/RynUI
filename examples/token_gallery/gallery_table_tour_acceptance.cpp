#include "gallery_table_tour_acceptance.hpp"
#include "component/table_component.hpp"
#include "component/tour_component.hpp"
#include "component/modal_component.hpp"
#include "graphics/rounded_effect.hpp"
#include <algorithm>
#include <iostream>

namespace rynui::example {
void accept_table_tour(const GalleryPageRoute& route, ExistingDataEntryActions a) {
    using namespace ryn;
    const auto owns = [&](runtime::ComponentId child, runtime::ComponentId parent) {
        for (auto current = std::optional{child}; current; current = a.app.components().parent(*current)) {
            if (*current == parent) {
                return true;
            }
        }
        return false;
    };
    const auto run = [&](std::string_view target) {
        const auto id = a.pages.target(target);
        a.check(id.valid(), "Table/Tour native Gallery action is absent");
        a.reveal(id);
        a.activate(id);
        a.render();
    };
    if (route.component_identity == "ant.component.table") {
        auto& table = a.app.services().table();
        const auto id = a.pages.target("gallery.table.virtual");
        if (!id.valid()) {
            return;
        }
        a.reveal(id);
        a.render();
        const auto before = table.snapshot(id);
        a.check(before.projected_rows == 1000 && before.rows.size() < 20,
                "native virtual Table eagerly mounted the source");
        runtime::ComponentId editor;
        for (const auto input : a.inputs.mounted_inputs()) {
            if (std::ranges::any_of(before.rows,
                                    [&](const auto& row) { return owns(input.component, row.component); })) {
                const auto node = a.app.components().root(input.component);
                const auto& geometry = a.app.nodes().require(node);
                const auto clipped = graphics::intersect_effect_bounds(a.app.nodes().content_clip(node, before.body),
                                                                       {geometry.bounds.x + geometry.translation.x,
                                                                        geometry.bounds.y + geometry.translation.y,
                                                                        geometry.bounds.width, geometry.bounds.height});
                if (clipped.width <= 0 || clipped.height <= 0) {
                    continue;
                }
                editor = input.component;
                static_cast<void>(a.app.focus().request_focus(input.interaction, input::FocusModality::keyboard));
                if (a.app.focus().state().focused != input.interaction) {
                    const auto* record = a.app.interactions().find(input.interaction);
                    std::cerr << "native_table_focus: body_width=" << before.body.width
                              << " eligible=" << (record && record->eligible)
                              << " active=" << a.app.interactions().branch_active(input.interaction)
                              << " clip=" << clipped.x << ',' << clipped.y << ',' << clipped.width << ','
                              << clipped.height << '\n';
                }
                a.check(a.app.focus().state().focused == input.interaction, "native Table editor could not focus");
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
                a.pages.dispatch(input::TextCommitted{String{u8"native-table-draft"}, a.inputs.sessions().active()});
                break;
            }
        }
        if (!editor.valid()) {
            std::cerr << "native_table_editor: body_width=" << before.body.width << " columns=";
            for (const auto width : before.widths) {
                std::cerr << width << ',';
            }
            std::cerr << '\n';
        }
        a.check(editor.valid(), "native Table visible virtual rich editor absent");
        a.render();
        const auto body = before.body;
        a.pages.dispatch(input::ScrollInputEvent{0, -5, body.x + body.width / 2, body.y + body.height / 2});
        a.render();
        a.check(table.snapshot(id).scroll_y > 0 && a.inputs.display_snapshot(editor).text == "native-table-draft",
                "native Table wheel lost displacement or retained draft");
        const auto current = table.snapshot(id);
        const auto& thumb_node = a.app.nodes().require(a.app.components().root(current.scrollbars[1]));
        const runtime::Point thumb{thumb_node.bounds.x + thumb_node.bounds.width / 2,
                                   thumb_node.bounds.y + thumb_node.bounds.height / 2};
        a.pointer(input::PointerAction::down, thumb);
        a.pointer(input::PointerAction::move, {thumb.x, thumb.y + 30});
        a.pointer(input::PointerAction::up, {thumb.x, thumb.y + 30});
        a.render();
        const auto pointer = a.app.pointer().state(input::PointerIdentity::mouse());
        a.check(table.snapshot(id).scroll_y > current.scroll_y && (!pointer || !pointer->capture),
                "native Table scrollbar did not drag/release capture");
        run("gallery.table.rtl");
        run("gallery.table.expand");
        a.check(table.snapshot(id).projected_rows == 1001, "native Table tree expansion is a static sample");
        a.check(a.pages.sample_runs("gallery.sample.table.completion") == 1, "native Table reran its sample factory");
        std::cout << "table_completion_native=passed virtual=true retained_editor=true wheel=true scrollbar=true "
                     "rtl=true tree=true\n";
    }
    if (route.component_identity == "ant.component.tour") {
        auto& tours = a.app.services().tour();
        run("gallery.tour.open");
        const auto id = a.pages.target("gallery.tour.main");
        auto view = tours.snapshot(id);
        a.check(view.visible && view.target && !view.centered, "native Tour did not resolve a retained target");
        const auto page = view.content;
        const auto draft_owner = a.pages.target("gallery.tour.draft");
        runtime::ComponentId editor;
        for (const auto input : a.inputs.mounted_inputs()) {
            if (owns(input.component, draft_owner)) {
                editor = input.component;
                static_cast<void>(a.app.focus().request_focus(input.interaction, input::FocusModality::keyboard));
                a.check(a.app.focus().state().focused == input.interaction, "native Tour editor could not focus");
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
                a.pages.dispatch(input::TextCommitted{String{u8"native-tour-draft"}, a.inputs.sessions().active()});
                break;
            }
        }
        a.check(editor.valid(), "native Tour rich editor absent");
        a.activate(view.next);
        a.render();
        a.check(tours.snapshot(id).current == 1, "native Tour Next did not change step");
        a.activate(tours.snapshot(id).previous);
        a.render();
        a.check(tours.snapshot(id).content == page && a.inputs.display_snapshot(editor).text == "native-tour-draft",
                "native Tour navigation lost its rich editor draft");
        run("gallery.tour.refuse");
        a.activate(tours.snapshot(id).next);
        a.render();
        a.activate(tours.snapshot(id).close);
        a.render();
        a.check(tours.snapshot(id).current == 0 && tours.snapshot(id).visible,
                "native Tour overwrote a controlled refusal");
        run("gallery.tour.refuse");
        run("gallery.tour.modal");
        a.check(a.app.services().modal().visible() && !a.app.interactions().component_active(view.panel),
                "native Tour escaped child Modal confinement");
        a.press(input::Key::escape);
        a.render();
        a.activate(tours.snapshot(id).next);
        a.render();
        a.activate(tours.snapshot(id).next);
        a.render();
        a.check(tours.snapshot(id).centered && !tours.snapshot(id).target,
                "native Tour missing-target fallback failed");
        a.activate(tours.snapshot(id).next);
        a.render();
        a.check(!tours.snapshot(id).visible && a.app.interactions().input_constraint_diagnostics().active_leases == 0,
                "native Tour Finish retained input authority");
        a.check(a.pages.sample_runs("gallery.sample.tour.native") == 1, "native Tour reran its sample factory");
        std::cout << "tour_native=passed target=true rich_draft=true navigation=true refusal=true child_modal=true "
                     "missing_target=true finish_release=true\n";
    }
}
} // namespace rynui::example
