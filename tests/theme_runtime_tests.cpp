#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"
#include "runtime/node_store.hpp"
#include "theme/theme_runtime.hpp"

#include <ryn/theme.hpp>

#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::ThemeConfig text_color_config(ryn::Color color) {
    ryn::ThemeConfig config;
    config.text.tokens.color = color;
    return config;
}

void test_default_nested_inheritance_and_reset() {
    const auto root = ryn::theme_runtime::ThemeScope::create_default();
    require(root->snapshot() == ryn::resolve_theme(),
        "default ThemeScope did not inject the Default snapshot");

    ryn::ThemeConfig brand;
    brand.seed.color_primary = ryn::Color::rgba8(210, 32, 54);
    const auto parent = ryn::theme_runtime::ThemeScope::create(root, brand);
    ryn::ThemeConfig inherited;
    inherited.alias.color_text = ryn::Color::rgba8(20, 30, 40);
    const auto child = ryn::theme_runtime::ThemeScope::create(parent, inherited);
    require(child->snapshot() == ryn::resolve_theme(inherited, &parent->snapshot()),
        "nested ThemeScope did not inherit its parent snapshot");

    ryn::ThemeConfig reset;
    reset.inherit = false;
    const auto isolated = ryn::theme_runtime::ThemeScope::create(parent, reset);
    require(isolated->snapshot() == ryn::resolve_theme(reset),
        "inherit=false did not reset to the Default seed");

    ryn::ThemeConfig sibling_config;
    sibling_config.seed.color_primary = ryn::Color::rgba8(26, 115, 232);
    const auto sibling = ryn::theme_runtime::ThemeScope::create(root, sibling_config);
    require(sibling->snapshot().identity() != parent->snapshot().identity(),
        "sibling Theme scopes leaked resolved state");
}

void test_typed_identity_subscription_and_snapshot_diff() {
    const auto scope = ryn::theme_runtime::ThemeScope::create_default();
    int color_notifications = 0;
    int typography_notifications = 0;
    ryn::theme_runtime::DirtyPhase color_phase{};
    auto color_subscription = scope->capture(
        [&](ryn::theme_runtime::DirtyPhase phase) {
            ++color_notifications;
            color_phase = phase;
        },
        [&] { static_cast<void>(scope->text_color()); });
    auto typography_subscription = scope->capture(
        [&](ryn::theme_runtime::DirtyPhase) { ++typography_notifications; },
        [&] { static_cast<void>(scope->text_font_size()); });

    const auto generation = scope->generation();
    const auto red = ryn::Color::rgba8(200, 10, 20);
    require(scope->update(text_color_config(red)),
        "changed Theme config was treated as equal");
    require(scope->generation() == generation + 1,
        "changed Theme did not advance generation");
    require(scope->text_color() == red,
        "typed Text color accessor did not expose the new snapshot");
    require(color_notifications == 1 && typography_notifications == 0,
        "Theme diff notified an unrelated typed subscription");
    require(color_phase == ryn::theme_runtime::DirtyPhase::paint_material,
        "Text color did not map to Paint/Material only");

    const auto changed = scope->changed_identities();
    require(changed.size() == 1
            && changed.front() == ryn::theme_runtime::TokenIdentity::text_color,
        "Theme diagnostics did not retain the exact changed identity");
    const auto allocations = scope->diagnostics().subscription_allocations;
    require(!scope->update(text_color_config(red)),
        "equal Theme update replaced an immutable snapshot");
    require(scope->generation() == generation + 1
            && scope->diagnostics().snapshot_reuses >= 1
            && scope->diagnostics().subscription_allocations == allocations,
        "equal Theme update changed generation or stable subscriptions");
    require(color_notifications == 1 && typography_notifications == 0,
        "equal Theme update emitted an invalidation");

    color_subscription.reset();
    require(scope->update(text_color_config(ryn::Color::rgba8(5, 80, 160))),
        "second color update was not applied");
    require(color_notifications == 1,
        "stale Theme subscription received an invalidation");
    static_cast<void>(typography_subscription);
}

