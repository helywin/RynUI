#include "component/tree_component.hpp"
#include "component/select_component.hpp"
#include "support/timeline_fixture.hpp"
#include <exception>
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.components().component_count() == 0 && f.scene.size() == 0 &&
              f.services.surfaces().size() == 0 && f.services.interactions().size() == 0 &&
              f.inputs.editors().size() == 0 && !f.inputs.sessions().active().valid() &&
              blocked_roots(f.services.interactions()).empty() && !f.services.next_frame_deadline(),
          "R2 fixture resources did not return to baseline");
}

void nested_wheel() {
    auto f = std::make_unique<Fixture>();
    Signal<bool> many{false};
    Signal<bool> child_open{false};
    Signal<bool> foreign_open{false};
    std::vector<SelectOption> options;
    for (int i = 0; i < 30; ++i) {
        options.push_back({i, String{u8"Scrollable option"}});
    }
    std::vector<TreeItem> inner_items{{u8"first", u8"Inner first"}};
    std::vector<TreeItem> siblings;
    for (int i = 0; i < 8; ++i) {
        const auto name = String::from_utf8(std::to_string(i)).value();
        inner_items.push_back(TreeItem{name, u8"Inner row"}.visible(many));
        siblings.emplace_back(name, String{u8"Independent row"});
    }
    std::vector<TreeItem> outer_items{
        {u8"prefix-1", u8"Prefix one"},
        {u8"prefix-2", u8"Prefix two"},
        TreeItem{u8"nested", u8"Nested"}.richTitle(TreeTitle{[&] {
            Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                     Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(200)).height(dp(48))), inner_items);
                     Select(SelectProps{}.options(options).open(child_open).listHeight(64));
                 }});
        }})};
    for (int i = 0; i < 8; ++i) {
        outer_items.emplace_back(String::from_utf8(std::to_string(i)).value(), String{u8"Outer row"});
    }
    f->services.mount(Content{[&] {
        Select(SelectProps{}.options(options).open(foreign_open).listHeight(64));
        Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(360)).height(dp(80))), outer_items);
        Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(360)).height(dp(64))), siblings);
    }});
    sync(*f);
    const auto ids = f->services.tree().mounted();
    check(ids.size() == 3, "expected outer, nested and independent Trees");
    const auto outer = ids[0];
    const auto inner = ids[1];
    const auto sibling = ids[2];
    const auto scroll = [&](runtime::ComponentId id) {
        return f->services.tree().snapshot(id).scroll;
    };
    const auto wheel_outer = [&](float delta) {
        const auto b = bounds(*f, outer);
        f->services.dispatch_scroll({0, delta, b.x + b.width / 2, b.y + b.height / 2});
        sync(*f);
    };
    const auto wheel_inner = [&](float delta) {
        const auto b = graphics::intersect_effect_bounds(bounds(*f, outer), bounds(*f, inner));
        check(b.width > 0 && b.height > 0, "nested wheel point must remain inside both actual viewports");
        const runtime::Point p{b.x + b.width / 2, b.y + b.height / 2};
        const auto hit = f->services.hit_test().hit_test(p);
        const auto* record = hit ? f->services.interactions().find(*hit) : nullptr;
        bool owned = false;
        for (auto owner = record ? std::optional{record->component} : std::nullopt; owner;
             owner = f->services.components().parent(*owner)) {
            owned |= *owner == inner;
        }
        check(owned, "wheel must actually hit an interaction owned by the nested Tree");
        f->services.dispatch_scroll({0, delta, p.x, p.y});
        sync(*f);
    };
    check(f->services.tree().snapshot(inner).content_height <= bounds(*f, inner).height &&
              f->services.tree().snapshot(outer).content_height > bounds(*f, outer).height,
          "no-range inner / overflowing outer fixture failed");
    wheel_inner(-1);
    std::cout << "no-range nested wheel: inner=" << scroll(inner) << " outer=" << scroll(outer)
              << " independent=" << scroll(sibling) << std::endl;
    check(scroll(inner) == 0 && scroll(outer) > 0 && scroll(sibling) == 0,
          "R2.2: no-range nested Tree failed to hand wheel to its logical Tree ancestor");
    wheel_outer(100);
    many.set(true);
    sync(*f);
    wheel_inner(-1);
    check(scroll(inner) > 0 && scroll(outer) == 0 && scroll(sibling) == 0,
          "inner Tree with range failed to consume its own wheel");
    wheel_inner(100);
    wheel_outer(-1);
    check(scroll(inner) == 0 && scroll(outer) > 0, "upper boundary fixture failed");
    wheel_inner(1);
    check(scroll(inner) == 0 && scroll(outer) == 0, "inner upper boundary failed ancestor handoff");
    wheel_inner(-100);
    const float bottom = scroll(inner);
    check(bottom > 0 && scroll(outer) == 0, "inner bottom fixture failed");
    wheel_inner(-1);
    check(scroll(inner) == bottom && scroll(outer) > 0, "inner lower boundary failed ancestor handoff");
    const float outer_before = scroll(outer);
    const auto independent_bounds = bounds(*f, sibling);
    f->services.dispatch_scroll({0, -1, independent_bounds.x + independent_bounds.width / 2,
                                 independent_bounds.y + independent_bounds.height / 2});
    sync(*f);
    check(scroll(sibling) > 0 && scroll(outer) == outer_before && scroll(inner) == bottom,
          "wheel crossed into a different Tree owner");
    const float sibling_before = scroll(sibling);
    wheel_outer(100);
    for (int foreign = 0; foreign < 2; ++foreign) {
        child_open.set(false);
        foreign_open.set(false);
        (foreign ? foreign_open : child_open).set(true);
        sync(*f);
        const auto select = f->services.select().mounted()[foreign ? 0 : 1];
        const auto popup = f->services.select().snapshot(select);
        check(popup.visible, "popup isolation fixture failed");
        const float outer_position = scroll(outer);
        const float inner_position = scroll(inner);
        f->services.dispatch_scroll({0, -1, popup.popup.x + 10, popup.popup.y + 10});
        sync(*f);
        check(f->services.select().snapshot(select).scroll > popup.scroll && scroll(outer) == outer_position &&
                  scroll(inner) == inner_position && scroll(sibling) == sibling_before,
              "nested Tree traversal crossed a child or foreign window-layer boundary");
    }
    clean(*f);
}

