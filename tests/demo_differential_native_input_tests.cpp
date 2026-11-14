#include "gallery_demo_scenario.hpp"
#include "component/select_component.hpp"
#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <iostream>
#include <array>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace rynui::example;
using Json = GalleryDemoJson;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GalleryPagesHost pages{surfaces, inputs, frames};
    runtime::Size viewport{1440, 900};

    Fixture() {
        pages.set_locale(locales::zh_CN);
        pages.mount(viewport);
        pages.dispatch(input::WindowInputEvent{input::WindowInputAction::focus_gained});
        sync();
    }

    void sync() {
        require(pages.synchronize(viewport) && !pages.snapshot().failed, "production Gallery sync failed");
    }

    void show(const GalleryDemoScenario& scenario) {
        require(pages.request(scenario.route), "production sample request failed");
        pages.process();
        sync();
    }
};

GalleryDemoScenario scenario(std::string component, std::string sample, Json::Array targets) {
    GalleryDemoScenario result;
    result.route = {GalleryPageKind::component, std::move(component), {}, std::move(sample)};
    result.definition = Json::Object{{"native", Json::Object{{"targets", std::move(targets)}}}};
    return result;
}

void routing() {
    const auto context = std::make_shared<GalleryDemoSampleContext>();
    context->fixture = Json::Object{{"data", Json::Object{}}};
    GalleryDemoSampleScope scope{context};
    Fixture fixture;
    auto definition = scenario("ant.component.button", "gallery.sample.button.basics",
                               {"gallery.state.default", "gallery.state.disabled", "gallery.state.loading"});
    GalleryDemoNativeActions ports{fixture.buttons,
                                   fixture.inputs,
                                   fixture.pages,
                                   context,
                                   [&] { fixture.sync(); },
                                   [](std::int64_t) {},
                                   {},
                                   [&]() -> Json {
                                       return Json::Object{{"logical_width", fixture.viewport.width},
                                                           {"logical_height", fixture.viewport.height}};
                                   },
                                   {},
                                   {},
                                   {}};
    GalleryDemoInputBridge bridge{definition, ports};
    std::size_t step{};
    const auto dispatch = [&](std::string kind, std::string target, Json parameters) {
        const GalleryDemoAction action{
            "s" + std::to_string(++step),    std::move(kind),       std::move(target), "native-core-dispatch",
            static_cast<std::int64_t>(step), std::move(parameters), Json::Object{}};
        const auto receipt = bridge.dispatch(action);
        fixture.sync();
        require(receipt.at("dispatched").boolean(), "bridge skipped a production dispatch");
        return receipt;
    };
    const auto click = [&](const char* target) {
        for (const auto* kind : {"pointer-down", "pointer-up"}) {
            dispatch(kind, target, Json::Object{{"button", "primary"}, {"x_fraction", .5}, {"y_fraction", .5}});
        }
    };
    const auto keyboard = [&](const char* key) {
        for (const auto* kind : {"key-down", "key-up"}) {
            dispatch(kind, "sample", Json::Object{{"key", key}, {"modifiers", Json::Array{}}});
        }
    };
    fixture.show(definition);
    require(dispatch("checkpoint", "sample", Json::Object{}).at("layer").string() == "observer",
            "checkpoint observation was mislabeled as an input dispatch");
    click("gallery.state.default");
    require(context->callbacks.size() == 1, "real pointer activation callback missing or duplicated");
    keyboard("Tab");
    keyboard("Enter");
    require(context->callbacks.size() == 2, "keyboard activation did not use the real focused Button");
    click("gallery.state.disabled");
    click("gallery.state.loading");
    require(context->callbacks.size() == 2, "disabled/loading Button activated");

    definition = scenario("ant.component.input", "gallery.sample.input.basics", {"gallery.input.controlled"});
    fixture.show(definition);
    const auto input_owner = fixture.pages.target("gallery.input.controlled");
    const auto input_metadata = fixture.inputs.input_snapshot(input_owner);
    require(input_metadata.controlled && !input_metadata.disabled && !input_metadata.read_only &&
                input_metadata.max_length == 32 &&
                input_metadata.placeholder == String{u8"受控：输入中文 / Latin / emoji"},
            "Input source query did not expose applied production state");
    std::get<Json::Object>(definition.definition.value)["fixture"] = Json::Object{
        {"props", Json::Object{{"controls", Json::Object{{"gallery.input.controlled", Json::Object{}},
                                                         {"gallery.input.uncontrolled", Json::Object{}}}},
                               {"composition",
                                Json::Object{{"component", "control"}, {"target", "gallery.input.uncontrolled"}}}}},
        {"slots", Json::Object{{"gallery.input.controlled", Json::Object{{"prefix", "前"}, {"suffix", "32"}}}}}};
    require(bridge.input_targets() ==
                std::vector<std::string>{"gallery.input.controlled", "gallery.input.uncontrolled"},
            "plain text affixes were treated as nested component slots");
    const std::array input_roots{fixture.pages.target("gallery.sample.input.basics")};
    require(bridge.query_inputs(input_roots, "text-affix-query").at("ordered_targets").array().size() == 2,
            "actual production Input query failed with plain text prefix/suffix");
    const auto layers = fixture.inputs.text_layers(input_owner);
    const auto node = fixture.nodes.require(fixture.services.components().root(input_owner));
    const auto font_info = [&](font::FontIdentity identity) -> Json {
        const auto source = fixture.fonts->source(identity);
        require(source.has_value(), "actual text-view font source is stale");
        return Json::Object{{"family", source->family}, {"face_index", static_cast<std::int64_t>(source->face_index)}};
    };
    const auto nominal_glyph = [&](font::FontIdentity identity, char32_t codepoint) {
        const auto result = fixture.fonts->glyph_index(identity, codepoint);
        require(static_cast<bool>(result), "actual nominal glyph query failed");
        return result.glyph.glyph_id;
    };
    for (const auto layer : {layers.base, layers.selected, layers.placeholder}) {
        const auto metadata = gallery_demo_text_snapshot(fixture.scene, layer, node.bounds, font_info, nominal_glyph);
        require(metadata.at("index_unit").string() == "UTF-8-bytes", "text snapshot omitted explicit byte indices");
        require(!metadata.at("font_runs").array().empty() && !metadata.at("glyphs").array().empty(),
                "actual editor text-view snapshot lost shared shaping/font observations");
        require(metadata.at("font_coverage_available").boolean() && metadata.at("missing_font_scalars").array().empty(),
                "declared Latin/CJK text unexpectedly lacks a real nominal glyph");
    }
    click("gallery.input.controlled");
    require(fixture.inputs.sessions().active().valid(), "pointer did not start actual editor session");
    const auto previous_input_session = fixture.inputs.sessions().active();
    require(fixture.inputs.set_caret_deadline(input_owner, animation::AnimationTime::microseconds(500000)) &&
                fixture.inputs.caret_deadline(input_owner) == animation::AnimationTime::microseconds(500000),
            "owner-local injected caret deadline was not observed");
    dispatch("text-commit", "gallery.input.controlled", Json::Object{{"text", "abc🙂"}});
    require(context->values.at("gallery.input.controlled")().string() == "abc🙂", "accepted text commit missing");
    const auto coverage = gallery_demo_text_snapshot(fixture.scene, layers.base, node.bounds, font_info, nominal_glyph);
    require(coverage.at("replacement_count").number() == 0 &&
                coverage.at("replacement_count_unit").string() == "invalid-UTF8-repairs" &&
                coverage.at("missing_font_scalars").array().size() == 1 &&
                coverage.at("missing_font_scalars").array().front().at("codepoint").number() == 0x1F642,
            "valid UTF8 repair count incorrectly certified missing emoji font coverage");
    dispatch("external-props", "sample", Json::Object{{"values", Json::Object{{"parent_policy", "reject"}}}});
    dispatch("text-commit", "gallery.input.controlled", Json::Object{{"text", "x"}});
    require(context->values.at("gallery.input.controlled")().string() == "abc🙂", "reject parent committed proposal");
    require(!context->callbacks.back().at("payload").at("accepted").boolean(), "reject callback metadata is false");
    context->fault = "input-reject-commit";
    dispatch("text-commit", "gallery.input.controlled", Json::Object{{"text", "y"}});
    require(context->values.at("gallery.input.controlled")().string() != "abc🙂",
            "real controlled-commit fault did not alter the application value");
    context->fault = "duplicate-callback";
    context->parent_policy = "accept";
    const auto callback_count = context->callbacks.size();
    dispatch("text-commit", "gallery.input.controlled", Json::Object{{"text", "z"}});
    require(context->callbacks.size() == callback_count + 2,
            "real callback fault did not duplicate the application path");
    context->fault = "none";
    dispatch("text-commit", "gallery.input.controlled", Json::Object{{"text", "w"}});
    require(context->callbacks.size() == callback_count + 3, "restored callback path still duplicates");

    context->parent_policy = "accept";
    definition = scenario("ant.component.select", "gallery.sample.select.options",
                          {"gallery.select.controlled-search", "gallery.select.disabled", "gallery.select.loading"});
    fixture.show(definition);
    const auto select_owner = fixture.pages.target("gallery.select.controlled-search");
    const auto before_stale = context->callbacks.size();
    require(!fixture.pages.dispatch(input::TextCommitted{String{u8"late old owner"}, previous_input_session}) &&
                context->callbacks.size() == before_stale,
            "retired editor session accepted a late generation event");
    bool rejected_input_deadline{};
    try {
        static_cast<void>(fixture.inputs.caret_deadline(input_owner));
    } catch (const std::out_of_range&) {
        rejected_input_deadline = true;
    }
    require(rejected_input_deadline, "retired Input deadline query silently reported no deadline");
    const auto select_metadata = fixture.services.select().input_snapshot(select_owner);
    require(select_metadata.controlled_value && select_metadata.show_search && select_metadata.allow_clear &&
                select_metadata.options.size() == 2 && select_metadata.options[0].value == SelectValue{u8"desktop"} &&
                select_metadata.options[1].value == SelectValue{u8"service"},
            "Select source query did not expose ordered applied options/flags");
    click("gallery.select.controlled-search");
    keyboard("ArrowDown");
    keyboard("Enter");
    require(context->values.at("gallery.select.controlled-search")().string() == "service",
            "Select keyboard navigation did not commit second option");
    context->parent_policy = "reject";
    click("gallery.select.controlled-search");
    keyboard("ArrowUp");
    keyboard("Enter");
    require(fixture.services.select().snapshot(select_owner).value == SelectSelection{SelectValue{u8"service"}},
            "Select rejected proposal changed the applied selection");
    bool rejected_change{};
    for (const auto& callback : context->callbacks) {
        if (callback.at("target").string() == "gallery.select.controlled-search" &&
            callback.at("event").string() == "change" && !callback.at("payload").at("accepted").boolean()) {
            rejected_change = true;
        }
    }
    require(rejected_change, "real Select rejection callback was not observed");
    keyboard("Escape");
    require(dispatch("wheel", "sample", Json::Object{{"delta_x", 0}, {"delta_y", 240}}).at("consumed").boolean(),
            "controlled-open source row was not scrolled through the actual Gallery input route");
    const auto controlled_open = fixture.pages.target("gallery.select.controlled-open");
    const auto open_callbacks = context->callbacks.size();
    click("gallery.select.controlled-open");
    require(context->callbacks.size() == open_callbacks + 1 &&
                context->callbacks.back().at("target").string() == "gallery.select.controlled-open" &&
                context->callbacks.back().at("event").string() == "open-change",
            "controlled open rejection had no real callback");
    require(!fixture.services.select().snapshot(controlled_open).visible, "controlled open rejection was ignored");
    context->parent_policy = "accept";
    click("gallery.select.controlled-open");
    require(fixture.services.select().snapshot(controlled_open).visible, "controlled open acceptance did not apply");
    keyboard("Escape");
    require(!fixture.services.select().snapshot(controlled_open).visible, "controlled popup did not close with Escape");
    require(dispatch("wheel", "sample", Json::Object{{"delta_x", 0}, {"delta_y", -240}}).at("consumed").boolean(),
            "Select search source row was not restored through actual input");
    click("gallery.select.controlled-search");
    dispatch("text-commit", "gallery.select.controlled-search", Json::Object{{"text", "服"}});
    require(fixture.services.select().snapshot(select_owner).query == String{u8"服"},
            "Chinese search did not reach the actual Select editor/model");
    keyboard("Escape");
    for (const auto* target : {"gallery.select.disabled", "gallery.select.loading"}) {
        const auto owner = fixture.pages.target(target);
        const auto before = fixture.services.select().snapshot(owner);
        const auto inputs = fixture.services.select().input_snapshot(owner);
        require(inputs.disabled || inputs.loading, "actual Select eligibility source was lost");
        click(target);
        if (inputs.disabled) {
            require(!fixture.services.select().snapshot(owner).visible && !before.visible, "disabled Select opened");
        } else {
            // Select loading retains its search/popup but rejects commits; it is
            // a different component contract from Button loading eligibility.
            keyboard("ArrowDown");
            keyboard("Enter");
            require(fixture.services.select().snapshot(owner).value == before.value, "loading Select committed option");
        }
    }
    keyboard("Escape");
    const auto scroll_before = fixture.pages.snapshot().document.offset;
    dispatch("wheel", "sample", Json::Object{{"delta_x", 0}, {"delta_y", 48}});
    require(fixture.pages.snapshot().document.offset == scroll_before + 48,
            "real document wheel did not preserve the declared logical distance/owner");
    const auto retired = fixture.pages.target("gallery.select.controlled-search");
    definition.route = {};
    fixture.show(definition);
    require(!fixture.services.components().contains(retired), "navigation retained disposed sample owner");
    require(!fixture.inputs.sessions().active().valid(), "navigation retained editor session");
    bool rejected_stale{};
    try {
        static_cast<void>(fixture.services.select().input_snapshot(retired));
    } catch (const std::out_of_range&) {
        rejected_stale = true;
    }
    require(rejected_stale, "source query accepted a retired Select generation");
}

