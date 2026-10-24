#include "component/breadcrumb_component.hpp"
#include "component/steps_component.hpp"
#include "runtime/component_host.hpp"
#include "support/navigation_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
using namespace ryn_test::navigation;

// Read-only test observability of the existing registry collection, without a production accessor or layout cast.
struct BlockedRootsMember final {
    using Pointer = std::vector<runtime::ComponentId> InteractionRegistry::*;
    friend Pointer blocked_roots_member(BlockedRootsMember);
};

template <BlockedRootsMember::Pointer Member> struct ReadBlockedRoots final {
    friend BlockedRootsMember::Pointer blocked_roots_member(BlockedRootsMember) {
        return Member;
    }
};

template struct ReadBlockedRoots<&InteractionRegistry::blocked_roots_>;

const std::vector<runtime::ComponentId>& blocked_roots(const InteractionRegistry& registry) {
    return registry.*blocked_roots_member(BlockedRootsMember{});
}

runtime::Point center(Fixture& f, runtime::ComponentId id) {
    const auto b = bounds(f, id);
    return {b.x + b.width / 2, b.y + b.height / 2};
}

void lifetime_cycles(bool steps, std::string_view mode) {
    const bool hidden = mode == "hidden" || mode == "hidden_blur";
    const bool disabled = mode == "disabled" || mode == "disabled_blur";
    const bool inactive = mode == "inactive";
    const bool blur_destroy = mode == "hidden_blur" || mode == "disabled_blur";
    check(hidden || disabled || inactive, "unknown navigation block lifetime mode");
    Fixture f;
    runtime::ComponentId append_parent;
    int protected_clicks = 0;
    int survivor_clicks = 0;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                 Button(ButtonProps{}.onClick([&] { ++protected_clicks; }),
                        ButtonContent{[] { Text(u8"Other owner"); }});
                 Button(ButtonProps{}.onClick([&] { ++survivor_clicks; }), ButtonContent{[] { Text(u8"Survivor"); }});
                 Input(InputProps{}.layout(LayoutStyle{}.width(dp(160))));
                 auto& build = runtime::require_component_build_context();
                 append_parent = build.mount_component<int>(0);
                 const auto node = build.root(append_parent);
                 f.layout.set_layout(node, layout::BoxLayout{{}, true});
                 build.on_resource_cleanup(append_parent, [&f, node] { f.layout.remove_layout(node); });
             }});
    }});
    sync(f);
    auto& registry = f.services.interactions();
    const auto protected_button = f.buttons.mounted_buttons()[0];
    const auto survivor_button = f.buttons.mounted_buttons()[1];
    const auto survivor_input = f.inputs.mounted_inputs()[0];
    check(f.services.set_input_blocked(protected_button.component, true), "unrelated owner block setup failed");
    sync(f);
    const auto baseline_blocks = blocked_roots(registry);
    const auto baseline_interactions = registry.size();
    const auto baseline_components = f.services.components().component_count();
    const auto baseline_nodes = f.nodes.size();
    const auto baseline_effects = f.services.rounded_effects().live_count();
    check(baseline_blocks == std::vector{protected_button.component}, "unexpected registry baseline");
    constexpr std::size_t cycles = 8;
    bool registrations_returned_to_baseline = true;
    for (std::size_t cycle = 0; cycle < cycles; ++cycle) {
        Signal<bool> shown{true};
        Signal<bool> row_disabled{false};
        runtime::ComponentId owner;
        runtime::ComponentId blocked_row;
        int blur_calls = 0;
        int requests = 0;
        int child_clicks = 0;
        int slot_runs = 0;
        bool retire_on_blur = false;
        bool block_seen_in_blur = false;
        const auto rich = [&] {
            ++slot_runs;
            Flex(FlexProps{}.gap(dp(8)), FlexContent{[&] {
                     Button(ButtonProps{}.onClick([&] { ++child_clicks; }), ButtonContent{[] { Text(u8"Child"); }});
                     Input(InputProps{}.layout(LayoutStyle{}.width(dp(100))).onBlur([&] {
                         ++blur_calls;
                         if (retire_on_blur) {
                             block_seen_in_blur = std::ranges::find(blocked_roots(registry), blocked_row) !=
                                                  blocked_roots(registry).end();
                             check(f.services.destroy(owner), "blur callback owner destruction failed");
                         }
                     }));
                 }});
        };
        f.services.append_slot(append_parent, Content{[&] {
                                   if (steps) {
                                       Steps(StepsProps{}.onChange([&](std::size_t) { ++requests; }),
                                             {StepsItem{u8"rich", u8"Rich"}
                                                  .visible(shown)
                                                  .disabled(row_disabled)
                                                  .richDescription(StepsDescription{rich}),
                                              {u8"tail", u8"Tail"}});
                                   } else {
                                       Breadcrumb(
                                           BreadcrumbProps{}.onActivate([&](BreadcrumbActivation) { ++requests; }),
                                           {BreadcrumbItem{u8"rich", u8"Rich"}
                                                .visible(shown)
                                                .disabled(row_disabled)
                                                .richTitle(BreadcrumbTitle{rich}),
                                            {u8"tail", u8"Tail"}});
                                   }
                               }});
        sync(f);
        owner = steps ? f.services.steps().mounted()[0] : f.services.breadcrumb().mounted()[0];
        const auto items =
            steps ? f.services.steps().snapshot(owner).items : f.services.breadcrumb().snapshot(owner).items;
        blocked_row = items[0];
        const auto child_button = f.buttons.mounted_buttons()[2];
        const auto child_input = f.inputs.mounted_inputs()[1];
        const auto registrations_before = blocked_roots(registry).size();
        registrations_returned_to_baseline &= blocked_roots(registry) == baseline_blocks;
        check(slot_runs == 1, "new mount reran the retained rich slot");
        check(f.services.hit_test().hit_test(center(f, child_button.component)) == child_button.interaction &&
                  f.services.hit_test().hit_test(center(f, child_input.component)) == child_input.interaction,
              "remounted native child lost actual pointer eligibility");
        click(f, child_button.component);
        click(f, child_input.component);
        check(child_clicks == 1 && requests == 0 && f.services.focus().state().focused == child_input.interaction,
              "remounted child activation/focus failed or navigated wrapper");
        const auto stamp = f.inputs.sessions().active();
        check(stamp.valid() && static_cast<bool>(f.inputs.dispatch(TextCommitted{String{u8"你"}, stamp})),
              "child commit failed");
        check(static_cast<bool>(
                  f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, f.inputs.sessions().active()})),
              "child composition setup failed");
        retire_on_blur = blur_destroy;
        if (hidden) {
            shown.set(false);
        } else if (disabled) {
            row_disabled.set(true);
        } else {
            f.services.set_window_active(false);
        }
        sync(f);
        check(blur_calls == 1 && !f.inputs.sessions().active().valid(),
              "blocked teardown did not close child input once");
        if (blur_destroy) {
            check(block_seen_in_blur && !f.services.components().contains(owner),
                  "blur destruction did not run with owned row block");
        } else {
            const auto expected_blocks = registrations_before + (inactive ? items.size() : 1);
            check(blocked_roots(registry).size() == expected_blocks &&
                      std::ranges::find(blocked_roots(registry), blocked_row) != blocked_roots(registry).end(),
                  "fixture did not register actual row blocks");
            check(f.services.hit_test().hit_test(center(f, child_input.component)) != child_input.interaction,
                  "blocked native child remained hittable");
            check(f.services.destroy(owner), "direct owner destruction failed");
        }
        sync(f);
        std::cout << "block teardown steps=" << steps << " mode=" << mode << " cycle=" << cycle << " mounted="
                  << (steps ? f.services.steps().mounted().size() : f.services.breadcrumb().mounted().size())
                  << " records=" << registry.size() << '/' << baseline_interactions
                  << " blocks=" << blocked_roots(registry).size() << '/' << baseline_blocks.size() << '\n';
        check((steps ? f.services.steps().mounted().empty() : f.services.breadcrumb().mounted().empty()) &&
                  registry.size() == baseline_interactions &&
                  f.services.components().component_count() == baseline_components &&
                  f.nodes.size() == baseline_nodes && f.services.rounded_effects().live_count() == baseline_effects,
              "blocked navigation teardown retained mounted resources");
        // Complete the same-window remount sequence even before the fix, so stale registrations visibly accumulate.
        registrations_returned_to_baseline &= blocked_roots(registry) == baseline_blocks;
        check(!registry.component_active(protected_button.component), "navigation cleanup unblocked another owner");
        if (inactive) {
            f.services.set_window_active(true);
            sync(f);
        }
        check(f.services.hit_test().hit_test(center(f, survivor_button.component)) == survivor_button.interaction,
              "teardown left a stale input gate on surviving content");
        click(f, survivor_button.component);
        click(f, protected_button.component);
        check(survivor_clicks == static_cast<int>(cycle + 1) && protected_clicks == 0,
              "cleanup broke surviving or independently blocked pointer behavior");
        click(f, survivor_input.component);
        const auto survivor_stamp = f.inputs.sessions().active();
        check(f.services.focus().state().focused == survivor_input.interaction && survivor_stamp.valid() &&
                  static_cast<bool>(f.inputs.dispatch(TextCommitted{String{u8"x"}, survivor_stamp})),
              "surviving editor blocked after remount teardown");
    }
    check(registrations_returned_to_baseline, "destroyed navigation row input blocks accumulated across remounts");
    check(f.services.set_input_blocked(protected_button.component, false), "unrelated owner lost its registered block");
    sync(f);
    click(f, protected_button.component);
    check(blocked_roots(registry).empty() && protected_clicks == 1,
          "unrelated owner block release or final baseline failed");
    std::cout << "verified " << cycles << " same-window teardown/remount cycles\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 3, "expected widget and lifetime mode");
        const std::string_view widget = argv[1];
        check(widget == "breadcrumb" || widget == "steps", "unknown widget");
        lifetime_cycles(widget == "steps", argv[2]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
