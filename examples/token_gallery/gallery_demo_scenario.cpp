#include "gallery_demo_scenario.hpp"
#include "gallery_demo_hash.hpp"
#include "component/select_component.hpp"
#include "component/flex_component.hpp"
#include "theme/theme_runtime.hpp"
#include "icons/bundled_icon_catalog.hpp"
#include "theme/input_tokens.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace rynui::example {
namespace {
using Json = GalleryDemoJson;
using namespace ryn;

Json count(std::uint64_t value) {
    return static_cast<std::int64_t>(value);
}

Json rect(runtime::Rect value) {
    return Json::Object{{"x", value.x}, {"y", value.y}, {"width", value.width}, {"height", value.height}};
}

Json color(Color value) {
    return Json::Array{value.red(), value.green(), value.blue(), value.alpha()};
}

bool owns(ryn::detail::ButtonComponentHost& app, runtime::ComponentId owner, runtime::ComponentId child) {
    for (auto current = std::optional{child}; current; current = app.components().parent(*current)) {
        if (*current == owner) {
            return true;
        }
    }
    return false;
}

runtime::Rect bounds(ryn::detail::ButtonComponentHost& app, runtime::ComponentId component) {
    const auto& node = app.nodes().require(app.components().root(component));
    auto result = node.bounds;
    result.x += node.translation.x;
    result.y += node.translation.y;
    return result;
}

input::Key key(std::string_view name) {
    const std::map<std::string_view, input::Key> keys{{"Tab", input::Key::tab},
                                                      {"Enter", input::Key::enter},
                                                      {"Space", input::Key::space},
                                                      {"ArrowLeft", input::Key::left},
                                                      {"ArrowRight", input::Key::right},
                                                      {"ArrowUp", input::Key::up},
                                                      {"ArrowDown", input::Key::down},
                                                      {"Escape", input::Key::escape},
                                                      {"Backspace", input::Key::backspace},
                                                      {"Delete", input::Key::delete_forward},
                                                      {"Home", input::Key::home},
                                                      {"End", input::Key::end},
                                                      {"PageUp", input::Key::page_up},
                                                      {"PageDown", input::Key::page_down},
                                                      {"a", input::Key::a},
                                                      {"c", input::Key::c},
                                                      {"v", input::Key::v},
                                                      {"x", input::Key::x},
                                                      {"z", input::Key::z},
                                                      {"y", input::Key::y}};
    const auto found = keys.find(name);
    if (found == keys.end()) {
        throw std::invalid_argument("unsupported native scenario key");
    }
    return found->second;
}

Json selected(const SelectSelection& value) {
    if (!value) {
        return {};
    }
    return std::visit(
        [](const auto& item) -> Json {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, String>) {
                return std::string{item.bytes()};
            } else {
                return item;
            }
        },
        value->storage());
}

class Observer final : public GalleryDemoInputBridge {
public:
    Observer(const GalleryDemoScenario& scenario, GalleryDemoNativeActions actions, std::filesystem::path output,
             Json provenance)
        : GalleryDemoInputBridge(scenario, std::move(actions)), output(std::move(output)),
          provenance(std::move(provenance)) {}

    std::filesystem::path output;
    Json provenance;
    Json::Array completed_steps;
    Json::Array completed_checkpoints;
    std::set<std::pair<std::uint32_t, std::uint32_t>> before_factory;
    std::vector<runtime::ComponentId> factory_roots;
    Json observed_initial_inputs;
    Json observed_inputs;
    std::vector<animation::AnimationScopeId> before_scopes;
    std::vector<animation::AnimationScopeId> sample_scopes;
    std::vector<runtime::ComponentId> sample_components;
    std::vector<input::InteractionId> sample_interactions;
    std::vector<input::TextInputOwnerId> sample_editors;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint64_t> component_pages;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint64_t> scope_pages;
    std::uint64_t sample_generation{};
    std::uint64_t last_readback{};
    Json readback_evidence;
    Json captured_paint;

    std::vector<runtime::ComponentId> all_components() {
        std::vector<runtime::ComponentId> result;
        const auto append = [&](auto&& self, runtime::ComponentId component) -> void {
            result.push_back(component);
            for (const auto child : actions.app.components().children(component)) {
                self(self, child);
            }
        };
        for (const auto root : actions.app.components().root_components()) {
            append(append, root);
        }
        return result;
    }

    void factory_boundary(std::string_view identity, bool begin) {
        if (identity != scenario.route.sample_identity) {
            return;
        }
        const auto components = all_components();
        if (begin) {
            sample_generation = actions.pages.snapshot().generation;
            before_scopes = actions.app.animations().scopes();
            before_factory.clear();
            factory_roots.clear();
            for (const auto component : components) {
                before_factory.emplace(component.index, component.generation);
            }
        } else {
            for (const auto component : components) {
                const auto parent = actions.app.components().parent(component);
                if (!before_factory.contains({component.index, component.generation}) &&
                    (!parent || before_factory.contains({parent->index, parent->generation}))) {
                    factory_roots.push_back(component);
                }
            }
            if (factory_roots.empty()) {
                throw std::runtime_error("actual sample factory created no retained roots");
            }
            for (const auto scope : actions.app.animations().scopes()) {
                if (std::ranges::find(before_scopes, scope) == before_scopes.end()) {
                    sample_scopes.push_back(scope);
                    scope_pages.emplace(std::pair{scope.index, scope.generation}, sample_generation);
                }
            }
            remember_components();
        }
    }

    void remember_components() {
        for (const auto component : all_components()) {
            if (std::ranges::any_of(factory_roots,
                                    [&](auto root) {
                                        return actions.app.components().contains(root) &&
                                               owns(actions.app, root, component);
                                    }) &&
                std::ranges::find(sample_components, component) == sample_components.end()) {
                sample_components.push_back(component);
                component_pages.emplace(std::pair{component.index, component.generation}, sample_generation);
            }
        }
        for (const auto interaction : actions.app.interactions().declaration_order()) {
            const auto* record = actions.app.interactions().find(interaction);
            if (record && sample_owns(record->component) &&
                std::ranges::find(sample_interactions, interaction) == sample_interactions.end()) {
                sample_interactions.push_back(interaction);
            }
        }
        for (const auto& input : actions.inputs.mounted_inputs()) {
            if (sample_owns(input.component) &&
                std::ranges::find(sample_editors, input.editor) == sample_editors.end()) {
                sample_editors.push_back(input.editor);
            }
        }
    }

    bool sample_owns(runtime::ComponentId component) const {
        return std::ranges::find(sample_components, component) != sample_components.end();
    }

    bool sample_interaction(std::optional<input::InteractionId> interaction) {
        return interaction && std::ranges::find(sample_interactions, *interaction) != sample_interactions.end();
    }