void independently_applied_inputs() {
    require(!gallery_demo_sample_context(), "ordinary Gallery unexpectedly enabled fixture/fault hooks");
    Fixture fixture;
    const auto input_case = scenario("ant.component.input", "gallery.sample.input.basics", {});
    fixture.show(input_case);
    const auto input = fixture.pages.target("gallery.input.controlled");
    const auto applied = fixture.inputs.input_snapshot(input);
    require(fixture.inputs.variant(input) == InputVariant::Outlined &&
                fixture.inputs.status(input) == InputStatus::Default && applied.max_length == 32,
            "real Input applied status/variant/limit are missing");
    require(gallery_demo_retained_content(fixture.buttons, applied.prefix).dump().find("前") != std::string::npos &&
                gallery_demo_retained_content(fixture.buttons, applied.suffix).dump().find("32") != std::string::npos,
            "real Input affix text was not read from retained slots");
    require(!gallery_demo_sample_context(), "ordinary source query enabled a demo context");

    fixture.show(scenario("ant.component.select", "gallery.sample.select.options", {}));
    // All registered pilot targets remain present in their original source.
    for (const auto* name : {"controlled-search",
                             "numeric",
                             "disabled",
                             "loading",
                             "empty",
                             "controlled-open",
                             "outlined",
                             "filled",
                             "borderless",
                             "underlined",
                             "small",
                             "middle",
                             "large",
                             "warning",
                             "error",
                             "component-theme",
                             "focus",
                             "toggle-open",
                             "dynamic-options",
                             "multiple",
                             "authority",
                             "tags",
                             "virtual",
                             "prefix",
                             "async"}) {
        require(fixture.pages.target(std::string{"gallery.select."} + name).valid(),
                "complete Select factory lost a target");
    }
    const auto numeric = fixture.services.select().snapshot(fixture.pages.target("gallery.select.numeric"));
    require(numeric.value == SelectSelection{SelectValue{42}}, "numeric source value was converted to text");
    const auto themed = fixture.pages.target("gallery.select.component-theme");
    const auto theme = gallery_demo_applied_theme(fixture.buttons, themed);
    require(theme.at("Select").at("algorithm").boolean() &&
                theme.at("Select").at("colorPrimary").string() == "#722ed1" &&
                theme.at("select_option_height").number() == 36,
            "applied local Select Theme lost the authored component overrides");
    const auto multiple = fixture.services.select().input_snapshot(fixture.pages.target("gallery.select.multiple"));
    require(multiple.mode == SelectMode::Multiple && multiple.controlled_value && multiple.max_tag_count == 2,
            "nested multiple source was omitted");
    const auto tags = fixture.services.select().input_snapshot(fixture.pages.target("gallery.select.tags"));
    require(tags.mode == SelectMode::Tags && tags.tag_renderer && tags.responsive_tags && tags.separators.size() == 2,
            "nested tags source was omitted");
    const auto virtual_list = fixture.services.select().input_snapshot(fixture.pages.target("gallery.select.virtual"));
    require(virtual_list.options.size() == 10000 && virtual_list.list_height == 160 && virtual_list.option_renderer &&
                virtual_list.popup_renderer && virtual_list.direction == TextDirection::RightToLeft,
            "real virtual source options/context were truncated");
    const auto async = fixture.services.select().input_snapshot(fixture.pages.target("gallery.select.async"));
    require(async.async_source && async.popup_renderer, "real deterministic async source was omitted");
}

