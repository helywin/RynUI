#include "component/collapse_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

struct RootsMember final {
    using Pointer = std::vector<runtime::ComponentId> InteractionRegistry::*;
    friend Pointer roots_member(RootsMember);
};

template <RootsMember::Pointer Member> struct ReadRoots final {
    friend RootsMember::Pointer roots_member(RootsMember) {
        return Member;
    }
};
template struct ReadRoots<&InteractionRegistry::blocked_roots_>;

const auto& roots(Fixture& f) {
    return f.services.interactions().*roots_member(RootsMember{});
}

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void exact(Fixture& f, std::vector<runtime::ComponentId> expected) {
    check(roots(f).size() == expected.size() &&
              std::ranges::all_of(expected, [&](auto id) { return std::ranges::find(roots(f), id) != roots(f).end(); }),
          "Collapse cleanup changed another block or retained a retired body ticket");
}

void clean(Fixture& f) {
    f.services.dispose();
    f.services.dispose();
    exact(f, {});
    check(f.services.components().component_count() == 0 && f.nodes.size() == 0 && f.scene.size() == 0 &&
              f.scene.glyph_scene().instances().size() == 0 && f.services.surfaces().size() == 0 &&
              f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid() &&
              !f.services.next_frame_deadline(),
          "Collapse block cleanup leaked components/resources/input/deadlines");
}

runtime::ComponentId region(Fixture& f) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(id, [&f, id, node] {
        f.services.interactions().set_input_blocked(id, false);
        f.layout.remove_layout(node);
    });
    return id;
}

CollapsePanel panel(String key, InputRef ref = {}) {
    return {key, CollapseTitle{[key] { Text(key); }}, CollapseBody{[ref] { Input(InputProps{}.ref(ref)); }}};
}

void partial() {
    Fixture f;
    Signal<int> topology{0};
    Signal<CollapseKeys> active{CollapseKeys{}};
    InputRef editor;
    runtime::ComponentId ancestor;
    f.services.mount(Content{[&] {
        ancestor = region(f);
        runtime::require_component_build_context().mount_slot(
            ancestor, Content{[&] {
                Collapse(CollapseProps{}.activeKeys(active).animated(false), bind([&] {
                             return topology.get() == 0 ? std::vector{panel(u8"a"), panel(u8"b", editor)}
                                                        : std::vector{panel(u8"b", editor)};
                         }));
            }});
    }});
    f.synchronize();
    const auto id = f.services.collapse().mounted().front();
    const auto old = f.services.collapse().snapshot(id).panels;
    f.services.set_input_blocked(ancestor, true);
    exact(f, {old[0].body, old[1].body, ancestor});
    topology.set(1);
    f.synchronize();
    exact(f, {old[1].body, ancestor});
    check(!f.services.components().contains(old[0].body) && !editor.focus(),
          "partial retirement opened blocked sibling");
    active.set({u8"b"});
    f.synchronize();
    exact(f, {ancestor});
    check(!editor.focus(), "expanded child escaped live blocked ancestor");
    f.services.set_input_blocked(ancestor, false);
    check(editor.focus(), "surviving panel could not recover input");
    active.set({});
    f.synchronize();
    exact(f, {old[1].body});
    check(f.services.destroy(id), "destroy collapsed owner");
    exact(f, {});
    check(f.services.components().contains(ancestor), "owner cleanup destroyed its independent ancestor");
    clean(f);
}

void content_only() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{u8"a"}};
    InputRef editor;
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}.activeKeys(active).destroyOnHidden().animated(false), {panel(u8"a", editor)});
    }});
    f.synchronize();
    const auto id = f.services.collapse().mounted().front();
    const auto body = f.services.collapse().snapshot(id).panels.front().body;
    check(editor.focus(), "destroyOnHidden editor focus");
    const auto stamp = f.inputs.sessions().active();
    static_cast<void>(f.inputs.dispatch(CompositionChanged{u8"preedit", {}, stamp}));
    active.set({});
    f.synchronize();
    exact(f, {body});
    check(f.services.components().contains(body) && !editor.bound() && !f.inputs.sessions().active().valid(),
          "content-only retirement released wrapper block or kept old editor");
    active.set({u8"a"});
    f.synchronize();
    exact(f, {});
    check(editor.bound() && editor.focus() && f.inputs.sessions().active() != stamp,
          "reopened generation did not bind");
    active.set({});
    clean(f);
}

struct Failure final {
    std::shared_ptr<int> identity;
};