    Json lifecycle() {
        remember_components();
        Json::Array owners;
        Json::Array deadlines;
        std::size_t live{};
        const auto deadline = [&](std::string_view kind, std::size_t index,
                                  std::optional<animation::AnimationTime> value) {
            if (value) {
                deadlines.emplace_back(Json::Object{{"kind", kind},
                                                    {"owner", "sample-owner-" + std::to_string(index)},
                                                    {"time_us", value->count_microseconds()}});
            }
        };
        for (std::size_t index = 0; index < sample_components.size(); ++index) {
            const auto component = sample_components[index];
            const auto present = actions.app.components().contains(component);
            live += present;
            owners.emplace_back(Json::Object{
                {"identity", "sample-owner-" + std::to_string(index)},
                {"present", present},
                {"branch_active", present ? Json{actions.app.components().branch_active(component)} : Json{}},
                {"generation", count(component.generation)}});
            std::get<Json::Object>(owners.back().value)
                .emplace("page_generation", count(component_pages.at({component.index, component.generation})));
            for (const auto& button : actions.app.mounted_buttons()) {
                if (button.component == component) {
                    deadline("button-loading", index, actions.app.loading_deadline(component));
                }
            }
            for (const auto& input : actions.inputs.mounted_inputs()) {
                if (input.component == component) {
                    deadline("input-caret", index, actions.inputs.caret_deadline(component));
                }
            }
            for (const auto selector : actions.app.services().select().mounted()) {
                if (selector == component) {
                    deadline("select-motion", index, actions.app.services().select().motion_deadline(component));
                }
            }
        }
        Json::Array scopes;
        std::size_t live_scopes{};
        for (std::size_t index = 0; index < sample_scopes.size(); ++index) {
            const auto scope = sample_scopes[index];
            const auto present = actions.app.animations().contains(scope);
            live_scopes += present;
            const auto next = present ? actions.app.animations().next_deadline(scope) : std::nullopt;
            scopes.emplace_back(
                Json::Object{{"identity", "sample-scope-" + std::to_string(index)},
                             {"present", present},
                             {"active", present ? Json{count(actions.app.animations().size(scope))} : Json{}},
                             {"deadline_us", next ? Json{next->count_microseconds()} : Json{}},
                             {"generation", count(scope.generation)}});
            std::get<Json::Object>(scopes.back().value)
                .emplace("page_generation", count(scope_pages.at({scope.index, scope.generation})));
        }
        const auto session = actions.inputs.sessions().active();
        const auto sample_session =
            session.valid() && std::ranges::find(sample_editors, session.owner) != sample_editors.end();
        const auto active_interactions = std::ranges::count_if(
            sample_interactions, [&](auto interaction) { return actions.app.interactions().contains(interaction); });
        const auto pointer = actions.app.pointer().state(input::PointerIdentity::mouse());
        const auto capture = sample_interaction(pointer ? pointer->capture : std::nullopt);
        const auto focus = sample_interaction(actions.app.focus().state().focused);
        const auto next = actions.app.next_deadline();
        return Json::Object{{"owner_generation", count(sample_generation)},
                            {"owners", std::move(owners)},
                            {"animation_scopes", std::move(scopes)},
                            {"auxiliary_deadlines", std::move(deadlines)},
                            {"live_components", count(live)},
                            {"live_scopes", count(live_scopes)},
                            {"live_interactions", static_cast<std::int64_t>(active_interactions)},
                            {"focus_owned", focus},
                            {"capture_owned", capture},
                            {"session_owned", sample_session},
                            {"retired", sample_generation
                                            ? Json{live == 0 && live_scopes == 0 && active_interactions == 0 &&
                                                   !focus && !capture && !sample_session}
                                            : Json{}},
                            {"idle_observation", "not-sampled"},
                            {"deadline_coverage", "sample-animation-scopes-and-pilot-auxiliary"},
                            {"window_deadline_us", next ? Json{next->count_microseconds()} : Json{}},
                            {"shell_owner_present", actions.app.components().contains(actions.pages.snapshot().shell)},
                            {"shared_font_cache_policy", "window-owned-retained-not-page-leak"}};
    }

    bool contains(std::string_view name, runtime::ComponentId component) {
        if (name == "sample" || name == scenario.route.sample_identity) {
            return std::ranges::any_of(factory_roots, [&](auto root) {
                return actions.app.components().contains(root) && owns(actions.app, root, component);
            });
        }
        return owns(actions.app, target(name), component);
    }

    runtime::Rect target_bounds(std::string_view name) {
        if (name != "sample" && name != scenario.route.sample_identity) {
            return bounds(actions.app, target(name));
        }
        std::optional<runtime::Rect> area;
        for (const auto root : factory_roots) {
            if (!actions.app.components().contains(root)) {
                continue;
            }
            const auto value = bounds(actions.app, root);
            if (!area) {
                area = value;
                continue;
            }
            const auto right = std::max(area->x + area->width, value.x + value.width);
            const auto bottom = std::max(area->y + area->height, value.y + value.height);
            area->x = std::min(area->x, value.x);
            area->y = std::min(area->y, value.y);
            area->width = right - area->x;
            area->height = bottom - area->y;
        }
        if (!area) {
            throw std::runtime_error("sample factory has no live retained roots");
        }
        return *area;
    }

    [[nodiscard]] Json text_record(ryn::detail::TextSceneId scene, runtime::ComponentId component, bool icon = false) {
        auto result = gallery_demo_text_snapshot(
            actions.app.text().scene_service(), scene, bounds(actions.app, component),
            [&](auto font) {
                auto info = actions.font_info(font);
                gallery_demo_validate_font_source(scenario, info, icon);
                return info;
            },
            actions.nominal_glyph);
        std::get<Json::Object>(result.value)
            .emplace("owner",
                     Json::Object{{"index", count(component.index)}, {"generation", count(component.generation)}});
        return result;
    }

