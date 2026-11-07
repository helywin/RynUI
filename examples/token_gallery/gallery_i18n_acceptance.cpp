#include "gallery_i18n_acceptance.hpp"
#include "gallery_i18n.hpp"

#include "component/date_picker_component.hpp"
#include "component/form_component.hpp"
#include "component/input_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "component/table_component.hpp"

#include <algorithm>
#include <string>
#include <cstdlib>
#include <vector>
#include <iostream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

namespace rynui::example {
namespace {
using namespace ryn;

bool owns(ryn::detail::WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto id = std::optional{child}; id; id = services.components().parent(*id)) {
        if (*id == parent) {
            return true;
        }
    }
    return false;
}

} // namespace

void gallery_suppress_native_errors() {
#if defined(_WIN32)
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if !defined(NDEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
}

std::uint64_t run_gallery_i18n_acceptance(GalleryPagesHost& pages, ryn::detail::WindowComponentServices& services,
                                          ryn::detail::InputComponentHost& inputs, runtime::FrameRequestState& frames,
                                          const GalleryI18nAcceptanceHooks& h) {
    std::uint64_t captures{};
    runtime::ComponentId retired;
    const auto has_text = [&](runtime::ComponentId owner, String value) {
        return std::ranges::any_of(services.text().mounted_texts(), [&](const auto& item) {
            return owns(services, item.component, owner) &&
                   services.text().scene_service().text_state(item.scene).content().bytes() == value.bytes();
        });
    };
    for (const auto identity :
         {"ant.component.modal", "ant.component.select", "ant.component.date-picker", "ant.component.form",
          "ant.component.table", "ant.component.empty", "ant.component.config-provider"}) {
        std::cout << "i18n_native_case=" << identity << std::endl;
        h.check(pages.request(pages.model().find(identity)->route()), "i18n native route was rejected");
        static_cast<void>(pages.process());
        h.render();
        h.check(!services.components().contains(retired), "i18n native navigation retained a retired page");
        const auto initial = pages.snapshot();
        const auto mounts = pages.work().page_mounts;
        pages.set_locale(locales::zh_CN);
        h.render();
        h.check(has_text(initial.page, u8"示例"), "native current-page Chinese heading did not update");
        const auto chinese = h.capture(std::string{identity} + "-zh", initial.layout.content);
        ++captures;
        h.click(pages.target("gallery.shell.language"));
        h.check(pages.locale() == locales::en_US, "actual native language click did not select English");
        h.check(has_text(initial.page, u8"Examples"), "native current-page English heading did not update");
        const auto english = h.capture(std::string{identity} + "-en", pages.snapshot().layout.content);
        ++captures;
        h.check(chinese != english, "local GPU readbacks did not reflect the language change");
        h.check(pages.snapshot().page == initial.page && pages.work().page_mounts == mounts,
                "actual native language click rebuilt the current page");
        h.click(pages.target("gallery.shell.language"));
        h.check(pages.locale() == locales::zh_CN, "actual native language click did not restore Chinese");
        h.check(has_text(initial.page, u8"示例"), "native Chinese heading did not restore");

        runtime::ComponentId popup;
        if (std::string_view{identity} == "ant.component.modal") {
            const auto trigger = pages.target("gallery.modal.retained-edit");
            h.reveal(trigger);
            h.click(trigger);
            popup = pages.target("gallery.modal.retained-edit.dialog");
            h.check(services.modal().snapshot(popup).visible, "i18n native retained Modal did not open");
        } else if (std::string_view{identity} == "ant.component.select") {
            popup = pages.target("gallery.select.controlled-search");
            h.reveal(popup);
            h.click(services.select().snapshot(popup).selector);
            h.check(services.select().snapshot(popup).visible, "i18n native Select did not open");
        } else if (std::string_view{identity} == "ant.component.date-picker") {
            const auto all = services.date_picker().mounted();
            h.check(!all.empty(), "i18n native Picker is absent");
            popup = all.front();
            h.reveal(popup);
            h.click(services.date_picker().snapshot(popup).input);
            h.check(services.date_picker().snapshot(popup).visible, "i18n native Picker did not open");
        } else if (std::string_view{identity} == "ant.component.form") {
            const auto submit = pages.target("gallery.form.validate");
            h.reveal(submit);
            h.click(submit);
            h.check(!services.form().mounted().empty(), "i18n native Form path did not mount");
        } else if (std::string_view{identity} == "ant.component.table") {
            h.check(!services.table().mounted().empty(), "i18n native Table path did not mount");
            const auto action = pages.target("gallery.table.reverse");
            h.reveal(action);
            h.click(action);
        } else if (std::string_view{identity} == "ant.component.config-provider") {
            const auto inherited = pages.target("gallery.config-provider.inherited");
            const auto explicit_value = pages.target("gallery.config-provider.explicit");
            const auto nested = pages.target("gallery.config-provider.nested");
            const auto draft = pages.target("gallery.config-provider.draft");
            const auto eligible = [&](runtime::ComponentId owner) {
                for (const auto interaction : services.interactions().declaration_order()) {
                    const auto* item = services.interactions().find(interaction);
                    if (item && owns(services, item->component, owner)) {
                        return item->eligible && services.interactions().branch_active(interaction);
                    }
                }
                return false;
            };
            for (const auto action : {"gallery.config-provider.size", "gallery.config-provider.direction"}) {
                const auto target = pages.target(action);
                h.reveal(target);
                h.click(target);
            }
            const auto disable = pages.target("gallery.config-provider.disabled");
            h.reveal(disable);
            h.click(disable);
            h.check(!eligible(inherited) && !eligible(draft) && eligible(explicit_value) && eligible(nested),
                    "native Provider disabled lost own/nested false authority");
            h.click(disable);
            h.check(eligible(inherited) && eligible(draft) && pages.target("gallery.config-provider.draft") == draft,
                    "native Provider reenable lost eligibility or retained editor identity");
        }

        // Programmatic locale publication while an editor owns focus isolates
        // language preservation from the native act of focusing another button.
        // Actual language buttons were exercised above in both directions.
        std::optional<ryn::detail::MountedInputComponent> active_editor;
        const std::vector<ryn::detail::MountedInputComponent> candidates{inputs.mounted_inputs().begin(),
                                                                         inputs.mounted_inputs().end()};
        for (const auto item : candidates) {
            if (!owns(services, item.component, popup.valid() ? popup : initial.page)) {
                continue;
            }
            const auto* interaction = services.interactions().find(item.interaction);
            const auto* editor = inputs.editors().find(item.editor);
            if (!interaction || !editor || !interaction->eligible ||
                !services.interactions().branch_active(item.interaction) || editor->read_only()) {
                continue;
            }
            if (std::string_view{identity} == "ant.component.table") {
                const auto records = services.hit_test().records();
                const auto record = std::ranges::find_if(
                    records, [&](const auto& value) { return value.interaction == item.interaction; });
                if (record == records.end() || !record->effective_eligible ||
                    std::max(record->translated_bounds.x, record->effective_clip.x) >=
                        std::min(record->translated_bounds.x + record->translated_bounds.width,
                                 record->effective_clip.x + record->effective_clip.width) ||
                    std::max(record->translated_bounds.y, record->effective_clip.y) >=
                        std::min(record->translated_bounds.y + record->translated_bounds.height,
                                 record->effective_clip.y + record->effective_clip.height)) {
                    continue;
                }
            }
            h.reveal(item.component);
            if (!services.components().contains(item.component)) {
                continue;
            }
            if (owns(services, item.component, popup.valid() ? popup : initial.page) &&
                services.interactions().branch_active(item.interaction) &&
                services.focus().request_focus(item.interaction, input::FocusModality::keyboard) &&
                inputs.sessions().active().valid()) {
                active_editor = item;
                break;
            }
        }
        h.check(active_editor.has_value() || std::string_view{identity} == "ant.component.empty",
                "native i18n case did not exercise its actual editor");
        if (active_editor) {
            const auto input = *active_editor;
            const auto session = inputs.sessions().active();
            h.check(session.valid(), "i18n native editor has no session");
            pages.dispatch(input::TextCommitted{String{u8" native 中文 مرحبا"}, session});
            h.render();
            h.capture(std::string{identity} + "-mixed-editor", inputs.layout_snapshot(input.component).viewport);
            ++captures;
            auto& state = inputs.editors().require(input.editor);
            h.check(static_cast<bool>(state.select({state.value().size(), 0})), "i18n native reverse selection failed");
            pages.dispatch(input::CompositionChanged{String{u8"候选"}, {0, 1}, session});
            const auto value = std::string{state.value()};
            const auto selection = state.selection();
            const auto undo = state.history().undo_count;
            pages.set_locale(locales::en_US);
            h.render();
            h.check(state.value() == value && state.selection() == selection && state.composition().active &&
                        state.history().undo_count == undo,
                    "native language change lost draft, reverse selection, preedit or undo");
            pages.set_locale(locales::zh_CN);
            h.render();
            if (std::string_view{identity} == "ant.component.config-provider") {
                const auto config = services.components().config_scope(initial.page);
                const auto* before = &config->snapshot();
                const auto generation = config->generation();
                const auto direction = before->direction;
                bool rejected = false;
                try {
                    pages.set_direction(static_cast<TextDirection>(99));
                } catch (const I18nException& error) {
                    rejected = error.error().code == I18nErrorCode::InvalidSchema;
                }
                h.check(rejected && &config->snapshot() == before && config->generation() == generation,
                        "native rejected configuration published a partial snapshot");
                pages.set_direction(direction);
                h.render();
                h.check(state.value() == value && state.selection() == selection && state.composition().active &&
                            state.history().undo_count == undo,
                        "native configuration rejection/recovery changed the active edit");
                Scope observations;
                bool reenter = true;
                static_cast<void>(connect_binding(
                    observations, bind([config] {
                        const auto& current = config->snapshot();
                        return translate(current.catalog, current.locale.requested,
                                         gallery_message("gallery.shell.components"), {}, current.messages);
                    }),
                    [&](const String&) {
                        if (reenter && config->snapshot().locale.effective == locales::en_US) {
                            reenter = false;
                            pages.set_locale(locales::zh_CN);
                        }
                    }));
                pages.set_locale(locales::en_US);
                h.render();
                h.check(!reenter && pages.locale() == locales::zh_CN &&
                            config->snapshot().locale.effective == locales::zh_CN &&
                            pages.snapshot().page == initial.page && pages.work().page_mounts == mounts,
                        "native reentrant localization overwrote a newer retained page state");
                h.check(state.value() == value && state.selection() == selection && state.composition().active &&
                            state.history().undo_count == undo,
                        "native reentrant localization changed the active edit");
            }
        } else {
            pages.set_locale(locales::en_US);
            h.render();
            pages.set_locale(locales::zh_CN);
            h.render();
        }
        if (std::string_view{identity} == "ant.component.modal") {
            h.check(services.modal().snapshot(popup).visible, "native locale switch closed Modal");
            h.capture("modal-open-restored", services.modal().snapshot(popup).bounds);
            ++captures;
        } else if (std::string_view{identity} == "ant.component.select") {
            h.check(services.select().snapshot(popup).visible, "native locale switch closed Select");
            h.capture("select-open-restored", services.select().snapshot(popup).popup);
            ++captures;
        } else if (std::string_view{identity} == "ant.component.date-picker") {
            h.check(services.date_picker().snapshot(popup).visible, "native locale switch closed Picker");
            const auto panel = services.date_picker().snapshot(popup).popup;
            const auto& node = services.nodes().require(services.components().root(panel));
            h.capture("picker-open-restored", {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                               node.bounds.width, node.bounds.height});
            ++captures;
        }
        // Navigation itself must retire open popups and any active input session.
        retired = initial.page;
        h.check(pages.request({GalleryPageKind::introduction}), "i18n native retirement route failed");
        static_cast<void>(pages.process());
        h.render();
        h.check(!services.components().contains(retired) && !inputs.sessions().active().valid() &&
                    !services.interactions().has_input_confinement(),
                "native i18n retirement leaked input/popup");
        retired = pages.snapshot().page;
    }
    h.resize(960);
    h.render();
    h.resize(1440);
    h.render();
    pages.dispatch(input::WindowInputEvent{input::WindowInputAction::focus_lost});
    h.render();
    static_cast<void>(frames.consume_request());
    const auto requests = frames.counters().requests;
    pages.set_locale(locales::zh_CN);
    h.check(!frames.pending() && frames.counters().requests == requests && !services.next_frame_deadline(),
            "native static localization did not return to idle");
    return captures;
}

} // namespace rynui::example
