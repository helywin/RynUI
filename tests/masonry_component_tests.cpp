#include "component/masonry_component.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.02F;
}

runtime::ComponentId root(Fixture& f) {
    return f.services.components().root_components().front();
}

void leaf(Fixture& f, float height) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::LeafLayout{{20, height}});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
}

MasonryItem<> item(Fixture& f, std::int64_t key, float height) {
    return MasonryItem<>{MasonryKey{key}}.children(MasonryContent{[&f, height] { leaf(f, height); }});
}

auto snapshot(Fixture& f) {
    return f.services.masonry().snapshot(root(f));
}

void shortest_and_explicit_height() {
    Fixture f;
    auto first = item(f, 0, 200).height(dp(100));
    auto forced = item(f, 4, 10).column(1);
    f.services.mount(Content{[&] {
        Masonry(MasonryProps<>{}
                    .items(std::vector{first, item(f, 1, 40), item(f, 2, 60), item(f, 3, 30), forced})
                    .columns(2)
                    .gutter(dp(10), dp(20))
                    .layout(LayoutStyle{}.width(dp(210))));
    }});
    f.synchronize(600);
    const auto values = snapshot(f);
    check(values.size() == 5 && values[0].column == 0 && values[1].column == 1 && values[2].column == 1 &&
              values[3].column == 0 && values[4].column == 1,
          "shortest/pinned column choice failed");
    check(near(values[0].bounds.width, 100) && near(values[0].bounds.height, 100) && near(values[2].bounds.y, 60) &&
              near(values[3].bounds.y, 120) && near(values[4].bounds.x, 110),
          "measured dimensions/height override/gutter failed");
    const auto node = f.services.components().root(root(f));
    check(near(f.nodes.require(node).measured_size.height, 150), "Masonry total height includes trailing gutter");
}

void responsive_window_and_rtl() {
    Fixture f;
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    f.services.mount(Content{[&] {
        Masonry(MasonryProps<>{}
                    .items(std::vector{item(f, 0, 30), item(f, 1, 40).column(8), item(f, 2, 50)})
                    .columns(Responsive<int>{1}.at(Breakpoint::Md, 3))
                    .gutter(MasonryGutter{Responsive<LogicalLength>{dp(10)}.at(Breakpoint::Md, dp(20)),
                                          Responsive<LogicalLength>{dp(6)}})
                    .direction(direction)
                    .layout(LayoutStyle{}.width(dp(200))));
    }});
    f.synchronize(1000);
    const auto wide = snapshot(f);
    check(near(wide[0].bounds.width, 160.0F / 3) && wide[1].column == 2 && wide[2].column == 1,
          "Masonry used container width as breakpoint or lost specified column");
    direction.set(FlexDirection::RightToLeft);
    f.synchronize(1000);
    auto next = snapshot(f);
    check(near(next[0].bounds.x, 200 - wide[0].bounds.width) && near(next[1].bounds.x, 0), "Masonry RTL failed");
    f.synchronize(500);
    next = snapshot(f);
    check(next[1].column == 0 && near(next[0].bounds.width, 200) && next[0].component == wide[0].component &&
              next[2].component == wide[2].component,
          "responsive shrink remounted content or failed to clamp pin");
}

struct CardData final {
    String title;
    float height{};
};

static_assert(std::is_same_v<decltype(std::declval<MasonryItemView<CardData>>().data()), Prop<CardData>>);
static_assert(std::is_same_v<decltype(std::declval<MasonryItemView<CardData>>().column()), Prop<int>>);

void typed_data_and_incremental_identity() {
    Fixture f;
    std::vector<MasonryItem<CardData>> rows{{MasonryKey{std::int64_t{1}}, CardData{u8"one", 80}},
                                            {MasonryKey{std::int64_t{2}}, CardData{u8"two", 90}}};
    Signal<std::vector<MasonryItem<CardData>>> source{rows};
    int mounts{};
    f.services.mount(Content{[&] {
        Masonry(MasonryProps<CardData>{}
                    .items(source)
                    .columns(2)
                    .gutter(dp(8))
                    .itemRender([&](MasonryItemView<CardData> view) {
                        ++mounts;
                        Flex(FlexProps{}.vertical(true).layout(
                                 LayoutStyle{}.height(bind([view] { return dp(view.value().height); }))),
                             FlexContent{[view] {
                                 Text(TextProps{}.content(bind([view] { return view.value().title; })));
                                 Input(InputProps{}.defaultValue(view.value().title));
                             }});
                    })
                    .layout(LayoutStyle{}.width(dp(400))));
    }});
    f.synchronize(1000);
    const auto before = snapshot(f);
    const auto editor = f.inputs.mounted_inputs().front();
    check(f.services.focus().request_focus(editor.interaction, input::FocusModality::keyboard),
          "Masonry editor focus failed");
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8" edited"}, f.inputs.sessions().active()})),
          "Masonry edit failed");
    const auto draft = std::string{f.inputs.display_snapshot(editor.component).text};
    std::reverse(rows.begin(), rows.end());
    rows[1].data(CardData{u8"changed", 130});
    rows.push_back(MasonryItem<CardData>{MasonryKey{std::int64_t{3}}, CardData{u8"three", 60}});
    source.set(rows);
    f.synchronize(1000);
    auto after = snapshot(f);
    check(after[0].component == before[1].component && after[1].component == before[0].component && mounts == 3,
          "keyed insert/reorder remounted retained content");
    check(near(after[1].bounds.height, 130) && f.inputs.display_snapshot(editor.component).text == draft,
          "typed data binding failed or reset retained editor state");
    const auto removed = before[1].component;
    rows.erase(rows.begin());
    source.set(rows);
    f.synchronize(1000);
    after = snapshot(f);
    check(!f.services.components().contains(removed) && after.size() == 2 &&
              after[0].component == before[0].component && mounts == 3,
          "key removal retained dead content or remounted survivor");
}