    [[nodiscard]] Json snapshot(std::string_view name) {
        const auto id = target(name);
        const auto page = actions.pages.snapshot();
        const runtime::Rect viewport{0, 0, static_cast<float>(actions.metrics().at("logical_width").number()),
                                     static_cast<float>(actions.metrics().at("logical_height").number())};
        const auto clip = actions.app.services().component_clip(id, {viewport.width, viewport.height}, viewport);
        const auto* focus = actions.app.focus().state().focused
                                ? actions.app.interactions().find(*actions.app.focus().state().focused)
                                : nullptr;
        const auto pointer = actions.app.pointer().state(input::PointerIdentity::mouse());
        const auto* capture =
            pointer && pointer->capture ? actions.app.interactions().find(*pointer->capture) : nullptr;
        Json::Object state{{"focused", focus && contains(name, focus->component)},
                           {"instance", std::to_string(page.generation) + ":" + std::to_string(id.index) + ":" +
                                            std::to_string(id.generation)},
                           {"captured", capture && contains(name, capture->component)},
                           {"branch_active", actions.app.components().branch_active(id)},
                           {"key_source", "static-semantic-target"},
                           {"value", nullptr},
                           {"proposed_value", nullptr},
                           {"disabled", nullptr},
                           {"loading", nullptr},
                           {"open", nullptr},
                           {"selection", nullptr}};
        const bool sample = name == "sample" || name == scenario.route.sample_identity;
        if (const auto found = actions.context->proposed.find(std::string{name});
            found != actions.context->proposed.end()) {
            state["proposed_value"] = found->second;
        }
        if (const auto found = actions.context->values.find(std::string{name});
            found != actions.context->values.end()) {
            state["value"] = found->second();
        }
        Json::Object tokens;
        Json::Array texts;
        Json::Array icons;
        for (const auto& button : actions.app.mounted_buttons()) {
            if (!contains(name, button.component)) {
                continue;
            }
            if (sample) {
                continue;
            }
            const auto button_state = actions.app.snapshot(button.component);
            state["disabled"] = button_state.disabled;
            state["loading"] = button_state.loading;
            state["hovered"] = button_state.hovered;
            state["pressed"] = button_state.pointer_pressed;
            const auto& theme = actions.app.components().theme_scope(button.component)->snapshot().button();
            const bool small = button_state.size == ControlSize::Small;
            const bool large = button_state.size == ControlSize::Large;
            tokens["source"] = "effective-component-theme";
            tokens["padding_inline"] = small   ? theme.padding_inline_small
                                       : large ? theme.padding_inline_large
                                               : theme.padding_inline;
            tokens["border_width"] = theme.border_width;
            tokens["border_radius"] = small   ? theme.border_radius_small
                                      : large ? theme.border_radius_large
                                              : theme.border_radius;
            tokens["font_size"] = small   ? theme.content_font_size_small
                                  : large ? theme.content_font_size_large
                                          : theme.content_font_size;
            tokens["background"] = color(button_state.presentation_background);
            tokens["border_color"] = color(button_state.presentation_border);
            tokens["color"] = color(button_state.presentation_foreground);
        }
        for (const auto& input : actions.inputs.mounted_inputs()) {
            if (!contains(name, input.component)) {
                continue;
            }
            const auto display = actions.inputs.display_snapshot(input.component);
            const auto layout = actions.inputs.layout_snapshot(input.component);
            if (!sample && std::holds_alternative<std::nullptr_t>(state["value"].value)) {
                state["value"] = std::string{actions.inputs.editors().require(input.editor).value()};
            }
            if (!sample) {
                state["disabled"] = actions.inputs.editors().require(input.editor).disabled();
                state["read_only"] = actions.inputs.editors().require(input.editor).read_only();
                state["display_value"] = std::string{display.text};
                state["focused"] = actions.inputs.focused(input.component);
                state["selection"] = Json::Array{count(display.selection.anchor), count(display.selection.caret)};
                state["selection_unit"] = "UTF-8-bytes";
                state["composing"] = display.composing;
                const auto session = actions.inputs.sessions().active();
                state["session_active"] = session.valid() && session.owner == input.editor;
                state["session_epoch"] =
                    session.valid() && session.owner == input.editor ? count(session.epoch) : Json{};
                const auto& input_tokens =
                    ryn::detail::derive_input_tokens(actions.app.components().theme_scope(input.component)->snapshot());
                const auto& size_tokens = input_tokens.size(actions.inputs.size(input.component));
                tokens["source"] = "effective-component-theme";
                tokens["padding_inline"] = size_tokens.padding_inline;
                tokens["padding_block"] = size_tokens.padding_block;
                tokens["border_width"] = input_tokens.border_width;
                tokens["border_radius"] = size_tokens.border_radius;
                tokens["font_size"] = size_tokens.font_size;
                tokens["line_height"] = size_tokens.line_height;
                tokens["editor_viewport"] = rect(layout.viewport);
                tokens["editor_clip"] = rect(layout.clip);
                tokens["editor_baseline"] = layout.baseline;
                tokens["scroll_offset"] = layout.scroll_offset;
                tokens["paint"] = gallery_demo_input_paint_snapshot(actions.inputs.paint_snapshot(input.component));
            }
            const auto layers = actions.inputs.text_layers(input.component);
            for (const auto& [kind, scene] :
                 std::array{std::pair{"editor-base", layers.base}, std::pair{"editor-selected", layers.selected},
                            std::pair{"editor-placeholder", layers.placeholder}}) {
                auto item = text_record(scene, input.component);
                std::get<Json::Object>(item.value).emplace("kind", kind);
                texts.push_back(std::move(item));
            }
        }
        for (const auto selector : actions.app.services().select().mounted()) {
            if (!sample && contains(name, selector)) {
                const auto select = actions.app.services().select().snapshot(selector);
                const auto inputs = actions.app.services().select().input_snapshot(selector);
                state["disabled"] = inputs.disabled;
                state["loading"] = inputs.loading;
                state["value"] = selected(select.value);
                state["open"] = select.visible;
                state["query"] = std::string{select.query.bytes()};
                tokens["paint"] = gallery_demo_input_paint_snapshot(actions.inputs.paint_snapshot(select.selector));
                tokens["popup_bounds"] = rect(select.popup);
                tokens["popup_scroll"] = select.scroll;
                state["active_option_index"] = select.active ? count(*select.active) : Json{};
                Json::Array keys;
                for (const auto& option : inputs.options) {
                    keys.push_back(selected(SelectSelection{option.value}));
                }
                state["option_keys"] = std::move(keys);
                const auto& token = actions.app.components().theme_scope(selector)->snapshot().select();
                tokens["select"] = Json::Object{{"option_height", token.metrics.option_height},
                                                {"option_padding_inline", token.metrics.option_padding_inline},
                                                {"option_padding_block", token.metrics.option_padding_block},
                                                {"popup_padding", token.metrics.popup_padding},
                                                {"border_radius", token.metrics.border_radius},
                                                {"font_size", token.typography.font_size},
                                                {"font_weight", count(token.typography.font_weight)},
                                                {"popup_background", color(token.colors.popup_background)},
                                                {"option_color", color(token.colors.option_color)}};
            }
        }
        for (const auto& text : actions.app.text().mounted_texts()) {
            if (!contains(name, text.component)) {
                continue;
            }
            bool is_icon{};
            try {
                const auto icon = actions.app.text().icon_snapshot(text.component);
                if (const auto bundled = icon.source.bundled_name(); bundled && !icon.layers.empty()) {
                    is_icon = true;
                    Json::Array layers;
                    const auto source_layers = ryn::detail::bundled_layers(*bundled);
                    for (std::size_t ordinal = 0; ordinal < icon.layers.size(); ++ordinal) {
                        auto layer = text_record(icon.layers[ordinal], text.component, true);
                        auto& fields = std::get<Json::Object>(layer.value);
                        fields.emplace("layer_ordinal", count(ordinal));
                        const auto& scalars =
                            actions.app.text().scene_service().text_state(icon.layers[ordinal]).shaped().scalars;
                        fields.emplace("codepoint", scalars.size() == 1 ? count(scalars.front().value) : Json{});
                        fields.emplace("role", ordinal < source_layers.size()
                                                   ? (source_layers[ordinal].secondary ? "secondary" : "primary")
                                                   : "unavailable");
                        layers.push_back(std::move(layer));
                    }
                    icons.emplace_back(
                        Json::Object{{"identity", std::string{ryn::detail::bundled_icon_entry(*bundled).name}},
                                     {"visible", icon.visible},
                                     {"angle", icon.angle_degrees},
                                     {"layers", std::move(layers)},
                                     {"bounds", rect(bounds(actions.app, text.component))}});
                }
            } catch (const std::out_of_range&) {
                // The existing read-only icon query rejects ordinary Text.
            }
            auto text_item = text_record(text.scene, text.component, is_icon);
            std::get<Json::Object>(text_item.value).emplace("kind", is_icon ? "icon-font" : "text");
            texts.push_back(std::move(text_item));
        }
        if (sample) {
            std::vector<runtime::ComponentId> anchor_roots;
            for (const auto root : factory_roots) {
                if (actions.app.components().contains(root)) {
                    anchor_roots.push_back(root);
                }
            }
            if (anchor_roots.empty()) {
                throw std::runtime_error("sample content anchor has no retained factory roots");
            }
            const auto host = actions.app.components().parent(anchor_roots.front());
            if (!host || std::ranges::any_of(
                             anchor_roots, [&](auto root) { return actions.app.components().parent(root) != host; })) {
                throw std::runtime_error("sample content anchor has no common retained host");
            }
            const auto* flex = std::as_const(actions.app.components()).state<ryn::detail::FlexComponentState>(*host);
            if (!flex) {
                throw std::runtime_error("sample retained host layout is unavailable");
            }
            const auto container = bounds(actions.app, *host);
            const auto padding = flex->model.padding;
            tokens["sample_anchor"] =
                Json::Object{{"source", "retained-host-layout"},
                             {"container", rect(container)},
                             {"content", rect({container.x + padding.left, container.y + padding.top,
                                               container.width - padding.left - padding.right,
                                               container.height - padding.top - padding.bottom})},
                             {"padding", Json::Object{{"left", padding.left},
                                                      {"top", padding.top},
                                                      {"right", padding.right},
                                                      {"bottom", padding.bottom}}}};
            Json::Object controls;
            Json::Object control_tokens;
            for (const auto& identity : input_targets()) {
                const auto id = actions.pages.target(identity);
                if (id.valid() && actions.app.components().contains(id)) {
                    const auto control = snapshot(identity);
                    controls.emplace(identity, control.at("state"));
                    control_tokens.emplace(identity, control.at("tokens"));
                }
            }
            state["controls"] = std::move(controls);
            tokens["controls"] = std::move(control_tokens);
        }
        return Json::Object{{"identity", name},
                            {"key", name == "sample" ? scenario.route.sample_identity : std::string{name}},
                            {"bounds", rect(target_bounds(name))},
                            {"clip", rect(clip)},
                            {"scroll", Json::Object{{"document_offset", page.document.offset},
                                                    {"document_extent", page.document.content_extent},
                                                    {"document_viewport", page.document.viewport_extent},
                                                    {"document_maximum_offset", page.document.maximum_offset}}},
                            {"state", std::move(state)},
                            {"text", std::move(texts)},
                            {"icons", std::move(icons)},
                            {"tokens", std::move(tokens)},
                            {"owner_generation", count(page.generation)},
                            {"scene_revision", count(actions.scene_revision())}};
    }

    void emit(std::string_view type, std::string_view step, Json receipt, Json::Array targets = {},
              Json::Array captures = {}) {
        Json::Object states;
        for (const auto& item : targets) {
            states.emplace(item.at("identity").string(), item.at("state"));
        }
        states.emplace("focused_target", semantic(actions.app.focus().state().focused));
        states.emplace("parent_policy", actions.context->parent_policy);
        states.emplace("session_active", actions.inputs.sessions().active().valid());
        const auto pointer = actions.app.pointer().state(input::PointerIdentity::mouse());
        states.emplace("capture_target", semantic(pointer ? pointer->capture : std::nullopt));
        Json::Object resources{{"metrics", actions.metrics()},
                               {"component_count", count(actions.app.components().component_count())},
                               {"callback_count", count(actions.context->callbacks.size())},
                               {"framebuffer_readback", !captures.empty()},
                               {"compositor_callback", nullptr},
                               {"presented", nullptr},
                               {"presentation_evidence", "not-observed-by-checkpoint-readback"},
                               {"input_setup", "WindowInputEvent::focus_gained"},
                               {"factory_root_count", count(factory_roots.size())},
                               {"actual_initial_inputs", observed_initial_inputs},
                               {"actual_inputs", observed_inputs},
                               {"lifecycle", lifecycle()},
                               {"readback", captures.empty() ? Json{} : readback_evidence}};
        resources.emplace("paint_stack", captures.empty() ? Json{} : captured_paint);
        Json::Object effective = actions.context->fixture.object();
        std::get<Json::Object>(effective.at("viewport").value).insert_or_assign("dpr", actions.metrics().at("dpr"));
        Json::Object consumed;
        for (const auto& [identity, props] : actions.context->consumed_props) {
            consumed.emplace(identity, props);
        }
        // Keep independently observed source inputs separately from the fixture.
        // The runner must validate these before accepting paired comparison.
        resources.emplace("consumed_props", std::move(consumed));
        const Json record = Json::Object{{"schema_version", 1},
                                         {"type", type},
                                         {"nonce", scenario.nonce},
                                         {"case", scenario.identity},
                                         {"step", step},
                                         {"adapter", "rynui-gallery-native-v1"},
                                         {"provenance", provenance},
                                         {"effective_inputs", std::move(effective)},
                                         {"receipt", std::move(receipt)},
                                         {"state", std::move(states)},
                                         {"callbacks", actions.context->callbacks},
                                         {"targets", std::move(targets)},
                                         {"captures", std::move(captures)},
                                         {"resources", std::move(resources)},
                                         {"completed_steps", completed_steps},
                                         {"completed_checkpoints", completed_checkpoints}};
        std::ofstream stream(output, std::ios::binary | std::ios::app);
        stream.exceptions(std::ios::badbit | std::ios::failbit);
        stream << record.dump() << '\n';
    }
};

} // namespace

