#include "runtime/component_host.hpp"
#include "runtime/node_store.hpp"

#include <ryn/component.hpp>
#include <ryn/reactive.hpp>
#include <ryn/theme.hpp>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct DummyState final {};

ryn::runtime::ComponentId mount_dummy() {
    return ryn::runtime::require_component_build_context().mount_component<DummyState>();
}

ryn::ThemeConfig primary_config(ryn::Color color) {
    ryn::ThemeConfig config;
    config.seed.color_primary = color;
    return config;
}

void test_transparent_scopes_and_sibling_isolation() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::runtime::ComponentId default_component;
    ryn::runtime::ComponentId themed_component;
    ryn::runtime::ComponentId nested_component;
    ryn::runtime::ComponentId reset_component;
    ryn::runtime::ComponentId sibling_component;
    const auto red = ryn::Color::rgba8(220, 40, 60);
    const auto blue = ryn::Color::rgba8(25, 90, 220);

    host.mount(ryn::Content{[&] {
        default_component = mount_dummy();
        ryn::Theme(ryn::ThemeProps{}.config(primary_config(red)), ryn::ThemeContent{[&] {
                       themed_component = mount_dummy();
                       ryn::ThemeConfig nested;
                       nested.alias.color_text = ryn::Color::rgba8(15, 25, 35);
                       ryn::Theme(ryn::ThemeProps{}.config(nested),
                                  ryn::ThemeContent{[&] { nested_component = mount_dummy(); }});
                       ryn::ThemeConfig reset;
                       reset.inherit = false;
                       ryn::Theme(ryn::ThemeProps{}.config(reset),
                                  ryn::ThemeContent{[&] { reset_component = mount_dummy(); }});
                   }});
        ryn::Theme(ryn::ThemeProps{}.config(primary_config(blue)),
                   ryn::ThemeContent{[&] { sibling_component = mount_dummy(); }});
    }});

    require(host.component_count() == 5 && nodes.size() == 5,
            "Theme added a retained layout Node instead of remaining transparent");
    require(host.theme_scope(default_component)->snapshot() == ryn::resolve_theme(),
            "Host did not inject the Default Theme snapshot");
    require(host.theme_scope(themed_component)->snapshot().seed().color_primary == red,
            "single Theme scope did not reach its content");
    require(host.theme_scope(nested_component)->snapshot().seed().color_primary == red,
            "nested Theme did not inherit the parent seed");
    require(host.theme_scope(reset_component)->snapshot().seed().color_primary ==
                ryn::resolve_theme().seed().color_primary,
            "inherit=false retained the parent seed");
    require(host.theme_scope(sibling_component)->snapshot().seed().color_primary == blue,
            "sibling Theme scope leaked another sibling's seed");
}

void test_reactive_theme_does_not_rerun_content() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    ryn::runtime::ComponentId component;
    int content_runs = 0;

    host.mount(ryn::Content{[&] {
        ++content_runs;
        ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[&] { component = mount_dummy(); }});
    }});
    const auto root = host.root(component);
    const auto scope = host.theme_scope(component);
    const auto generation = scope->generation();
    auto next = primary_config(ryn::Color::rgba8(180, 30, 45));
    require(config.set(next), "reactive Theme config Signal did not change");
    require(content_runs == 1 && host.mount_runs() == 1, "reactive Theme update reran Component content");
    require(host.root(component) == root && scope->generation() == generation + 1,
            "reactive Theme update rebuilt identity or missed the retained scope");

    const auto requests_before = scope->diagnostics().notifications;
    require(!config.set(next) && scope->diagnostics().notifications == requests_before,
            "equal reactive Theme update emitted work");
}

void test_exception_destroy_reuse_and_scope_lifetime() {
    ryn::runtime::NodeStore throwing_nodes;
    ryn::runtime::ComponentHost throwing_host(throwing_nodes);
    bool observed = false;
    try {
        throwing_host.mount(ryn::Content{[] {
            ryn::Theme(ryn::ThemeProps{}, ryn::ThemeContent{[] {
                           static_cast<void>(mount_dummy());
                           throw std::runtime_error("Theme slot failure");
                       }});
        }});
    } catch (const std::runtime_error&) {
        observed = true;
    }
    require(observed && !throwing_host.active() && throwing_nodes.size() == 0,
            "throwing Theme slot did not unwind its Host atomically");

    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::runtime::ComponentId stale;
    ryn::runtime::ComponentId replacement;
    std::weak_ptr<ryn::theme_runtime::ThemeScope> stale_scope;
    host.mount(ryn::Content{[&] {
        ryn::Theme(ryn::ThemeProps{}.config(primary_config(ryn::Color::rgba8(100, 10, 20))), ryn::ThemeContent{[&] {
                       stale = mount_dummy();
                       stale_scope = host.theme_scope(stale);
                       require(host.destroy(stale), "Theme-scoped component could not be destroyed");
                   }});
        replacement = mount_dummy();
    }});
    require(stale.index == replacement.index && stale.generation != replacement.generation,
            "Theme-scoped component slot was not safely reused");
    require(stale_scope.expired(), "destroyed Theme content retained a stale scope");
}

