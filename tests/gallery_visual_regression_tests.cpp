#include "gallery_pages_host.hpp"
#include "component/selection_component.hpp"
#include "component/shell_layout_component.hpp"
#include "component/pagination_component.hpp"
#include "component/tabs_component.hpp"
#include "component/dropdown_component.hpp"
#include "component/form_component.hpp"
#include "component/feedback_component.hpp"
#include "graphics/color_field.hpp"
#include "renderer/common/rounded_effect_packing.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>

namespace {
using namespace ryn;
using namespace rynui::example;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GalleryPagesHost host{surfaces, inputs, frames};
    runtime::Size viewport{480, 900};

    Fixture() {
        host.mount(viewport);
        sync();
    }

    void sync() {
        check(host.synchronize(viewport) && !host.snapshot().failed, "Gallery visual fixture failed");
    }

    void show(std::string_view identity) {
        check(host.request(host.model().find(identity)->route()), "visual route rejected");
        host.process();
        sync();
    }

    bool within(runtime::ComponentId child, runtime::ComponentId owner) {
        for (auto id = std::optional{child}; id; id = services.components().parent(*id)) {
            if (*id == owner) {
                return true;
            }
        }
        return false;
    }

    runtime::Rect rect(runtime::ComponentId id) {
        const auto& node = nodes.require(services.components().root(id));
        return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
                node.bounds.height};
    }

    void reveal(runtime::ComponentId id) {
        const auto lane = host.snapshot().layout.document;
        host.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, (lane.y + 12 - rect(id).y) / 48);
        sync();
    }

    void click(runtime::ComponentId id) {
        const auto b = rect(id);
        for (const auto action : {input::PointerAction::down, input::PointerAction::up}) {
            services.pointer().dispatch({input::PointerIdentity::mouse(), action, input::PointerButton::primary,
                                         b.x + b.width / 2, b.y + b.height / 2});
        }
        sync();
    }

    std::size_t glyphs(runtime::ComponentId owner) {
        std::size_t count{};
        for (const auto& text : services.text().mounted_texts()) {
            if (within(text.component, owner)) {
                const auto range = scene.primitive(text.scene).instances;
                for (std::uint32_t i = 0; i < range.count; ++i) {
                    const auto& glyph = scene.glyph_scene().instances().at(range.first + i);
                    const auto& p = glyph.position_size;
                    const auto& c = glyph.clip_bounds;
                    const auto& t = glyph.translation_opacity;
                    const auto visible = graphics::intersect_effect_bounds({p[0] + t[0], p[1] + t[1], p[2], p[3]},
                                                                           {c[0], c[1], c[2] - c[0], c[3] - c[1]});
                    if (visible.width > 0 && visible.height > 0 && t[2] > 0 && glyph.color[3] > 0) {
                        ++count;
                    }
                }
            }
        }
        return count;
    }
};

void color_field() {
    for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        for (const float phase : {0.0F, .25F, .5F, .75F}) {
            const runtime::Rect bounds{12 + phase / scale, 16 + phase / scale, 36, 14};
            const auto effects = graphics::sampled_color_field(
                bounds, {}, 8, 4, [](float, float) { return Color{.2F, .4F, .6F, 1}; }, {1, {0, 0, 100, 80}});
            std::vector<ryn::detail::RoundedEffectGpuInstance> packed;
            for (auto effect : effects) {
                effect.material.opacity = .37F;
                packed.push_back(ryn::detail::pack_rounded_effect_instance(effect, {200, 160, scale}));
            }
            const auto sample = [&](runtime::Point point) {
                float alpha{};
                for (const auto& effect : packed) {
                    const auto pixel = ryn::detail::rounded_effect_gpu_fragment_reference(point, effect);
                    alpha = pixel[3] + alpha * (1 - pixel[3]);
                }
                check(std::abs(alpha - .37F) < .0001F, "sampled color field has an AA crack or double-alpha seam");
            };
            for (int y = int((bounds.y + 2) * scale); y < int((bounds.y + bounds.height - 2) * scale); ++y) {
                for (int x = int((bounds.x + 2) * scale); x < int((bounds.x + bounds.width - 2) * scale); ++x) {
                    sample({x + .5F, y + .5F});
                }
            }
            for (int i = 1; i < 8; ++i) {
                sample({(bounds.x + bounds.width * float(i) / 8) * scale, (bounds.y + 5) * scale});
            }
        }
    }
}