void test_nested_override_masks_parent_subscription() {
    const auto root = ryn::theme_runtime::ThemeScope::create_default();
    const auto fixed = ryn::Color::rgba8(90, 40, 130);
    const auto child = ryn::theme_runtime::ThemeScope::create(
        root,
        text_color_config(fixed));
    int notifications = 0;
    auto subscription = child->capture(
        [&](ryn::theme_runtime::DirtyPhase) { ++notifications; },
        [&] { static_cast<void>(child->text_color()); });

    ryn::ThemeConfig parent_update;
    parent_update.alias.color_text = ryn::Color::rgba8(1, 2, 3);
    require(root->update(parent_update), "parent Theme update failed");
    require(child->text_color() == fixed && notifications == 0,
        "nested override did not mask its parent Token change");
    static_cast<void>(subscription);
}

void test_dirty_domains_and_queue_bridge() {
    using Phase = ryn::theme_runtime::DirtyPhase;
    using Identity = ryn::theme_runtime::TokenIdentity;
    require(ryn::theme_runtime::dirty_phase_for(Identity::text_color)
            == Phase::paint_material,
        "paint Token phase mapping is incorrect");
    require(ryn::theme_runtime::has_any(
            ryn::theme_runtime::dirty_phase_for(Identity::button_shadows),
            Phase::geometry | Phase::paint_material),
        "effect Token phase mapping is incomplete");
    require(ryn::theme_runtime::has_any(
            ryn::theme_runtime::dirty_phase_for(Identity::text_font_size),
            Phase::text | Phase::measure_layout),
        "text Token phase mapping is incomplete");
    require(ryn::theme_runtime::has_any(
            ryn::theme_runtime::dirty_phase_for(Identity::button_padding_inline),
            Phase::measure_layout | Phase::geometry | Phase::hit_test),
        "layout Token phase mapping is incomplete");
    require(ryn::theme_runtime::dirty_phase_for(Identity::map_motion_base)
            == Phase::animation,
        "motion Token phase mapping is incorrect");

    // Typography and Divider groups must land on disjoint domains: a color-only
    // change may not re-measure, and a font change must re-shape text.
    require(ryn::theme_runtime::dirty_phase_for(Identity::typography_colors)
            == Phase::paint_material
                && ryn::theme_runtime::dirty_phase_for(Identity::divider_colors)
                    == Phase::paint_material
                && ryn::theme_runtime::dirty_phase_for(Identity::alias_color_split)
                    == Phase::paint_material
                && ryn::theme_runtime::dirty_phase_for(Identity::map_color_success_text)
                    == Phase::paint_material
                && ryn::theme_runtime::dirty_phase_for(Identity::map_color_link)
                    == Phase::paint_material,
        "Typography/Divider color identities are not paint-only");
    require(ryn::theme_runtime::has_any(
            ryn::theme_runtime::dirty_phase_for(Identity::typography_headings),
            Phase::text | Phase::measure_layout)
                && ryn::theme_runtime::has_any(
                    ryn::theme_runtime::dirty_phase_for(Identity::typography_fonts),
                    Phase::text | Phase::measure_layout)
                && ryn::theme_runtime::has_any(
                    ryn::theme_runtime::dirty_phase_for(Identity::typography_base_typography),
                    Phase::text | Phase::measure_layout)
                && ryn::theme_runtime::has_any(
                    ryn::theme_runtime::dirty_phase_for(Identity::divider_typography),
                    Phase::text | Phase::measure_layout),
        "Typography/Divider font identities do not re-shape text");
    require(ryn::theme_runtime::has_any(
            ryn::theme_runtime::dirty_phase_for(Identity::typography_metrics),
            Phase::measure_layout | Phase::geometry)
                && ryn::theme_runtime::has_any(
                    ryn::theme_runtime::dirty_phase_for(Identity::divider_metrics),
                    Phase::measure_layout | Phase::geometry | Phase::hit_test)
                && ryn::theme_runtime::has_any(
                    ryn::theme_runtime::dirty_phase_for(Identity::typography_inline_code),
                    Phase::geometry)
                && ryn::theme_runtime::has_any(
                    ryn::theme_runtime::dirty_phase_for(Identity::typography_inline_keyboard),
                    Phase::geometry),
        "Typography/Divider metric identities do not re-lay out");
    for (auto identity = Identity::typography_colors;
         identity <= Identity::divider_typography;
         identity = static_cast<Identity>(static_cast<std::uint8_t>(identity) + 1U)) {
        require(ryn::theme_runtime::dirty_phase_for(identity) != Phase::none,
                "a Typography/Divider identity has no invalidation domain");
    }
    // Every identity must have a stable, distinct, non-placeholder name.
    require(ryn::theme_runtime::token_identity_name(Identity::alias_color_split)
                == "alias.colorSplit"
                && ryn::theme_runtime::token_identity_name(Identity::typography_colors)
                    == "Typography.colors"
                && ryn::theme_runtime::token_identity_name(Identity::typography_inline_keyboard)
                    == "Typography.inlineKeyboard"
                && ryn::theme_runtime::token_identity_name(Identity::divider_typography)
                    == "Divider.typography"
                && ryn::theme_runtime::token_identity_name(Identity::seed_line_width)
                    == "seed.lineWidth",
        "new Token identity names drifted from their declaration order");

    ryn::runtime::NodeStore nodes;
    const auto node = nodes.create_root();
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    const auto mixed = Phase::paint_material | Phase::geometry | Phase::text
        | Phase::measure_layout | Phase::hit_test | Phase::animation;
    const auto flags = ryn::runtime::dirty_flags_for_theme(mixed);
    require(!ryn::runtime::has_any(flags, ryn::runtime::DirtyFlags::Structure),
        "Theme phase mapping introduced Structure invalidation");
    dirty.invalidate(node, flags);
    require(dirty.material_nodes().size() == 1
            && dirty.geometry_nodes().size() == 1
            && dirty.text_nodes().size() == 1
            && dirty.layout_roots().size() == 1
            && dirty.hit_test_nodes().size() == 1
            && dirty.animation_nodes().size() == 1
            && frames.pending(),
        "mixed Theme phases did not reach the exact dirty queues");
}