struct MountFailure final {
    const void* token;
};

struct BlurFailure final {};

void throwing_blur_rollback() {
    auto f = std::make_unique<Fixture>();
    InputRef ref;
    detail::MountedInputComponent old;
    Signal<int> value{0};
    int reads = 0;
    int blurs = 0;
    int clicks = 0;
    bool original_caught = false;
    bool blur_escaped = false;
    bool slot_threw = false;
    const auto original = std::make_exception_ptr(MountFailure{&clicks});
    f->services.mount(Content{[&] {
        try {
            Tree(TreeProps{}.visible(bind([&] {
                ++reads;
                return value.get() == 0;
            })),
                 {TreeItem{u8"a", u8"A"}.richTitle(TreeTitle{[&] {
                     Input(InputProps{}.ref(ref).defaultValue(u8"Draft").onBlur([&] {
                         ++blurs;
                         throw BlurFailure{};
                     }));
                     old = f->inputs.mounted_inputs().back();
                     check(ref.focus(), "throwing-blur fixture must focus Input before the slot throws");
                     check(bool(f->inputs.dispatch(
                               CompositionChanged{String{u8"preedit"}, {0, 1}, f->inputs.sessions().active()})),
                           "throwing-blur preedit fixture failed");
                     slot_threw = true;
                     std::rethrow_exception(original);
                 }})});
        } catch (const MountFailure& error) {
            original_caught = error.token == &clicks;
        } catch (const BlurFailure&) {
            blur_escaped = true;
        } catch (...) {
        }
        Button(ButtonProps{}.wave(false).onClick([&] { ++clicks; }), [] { Text(u8"Fallback"); });
    }});
    const int previous_reads = reads;
    value.set(1);
    std::cout << "throwing blur rollback: original=" << original_caught << " slot_threw=" << slot_threw
              << " blur_escaped=" << blur_escaped << " blurs=" << blurs
              << " editor_live=" << (f->inputs.editors().find(old.editor) != nullptr)
              << " interaction_live=" << (f->services.interactions().find(old.interaction) != nullptr)
              << " text_scenes=" << f->scene.size() << " surfaces=" << f->services.surfaces().size()
              << " session=" << f->inputs.sessions().active().valid() << " ref=" << ref.bound() << std::endl;
    check(original_caught && blurs == 0 && !blur_escaped && reads == previous_reads,
          "rollback replaced the original exception or retained a subscription");
    check(!f->inputs.editors().find(old.editor) && !f->services.interactions().find(old.interaction),
          "R2.1: focused Input onBlur throw interrupted failed-Tree resource retirement");
    check(!ref.bound() && !f->inputs.sessions().active().valid() && f->inputs.mounted_inputs().empty() &&
              f->services.tree().mounted().empty() && blocked_roots(f->services.interactions()).empty(),
          "failed Tree retained ref/session/owned blocks");
    sync(*f);
    click(*f, f->buttons.mounted_buttons().back().component);
    check(clicks == 1, "same-Content fallback must remain usable after failed mount");
    clean(*f);
}

