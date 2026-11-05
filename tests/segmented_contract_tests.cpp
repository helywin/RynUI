#include "support/data_display_fixture.hpp"

namespace {
using namespace data_display_test;

void topology_fallback_uses_new_item_flags() {
    Fixture f;
    Signal<bool> remove{false};
    Signal<bool> disable_last{false};
    int changes{};
    f.services.mount(Content{[&] {
        Segmented(SegmentedProps{}.defaultValue(u8"a").onChange([&](String) { ++changes; }), bind([&] {
                      std::vector<SegmentedOption> options;
                      if (!remove.get()) {
                          options.emplace_back(u8"a", u8"A");
                      }
                      options.push_back(SegmentedOption{u8"b", u8"B"}.disabled(remove.get()));
                      options.push_back(SegmentedOption{u8"c", u8"C"}.disabled(disable_last));
                      return options;
                  }));
    }});
    sync(f);
    const auto id = f.services.segmented().mounted().front();
    const auto original = f.services.segmented().snapshot(id);
    remove.set(true);
    sync(f);
    const auto moved = f.services.segmented().snapshot(id);
    check(moved.selected_value == SegmentedValue{String{u8"c"}} && moved.items[1] == original.items[2] && changes == 0,
          "removed selection fallback used stale disabled flags from retained options");
    disable_last.set(true);
    sync(f);
    check(f.services.segmented().snapshot(id).selected_value == SegmentedValue{String{u8"c"}} && changes == 0,
          "merely disabling a retained selection changed its authority");
}

void typed_topology_and_rich_input() {
    Fixture f;
    Signal<int> topology{0};
    Signal<bool> disabled{false};
    Signal<bool> rtl{false};
    Signal<SegmentedValue> selected{SegmentedValue{1.0}};
    int changes{};
    int runs{};
    InputRef ref;
    const auto source = bind([&] {
        std::vector<SegmentedOption> options;
        auto draft = SegmentedOption{u8"editor", u8"Editor"}.value(1.0).content(SegmentedLabel{[&] {
            ++runs;
            Input(InputProps{}.ref(ref).defaultValue(u8"initial"));
        }});
        if (topology.get() != 2) {
            options.push_back(draft);
        }
        options.push_back(SegmentedOption{u8"second", u8"Two"}.value(2.0).disabled(disabled));
        if (topology.get() == 1) {
            std::reverse(options.begin(), options.end());
        }
        return options;
    });
    f.services.mount(Content{[&] {
        Segmented(SegmentedProps{}
                      .value(selected)
                      .rtl(rtl)
                      .block()
                      .shape(SegmentedShape::Round)
                      .onValueChange([&](SegmentedValue) { ++changes; }),
                  source);
    }});
    sync(f);
    const auto id = f.services.segmented().mounted()[0];
    const auto original = f.services.segmented().snapshot(id);
    check(original.selected_value == SegmentedValue{1.0}, "numeric value authority was stringified");
    draft(f, ref, u8"retained rich editor");
    check(changes == 0, "rich child editing selected its parent");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    check(ref.focus({InputFocusCursor::All}), "retained editor selection focus failed");
    const auto selection = f.inputs.editors().require(editor).selection();
    topology.set(1);
    sync(f);
    check(f.inputs.editors().require(editor).selection() == selection, "keyed move changed native Input selection");
    const auto moved = f.services.segmented().snapshot(id);
    check(moved.items[1] == original.items[0] && f.inputs.mounted_inputs()[0].editor == editor && runs == 1,
          "keyed options remounted rich editor");
    rtl.set(true);
    sync(f);
    check(bounds(f, moved.items[0]).x > bounds(f, moved.items[1]).x, "RTL has no measured reversal");
    click(f, moved.items[0]);
    check(changes == 1 && f.services.segmented().snapshot(id).selected_value == SegmentedValue{1.0},
          "controlled refusal changed selected value");
    disabled.set(true);
    sync(f);
    const auto count = changes;
    click(f, moved.items[0]);
    check(changes == count, "disabled option activated");
    topology.set(2);
    sync(f);
    check(f.inputs.mounted_inputs().empty() && !ref.focus() && !f.services.segmented().snapshot(id).selected,
          "removed selected item left invisible input or changed controlled authority");
}

void measured_pill_and_cancellation() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<LogicalLength> width{dp(600)};
    Signal<String> selected{String{u8"a"}};
    f.services.mount(Content{[&] {
        Segmented(SegmentedProps{}.value(selected).block().layout(LayoutStyle{}.width(width)),
                  {{u8"a", u8"One"}, {u8"b", u8"Two"}});
    }});
    sync(f);
    const auto id = f.services.segmented().mounted()[0];
    const auto initial = f.services.segmented().snapshot(id).pill;
    selected.set(u8"b");
    sync(f);
    check(f.services.animations().size() > 0, "selection has no pill motion");
    tick(f, 50000);
    const auto mid = f.services.segmented().snapshot(id).pill;
    const auto destination = bounds(f, f.services.segmented().snapshot(id).items[1]);
    check(mid.x > initial.x && mid.x < destination.x, "pill did not interpolate measured rectangles");
    selected.set(u8"a");
    sync(f);
    tick(f, 300000);
    check(f.services.segmented().snapshot(id).pill == initial && f.services.animations().size() == 0,
          "reverse failed to settle");
    selected.set(u8"b");
    sync(f);
    width.set(dp(200));
    sync(f);
    check(f.services.animations().size() == 0, "resize did not cancel pill motion");
    width.set(dp(0));
    sync(f);
    check(f.services.segmented().snapshot(id).pill.width == 0, "zero allocation retained pill paint");
    width.set(dp(600));
    sync(f);
    selected.set(u8"a");
    sync(f);
    f.services.set_window_active(false);
    sync(f);
    check(f.services.animations().size() == 0, "window inactivity retained pill motion");
    f.services.set_window_active(true);
    selected.set(u8"b");
    sync(f);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    check(f.services.animations().size() == 0, "reduced motion did not immediately cancel retained pill motion");
}

