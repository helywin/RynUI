#include "gallery_pages_host.hpp"
#include "gallery_sample_registry.hpp"
#include "component/selection_component.hpp"
#include "component/select_component.hpp"
#include "component/popover_component.hpp"
#include <algorithm>
#include "support/input_fixture.hpp"

#include <iostream>
#include <cmath>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace rynui::example;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GalleryPagesHost host{surfaces, inputs, frames};
    runtime::Size viewport{1440, 900};

    Fixture() {
        host.mount(viewport);
        sync();
    }

    void sync() {
        if (!host.synchronize(viewport)) {
            throw std::runtime_error("Gallery pages sync failed");
        }
        if (host.snapshot().failed) {
            throw std::runtime_error("Gallery page factory failed");
        }
    }

    void show(std::string_view identity, GalleryInputFamily family = GalleryInputFamily::input) {
        require(host.request(host.model().find(identity)->route(family)), "page request failed");
        static_cast<void>(host.process());
        sync();
    }

    void key(input::Key value, input::KeyModifier modifier = input::KeyModifier::none) {
        host.dispatch(input::KeyboardInputEvent{value, input::KeyAction::down, modifier});
        host.dispatch(input::KeyboardInputEvent{value, input::KeyAction::up, modifier});
        sync();
    }
};

void test_pages_and_geometry() {
    Fixture f;
    require(f.host.snapshot().document.maximum_offset > 0, "Overview is not scrollable");
    f.show("ant.component.button");
    for (const auto width : {1440.0F, 960.0F, 728.0F, 480.0F, 360.0F}) {
        const auto root = f.host.snapshot().page;
        const auto mounts = f.host.work().page_mounts;
        f.viewport.width = width;
        f.sync();
        auto snapshot = f.host.snapshot();
        require(root == snapshot.page && mounts == f.host.work().page_mounts, "resize remounted page");
        const auto& bounds = f.nodes.require(f.services.components().root(f.host.target("gallery.page.usage"))).bounds;
        if (std::abs(bounds.x - snapshot.layout.content.x) > 0.2F ||
            std::abs(bounds.width - snapshot.layout.content.width) > 0.2F) {
            std::cerr << "geometry " << width << " actual " << bounds.x << ',' << bounds.width << " expected "
                      << snapshot.layout.content.x << ',' << snapshot.layout.content.width << '\n';
            throw std::runtime_error("document lanes do not match responsive metrics");
        }
        const auto card = f.services.components().root(f.host.target("gallery.sample.button.basics"));
        const auto& card_node = f.nodes.require(card);
        const auto& content_node = f.nodes.require(card_node.children.front());
        const float expected_padding = snapshot.layout.document_width < 560 ? 16 : 24;
        require(std::abs(content_node.bounds.x - card_node.bounds.x - expected_padding) < 0.1F,
                "example card spacing differs from design");
        require(snapshot.document.maximum_offset > 0, "component long page lost intrinsic height");
        require(f.host.sample_runs("gallery.sample.button.basics") == 1, "Button sample should mount once");
    }
    const auto old = f.host.snapshot().page;
    f.show("ant.component.upload");
    require(!f.services.components().contains(old), "retired page remains live");
    require(f.host.sample_runs("gallery.sample.button.basics") == 0, "unvisited sample is active");
    require(!f.host.target("gallery.page.examples").valid(), "planned page has a fake live sample");
}

void test_wheel_and_routes() {
    Fixture f;
    f.show("ant.component.input");
    const auto initial = f.host.snapshot();
    const auto lane = initial.layout.document;
    f.host.dispatch(input::ScrollInputEvent{0, -0.25F, lane.x + lane.width - 8, lane.y + 200});
    require(f.host.snapshot().document.offset == 12, "fractional wheel did not reach document");
    f.sync();
    require(f.host.work().document_wheels == 1, "real wheel route was not counted");
    auto route = f.host.model().current();
    route.section = GalleryPageSection::support;
    f.host.request(route);
    f.host.process();
    f.sync();
    require(f.host.snapshot().generation == initial.generation && f.host.snapshot().page == initial.page,
            "TOC replaced the page");
    require(f.host.snapshot().document.offset > 12, "TOC did not jump by named anchor");
    const auto saved = f.host.snapshot().document.offset;
    f.show("ant.component.upload");
    f.show("ant.component.input");
    require(std::abs(f.host.snapshot().document.offset - saved) < 1, "return did not restore old page anchor");
    f.key(input::Key::k, input::KeyModifier::control);
    require(f.inputs.sessions().active().valid(), "Ctrl+K did not focus search");
    const auto page = f.host.snapshot().page;
    f.host.dispatch(input::TextCommitted{String{u8"password"}, f.inputs.sessions().active()});
    f.sync();
    require(f.host.snapshot().page == page, "search changed active page");
    require(f.host.model().results().size() == 1, "search alias did not filter");
    f.key(input::Key::enter);
    require(f.host.model().current().family == GalleryInputFamily::password, "search Enter did not route family");
}

