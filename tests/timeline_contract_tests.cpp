#include "support/data_display_fixture.hpp"

namespace {
using namespace data_display_test;

void horizontal_retained_and_titles() {
    Fixture f;
    Signal<int> topology{0};
    Signal<int> revision{0};
    Signal<TimelineOrientation> orientation{TimelineOrientation::Horizontal};
    Signal<bool> rtl{false};
    Signal<TimelineTitleSpan> span{TimelineTitleSpan::fraction(0.25F)};
    Signal<LogicalLength> width{dp(600)};
    InputRef ref;
    int title_runs{};
    int content_runs{};
    f.services.mount(Content{[&] {
        Timeline(TimelineProps{}.orientation(orientation).rtl(rtl).titleSpan(span).layout(LayoutStyle{}.width(width)),
                 bind([&] {
                     auto draft = TimelineItem{u8"draft", u8""}
                                      .richTitle(TimelineTitle{[&] {
                                          ++title_runs;
                                          Text(u8"Title");
                                      }})
                                      .richContent(TimelineContent{[&] {
                                          ++content_runs;
                                          Input(InputProps{}.ref(ref));
                                      }})
                                      .titleRevision(static_cast<std::uint64_t>(revision.get()));
                     std::vector<TimelineItem> items;
                     if (topology.get() == 1) {
                         items.push_back(TimelineItem{u8"new", u8"New"}.title(u8"10:00"));
                     }
                     items.push_back(draft);
                     items.push_back(TimelineItem{u8"done", u8"Complete"}.tone(TimelineTone::Green));
                     return items;
                 }));
    }});
    sync(f);
    const auto id = f.services.timeline().mounted()[0];
    const auto original = f.services.timeline().snapshot(id);
    check(bounds(f, original.items[0].viewport).x < bounds(f, original.items[1].viewport).x &&
              original.items[0].tail.width > original.items[0].tail.height && original.items[1].tail.width == 0,
          "horizontal Timeline has vertical or trailing rail");
    draft(f, ref, u8"timeline draft");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    check(ref.focus({InputFocusCursor::All}), "retained editor selection focus failed");
    const auto selection = f.inputs.editors().require(editor).selection();
    topology.set(1);
    sync(f);
    check(f.inputs.editors().require(editor).selection() == selection, "keyed move changed native Input selection");
    auto current = f.services.timeline().snapshot(id);
    check(current.items[1].content == original.items[0].content && f.inputs.mounted_inputs()[0].editor == editor,
          "dynamic Timeline remounted retained draft");
    rtl.set(true);
    sync(f);
    current = f.services.timeline().snapshot(id);
    check(bounds(f, current.items[0].viewport).x > bounds(f, current.items[1].viewport).x &&
              current.items[0].tail.width > 0,
          "horizontal RTL failed to reverse item geometry and rail");
    revision.set(1);
    sync(f);
    check(title_runs == 2 && content_runs == 1, "title replacement rebuilt description");
    orientation.set(TimelineOrientation::Vertical);
    rtl.set(false);
    sync(f);
    current = f.services.timeline().snapshot(id);
    const auto narrow_title = bounds(f, current.items[1].title).width;
    span.set(TimelineTitleSpan::logicalWidth(240));
    sync(f);
    check(bounds(f, current.items[1].title).width > narrow_title && content_runs == 1,
          "titleSpan did not change measured allocation");
    for (float extent : {0.0F, 2.0F, 80.0F, 600.0F}) {
        width.set(dp(extent));
        sync(f);
    }
    check(f.inputs.mounted_inputs()[0].editor == editor, "narrow/zero layout lost Input identity");
}

void loading_and_cleanup() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> loading{true};
    Signal<bool> visible{true};
    f.services.mount(Content{[&] {
        Timeline(TimelineProps{}
                     .orientation(TimelineOrientation::Horizontal)
                     .visible(visible)
                     .variant(TimelineVariant::Filled),
                 {TimelineItem{u8"pending", u8"Pending"}.loading(loading).color(Color::rgba8(120, 80, 180)),
                  TimelineItem{u8"done", u8"Done"}});
    }});
    sync(f);
    check(f.services.animations().size() > 0, "visible loading icon has no real spin");
    visible.set(false);
    sync(f);
    check(f.services.animations().size() == 0, "hidden Timeline retained spin");
    visible.set(true);
    loading.set(false);
    sync(f);
    check(f.services.animations().size() == 0, "completed Timeline kept active animation");
    const auto count = f.services.frame_request_revision();
    sync(f);
    sync(f);
    check(count == f.services.frame_request_revision(), "idle Timeline requested constant frames");
}

