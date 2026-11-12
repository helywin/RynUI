#include "gallery_demo_scenario.hpp"
#include "gallery_demo_hash.hpp"
#include "component/select_component.hpp"
#include "theme/theme_runtime.hpp"
#include "icons/bundled_icon_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
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
        }
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
        if (factory_roots.empty()) {
            throw std::runtime_error("sample factory boundary was not observed");
        }
        auto area = bounds(actions.app, factory_roots.front());
        for (const auto root : factory_roots) {
            const auto value = bounds(actions.app, root);
            const auto right = std::max(area.x + area.width, value.x + value.width);
            const auto bottom = std::max(area.y + area.height, value.y + value.height);
            area.x = std::min(area.x, value.x);
            area.y = std::min(area.y, value.y);
            area.width = right - area.x;
            area.height = bottom - area.y;
        }
        return area;
    }

    [[nodiscard]] Json text_record(ryn::detail::TextSceneId scene, runtime::ComponentId component) {
        return gallery_demo_text_snapshot(actions.app.text().scene_service(), scene, bounds(actions.app, component),
                                          actions.font_info);
    }

    [[nodiscard]] Json snapshot(std::string_view name) {
        const auto id = target(name);
        const auto page = actions.pages.snapshot();
        const runtime::Rect viewport{0, 0, static_cast<float>(actions.metrics().at("logical_width").number()),
                                     static_cast<float>(actions.metrics().at("logical_height").number())};
        const auto clip = actions.app.nodes().content_clip(actions.app.components().root(id), viewport);
        Json::Object state{{"focused", semantic(actions.app.focus().state().focused) == name},
                           {"value", nullptr},
                           {"proposed_value", nullptr},
                           {"disabled", false},
                           {"loading", false},
                           {"open", false},
                           {"selection", Json::Array{0, 0}}};
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
            const auto button_state = actions.app.snapshot(button.component);
            state["disabled"] = button_state.disabled;
            state["loading"] = button_state.loading;
            state["hovered"] = button_state.hovered;
            state["pressed"] = button_state.pointer_pressed;
            const auto& theme = actions.app.components().theme_scope(button.component)->snapshot().button();
            const bool small = button_state.size == ControlSize::Small;
            tokens["padding_inline"] = small ? theme.padding_inline_small : theme.padding_inline;
            tokens["border_width"] = theme.border_width;
            tokens["border_radius"] = small ? theme.border_radius_small : theme.border_radius;
            tokens["font_size"] = small ? theme.content_font_size_small : theme.content_font_size;
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
            if (std::holds_alternative<std::nullptr_t>(state["value"].value)) {
                state["value"] = std::string{actions.inputs.editors().require(input.editor).value()};
            }
            state["disabled"] = actions.inputs.editors().require(input.editor).disabled();
            state["read_only"] = actions.inputs.editors().require(input.editor).read_only();
            state["display_value"] = std::string{display.text};
            state["focused"] = actions.inputs.focused(input.component);
            state["selection"] = Json::Array{count(display.selection.anchor), count(display.selection.caret)};
            state["selection_unit"] = "UTF-8-bytes";
            state["composing"] = display.composing;
            tokens["editor_viewport"] = rect(layout.viewport);
            tokens["editor_clip"] = rect(layout.clip);
            tokens["editor_baseline"] = layout.baseline;
            tokens["scroll_offset"] = layout.scroll_offset;
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
            if (contains(name, selector)) {
                const auto select = actions.app.services().select().snapshot(selector);
                const auto inputs = actions.app.services().select().input_snapshot(selector);
                state["disabled"] = inputs.disabled;
                state["loading"] = inputs.loading;
                state["value"] = selected(select.value);
                state["open"] = select.visible;
                state["query"] = std::string{select.query.bytes()};
                tokens["popup_bounds"] = rect(select.popup);
                tokens["popup_scroll"] = select.scroll;
                state["active_option_index"] = select.active ? count(*select.active) : Json{};
            }
        }
        for (const auto& text : actions.app.text().mounted_texts()) {
            if (!contains(name, text.component)) {
                continue;
            }
            auto text_item = text_record(text.scene, text.component);
            bool is_icon{};
            try {
                const auto icon = actions.app.text().icon_snapshot(text.component);
                if (const auto bundled = icon.source.bundled_name(); bundled && !icon.layers.empty()) {
                    is_icon = true;
                    icons.emplace_back(
                        Json::Object{{"identity", std::string{ryn::detail::bundled_icon_entry(*bundled).name}},
                                     {"visible", icon.visible},
                                     {"angle", icon.angle_degrees},
                                     {"bounds", rect(bounds(actions.app, text.component))}});
                }
            } catch (const std::out_of_range&) {
                // The existing read-only icon query rejects ordinary Text.
            }
            std::get<Json::Object>(text_item.value).emplace("kind", is_icon ? "icon-font" : "text");
            texts.push_back(std::move(text_item));
        }
        return Json::Object{{"identity", name},
                            {"key", ""},
                            {"bounds", rect(target_bounds(name))},
                            {"clip", rect(clip)},
                            {"scroll", Json::Object{{"document_offset", page.document.offset},
                                                    {"document_extent", page.document.content_extent}}},
                            {"state", std::move(state)},
                            {"text", std::move(texts)},
                            {"icons", std::move(icons)},
                            {"tokens", std::move(tokens)},
                            {"owner_generation", count(page.generation)},
                            {"scene_revision", count(actions.scene_revision())}};
    }

    Json applied_inputs() {
        Json::Object controls;
        Json::Object data;
        const auto& requested = actions.context->fixture.at("props").at("controls").object();
        for (const auto& [name, declaration] : requested) {
            const auto id = target(name);
            Json::Object actual{{"width", target_bounds(name).width}};
            Json::Object values;
            const auto size_name = [](ControlSize size) {
                return size == ControlSize::Small ? "small" : size == ControlSize::Large ? "large" : "middle";
            };
            if (declaration.at("component").string() == "Button") {
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
            } else if (declaration.at("component").string() == "Input") {
                const auto value = actions.inputs.input_snapshot(id);
                actual.emplace("component", "Input");
                actual.emplace("disabled", value.disabled);
                actual.emplace("readOnly", value.read_only);
                actual.emplace("allowClear", value.allow_clear);
                actual.emplace("placeholder", std::string{value.placeholder.bytes()});
                actual.emplace("size", size_name(actions.inputs.size(id)));
                if (value.max_length) {
                    actual.emplace("maxLength", count(*value.max_length));
                }
                const auto mounted = std::ranges::find(actions.inputs.mounted_inputs(), id,
                                                       &ryn::detail::MountedInputComponent::component);
                if (mounted == actions.inputs.mounted_inputs().end()) {
                    throw std::runtime_error("declared Input does not resolve to an actual mounted editor");
                }
                values.emplace(value.controlled ? "value" : "defaultValue",
                               std::string{actions.inputs.editors().require(mounted->editor).value()});
            } else if (declaration.at("component").string() == "Select") {
                const auto value = actions.app.services().select().input_snapshot(id);
                const auto state = actions.app.services().select().snapshot(id);
                actual.emplace("component", "Select");
                actual.emplace("disabled", value.disabled);
                actual.emplace("loading", value.loading);
                actual.emplace("showSearch", value.show_search);
                actual.emplace("allowClear", value.allow_clear);
                if (value.controlled_open) {
                    actual.emplace("open", state.visible);
                }
                Json::Array options;
                for (const auto& option : value.options) {
                    options.emplace_back(Json::Object{{"value", selected(SelectSelection{option.value})},
                                                      {"label", std::string{option.label.bytes()}},
                                                      {"disabled", option.disabled},
                                                      {"group", option.group}});
                }
                values.emplace("options", std::move(options));
                values.emplace(value.controlled_value ? "value" : "defaultValue", selected(state.value));
            } else {
                throw std::runtime_error("native source input query has no declared component adapter");
            }
            controls.emplace(name, std::move(actual));
            data.emplace(name, std::move(values));
        }
        return Json::Object{{"controls", std::move(controls)}, {"data", std::move(data)}};
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
                               {"compositor_callback", false},
                               {"presented", false},
                               {"input_setup", "WindowInputEvent::focus_gained"},
                               {"factory_root_count", count(factory_roots.size())},
                               {"actual_initial_inputs", observed_initial_inputs}};
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