GalleryDemoJson
gallery_demo_text_snapshot(const ryn::detail::TextSceneService& service, ryn::detail::TextSceneId scene,
                           ryn::runtime::Rect area,
                           const std::function<GalleryDemoJson(ryn::font::FontIdentity)>& font_info,
                           const std::function<std::uint32_t(ryn::font::FontIdentity, char32_t)>& nominal_glyph) {
    const auto& state = service.text_state(scene);
    const auto& measurement = state.measurement();
    Json::Array lines;
    for (const auto& line : measurement.lines) {
        lines.emplace_back(Json::Object{{"byte_start", count(line.byte_start)},
                                        {"byte_end", count(line.byte_end)},
                                        {"width", line.width},
                                        {"baseline", line.baseline},
                                        {"overflow", line.overflow}});
    }
    Json::Array runs;
    for (const auto& run : state.shaped().runs) {
        runs.emplace_back(Json::Object{{"byte_start", count(run.byte_start)},
                                       {"byte_end", count(run.byte_end)},
                                       {"right_to_left", run.right_to_left},
                                       {"font", font_info(run.font)}});
    }
    Json::Array glyphs;
    for (const auto& glyph : state.shaped().glyphs) {
        glyphs.emplace_back(Json::Object{{"glyph_id", count(glyph.glyph_id)},
                                         {"cluster", count(glyph.cluster)},
                                         {"advance_x", glyph.advance_x},
                                         {"advance_y", glyph.advance_y},
                                         {"offset_x", glyph.offset_x},
                                         {"offset_y", glyph.offset_y},
                                         {"font", font_info(glyph.font)}});
    }
    Json::Array missing;
    if (nominal_glyph) {
        for (const auto& scalar : state.shaped().scalars) {
            const auto value = scalar.value;
            // Same non-painting controls as text_engine.cpp; they influence
            // shaping without requiring their own nominal glyph.
            if (value == 0x00AD || value == 0x034F || value == 0x061C || value == 0x180E ||
                (value >= 0x200B && value <= 0x200F) || (value >= 0x202A && value <= 0x202E) ||
                (value >= 0x2060 && value <= 0x206F) || (value >= 0xFE00 && value <= 0xFE0F) || value == 0xFEFF ||
                (value >= 0xE0000 && value <= 0xE0FFF)) {
                continue;
            }
            const auto run = std::ranges::find_if(state.shaped().runs, [&](const auto& item) {
                return item.byte_start <= scalar.byte_start && scalar.byte_start < item.byte_end;
            });
            if (run != state.shaped().runs.end() && nominal_glyph(run->font, value) == 0) {
                missing.emplace_back(Json::Object{{"codepoint", count(value)},
                                                  {"byte_start", count(scalar.byte_start)},
                                                  {"byte_end", count(scalar.byte_end)},
                                                  {"selected_font", font_info(run->font)}});
            }
        }
    }
    Json::Array ink;
    const auto& primitive = service.primitive(scene);
    if (primitive.coverage.size() != primitive.instances.count) {
        throw std::runtime_error("glyph instance ownership coverage is unavailable");
    }
    for (std::uint32_t index = 0; index < primitive.instances.count; ++index) {
        const auto& instance = service.glyph_scene().instances().at(primitive.instances.first + index);
        ink.emplace_back(
            Json::Object{{"bounds", rect({instance.position_size[0], instance.position_size[1],
                                          instance.position_size[2], instance.position_size[3]})},
                         {"clip", Json::Array{instance.clip_bounds[0], instance.clip_bounds[1], instance.clip_bounds[2],
                                              instance.clip_bounds[3]}},
                         {"translation", Json::Array{instance.translation_opacity[0], instance.translation_opacity[1]}},
                         {"opacity", instance.translation_opacity[2]}});
        auto& item = std::get<Json::Object>(ink.back().value);
        const auto& coverage = primitive.coverage[index];
        item.emplace("byte_begin", count(coverage.byte_begin));
        item.emplace("byte_end", count(coverage.byte_end));
        item.emplace("line", count(coverage.line));
        item.emplace("advance_x", coverage.x);
        item.emplace("advance_width", coverage.width);
        item.emplace("color", Json::Array{instance.color[0], instance.color[1], instance.color[2], instance.color[3]});
        item.emplace("rotation_degrees", instance.transform.angle_degrees);
        item.emplace("instance_index", count(primitive.instances.first + index));
        item.emplace("packed_index", count(primitive.instances.first + index));
        item.emplace("atlas_uv",
                     Json::Array{instance.uv_rect[0], instance.uv_rect[1], instance.uv_rect[2], instance.uv_rect[3]});
        for (const auto& range : primitive.draw_ranges) {
            if (range.instances.first <= primitive.instances.first + index &&
                primitive.instances.first + index - range.instances.first < range.instances.count) {
                item.emplace("atlas_page", count(range.atlas_page));
                break;
            }
        }
        item.emplace("paint_opacity", instance.translation_opacity[2]);
        for (const auto& entry : service.atlas().entries()) {
            if (entry.uv.left == instance.uv_rect[0] && entry.uv.top == instance.uv_rect[1] &&
                entry.uv.right == instance.uv_rect[2] && entry.uv.bottom == instance.uv_rect[3] &&
                item.contains("atlas_page") && item.at("atlas_page").integer() == entry.page) {
                item.emplace("atlas_source", Json::Object{{"glyph_id", count(entry.key.glyph_id)},
                                                          {"font", font_info(entry.key.font)},
                                                          {"phase", count(static_cast<std::uint8_t>(entry.key.phase))},
                                                          {"bearing_x", entry.bearing_x},
                                                          {"bearing_y", entry.bearing_y},
                                                          {"padding", count(graphics::glyph_atlas_padding)},
                                                          {"display_scale", entry.display_scale},
                                                          {"coverage_width", count(entry.coverage_rect.width)},
                                                          {"coverage_height", count(entry.coverage_rect.height)}});
                break;
            }
        }
    }
    Json::Array groups;
    for (std::size_t line_index = 0; line_index < measurement.lines.size(); ++line_index) {
        const auto& line = measurement.lines[line_index];
        for (std::size_t index = line.cluster_begin; index < line.cluster_begin + line.cluster_count; ++index) {
            const auto& cluster = measurement.visual_clusters.at(index);
            Json::Array boxes;
            for (const auto& item : ink) {
                if (item.at("byte_begin").integer() == static_cast<std::int64_t>(cluster.byte_start) &&
                    item.at("byte_end").integer() == static_cast<std::int64_t>(cluster.byte_end)) {
                    auto box = item.at("bounds");
                    auto& fields = std::get<Json::Object>(box.value);
                    fields["x"] = box.at("x").number() + item.at("translation").array()[0].number();
                    fields["y"] = box.at("y").number() + item.at("translation").array()[1].number();
                    boxes.push_back(std::move(box));
                }
            }
            groups.emplace_back(Json::Object{{"byte_begin", count(cluster.byte_start)},
                                             {"byte_end", count(cluster.byte_end)},
                                             {"line", count(line_index)},
                                             {"advance", cluster.width},
                                             {"boxes", std::move(boxes)}});
        }
    }
    return Json::Object{
        {"index_unit", "UTF-8-bytes"},
        {"scene_identity", Json::Object{{"index", count(scene.index)}, {"generation", count(scene.generation)}}},
        {"content", std::string{state.content().bytes()}},
        {"display_content", std::string{state.display_content().bytes()}},
        {"layer_opacity", state.material().opacity},
        {"bounds", rect(area)},
        {"lines", std::move(lines)},
        {"font_runs", std::move(runs)},
        {"glyphs", std::move(glyphs)},
        {"ink_instances", std::move(ink)},
        {"semantic_groups", std::move(groups)},
        {"replacement_count", count(state.shaped().replacement_count)},
        {"replacement_count_unit", "invalid-UTF8-repairs"},
        {"font_coverage_available", static_cast<bool>(nominal_glyph)},
        {"missing_font_scalars", std::move(missing)},
        {"overflow", measurement.overflow},
        {"width", measurement.width},
        {"height", measurement.height},
        {"first_baseline", measurement.first_baseline}};
}