void layout_trigger() {
    Fixture f;
    f.show("ant.component.layout");
    f.reveal(f.host.target("gallery.sample.layout.responsive"));
    bool found{};
    for (const auto id : f.services.shell_layout().mounted()) {
        const auto s = f.services.shell_layout().snapshot(id);
        if (s.role != ryn::detail::ShellLayoutRole::Sider || !s.zero_width) {
            continue;
        }
        found = true;
        const auto b = f.rect(s.trigger);
        std::cout << "zero-trigger=" << b.x << ',' << b.y << ',' << b.width << ',' << b.height
                  << " glyphs=" << f.glyphs(s.trigger) << '\n';
        check(b.width > 0 && b.height > 0 && f.glyphs(s.trigger) > 0,
              "zero-width Sider lost its visible external trigger");
        const auto& trigger_node = f.nodes.require(f.services.components().root(s.trigger));
        const auto& sider_node = f.nodes.require(f.services.components().root(id));
        check(trigger_node.translation == sider_node.translation && !trigger_node.clip_boundary,
              "Sider trigger escaped its document translation domain");
        f.click(s.trigger);
        check(!f.services.shell_layout().snapshot(id).collapsed, "zero-width Sider trigger cannot expand");
        check(f.services.focus().request_focus(s.interaction, input::FocusModality::keyboard),
              "Sider trigger cannot focus");
        f.host.dispatch(input::KeyboardInputEvent{input::Key::space, input::KeyAction::down});
        f.host.dispatch(input::KeyboardInputEvent{input::Key::space, input::KeyAction::up});
        f.sync();
        check(f.services.shell_layout().snapshot(id).collapsed, "Sider keyboard trigger cannot collapse");
        const auto lane = f.host.snapshot().layout.document;
        f.host.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, -10000);
        f.sync();
        check(f.glyphs(s.trigger) == 0, "offscreen Sider trigger leaked visible glyphs");
        f.reveal(f.host.target("gallery.sample.layout.responsive"));
        check(f.glyphs(s.trigger) > 0, "Sider trigger did not recover after scroll roundtrip");
        f.viewport.width = 1440;
        f.sync();
        f.viewport.width = 480;
        f.sync();
        f.reveal(f.host.target("gallery.sample.layout.responsive"));
        check(f.services.shell_layout().snapshot(id).trigger == s.trigger && f.glyphs(s.trigger) > 0,
              "Sider resize roundtrip remounted or lost its trigger");
    }
    check(found, "responsive zero-width Sider was not exercised");
}

void pagination() {
    Fixture f;
    f.show("ant.component.pagination");
    bool found{};
    for (const auto id : f.services.pagination().mounted()) {
        const auto s = f.services.pagination().snapshot(id);
        if (s.pages != 13) {
            continue;
        }
        for (const auto& text : f.services.text().mounted_texts()) {
            if (f.within(text.component, id) &&
                f.scene.text_state(text.scene).content().bytes() == String{u8"13"}.bytes()) {
                found = true;
                check(f.rect(text.component).width >= 14 && f.scene.primitive(text.scene).instances.count == 2,
                      "compact Pagination clipped its two-digit last page");
            }
        }
    }
    check(found, "two-digit page was not exercised");
}