void test_motion_subscription_is_animation_only() {
    using Phase = ryn::theme_runtime::DirtyPhase;
    using Identity = ryn::theme_runtime::TokenIdentity;
    const auto scope = ryn::theme_runtime::ThemeScope::create_default();
    int motion_notifications = 0;
    int typography_notifications = 0;
    Phase motion_phase{Phase::none};
    auto motion_subscription = scope->capture(
        [&](Phase phase) {
            ++motion_notifications;
            motion_phase = phase;
        },
        [&] {
            static_cast<void>(scope->motion_unit());
            static_cast<void>(scope->motion_base());
            static_cast<void>(scope->motion_enabled());
        });
    auto typography_subscription = scope->capture(
        [&](Phase) { ++typography_notifications; },
        [&] { static_cast<void>(scope->text_font_size()); });

    ryn::ThemeConfig changed;
    changed.seed.motion_unit = ryn::Duration::milliseconds(150.0F);
    require(scope->update(changed), "motion Token update was suppressed");
    require(scope->motion_unit() == ryn::Duration::milliseconds(150.0F)
                && scope->motion_base() == ryn::Duration{}
                && scope->motion_enabled(),
            "typed motion accessors did not expose the resolved Theme values");
    require(motion_notifications == 1 && typography_notifications == 0
                && motion_phase == Phase::animation,
            "motion Token update notified unrelated Theme subscribers");
    require(scope->changed_identities().size() == 1
                && scope->changed_identities().front() == Identity::map_motion_unit,
            "motion Token diagnostics did not retain the exact changed identity");

    ryn::runtime::NodeStore nodes;
    const auto node = nodes.create_root();
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    const auto flags = ryn::runtime::dirty_flags_for_theme(motion_phase);
    dirty.invalidate(node, flags);
    require(dirty.animation_nodes().size() == 1
                && dirty.material_nodes().empty()
                && dirty.geometry_nodes().empty()
                && dirty.text_nodes().empty()
                && dirty.layout_roots().empty()
                && dirty.hit_test_nodes().empty(),
            "motion Theme update expanded beyond Animation dirty state");
    static_cast<void>(motion_subscription);
    static_cast<void>(typography_subscription);
}

