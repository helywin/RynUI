#include "component/tree_component.hpp"
#include "support/timeline_fixture.hpp"
#include <thread>
#include <map>

namespace {
using namespace timeline_test;

String str(int i) {
    return String::from_utf8(std::to_string(i)).value();
}

void pointer(Fixture& f, runtime::ComponentId child, bool control = false, bool shift = false) {
    const auto r = bounds(f, child);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2,
                                       r.y + r.height / 2, 1, shift, control});
    }
    sync(f);
}

void topology() {
    Fixture f;
    InputRef ref;
    int factories = 0;
    auto rich = TreeItem{u8"edit", u8"Editor"}.richTitle(TreeTitle{[&] {
        ++factories;
        Input(InputProps{}.ref(ref));
    }});
    Signal<std::vector<TreeItem>> items{
        std::vector<TreeItem>{TreeItem{u8"root", u8"Root", {rich, TreeItem{u8"a", u8"A"}}}}};
    f.services.mount(Content{[&] {
        Tree(TreeProps{}.items(items).multiple().checkable().defaultExpandAll().layout(
            LayoutStyle{}.width(dp(320)).height(dp(200))));
    }});
    sync(f);
    const auto id = f.services.tree().mounted().front();
    const auto before = f.services.tree().snapshot(id);
    auto editor = before.rows[1].title;
    pointer(f, before.rows[1].checkbox);
    pointer(f, before.rows[2].title);
    pointer(f, before.rows[0].title, true);
    check(f.services.tree().snapshot(id).selected.size() == 2, "Control pointer did not add selection");
    items.set({TreeItem{u8"other", u8"Other", {TreeItem{u8"a", u8"Updated"}, rich}}, TreeItem{u8"root", u8"Root"}});
    sync(f);
    auto next = f.services.tree().snapshot(id);
    check(next.rows[2].title == editor && factories == 1, "move/reorder remounted editor");
    bool rejected = false;
    try {
        items.set({rich, rich});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.components().contains(id), "invalid whole topology did not roll back");
    items.set({rich.contentRevision(1)});
    sync(f);
    check(factories == 2, "explicit factory replacement ignored");
    f.services.dispose();
    check(!ref.bound() && !f.services.next_frame_deadline(), "topology cleanup leaked ref or motion");
}

void strict_range() {
    Fixture f;
    Signal<TreeKeys> half{TreeKeys{u8"r"}};
    f.services.mount(Content{[&] {
        Tree(TreeProps{}.multiple().checkable().checkStrictly().halfCheckedKeys(half).defaultExpandAll(),
             {TreeItem{u8"r",
                       u8"Root",
                       {TreeItem{u8"a", u8"A"}, TreeItem{u8"b", u8"B"}.disableCheckbox(), TreeItem{u8"c", u8"C"}}}});
    }});
    sync(f);
    const auto id = f.services.tree().mounted().front();
    auto s = f.services.tree().snapshot(id);
    pointer(f, s.rows[1].title);
    pointer(f, s.rows[3].title, false, true);
    check(f.services.tree().snapshot(id).selected == TreeKeys{u8"a", u8"b", u8"c"},
          "Shift range omitted visible nodes");
    pointer(f, s.rows[1].checkbox);
    s = f.services.tree().snapshot(id);
    check(s.checked == TreeKeys{u8"a"} && s.half_checked == TreeKeys{u8"r"},
          "strict check conducted or lost half authority");
    pointer(f, s.rows[2].checkbox);
    check(f.services.tree().snapshot(id).checked == TreeKeys{u8"a"}, "disabled checkbox accepted input");
    pointer(f, s.rows[0].title, true);
    check(f.services.focus().request_focus(s.rows[0].interaction, FocusModality::keyboard),
          "range keyboard focus failed");
    const auto modifiers = KeyModifier::meta | KeyModifier::shift;
    f.services.focus().dispatch({Key::down, KeyAction::down, modifiers});
    f.services.focus().dispatch({Key::down, KeyAction::up, modifiers});
    sync(f);
    check(f.services.tree().snapshot(id).selected.size() == 4,
          "Meta Shift keyboard range discarded existing selection");
    f.services.dispose();
}