GalleryDemoJson gallery_demo_text_snapshot(const ryn::detail::TextSceneService& service, ryn::detail::TextSceneId scene,
                                           ryn::runtime::Rect area,
                                           const std::function<GalleryDemoJson(ryn::font::FontIdentity)>& font_info) {
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
    Json::Array ink;
    const auto& primitive = service.primitive(scene);
    for (std::uint32_t index = 0; index < primitive.instances.count; ++index) {
        const auto& instance = service.glyph_scene().instances().at(primitive.instances.first + index);
        ink.emplace_back(
            Json::Object{{"bounds", rect({instance.position_size[0], instance.position_size[1],
                                          instance.position_size[2], instance.position_size[3]})},
                         {"clip", Json::Array{instance.clip_bounds[0], instance.clip_bounds[1], instance.clip_bounds[2],
                                              instance.clip_bounds[3]}},
                         {"translation", Json::Array{instance.translation_opacity[0], instance.translation_opacity[1]}},
                         {"opacity", instance.translation_opacity[2]}});
    }
    return Json::Object{{"index_unit", "UTF-8-bytes"},
                        {"content", std::string{state.content().bytes()}},
                        {"display_content", std::string{state.display_content().bytes()}},
                        {"bounds", rect(area)},
                        {"lines", std::move(lines)},
                        {"font_runs", std::move(runs)},
                        {"glyphs", std::move(glyphs)},
                        {"ink_instances", std::move(ink)},
                        {"replacement_count", count(state.shaped().replacement_count)},
                        {"overflow", measurement.overflow},
                        {"width", measurement.width},
                        {"height", measurement.height},
                        {"first_baseline", measurement.first_baseline}};
}

