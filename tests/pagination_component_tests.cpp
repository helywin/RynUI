#include "component/pagination_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void click(Fixture& f, runtime::ComponentId wrapper) {
    const auto& node = f.nodes.require(f.services.components().root(wrapper));
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       node.bounds.x + node.translation.x + node.bounds.width / 2,
                                       node.bounds.y + node.translation.y + node.bounds.height / 2});
    }
    f.synchronize(600, {0, 0, 600, 240});
}

void controlled() {
    Fixture f;
    Signal<std::size_t> total{200};
    Signal<std::size_t> page{5};
    Signal<std::size_t> size{10};
    Signal<bool> disabled{false};
    bool echo{};
    int runs{};
    std::vector<std::size_t> requests;
    f.services.mount(Content{[&] {
        ++runs;
        Pagination(PaginationProps{}.total(total).current(page).pageSize(size).disabled(disabled).onChange(
            [&](auto next, auto per_page) {
                check(per_page == size.get(), "callback pageSize");
                requests.push_back(next);
                if (echo) {
                    page.set(next);
                }
            }));
    }});
    f.synchronize(600, {0, 0, 600, 240});
    const auto id = f.services.pagination().mounted()[0];
    const auto original_count = f.services.components().component_count();
    auto s = f.services.pagination().snapshot(id);
    check(s.pages == 20 && s.current == 5 && s.items.size() <= 11, "initial pagination");
    click(f, s.items.back());
    check(requests == std::vector<std::size_t>{6} && f.services.pagination().snapshot(id).current == 5,
          "controlled refusal");
    echo = true;
    click(f, s.items.back());
    check(f.services.pagination().snapshot(id).current == 6, "controlled echo");
    disabled.set(true);
    click(f, s.items.front());
    check(requests.size() == 2, "disabled callback");
    disabled.set(false);
    total.set(12);
    f.synchronize(600, {0, 0, 600, 240});
    check(f.services.pagination().snapshot(id).current == 2 && requests.size() == 2, "shrink clamp callback");
    size.set(20);
    total.set(0);
    f.synchronize(600, {0, 0, 600, 240});
    s = f.services.pagination().snapshot(id);
    check(s.pages == 1 && s.current == 1 && s.items.size() == 3 && runs == 1 &&
              f.services.components().component_count() == original_count,
          "retained zero pagination");
    total.set(std::numeric_limits<std::size_t>::max());
    page.set(std::numeric_limits<std::size_t>::max());
    f.synchronize(600, {0, 0, 600, 240});
    s = f.services.pagination().snapshot(id);
    check(s.current == s.pages && s.items.size() <= 11, "overflow arithmetic");
    check(f.services.destroy(id), "destroy failed");
    f.synchronize();
    check(f.services.pagination().mounted().empty() && f.services.interactions().size() == 0 &&
              f.services.components().component_count() == 0 && f.scene.size() == 0,
          "Pagination leaked resources");
}

void keyboard_and_theme() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<std::size_t> total{30};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme),
              ThemeContent{[&] { Pagination(PaginationProps{}.total(total).hideOnSinglePage()); }});
    }});
    f.synchronize(600, {0, 0, 600, 240});
    const auto id = f.services.pagination().mounted()[0];
    auto s = f.services.pagination().snapshot(id);
    click(f, s.items[2]);
    check(f.services.pagination().snapshot(id).current == 2, "uncontrolled click");
    for (auto action : {KeyAction::down, KeyAction::up}) {
        f.services.focus().dispatch({Key::enter, action});
    }
    f.synchronize(600, {0, 0, 600, 240});
    check(f.services.pagination().snapshot(id).current == 2, "same page keyboard changed page");
    s = f.services.pagination().snapshot(id);
    const auto next_wrapper = s.items.back();
    std::optional<InteractionId> next;
    for (auto hit : f.services.interactions().declaration_order()) {
        for (auto ancestor = std::optional{f.services.interactions().require(hit).component}; ancestor; ancestor = f.services.components().parent(*ancestor)) {
            if (*ancestor == next_wrapper) { next = hit; break; }
        }
    }
    check(next && f.services.focus().request_focus(*next, FocusModality::keyboard), "focus next page");
    for (auto action : {KeyAction::down, KeyAction::up}) { f.services.focus().dispatch({Key::space, action}); }
    f.synchronize(600, {0, 0, 600, 240});
    check(f.services.pagination().snapshot(id).current == 3, "keyboard page navigation");
    ThemeConfig custom;
    custom.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    custom.pagination.tokens.item_size = dp(28);
    custom.pagination.tokens.active = Color::rgba8(255, 0, 0);
    theme.set(custom);
    f.synchronize(600, {0, 0, 600, 240});
    check(f.nodes.require(f.services.components().root(s.items[0])).bounds.height == 28, "Pagination metrics override");
    check(resolve_theme(custom).pagination().colors.active == *custom.pagination.tokens.active,
          "Pagination color override");
    const auto inherited = resolve_theme({}, &f.services.components().theme_scope(id)->snapshot());
    check(inherited.pagination().metrics.item_size == 28, "nested Pagination inheritance");
    total.set(0);
    f.synchronize();
    check(f.services.pagination().snapshot(id).hidden, "hideOnSinglePage");
    f.services.dispose();
}

void token_contracts_and_callback_cleanup() {
    const auto token = resolve_theme();
    check(token.pagination().metrics.item_size == token.map().control_height && token.pagination().metrics.item_size_small == token.map().control_height_small && token.pagination().colors.active == token.map().color_primary, "locked Pagination token defaults");
    const auto scope = theme_runtime::ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto color_subscription = scope->capture([&](auto phase) { check(phase == theme_runtime::DirtyPhase::paint_material, "Pagination color phase"); ++colors; }, [&] { static_cast<void>(scope->pagination_colors()); });
    auto metric_subscription = scope->capture([&](auto) { ++metrics; }, [&] { static_cast<void>(scope->pagination_metrics()); });
    ThemeConfig config;
    config.pagination.tokens.active = Color::rgba8(2, 3, 4);
    check(scope->update(config) && colors == 1 && metrics == 0 && scope->snapshot().identity() != token.identity(), "Pagination token isolation/identity");
    check(scope->snapshot().diagnostic_json().find("\"pagination\"") != std::string::npos, "Pagination token serialization");
    const auto identity = scope->snapshot().identity();
    config.pagination.tokens.item_size = dp(-1);
    bool rejected{};
    try { scope->update(config); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && scope->snapshot().identity() == identity, "Pagination token validation rollback");
    Fixture f;
    runtime::ComponentId id;
    f.services.mount(Content{[&] { Pagination(PaginationProps{}.total(std::size_t{30}).onChange([&](auto, auto) { f.services.destroy(id); })); }});
    id = f.services.pagination().mounted()[0];
    f.synchronize(600, {0, 0, 600, 240});
    click(f, f.services.pagination().snapshot(id).items.back());
    check(f.services.pagination().mounted().empty() && f.services.interactions().size() == 0, "Pagination destruction callback leaked");
}
} // namespace

int main() {
    try {
        controlled();
        keyboard_and_theme();
        token_contracts_and_callback_cleanup();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
