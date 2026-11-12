#include "gallery_demo_scenario.hpp"
#include "component/select_component.hpp"
#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <iostream>
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
} // namespace

int main() {
    try {
        routing();
        std::cout << "Native production bridge input/controlled/eligibility/retirement controls passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