Json GalleryDemoInputBridge::query_inputs(std::span<const runtime::ComponentId> factory_roots, std::string_view step,
                                          bool initial) {
    Json::Object controls;
    Json::Object data;
    Json::Object slots;
    Json::Object themes;
    Json::Object slot_targets;
    Json::Object slot_composition;
    Json::Object contexts;
    Json::Object instances;
    Json::Object current;
    Json::Object aliases;
    Json::Array ordered;
    const auto generation = actions.pages.snapshot().generation;
    const auto sequence = ++input_query_sequence;
    const auto names = input_targets();
    const auto live = [&](runtime::ComponentId id) {
        return id.valid() && actions.app.components().contains(id) && actions.app.components().scope(id).active() &&
               actions.app.components().config_scope(id)->active() &&
               std::ranges::any_of(factory_roots, [&](auto root) {
                   return actions.app.components().contains(root) && owns(actions.app, root, id);
               });
    };
    const auto scope_token = [](auto& scopes, const auto& scope) {
        for (std::size_t i = 0; i < scopes.size(); ++i) {
            if (scopes[i].lock() == scope) {
                return std::to_string(i + 1);
            }
        }
        scopes.emplace_back(scope);
        return std::to_string(scopes.size());
    };
    std::vector<std::pair<std::string, runtime::ComponentId>> named;
    for (const auto& name : names) {
        const auto id = actions.pages.target(name);
        if (live(id)) {
            named.emplace_back(name, id);
        }
    }
    for (const auto& name : names) {
        ordered.emplace_back(name);
        const auto id = actions.pages.target(name);
        if (!live(id)) {
            instances.emplace(name, Json::Object{{"status", "missing"},
                                                 {"instance", ""},
                                                 {"owner_generation", count(generation)},
                                                 {"parent_target", ""},
                                                 {"slot", ""},
                                                 {"first_observed_query", 0},
                                                 {"reason", "no-live-host"}});
            continue;
        }
        const auto token =
            std::to_string(generation) + ":" + std::to_string(id.index) + ":" + std::to_string(id.generation);
        const auto [first, inserted] = input_first_observed.emplace(token, sequence);
        Json::Object instance{
            {"status", "mounted"}, {"instance", token}, {"owner_generation", count(generation)},
            {"parent_target", ""}, {"slot", ""},        {"first_observed_query", count(first->second)}};
        const auto* focused = actions.app.focus().state().focused
                                  ? actions.app.interactions().find(*actions.app.focus().state().focused)
                                  : nullptr;
        Json::Object live{{"focused", focused && semantic(actions.app.focus().state().focused) == name}};
        const auto config = actions.app.components().config_scope(id);
        const auto theme = actions.app.components().theme_scope(id);
        config->ensure_live();
        const auto& applied = config->snapshot();
        contexts.emplace(name,
                         Json::Object{{"config_scope", scope_token(input_config_scopes, config)},
                                      {"theme_scope", scope_token(input_theme_scopes, theme)},
                                      {"config_generation", count(config->generation())},
                                      {"theme_generation", count(theme->generation())},
                                      {"locale", applied.locale.effective.value()},
                                      {"format_locale", applied.format_locale.effective.value()},
                                      {"direction", applied.direction == TextDirection::RightToLeft ? "rtl" : "ltr"}});
        for (const auto& [parent_name, parent_id] : named) {
            if (parent_id == id || std::ranges::find(actions.app.services().select().mounted(), parent_id) ==
                                       actions.app.services().select().mounted().end()) {
                continue;
            }
            const auto parent = actions.app.services().select().input_snapshot(parent_id);
            const auto selector =
                actions.inputs.input_snapshot(actions.app.services().select().snapshot(parent_id).selector);
            for (const auto& [slot, root] :
                 {std::pair{"prefix", selector.prefix}, std::pair{"popup", parent.popup_root}}) {
                if (root.valid() && actions.app.components().contains(root) && owns(actions.app, root, id)) {
                    instance["parent_target"] = parent_name;
                    instance["slot"] = slot;
                }
            }
        }
        const auto& node = actions.app.nodes().require(actions.app.components().root(id));
        Json::Object actual;
        if (node.external_layout.width) {
            actual.emplace("width", *node.external_layout.width);
        }
        if (node.external_layout.margin.top || node.external_layout.margin.bottom) {
            if (node.external_layout.margin.top == node.external_layout.margin.bottom) {
                actual.emplace("marginBlock", node.external_layout.margin.top);
            } else {
                actual.emplace("marginTop", node.external_layout.margin.top);
                actual.emplace("marginBottom", node.external_layout.margin.bottom);
            }
        }
        Json::Object values;
        Json::Object content;
        themes.emplace(name, gallery_demo_applied_theme(actions.app, id));
        const auto size_name = [](ControlSize size) {
            return size == ControlSize::Small ? "small" : size == ControlSize::Large ? "large" : "middle";
        };
        const auto variant_name = [](InputVariant value) {
            return value == InputVariant::Filled       ? "filled"
                   : value == InputVariant::Borderless ? "borderless"
                   : value == InputVariant::Underlined ? "underlined"
                                                       : "outlined";
        };
        const auto status_name = [](InputStatus value) {
            return value == InputStatus::Warning ? "warning" : value == InputStatus::Error ? "error" : "";
        };
        // Choose by the actual mounted host identity, never the fixture's
        // claimed component type. A wrong declaration must be detectable.
        if (std::ranges::find(actions.app.mounted_buttons(), id, &ryn::detail::MountedButtonComponent::component) !=
            actions.app.mounted_buttons().end()) {
            const auto value = actions.app.snapshot(id);
            actual.emplace("component", "Button");
            actual.emplace("disabled", value.disabled);
            actual.emplace("loading", value.loading);
            actual.emplace("size", size_name(value.size));
            const auto type = value.type == ButtonType::Primary || value.type == ButtonType::Danger ? "primary"
                              : value.type == ButtonType::Text                                      ? "text"
                              : value.type == ButtonType::Dashed                                    ? "dashed"
                              : value.type == ButtonType::Link                                      ? "link"
                                                                                                    : "default";
            actual.emplace("type", type);
            actual.emplace("danger", value.type == ButtonType::Danger || value.color == ButtonColor::Danger);
            content.emplace("content", gallery_demo_retained_content(actions.app, id));
        } else if (std::ranges::find(actions.inputs.mounted_inputs(), id,
                                     &ryn::detail::MountedInputComponent::component) !=
                   actions.inputs.mounted_inputs().end()) {
            const auto value = actions.inputs.input_snapshot(id);
            actual.emplace("component", "Input");
            actual.emplace("disabled", value.disabled);
            actual.emplace("readOnly", value.read_only);
            actual.emplace("allowClear", value.allow_clear);
            actual.emplace("placeholder", std::string{value.placeholder.bytes()});
            actual.emplace("size", size_name(actions.inputs.size(id)));
            actual.emplace("status", status_name(actions.inputs.status(id)));
            actual.emplace("variant", variant_name(actions.inputs.variant(id)));
            content.emplace("prefix", gallery_demo_retained_content(actions.app, value.prefix));
            content.emplace("suffix", gallery_demo_retained_content(actions.app, value.suffix));
            if (value.max_length) {
                actual.emplace("maxLength", count(*value.max_length));
            }
            const auto mounted =
                std::ranges::find(actions.inputs.mounted_inputs(), id, &ryn::detail::MountedInputComponent::component);
            if (mounted == actions.inputs.mounted_inputs().end()) {
                throw std::runtime_error("declared Input does not resolve to an actual mounted editor");
            }
            const auto& editor = actions.inputs.editors().require(mounted->editor);
            values.emplace(value.controlled ? "value" : "defaultValue", std::string{editor.value()});
            live.emplace("value", std::string{editor.value()});
            const auto selection = editor.selection();
            live.emplace("selection", Json::Object{{"start", count(selection.anchor)},
                                                   {"end", count(selection.caret)},
                                                   {"unit", "UTF-8-bytes"}});
        } else if (std::ranges::find(actions.app.services().select().mounted(), id) !=
                   actions.app.services().select().mounted().end()) {
            const auto value = actions.app.services().select().input_snapshot(id);
            const auto state = actions.app.services().select().snapshot(id);
            if (value.popup_renderer && value.popup_root.valid() &&
                actions.app.components().contains(value.popup_root)) {
                aliases.emplace(name + ".popup",
                                Json::Object{{"parent_target", name},
                                             {"owner_generation", count(generation)},
                                             {"instance", std::to_string(generation) + ":" +
                                                              std::to_string(value.popup_root.index) + ":" +
                                                              std::to_string(value.popup_root.generation)}});
            }
            actual.emplace("component", "Select");
            actual.emplace("disabled", value.disabled);
            actual.emplace("loading", value.loading);
            actual.emplace("showSearch", value.show_search);
            actual.emplace("allowClear", value.allow_clear);
            actual.emplace("size", size_name(value.size));
            actual.emplace("status", status_name(value.status));
            actual.emplace("variant", variant_name(value.variant));
            actual.emplace("placeholder", std::string{value.placeholder.bytes()});
            actual.emplace("notFoundContent", std::string{value.not_found.bytes()});
            actual.emplace("listHeight", value.list_height);
            actual.emplace("direction", value.direction == TextDirection::RightToLeft ? "rtl" : "ltr");
            actual.emplace("virtual", value.virtual_list);
            actual.emplace("optionFilterProp", value.filter_field == SelectOptionField::Label ? "label" : "value");
            if (value.mode != SelectMode::Single) {
                actual.emplace("mode", value.mode == SelectMode::Tags ? "tags" : "multiple");
            }
            if (value.responsive_tags) {
                actual.emplace("maxTagCount", "responsive");
            } else if (value.max_tag_count != std::numeric_limits<std::size_t>::max()) {
                actual.emplace("maxTagCount", count(value.max_tag_count));
            }
            if (value.max_tag_text != std::numeric_limits<std::size_t>::max()) {
                actual.emplace("maxTagTextLength", count(value.max_tag_text));
            }
            if (value.max_count != std::numeric_limits<std::size_t>::max()) {
                actual.emplace("maxCount", count(value.max_count));
            }
            Json::Array separators;
            for (const auto& separator : value.separators) {
                separators.emplace_back(std::string{separator.bytes()});
            }
            actual.emplace("tokenSeparators", separators);
            actual.emplace("applied_renderers", Json::Object{{"option", value.option_renderer},
                                                             {"tag", value.tag_renderer},
                                                             {"popup", value.popup_renderer},
                                                             {"async_source", value.async_source}});
            const auto selector = actions.inputs.input_snapshot(state.selector);
            content.emplace("prefix", gallery_demo_retained_content(actions.app, selector.prefix, value.selected_root));
            Json::Object members;
            for (const auto& [slot, root] :
                 {std::pair{"prefix", selector.prefix}, std::pair{"popup", value.popup_root}}) {
                Json::Array targets;
                if (root.valid() && actions.app.components().contains(root)) {
                    for (const auto& [identity, child] : named) {
                        if (owns(actions.app, root, child)) {
                            targets.emplace_back(Json::Object{{"control", identity}});
                        }
                    }
                }
                members.emplace(slot, targets);
            }
            slot_targets.emplace(name, members);
            if (value.popup_body.valid() && actions.app.components().contains(value.popup_body) &&
                value.list_root.valid() && actions.app.components().contains(value.list_root)) {
                const std::array roots{value.popup_body};
                const auto tree = gallery_demo_retained_tree(actions.app, roots, named, value.list_root);
                slot_composition.emplace(name, Json::Object{{"popup", tree.at("roots").array().front()}});
            }
            live.emplace("open", state.visible);
            live.emplace("query", std::string{state.query.bytes()});
            if (value.controlled_open) {
                actual.emplace("open", state.visible);
            }
            Json::Array options;
            for (const auto& option : value.options) {
                options.emplace_back(Json::Object{{"value", selected(SelectSelection{option.value})},
                                                  {"label", std::string{option.label.bytes()}},
                                                  {"disabled", option.disabled},
                                                  {"key", std::string{option.key.bytes()}},
                                                  {"group", option.group}});
            }
            values.emplace("options", std::move(options));
            if (value.mode == SelectMode::Single) {
                live.emplace("value", selected(state.value));
                values.emplace(value.controlled_value ? "value" : "defaultValue", selected(state.value));
            } else {
                Json::Array selected_values;
                for (const auto& item : state.values) {
                    selected_values.push_back(selected(SelectSelection{item}));
                }
                live.emplace("value", selected_values);
                values.emplace(value.controlled_value ? "value" : "defaultValue", selected_values);
            }
        } else {
            throw std::runtime_error("native source input query has no declared component adapter");
        }
        if (const auto seed = values.find("defaultValue"); seed != values.end()) {
            const auto [saved, fresh] = input_uncontrolled_seeds.emplace(token, seed->second);
            seed->second = saved->second;
        }
        instances.emplace(name, std::move(instance));
        current.emplace(name, std::move(live));
        controls.emplace(name, std::move(actual));
        data.emplace(name, std::move(values));
        slots.emplace(name, std::move(content));
    }
    std::vector<runtime::ComponentId> live_roots;
    for (const auto root : factory_roots) {
        if (actions.app.components().contains(root) && actions.app.components().scope(root).active()) {
            live_roots.push_back(root);
        }
    }
    std::vector<std::pair<std::string, runtime::ComponentId>> feedback_targets;
    const auto collect_targets = [&](auto&& self, const Json& node) -> void {
        if (node.at("component").string() == "source-feedback") {
            const auto& identity = node.at("identity").string();
            const auto target = actions.pages.target("gallery.source-feedback." + identity);
            if (live(target)) {
                feedback_targets.emplace_back(identity, target);
            }
        }
        if (node.contains("children")) {
            for (const auto& child : node.at("children").array()) {
                self(self, child);
            }
        }
    };
    if (scenario.definition.contains("fixture")) {
        const auto& props = scenario.definition.at("fixture").at("props");
        if (props.contains("composition")) {
            collect_targets(collect_targets, props.at("composition"));
        }
    }
    auto composition = gallery_demo_retained_tree(actions.app, live_roots, named, {}, feedback_targets);
    Json::Object feedbacks;
    const auto collect_text = [&](auto&& self, const Json& node) -> void {
        if (node.at("component").string() == "source-feedback" &&
            !feedbacks.emplace(node.at("identity").string(), node.at("content")).second) {
            throw std::runtime_error("authored feedback identity is duplicated");
        }
        if (node.contains("children")) {
            for (const auto& child : node.at("children").array()) {
                self(self, child);
            }
        }
    };
    for (const auto& root : composition.at("roots").array()) {
        collect_text(collect_text, root);
    }
    return Json::Object{{"version", 1},
                        {"phase", initial ? "initial" : "settled"},
                        {"step", step},
                        {"query_seq", count(sequence)},
                        {"page_generation", count(generation)},
                        {"ordered_targets", std::move(ordered)},
                        {"instances", std::move(instances)},
                        {"contexts", std::move(contexts)},
                        {"current", std::move(current)},
                        {"aliases", std::move(aliases)},
                        {"slot_composition", std::move(slot_composition)},
                        {"source", "mounted hosts, applied editor state, retained slots/ThemeScope/layout"},
                        {"controls", std::move(controls)},
                        {"data", std::move(data)},
                        {"slots", std::move(slots)},
                        {"themes", std::move(themes)},
                        {"slot_targets", std::move(slot_targets)},
                        {"composition", std::move(composition)},
                        {"feedbacks", std::move(feedbacks)}};
}

