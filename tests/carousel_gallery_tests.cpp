#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"
#include "reference_surface.hpp"
#include "component/carousel_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
    try {
        using namespace ryn;
        using namespace rynui::example;
        ryn_test::input_component::Fixture f;
        ReferenceSurfaceHost surfaces{f.buttons};
        GalleryPageModel model;
        const auto entry = model.find("ant.component.carousel");
        if (!entry || !entry->native_scope()) {
            throw std::runtime_error("Carousel page missing");
        }
        auto page = make_gallery_sample_page(entry->route(), {});
        page.set_width(720);
        surfaces.mount(page.content, &f.inputs);
        const auto sync = [&] {
            if (!f.services.layout_and_synchronize({900, 2200}, {0, 0, 900, 2200}, {}, 0, true)) {
                throw std::runtime_error("Gallery layout");
            }
        };
        sync();
        if (page.sample_runs("gallery.sample.carousel.panels") != 1 ||
            page.sample_runs("gallery.sample.carousel.controlled") != 1 ||
            !gallery_sample_source("gallery.sample.carousel.panels") ||
            !gallery_sample_source("gallery.sample.carousel.controlled") || gallery_legacy_test_ids().size() != 458 ||
            f.services.carousel().mounted().size() != 2 || f.inputs.mounted_inputs().size() < 9) {
            throw std::runtime_error("Carousel real Gallery/source/legacy");
        }
        const auto first = f.services.carousel().mounted().front();
        const auto original = f.services.carousel().snapshot(first);
        const auto r = f.nodes.require(f.services.components().root(original.arrows[1])).bounds;
        const auto pointer = input::PointerIdentity::mouse();
        const auto arrow_opacity = [&] {
            float result{};
            for (const auto& effect : f.services.surfaces().effects().packed_instances()) {
                const auto shape = effect.geometry.shape.rect;
                if (effect.material.color == Color::rgba8(22, 119, 255) && shape.width < 2 && shape.x >= r.x &&
                    shape.y >= r.y && shape.x + shape.width <= r.x + r.width &&
                    shape.y + shape.height <= r.y + r.height) {
                    result = std::max(result, effect.material.opacity);
                }
            }
            return result;
        };
        if (std::abs(arrow_opacity() - .4F) > .001F) {
            throw std::runtime_error("Gallery idle arrow appearance");
        }
        f.services.pointer().dispatch(
            {pointer, input::PointerAction::move, input::PointerButton::none, r.x + r.width / 2, r.y + r.height / 2});
        sync();
        if (std::abs(arrow_opacity() - 1) > .001F) {
            throw std::runtime_error("Gallery arrow hover appearance");
        }

        f.services.pointer().dispatch({pointer, input::PointerAction::down, input::PointerButton::primary,
                                       r.x + r.width / 2, r.y + r.height / 2});
        f.services.pointer().dispatch(
            {pointer, input::PointerAction::up, input::PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
        sync();
        if (f.services.carousel().snapshot(first).current != 1 ||
            f.services.carousel().snapshot(first).slides != original.slides) {
            throw std::runtime_error("Gallery native arrow failed retained transition");
        }
        f.services.dispose();
        if (f.services.next_frame_deadline() || f.nodes.size()) {
            throw std::runtime_error("Gallery teardown");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