void snapshot_does_not_reread_props() {
    ryn_test::input_component::Fixture fixture;
    int reads{};
    fixture.inputs.mount(Content{[&] {
        Input(InputProps{}
                  .placeholder(bind([&] {
                      ++reads;
                      return String{u8"actual"};
                  }))
                  .variant(InputVariant::Filled),
              InputPrefix{[] { Text(u8"retained"); }});
    }});
    fixture.synchronize();
    const auto id = fixture.inputs.mounted_inputs().front().component;
    const auto before = reads;
    const auto revision = fixture.scene.ordered_scene().revision();
    const auto snapshot = fixture.inputs.input_snapshot(id);
    require(snapshot.placeholder == String{u8"actual"} && fixture.inputs.variant(id) == InputVariant::Filled &&
                gallery_demo_retained_content(fixture.buttons, snapshot.prefix).array().size() == 1,
            "snapshot did not read the applied editor and retained slot");
    require(reads == before && fixture.scene.ordered_scene().revision() == revision,
            "read-only observer reran a business Prop getter or mutated the scene");
}

GalleryDemoScenario complete_input_case() {
    const std::array names{"controlled-search",
                           "numeric",
                           "disabled",
                           "loading",
                           "empty",
                           "controlled-open",
                           "outlined",
                           "filled",
                           "borderless",
                           "underlined",
                           "small",
                           "middle",
                           "large",
                           "warning",
                           "error",
                           "component-theme",
                           "focus",
                           "toggle-open",
                           "dynamic-options",
                           "multiple",
                           "authority",
                           "tags",
                           "virtual",
                           "prefix",
                           "popup-note",
                           "async",
                           "deliver"};
    Json::Array legacy;
    Json::Array children;
    Json::Object controls;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto name = "gallery.select." + std::string{names[i]};
        if (i < 19) {
            legacy.emplace_back(name);
        }
        controls.emplace(name, Json::Object{}); // Navigation identities only, never actual values.
        if (i != 23 && i != 24 && i != 26) {
            children.emplace_back(Json::Object{{"component", "control"}, {"target", name}});
        }
    }
    auto result = scenario("ant.component.select", "gallery.sample.select.options", legacy);
    auto& object = std::get<Json::Object>(result.definition.value);
    object.emplace(
        "fixture",
        Json::Object{
            {"props", Json::Object{{"controls", controls},
                                   {"composition", Json::Object{{"component", "Flex"}, {"children", children}}}}},
            {"slots", Json::Object{{"gallery.select.virtual",
                                    Json::Object{{"prefix", Json::Object{{"control", "gallery.select.prefix"}}},
                                                 {"popup", Json::Object{{"control", "gallery.select.popup-note"}}}}},
                                   {"gallery.select.async",
                                    Json::Object{{"popup", Json::Object{{"control", "gallery.select.deliver"}}}}}}}});
    return result;
}