GalleryDemoInputBridge::GalleryDemoInputBridge(const GalleryDemoScenario& value, GalleryDemoNativeActions ports)
    : scenario(value), actions(std::move(ports)) {}

std::vector<std::string> GalleryDemoInputBridge::input_targets() const {
    std::vector<std::string> result;
    if (!scenario.definition.contains("fixture")) {
        // The isolated bridge-routing contract does not request input evidence.
        for (const auto& name : scenario.definition.at("native").at("targets").array()) {
            result.push_back(name.string());
        }
        return result;
    }
    const auto& controls = scenario.definition.at("fixture").at("props").at("controls").object();
    const auto& slots = scenario.definition.at("fixture").at("slots").object();
    for (const auto& name : scenario.definition.at("native").at("targets").array()) {
        if (controls.contains(name.string())) {
            result.push_back(name.string());
        }
    }
    const auto add = [&](auto&& self, const std::string& name) -> void {
        if (std::ranges::find(result, name) != result.end()) {
            return;
        }
        result.push_back(name);
        if (const auto found = slots.find(name); found != slots.end()) {
            for (const auto key : {"prefix", "suffix", "popup"}) {
                if (found->second.contains(key) && std::holds_alternative<Json::Object>(found->second.at(key).value) &&
                    found->second.at(key).contains("control")) {
                    self(self, found->second.at(key).at("control").string());
                }
            }
        }
    };
    const auto visit = [&](auto&& self, const Json& node) -> void {
        if (node.at("component").string() == "control") {
            add(add, node.at("target").string());
        }
        if (node.contains("children")) {
            for (const auto& child : node.at("children").array()) {
                self(self, child);
            }
        }
    };
    visit(visit, scenario.definition.at("fixture").at("props").at("composition"));
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto found = slots.find(result[i]);
        if (found != slots.end()) {
            for (const auto key : {"prefix", "suffix", "popup"}) {
                if (found->second.contains(key) && std::holds_alternative<Json::Object>(found->second.at(key).value) &&
                    found->second.at(key).contains("control")) {
                    add(add, found->second.at(key).at("control").string());
                }
            }
        }
    }
    if (result.size() != controls.size()) {
        throw std::runtime_error("input query composition/slot identity set is incomplete");
    }
    return result;
}