void local_revision_and_failure() {
    Fixture f;
    Signal<int> revision{0};
    int icon_runs{};
    int label_runs{};
    f.services.mount(Content{[&] {
        Segmented(SegmentedProps{}, bind([&] {
                      return std::vector<SegmentedOption>{
                          SegmentedOption{u8"a", u8"A"}
                              .icon(SegmentedIcon{[&] {
                                  ++icon_runs;
                                  Icon(IconProps{}.name(IconName::CheckOutlined));
                              }})
                              .content(SegmentedLabel{[&] {
                                  ++label_runs;
                                  if (revision.get() == 2) {
                                      throw std::runtime_error("label");
                                  }
                                  Text(u8"Label");
                              }})
                              .contentRevision(static_cast<std::uint64_t>(revision.get()))};
                  }));
    }});
    sync(f);
    revision.set(1);
    sync(f);
    check(icon_runs == 1 && label_runs == 2, "label replacement remounted icon");
    bool threw{};
    try {
        revision.set(2);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && f.services.segmented().mounted().empty() && f.services.animations().size() == 0,
          "throwing renderer left active component");
}

void self_destroying_factory() {
    Fixture f;
    Signal<bool> remove{false};
    runtime::ComponentId owner;
    const auto body = [&] {
        Input(InputProps{});
        if (remove.get()) {
            check(f.services.destroy(owner), "factory could not destroy owner");
        }
    };
    f.services.mount(Content{[&] {
        Segmented(SegmentedProps{}, bind([&] {
                      std::vector<SegmentedOption> rows{{u8"safe", u8"Safe"}};
                      if (remove.get()) {
                          rows.push_back(SegmentedOption{u8"retire", u8""}.content(SegmentedLabel{body}));
                      }
                      return rows;
                  }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    owner = f.services.segmented().mounted()[0];
    sync(f);
    try {
        remove.set(true);
    } catch (const std::exception&) {
    }
    sync(f);
    check(!f.services.components().contains(owner) && f.inputs.mounted_inputs().empty() &&
              f.buttons.mounted_buttons().size() == 1,
          "self-destroying factory left orphan input or removed sibling");
}

} // namespace

int main() {
    try {
        topology_fallback_uses_new_item_flags();
        form_composition(1);
        self_destroying_factory();
        typed_topology_and_rich_input();
        measured_pill_and_cancellation();
        local_revision_and_failure();
        std::cout << "Segmented: 4 new typed/retained/motion/failure scenarios passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