void virtual_rows() {
    Fixture f;
    TreeRef ref;
    std::vector<TreeItem> items;
    for (int i = 0; i < 10000; ++i) {
        items.emplace_back(str(i), String{u8"Node"});
    }
    f.services.mount(Content{[&] {
        Tree(TreeProps{}
                 .virtualized()
                 .ref(ref)
                 .itemHeight([](const String& key) { return key == u8"9999" ? 48.F : 24.F; })
                 .layout(LayoutStyle{}.width(dp(320)).height(dp(240))),
             items);
    }});
    sync(f);
    sync(f);
    const auto id = f.services.tree().mounted().front();
    auto s = f.services.tree().snapshot(id);
    check(s.rows.size() == 10000 && std::ranges::count(s.rows, true, &detail::TreeRowSnapshot::realized) <= 16,
          "10k tree did not bound ordinary realized rows");
    check(ref.scrollTo({u8"9999"}), "scrollTo rejected valid key");
    sync(f);
    s = f.services.tree().snapshot(id);
    check(s.rows.back().realized && s.rows.back().shown &&
              std::ranges::count(s.rows, true, &detail::TreeRowSnapshot::realized) <= 16,
          "scrollTo did not realize distant row");
    check(std::abs(bounds(f, s.rows.back().component).height - 48.F) < .01F, "virtual row ignored per-key height");
    for (int i = 0; i < 20; ++i) {
        const auto r = bounds(f, id);
        f.services.dispatch_scroll({0, 1, r.x + 30, r.y + 50});
        sync(f);
    }
    s = f.services.tree().snapshot(id);
    check(std::ranges::count(s.rows, true, &detail::TreeRowSnapshot::realized) <= 16,
          "continuous wheel grew row instances");
    f.services.dispose();
    check(!ref.bound(), "destroy retained TreeRef");
}

void async_nodes() {
    Fixture f;
    TreeRef ref;
    std::map<std::string, TreeLoader::Complete> completions;
    std::vector<std::function<void()>> queue;
    int cancelled = 0;
    TreeLoader source{[&](AsyncOptionsRequest request, TreeLoader::Complete done) {
                          completions.emplace(std::string{request.query.bytes()}, std::move(done));
                          return [&] {
                              ++cancelled;
                          };
                      },
                      [&](auto apply) { queue.push_back(std::move(apply)); }};
    f.services.mount(Content{
        [&] { Tree(TreeProps{}.loadData(source).ref(ref), {TreeItem{u8"a", u8"A"}, TreeItem{u8"b", u8"B"}}); }});
    sync(f);
    const auto id = f.services.tree().mounted().front();
    auto s = f.services.tree().snapshot(id);
    pointer(f, s.rows[0].switcher);
    pointer(f, s.rows[1].switcher);
    check(completions.size() == 2 && cancelled == 0, "different node expansion cancelled a valid request");
    completions.at("b")({{TreeItem{u8"b1", u8"B1"}}, {}});
    completions.at("a")({{TreeItem{u8"a1", u8"A1"}}, {}});
    auto pending = std::move(queue);
    for (auto& apply : pending) {
        apply();
    }
    sync(f);
    check(f.services.tree().snapshot(id).rows.size() == 4, "out-of-order loads lost children");
    completions.at("a")({{TreeItem{u8"duplicate", u8"Duplicate"}}, {}});
    check(queue.empty(), "duplicate completion was queued");
    ref.reset();
    f.services.dispose();
}