void test_error_rollback_and_cross_thread_failure() {
    const auto scope = ryn::theme_runtime::ThemeScope::create_default();
    const auto identity = scope->snapshot().identity();
    const auto generation = scope->generation();
    ryn::ThemeConfig invalid;
    invalid.algorithms = {static_cast<ryn::ThemeAlgorithm>(255)};
    bool rejected = false;
    try {
        static_cast<void>(scope->update(invalid));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && scope->snapshot().identity() == identity
            && scope->generation() == generation,
        "invalid Theme update was not rolled back atomically");

    bool wrong_thread_rejected = false;
    std::thread worker([&] {
        try {
            static_cast<void>(scope->snapshot());
        } catch (const std::logic_error&) {
            wrong_thread_rejected = true;
        }
    });
    worker.join();
    require(wrong_thread_rejected,
        "cross-thread ThemeScope access did not fail fast");
}

void test_typography_and_divider_subscription_domains() {
    using Phase = ryn::theme_runtime::DirtyPhase;

    // Each case uses its own scope: `ThemeScope::update` replaces the whole
    // config rather than merging with the previous one, so reusing a scope would
    // let a dropped override look like an unrelated token change.
    const auto observe = [](const ryn::ThemeConfig& config, int& colors, int& typography,
                             Phase& color_phase) {
        const auto scope = ryn::theme_runtime::ThemeScope::create_default();
        auto color_subscription = scope->capture(
            [&](Phase phase) {
                ++colors;
                color_phase = phase;
            },
            [&] {
                static_cast<void>(scope->typography_colors());
                static_cast<void>(scope->divider_colors());
            });
        auto typography_subscription = scope->capture(
            [&](Phase) { ++typography; },
            [&] {
                static_cast<void>(scope->typography_headings());
                static_cast<void>(scope->typography_fonts());
                static_cast<void>(scope->typography_base_typography());
                static_cast<void>(scope->typography_metrics());
                static_cast<void>(scope->typography_inline_code());
                static_cast<void>(scope->typography_inline_keyboard());
                static_cast<void>(scope->divider_metrics());
                static_cast<void>(scope->divider_typography());
                static_cast<void>(scope->code_font_family());
            });
        const bool updated = scope->update(config);
        static_cast<void>(color_subscription);
        static_cast<void>(typography_subscription);
        return updated;
    };

    // A color-only change must stay in paint/material.
    {
        int colors = 0;
        int typography = 0;
        Phase color_phase{Phase::none};
        ryn::ThemeConfig config;
        config.typography.tokens.error = ryn::Color::rgba8(1, 2, 3);
        require(observe(config, colors, typography, color_phase),
            "Typography color update was suppressed");
        require(colors == 1 && color_phase == Phase::paint_material,
            "Typography color change did not stay in paint/material");
        require(typography == 0,
            "Typography color change invalidated measurement tokens");
    }

    // A divider color-only change must also stay in paint/material.
    {
        int colors = 0;
        int typography = 0;
        Phase color_phase{Phase::none};
        ryn::ThemeConfig config;
        config.divider.tokens.line = ryn::Color::rgba8(9, 9, 9);
        require(observe(config, colors, typography, color_phase),
            "Divider color update was suppressed");
        require(colors == 1 && color_phase == Phase::paint_material,
            "Divider color change did not stay in paint/material");
        require(typography == 0,
            "Divider color change invalidated measurement tokens");
    }

    // A metric-only change must reach measurement tokens and never colors.
    {
        int colors = 0;
        int typography = 0;
        Phase color_phase{Phase::none};
        ryn::ThemeConfig config;
        config.divider.tokens.horizontal_margin = ryn::dp(9.0F);
        require(observe(config, colors, typography, color_phase),
            "Divider metric update was suppressed");
        require(colors == 0, "Divider metric change reached color tokens");
        require(typography == 1,
            "Divider metric change missed the measurement tokens");
    }

    // A font change re-shapes text and must not be reported as a color change.
    {
        int colors = 0;
        int typography = 0;
        Phase color_phase{Phase::none};
        ryn::ThemeConfig config;
        config.typography.tokens.font_family_code = ryn::SystemFontFamily::ui_sans;
        require(observe(config, colors, typography, color_phase),
            "Typography font update was suppressed");
        require(colors == 0 && typography == 1,
            "Typography font change reached the wrong token groups");
    }

    // Heading metrics must stay out of the color group as well.
    {
        int colors = 0;
        int typography = 0;
        Phase color_phase{Phase::none};
        ryn::ThemeConfig config;
        config.typography.tokens.heading_font_sizes[0] = ryn::dp(40.0F);
        require(observe(config, colors, typography, color_phase),
            "Typography heading update was suppressed");
        require(colors == 0 && typography == 1,
            "Typography heading change reached the wrong token groups");
    }

    // `code_font_family` reads the code family, not the UI family, and follows
    // Theme updates.
    {
        const auto scope = ryn::theme_runtime::ThemeScope::create_default();
        require(scope->code_font_family() == ryn::SystemFontFamily::ui_monospace,
            "code_font_family did not return the monospace family");
        ryn::ThemeConfig config;
        config.typography.tokens.font_family_code = ryn::SystemFontFamily::ui_sans;
        require(scope->update(config), "Typography font update was suppressed");
        require(scope->code_font_family() == ryn::SystemFontFamily::ui_sans,
            "code_font_family did not follow the Theme update");
        // An equal update must be suppressed without notifying the new groups.
        int colors = 0;
        int typography = 0;
        auto subscription = scope->capture(
            [&](Phase) { ++colors; },
            [&] { static_cast<void>(scope->typography_colors()); });
        auto heading_subscription = scope->capture(
            [&](Phase) { ++typography; },
            [&] { static_cast<void>(scope->typography_headings()); });
        require(!scope->update(config), "equal Typography update was not suppressed");
        require(colors == 0 && typography == 0,
            "equal Typography update notified subscribers");
        static_cast<void>(subscription);
        static_cast<void>(heading_subscription);
    }
}

