#include "support/state_projection_fixture.hpp"
#include "support/allocation_probe.hpp"
#include <cstdio>
#include <exception>
#include <string_view>

namespace {
using namespace projection_test;

void tree_local() {
    for (const auto count : {128U, 512U, 1024U}) {
        Fixture f;
        const auto id = mount_tree(f, count);
        const auto initial = f.services.tree().snapshot(id);
        hover(f, initial.rows[0]);
        sync(f);
        auto before = tree_counts(f.services.tree(), id);
        auto rebuilds = f.services.scene_composer().diagnostics().rebuilds;
        hover(f, initial.rows[1]);
        sync(f);
        const auto after_hover = tree_counts(f.services.tree(), id);
        check(f.services.scene_composer().diagnostics().rebuilds == rebuilds,
              "moving Tree hover rebuilt scene structure");
        check(!before.available ||
                  (after_hover.projected == before.projected && after_hover.presented - before.presented == 2),
              "Tree hover visited unrelated projection/presentation rows");
        check(f.services.focus().request_focus(initial.rows[0].interaction, FocusModality::keyboard),
              "Tree initial keyboard focus failed");
        sync(f);
        before = tree_counts(f.services.tree(), id);
        rebuilds = f.services.scene_composer().diagnostics().rebuilds;
        f.services.focus().dispatch({Key::down, KeyAction::down});
        f.services.focus().dispatch({Key::down, KeyAction::up});
        sync(f);
        const auto after_focus = tree_counts(f.services.tree(), id);
        check(f.services.focus().state().focused == initial.rows[1].interaction &&
                  !f.services.interactions().require(initial.rows[0].interaction).tab_stop &&
                  f.services.interactions().require(initial.rows[1].interaction).tab_stop,
              "Tree local keyboard move lost focus or roving entry");
        check(!before.available ||
                  (after_focus.projected == before.projected && after_focus.presented - before.presented == 2),
              "Tree keyboard move reprojected or presented unrelated rows");
        check(f.services.scene_composer().diagnostics().rebuilds == rebuilds,
              "Tree keyboard move without scrolling rebuilt scene structure");
        const auto after = f.services.tree().snapshot(id);
        for (std::size_t i = 0; i < initial.rows.size(); ++i) {
            check(after.rows[i].component == initial.rows[i].component && after.rows[i].title == initial.rows[i].title,
                  "local Tree update changed a retained generation");
        }
    }
}

void tree_invalidation() {
    Fixture f;
    Signal<TreeKeys> expanded{TreeKeys{u8"parent"}};
    Signal<TreeKeys> checked{TreeKeys{u8"parent"}};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Tree(
                      TreeProps{}.expandedKeys(expanded).checkedKeys(checked).checkable(),
                      {TreeItem{u8"parent",
                                u8"Parent",
                                {TreeItem{u8"a", u8"A"}.disabled(disabled), TreeItem{u8"b", u8"B"}.visible(visible)}}});
              }});
    }});
    sync(f);
    const auto id = f.services.tree().mounted().back();
    const auto initial = f.services.tree().snapshot(id);
    hover(f, initial.rows[1]);
    sync(f);
    disabled.set(true);
    sync(f);
    auto view = f.services.tree().snapshot(id);
    check(!view.rows[1].checked && view.rows[2].checked &&
              !f.services.interactions().require(view.rows[1].interaction).focusable,
          "hover cache hid a disabled conduction change");
    checked.set(TreeKeys{u8"b"});
    disabled.set(false);
    sync(f);
    view = f.services.tree().snapshot(id);
    check(view.rows[0].indeterminate && !view.rows[0].checked && view.rows[2].checked,
          "hover cache hid a checked/half-checked change");
    expanded.set(TreeKeys{});
    sync(f);
    check(!f.services.tree().snapshot(id).rows[1].shown, "hover cache hid collapsed children");
    expanded.set(TreeKeys{u8"parent"});
    visible.set(false);
    auto config = ThemeConfig{};
    config.tree.tokens.font_size = dp(21);
    config.tree.tokens.font_family = SystemFontFamily::ui_monospace;
    theme.set(config);
    sync(f);
    view = f.services.tree().snapshot(id);
    check(view.rows[1].shown && !view.rows[2].shown &&
              f.services.components().theme_scope(id)->snapshot().tree().metrics.font_size == 21 &&
              view.rows[1].title == initial.rows[1].title,
          "Tree model/Theme invalidation lost visibility, font or retained title");
    bool actual_font = false;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (descendant(f, text.component, view.rows[1].title)) {
            actual_font |= f.scene.text_state(text.scene).content().bytes() == "A" &&
                           f.nodes.require(f.services.components().root(text.component)).measured_size.height > 20;
        }
    }
    check(actual_font && f.chains.contains(21) &&
              f.services.components().theme_scope(id)->snapshot().tree().metrics.font_family ==
                  SystemFontFamily::ui_monospace,
          "Tree Theme font did not reach actual retained text layout");
}

void tree_retry() {
    std::size_t failures = 0;
    for (std::size_t failure = 0; failure < 16; ++failure) {
        Fixture f;
        const auto id = mount_tree(f, 32);
        const auto initial = f.services.tree().snapshot(id);
        hover(f, initial.rows[0]);
        sync(f);
        const runtime::Size viewport{640, 600};
        const runtime::Rect full{0, 0, 640, 600};
        const runtime::Rect clipped{0, 40, 640, 560};
        auto& tree = f.services.tree();
        auto& effects = f.services.rounded_effects();
        tree.synchronize_auxiliary_geometry(viewport, clipped);
        static_cast<void>(effects.compact(full));
        const std::vector expected(effects.packed_instances().begin(), effects.packed_instances().end());
        tree.synchronize_auxiliary_geometry(viewport, full);
        static_cast<void>(effects.compact(full));
        // MSVC's noexcept empty-vector constructor allocates a small iterator proxy.
        // Inject data-buffer failures rather than failures that must terminate by contract.
        ryn_test::allocation::begin(failure, 64);
        try {
            tree.synchronize_auxiliary_geometry(viewport, clipped);
        } catch (const std::bad_alloc&) {
            ++failures;
        }
        ryn_test::allocation::end();
        tree.synchronize_auxiliary_geometry(viewport, clipped);
        static_cast<void>(effects.compact(full));
        check(std::ranges::equal(effects.packed_instances(), expected),
              "Tree retry lost clipped effect updates after allocation failure");
        check(f.services.tree().snapshot(id).rows[0].component == initial.rows[0].component,
              "Tree allocation retry changed retained identity");
    }
    check(failures > 0, "Tree allocation retry probe did not throw");
    std::cout << "tree_retry allocation failures=" << failures << " minimum_request=64\n";
}

} // namespace

int main(int argc, char** argv) {
    std::set_terminate([] {
        ryn_test::allocation::end();
        std::fputs("Unexpected termination in state projection test\n", stderr);
        std::_Exit(3);
    });
    try {
        const std::string_view test = argc > 1 ? argv[1] : "all";
        check(test == "all" || test == "tree_local" || test == "tree_invalidation" || test == "tree_retry",
              "unknown state projection test");
        if (test == "tree_local" || test == "all") {
            tree_local();
        }
        if (test == "tree_invalidation" || test == "all") {
            tree_invalidation();
        }
        if (test == "tree_retry" || test == "all") {
            tree_retry();
        }
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