void drag_drop() {
    Fixture f;
    TreeDragInfo dropped;
    int drops = 0;
    int starts = 0;
    f.services.mount(Content{[&] {
        Tree(TreeProps{}
                 .draggable()
                 .onDragStart([&](auto) { ++starts; })
                 .onDrop([&](auto info) {
                     ++drops;
                     dropped = info;
                 })
                 .layout(LayoutStyle{}.width(dp(320)).height(dp(140))),
             {TreeItem{u8"a", u8"A"}, TreeItem{u8"b", u8"B"}});
    }});
    sync(f);
    const auto id = f.services.tree().mounted().front();
    const auto rows = f.services.tree().snapshot(id).rows;
    const auto a = bounds(f, rows[0].title);
    const auto b = bounds(f, rows[1].title);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, a.x + 4, a.y + 4});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, b.x + 4, b.y + b.height / 2});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, b.x + 4, b.y + b.height / 2});
    sync(f);
    check(starts == 1 && drops == 1 && dropped.key == u8"a" && dropped.target_key == u8"b" && dropped.allowed,
          "native captured drag/drop metadata missing");
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "drop retained pointer capture");
    f.services.dispose();
}

void async_lifecycle() {
    Fixture f;
    TreeRef reference;
    Signal<std::vector<TreeItem>> items{std::vector<TreeItem>{TreeItem{u8"a", u8"A"}}};
    TreeLoader::Complete finish;
    std::vector<std::function<void()>> queue;
    int loads = 0;
    std::shared_ptr<const std::atomic_bool> cancelled;
    TreeLoader source{[&](AsyncOptionsRequest request, TreeLoader::Complete done) {
                          ++loads;
                          cancelled = request.cancelled;
                          finish = std::move(done);
                          return AsyncOptionsCancel{};
                      },
                      [&](auto apply) { queue.push_back(std::move(apply)); }};
    f.services.mount(Content{[&] { Tree(TreeProps{}.items(items).loadData(source).ref(reference)); }});
    sync(f);
    const auto id = f.services.tree().mounted().front();
    pointer(f, f.services.tree().snapshot(id).rows[0].switcher);
    std::thread worker([&] { finish({{}, u8"retry"}); });
    worker.join();
    check(f.services.tree().snapshot(id).rows[0].loading, "worker bypassed owner dispatcher");
    auto jobs = std::move(queue);
    for (auto& apply : jobs) {
        apply();
    }
    sync(f);
    check(!f.services.tree().snapshot(id).rows[0].error.empty(), "async error was not retained for retry");
    pointer(f, f.services.tree().snapshot(id).rows[0].switcher);
    check(loads == 2, "error retry did not issue new ticket");
    finish({{TreeItem{u8"late", u8"Late"}}, {}});
    reference.reset();
    jobs = std::move(queue);
    for (auto& apply : jobs) {
        apply();
    }
    sync(f);
    check(f.services.tree().snapshot(id).rows.size() == 1 && cancelled->load(), "reset accepted a queued old ticket");
    pointer(f, f.services.tree().snapshot(id).rows[0].switcher);
    pointer(f, f.services.tree().snapshot(id).rows[0].switcher);
    finish({{TreeItem{u8"late", u8"Late"}}, {}});
    items.set({TreeItem{u8"other", u8"Other"}});
    jobs = std::move(queue);
    for (auto& apply : jobs) {
        apply();
    }
    sync(f);
    check(f.services.tree().snapshot(id).rows.size() == 1 && f.services.tree().snapshot(id).rows[0].key == u8"other",
          "removed node accepted an old completion");
    auto stale = finish;
    f.services.dispose();
    stale({{TreeItem{u8"stale", u8"Stale"}}, {}});
    check(queue.empty() && !reference.bound() && !f.services.next_frame_deadline(),
          "unmount retained completion or ref");
}