void test_cross_thread_and_disposal() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    ryn::runtime::ComponentId component;
    host.mount(ryn::Content{
        [&] { ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[&] { component = mount_dummy(); }}); }});
    const auto scope = host.theme_scope(component);
    bool rejected = false;
    std::thread worker([&] {
        try {
            static_cast<void>(scope->snapshot());
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    worker.join();
    require(rejected, "cross-thread Theme access did not fail fast");

    const auto generation = scope->generation();
    host.dispose();
    require(config.set(primary_config(ryn::Color::rgba8(2, 3, 4))), "disposed Theme test Signal did not change");
    require(scope->generation() == generation, "disposed Host retained an active Theme Prop observer");
}

void test_nested_themes_release_configuration_frames_before_content() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    const auto props = ryn::ThemeProps{}.config(primary_config(ryn::Color::rgba8(12, 34, 56)));
    ryn::runtime::ComponentId leaf;
    std::function<void(int)> nested;
    nested = [&](int depth) {
        ryn::Theme(props, ryn::ThemeContent{[&, depth] {
                       if (depth) {
                           nested(depth - 1);
                       } else {
                           leaf = mount_dummy();
                       }
                   }});
    };
    host.mount(ryn::Content{[&] { nested(24); }});
    require(host.component_count() == 1 && nodes.size() == 1,
            "deep transparent themes introduced retained wrapper components");
    require(host.theme_scope(leaf)->snapshot().seed().color_primary == ryn::Color::rgba8(12, 34, 56),
            "nested scope initialization lost inherited configuration");
    host.dispose();
    require(nodes.size() == 0, "deep Theme content survived host disposal");
}

void test_binding_rejects_repeated_invalid_config_and_recovers() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    const auto config = std::make_unique<ryn::ThemeConfig>();
    ryn::Signal<int> revision{0};
    int evaluations = 0;
    const auto binding = ryn::bind([&] {
        static_cast<void>(revision.get());
        ++evaluations;
        return *config;
    });
    ryn::runtime::ComponentId component;
    host.mount(ryn::Content{
        [&] { ryn::Theme(ryn::ThemeProps{}.config(binding), ryn::ThemeContent{[&] { component = mount_dummy(); }}); }});
    const auto scope = host.theme_scope(component);
    const auto* before = &scope->snapshot();
    const auto generation = scope->generation();
    int notifications = 0;
    auto subscription = scope->capture([&](ryn::theme_runtime::DirtyPhase) { ++notifications; },
                                       [&] { static_cast<void>(scope->card_metrics()); });
    config->card.tokens.body_padding = ryn::dp(-1);
    for (int attempt = 1; attempt <= 2; ++attempt) {
        const auto evaluations_before = evaluations;
        bool rejected = false;
        try {
            static_cast<void>(revision.set(attempt));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && evaluations == evaluations_before + 1,
                "Theme binding skipped a repeated invalid configuration");
        require(&scope->snapshot() == before && scope->generation() == generation && notifications == 0,
                "rejected Theme binding published a snapshot or notification");
    }
    config->card.tokens.body_padding = ryn::dp(30);
    require(revision.set(3) && scope->snapshot().card().metrics.body_padding == 30 &&
                scope->generation() == generation + 1 && notifications == 1,
            "Theme binding did not recover after repeated invalid configurations");
    const auto recovered_generation = scope->generation();
    const auto recovered_evaluations = evaluations;
    host.dispose();
    config->card.tokens.body_padding = ryn::dp(32);
    require(revision.set(4) && scope->generation() == recovered_generation &&
                scope->snapshot().card().metrics.body_padding == 30 && notifications == 1 &&
                evaluations == recovered_evaluations,
            "disposed Theme binding retained an active observer");
}

} // namespace

int main() {
    try {
        test_transparent_scopes_and_sibling_isolation();
        test_reactive_theme_does_not_rerun_content();
        test_exception_destroy_reuse_and_scope_lifetime();
        test_cross_thread_and_disposal();
        test_nested_themes_release_configuration_frames_before_content();
        test_binding_rejects_repeated_invalid_config_and_recovers();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