GalleryDemoInputBridge::GalleryDemoInputBridge(const GalleryDemoScenario& value, GalleryDemoNativeActions ports)
    : scenario(value), actions(std::move(ports)) {}

runtime::ComponentId GalleryDemoInputBridge::target(std::string_view name) {
    const auto owner = actions.pages.target(name == "sample" ? scenario.route.sample_identity : name);
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
    for (const auto& name : scenario.definition.at("native").at("targets").array()) {
        const auto id = actions.pages.target(name.string());
        if (id.valid() && owns(actions.app, id, record->component)) {
            return name.string();
        }
    }
    return "shell-or-other";
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
    observer.actions.set_time(0);
    observer.actions.render();
    observer.observed_initial_inputs = observer.applied_inputs();
    for (const auto& action : scenario.actions) {
        const auto receipt = observer.dispatch(action);
        observer.actions.render();
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
            if (std::filesystem::exists(path) || !observer.actions.capture(path)) {
                throw std::runtime_error("scenario current GPU checkpoint readback failed");
            }
            const auto metrics = observer.actions.metrics();
            observer.completed_checkpoints.emplace_back(action.step);
            const auto capture = Json::Object{{"path", filename},
                                              {"sha256", gallery_demo_file_sha256(path)},
                                              {"nonce", scenario.nonce + "-capture-" + std::to_string(index)},
                                              {"checkpoint", action.step},
                                              {"scene_revision", count(observer.actions.scene_revision())},
                                              {"width", metrics.at("pixel_width")},
                                              {"height", metrics.at("pixel_height")},
                                              {"dpr", metrics.at("dpr")},
                                              {"fence_completed", true},
                                              {"framebuffer_format", "RGBA8_UNORM"},
                                              {"channel_order", "RGBA"},
                                              {"alpha", "premultiplied"},
                                              {"color_profile", "native-encoded-SDR"}};
            observer.emit("checkpoint", action.step, receipt, std::move(targets), Json::Array{capture});
        }
    }
    observer.emit("complete", "", empty_receipt);
}

} // namespace rynui::example