void drag_cancel() {
    Fixture f;
    Signal<bool> visible{true};
    Signal<std::vector<TreeItem>> items{std::vector<TreeItem>{
        TreeItem{u8"root", u8"Root", {TreeItem{u8"child", u8"Child"}}}, TreeItem{u8"blocked", u8"Blocked"}.disabled()}};
    int drops = 0;
    f.services.mount(Content{[&] {
        Tree(TreeProps{}
                 .items(items)
                 .draggable()
                 .visible(visible)
                 .defaultExpandAll()
                 .onDrop([&](auto) { ++drops; })
                 .layout(LayoutStyle{}.width(dp(320)).height(dp(160))));
    }});
    sync(f);
    const auto id = f.services.tree().mounted().front();
    auto rows = f.services.tree().snapshot(id).rows;
    const auto start = bounds(f, rows[0].title);
    const auto child = bounds(f, rows[1].title);
    const auto down = [&] {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, start.x + 4, start.y + 4});
    };
    const auto move = [&](runtime::Rect target) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, target.x + 4,
                                       target.y + target.height / 2});
    };
    down();
    move(child);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, child.x + 4, child.y + child.height / 2});
    check(drops == 0, "cycle drop accepted a descendant target");
    down();
    move(bounds(f, rows[2].title));
    visible.set(false);
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture && drops == 0,
          "hidden Tree retained drag capture");
    visible.set(true);
    sync(f);
    down();
    move(child);
    items.set({TreeItem{u8"replacement", u8"Replacement"}});
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture && drops == 0,
          "topology update retained stale drag capture");
    f.services.dispose();
}

void async_failures() {
    Fixture f;
    TreeRef reference;
    TreeLoader::Complete done;
    bool throwing = true;
    int healthy = 0;
    TreeLoader source{[&](AsyncOptionsRequest, TreeLoader::Complete complete) {
                          if (throwing) {
                              throw std::runtime_error("provider");
                          }
                          done = std::move(complete);
                          return AsyncOptionsCancel{};
                      },
                      [](auto apply) { apply(); }};
    f.services.mount(Content{[&] {
        Tree(TreeProps{}.loadData(source).ref(reference), {TreeItem{u8"a", u8"A"}});
        Button(ButtonProps{}.onClick([&] { ++healthy; }), [] { Text(u8"Healthy"); });
    }});
    sync(f);
    const auto id = f.services.tree().mounted().front();
    bool threw = false;
    try {
        pointer(f, f.services.tree().snapshot(id).rows[0].switcher);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !f.services.tree().snapshot(id).rows[0].loading &&
              !f.services.tree().snapshot(id).rows[0].error.empty(),
          "provider throw retained loading or lost retry state");
    throwing = false;
    pointer(f, f.services.tree().snapshot(id).rows[0].switcher);
    threw = false;
    try {
        done({{TreeItem{u8"a", u8"Duplicate"}}, {}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw && f.services.tree().snapshot(id).rows.size() == 1, "invalid async topology was partially published");
    pointer(f, f.services.tree().snapshot(id).rows[0].switcher);
    std::thread foreign([&] { done({{TreeItem{u8"foreign", u8"Foreign"}}, {}}); });
    foreign.join();
    check(f.services.tree().snapshot(id).rows.size() == 1, "wrong owner dispatcher published foreign-thread data");
    reference.reset();
    click(f, f.buttons.mounted_buttons().front().component);
    check(healthy == 1, "failed async Tree damaged unrelated component");
    f.services.dispose();

    Fixture retired;
    int cancellations = 0;
    TreeLoader::Complete stale;
    TreeLoader retire_source{[&](AsyncOptionsRequest, TreeLoader::Complete complete) {
                                 stale = std::move(complete);
                                 retired.services.destroy(retired.services.tree().mounted().front());
                                 return [&] {
                                     ++cancellations;
                                 };
                             },
                             [](auto apply) { apply(); }};
    retired.services.mount(Content{[&] { Tree(TreeProps{}.loadData(retire_source), {TreeItem{u8"a", u8"A"}}); }});
    sync(retired);
    const auto owner = retired.services.tree().mounted().front();
    pointer(retired, retired.services.tree().snapshot(owner).rows[0].switcher);
    stale({{TreeItem{u8"stale", u8"Stale"}}, {}});
    check(retired.services.tree().mounted().empty() && cancellations == 1,
          "provider self-destruction retained owner or cancel hook");
    retired.services.dispose();
}

void virtual_drag_autoscroll() {
    Fixture f;
    std::vector<TreeItem> items;
    int drops = 0;
    for (int i = 0; i < 10000; ++i) {
        items.emplace_back(str(i), String{u8"Drag row"});
    }
    f.services.mount(Content{[&] {
        Tree(TreeProps{}
                 .virtualized()
                 .draggable()
                 .onDrop([&](auto) { ++drops; })
                 .layout(LayoutStyle{}.width(dp(320)).height(dp(120))),
             items);
    }});
    sync(f);
    sync(f);
    const auto id = f.services.tree().mounted().front();
    const auto source = bounds(f, f.services.tree().snapshot(id).rows[0].title);
    const auto viewport = bounds(f, id);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, source.x + 4, source.y + 4});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, source.x + 4,
                                   viewport.y + viewport.height - 4});
    sync(f);
    const auto before = f.services.tree().snapshot(id).scroll;
    for (int i = 1; i <= 20; ++i) {
        const auto time = ryn::animation::AnimationTime::microseconds(i * 16000);
        f.services.set_animation_time(time);
        f.services.tree().tick_auxiliary(time);
        sync(f);
    }
    auto tree = f.services.tree().snapshot(id);
    const auto capture = f.services.pointer().state(PointerIdentity::mouse())->capture;
    check(tree.scroll > before + 120 && capture && !tree.rows[0].realized &&
              std::ranges::count(tree.rows, true, &detail::TreeRowSnapshot::realized) <= 12,
          "stationary edge drag did not scroll with bounded virtual rows");
    check(std::ranges::none_of(tree.rows, [&](const auto& row) { return row.interaction == capture; }),
          "virtual drag captured an invisible row");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, source.x + 4,
                                   viewport.y + viewport.height - 4});
    sync(f);
    check(drops == 1 && !f.services.pointer().state(PointerIdentity::mouse())->capture &&
              !f.services.next_frame_deadline(),
          "virtual edge drop retained capture/deadline or lost callback");
    f.services.dispose();
}

