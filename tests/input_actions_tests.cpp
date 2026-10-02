#include "support/input_fixture.hpp"
#include "theme/input_tokens.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

IconSource custom_icon() {
    return IconVector{
        {0, 0, 100, 100},
        {{IconColorRole::Primary, {IconMove{{10, 10}}, IconLine{{90, 10}}, IconLine{{50, 90}}, IconClose{}}},
         {IconColorRole::Secondary, {IconMove{{30, 30}}, IconLine{{70, 30}}, IconLine{{50, 65}}, IconClose{}}}}};
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void pointer(Fixture& f, InteractionId id, PointerAction action) {
    const auto bounds = f.nodes.require(f.buttons.interactions().require(id).node).bounds;
    f.buttons.pointer().dispatch({PointerIdentity::mouse(), action,
                                  action == PointerAction::move ? PointerButton::none : PointerButton::primary,
                                  bounds.x + bounds.width / 2, bounds.y + bounds.height / 2, 1});
}

void click(Fixture& f, InteractionId id) {
    pointer(f, id, PointerAction::down);
    pointer(f, id, PointerAction::up);
}

void clear_configuration() {
    Fixture f;
    InputRef reference;
    Signal<bool> disabled{true};
    Signal<IconSource> icon{IconSource{IconName::CloseOutlined}};
    int changes{};
    int clears{};
    int runs{};
    f.inputs.mount(Content{[&] {
        ++runs;
        Input(InputProps{}
                  .defaultValue(u8"value")
                  .ref(reference)
                  .allowClear(true)
                  .clearDisabled(disabled)
                  .clearIcon(icon)
                  .showCount()
                  .onChange([&](String value) {
                      require(value.empty(), "clear candidate not empty");
                      ++changes;
                  })
                  .onClear([&] { ++clears; }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto clear = f.buttons.interactions().declaration_order()[1];
    require(!f.buttons.interactions().require(clear).eligible &&
                f.inputs.count_text(mounted.component) == String{u8"5"},
            "clearDisabled changed value/count or allowed action");
    click(f, clear);
    require(changes == 0 && clears == 0, "disabled clear callback ran");
    disabled.set(false);
    const auto custom = custom_icon();
    icon.set(custom);
    f.synchronize();
    require(f.buttons.text().icon_snapshot(f.buttons.text().mounted_texts().front().component).source == custom,
            "clearIcon custom vector was not retained");
    pointer(f, clear, PointerAction::down);
    disabled.set(true);
    pointer(f, clear, PointerAction::up);
    require(changes == 0 && clears == 0, "clearDisabled did not cancel captured press");
    disabled.set(false);
    require(reference.focus({InputFocusCursor::End}), "clear focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 7}, stamp})), "clear preedit failed");
    click(f, clear);
    require(changes == 1 && clears == 1 && f.inputs.editors().require(mounted.editor).value().empty() &&
                !f.inputs.editors().require(mounted.editor).composition().active && f.platform.cancels == 1 &&
                f.inputs.count_text(mounted.component) == String{u8"0"} && runs == 1,
            "clear transaction/callback failed");
    f.synchronize();
    require(!f.buttons.interactions().require(clear).eligible, "empty clear remained eligible");
    Fixture removal;
    runtime::ComponentId victim;
    bool called{};
    removal.inputs.mount(Content{[&] {
        Input(InputProps{}
                  .defaultValue(u8"x")
                  .allowClear(true)
                  .onChange([&](String) { require(removal.buttons.destroy(victim), "clear removal failed"); })
                  .onClear([&] { called = true; }));
    }});
    removal.synchronize();
    victim = removal.inputs.mounted_inputs().front().component;
    click(removal, removal.buttons.interactions().declaration_order()[1]);
    require(called && removal.inputs.mounted_inputs().empty(), "clear callback did not survive onChange removal");
}

void password_actions() {
    Fixture f;
    InputRef reference;
    Signal<bool> toggle{false};
    Signal<bool> focusable{true};
    Signal<PasswordAction> action{PasswordAction::Hover};
    Signal<bool> visible{false};
    const auto custom = custom_icon();
    int requests{};
    int runs{};
    f.inputs.mount(Content{[&] {
        ++runs;
        Password(
            PasswordProps{}
                .defaultValue(u8"secret")
                .ref(reference)
                .visible(visible)
                .visibilityToggle(toggle)
                .toggleFocusable(focusable)
                .action(action)
                .iconRender([custom](bool visible) { return visible ? custom : IconSource{IconName::LockOutlined}; })
                .onVisibleChange([&](bool next) {
                    ++requests;
                    visible.set(next);
                }),
            InputPrefix{[] { Text(u8"prefix"); }}, InputSuffix{[] { Text(u8"suffix"); }});
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto toggler = f.buttons.interactions().declaration_order()[1];
    const auto text_scene = f.inputs.text_scene(mounted.component);
    require(!f.buttons.interactions().require(toggler).eligible, "hidden reactive toggle eligible");
    toggle.set(true);
    f.synchronize();
    require(f.buttons.interactions().require(toggler).tab_stop && reference.focus({InputFocusCursor::End}),
            "password default keyboard parking missing");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 7}, stamp})), "password preedit failed");
    pointer(f, toggler, PointerAction::move);
    require(visible.get() && requests == 1 && f.inputs.sessions().active() == stamp &&
                f.inputs.editors().require(mounted.editor).composition().active,
            "hover stole IME or did not toggle");
    require(f.buttons.text().icon_snapshot(f.buttons.text().mounted_texts()[1].component).source == custom,
            "Password custom vector renderer was not applied");
    pointer(f, toggler, PointerAction::move);
    click(f, toggler);
    require(requests == 1, "hover action toggled on repeated move or click");
    require(f.buttons.focus().request_focus(toggler, FocusModality::keyboard), "toggle keyboard focus failed");
    f.buttons.focus().dispatch({Key::space, KeyAction::down});
    focusable.set(false);
    f.buttons.focus().dispatch({Key::space, KeyAction::up});
    require(requests == 1, "toggle focusability change did not cancel keyboard press");
    require(!f.buttons.interactions().require(toggler).focusable &&
                !f.buttons.interactions().require(toggler).tab_stop &&
                !f.buttons.focus().request_focus(toggler, FocusModality::keyboard),
            "toggleFocusable ignored");
    action.set(PasswordAction::Click);
    click(f, toggler);
    require(!visible.get() && requests == 2, "reactive click action failed");
    toggle.set(false);
    f.synchronize();
    require(!f.buttons.interactions().require(toggler).eligible &&
                f.inputs.text_scene(mounted.component) == text_scene && runs == 1,
            "reactive visibility remounted editor or left action eligible");
    Fixture no_suffix;
    Signal<bool> no_suffix_toggle{true};
    no_suffix.inputs.mount(
        Content{[&] { Password(PasswordProps{}.defaultValue(u8"x").visibilityToggle(no_suffix_toggle)); }});
    no_suffix.synchronize();
    const auto no_suffix_input = no_suffix.inputs.mounted_inputs().front();
    const auto before = no_suffix.inputs.layout_snapshot(no_suffix_input.component).viewport.width;
    no_suffix_toggle.set(false);
    no_suffix.synchronize();
    require(no_suffix.inputs.layout_snapshot(no_suffix_input.component).viewport.width > before,
            "hidden password toggle kept suffix gap");
}

void search_clear_and_visuals() {
    Fixture f;
    Signal<String> value{String{u8"accepted"}};
    Signal<InputVariant> variant{InputVariant::Filled};
    Signal<IconSource> icon{IconSource{IconName::PlusOutlined}};
    Signal<bool> loading{false};
    InputRef reference;
    std::vector<SearchSource> sources;
    std::vector<String> searched;
    int clears{};
    int runs{};
    f.inputs.mount(Content{[&] {
        ++runs;
        Search(SearchProps{}
                   .value(value)
                   .ref(reference)
                   .variant(variant)
                   .allowClear(true)
                   .loading(loading)
                   .searchIcon(icon)
                   .onClear([&] { ++clears; })
                   .onChange([&](String) { value.set(String{u8"rejected"}); })
                   .onSearch([&](String text, SearchSource source) {
                       searched.push_back(std::move(text));
                       sources.push_back(source);
                   }),
               {}, InputPrefix{[] { Text(u8"P"); }}, InputSuffix{[] { Text(u8"S"); }});
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto clear = f.buttons.interactions().declaration_order()[1];
    const auto button = f.buttons.mounted_buttons().front();
    loading.set(true);
    click(f, clear);
    require(clears == 1 && sources == std::vector{SearchSource::Clear} && searched[0].empty() &&
                value.get() == String{u8"rejected"},
            "Search clear source used accepted rewrite or was blocked by loading");
    loading.set(false);
    click(f, button.interaction);
    require(sources.back() == SearchSource::Input && searched.back() == String{u8"rejected"},
            "Search button lost accepted value");
    require(f.buttons.focus().state().focused == button.interaction, "unfocused Search action lost pointer focus");
    require(reference.focus({InputFocusCursor::End}), "Search reference focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 7}, stamp})),
            "Search action preedit failed");
    click(f, button.interaction);
    require(f.buttons.focus().state().focused == mounted.interaction && f.inputs.sessions().active() == stamp &&
                f.inputs.editors().require(mounted.editor).composition().active,
            "Search pointer action stole Input focus or IME");
    const auto tokens = detail::derive_input_tokens(resolve_theme(ThemeConfig{}));
    const auto background = [&] {
        const auto range = f.buttons.button_scene().visual_range(button.scene);
        const auto& channels =
            f.buttons.button_scene()
                .instances()
                .at(range.first + static_cast<std::uint32_t>(component::ButtonVisualLayer::background))
                .color;
        return Color(channels[0], channels[1], channels[2], channels[3]);
    };
    pointer(f, button.interaction, PointerAction::move);
    f.synchronize();
    require(background() == tokens.colors.filled_hover_background, "Filled Search action hover background missing");
    pointer(f, button.interaction, PointerAction::down);
    f.synchronize();
    require(background() == Color(0, 0, 0, 0.15F), "Filled Search action pressed background missing");
    pointer(f, button.interaction, PointerAction::up);
    variant.set(InputVariant::Underlined);
    const auto custom = custom_icon();
    icon.set(custom);
    f.synchronize();
    require(f.buttons.text().icon_snapshot(f.buttons.text().mounted_texts().back().component).source == custom,
            "Search custom vector icon was not applied");
    require(f.buttons.snapshot(button.component).variant == ButtonVariant::Text &&
                f.inputs.variant(mounted.component) == InputVariant::Underlined && runs == 1,
            "Search variant/icon remounted or mismatched action");
    const auto root = f.nodes.require(button.node).bounds;
    require(root.width >= root.height, "icon-only Search action smaller than height");
}

void search_theme_dependencies() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int runs{};
    f.inputs.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  ++runs;
                  Search(SearchProps{}.variant(InputVariant::Filled).size(ControlSize::Small));
              }});
    }});
    f.synchronize();
    const auto input = f.inputs.mounted_inputs().front();
    const auto button = f.buttons.mounted_buttons().front();
    const auto rebuilds = f.buttons.scene_composer().diagnostics().rebuilds;
    const auto shapes = f.scene.text_state(f.inputs.text_scene(input.component)).counters().shape_count;
    ThemeConfig config;
    config.input.tokens.filled_background = Color(1, 0, 0, 0.25F);
    theme.set(config);
    f.synchronize();
    const auto range = f.buttons.button_scene().visual_range(button.scene);
    const auto& material = f.buttons.button_scene().instances().at(range.first + 1);
    require(material.color == std::array{1.0F, 0.0F, 0.0F, 0.25F} &&
                f.buttons.scene_composer().diagnostics().rebuilds == rebuilds &&
                f.scene.text_state(f.inputs.text_scene(input.component)).counters().shape_count == shapes && runs == 1,
            "Search action Input color dependency remounted or did not update");
    config.input.tokens.input_font_size_small = dp(24);
    theme.set(config);
    f.synchronize();
    const auto input_bounds = f.nodes.require(input.node).bounds;
    const auto button_bounds = f.nodes.require(button.node).bounds;
    require(input_bounds.height > 24 && std::abs(input_bounds.height - button_bounds.height) < 0.001F &&
                button_bounds.width >= button_bounds.height,
            "Small Search did not consume Input typography minimum");
}

} // namespace

int main() {
    try {
        clear_configuration();
        password_actions();
        search_clear_and_visuals();
        search_theme_dependencies();
        std::cout << "Native Input family actions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