bool within(Fixture& f, runtime::ComponentId child, runtime::ComponentId owner) {
    for (auto id = std::optional{child}; id; id = f.services.components().parent(*id)) {
        if (*id == owner) {
            return true;
        }
    }
    return false;
}

input::InteractionId interaction(Fixture& f, runtime::ComponentId owner) {
    for (const auto id : f.services.interactions().declaration_order()) {
        const auto* record = f.services.interactions().find(id);
        if (record && record->focusable && record->eligible && within(f, record->component, owner) &&
            f.services.interactions().branch_active(id)) {
            return id;
        }
    }
    throw std::runtime_error("target has no eligible interaction");
}

void test_failed_page_viewport_and_focus() {
    ryn_test::input_component::Fixture f;
    ReferenceSurfaceHost surfaces(f.buttons);
    ryn::detail::SelectionComponentHost selections(f.services);
    const GallerySampleDescriptor invalid{.identity = "not-a-real-factory"};
    GalleryPagesHost host(surfaces, f.inputs, f.frames, {}, std::span{&invalid, 1});
    host.mount({1440, 900});
    require(host.snapshot().failed && host.synchronize({1440, 900}), "failed page has no recoverable shell");
    const auto snapshot = host.snapshot();
    require(snapshot.document.viewport_extent == snapshot.layout.document.height &&
                snapshot.document.maximum_offset == 0,
            "failure page kept a stale scroll extent");
    const auto focused = f.services.focus().state().focused;
    require(focused.has_value(), "failure recovery controls have no keyboard focus");
    auto component = std::optional{f.services.interactions().require(*focused).component};
    while (component && *component != snapshot.page) {
        component = f.services.components().parent(*component);
    }
    require(component.has_value(), "failure focus escaped recovery controls");
    static_cast<void>(f.frames.consume_request());
    require(host.synchronize({1440, 900}) && !host.pending() && !f.frames.pending(),
            "failed page is retrying in a busy loop");
}

void test_empty_overview_reflow() {
    Fixture f;
    const auto root = f.host.snapshot().page;
    const auto extent = f.host.snapshot().document.content_extent;
    f.key(input::Key::k, input::KeyModifier::control);
    f.host.dispatch(input::TextCommitted{String{u8"no-such-component"}, f.inputs.sessions().active()});
    f.sync();
    require(f.host.model().results().empty() && f.host.snapshot().page == root,
            "empty overview filtering changed page");
    require(f.host.snapshot().document.content_extent < extent - 2000,
            "hidden overview rows retain phantom layout gaps");
}

void test_header_controls() {
    Fixture f;
    const auto page = f.host.snapshot().page;
    const auto activate = [&f](std::string_view identity, input::Key key) {
        const auto target = interaction(f, f.host.target(identity));
        static_cast<void>(f.services.focus().request_focus(target, input::FocusModality::keyboard));
        require(f.services.focus().state().focused == target, "header control cannot be focused");
        f.key(key);
    };
    activate("gallery.shell.theme", input::Key::space);
    const auto dark = f.host.theme();
    require(std::ranges::find(dark.algorithms, ThemeAlgorithm::Dark) != dark.algorithms.end(),
            "header theme button did not switch Dark");
    activate("gallery.shell.compact", input::Key::space);
    const auto theme = f.host.theme();
    require(std::ranges::find(theme.algorithms, ThemeAlgorithm::Compact) != theme.algorithms.end() &&
                std::ranges::find(theme.algorithms, ThemeAlgorithm::Dark) != theme.algorithms.end(),
            "Compact lost Dark base theme");
    activate("gallery.shell.settings", input::Key::enter);
    const auto popover = f.services.popover().mounted().front();
    require(f.services.popover().snapshot(popover).geometry.visible, "header settings popover did not open");
    f.key(input::Key::escape);
    require(!f.services.popover().snapshot(popover).geometry.visible && f.host.snapshot().page == page,
            "header settings close replaced active page");
}