void dynamic_measurement_and_callbacks() {
    Fixture f;
    Signal<LogicalLength> height{dp(20)};
    int callbacks{};
    std::vector<MasonryPlacement> assignments;
    auto dynamic = MasonryItem<>{MasonryKey{std::int64_t{0}}}.children(MasonryContent{
        [&] { Flex(FlexProps{}.layout(LayoutStyle{}.height(height)), FlexContent{[&] { leaf(f, 5); }}); }});
    f.services.mount(Content{[&] {
        Masonry(MasonryProps<>{}
                    .items(std::vector{dynamic, item(f, 1, 80), item(f, 2, 20), item(f, 3, 20)})
                    .columns(2)
                    .onLayoutChange([&](auto value) {
                        ++callbacks;
                        assignments = std::move(value);
                    })
                    .layout(LayoutStyle{}.width(dp(200))));
    }});
    f.synchronize(1000);
    const auto before = snapshot(f);
    check(callbacks == 1 && assignments[2].column == 0, "initial allocation callback failed");
    f.synchronize(1000);
    check(callbacks == 1, "unchanged layout emitted callback");
    height.set(dp(120));
    f.synchronize(1000);
    const auto after = snapshot(f);
    check(callbacks == 2 && assignments[2].column == 1 && after[0].component == before[0].component &&
              near(after[2].bounds.y, 80),
          "real child resize failed to redistribute retained items");
    height.set(dp(130));
    f.synchronize(1000);
    check(callbacks == 2, "height-only change emitted unchanged assignment");
}

void callback_reentry_and_destruction() {
    Fixture f;
    runtime::ComponentId owner;
    int calls{};
    f.services.mount(Content{[&] {
        Masonry(MasonryProps<>{}.items(std::vector{item(f, 1, 20)}).onLayoutChange([&](auto) {
            ++calls;
            check(f.services.destroy(owner), "callback self-destruction failed");
        }));
    }});
    owner = root(f);
    f.synchronize(600);
    check(calls == 1 && f.nodes.size() == 0, "Masonry callback accessed destroyed state");

    Fixture reentry;
    Signal<Responsive<int>> columns{Responsive<int>{2}};
    calls = 0;
    reentry.services.mount(Content{[&] {
        Masonry(MasonryProps<>{}
                    .items(std::vector{item(reentry, 1, 20), item(reentry, 2, 20)})
                    .columns(columns)
                    .onLayoutChange([&](auto) {
                        if (++calls == 1) {
                            columns.set(Responsive<int>{1});
                        }
                    }));
    }});
    reentry.synchronize(600);
    reentry.synchronize(600);
    check(calls == 2 && snapshot(reentry)[1].column == 0, "callback-driven relayout was lost or recursed");
}