void complete_factory_queries_and_source_counterexamples() {
    for (const auto fault :
         {GalleryDemoInputFault::none, GalleryDemoInputFault::popup_placeholder, GalleryDemoInputFault::popup_label,
          GalleryDemoInputFault::popup_theme, GalleryDemoInputFault::popup_order, GalleryDemoInputFault::popup_owner}) {
        const auto context = std::make_shared<GalleryDemoSampleContext>();
        context->fixture = Json::Object{{"data", Json::Object{}}};
        context->input_fault = fault;
        std::size_t factory_runs{};
        context->factory_boundary = [&](std::string_view, bool begin) {
            factory_runs += begin;
        };
        GalleryDemoSampleScope scope{context};
        Fixture fixture;
        const auto definition = complete_input_case();
        runtime::ComponentId retired_root;
        if (fault == GalleryDemoInputFault::none) {
            fixture.show(
                scenario("ant.component.button", "gallery.sample.button.basics", {"gallery.button.increment"}));
            retired_root = fixture.pages.target("gallery.sample.button.basics");
            require(retired_root.valid(), "previous sample root was not mounted");
        }
        fixture.show(definition);
        GalleryDemoInputBridge bridge{
            definition, GalleryDemoNativeActions{fixture.buttons,
                                                 fixture.inputs,
                                                 fixture.pages,
                                                 context,
                                                 [&] { fixture.sync(); },
                                                 [](std::int64_t) {},
                                                 {},
                                                 [&]() -> Json {
                                                     return Json::Object{{"logical_width", fixture.viewport.width},
                                                                         {"logical_height", fixture.viewport.height}};
                                                 },
                                                 {},
                                                 {},
                                                 {}}};
        const std::array roots{fixture.pages.target("gallery.sample.select.options")};
        const auto before_runs = factory_runs;
        const auto before_slots = context->popup_body_runs;
        const auto before_frames = fixture.frames.revision();
        const auto before_scene = fixture.scene.ordered_scene().revision();
        const auto before_focus = fixture.buttons.focus().state().focused;
        const auto before_session = fixture.inputs.sessions().active();
        std::vector<std::pair<runtime::NodeId, std::uint64_t>> measures;
        const auto collect = [&](auto&& self, runtime::ComponentId id) -> void {
            const auto node = fixture.services.components().root(id);
            measures.emplace_back(node, fixture.nodes.require(node).measure_count);
            for (const auto child : fixture.services.components().children(id)) {
                self(self, child);
            }
        };
        collect(collect, roots.front());
        const auto initial = bridge.query_inputs(roots, "setup", true);
        const auto again = bridge.query_inputs(roots, "query-purity");
        if (retired_root.valid()) {
            require(!fixture.services.components().contains(retired_root), "previous sample root did not retire");
            const std::array historical_roots{retired_root, roots.front()};
            const auto historical = bridge.query_inputs(historical_roots, "retired-root-query");
            require(historical.at("composition").dump() == initial.at("composition").dump() &&
                        historical.at("instances").dump() == initial.at("instances").dump(),
                    "historical roots invalidated live input evidence or fabricated an instance");
        }
        require(initial.at("ordered_targets").array().size() == 27 && initial.at("instances").object().size() == 27,
                "full factory input query lost an identity");
        for (const auto& [name, instance] : initial.at("instances").object()) {
            require(instance.at("status").string() == "mounted", "native eager child was excused as deferred");
            require(instance.at("instance").string() == again.at("instances").at(name).at("instance").string(),
                    "read-only query replaced a retained component");
        }
        const auto placeholder = initial.at("controls").at("gallery.select.popup-note").at("placeholder").string();
        require((placeholder == "popup 内独立草稿") == (fault != GalleryDemoInputFault::popup_placeholder),
                "source placeholder counterexample was hidden by the declaration");
        const auto label = initial.at("slots").at("gallery.select.deliver").at("content").dump();
        require((label.find("交付合成异步选项") != std::string::npos) == (fault != GalleryDemoInputFault::popup_label),
                "source label counterexample was hidden by the declaration");
        const auto& algorithms = initial.at("themes").at("gallery.select.popup-note").at("algorithms").array();
        const auto algorithm = algorithms.empty() ? std::string{"default"} : algorithms.front().string();
        require((algorithm == "dark") == (fault == GalleryDemoInputFault::popup_theme),
                "source Theme counterexample was hidden by a global scope");
        require((initial.at("instances").at("gallery.select.popup-note").at("slot").string() == "popup") ==
                    (fault != GalleryDemoInputFault::popup_owner),
                "wrong actual slot owner was accepted");
        const auto tree = initial.at("slot_composition").at("gallery.select.virtual").at("popup").dump();
        if (fault == GalleryDemoInputFault::popup_order) {
            require(tree.find("gallery.select.popup-note") < tree.find("origin"),
                    "wrong source slot order was normalized away");
        } else if (fault != GalleryDemoInputFault::popup_owner) {
            require(tree.find("origin") < tree.find("gallery.select.popup-note"),
                    "actual origin is not before its child");
        }
        require(factory_runs == before_runs && context->popup_body_runs == before_slots &&
                    fixture.frames.revision() == before_frames &&
                    fixture.scene.ordered_scene().revision() == before_scene &&
                    fixture.buttons.focus().state().focused == before_focus &&
                    fixture.inputs.sessions().active() == before_session,
                "query caused factory/slot/frame/scene/focus/session work");
        for (const auto& [node, measured] : measures) {
            require(fixture.nodes.require(node).measure_count == measured, "query performed additional measurement");
        }
        if (fault == GalleryDemoInputFault::none) {
            const auto note = fixture.pages.target("gallery.select.popup-note");
            const auto send = [&](std::string step, std::string kind, std::string target, Json parameters) {
                const auto receipt = bridge.dispatch(
                    {step, kind, target, "native-core-dispatch", 0, std::move(parameters), Json::Object{}});
                fixture.sync();
                require(receipt.at("dispatched").boolean(), "CORE real popup action was not dispatched");
            };
            send("core-virtual-wheel", "wheel", "sample", Json::Object{{"delta_x", 0}, {"delta_y", 1392}});
            for (const auto* kind : {"pointer-down", "pointer-up"}) {
                send(kind, kind, "gallery.select.virtual", Json::Object{{"button", "primary"}});
            }
            const auto mounted = bridge.query_inputs(roots, "core-virtual-mounted");
            require(mounted.at("current").at("gallery.select.virtual").at("open").boolean(),
                    "CORE real pointer did not open virtual popup before child editing");
            for (const auto* kind : {"pointer-down", "pointer-up"}) {
                send(kind, kind, "gallery.select.popup-note", Json::Object{{"button", "primary"}});
            }
            send("core-popup-text", "text-commit", "gallery.select.popup-note", Json::Object{{"text", "popup draft"}});
            const auto mounted_note = std::ranges::find(fixture.inputs.mounted_inputs(), note,
                                                        &ryn::detail::MountedInputComponent::component);
            require(mounted_note != fixture.inputs.mounted_inputs().end(),
                    "real opening lost the original popup editor instance");
            const auto editor = mounted_note->editor;
            const auto draft = std::string{fixture.inputs.editors().require(editor).value()};
            const auto selection = fixture.inputs.editors().require(editor).selection();
            const auto caret = fixture.inputs.caret_deadline(note);
            const auto focus = fixture.buttons.focus().state().focused;
            const auto frame = fixture.frames.revision();
            const auto slot_runs = context->popup_body_runs;
            const auto callbacks = context->callbacks.size();
            const auto edited = bridge.query_inputs(roots, "core-query-after-edit");
            require(draft == "popup draft" &&
                        edited.at("current").at("gallery.select.popup-note").at("value").string() == draft &&
                        fixture.inputs.editors().require(editor).selection() == selection &&
                        fixture.inputs.caret_deadline(note) == caret &&
                        fixture.buttons.focus().state().focused == focus && fixture.frames.revision() == frame &&
                        context->popup_body_runs == slot_runs && context->callbacks.size() == callbacks,
                    "post-input query altered draft/selection/caret/focus/frame/slot/callback state");
            fixture.pages.set_locale(locales::en_US);
            fixture.sync();
            const auto translated = bridge.query_inputs(roots, "core-locale");
            require(fixture.pages.target("gallery.select.popup-note") == note &&
                        translated.at("controls").at("gallery.select.popup-note").at("placeholder").string() ==
                            "Independent draft inside popup" &&
                        translated.at("contexts").at("gallery.select.popup-note").at("locale").string() == "en-US" &&
                        initial.at("controls").at("gallery.select.popup-note").at("placeholder").string() ==
                            "popup 内独立草稿",
                    "CORE locale update remounted the child or rewrote the initial snapshot");
            require(translated.at("current").at("gallery.select.popup-note").at("value").string() == draft &&
                        fixture.inputs.editors().require(editor).selection() == selection &&
                        fixture.buttons.focus().state().focused == focus,
                    "CORE locale update lost an existing popup draft/selection/focus");
            fixture.pages.request({});
            fixture.pages.process();
            fixture.sync();
            const auto retired = bridge.query_inputs(roots, "retired");
            require(retired.at("instances").at("gallery.select.popup-note").at("status").string() == "missing",
                    "retired factory generation remained a mounted input witness");
        }
    }
}

