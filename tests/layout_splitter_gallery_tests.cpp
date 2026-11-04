#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "reference_surface.hpp"
#include "component/shell_layout_component.hpp"
#include "component/splitter_component.hpp"
#include "support/input_fixture.hpp"

#include <cmath>
#include <iostream>
#include <string_view>

namespace {
using namespace ryn;
using namespace rynui::example;
using namespace ryn::input;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void sync() {
        check(buttons.layout_and_synchronize({800, 1600}, {0, 0, 800, 1600}, {}, 0, true), "Gallery layout failed");
    }

    void mount(std::string_view identity) {
        GalleryPageModel model;
        page = make_gallery_sample_page(model.find(identity)->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &inputs);
        sync();
        for (const auto& sample : gallery_samples()) {
            if (sample.component_identity == identity) {
                check(page.sample_runs(sample.identity) == 1 && gallery_sample_source(sample.identity),
                      "Gallery sample factory/source is absent");
            }
        }
    }

    void cleanup() {
        services.dispose();
        check(services.components().component_count() == 0 && services.interactions().size() == 0 &&
                  services.surfaces().size() == 0 && inputs.mounted_inputs().empty(),
              "Gallery teardown leaked resources");
    }
};

void test_layout_page() {
    Fixture f;
    f.mount("ant.component.layout");
    check(f.services.splitter().mounted().empty() && f.services.shell_layout().mounted().size() >= 12,
          "Layout page did not mount its independent real components");
    const auto count = f.services.components().component_count();
    const auto inputs = f.inputs.mounted_inputs().size();
    runtime::ComponentId responsive;
    for (auto id : f.services.shell_layout().mounted()) {
        const auto s = f.services.shell_layout().snapshot(id);
        if (s.role == ryn::detail::ShellLayoutRole::Sider && s.zero_width) {
            responsive = id;
            break;
        }
    }
    check(responsive.valid(), "Layout Gallery did not use window-responsive zero Sider");
    const auto s = f.services.shell_layout().snapshot(responsive);
    check(f.services.focus().request_focus(s.interaction, FocusModality::keyboard),
          "Gallery zero trigger cannot focus");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    f.sync();
    check(!f.services.shell_layout().snapshot(responsive).collapsed,
          "Gallery zero trigger is not wired to real collapse");
    f.page.set_width(520);
    f.sync();
    check(f.services.components().component_count() == count && f.inputs.mounted_inputs().size() == inputs,
          "Layout Gallery width update remounted content");
    f.cleanup();
}

void test_splitter_page() {
    Fixture f;
    f.mount("ant.component.splitter");
    check(f.services.shell_layout().mounted().empty() && f.services.splitter().mounted().size() == 5,
          "Splitter page did not mount multiple, controlled, lazy and nested production components");
    const auto id = f.services.splitter().mounted().front();
    const auto initial = f.services.splitter().snapshot(id);
    const auto r = f.nodes.require(f.services.components().root(initial.bars[0].component)).bounds;
    const runtime::Point p{r.x + r.width / 2, r.y + 20};
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, p.x, p.y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, p.x + 40, p.y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, p.x + 40, p.y});
    f.sync();
    const auto resized = f.services.splitter().snapshot(id);
    check(resized.sizes[0] > initial.sizes[0] && resized.sizes[2] == initial.sizes[2] &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "Gallery Splitter drag is not connected to production input");
    const auto count = f.services.components().component_count();
    f.page.set_width(520);
    f.sync();
    check(f.services.components().component_count() == count &&
              f.page.sample_runs("gallery.sample.splitter.multiple") == 1,
          "Splitter Gallery resize remounted factories");
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "layout") {
            test_layout_page();
        } else if (name == "splitter") {
            test_splitter_page();
        } else {
            throw std::runtime_error("Unknown Gallery case");
        }
        std::cout << name << " Gallery passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