void tabs() {
    Fixture f;
    f.show("ant.component.tabs");
    const auto id = f.host.target("gallery.tabs.editable");
    f.reveal(id);
    const auto s = f.services.tabs().snapshot(id);
    check(f.glyphs(s.add) > 0, "Tabs add icon is blank");
    std::size_t close{};
    for (const auto& button : f.buttons.mounted_buttons()) {
        if (f.within(button.component, s.headers[1])) {
            ++close;
            check(f.glyphs(button.component) > 0, "Tabs close icon is blank");
        }
    }
    check(close > 0, "closable tab was not exercised");
    bool overflow{};
    for (const auto item : f.services.tabs().mounted()) {
        const auto snapshot = f.services.tabs().snapshot(item);
        if (snapshot.overflow) {
            overflow = true;
            f.reveal(snapshot.more);
            check(f.glyphs(snapshot.more) > 0, "Tabs overflow menu icon is blank after updating its items");
        }
    }
    check(overflow, "overflow menu was not exercised");
}

void popup() {
    Fixture f;
    f.viewport.width = 1440;
    f.show("ant.component.dropdown");
    bool found{};
    for (const auto id : f.services.dropdown().mounted()) {
        const auto s = f.services.dropdown().snapshot(id);
        bool custom{};
        for (const auto& text : f.services.text().mounted_texts()) {
            custom = custom || (f.within(text.component, s.content) &&
                                f.scene.text_state(text.scene).content().bytes() == String{u8"自定义内容"}.bytes());
        }
        if (custom) {
            found = true;
            f.reveal(s.trigger);
            f.click(s.trigger);
            check(f.rect(s.popup).width > 100 && f.rect(s.popup).width < 600,
                  "custom Dropdown stretched to the window width");
        }
    }
    check(found, "custom popup renderer was not exercised");
}

void flow() {
    Fixture f;
    f.show("ant.component.masonry");
    bool found{};
    for (const auto& text : f.services.text().mounted_texts()) {
        if (!f.scene.text_state(text.scene).content().bytes().starts_with("较高卡片")) {
            continue;
        }
        auto parent = f.services.components().parent(text.component);
        while (parent) {
            bool located{};
            for (const auto& input : f.inputs.mounted_inputs()) {
                if (f.within(input.component, *parent)) {
                    found = true;
                    located = true;
                    const auto a = f.rect(text.component);
                    check(f.rect(input.component).y >= a.y + a.height + 7.9F,
                          "Masonry Card editor overlaps its descriptive text");
                    break;
                }
            }
            if (located) {
                break;
            }
            parent = f.services.components().parent(*parent);
        }
    }
    check(found, "Masonry rich card was not exercised");
}

void disabled_input() {
    ryn_test::input_component::Fixture f;
    f.inputs.mount(
        Content{[] { Input(InputProps{}.disabled(true).status(InputStatus::Warning).defaultValue(u8"value")); }});
    check(f.buttons.layout_and_synchronize({800, 600}, {0, 0, 800, 600}), "disabled Input layout failed");
    const auto& node = f.nodes.require(f.inputs.mounted_inputs().front().node);
    const runtime::Point point{node.bounds.x + node.bounds.width / 2, node.bounds.y + node.bounds.height / 2};
    std::array<float, 3> result{1, 1, 1};
    const auto effects = f.services.surfaces().effects().packed_instances();
    check(!effects.empty(), "disabled Input has no actual container effects");
    for (const auto& effect : effects) {
        const auto packed = ryn::detail::pack_rounded_effect_instance(effect, {800, 600, 1});
        const auto pixel = ryn::detail::rounded_effect_gpu_fragment_reference(point, packed);
        for (std::size_t i = 0; i < result.size(); ++i) {
            result[i] = pixel[i] * pixel[3] + result[i] * (1 - pixel[3]);
        }
    }
    check(result[0] > .9F && result[0] < .99F && std::abs(result[0] - result[1]) < .0001F &&
              std::abs(result[1] - result[2]) < .0001F,
          "warning border filled the interior of a disabled Input");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "field";
        if (name == "field") {
            color_field();
        } else if (name == "layout") {
            layout_trigger();
        } else if (name == "pagination") {
            pagination();
        } else if (name == "tabs") {
            tabs();
        } else if (name == "popup") {
            popup();
        } else if (name == "flow") {
            flow();
        } else if (name == "input") {
            disabled_input();
        } else {
            throw std::invalid_argument("unknown visual regression case");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