runtime::ComponentId GalleryDemoInputBridge::target(std::string_view name) {
    auto owner = actions.pages.target(name == "sample" ? scenario.route.sample_identity : name);
    if (!owner.valid() && name.ends_with(".popup")) {
        const auto parent = actions.pages.target(name.substr(0, name.size() - 6));
        const auto& selectors = actions.app.services().select();
        if (std::ranges::find(selectors.mounted(), parent) != selectors.mounted().end()) {
            owner = selectors.input_snapshot(parent).popup_root;
        }
    }
    if (!owner.valid() || !actions.app.components().contains(owner)) {
        throw std::runtime_error("scenario semantic target is absent: " + std::string{name});
    }
    return owner;
}

std::string GalleryDemoInputBridge::semantic(std::optional<input::InteractionId> interaction) {
    if (!interaction) {
        return {};
    }
    const auto* record = actions.app.interactions().find(*interaction);
    if (!record) {
        return {};
    }
    std::string result{"shell-or-other"};
    runtime::ComponentId deepest;
    for (const auto& name : input_targets()) {
        const auto id = actions.pages.target(name);
        if (id.valid() && actions.app.components().contains(id) && actions.app.components().scope(id).active() &&
            actions.app.components().config_scope(id)->active() && owns(actions.app, id, record->component) &&
            (!deepest.valid() || owns(actions.app, deepest, id))) {
            result = name;
            deepest = id;
        }
    }
    return result;
}

Json GalleryDemoInputBridge::dispatch(const GalleryDemoAction& action) {
    auto& context = *actions.context;
    context.step = action.step;
    context.time_us = action.time_us;
    context.owner_generation = static_cast<std::int64_t>(actions.pages.snapshot().generation);
    actions.set_time(action.time_us);
    Json::Object receipt{{"dispatched", true},
                         {"hit", ""},
                         {"consumed", false},
                         {"layer", action.kind == "checkpoint" ? "observer" : "native-core-dispatch"}};
    const auto& params = action.parameters;
    if (action.kind.starts_with("pointer-")) {
        const auto owner = target(action.target);
        const auto area = bounds(actions.app, owner);
        const float x =
            area.x +
            area.width * static_cast<float>(params.contains("x_fraction") ? params.at("x_fraction").number() : .5);
        const float y =
            area.y +
            area.height * static_cast<float>(params.contains("y_fraction") ? params.at("y_fraction").number() : .5);
        const auto metrics = actions.metrics();
        const runtime::Rect viewport{0, 0, static_cast<float>(metrics.at("logical_width").number()),
                                     static_cast<float>(metrics.at("logical_height").number())};
        const auto clip = actions.app.services().component_clip(owner, {viewport.width, viewport.height}, viewport);
        if (x < clip.x || y < clip.y || x >= clip.x + clip.width || y >= clip.y + clip.height || x < 0 || y < 0 ||
            x >= viewport.width || y >= viewport.height) {
            throw std::runtime_error("pointer semantic target is outside its visible viewport/clip");
        }
        const auto hit = actions.app.hit_test().hit_test({x, y});
        receipt["hit"] = semantic(hit);
        const auto kind = action.kind == "pointer-move"   ? input::PointerAction::move
                          : action.kind == "pointer-down" ? input::PointerAction::down
                          : action.kind == "pointer-up"   ? input::PointerAction::up
                                                          : input::PointerAction::cancel;
        const auto button = kind == input::PointerAction::move || kind == input::PointerAction::cancel
                                ? input::PointerButton::none
                            : params.at("button").string() == "primary" ? input::PointerButton::primary
                                                                        : input::PointerButton::secondary;
        receipt["consumed"] =
            actions.pages.dispatch(input::PointerInputEvent{input::PointerIdentity::mouse(), kind, button, x, y});
    } else if (action.kind.starts_with("key-")) {
        input::KeyModifier modifiers = input::KeyModifier::none;
        for (const auto& item : params.at("modifiers").array()) {
            modifiers = modifiers | (item.string() == "shift"     ? input::KeyModifier::shift
                                     : item.string() == "control" ? input::KeyModifier::control
                                     : item.string() == "alt"     ? input::KeyModifier::alt
                                                                  : input::KeyModifier::meta);
        }
        receipt["hit"] = semantic(actions.app.focus().state().focused);
        receipt["consumed"] = actions.pages.dispatch(input::KeyboardInputEvent{
            key(params.at("key").string()), action.kind == "key-down" ? input::KeyAction::down : input::KeyAction::up,
            modifiers});
    } else if (action.kind == "text-commit" || action.kind == "composition") {
        receipt["hit"] = semantic(actions.app.focus().state().focused);
        auto session = actions.inputs.sessions().active();
        if (action.kind == "text-commit") {
            receipt["consumed"] = actions.pages.dispatch(
                input::TextCommitted{String::from_utf8(params.at("text").string()).value(), session});
        } else {
            receipt["layer"] = "native-composition-seam";
            const auto name = params.at("session").string();
            if (params.at("phase").string() == "start") {
                sessions.insert_or_assign(name, session);
            }
            const auto found = sessions.find(name);
            if (found == sessions.end()) {
                throw std::invalid_argument("composition refers to an unobserved session");
            }
            session = found->second;
            const auto text = String::from_utf8(params.at("text").string()).value();
            if (params.at("phase").string() == "commit") {
                receipt["consumed"] = actions.pages.dispatch(input::TextCommitted{text, session});
            } else {
                const auto start = params.at("selection_start").integer();
                const auto end = params.at("selection_end").integer();
                if (start < 0 || end < start) {
                    throw std::invalid_argument("invalid composition scalar range");
                }
                receipt["consumed"] = actions.pages.dispatch(
                    input::CompositionChanged{params.at("phase").string() == "cancel" ? String{} : text,
                                              {static_cast<std::size_t>(start), static_cast<std::size_t>(end - start)},
                                              session});
            }
        }
    } else if (action.kind == "wheel") {
        auto area = bounds(actions.app, target(action.target));
        const auto metrics = actions.metrics();
        const runtime::Rect viewport{0, 0, static_cast<float>(metrics.at("logical_width").number()),
                                     static_cast<float>(metrics.at("logical_height").number())};
        const auto clip =
            actions.app.nodes().content_clip(actions.app.components().root(target(action.target)), viewport);
        const auto right = std::min(area.x + area.width, clip.x + clip.width);
        const auto bottom = std::min(area.y + area.height, clip.y + clip.height);
        area.x = std::max(area.x, clip.x);
        area.y = std::max(area.y, clip.y);
        area.width = right - area.x;
        area.height = bottom - area.y;
        if (area.width <= 0 || area.height <= 0) {
            throw std::runtime_error("wheel semantic target has no visible intersection");
        }
        receipt["hit"] = action.target;
        receipt["consumed"] =
            actions.pages.dispatch(input::ScrollInputEvent{static_cast<float>(-params.at("delta_x").number() / 48),
                                                           static_cast<float>(-params.at("delta_y").number() / 48),
                                                           area.x + area.width / 2, area.y + area.height / 2});
    } else if (action.kind == "external-props") {
        const auto& values = params.at("values");
        gallery_demo_fields(values, {"parent_policy"});
        context.parent_policy = values.at("parent_policy").string();
        if (context.parent_policy != "accept" && context.parent_policy != "reject") {
            throw std::invalid_argument("unknown controlled parent policy");
        }
        receipt["layer"] = "external-props";
        receipt["consumed"] = true;
    } else if (action.kind == "advance-clock") {
        receipt["layer"] = "clock";
        receipt["consumed"] = true;
    } else if (action.kind == "navigate") {
        GalleryPageRoute route{GalleryPageKind::component,
                               "ant.component." + params.at("route").string(),
                               {},
                               params.at("sample_id").string()};
        if (params.at("route").string() == "overview") {
            route = {};
        }
        actions.pages.request(route);
        actions.pages.process();
        receipt["consumed"] = true;
    } else if (action.kind != "checkpoint") {
        throw std::invalid_argument("scenario action has no implemented native execution adapter: " + action.kind);
    }

    return receipt;
}