void validation() {
    for (int scenario = 0; scenario < 11; ++scenario) {
        Fixture f;
        bool rejected{};
        try {
            auto rows = std::vector{item(f, 1, 20)};
            if (scenario == 0) {
                rows.push_back(item(f, 1, 10));
            }
            if (scenario == 1) {
                rows[0] = MasonryItem<>{MasonryKey{String{}}}.children(MasonryContent{[] {}});
            }
            if (scenario == 2) {
                rows[0].height(dp(-1));
            }
            if (scenario == 3) {
                rows[0].height(dp(std::numeric_limits<float>::quiet_NaN()));
            }
            if (scenario == 4) {
                rows[0].height(auto_length);
            }
            if (scenario == 5) {
                rows[0].column(-1);
            }
            if (scenario == 7) {
                rows[0].height(dp(std::numeric_limits<float>::infinity()));
            }
            MasonryGutter gap;
            if (scenario == 8) {
                gap.horizontal = Responsive<LogicalLength>{dp(-1)};
            }
            if (scenario == 9) {
                gap.vertical.at(Breakpoint::Xxxl, dp(std::numeric_limits<float>::infinity()));
            }
            f.services.mount(Content{[&] {
                Masonry(MasonryProps<>{}.items(rows).gutter(gap).columns(scenario == 6 ? 0 : scenario == 10 ? -1 : 2));
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.nodes.size() == 0, "invalid Masonry mount was accepted or leaked resources");
    }
    Fixture f;
    Signal<std::vector<MasonryItem<>>> source{std::vector{item(f, 1, 20)}};
    Signal<Responsive<int>> columns{Responsive<int>{2}};
    f.services.mount(Content{[&] { Masonry(MasonryProps<>{}.items(source).columns(columns)); }});
    f.synchronize();
    const auto stable = snapshot(f)[0].component;
    bool rejected{};
    try {
        source.set(std::vector{item(f, 1, 10), item(f, 1, 20)});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && snapshot(f)[0].component == stable, "invalid reactive keys changed committed items");
    rejected = false;
    try {
        columns.set(Responsive<int>{1}.at(Breakpoint::Xxxl, 0));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "inactive invalid responsive column count accepted");

    Fixture mixed;
    mixed.services.mount(Content{[] {
        Masonry(
            MasonryProps<>{}.items(std::vector{MasonryItem<>{1}.height(dp(10)), MasonryItem<>{u8"1"}.height(dp(20))}));
    }});
    mixed.synchronize();
    check(snapshot(mixed).size() == 2 && snapshot(mixed)[0].key != snapshot(mixed)[1].key,
          "string/integer keys collided or empty explicit-height content was rejected");
}

void mount_failure_and_retry() {
    Fixture f;
    int live{};
    int cleanups{};
    auto stable = item(f, 1, 20);
    bool throwing{true};
    auto added = MasonryItem<>{MasonryKey{std::int64_t{2}}}.children(MasonryContent{[&] {
        auto& build = runtime::require_component_build_context();
        const auto child = build.mount_component<int>(0);
        const auto node = build.root(child);
        f.layout.set_layout(node, layout::LeafLayout{});
        build.on_resource_cleanup(child, [&f, node] { f.layout.remove_layout(node); });
        ++live;
        build.on_resource_cleanup(child, [&] {
            --live;
            ++cleanups;
        });
        if (throwing) {
            throw std::runtime_error("intentional Masonry slot failure");
        }
    }});
    Signal<std::vector<MasonryItem<>>> source{std::vector{stable}};
    f.services.mount(Content{[&] { Masonry(MasonryProps<>{}.items(source)); }});
    f.synchronize();
    const auto before = snapshot(f);
    source.set(std::vector{stable, added});
    bool rejected{};
    try {
        f.synchronize();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected && live == 0 && cleanups == 1 && snapshot(f)[0].component == before[0].component,
          "failed incremental slot leaked or destroyed retained key");
    throwing = false;
    f.synchronize();
    check(snapshot(f).size() == 2 && live == 1 && snapshot(f)[0].component == before[0].component,
          "Masonry incremental mount failed to retry");
    f.services.destroy(root(f));
    check(live == 0 && cleanups == 2, "retried slot cleanup did not run once");
}

void lifecycle_and_extreme_width() {
    Fixture f;
    Signal<Responsive<int>> columns{Responsive<int>{std::numeric_limits<int>::max()}};
    Signal<std::uint64_t> refresh{0};
    f.services.mount(Content{[&] {
        Masonry(MasonryProps<>{}
                    .items(std::vector{item(f, 1, 0), item(f, 2, 0)})
                    .columns(columns)
                    .fresh()
                    .refresh(refresh)
                    .gutter(dp(100))
                    .layout(LayoutStyle{}.width(dp(0))));
    }});
    f.synchronize(1, {0, 0, 1, 240});
    const auto values = snapshot(f);
    check(std::ranges::all_of(values,
                              [](const auto& value) {
                                  return std::isfinite(value.bounds.x) && std::isfinite(value.bounds.y) &&
                                         value.bounds.width == 0;
                              }),
          "tiny window produced non-finite geometry");
    static_cast<void>(f.frames.consume_request());
    f.synchronize(1, {0, 0, 1, 240});
    check(!f.frames.pending(), "fresh sampling requested an endless follow-up frame");
    check(f.services.destroy(root(f)), "Masonry destroy failed");
    f.dirty.clear();
    static_cast<void>(f.frames.consume_request());
    columns.set(Responsive<int>{1});
    refresh.set(1);
    check(f.nodes.size() == 0 && !f.frames.pending(), "Masonry destroy retained subscriptions or frames");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "shortest") {
            shortest_and_explicit_height();
        } else if (name == "responsive") {
            responsive_window_and_rtl();
        } else if (name == "retained") {
            typed_data_and_incremental_identity();
        } else if (name == "dynamic") {
            dynamic_measurement_and_callbacks();
        } else if (name == "callbacks") {
            callback_reentry_and_destruction();
        } else if (name == "validation") {
            validation();
        } else if (name == "rollback") {
            mount_failure_and_retry();
        } else if (name == "lifecycle") {
            lifecycle_and_extreme_width();
        } else {
            throw std::runtime_error("unknown Masonry test case");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