void slot_retirement() {
    for (int mode = 0; mode < 6; ++mode) {
        auto f = std::make_unique<Fixture>();
        runtime::ComponentId safe;
        runtime::ComponentId foreign;
        runtime::ComponentId owner;
        detail::MountedInputComponent old;
        TextInputSessionStamp stamp;
        std::vector<runtime::NodeId> old_nodes;
        InputRef ref;
        Signal<int> signal{0};
        int reads = 0;
        int blurs = 0;
        int clicks = 0;
        bool original = false;
        const auto resources = [&] {
            return std::array{f->nodes.size(),
                              f->services.components().component_count(),
                              f->scene.size(),
                              f->services.surfaces().size(),
                              f->services.interactions().size(),
                              f->inputs.editors().size(),
                              f->scene.glyph_scene().instances().size()};
        };
        const auto replacement = [&] {
            auto& build = runtime::require_component_build_context();
            build.mount_slot(safe, Content{[&] {
                                 Tree({}, {TreeItem{u8"new", u8"New"}.richTitle(TreeTitle{
                                              [&] { Input(InputProps{}.ref(ref).defaultValue(u8"Replacement")); }})});
                             }});
        };
        const auto attempt = [&] {
            const auto baseline = resources();
            const auto theme = f->services.components().theme_scope(safe);
            const auto subscribers = theme->diagnostics().subscriber_count;
            const auto content = [&] {
                owner = f->services.tree().mounted().back();
                Input(InputProps{}.ref(ref).onBlur([&] {
                    ++blurs;
                    if (mode == 0 || mode == 5) {
                        f->services.destroy(owner);
                    } else if (mode == 1) {
                        f->services.retire_failed_component(owner);
                        replacement();
                    }
                    throw BlurFailure{};
                }));
                old = f->inputs.mounted_inputs().back();
                const auto remember = [&](auto&& self, runtime::NodeId node) -> void {
                    old_nodes.push_back(node);
                    for (auto child : f->nodes.require(node).children) {
                        self(self, child);
                    }
                };
                remember(remember, f->services.components().root(owner));
                check(ref.focus(), "slot failure must start from a focused rich Input");
                stamp = f->inputs.sessions().active();
                check(bool(f->inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 1}, stamp})),
                      "slot failure preedit fixture failed");
                if (mode == 2 || mode == 3) {
                    f->services.retire_failed_component(mode == 2 ? owner : old.component);
                    replacement();
                }
                throw MountFailure{&clicks};
            };
            try {
                auto item = TreeItem{u8"old", u8"Old"};
                if (mode == 4) {
                    item.icon(TreeIcon{content});
                } else {
                    item.richTitle(TreeTitle{content});
                }
                Tree(TreeProps{}.visible(bind([&] {
                    ++reads;
                    return signal.get() == 0;
                })),
                     {item});
            } catch (const MountFailure& error) {
                original = error.token == &clicks;
            }
            check(original && blurs == 0 && !f->inputs.editors().find(old.editor) &&
                      !f->services.interactions().find(old.interaction) && !f->nodes.find(old.node),
                  "slot-local retirement replaced the exception or delivered destructive blur");
            for (auto node : old_nodes) {
                check(!f->nodes.find(node) && !f->layout.remove_layout(node) &&
                          !f->layout.remove_intrinsic_measure(node),
                      "old generation node/layout/intrinsic survived slot-local retirement");
            }
            if (mode == 2 || mode == 3) {
                check(f->services.tree().mounted().size() == 1 && f->services.tree().mounted()[0] != owner &&
                          ref.bound() && ref.focus(),
                      "old slot guard retired a replacement Tree generation or same-ref binding");
                const auto live = f->inputs.mounted_inputs().back();
                check(!bool(f->inputs.dispatch(TextCommitted{String{u8"late"}, stamp})) &&
                          f->inputs.sessions().active().owner == live.editor &&
                          f->inputs.editors().require(live.editor).value() == "Replacement" &&
                          bool(f->inputs.dispatch(TextCommitted{String{u8"!"}, f->inputs.sessions().active()})),
                      "replacement Input is not actually usable");
                f->services.destroy(f->services.tree().mounted()[0]);
            }
            const int previous_reads = reads;
            signal.set(1);
            check(reads == previous_reads && resources() == baseline &&
                      theme->diagnostics().subscriber_count == subscribers && !ref.bound() &&
                      !f->inputs.sessions().active().valid() &&
                      !bool(f->inputs.dispatch(TextCommitted{String{u8"late"}, stamp})) &&
                      blocked_roots(f->services.interactions()) == std::vector{foreign} &&
                      !f->services.next_frame_deadline(),
                  "slot-local retirement retained resources/subscriptions/session or changed foreign blocks");
            std::cout << "slot retirement mode=" << mode << " original=" << original << " blur=" << blurs
                      << " old_editor=0 resources_at_baseline=1" << std::endl;
            Button(ButtonProps{}.wave(false).onClick([&] { ++clicks; }), [] { Text(u8"Fallback"); });
        };
        f->services.mount(Content{[&] {
            auto& build = runtime::require_component_build_context();
            safe = build.mount_component<int>(0);
            const auto node = build.root(safe);
            f->layout.set_layout(node, layout::BoxLayout{});
            build.on_resource_cleanup(safe, [&, node] { f->layout.remove_layout(node); });
            Button(ButtonProps{}.wave(false), [] { Text(u8"Foreign"); });
            foreign = f->buttons.mounted_buttons().back().component;
            f->services.set_input_blocked(foreign, true);
            if (mode != 5) {
                build.mount_slot(safe, Content{attempt});
            }
        }});
        if (mode == 5) {
            sync(*f);
            f->services.append_slot(safe, Content{attempt});
        }
        sync(*f);
        click(*f, f->buttons.mounted_buttons().back().component);
        check(clicks == 1, "same-Content fallback after slot failure is not actually clickable");
        f->services.set_input_blocked(foreign, false);
        clean(*f);
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        check(argc == 2, "expected Tree R2 case");
        const std::string_view name{argv[1]};
        if (name == "nested_wheel") {
            nested_wheel();
        } else if (name == "throwing_blur_rollback") {
            throwing_blur_rollback();
        } else if (name == "slot_retirement") {
            slot_retirement();
        } else {
            throw std::invalid_argument("unknown Tree R2 case");
        }
        std::cout << "Tree R2 " << name << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "unexpected Tree R2 exception escaped\n";
        return 1;
    }
}