void run_gallery_demo_scenario(const GalleryDemoScenario& scenario, const std::filesystem::path& observations,
                               GalleryDemoJson provenance, GalleryDemoNativeActions actions) {
    gallery_demo_validate_font_chain(scenario);
    if (std::filesystem::exists(observations)) {
        throw std::invalid_argument("scenario observation output already exists");
    }
    std::filesystem::create_directories(observations.parent_path());
    actions.context->fixture = scenario.definition.at("fixture");
    actions.context->parent_policy = actions.context->fixture.at("parent_policy").string();
    actions.context->fault = scenario.fault;
    Observer observer{scenario, std::move(actions), observations, std::move(provenance)};
    const Json empty_receipt =
        Json::Object{{"dispatched", false}, {"hit", ""}, {"consumed", false}, {"layer", "observer"}};
    observer.emit("hello", "", empty_receipt);
    observer.actions.context->factory_boundary = [&](std::string_view identity, bool begin) {
        observer.factory_boundary(identity, begin);
    };

    struct BoundaryScope final {
        std::shared_ptr<GalleryDemoSampleContext> context;

        ~BoundaryScope() {
            context->factory_boundary = {};
        }
    } boundary_scope{observer.actions.context};

    observer.actions.pages.request(scenario.route);
    observer.actions.pages.process();
    if (observer.actions.pages.snapshot().failed) {
        throw std::runtime_error("scenario page construction failed: " + observer.actions.pages.last_error());
    }
    observer.actions.set_time(0);
    observer.actions.render();
    observer.sample_generation = observer.actions.pages.snapshot().generation;
    observer.remember_components();
    observer.observed_initial_inputs = observer.query_inputs(observer.factory_roots, "setup", true);
    observer.observed_inputs = observer.observed_initial_inputs;
    for (const auto& action : scenario.actions) {
        observer.remember_components();
        const auto receipt = observer.dispatch(action);
        observer.actions.render();
        observer.observed_inputs = observer.query_inputs(observer.factory_roots, action.step);
        observer.completed_steps.emplace_back(action.step);
        observer.emit("receipt", action.step, receipt);
        if (action.kind == "checkpoint") {
            const auto index = observer.completed_checkpoints.size();
            const auto& declaration = scenario.definition.at("checkpoints").array().at(index);
            Json::Array targets;
            for (const auto& identity : declaration.at("targets").array()) {
                targets.push_back(observer.snapshot(identity.string()));
            }
            const auto filename = "checkpoint-" + std::to_string(index) + ".bmp";
            const auto path = observations.parent_path() / filename;
            if (std::filesystem::exists(path)) {
                throw std::runtime_error("scenario checkpoint path already exists");
            }
            const auto revision = observer.actions.scene_revision();
            const auto metrics = observer.actions.metrics();
            const auto observe_paint = [&]() -> Json {
                if (!observer.actions.submitted_scene) {
                    return Json::Object{{"status", "unsupported"},
                                        {"reason", "actual submitter scene query unavailable"}};
                }
                auto stack = gallery_demo_paint_stack(observer.actions.app, observer.factory_roots,
                                                      observer.actions.submitted_scene());
                auto& stack_fields = std::get<Json::Object>(stack.value);
                for (auto& layer : std::get<Json::Array>(stack_fields.at("layers").value)) {
                    const auto& owner = layer.at("owner");
                    const runtime::ComponentId component{static_cast<std::uint32_t>(owner.at("index").integer()),
                                                         static_cast<std::uint32_t>(owner.at("generation").integer())};
                    Json::Array owners;
                    for (const auto& name : observer.input_targets()) {
                        const auto semantic_owner = observer.actions.pages.target(name);
                        if (semantic_owner.valid() && observer.actions.app.components().contains(semantic_owner) &&
                            owns(observer.actions.app, semantic_owner, component)) {
                            owners.emplace_back(name);
                        }
                    }
                    std::get<Json::Object>(layer.value).emplace("owner_targets", std::move(owners));
                }
                stack_fields.emplace("glyph_inventory", observer.snapshot(scenario.route.sample_identity).at("text"));
                stack_fields.erase("snapshot_digest");
                const auto bytes = stack.dump();
                stack_fields.emplace(
                    "snapshot_digest",
                    gallery_demo_sha256(std::span{reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()}));
                return stack;
            };
            auto paint_before = observe_paint();
            const auto evidence = observer.actions.capture(path);
            gallery_demo_validate_capture(evidence, observer.last_readback, revision, metrics,
                                          scenario.definition.at("fixture"), observer.provenance);
            if (!std::filesystem::is_regular_file(path) || observer.actions.scene_revision() != revision) {
                throw std::runtime_error("scenario current GPU checkpoint readback failed");
            }
            if (paint_before.dump() != observe_paint().dump()) {
                throw std::runtime_error("actual paint publication changed across checkpoint capture");
            }
            observer.last_readback = evidence.readback_id;
            const auto nonce = scenario.nonce + "-capture-" + std::to_string(index);
            observer.readback_evidence = Json::Object{{"run_nonce", scenario.nonce},
                                                      {"nonce", nonce},
                                                      {"checkpoint", action.step},
                                                      {"readback_id", count(evidence.readback_id)},
                                                      {"fence_id", count(evidence.fence_id)},
                                                      {"scene_revision", count(evidence.scene_revision)},
                                                      {"renderer_owner", count(evidence.renderer_owner)},
                                                      {"device_epoch", count(evidence.device_epoch)},
                                                      {"metrics", evidence.metrics},
                                                      {"source_channel_order", evidence.source_channel_order},
                                                      {"source_hash", evidence.source_hash},
                                                      {"binary_sha256", evidence.binary_sha256}};
            auto& raw = std::get<Json::Object>(observer.readback_evidence.value);
            raw.emplace("raw_alpha_available", evidence.raw_alpha_available);
            raw.emplace("raw_alpha_min", count(evidence.raw_alpha_min));
            raw.emplace("raw_alpha_max", count(evidence.raw_alpha_max));
            raw.emplace("raw_opaque_pixels", count(evidence.raw_opaque_pixels));
            raw.emplace("raw_total_pixels", count(evidence.raw_total_pixels));
            raw.emplace("raw_rgba_fnv1a64", evidence.raw_rgba_fnv1a64);
            raw.emplace("clear_rgba", evidence.clear_rgba);
            observer.captured_paint = std::move(paint_before);
            auto& paint = std::get<Json::Object>(observer.captured_paint.value);
            paint.emplace("capture_binding", observer.readback_evidence);
            observer.completed_checkpoints.emplace_back(action.step);
            const auto capture = Json::Object{{"path", filename},
                                              {"sha256", gallery_demo_file_sha256(path)},
                                              {"nonce", nonce},
                                              {"checkpoint", action.step},
                                              {"scene_revision", count(observer.actions.scene_revision())},
                                              {"width", metrics.at("pixel_width")},
                                              {"height", metrics.at("pixel_height")},
                                              {"dpr", metrics.at("dpr")},
                                              {"fence_completed", evidence.fence_completed},
                                              {"framebuffer_format", evidence.framebuffer_format},
                                              {"channel_order", evidence.channel_order},
                                              {"alpha", evidence.alpha},
                                              {"color_profile", evidence.color_profile}};
            observer.emit("checkpoint", action.step, receipt, std::move(targets), Json::Array{capture});
        }
    }
    observer.emit("complete", "", empty_receipt);
}

} // namespace rynui::example
