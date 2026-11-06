#include "gallery_status_acceptance.hpp"
#include "component/avatar_component.hpp"
#include "component/image_component.hpp"
#include "component/popover_component.hpp"
#include "component/tag_badge_component.hpp"
#include <algorithm>
#include <iostream>

namespace rynui::example {
void accept_status_data_display(const GalleryPageRoute& route, ExistingDataEntryActions a,
                                const std::function<void(int)>& resize) {
    using namespace ryn;
    if (route.component_identity != "ant.component.avatar" && route.component_identity != "ant.component.badge" &&
        route.component_identity != "ant.component.tag") {
        return;
    }
    auto motion = a.pages.theme();
    motion.seed.motion = true;
    motion.seed.motion_unit = Duration::seconds(.24F);
    a.pages.set_theme(motion);
    a.app.services().set_motion_preference(animation::MotionPreference::normal);
    a.render();
    const auto owns = [&](runtime::ComponentId child, runtime::ComponentId root) {
        for (auto current = std::optional{child}; current; current = a.app.components().parent(*current)) {
            if (*current == root) {
                return true;
            }
        }
        return false;
    };
    const auto bounds = [&](runtime::ComponentId id) {
        const auto& node = a.app.nodes().require(a.app.components().root(id));
        return runtime::Rect{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
                             node.bounds.height};
    };
    const auto center = [&](runtime::ComponentId id) {
        const auto rect = bounds(id);
        return runtime::Point{rect.x + rect.width / 2, rect.y + rect.height / 2};
    };
    const auto has_text = [&](runtime::ComponentId owner, std::string_view value) {
        return std::ranges::any_of(a.app.services().text().mounted_texts(), [&](const auto& text) {
            return owns(text.component, owner) &&
                   a.app.services().text().scene_service().text_state(text.scene).content().bytes().find(value) !=
                       std::string_view::npos;
        });
    };
    const auto run = [&](std::string_view target) {
        const auto id = a.pages.target(target);
        a.reveal(id);
        a.activate(id);
        a.render();
    };
    const auto captured = [&] {
        const auto state = a.app.pointer().state(input::PointerIdentity::mouse());
        return state && state->capture.has_value();
    };
    const auto editor = [&](runtime::ComponentId owner) {
        for (const auto& item : a.inputs.mounted_inputs()) {
            if (owns(item.component, owner) && a.app.interactions().branch_active(item.interaction)) {
                return item;
            }
        }
        throw std::runtime_error("native status sample has no active editor");
    };
    const auto focus = [&](input::InteractionId id) {
        a.check(a.app.focus().state().focused == id || a.app.focus().request_focus(id, input::FocusModality::keyboard),
                "native status editor/handle focus failed");
    };
    const auto replace = [&](runtime::ComponentId owner, String text) {
        const auto item = editor(owner);
        a.reveal(owner);
        focus(item.interaction);
        a.pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
        a.pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
        a.pages.dispatch(input::TextCommitted{std::move(text), a.inputs.sessions().active()});
        a.render();
        return item;
    };
    if (route.component_identity == "ant.component.avatar") {
        auto& avatars = a.app.services().avatar();
        auto& images = a.app.services().image();
        runtime::ComponentId avatar;
        runtime::ComponentId image;
        for (auto id : avatars.mounted()) {
            const auto child = avatars.snapshot(id).image;
            if (child.valid() && images.snapshot(child).status == ImageStatus::Loading) {
                avatar = id;
                image = child;
                break;
            }
        }
        a.check(image.valid(), "native Avatar async sample missing");
        run("gallery.avatar.image-success");
        const auto ready = images.snapshot(image);
        a.check(ready.status == ImageStatus::Ready && ready.image &&
                    a.app.services().images().find(*ready.image)->instance.pixels->bytes()[0] == std::byte{255},
                "native Avatar completion did not publish real RGBA pixels");
        a.check(!ready.modal.valid(), "Avatar preview(false) mounted a Modal");
        run("gallery.avatar.image-shape");
        const auto square = images.snapshot(image);
        a.check(square.image &&
                    a.app.services().images().find(*square.image)->instance.source_uv != runtime::Rect{0, 0, 1, 1},
                "native Square Avatar bypassed final rounded Image geometry");
        resize(640);
        a.render();
        a.check(bounds(avatar).width == 32, "native Avatar did not use the narrow logical window breakpoint");
        resize(1440);
        a.render();
        a.check(bounds(avatar).width == 80, "native Avatar responsive size did not restore");
        run("gallery.avatar.image-retry");
        run("gallery.avatar.image-error");
        a.check(images.snapshot(image).status == ImageStatus::Error && !avatars.snapshot(avatar).image_visible,
                "native Avatar async error did not reveal its retained fallback");
        run("gallery.avatar.image-retry");
        run("gallery.avatar.image-success");
        a.check(images.snapshot(image).status == ImageStatus::Ready, "native Avatar retry did not recover");
        runtime::ComponentId group;
        for (auto id : avatars.groups()) {
            if (avatars.group_snapshot(id).items.size() == 3) {
                group = id;
                break;
            }
        }
        a.check(group.valid(), "native keyed Avatar group missing");
        const auto before = avatars.group_snapshot(group);
        run("gallery.avatar.keyed-reorder");
        const auto after = avatars.group_snapshot(group);
        a.check(after.items.back().inline_avatar == before.items.front().inline_avatar && after.overflow_count == 2,
                "native Avatar reorder lost stable identity or overflow members");
        a.reveal(after.overflow);
        a.click(after.overflow);
        a.check(a.app.services().popover().snapshot(after.popover).geometry.visible,
                "native Avatar overflow did not open its real Popover");
        a.check(a.pages.sample_runs("gallery.sample.avatar.images") == 1,
                "native Avatar updates reran the sample factory");
        std::cout << "avatar_native=passed rgba=true async_error_retry=true rounded_square=true lazy_modal=true "
                     "logical_breakpoints=true keyed_overflow_popup=true\n";
    }
    if (route.component_identity == "ant.component.badge") {
        auto& host = a.app.services().tag_badge();
        runtime::ComponentId count;
        runtime::ComponentId busy;
        runtime::ComponentId ribbon;
        a.render();
        for (auto id : host.badges()) {
            const auto view = host.badge_snapshot(id);
            if (view.display_count == String{u8"8"}) {
                count = id;
            }
            if (view.status && has_text(id, "显式演示处理状态")) {
                busy = id;
            }
            if (view.ribbon) {
                ribbon = id;
            }
        }
        a.check(count.valid() && busy.valid() && ribbon.valid(), "native Badge count/Processing/Ribbon missing");
        run("gallery.badge.native-increment");
        bool midpoint = false;
        for (int frame = 0; frame < 12; ++frame) {
            const auto view = host.badge_snapshot(count);
            midpoint |= view.number_motion > 0 && view.number_motion < 1 &&
                        a.app.components().branch_active(view.previous_count);
            if (view.number_motion == 1) {
                break;
            }
            a.render();
        }
        a.check(midpoint && host.badge_snapshot(count).display_count == String{u8"9"},
                "native Badge did not present rolling digits at an intermediate frame");
        a.reveal(busy);
        const auto phase = host.badge_snapshot(busy).processing_phase;
        a.render();
        a.render();
        a.check(host.badge_snapshot(busy).processing_phase != phase, "native Processing phase did not advance");
        run("gallery.badge.processing-toggle");
        // `shown` describes retained indicator content. `visible` controls the
        // whole branch independently, including normal-flow layout participation.
        a.check(!a.app.components().branch_active(busy) &&
                    a.app.nodes().require(a.app.components().root(busy)).out_of_flow,
                "native Processing visibility did not change");
        const auto paused = host.badge_snapshot(busy).processing_phase;
        a.render();
        a.render();
        a.check(host.badge_snapshot(busy).processing_phase == paused, "hidden Processing continued animation work");
        run("gallery.badge.processing-toggle");
        a.check(a.app.components().branch_active(busy) &&
                    !a.app.nodes().require(a.app.components().root(busy)).out_of_flow &&
                    host.badge_snapshot(busy).shown,
                "native Processing did not resume");
        replace(a.pages.target("gallery.badge.arbitrary-input"), String{u8"NATIVE"});
        a.check(
            std::ranges::any_of(host.badges(),
                                [&](auto id) { return host.badge_snapshot(id).display_count == String{u8"NATIVE"}; }),
            "native Badge arbitrary count editor did not publish its real callback");
        bool activated = false;
        for (const auto& button : a.app.mounted_buttons()) {
            if (owns(button.component, ribbon)) {
                a.reveal(button.component);
                a.click(button.component);
                activated = true;
                break;
            }
        }
        a.check(activated && has_text(a.pages.snapshot().page, "Ribbon 内部独立按钮回调已执行"),
                "native Ribbon swallowed its independent retained action");
        a.check(a.pages.sample_runs("gallery.sample.badge.native") == 1, "native Badge animation rebuilt the sample");
        std::cout << "badge_native=passed rolling_midpoint=true processing_pause_resume=true arbitrary_input=true "
                     "ribbon_callback=true\n";
    }
    if (route.component_identity == "ant.component.tag") {
        auto& host = a.app.services().tag_badge();
        runtime::ComponentId closable;
        runtime::ComponentId choice;
        runtime::ComponentId group;
        for (auto id : host.tags()) {
            if (has_text(id, "可取消的关闭")) {
                closable = id;
            }
            if (has_text(id, "受控 CheckableTag")) {
                choice = id;
            }
        }
        for (auto id : host.groups()) {
            if (host.group_snapshot(id).items.size() == 3) {
                group = id;
            }
        }
        a.check(closable.valid() && choice.valid() && group.valid(), "native Tag workflow missing");
        const auto close = host.tag_snapshot(closable).close;
        a.reveal(close);
        a.click(close);
        a.check(host.tag_snapshot(closable).visible && !host.tag_snapshot(closable).closing &&
                    has_text(a.pages.snapshot().page, "关闭被应用取消"),
                "native cancellable Tag close mutated accepted state");
        run("gallery.tag.allow-close");
        a.reveal(close);
        a.click(close);
        for (int frame = 0; frame < 40 && host.tag_snapshot(closable).visible; ++frame) {
            a.render();
        }
        a.check(!host.tag_snapshot(closable).visible, "native accepted Tag close did not settle");
        a.reveal(choice);
        a.click(choice);
        a.check(host.tag_snapshot(choice).checked, "native controlled CheckableTag callback was not accepted");
        const auto original = host.group_snapshot(group);
        const auto draft_tag = original.items.front().second;
        const auto draft = replace(draft_tag, String{u8"native retained draft"});
        const auto selection = a.inputs.editors().require(draft.editor).selection();
        const auto handle_for = [&](runtime::ComponentId tag) {
            return a.app.components().children(*a.app.components().parent(tag)).front();
        };
        const auto handle = handle_for(draft_tag);
        const auto focus_handle = [&] {
            for (auto hit : a.app.interactions().declaration_order()) {
                if (a.app.interactions().require(hit).component == handle) {
                    focus(hit);
                    return;
                }
            }
            throw std::runtime_error("native Tag drag handle has no interaction");
        };
        a.reveal(group);
        focus_handle();
        a.press(input::Key::right);
        a.check(host.group_snapshot(group).items == original.items,
                "native controlled Tag reorder refusal changed accepted order");
        run("gallery.tag.allow-reorder");
        a.reveal(group);
        focus_handle();
        a.press(input::Key::right);
        a.check(host.group_snapshot(group).items[1].second == draft_tag &&
                    a.inputs.editors().require(draft.editor).value() == "native retained draft" &&
                    a.inputs.editors().require(draft.editor).selection() == selection,
                "native accepted Tag reorder lost retained draft/selection/identity");
        auto rows = host.group_snapshot(group).items;
        const auto from = center(handle_for(rows.front().second));
        const auto target = center(*a.app.components().parent(rows.back().second));
        a.pointer(input::PointerAction::down, from);
        a.render();
        a.check(captured(), "native Tag handle did not capture pointer");
        a.pointer(input::PointerAction::move, target);
        a.render();
        a.check(host.group_snapshot(group).dragging, "native Tag did not enter a real drag");
        a.pointer(input::PointerAction::up, target);
        a.render();
        a.check(!captured() && host.group_snapshot(group).items.back().second == rows.front().second,
                "native Tag drop did not resolve the third visible row");
        rows = host.group_snapshot(group).items;
        const auto row_bounds = bounds(group);
        const runtime::Point blank{row_bounds.x + row_bounds.width / 2, row_bounds.y + row_bounds.height + 10};
        a.pointer(input::PointerAction::down, center(handle_for(rows.front().second)));
        a.pointer(input::PointerAction::move, blank);
        a.render();
        a.pointer(input::PointerAction::up, blank);
        a.render();
        a.check(!captured() && host.group_snapshot(group).items == rows,
                "native blank-space Tag drop reordered items or leaked capture");
        a.reveal(draft.component);
        focus(draft.interaction);
        const auto viewport = a.inputs.layout_snapshot(draft.component).viewport;
        a.pointer(input::PointerAction::down, {viewport.x + 4, viewport.y + viewport.height / 2});
        a.render();
        const auto stamp = a.inputs.sessions().active();
        a.pages.dispatch(input::CompositionChanged{String{u8"候选"}, {}, stamp});
        a.render();
        a.check(captured() && stamp.valid() && a.inputs.editors().require(draft.editor).composition().active,
                "native Tag retirement did not arm real editor capture/preedit");
        a.check(a.pages.sample_runs("gallery.sample.tag.native") == 1, "native Tag updates reran the sample factory");
        std::cout << "tag_native=passed cancellable_close=true controlled_selection=true reorder_refuse_accept=true "
                     "retained_draft=true exact_pointer_drop=true blank_drop_rejected=true capture_preedit=armed\n";
    }
}
} // namespace rynui::example