void test_focus_outline_seed_invalidation() {
    const auto scope = ryn::theme_runtime::ThemeScope::create_default();
    int notifications = 0;
    ryn::theme_runtime::DirtyPhase phase{};
    auto subscription = scope->capture(
        [&](ryn::theme_runtime::DirtyPhase dirty) {
            ++notifications;
            phase = dirty;
        },
        [&] { static_cast<void>(scope->focus_outline_width()); });
    ryn::ThemeConfig config;
    config.seed.focus_outline = false;
    require(scope->update(config) && scope->focus_outline_width() == 0.0F
                && notifications == 1
                && phase == (ryn::theme_runtime::DirtyPhase::geometry
                             | ryn::theme_runtime::DirtyPhase::paint_material),
            "focusOutline seed did not invalidate focus outline geometry and paint");
    require(!scope->update(config) && notifications == 1,
            "equal focusOutline update caused redundant invalidation");
}

} // namespace

int main() {
    try {
        test_default_nested_inheritance_and_reset();
        test_typed_identity_subscription_and_snapshot_diff();
        test_nested_override_masks_parent_subscription();
        test_dirty_domains_and_queue_bridge();
        test_motion_subscription_is_animation_only();
        test_typography_and_divider_subscription_domains();
        test_error_rollback_and_cross_thread_failure();
        test_focus_outline_seed_invalidation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