void failed_factory_retires_owner() {
    Fixture f;
    Signal<bool> fail{false};
    f.services.mount(Content{[&] {
        Timeline(TimelineProps{}, bind([&] {
                     std::vector<TimelineItem> items{{u8"a", u8"A"}};
                     if (fail.get()) {
                         items.push_back(TimelineItem{u8"bad", u8""}.richContent(TimelineContent{[] {
                             Input(InputProps{});
                             throw std::runtime_error("description");
                         }}));
                     }
                     return items;
                 }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    sync(f);
    bool threw{};
    try {
        fail.set(true);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && f.services.timeline().mounted().empty() && f.inputs.mounted_inputs().empty() &&
              f.buttons.mounted_buttons().size() == 1,
          "throwing description orphaned native regions");
}

void finite_palette_motion() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<TimelineTone> tone{TimelineTone::Blue};
    f.services.mount(
        Content{[&] { Timeline(TimelineProps{}, {TimelineItem{u8"a", u8"Color transition"}.tone(tone)}); }});
    sync(f);
    const auto id = f.services.timeline().mounted()[0];
    const auto snapshot = f.services.timeline().snapshot(id);
    const auto root = bounds(f, id);
    const runtime::Rect dot{root.x + snapshot.items[0].dot.x, root.y + snapshot.items[0].dot.y,
                            snapshot.items[0].dot.width, snapshot.items[0].dot.height};
    const auto painted = [&] {
        for (const auto& effect : f.services.rounded_effects().packed_instances()) {
            if (effect.geometry.shape.rect == dot && effect.geometry.kind == graphics::RoundedEffectKind::outline &&
                effect.material.opacity > 0) {
                return effect.material.color;
            }
        }
        throw std::runtime_error("Timeline has no actual dot outline");
    };
    const auto before = painted();
    tone.set(TimelineTone::Green);
    sync(f);
    check(f.services.animations().size() > 0 && painted() == before,
          "status color did not begin a finite retained transition");
    tick(f, 50000);
    const auto midpoint = painted();
    const auto target = f.services.components().theme_scope(id)->snapshot().timeline().colors.green;
    check(midpoint != before && midpoint != target, "Timeline status transition has no painted midpoint");
    tick(f, 300000);
    check(painted() == target && f.services.animations().size() == 0, "Timeline status transition did not settle");
    tone.set(TimelineTone::Blue);
    sync(f);
    f.services.set_window_active(false);
    sync(f);
    check(f.services.animations().size() == 0, "inactive Timeline retained palette animation");
    f.services.set_window_active(true);
    tone.set(TimelineTone::Green);
    sync(f);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    check(f.services.animations().size() == 0, "reduced motion retained Timeline palette animation");
}

void self_destroying_predicate() {
    Fixture f;
    Signal<int> topology{0};
    bool retire{};
    runtime::ComponentId owner;
    f.services.mount(Content{[&] {
        Timeline(TimelineProps{}, bind([&] {
                     static_cast<void>(topology.get());
                     return std::vector<TimelineItem>{TimelineItem{u8"a", u8"Body"}.visible(bind([&] {
                         if (retire) {
                             static_cast<void>(f.services.destroy(owner));
                         }
                         return true;
                     }))};
                 }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    owner = f.services.timeline().mounted()[0];
    sync(f);
    retire = true;
    topology.set(1);
    sync(f);
    check(!f.services.components().contains(owner) && f.buttons.mounted_buttons().size() == 1,
          "predicate retirement left or accessed a dead owner");
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
        Timeline(TimelineProps{}, bind([&] {
                     std::vector<TimelineItem> rows{{u8"safe", u8"Safe"}};
                     if (remove.get()) {
                         rows.push_back(TimelineItem{u8"retire", u8""}.richContent(TimelineContent{body}));
                     }
                     return rows;
                 }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    owner = f.services.timeline().mounted()[0];
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
        form_composition(3);
        self_destroying_factory();
        self_destroying_predicate();
        horizontal_retained_and_titles();
        loading_and_cleanup();
        finite_palette_motion();
        failed_factory_retires_owner();
        std::cout << "Timeline: 3 new layout/retained/loading/failure scenarios passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
