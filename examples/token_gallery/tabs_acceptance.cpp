#include "component/tabs_component.hpp"
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

struct TabsPageState {
    std::size_t selected{};
    std::vector<runtime::ComponentId> pages;
};
} // namespace

int run_tabs_acceptance(int argc, char** argv) {
    try {
        std::optional<float> requested_scale;
        bool dark{};
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
        require(!directory.empty(), "Tabs acceptance requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Tabs Acceptance";
        config.width = 1280;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require(bool(created), "native Tabs window creation failed");
        auto& platform = *created.state;
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require(scale == 1 || scale == 1.25F || scale == 1.5F || scale == 2, "invalid acceptance scale");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require(bool(font_result), "native Tabs font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        auto font_chain = detail::load_default_ui_font_chain(*fonts, font_request);
        require(bool(font_chain), "native Tabs font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, font_chain, scale);
        ThemeConfig initial_theme;
        initial_theme.seed.motion = false;
        if (dark) {
            initial_theme.algorithms = {ThemeAlgorithm::Dark};
        }
        Signal<ThemeConfig> theme{initial_theme};
        Signal<ThemeConfig> custom{ThemeConfig{}};
        Signal<LogicalLength> width{dp(300)};
        Signal<String> active{String{u8"draft"}};
        Signal<String> label{String{u8"Settings / 设置"}};
        Signal<bool> disabled{false};
        InputRef editor_ref;
        bool accept{true};
        int changes{}, clicks{}, content_runs{}, panel_runs{};
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
                    root = build.mount_component<TabsPageState>();
                    const auto page = [&](Content content) {
                        build.mount_slot(root, Content{[&] {
                                             auto& context = runtime::require_component_build_context();
                                             const auto id = context.mount_component<int>(0);
                                             pages.push_back(id);
                                             services.layout().set_layout(
                                                 context.root(id),
                                                 layout::FlexLayout{layout::FlexDirection::vertical, 8, {}, true});
                                             context.mount_slot(id, content);
                                         }});
                    };
                    const auto props = [&] {
                        return TabsProps{}.layout(LayoutStyle{}.width(width));
                    };
                    const auto basic = [] {
                        return std::vector<TabItem>{
                            TabItem{u8"one", u8"One / 一", TabContent{[] { Text(u8"First panel / 第一页"); }}},
                            TabItem{u8"two", u8"Two / 二", TabContent{[] { Text(u8"Second panel / 第二页"); }}}};
                    };
                    page(Content{[&] {
                        Text(u8"Controlled tabs / 保留编辑与弹层清理");
                        Tabs(props()
                                 .activeKey(active)
                                 .onChange([&](String key) {
                                     ++changes;
                                     if (accept) {
                                         active.set(std::move(key));
                                     }
                                 })
                                 .onTabClick([&](String) { ++clicks; }),
                             {TabItem{u8"draft", u8"Draft / 草稿", TabContent{[&] {
                                          ++panel_runs;
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
                                      }}},
                              TabItem{u8"settings", u8"Settings / 设置", TabContent{[&] {
                                          ++panel_runs;
                                          Input(InputProps{}
                                                    .autoFocus()
                                                    .defaultValue(u8"Second draft / 另一草稿")
                                                    .layout(LayoutStyle{}.width(dp(180))));
                                      }}},
                              TabItem{u8"locked", u8"Locked / 禁用", TabContent{[] { Text(u8"Locked"); }}}.disabled()});
                    }});
                    page(Content{[&] {
                        Text(u8"Line small / 小号");
                        Tabs(props().size(ControlSize::Small), basic());
                        Text(u8"Card middle / 卡片");
                        Tabs(props().type(TabsType::Card), basic());
                        Text(u8"Card large bottom / 大号底部");
                        Tabs(props().type(TabsType::Card).size(ControlSize::Large).placement(TabsPlacement::Bottom),
                             basic());
                        Text(u8"Centered bottom / 居中底部");
                        Tabs(props().placement(TabsPlacement::Bottom).centered(), basic());
                    }});
                    page(Content{[&] {
                        Text(u8"Overflow / 横向溢出");
                        std::vector<TabItem> items;
                        for (int i = 1; i <= 8; ++i) {
                            auto key = String::from_utf8(std::to_string(i)).value();
                            items.emplace_back(key, String::from_utf8("Project / 项目 " + std::to_string(i)).value(),
                                               TabContent{[key] { Text(key); }});
                        }
                        Tabs(props(), std::move(items));
                        Text(u8"Dynamic label / 响应标签");
                        auto dynamic = basic();
                        dynamic[1].label(label).disabled(disabled);
                        Tabs(props(), std::move(dynamic));
                        Theme(ThemeProps{}.config(custom), ThemeContent{[&] {
                                  Text(u8"Component Token / 局部主题");
                                  Tabs(props().type(TabsType::Card), basic());
                              }});
                    }});
                    build.state<TabsPageState>(root).pages = pages;
                    services.layout().set_layout(
                        build.root(root),
                        layout::ComponentLayout{
                            [&](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                const auto& state = *services.components().state<TabsPageState>(root);
                                return engine.measure_child(services.components().root(state.pages[state.selected]),
                                                            limits);
                            },
                            [&](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                                const auto& state = *services.components().state<TabsPageState>(root);
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
                        "native Tabs layout failed");
                require(resources.synchronize({&services.surfaces().instances(),
                                               scene.atlas(),
                                               scene.glyph_scene().instances(),
                                               &services.rounded_effects(),
                                               {static_cast<std::uint32_t>(metrics.pixel_width),
                                                static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                        "native Tabs GPU upload failed");
                require(backend.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                        "native Tabs scene attachment failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require(renderer.submit_frame(time) != runtime::FrameSubmissionResult::failed,
                        "native Tabs frame failed");
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
            }
            require(renderer.save_frame_bmp(directory / (name + ".bmp")), "native Tabs readback failed");
        };
        const auto page = [&](std::size_t selected) {
            services.focus().clear_focus();
            services.components().state<TabsPageState>(root)->selected = selected;
            for (std::size_t i = 0; i < pages.size(); ++i) {
                services.components().set_branch_active(pages[i], selected == i);
            }
            services.mark_scene_structure_dirty();
            dirty.invalidate(services.components().root(root), runtime::DirtyFlags::Measure |
                                                                   runtime::DirtyFlags::Layout |
                                                                   runtime::DirtyFlags::Geometry);
        };
        const auto bounds = [&](runtime::ComponentId id) {
            const auto& node = nodes.require(services.components().root(id));
            auto result = node.bounds;
            result.x += node.translation.x;
            result.y += node.translation.y;
            return result;
        };
        const auto click = [&](runtime::ComponentId id) {
            const auto rect = bounds(id);
            for (auto action : {input::PointerAction::down, input::PointerAction::up}) {
                services.pointer().dispatch({input::PointerIdentity::mouse(), action, input::PointerButton::primary,
                                             rect.x + rect.width / 2, rect.y + rect.height / 2});
            }
        };
        const auto focus = [&](runtime::ComponentId id) {
            for (auto hit : services.interactions().declaration_order()) {
                if (services.interactions().require(hit).component == id) {
                    return services.focus().request_focus(hit, input::FocusModality::keyboard);
                }
            }
            return false;
        };
        const auto key = [&](input::Key key) {
            services.focus().dispatch({key, input::KeyAction::down});
            services.focus().dispatch({key, input::KeyAction::up});
        };
        auto& tabs = services.tabs();
        require(tabs.mounted().size() == 8 && inputs.mounted_inputs().size() == 3 &&
                    !inputs.sessions().active().valid(),
                "native Tabs inventory or inactive autofocus failed");
        const auto main = tabs.mounted()[0];
        const auto overflow = tabs.mounted()[5];
        const auto themed = tabs.mounted()[7];
        const auto first = tabs.snapshot(main);
        const auto editor = inputs.mounted_inputs()[0];
        const auto select = services.select().mounted()[0];
        const auto select_first = services.select().snapshot(select);
        page(0);
        draw("initial");
        const auto count = services.components().component_count();
        require(focus(first.headers[0]), "native Tabs header focus failed");
        key(input::Key::end);
        draw("keyboard-focus");
        require(tabs.snapshot(main).active == 1, "native Tabs End failed to skip disabled");
        key(input::Key::home);
        require(editor_ref.focus(), "native Tabs editor focus failed");
        static_cast<void>(inputs.editors().require(editor.editor).select_all());
        require(
            bool(inputs.dispatch(input::CompositionChanged{String{u8"caogao"}, {6, 0}, inputs.sessions().active()})),
            "native Tabs composition failed");
        draw("composition");
        const String draft{u8"保留草稿 / Retained draft"};
        require(bool(inputs.dispatch(input::TextCommitted{draft, inputs.sessions().active()})),
                "native Tabs commit failed");
        draw("edited");
        click(first.headers[1]);
        draw("switched");
        require(tabs.snapshot(main).active == 1 && !inputs.sessions().active().valid(),
                "native Tabs switch retained input");
        click(first.headers[0]);
        draw("restored");
        require(inputs.editors().require(editor.editor).value() == draft.bytes(), "native Tabs lost draft");
        click(select_first.selector);
        draw("nested-popup");
        require(services.select().snapshot(select).visible, "native Tabs nested popup failed");
        click(first.headers[1]);
        draw("nested-hidden");
        require(!services.select().snapshot(select).visible, "native Tabs retained hidden popup");
        active.set(String{u8"draft"});
        accept = false;
        key(input::Key::end);
        draw("controlled-refused");
        require(tabs.snapshot(main).active == 0, "native Tabs ignored controlled refusal");
        accept = true;
        key(input::Key::enter);
        draw("controlled-accepted");
        require(tabs.snapshot(main).active == 1, "native Tabs controlled echo failed");
        page(1);
        draw("variants");
        page(2);
        draw("overflow-initial");
        auto nav = tabs.snapshot(overflow);
        require(nav.overflow, "native Tabs overflow absent");
        click(nav.next);
        draw("overflow-scroll");
        require(tabs.snapshot(overflow).scroll > 0, "native Tabs arrow failed");
        require(focus(nav.headers[0]), "native Tabs offscreen focus failed");
        key(input::Key::end);
        draw("overflow-end");
        nav = tabs.snapshot(overflow);
        const auto last = bounds(nav.headers.back());
        require(nav.active == 7 && last.x >= nav.viewport.x &&
                    last.x + last.width <= nav.viewport.x + nav.viewport.width,
                "native Tabs End did not reveal header");
        require(inputs.dispatch(input::ScrollInputEvent{0, 1, nav.viewport.x + 5, nav.viewport.y + 5}),
                "native Tabs wheel failed");
        const auto themed_node = services.components().root(themed);
        const auto measures = nodes.require(themed_node).measure_count;
        ThemeConfig changed;
        changed.tabs.tokens.item_selected = Color::rgba8(235, 47, 150);
        changed.tabs.tokens.card_selected_background = Color::rgba8(255, 240, 246);
        custom.set(changed);
        draw("themed");
        require(nodes.require(themed_node).measure_count == measures, "native Tabs color update remeasured");
        label.set(String{u8"Disabled / 不可用"});
        disabled.set(true);
        width.set(dp(160));
        draw("narrow");
        require(bounds(overflow).width == 160, "native narrow Tabs width failed");
        require(SDL_SetWindowSize(static_cast<SDL_Window*>(platform.window()), 1100, 900), "native Tabs resize failed");
        platform.delay(100);
        draw("resized");
        require(bounds(themed).x + bounds(themed).width <= viewport.width &&
                    bounds(themed).y + bounds(themed).height <= viewport.height,
                "native resized Tabs escaped viewport");
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
                    "native Tabs did not settle idle");
        }
        require(content_runs == 1 && panel_runs == 2 && services.components().component_count() == count &&
                    tabs.snapshot(main).panels == first.panels && inputs.mounted_inputs()[0].editor == editor.editor &&
                    services.select().snapshot(select).rows == select_first.rows && changes > 0 && clicks >= changes,
                "native Tabs ordinary changes remounted content");
        std::cout << "tabs_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " theme=" << (dark ? "dark" : "light")
                  << " keyboard=passed controlled=passed overflow=passed wheel=passed"
                  << " nested_input=passed automatic_ime=passed restoration=passed popup_cleanup=passed"
                  << " narrow=passed resize=passed idle_polls=3 identity=passed submits="
                  << renderer.counters().frame_submissions << '\n';
        for (auto weight : {400U, 600U}) {
            const auto face = detail::resolve_platform_face(SystemFontFamily::ui_sans, weight, false);
            require(bool(face), "native Tabs system face missing");
            std::cout << "font=" << weight << " family=" << face->family_name << " path=" << face->source_path.string()
                      << " face_index=" << face->face_index << " weight=" << face->weight << '\n';
        }
        for (const auto& fallback : font_chain.diagnostic_fallbacks) {
            std::cout << "font_fallback=" << fallback << '\n';
        }
        services.dispose();
        require(tabs.mounted().empty() && inputs.mounted_inputs().empty() && services.select().mounted().empty() &&
                    services.interactions().size() == 0 && nodes.size() == 0 && scene.size() == 0 &&
                    !editor_ref.bound() && !services.next_frame_deadline(),
                "native Tabs disposal leaked resources");
        std::cout << "disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "tabs_acceptance_error=" << error.what() << "\nexit_code=1\n";
        return 1;
    }
}
} // namespace rynui::example
