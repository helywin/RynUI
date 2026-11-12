#include "gallery_table_tour_acceptance.hpp"
#include "component/table_component.hpp"
#include "component/listy_component.hpp"
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
    if (route.component_identity == "ant.component.listy") {
        auto& lists = a.app.services().listy();
        const auto id = a.pages.target("gallery.listy.virtual");
        a.check(id.valid(), "native grouped Listy is absent");
        a.reveal(id);
        a.render();
        const auto before = lists.snapshot(id);
        a.check(before.items.size() == 200 && before.mounted_items < 20,
                "native Listy virtualization mounted unseen records");
        runtime::ComponentId editor;
        for (const auto& input : a.inputs.mounted_inputs()) {
            if (owns(input.component, before.items.front().component)) {
                editor = input.component;
                a.check(a.app.focus().request_focus(input.interaction, input::FocusModality::keyboard),
                        "native virtual Listy editor focus rejected");
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
                a.pages.dispatch(input::TextCommitted{String{u8"native-listy-draft"}, a.inputs.sessions().active()});
                break;
            }
        }
        a.check(editor.valid(), "native virtual Listy retained editor absent");
        a.render();
        runtime::ComponentId group_editor;
        for (const auto& input : a.inputs.mounted_inputs()) {
            if (!before.groups.empty() && owns(input.component, before.groups.front().component)) {
                group_editor = input.component;
                a.check(a.app.focus().request_focus(input.interaction, input::FocusModality::keyboard),
                        "native sticky group editor focus rejected");
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
                a.pages.dispatch(
                    input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
                a.pages.dispatch(input::TextCommitted{String{u8"native-group-draft"}, a.inputs.sessions().active()});
                break;
            }
        }
        a.check(group_editor.valid(), "native interactive sticky group editor absent");
        a.render();
        const auto& node = a.app.nodes().require(a.app.components().root(before.viewport));
        a.pages.dispatch(input::ScrollInputEvent{0, -2, node.bounds.x + node.translation.x + node.bounds.width / 2,
                                                 node.bounds.y + node.translation.y + node.bounds.height / 2});
        a.render();
        a.check(lists.snapshot(id).offset > 0, "native Listy actual wheel did not move the viewport");
        run("gallery.listy.group-jump");
        a.reveal(id);
        a.render();
        const auto distant = lists.snapshot(id);
        a.check(distant.offset > 1000 && distant.mounted_items < 40,
                "native Listy group jump eagerly mounted intervening rows");
        run("gallery.listy.group-start");
        a.reveal(id);
        a.render();
        a.check(a.inputs.display_snapshot(editor).text == "native-listy-draft" &&
                    lists.snapshot(id).items.front().component == before.items.front().component &&
                    a.inputs.display_snapshot(group_editor).text == "native-group-draft" &&
                    lists.snapshot(id).groups.front().component == before.groups.front().component,
                "native virtual Listy revisit lost row/group editor identity/draft");
        run("gallery.listy.virtual-toggle");
        a.check(lists.snapshot(id).mounted_items == 200, "native Listy disabling virtualization lost data");
        run("gallery.listy.virtual-toggle");
        const auto windowed = lists.snapshot(id);
        a.check(std::ranges::count_if(windowed.items, [](const auto& row) { return row.shown; }) < 12 &&
                    a.inputs.display_snapshot(editor).text == "native-listy-draft",
                "native Listy virtualization toggle lost the visible interval/draft");
        run("gallery.listy.sticky-toggle");
        run("gallery.listy.sticky-toggle");
        run("gallery.listy.group-reorder");
        a.check(lists.snapshot(id).items.back().component == before.items.front().component,
                "native Listy group reorder rebuilt the keyed row");
        run("gallery.listy.group-empty");
        a.check(lists.snapshot(id).items.empty() && lists.snapshot(id).groups.empty() &&
                    !a.app.components().contains(editor) && !a.app.components().contains(group_editor),
                "native Listy empty transition retained old group/editor owners");
        run("gallery.listy.group-empty");
        a.check(lists.snapshot(id).items.size() == 200 && lists.snapshot(id).mounted_items < 20,
                "native virtual Listy empty recovery failed");
        a.check(a.pages.sample_runs("gallery.sample.listy.retained") == 1, "native Listy reran its sample factory");
        a.reveal(id);
        a.render();
        const auto restored = lists.snapshot(id);
        input::TextInputSessionStamp stamp;
        for (const auto& input : a.inputs.mounted_inputs()) {
            if (owns(input.component, restored.items.front().component)) {
                a.check(a.app.focus().request_focus(input.interaction, input::FocusModality::keyboard),
                        "native Listy retirement editor focus rejected");
                const auto area = a.inputs.layout_snapshot(input.component).viewport;
                a.pointer(input::PointerAction::down, {area.x + 4, area.y + area.height / 2});
                a.render();
                stamp = a.inputs.sessions().active();
                a.pages.dispatch(input::CompositionChanged{String{u8"候选"}, {}, stamp});
                a.render();
                break;
            }
        }
        const auto capture = a.app.pointer().state(input::PointerIdentity::mouse());
        a.check(stamp.valid() && a.inputs.editors().require(stamp.owner).composition().active && capture &&
                    capture->capture.has_value(),
                "native Listy retirement did not arm real editor capture/preedit");
        a.check(a.pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native Listy retirement rejected");
        a.pages.process();
        a.render();
        const auto after_capture = a.app.pointer().state(input::PointerIdentity::mouse());
        a.check(lists.mounted().empty() && !a.app.components().contains(id) && !a.inputs.sessions().active().valid() &&
                    !a.inputs.editors().find(stamp.owner) && (!after_capture || !after_capture->capture),
                "native Listy page retirement leaked editor/capture/session");
        std::cout << "listy_native=passed groups=true measured_virtual=true actual_wheel=true retained_draft=true "
                     "sticky=true group_draft=true reorder=true empty=true preedit_capture_retired=true\n";
    }
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
