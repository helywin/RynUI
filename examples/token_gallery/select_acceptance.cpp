#include "component/select_component.hpp"
#include "component/input_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "renderer/common/scene_resources.hpp"
#include "token_gallery_definition.hpp"
#include "gallery_scroll_region.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <ryn/rynui.hpp>
#include <thread>
#include <SDL3/SDL_video.h>

namespace rynui::example {
namespace {
using namespace ryn;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct SelectPageState {
    std::size_t selected{};
    std::vector<runtime::ComponentId> pages;
};
} // namespace

int run_select_acceptance(int argc, char** argv) {
    try {
        std::optional<float> requested_scale;
        bool dark = false;
        std::filesystem::path directory;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg.starts_with("--acceptance-scale=")) {
                requested_scale = std::stof(std::string{arg.substr(19)});
            }
            if (arg.starts_with("--evidence-dir=")) {
                directory = std::filesystem::path{arg.substr(15)};
            }
            if (arg == "--acceptance-theme=dark") {
                dark = true;
            }
        }
        require(!directory.empty(), "Select acceptance requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Select Acceptance";
        config.width = 1280;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require(bool(created), "native window creation failed");
        auto& platform = *created.state;
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require(scale == 1 || scale == 1.25F || scale == 1.5F || scale == 2, "invalid acceptance scale");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require(bool(font_result), "font runtime creation failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto font_chain = detail::load_default_ui_font_chain(*fonts, request);
        require(bool(font_chain), "native font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, font_chain, scale);
        Signal<ThemeConfig> theme{ThemeConfig{}};
        ThemeConfig initial_theme;
        initial_theme.seed.motion = false;
        if (dark) {
            initial_theme.algorithms = {ThemeAlgorithm::Dark};
        }
        theme.set(initial_theme);
        Signal<SelectSelection> value{SelectSelection{u8"desktop"}};
        Signal<bool> open{false};
        Signal<bool> loading{true};
        Signal<std::vector<SelectOption>> options{std::vector<SelectOption>{
            {u8"desktop", u8"桌面 / Desktop"}, {u8"service", u8"服务 / Service"}, {u8"disabled", u8"禁用", true}}};
        SelectRef reference;
        int changes{};
        int searches{};
        int content_runs{};
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        detail::InputComponentHost inputs{services, platform, platform};
        inputs.set_display_scale(scale);
        runtime::ComponentId root;
        std::vector<runtime::ComponentId> pages;
        const std::vector<SelectOption> choices{{1, u8"One / 一"}, {2, u8"Disabled / 禁用", true}, {3, u8"Three / 三"}};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      auto& build = runtime::require_component_build_context();
                      root = build.mount_component<SelectPageState>();
                      const auto page = [&](Content content) {
                          build.mount_slot(root, Content{[&] {
                                               auto& context = runtime::require_component_build_context();
                                               const auto id = context.mount_component<int>(0);
                                               pages.push_back(id);
                                               services.layout().set_layout(
                                                   context.root(id),
                                                   layout::FlexLayout{layout::FlexDirection::vertical, 4, {}, true});
                                               context.mount_slot(id, content);
                                           }});
                      };
                      const auto field = [&](SelectProps props, String label) {
                          Text(TextProps{}.content(std::move(label)));
                          Select(props.layout(LayoutStyle{}.width(dp(260))));
                      };
                      page(Content{[&] {
                          field(SelectProps{}
                                    .options(options)
                                    .value(value)
                                    .open(open)
                                    .showSearch()
                                    .allowClear()
                                    .ref(reference)
                                    .placeholder(u8"请选择 / Search")
                                    .onChange([&](SelectSelection next) {
                                        ++changes;
                                        value.set(std::move(next));
                                    })
                                    .onOpenChange([&](bool next) { open.set(next); })
                                    .onSearch([&](String) { ++searches; }),
                                u8"Controlled / 搜索与清空");
                          field(SelectProps{}.options(choices).defaultValue(1), u8"Numeric / 数值");
                          field(SelectProps{}.options(choices).disabled(), u8"Disabled / 禁用");
                          field(SelectProps{}.options(choices).loading(loading), u8"Loading / 加载");
                          field(SelectProps{}.showSearch().notFoundContent(u8"没有匹配项 / Empty"), u8"Empty / 空列表");
                      }});
                      page(Content{[&] {
                          for (const auto variant : {InputVariant::Outlined, InputVariant::Filled,
                                                     InputVariant::Borderless, InputVariant::Underlined}) {
                              field(SelectProps{}.options(choices).defaultValue(1).variant(variant),
                                    u8"Outlined / Filled / Borderless / Underlined");
                          }
                      }});
                      page(Content{[&] {
                          for (const auto size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
                              field(SelectProps{}.options(choices).defaultValue(1).size(size),
                                    u8"Small / Middle / Large");
                          }
                          field(SelectProps{}.options(choices).status(InputStatus::Warning), u8"Warning / 警告");
                          field(SelectProps{}.options(choices).status(InputStatus::Error), u8"Error / 错误");
                      }});
                      page(Content{[&] {
                          ThemeConfig custom;
                          custom.select.tokens.option_selected_background = Color::rgba8(220, 200, 250);
                          Theme(ThemeProps{}.config(custom), ThemeContent{[&] {
                                    field(SelectProps{}.options(choices).defaultValue(1),
                                          u8"Component Token / 局部主题");
                                }});
                          std::vector<SelectOption> many;
                          for (int index = 0; index < 60; ++index) {
                              many.push_back(
                                  {index,
                                   String::from_utf8(std::string{"Option / 选项 "} + std::to_string(index)).value()});
                          }
                          field(SelectProps{}.options(std::move(many)).listHeight(160), u8"Scrollable / 滚动与边缘");
                      }});
                      build.state<SelectPageState>(root).pages = pages;
                      services.layout().set_layout(
                          build.root(root),
                          layout::ComponentLayout{
                              [&](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                  const auto& state = *services.components().state<SelectPageState>(root);
                                  return engine.measure_child(services.components().root(state.pages[state.selected]),
                                                              limits);
                              },
                              [&](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                                  const auto& state = *services.components().state<SelectPageState>(root);
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
                        "native Select layout failed");
                require(resources.synchronize({&services.surfaces().instances(),
                                               scene.atlas(),
                                               scene.glyph_scene().instances(),
                                               &services.rounded_effects(),
                                               {static_cast<std::uint32_t>(metrics.pixel_width),
                                                static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                        "native Select GPU upload failed");
                require(backend.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                        "native Select scene attachment failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require(renderer.submit_frame(time) != runtime::FrameSubmissionResult::failed,
                        "native Select frame failed");
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
            }
            require(renderer.save_frame_bmp(directory / (name + ".bmp")), "native Select readback failed");
        };
        const auto page = [&](std::size_t selected) {
            services.focus().clear_focus();
            services.components().state<SelectPageState>(root)->selected = selected;
            for (std::size_t index = 0; index < pages.size(); ++index) {
                services.components().set_branch_active(pages[index], index == selected);
            }
            services.mark_scene_structure_dirty();
            dirty.invalidate(services.components().root(root), runtime::DirtyFlags::Measure |
                                                                   runtime::DirtyFlags::Layout |
                                                                   runtime::DirtyFlags::Geometry);
        };
        const auto bounds = [&](runtime::ComponentId component) {
            const auto& node = nodes.require(services.components().root(component));
            return runtime::Rect{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                 node.bounds.width, node.bounds.height};
        };
        const auto click = [&](runtime::ComponentId component) {
            const auto rect = bounds(component);
            for (const auto action : {input::PointerAction::down, input::PointerAction::up}) {
                services.pointer().dispatch({input::PointerIdentity::mouse(), action, input::PointerButton::primary,
                                             rect.x + rect.width / 2, rect.y + rect.height / 2});
            }
        };
        const auto key = [&](input::Key key) {
            services.focus().dispatch({key, input::KeyAction::down});
            services.focus().dispatch({key, input::KeyAction::up});
        };
        auto& selects = services.select();
        const auto mounted = selects.mounted();
        require(mounted.size() == 16, "native Select inventory differs");
        const auto main = mounted[0];
        const auto selector = selects.snapshot(main).selector;
        const auto rows = selects.snapshot(main).rows;
        page(0);
        draw("states");
        const auto component_count = services.components().component_count();
        click(selector);
        draw("opened");
        require(selects.snapshot(main).visible && inputs.sessions().active().valid(),
                "native Select did not open/focus");
        click(rows[1]);
        draw("selected");
        require(!selects.snapshot(main).visible && value.get() == SelectSelection{u8"service"} && changes == 1,
                "native Select controlled pointer selection failed");
        bool cleared = false;
        for (auto id : services.interactions().declaration_order()) {
            const auto& interaction = services.interactions().require(id);
            if (!interaction.eligible || interaction.focusable || interaction.component == selector) {
                continue;
            }
            for (auto parent = services.components().parent(interaction.component); parent;
                 parent = services.components().parent(*parent)) {
                if (*parent == selector) {
                    click(interaction.component);
                    cleared = true;
                    break;
                }
            }
            if (cleared) {
                break;
            }
        }
        require(cleared && !value.get() && changes == 2, "native Select clear failed");
        draw("cleared");
        click(selector);
        draw("search-open");
        const auto stamp = inputs.sessions().active();
        require(bool(inputs.dispatch(input::CompositionChanged{String{u8"fu"}, {2, 0}, stamp})),
                "native Select preedit failed");
        key(input::Key::enter);
        require(selects.snapshot(main).visible && !value.get(), "native Select stole IME Enter");
        key(input::Key::escape);
        require(selects.snapshot(main).visible, "native Select IME Escape closed popup");
        require(bool(inputs.dispatch(input::TextCommitted{String{u8"服务"}, stamp})),
                "native Select text commit failed");
        draw("search-ime");
        require(selects.snapshot(main).query == String{u8"服务"} && selects.snapshot(main).active == 1 && searches > 0,
                "native Select Unicode search failed");
        key(input::Key::enter);
        require(value.get() == SelectSelection{u8"service"} && changes == 3, "native Select search selection failed");
        click(selector);
        require(bool(inputs.dispatch(input::TextCommitted{String{u8"missing"}, inputs.sessions().active()})),
                "native Select empty query failed");
        draw("not-found");
        require(!selects.snapshot(main).active, "native Select empty result has active option");
        key(input::Key::escape);
        click(selects.snapshot(mounted[1]).selector);
        key(input::Key::home);
        key(input::Key::down);
        key(input::Key::enter);
        require(selects.snapshot(mounted[1]).value == SelectSelection{3},
                "native Select numeric/disabled navigation failed");
        click(selects.snapshot(mounted[2]).selector);
        require(!selects.snapshot(mounted[2]).visible, "native disabled Select opened");
        loading.set(false);
        draw("keyboard");
        page(1);
        draw("variants");
        page(2);
        draw("sizes-status");
        page(3);
        draw("theme-scroll");
        const auto long_id = mounted[15];
        const auto long_selector = selects.snapshot(long_id).selector;
        click(long_selector);
        draw("scroll-open");
        const auto menu = selects.snapshot(long_id).popup;
        require(bool(inputs.dispatch(input::ScrollInputEvent{0, -3, menu.x + 10, menu.y + 10})),
                "native Select did not consume wheel");
        draw("scrolled");
        require(selects.snapshot(long_id).scroll > 0, "native Select wheel did not scroll");
        key(input::Key::end);
        draw("last-option");
        require(selects.snapshot(long_id).active == 59, "native Select End did not reveal last option");
        key(input::Key::escape);
        GalleryScrollTranslation edge_translation;
        const auto long_node = services.components().root(long_id);
        require(
            edge_translation.apply(long_node, -(viewport.height - bounds(long_selector).y - 36), nodes, dirty).changed,
            "native Select edge translation failed");
        draw("edge-anchor");
        click(long_selector);
        draw("edge-popup");
        require(selects.snapshot(long_id).above, "native Select did not flip at lower edge");
        require(SDL_SetWindowSize(static_cast<SDL_Window*>(platform.window()), 1100, 900),
                "native Select resize failed");
        platform.delay(100);
        require(edge_translation.apply(long_node, 0, nodes, dirty).changed, "native Select translation reset failed");
        draw("resized");
        const auto resized = selects.snapshot(long_id).popup;
        require(resized.x >= 0 && resized.y >= 0 && resized.x + resized.width <= viewport.width + 0.01F &&
                    resized.y + resized.height <= viewport.height + 0.01F,
                "native Select resized popup escaped viewport");
        services.set_window_active(false);
        draw("inactive");
        require(!selects.snapshot(long_id).visible && !inputs.sessions().active().valid(),
                "native Select inactivity cleanup failed");
        (void)frames.consume_request();
        const auto submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            (void)platform.poll_events();
            (void)services.tick_animations(animation::AnimationTime::microseconds(++frame_number * 250'000));
            require(!frames.pending() && !services.next_frame_deadline() &&
                        renderer.counters().frame_submissions == submissions,
                    "native Select did not settle idle");
        }
        require(content_runs == 1 && services.components().component_count() == component_count &&
                    selects.snapshot(main).selector == selector && selects.snapshot(main).rows == rows,
                "native Select ordinary changes remounted content");
        const auto regular = detail::resolve_platform_face(SystemFontFamily::ui_sans, 400, false);
        require(bool(regular), "native Select font missing");
        std::cout << "select_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " theme=" << (dark ? "dark" : "light")
                  << " selection=passed clear=passed search=passed automatic_ime=passed keyboard=passed"
                  << " scroll=passed edge=passed resize=passed idle_polls=3 identity=passed submits="
                  << renderer.counters().frame_submissions << '\n';
        std::cout << "font=regular family=" << regular->family_name << " path=" << regular->source_path.string()
                  << " face_index=" << regular->face_index << " weight=" << regular->weight << '\n';
        for (const auto& fallback : font_chain.diagnostic_fallbacks) {
            std::cout << "font_fallback=" << fallback << '\n';
        }
        services.dispose();
        require(selects.mounted().empty() && inputs.mounted_inputs().empty() && services.interactions().size() == 0 &&
                    nodes.size() == 0 && scene.size() == 0 && !reference.bound() && !services.next_frame_deadline(),
                "native Select disposal leaked resources");
        std::cout << "disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "select_acceptance_error=" << error.what() << "\nexit_code=1\n";
        return 1;
    }
}
} // namespace rynui::example