void test_keyboard_theme_and_boundaries() {
    Fixture f;
    f.show("ant.component.input");
    const auto page = f.host.snapshot().page;
    const auto input = *std::ranges::find_if(f.inputs.mounted_inputs(),
                                             [&f, page](const auto& item) { return within(f, item.component, page); });
    auto config = f.host.theme();
    config.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    config.seed.color_primary = Color::rgba8(114, 46, 209);
    config.seed.motion = false;
    f.host.set_theme(config);
    f.sync();
    const auto sample = f.services.components().root(f.host.target("gallery.sample.input.basics"));
    require(std::abs(f.nodes.require(f.nodes.require(sample).children.front()).bounds.x -
                     f.nodes.require(sample).bounds.x - 16) < 0.1F,
            "Compact did not update example padding in place");
    require(f.host.snapshot().page == page && f.services.components().contains(input.component),
            "theme remounted controls");
    require(f.services.focus().request_focus(input.interaction, input::FocusModality::keyboard), "editor focus failed");
    const auto session = f.inputs.sessions().active();
    f.host.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, session});
    f.key(input::Key::k, input::KeyModifier::control);
    require(f.inputs.sessions().active() == session && f.inputs.editors().require(input.editor).composition().active,
            "Ctrl+K stole active composition");
    f.host.dispatch(input::TextCommitted{String{u8"你"}, session});
    const auto before_home = f.host.snapshot().document.offset;
    f.key(input::Key::end);
    f.key(input::Key::home);
    require(f.host.snapshot().document.offset == before_home, "editor Home/End scrolled document");
    f.show("ant.component.select");
    const auto select = *std::ranges::find_if(f.services.select().mounted(),
                                              [&f](const auto id) { return within(f, id, f.host.snapshot().page); });
    const auto trigger = interaction(f, select);
    require(f.services.focus().request_focus(trigger, input::FocusModality::keyboard), "select focus failed");
    f.key(input::Key::enter);
    require(f.services.select().snapshot(select).visible, "select popup did not open");
    const auto offset = f.host.snapshot().document.offset;
    const auto popup = f.services.select().snapshot(select).popup;
    f.host.dispatch(input::ScrollInputEvent{0, -1, popup.x + popup.width - 4, popup.y + 4});
    f.sync();
    require(f.host.snapshot().document.offset == offset && f.host.work().component_wheels > 0,
            "popup boundary leaked wheel to document");
    f.key(input::Key::escape);
    f.viewport = {360, 900};
    f.sync();
    const auto generation = f.host.snapshot().generation;
    f.key(input::Key::k, input::KeyModifier::control);
    require(f.host.snapshot().directory_open && f.services.interactions().input_boundary().has_value(),
            "narrow directory has no input boundary");
    const auto search = f.inputs.sessions().active();
    require(search.valid(), "narrow directory search is not focused");
    f.key(input::Key::escape);
    require(!f.host.snapshot().directory_open && !f.services.interactions().input_boundary(),
            "Escape did not close directory");
    require(f.host.snapshot().generation == generation, "narrow directory remounted page");
    f.key(input::Key::k, input::KeyModifier::control);
    f.host.dispatch(input::TextCommitted{String{u8"upload"}, f.inputs.sessions().active()});
    f.sync();
    f.key(input::Key::enter);
    require(f.host.model().current().component_identity == "ant.component.upload" && !f.host.snapshot().directory_open,
            "narrow directory navigation did not close after search Enter");
    require(!f.inputs.editors().find(input.editor), "retired editor survived several page switches");
}

void test_focus_and_independent_scroll() {
    Fixture f;
    f.show("ant.component.button");
    const auto snapshot = f.host.snapshot();
    const auto bar = gallery_scrollbar_geometry(snapshot.layout.document_track, snapshot.document);
    static_cast<void>(f.frames.consume_request());
    f.host.dispatch(input::PointerInputEvent{input::PointerIdentity::mouse(), input::PointerAction::move,
                                             input::PointerButton::none, bar.thumb.x + 3,
                                             bar.thumb.y + bar.thumb.height / 2});
    require(f.frames.pending(), "scrollbar hover did not wake its visual frame");
    f.sync();
    require(f.surfaces.snapshot(f.host.target("gallery.shell.document.thumb")).scrollbar_visual.thumb_hover,
            "named scrollbar did not receive hover feedback");

    f.host.dispatch(
        input::ScrollInputEvent{0, -3, snapshot.layout.navigation.x + 50, snapshot.layout.navigation.y + 200});
    f.sync();
    require(f.host.snapshot().navigation.offset == 144 && f.host.snapshot().document.offset == 0,
            "navigation scroll changed document");
    auto route = f.host.model().current();
    route.section = GalleryPageSection::sources;
    f.host.request(route);
    f.host.process();
    f.sync();
    const auto entry = interaction(f, f.host.target("gallery.page.enter"));
    static_cast<void>(f.services.focus().request_focus(entry, input::FocusModality::keyboard));
    require(f.services.focus().state().focused == entry, "page entry focus failed");
    f.key(input::Key::tab, input::KeyModifier::shift);
    const auto focused = f.services.focus().state().focused;
    require(focused.has_value(), "Shift+Tab lost focus");
    const auto& record = f.services.interactions().require(*focused);
    const auto& node = f.nodes.require(record.node);
    require(node.bounds.y + node.translation.y >= snapshot.layout.document.y - 1,
            "focus reveal left keyboard focus clipped");
    f.key(input::Key::end);
    require(f.host.snapshot().document.offset <= f.host.snapshot().document.maximum_offset,
            "page keyboard offset escaped clamp");
}

void test_all_pages() {
    Fixture f;
    for (const auto& page : f.host.model().pages()) {
        f.show(page.entry->identity);
        require(f.host.target("gallery.page.support").valid(), "catalog page has no support section");
        require(f.host.target("gallery.page.sources").valid(), "catalog page has no sources section");
    }
}
} // namespace

int main() {
    try {
        test_pages_and_geometry();
        test_wheel_and_routes();
        test_failed_page_viewport_and_focus();
        test_empty_overview_reflow();
        test_header_controls();
        test_keyboard_theme_and_boundaries();
        test_focus_and_independent_scroll();
        test_all_pages();
        std::cout << "Gallery pages host checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
