#include "gallery_context_acceptance.hpp"
#include "component/decoration_component.hpp"
#include "component/modal_component.hpp"
#include "component/notice_component.hpp"
#include "component/popconfirm_component.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

namespace rynui::example {
void accept_application_context(const GalleryPageRoute& route, ExistingDataEntryActions a) {
    using namespace ryn;
    const auto name = route.component_identity;
    if (name != "ant.component.config-provider" && name != "ant.component.affix" &&
        name != "ant.component.border-beam" && name != "ant.component.app") {
        return;
    }
    auto& services = a.app.services();
    const std::unique_ptr<ThemeConfig> original_theme{new ThemeConfig(a.pages.theme())};
    const auto root = a.pages.snapshot().page;
    const auto owns = [&](runtime::ComponentId child, runtime::ComponentId owner) {
        for (auto current = std::optional{child}; current; current = a.app.components().parent(*current)) {
            if (*current == owner) {
                return true;
            }
        }
        return false;
    };
    const auto unrelated = [&](auto ids) {
        std::vector<runtime::ComponentId> result;
        for (const auto id : ids) {
            if (!owns(id, root)) {
                result.push_back(id);
            }
        }
        return result;
    };
    const auto unrelated_decorations = unrelated(services.decorations().mounted());
    const auto unrelated_notices = unrelated(services.notices().holders());
    const auto unrelated_modals = unrelated(services.modal().mounted());
    const auto unrelated_popconfirms = unrelated(services.popconfirm().mounted());
    const auto bounds = [&](runtime::ComponentId id) {
        const auto& node = a.app.nodes().require(a.app.components().root(id));
        return runtime::Rect{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
                             node.bounds.height};
    };
    const auto run = [&](std::string_view target) {
        const auto id = a.pages.target(target);
        a.reveal(id);
        a.activate(id);
        a.render();
    };
    const auto eligible = [&](runtime::ComponentId owner) {
        for (const auto id : a.app.interactions().declaration_order()) {
            const auto& item = a.app.interactions().require(id);
            if (owns(item.component, owner)) {
                return item.eligible && a.app.interactions().branch_active(id);
            }
        }
        return false;
    };
    const auto editor = [&](runtime::ComponentId owner) {
        for (const auto& item : a.inputs.mounted_inputs()) {
            if (owns(item.component, owner) && a.app.interactions().branch_active(item.interaction)) {
                return item;
            }
        }
        throw std::runtime_error("native application context editor is missing");
    };
    const auto replace = [&](runtime::ComponentId owner) {
        const auto item = editor(owner);
        if (name != "ant.component.app") {
            a.reveal(item.component);
        }
        a.check(a.app.focus().state().focused == item.interaction ||
                    a.app.focus().request_focus(item.interaction, input::FocusModality::keyboard),
                "native application context editor focus failed");
        a.pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
        a.pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
        a.pages.dispatch(input::TextCommitted{String{u8"retained context draft"}, a.inputs.sessions().active()});
        a.render();
        return item;
    };
    runtime::ComponentId draft;
    if (name == "ant.component.config-provider") {
        draft = a.pages.target("gallery.config-provider.draft");
        const auto input = replace(draft);
        const auto inherited = a.pages.target("gallery.config-provider.inherited");
        const auto explicit_value = a.pages.target("gallery.config-provider.explicit");
        const auto nested = a.pages.target("gallery.config-provider.nested");
        const auto small = bounds(inherited).height;
        const auto middle = bounds(explicit_value).height;
        a.check(small < middle, "native Provider initial size sources were collapsed");
        run("gallery.config-provider.size");
        a.check(bounds(inherited).height > small && bounds(explicit_value).height == middle,
                "native Provider size update overrode an explicit consumer");
        run("gallery.config-provider.direction");
        a.check(a.inputs.display_snapshot(input.component).text == "retained context draft" &&
                    editor(draft).editor == input.editor,
                "native Provider presentation update rebuilt the retained editor");
        run("gallery.config-provider.disabled");
        a.check(!eligible(inherited) && !eligible(draft) && eligible(explicit_value) && eligible(nested),
                "native Provider disabled lost explicit or nested false authority");
        run("gallery.config-provider.disabled");
        a.check(eligible(inherited) && eligible(draft) && editor(draft).editor == input.editor,
                "native Provider reenable lost editor identity or eligibility");
        const auto locale = a.pages.locale();
        a.pages.set_locale(locale == locales::zh_CN ? locales::en_US : locales::zh_CN);
        a.render();
        a.pages.set_locale(locale);
        a.render();
        a.check(a.pages.target("gallery.config-provider.draft") == draft &&
                    a.inputs.display_snapshot(input.component).text == "retained context draft",
                "native Provider language update lost retained consumer state");
    } else if (name == "ant.component.affix") {
        auto& host = services.decorations();
        const auto top = a.pages.target("gallery.affix.top");
        const auto bottom = a.pages.target("gallery.affix.bottom");
        const auto content = host.snapshot(top).content;
        draft = editor(content).component;
        const auto input = replace(draft);
        run("gallery.affix.target");
        run("gallery.affix.offset");
        a.check(host.snapshot(top).affixed || host.snapshot(bottom).affixed,
                "native Affix scrolling did not publish a fixed window layer");
        run("gallery.affix.disabled");
        a.check(!host.snapshot(top).affixed && !host.snapshot(bottom).affixed && !eligible(draft),
                "native disabled Affix retained a fixed layer or input");
        run("gallery.affix.disabled");
        run("gallery.affix.attach");
        a.check(!host.snapshot(top).affixed && !host.snapshot(bottom).affixed,
                "native detached Affix retained a window layer");
        run("gallery.affix.attach");
        a.check(host.snapshot(top).content == content && editor(draft).editor == input.editor &&
                    a.inputs.display_snapshot(input.component).text == "retained context draft",
                "native Affix target/release rebuilt its retained content");
    } else if (name == "ant.component.border-beam") {
        const auto theme = std::make_unique<ThemeConfig>(*original_theme);
        theme->seed.motion = true;
        a.pages.set_theme(*theme);
        a.render();
        auto& host = services.decorations();
        const auto beam = a.pages.target("gallery.border-beam.main");
        draft = a.pages.target("gallery.border-beam.note");
        const auto input = replace(draft);
        const auto content = host.snapshot(beam).content;
        services.set_motion_preference(animation::MotionPreference::normal);
        a.render();
        a.check(host.snapshot(beam).animating && host.next_auxiliary_deadline().has_value(),
                "native BorderBeam did not schedule its moving perimeter");
        const auto phase = host.snapshot(beam).phase;
        a.render();
        a.check(host.snapshot(beam).phase != phase, "native BorderBeam phase did not advance across callbacks");
        run("gallery.border-beam.direction");
        run("gallery.border-beam.duration");
        run("gallery.border-beam.theme");
        run("gallery.border-beam.motion");
        a.check(!host.snapshot(beam).animating && !host.next_auxiliary_deadline(),
                "native reduced-motion BorderBeam retained a deadline");
        run("gallery.border-beam.motion");
        a.check(host.snapshot(beam).animating, "native BorderBeam motion did not resume");
        run("gallery.border-beam.visible");
        a.check(!host.snapshot(beam).animating && !host.next_auxiliary_deadline() && !eligible(draft),
                "native hidden BorderBeam retained animation or input");
        run("gallery.border-beam.visible");
        a.check(host.snapshot(beam).content == content && editor(draft).editor == input.editor &&
                    a.inputs.display_snapshot(input.component).text == "retained context draft",
                "native BorderBeam parameter/visibility update rebuilt its draft");
    } else {
        run("gallery.app.open");
        auto& notices = services.notices();
        runtime::ComponentId notice;
        for (const auto holder : notices.holders()) {
            const auto entries = notices.entries(holder);
            if (owns(holder, root) && !entries.empty()) {
                notice = entries.front().id;
                break;
            }
        }
        a.check(notice.valid(), "native App did not mount its scoped notification");
        const auto notification = notices.snapshot(notice);
        const auto input = replace(notification.description);
        run("gallery.app.update");
        run("gallery.app.language");
        a.check(notices.snapshot(notice).status == NoticeStatus::Success &&
                    notices.snapshot(notice).description == notification.description &&
                    a.inputs.display_snapshot(input.component).text == "retained context draft",
                "native App update/language rebuilt its notification draft");
        run("gallery.app.modal");
        runtime::ComponentId modal;
        for (const auto id : services.modal().mounted()) {
            if (services.modal().snapshot(id).visible) {
                modal = id;
                break;
            }
        }
        a.check(modal.valid(), "native App did not mount its scoped Modal service");
        draft = services.modal().snapshot(modal).body;
        replace(draft);
    }
    const auto input = editor(draft);
    if (name != "ant.component.app") {
        a.reveal(input.component);
    }
    a.check(a.app.focus().state().focused == input.interaction ||
                a.app.focus().request_focus(input.interaction, input::FocusModality::keyboard),
            "native context retirement editor focus failed");
    const auto area = a.inputs.layout_snapshot(input.component).viewport;
    a.pointer(input::PointerAction::down, {area.x + 4, area.y + area.height / 2});
    a.render();
    a.pages.dispatch(input::CompositionChanged{String{u8"组合"}, {}, a.inputs.sessions().active()});
    a.render();
    const auto pointer = a.app.pointer().state(input::PointerIdentity::mouse());
    std::cout << "application_context_retirement_setup page=" << name
              << " capture=" << bool(pointer && pointer->capture)
              << " preedit=" << a.inputs.editors().require(input.editor).composition().active << " viewport=" << area.x
              << ',' << area.y << ',' << area.width << ',' << area.height << '\n';
    a.check(pointer && pointer->capture && a.inputs.editors().require(input.editor).composition().active,
            "native context retirement did not arm real capture and Core preedit");
    a.check(a.pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
            "native application context retirement route rejected");
    static_cast<void>(a.pages.process());
    a.render();
    const auto retired_pointer = a.app.pointer().state(input::PointerIdentity::mouse());
    a.check(!a.app.components().contains(root) && !a.inputs.editors().find(input.editor) &&
                !a.inputs.sessions().active().valid() && (!retired_pointer || !retired_pointer->capture),
            "native application context retirement retained editor/session/capture");
    std::cout << "application_context_retirement_remaining page=" << name
              << " decorations=" << services.decorations().mounted().size()
              << " decoration_deadline=" << bool(services.decorations().next_auxiliary_deadline())
              << " notice_holders=" << services.notices().holders().size()
              << " modals=" << services.modal().mounted().size()
              << " popconfirms=" << services.popconfirm().mounted().size() << '\n';
    a.check(std::ranges::equal(services.decorations().mounted(), unrelated_decorations) &&
                (!unrelated_decorations.empty() || !services.decorations().next_auxiliary_deadline()) &&
                std::ranges::equal(services.notices().holders(), unrelated_notices) &&
                std::ranges::equal(services.modal().mounted(), unrelated_modals) &&
                std::ranges::equal(services.popconfirm().mounted(), unrelated_popconfirms),
            "native application context retirement retained decoration/service owners or deadlines");
    a.pages.set_theme(*original_theme);
    a.render();
    std::cout << "application_context_native=passed page=" << name
              << " retained_draft=true input=true core_preedit=true capture=true retirement=true\n";
}
} // namespace rynui::example