void large_checks() {
    Fixture f;
    std::vector<TreeItem> children;
    for (int i = 0; i < 10000; ++i) {
        children.emplace_back(str(i), String{u8"Check child"});
    }
    f.services.mount(Content{[&] {
        Tree(TreeProps{}.checkable().virtualized().defaultExpandAll().layout(
                 LayoutStyle{}.width(dp(320)).height(dp(120))),
             {TreeItem{u8"root", u8"Root", children}});
    }});
    sync(f);
    sync(f);
    const auto id = f.services.tree().mounted().front();
    auto root = f.services.tree().snapshot(id).rows[0];
    pointer(f, root.checkbox);
    check(f.services.tree().snapshot(id).checked.size() == 10001, "large cascade truncated checked output");
    pointer(f, root.checkbox);
    check(f.services.tree().snapshot(id).checked.empty(), "large cascade deselect retained aggregate keys");
    f.services.dispose();
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "topology";
        if (name == "topology") {
            topology();
        } else if (name == "strict_range") {
            strict_range();
        } else if (name == "virtual") {
            virtual_rows();
        } else if (name == "async") {
            async_nodes();
        } else if (name == "async_lifecycle") {
            async_lifecycle();
        } else if (name == "drag_cancel") {
            drag_cancel();
        } else if (name == "async_failures") {
            async_failures();
        } else if (name == "virtual_drag") {
            virtual_drag_autoscroll();
        } else if (name == "large_checks") {
            large_checks();
        } else if (name == "drag") {
            drag_drop();
        } else {
            return 2;
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
