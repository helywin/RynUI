#include "component/card_component.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/select_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "renderer/common/scene_resources.hpp"
#include "token_gallery_definition.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <ryn/rynui.hpp>
#include <thread>
#include <SDL3/SDL_video.h>

namespace rynui::example {
namespace {
using namespace ryn;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct CardPageState {
    std::size_t selected{};
    std::vector<runtime::ComponentId> pages;
};
} // namespace

int run_card_acceptance(int argc, char** argv) {
    try {
        std::optional<float> requested_scale;
        bool dark = false;
        std::filesystem::path directory;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (argument.starts_with("--acceptance-scale=")) {
                requested_scale = std::stof(std::string{argument.substr(19)});
            }
            if (argument.starts_with("--evidence-dir=")) {
                directory = std::filesystem::path{argument.substr(15)};
            }
            if (argument == "--acceptance-theme=dark") {
                dark = true;
            }
        }
        require(!directory.empty(), "Card acceptance requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Card Acceptance";
        config.width = 1280;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require(bool(created), "native Card window creation failed");
        auto& platform = *created.state;
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require(scale == 1 || scale == 1.25F || scale == 1.5F || scale == 2, "invalid acceptance scale");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require(bool(font_result), "native Card font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        auto font_chain = detail::load_default_ui_font_chain(*fonts, font_request);
        require(bool(font_chain), "native Card font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, font_chain, scale);
        ThemeConfig initial_theme;
        initial_theme.seed.motion = false;
        if (dark) {
            initial_theme.algorithms = {ThemeAlgorithm::Dark};
        }
        Signal<ThemeConfig> theme{initial_theme};
        Signal<bool> loading{false};
        Signal<LogicalLength> width{dp(280)};
        Signal<String> title{String{u8"Theme / 主题"}};
        ThemeConfig initial_custom;
        initial_custom.card.tokens.background = Color::rgba8(230, 244, 255);
        initial_custom.card.tokens.body_padding = dp(16);
        initial_custom.card.tokens.text = Color(0, 0, 0, 0.88F);
        initial_custom.card.tokens.header_text = Color(0, 0, 0, 0.88F);
        Signal<ThemeConfig> custom{initial_custom};
        InputRef editor_ref;
        int activations{};
        int content_runs{};
        int body_runs{};
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        detail::ButtonComponentHost buttons{services};
        detail::InputComponentHost inputs{services, platform, platform};
        inputs.set_display_scale(scale);
        runtime::ComponentId root;
        std::vector<runtime::ComponentId> pages;
        services.mount(Content{[&] {
            ++content_runs;
            Theme(
                ThemeProps{}.config(theme), ThemeContent{[&] {
                    auto& build = runtime::require_component_build_context();
                    root = build.mount_component<CardPageState>();
                    const auto page = [&](Content body) {
                        build.mount_slot(root, Content{[&] {
                                             auto& context = runtime::require_component_build_context();
                                             const auto id = context.mount_component<int>(0);
                                             pages.push_back(id);
                                             services.layout().set_layout(
                                                 context.root(id),
                                                 layout::FlexLayout{layout::FlexDirection::vertical, 8, {}, true});
                                             context.mount_slot(id, body);
                                         }});
                    };
                    const auto props = [&] {
                        return CardProps{}.layout(LayoutStyle{}.width(width));
                    };
                    const auto action = [&](const char8_t (&label)[5]) {
                        Button(ButtonProps{}.onClick([&] { ++activations; }),
                               [label = String{label}] { Text(TextProps{}.content(label)); });
                    };
                    page(Content{[&] {
                        Card(props().title(u8"Account / 账户").hoverable().loading(loading), CardContent{[&] {
                                 ++body_runs;
                                 Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                                          Input(InputProps{}
                                                    .defaultValue(u8"Draft / 草稿")
                                                    .ref(editor_ref)
                                                    .layout(LayoutStyle{}.width(dp(180))));
                                          Select(SelectProps{}
                                                     .options({{1, u8"One / 一"}, {2, u8"Two / 二"}})
                                                     .defaultValue(1)
                                                     .layout(LayoutStyle{}.width(dp(180))));
                                      }});
                             }},
                             CardSlots{.extra = CardExtra{[&] { action(u8"Edit"); }},
                                       .actions = CardActions{[&] { action(u8"Save"); }}});
                        Card(props(), CardContent{[] { Text(u8"Plain content / 无标题正文"); }});
                    }});
                    page(Content{[&] {
                        Card(props().title(u8"Small / 小号").size(CardSize::Small),
                             CardContent{[] { Text(u8"12 dp padding"); }});
                        Card(props().title(u8"Borderless / 无边框").variant(CardVariant::Borderless).hoverable(),
                             CardContent{[&] { action(u8"Edit"); }});
                        Card(props().title(u8"Loading / 加载").loading(), CardContent{[] { Text(u8"Retained data"); }});
                    }});
                    page(Content{[&] {
                        Card(props(), CardContent{[] { Text(u8"Body / 正文"); }},
                             CardSlots{.title = CardTitle{[] { Text(u8"Rich title / 富标题"); }},
                                       .cover = CardCover{[] { Text(u8"Cover composition / 封面区域"); }},
                                       .actions = CardActions{[&] {
                                           Space(SpaceProps{}, SpaceContent{[&] {
                                                     action(u8"Save");
                                                     action(u8"More");
                                                 }});
                                       }}});
                        Theme(ThemeProps{}.config(custom), ThemeContent{[&] {
                                  Card(props().title(title), CardContent{[] { Text(u8"Component Token / 局部主题"); }});
                              }});
                    }});
                    build.state<CardPageState>(root).pages = pages;
                    services.layout().set_layout(
                        build.root(root),
                        layout::ComponentLayout{
                            [&](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                const auto& state = *services.components().state<CardPageState>(root);
                                return engine.measure_child(services.components().root(state.pages[state.selected]),
                                                            limits);
                            },
                            [&](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                                const auto& state = *services.components().state<CardPageState>(root);
                                engine.place_child(services.components().root(state.pages[state.selected]), bounds);
                            }});
                }});
        }});
        detail::SdlSceneRenderer renderer{platform};
        detail::SceneBackend& backend = renderer;
        detail::SceneResources resources{renderer};
        runtime::Size viewport;
        std::size_t frame_number{};
        const auto draw = [&](const std::string& name) {
            for (int settle = 0; settle < 2; ++settle) {
                (void)platform.poll_events();
                metrics = platform.window_metrics();
                viewport = {metrics.pixel_width / scale, metrics.pixel_height / scale};
                const auto time = animation::AnimationTime::microseconds(++frame_number * 250'000);
                (void)services.tick_animations(time);
                require(services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                        "native Card layout failed");
                require(resources.synchronize({&services.surfaces().instances(),
                                               scene.atlas(),
                                               scene.glyph_scene().instances(),
                                               &services.rounded_effects(),
                                               {static_cast<std::uint32_t>(metrics.pixel_width),
                                                static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                        "native Card GPU upload failed");
                require(backend.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                        "native Card scene attachment failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require(renderer.submit_frame(time) != runtime::FrameSubmissionResult::failed,
                        "native Card frame failed");
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
            }
            require(renderer.save_frame_bmp(directory / (name + ".bmp")), "native Card readback failed");
        };
        const auto page = [&](std::size_t selected) {
            services.focus().clear_focus();
            services.components().state<CardPageState>(root)->selected = selected;
            for (std::size_t index = 0; index < pages.size(); ++index) {
                services.components().set_branch_active(pages[index], selected == index);
            }
            services.mark_scene_structure_dirty();
            dirty.invalidate(services.components().root(root), runtime::DirtyFlags::Measure |
                                                                   runtime::DirtyFlags::Layout |
                                                                   runtime::DirtyFlags::Geometry);
        };
        const auto bounds = [&](runtime::ComponentId id) {
            return nodes.require(services.components().root(id)).bounds;
        };
        const auto point = [&](runtime::ComponentId id) {
            const auto rect = bounds(id);
            return runtime::Point{rect.x + rect.width / 2, rect.y + rect.height / 2};
        };
        const auto click = [&](runtime::ComponentId id) {
            const auto position = point(id);
            for (const auto action : {input::PointerAction::down, input::PointerAction::up}) {
                services.pointer().dispatch(
                    {input::PointerIdentity::mouse(), action, input::PointerButton::primary, position.x, position.y});
            }
        };
        const auto key = [&](input::Key key) {
            services.focus().dispatch({key, input::KeyAction::down});
            services.focus().dispatch({key, input::KeyAction::up});
        };
        auto& cards = services.card();
        require(cards.mounted().size() == 7 && buttons.mounted_buttons().size() == 5 &&
                    inputs.mounted_inputs().size() == 2,
                "native Card inventory differs");
        const auto main = cards.mounted()[0];
        const auto themed = cards.mounted()[6];
        const auto first = cards.snapshot(main);
        const auto editor = inputs.mounted_inputs()[0];
        const auto select_id = services.select().mounted()[0];
        const auto select_first = services.select().snapshot(select_id);
        page(0);
        draw("initial");
        const auto count = services.components().component_count();
        const auto extra = buttons.mounted_buttons()[0];
        const auto position = point(extra.component);
        services.pointer().dispatch({input::PointerIdentity::mouse(), input::PointerAction::move,
                                     input::PointerButton::none, position.x, position.y});
        draw("hover");
        require(cards.snapshot(main).hovered, "native Card nested hover failed");
        click(extra.component);
        require(activations == 1, "native Card nested click failed");
        require(services.focus().request_focus(extra.interaction, input::FocusModality::keyboard),
                "native Card action focus failed");
        key(input::Key::enter);
        draw("keyboard-focus");
        require(activations == 2, "native Card nested keyboard failed");
        require(editor_ref.focus(), "native Card editor ref focus failed");
        static_cast<void>(inputs.editors().require(editor.editor).select_all());
        require(
            bool(inputs.dispatch(input::CompositionChanged{String{u8"zhanghu"}, {7, 0}, inputs.sessions().active()})),
            "native Card composition failed");
        draw("composition");
        const String draft{u8"保留账户 / Retained account"};
        require(bool(inputs.dispatch(input::TextCommitted{draft, inputs.sessions().active()})),
                "native Card text commit failed");
        draw("edited");
        loading.set(true);
        draw("loading");
        require(cards.snapshot(main).loading && !inputs.sessions().active().valid() &&
                    !services.components().branch_active(first.body),
                "native Card loading retained focused editor");
        loading.set(false);
        draw("restored");
        require(inputs.editors().require(editor.editor).value() == draft.bytes(), "native Card loading lost draft");
        click(select_first.selector);
        draw("nested-popup");
        require(services.select().snapshot(select_id).visible, "native Card nested popup failed");
        loading.set(true);
        draw("nested-hidden");
        require(!services.select().snapshot(select_id).visible, "native Card loading retained nested popup");
        loading.set(false);
        page(1);
        draw("variants");
        const auto nested = buttons.mounted_buttons()[2];
        click(nested.component);
        draw("borderless-hover");
        require(activations == 3 && cards.snapshot(cards.mounted()[3]).hovered,
                "native borderless Card swallowed child action");
        page(2);
        draw("rich-slots");
        click(buttons.mounted_buttons()[3].component);
        draw("actions");
        require(activations == 4, "native Card footer action failed");
        const auto themed_node = services.components().root(themed);
        const auto measures = nodes.require(themed_node).measure_count;
        auto changed = initial_custom;
        changed.card.tokens.background = Color::rgba8(255, 240, 246);
        changed.card.tokens.header_background = Color::rgba8(255, 214, 231);
        custom.set(changed);
        draw("themed");
        require(nodes.require(themed_node).measure_count == measures, "native Card color-only update remeasured");
        title.set(String{});
        draw("empty-title");
        require(!cards.snapshot(themed).has_header, "native Card empty title retained header");
        width.set(dp(160));
        page(0);
        draw("narrow");
        require(bounds(main).width == 160 && bounds(editor.component).width <= 112,
                "native narrow Card content escaped available width");
        require(SDL_SetWindowSize(static_cast<SDL_Window*>(platform.window()), 1100, 900), "native Card resize failed");
        platform.delay(100);
        draw("resized");
        require(bounds(main).x + bounds(main).width <= viewport.width &&
                    bounds(main).y + bounds(main).height <= viewport.height,
                "native resized Card escaped viewport");
        services.set_window_active(false);
        draw("inactive");
        static_cast<void>(frames.consume_request());
        const auto submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            (void)platform.poll_events();
            (void)services.tick_animations(animation::AnimationTime::microseconds(++frame_number * 250'000));
            require(!frames.pending() && !services.next_frame_deadline() &&
                        renderer.counters().frame_submissions == submissions,
                    "native Card did not settle idle");
        }
        require(content_runs == 1 && body_runs == 1 && services.components().component_count() == count &&
                    cards.snapshot(main).body == first.body && inputs.mounted_inputs()[0].editor == editor.editor &&
                    services.select().snapshot(select_id).rows == select_first.rows,
                "native Card ordinary changes remounted content");
        std::cout
            << "card_acceptance=passed gpu_driver=" << renderer.gpu_driver()
            << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
            << " render_scale=" << scale << " theme=" << (dark ? "dark" : "light")
            << " nested_input=passed automatic_ime=passed hover=passed loading=passed restoration=passed actions=passed"
            << " narrow=passed resize=passed idle_polls=3 identity=passed submits="
            << renderer.counters().frame_submissions << '\n';
        for (const auto weight : {400U, 600U}) {
            const auto face = detail::resolve_platform_face(SystemFontFamily::ui_sans, weight, false);
            require(bool(face), "native Card system face missing");
            std::cout << "font=" << weight << " family=" << face->family_name << " path=" << face->source_path.string()
                      << " face_index=" << face->face_index << " weight=" << face->weight << '\n';
        }
        for (const auto& fallback : font_chain.diagnostic_fallbacks) {
            std::cout << "font_fallback=" << fallback << '\n';
        }
        services.dispose();
        require(cards.mounted().empty() && inputs.mounted_inputs().empty() && buttons.mounted_buttons().empty() &&
                    services.interactions().size() == 0 && nodes.size() == 0 && scene.size() == 0 &&
                    !editor_ref.bound() && !services.next_frame_deadline(),
                "native Card disposal leaked resources");
        std::cout << "disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "card_acceptance_error=" << error.what() << "\nexit_code=1\n";
        return 1;
    }
}
} // namespace rynui::example
