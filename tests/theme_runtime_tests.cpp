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

} // namespace

int main() {
    try {
        test_default_nested_inheritance_and_reset();
        test_typed_identity_subscription_and_snapshot_diff();
        test_nested_override_masks_parent_subscription();
        test_dirty_domains_and_queue_bridge();
        test_motion_subscription_is_animation_only();
        test_error_rollback_and_cross_thread_failure();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
