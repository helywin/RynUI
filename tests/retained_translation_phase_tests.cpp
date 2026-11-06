#include "support/mentions_fixture.hpp"
#include "runtime/invalidation.hpp"
#include <functional>
using namespace ryn;
using namespace mentions_test;

namespace {
struct Owner final : detail::WindowComponentParticipant {
    detail::WindowComponentServices& services;
    runtime::ComponentId id;
    runtime::Point local;
    std::function<void()> before;
    std::function<void()> custom;
    std::vector<int>& order;
    int label{};
    bool duplicate{};

    Owner(detail::WindowComponentServices& s, std::vector<int>& o, int l) : services(s), order(o), label(l) {
        s.attach(*this);
    }

    ~Owner() {
        services.detach(*this);
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

    void synchronize_retained_translations() override {
        if (before) {
            before();
        }
        if (!services.components().contains(id)) {
            return;
        }
        const auto callback = [this] {
            order.push_back(label);
            if (custom) {
                custom();
                return;
            }
            const auto node = services.components().root(id);
            const auto inherited = services.nodes().require(node).translation;
            runtime::NodePropertyWriter writer{services.nodes(), services.dirty()};
            const auto apply = [&](auto&& self, runtime::NodeId child) -> void {
                auto& n = services.nodes().require(child);
                if (n.clip_boundary) {
                    return;
                }
                writer.set_translation(child, {inherited.x + local.x, inherited.y + local.y});
                if (n.owns_child_translation) {
                    return;
                }
                for (auto next : n.children) {
                    self(self, next);
                }
            };
            for (auto child : services.nodes().require(node).children) {
                apply(apply, child);
            }
        };
        services.enqueue_retained_translation(id, callback);
        if (duplicate) {
            services.enqueue_retained_translation(id, callback);
        }
    }
};

struct LateLayer final : detail::WindowComponentParticipant {
    detail::WindowComponentServices& services;
    runtime::ComponentId root;
    runtime::Point offset;
    std::vector<int>& order;

    LateLayer(detail::WindowComponentServices& s, std::vector<int>& o) : services(s), order(o) {
        s.attach(*this);
    }

    ~LateLayer() {
        services.detach(*this);
    }

    void position_window_layers(runtime::Size, runtime::Rect) override {
        order.push_back(0);
        if (services.components().contains(root)) {
            runtime::NodePropertyWriter writer{services.nodes(), services.dirty()};
            writer.set_translation(services.components().root(root), offset);
        }
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}
};

void nested(bool reversed) {
    Fixture f;
    std::vector<int> order;
    Owner first{f.services, order, reversed ? 2 : 1};
    Owner second{f.services, order, reversed ? 1 : 2};
    auto& outer = reversed ? second : first;
    auto& inner = reversed ? first : second;
    LateLayer layer{f.services, order};
    InputRef ref;
    runtime::ComponentId input;
    runtime::ComponentId text;
    f.services.mount(Content{[&] {
        auto& b = runtime::require_component_build_context();
        outer.id = b.mount_component<int>(0);
        const auto node = b.root(outer.id);
        f.nodes.require(node).owns_child_translation = true;
        f.layout.set_layout(node, layout::BoxLayout{});
        b.on_resource_cleanup(outer.id, [&f, node] { f.layout.remove_layout(node); });
        b.mount_slot(outer.id, Content{[&] {
                         auto& c = runtime::require_component_build_context();
                         inner.id = c.mount_component<int>(0);
                         const auto child = c.root(inner.id);
                         f.nodes.require(child).owns_child_translation = true;
                         f.layout.set_layout(child, layout::FlexLayout{.direction = layout::FlexDirection::vertical});
                         c.on_resource_cleanup(inner.id, [&f, child] { f.layout.remove_layout(child); });
                         c.mount_slot(inner.id, Content{[&] {
                                          Text(u8"phase glyph");
                                          text = f.services.text().mounted_texts().back().component;
                                          Input(InputProps{}.ref(ref).defaultValue(u8"phase input"));
                                          input = f.inputs.mounted_inputs().back().component;
                                      }});
                     }});
        Button({}, [] { Text(u8"unrelated"); });
    }});
    layer.root = outer.id;
    outer.local = {0, -10};
    inner.local = {0, -7};
    layer.offset = {20, 50};
    sync(f, 600, 400);
    check(order == std::vector<int>{0, 1, 2},
          "retained owners did not follow actual ancestry after every layer writer");
    const auto translation = f.nodes.require(f.services.components().root(text)).translation;
    check(translation == runtime::Point{20, 33} &&
              f.nodes.require(f.services.components().root(input)).translation == translation,
          "double nested local/inherited composition");
    bool glyph{};
    for (const auto& g : f.scene.glyph_scene().instances().instances()) {
        glyph = glyph || g.translation_opacity[0] == 20 && g.translation_opacity[1] == 33;
    }
    check(glyph, "owner phase ran after actual glyph synchronization");
    check(ref.focus(), "translated native Input cannot focus");
    order.clear();
    layer.offset = {35, 75};
    sync(f, 600, 400);
    check(f.nodes.require(f.services.components().root(text)).translation == runtime::Point{35, 58},
          "moved parent did not reach nested glyph/input");
    while (f.frames.consume_request()) {
    }
    const auto requests = f.frames.counters().requests;
    order.clear();
    sync(f, 600, 400);
    sync(f, 600, 400);
    check(f.frames.counters().requests == requests && !f.services.next_frame_deadline(),
          "unchanged translation phase requested idle frames");
    bool rejected{};
    try {
        f.services.enqueue_retained_translation(outer.id, [] {});
    } catch (const std::logic_error&) {
        rejected = true;
    }
    check(rejected, "enqueue outside collection accepted");
    outer.before = [&] {
        if (f.services.components().contains(inner.id)) {
            f.services.destroy(inner.id);
        }
    };
    sync(f, 600, 400);
    check(!f.services.components().contains(inner.id), "stale queued generation survived");
    clean(f);
}

void failures() {
    Fixture f;
    std::vector<int> order;
    Owner owner{f.services, order, 1};
    f.services.mount(Content{[&] {
        Text(u8"unrelated");
        auto& b = runtime::require_component_build_context();
        owner.id = b.mount_component<int>(0);
        const auto node = b.root(owner.id);
        f.layout.set_layout(node, layout::LeafLayout{});
        b.on_resource_cleanup(owner.id, [&f, node] { f.layout.remove_layout(node); });
    }});
    owner.duplicate = true;
    bool thrown{};
    try {
        sync(f);
    } catch (const std::logic_error&) {
        thrown = true;
    }
    check(thrown && order.empty(), "duplicate owner applied partial work");
    owner.duplicate = false;
    owner.custom = [] {
        throw std::runtime_error("compose failure");
    };
    thrown = false;
    try {
        sync(f);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    check(thrown && !f.services.components().contains(owner.id) && !f.services.text().mounted_texts().empty(),
          "compose failure retired wrong generation or unrelated component");
    clean(f);
}
} // namespace

int main() {
    try {
        nested(false);
        nested(true);
        failures();
        std::cout << "retained translation phase: ancestry/layers/glyph/Input/stale/duplicate/failure/idle passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