void rebuilt_popup_queries_do_not_execute_user_code() {
    ryn_test::input_component::Fixture fixture;
    std::size_t slots{};
    std::size_t getters{};
    runtime::ComponentId child;
    const auto renderer = [&] {
        return SelectPopupRenderer{[&](std::function<void()> origin) {
            ++slots;
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[&, origin] {
                     origin();
                     Input(InputProps{}.placeholder(bind([&] {
                         ++getters;
                         return String{u8"applied"};
                     })));
                     child = fixture.inputs.mounted_inputs().back().component;
                 }});
        }};
    };
    const Signal<SelectPopupRenderer> popup{renderer()};
    fixture.inputs.mount(Content{[&] { Select(SelectProps{}.popupRender(popup)); }});
    fixture.synchronize();
    const auto owner = fixture.services.select().mounted().front();
    const auto old_child = child;
    popup.set(renderer()); // Real production renderer update, outside GalleryDraftBuild TLS.
    fixture.synchronize();
    require(child != old_child && !fixture.services.components().contains(old_child),
            "renderer update did not retire the old child generation");
    const auto before_slots = slots;
    const auto before_getters = getters;
    const auto before_frame = fixture.frames.revision();
    const auto before_scene = fixture.scene.ordered_scene().revision();
    const auto before_caret = fixture.inputs.caret_deadline(child);
    const auto applied = fixture.services.select().input_snapshot(owner);
    const std::array roots{applied.popup_body};
    const auto tree = gallery_demo_retained_tree(fixture.buttons, roots, {{"actual-child", child}}, applied.list_root);
    require(tree.dump().find("origin") != std::string::npos && tree.dump().find("actual-child") != std::string::npos,
            "real rebuilt slot subtree was not observed");
    require(slots == before_slots && getters == before_getters && fixture.frames.revision() == before_frame &&
                fixture.scene.ordered_scene().revision() == before_scene &&
                fixture.inputs.caret_deadline(child) == before_caret,
            "query reran a renderer/getter or changed scene/frame/caret");
    // A real renderer can retire its owner reentrantly. No old SelectState or
    // child witness may survive the user callback, even when mount rejects it.
    try {
        popup.set(SelectPopupRenderer{[&](std::function<void()> origin) {
            origin();
            fixture.services.components().destroy(owner);
        }});
        fixture.synchronize();
    } catch (const std::logic_error&) {
        // Explicit rejection of the retired mount is valid; a live witness is not.
    }
    require(!fixture.services.components().contains(owner), "reentrant popup owner did not retire");
    bool stale{};
    try {
        static_cast<void>(fixture.services.select().input_snapshot(owner));
    } catch (const std::out_of_range&) {
        stale = true;
    }
    require(stale && !fixture.services.components().contains(child), "destroy retained a stale popup generation");
}
} // namespace

int main() {
    try {
        independently_applied_inputs();
        snapshot_does_not_reread_props();
        complete_factory_queries_and_source_counterexamples();
        rebuilt_popup_queries_do_not_execute_user_code();
        routing();
        std::cout << "Native production bridge input/controlled/eligibility/retirement controls passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