void failure(int mode) {
    Fixture f;
    Signal<int> topology{0};
    runtime::ComponentId safe;
    runtime::ComponentId id;
    const auto identity = std::make_shared<int>(91);
    bool original{};
    f.services.mount(Content{[&] {
        safe = region(f);
        if (mode != 0) {
            Collapse(CollapseProps{}.animated(false), bind([&] {
                         std::vector<CollapsePanel> rows{panel(u8"closed")};
                         if (topology.get()) {
                             rows.emplace_back(u8"failure", CollapseTitle{[] { Text(u8"Failure"); }}, CollapseBody{[&] {
                                                   Input({});
                                                   if (mode == 2) {
                                                       f.services.destroy(id);
                                                   } else {
                                                       throw Failure{identity};
                                                   }
                                               }});
                         }
                         return rows;
                     }));
        }
    }});
    if (mode == 0) {
        try {
            f.services.append_slot(
                safe, Content{[&] {
                    Collapse(
                        CollapseProps{}.animated(false),
                        {panel(u8"closed"), {u8"failure", CollapseTitle{[] { Text(u8"Failure"); }}, CollapseBody{[&] {
                                                 Input({});
                                                 throw Failure{identity};
                                             }}}});
                }});
        } catch (const Failure& error) {
            original = error.identity == identity;
        }
    } else {
        id = f.services.collapse().mounted().front();
        exact(f, {f.services.collapse().snapshot(id).panels.front().body});
        try {
            topology.set(1);
        } catch (const Failure& error) {
            original = error.identity == identity;
        } catch (const std::logic_error&) {
            check(mode == 2, "unexpected factory self-destroy exception");
        }
        check(!f.services.components().contains(id), "failed Collapse generation survived");
    }
    check(mode == 2 || original, "factory failure replaced the original exception identity");
    exact(f, {});
    check(f.services.components().contains(safe) && f.inputs.mounted_inputs().empty(),
          "failure retired safe sibling or leaked input");
    clean(f);
}

void replacement() {
    Fixture f;
    runtime::ComponentId safe;
    f.services.mount(Content{[&] {
        safe = region(f);
        Collapse(CollapseProps{}.animated(false), {panel(u8"old")});
    }});
    const auto old_owner = f.services.collapse().mounted().front();
    const auto old = f.services.collapse().snapshot(old_owner).panels.front().body;
    f.services.append_slot(old, Content{[&] {
                               runtime::require_component_build_context().on_resource_cleanup(old, [&] {
                                   f.services.append_slot(
                                       safe,
                                       Content{[] { Collapse(CollapseProps{}.animated(false), {panel(u8"new")}); }});
                               });
                           }});
    f.services.destroy(old_owner);
    const auto replacement = f.services.collapse().mounted().front();
    const auto body = f.services.collapse().snapshot(replacement).panels.front().body;
    exact(f, {body});
    std::optional<runtime::ComponentId> reused;
    for (int i = 0; i < 100 && !reused; ++i) {
        f.services.append_slot(safe, Content{[&] {
                                   const auto fresh = region(f);
                                   if (fresh.index == old.index) {
                                       reused = fresh;
                                   }
                               }});
    }
    check(reused && reused->generation != old.generation, "fixture did not force exact slot index reuse");
    f.services.interactions().set_input_blocked(*reused, true);
    check(!f.services.interactions().set_input_blocked(old, false), "late old unblock changed reused generation");
    bool rejected{};
    try {
        f.services.interactions().set_input_blocked(old, true);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "late old block accepted a retired ticket");
    exact(f, {body, *reused});
    clean(f);
}

void windows() {
    Fixture first;
    Fixture second;
    for (auto* f : {&first, &second}) {
        f->services.mount(Content{[] { Collapse(CollapseProps{}.animated(false), {panel(u8"closed")}); }});
    }
    const auto a = first.services.collapse().mounted().front();
    const auto b = second.services.collapse().mounted().front();
    const auto body = first.services.collapse().snapshot(a).panels.front().body;
    check(body == second.services.collapse().snapshot(b).panels.front().body, "window fixture tickets differ");
    first.services.destroy(a);
    exact(first, {});
    exact(second, {body});
    clean(second);
    clean(first);
}

void quiet_idle() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{u8"a"}};
    InputRef ref;
    int blurs{};
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}.activeKeys(active).animated(false),
                 {{u8"a", CollapseTitle{[] { Text(u8"A"); }},
                   CollapseBody{[&] { Input(InputProps{}.ref(ref).onBlur([&] { ++blurs; })); }}}});
    }});
    f.synchronize();
    check(ref.focus(), "quiet retirement focus");
    const auto editor = f.inputs.mounted_inputs().front();
    const auto owner = f.services.collapse().mounted().front();
    f.services.focus().defer_focus(editor.interaction, FocusModality::keyboard);
    f.services.retire_failed_component(owner);
    exact(f, {});
    check(blurs == 0 && !ref.bound() && !f.inputs.sessions().active().valid(),
          "owned unblock delivered blur during failure retirement");
    f.synchronize();
    while (f.frames.consume_request()) {
    }
    const auto requests = f.frames.counters().requests;
    f.synchronize();
    check(f.frames.counters().requests == requests && !f.services.next_frame_deadline(),
          "retired Collapse requested idle frames");
    clean(f);
}
} // namespace

int main() {
    try {
        partial();
        content_only();
        for (int mode = 0; mode < 3; ++mode) {
            failure(mode);
        }
        replacement();
        windows();
        quiet_idle();
        std::cout << "Collapse exact body block cleanup: 8 fixtures passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
