#include "component/divider_component.hpp"
#include "component/input_component.hpp"
#include "component/typography_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "token_gallery_definition.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <ryn/rynui.hpp>
#include <thread>

namespace rynui::example {
namespace {
using namespace ryn;
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct PageState {
  std::size_t selected{};
  std::vector<runtime::ComponentId> pages;
};
} // namespace
int run_typography_acceptance(int argc, char **argv) {
  try {
    std::optional<float> requested_scale;
    bool copy_only = false, dark = false;
    std::filesystem::path directory;
    for (int i = 1; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg.starts_with("--acceptance-scale="))
        requested_scale = std::stof(std::string{arg.substr(19)});
      if (arg.starts_with("--evidence-dir="))
        directory = std::filesystem::path{arg.substr(15)};
      if (arg == "--copy-only")
        copy_only = true;
      if (arg == "--acceptance-theme=dark")
        dark = true;
    }
    require(!directory.empty(),
            "typography acceptance requires --evidence-dir");
    std::filesystem::create_directories(directory);
    detail::PlatformConfig config;
    config.title = "RynUI Typography / Divider Acceptance";
    config.width = 1280;
    config.height = 1000;
    auto created = detail::PlatformState::create(config);
    require(bool(created), "native window creation failed");
    auto &platform = *created.state;
    const auto metrics = platform.window_metrics();
    const float scale = requested_scale.value_or(metrics.display_scale);
    require(scale == 1 || scale == 1.25F || scale == 1.5F || scale == 2,
            "acceptance scale is invalid");
    const auto executable = std::filesystem::absolute(argv[0]).parent_path();
    auto font_result = font::FontRuntime::create();
    require(bool(font_result), "font runtime creation failed");
    auto fonts = std::move(font_result.runtime);
    detail::DefaultFontChainRequest font_request;
    font_request.raster = {14, scale};
    font_request.fallback_latin = executable / "fonts/latin.ttf";
    font_request.fallback_cjk = executable / "fonts/cjk.otf";
    auto font_chain = detail::load_default_ui_font_chain(*fonts, font_request);
    require(bool(font_chain), "native font chain failed");
    auto resolver =
        detail::make_default_ui_font_resolver(*fonts, font_chain, scale);
    struct Request {
      SystemFontFamily family;
      std::uint32_t weight;
      bool italic;
      std::uint32_t pixels;
      std::vector<font::FontIdentity> faces;
    };
    std::vector<Request> requests;
    runtime::NodeStore nodes;
    runtime::FrameRequestState frames;
    runtime::DirtyQueues dirty{nodes, &frames};
    layout::LayoutEngine layout{nodes};
    text::TextEngine engine{*fonts};
    detail::TextSceneService scene{*fonts, engine, frames};
    detail::WindowComponentServices services{
        nodes,
        layout,
        dirty,
        scene,
        [&](SystemFontFamily family, std::uint32_t weight, bool italic,
            std::uint32_t pixels) {
          auto result = resolver(family, weight, italic, pixels);
          requests.push_back({family, weight, italic, pixels, result});
          return result;
        },
        frames};
    services.bind_clipboard(platform);
    std::unique_ptr<detail::InputComponentHost> input;
    if (!copy_only) {
      input = std::make_unique<detail::InputComponentHost>(services, platform,
                                                           platform);
      input->set_display_scale(scale);
    }
    Signal<ThemeConfig> theme{ThemeConfig{}};
    if (dark) {
      ThemeConfig value;
      value.algorithms = {ThemeAlgorithm::Dark};
      theme.set(value);
    }
    Signal<String> edited{String{u8"标题草稿 / Heading draft"}};
    int edits{}, links{};
    const String original{u8"Copy must retain the original complete text. "
                          u8"原始全文必须完整复制，显示省略不会改变数据。"};
    runtime::ComponentId root;
    std::vector<runtime::ComponentId> pages;
    const auto page_width = dp(
        std::max(0.0F, static_cast<float>(metrics.pixel_width) / scale - 32));
    services.mount(Content{[&] {
      Theme(
          ThemeProps{}.config(theme), ThemeContent{[&] {
            auto &build = runtime::require_component_build_context();
            root = build.mount_component<PageState>();
            const auto page = [&](Content content) {
              build.mount_slot(
                  root, Content{[&] {
                    auto &context = runtime::require_component_build_context();
                    const auto id = context.mount_component<int>(0);
                    pages.push_back(id);
                    services.layout().set_layout(
                        context.root(id),
                        layout::FlexLayout{
                            layout::FlexDirection::vertical, 4, {}, true});
                    context.mount_slot(id, content);
                  }});
            };
            page(Content{[] {
              for (auto level : {TypographyLevel::H1, TypographyLevel::H2,
                                 TypographyLevel::H3, TypographyLevel::H4,
                                 TypographyLevel::H5})
                Title(TitleProps{}.level(level).content(
                    u8"Typography 标题 / Heading"));
            }});
            page(Content{[&] {
              Text(TypographyProps{}
                       .content(u8"Regular / Strong / 中文")
                       .strong(true));
              Text(TypographyProps{}
                       .content(u8"Italic face / 斜体")
                       .italic(true));
              Text(TypographyProps{}
                       .content(u8"code: aa ii WW 123; 中文回退")
                       .code(true));
              Text(TypographyProps{}
                       .content(u8"Ctrl + C / Keyboard")
                       .keyboard(true));
              Text(TypographyProps{}
                       .content(u8"Mark + Underline + Strikeout")
                       .mark(true)
                       .underline(true)
                       .strikethrough(true));
              Text(TypographyProps{}
                       .content(u8"Secondary / 次要")
                       .type(TypographyType::Secondary));
              Text(TypographyProps{}
                       .content(u8"Disabled / 禁用")
                       .disabled(true)
                       .type(TypographyType::Danger));
              Text(TypographyProps{}
                       .content(original)
                       .ellipsis(TypographyEllipsis{.expandable = true})
                       .copyable(TypographyCopyable{})
                       .layout(LayoutStyle{}.width(dp(300))));
              Paragraph(
                  TypographyProps{}
                      .content(u8"Multiline ellipsis / 多行省略\nExplicit "
                               u8"newline / 显式换行\nLast line / 最后一行")
                      .ellipsis(
                          TypographyEllipsis{.rows = 2, .expandable = true})
                      .layout(LayoutStyle{}.width(page_width)));
              if (!copy_only)
                Title(TitleProps{}
                          .level(TypographyLevel::H3)
                          .content(edited)
                          .editable(TypographyEditable{})
                          .onEdit([&](String next) {
                            ++edits;
                            edited.set(std::move(next));
                          })
                          .layout(LayoutStyle{}.width(page_width)));
            }});
            page(Content{[&] {
              Link(LinkProps{}
                       .content(u8"Link activation / 链接激活")
                       .onClick([&] { ++links; }));
              Link(LinkProps{}
                       .content(u8"Disabled link / 禁用链接")
                       .disabled(true));
              Divider();
              Divider(DividerProps{}.content(u8"Center / 居中"));
              Divider(DividerProps{}
                          .content(u8"Left / 左侧")
                          .orientation(DividerOrientation::Left));
              Divider(DividerProps{}
                          .content(u8"Right / 右侧")
                          .orientation(DividerOrientation::Right)
                          .dashed(true));
              Divider(DividerProps{}.content(u8"Plain / 常规").plain(true));
              Divider(DividerProps{}
                          .content(u8"None / 无朝向边距")
                          .orientation(DividerOrientation::Left)
                          .orientationMargin(DividerOrientationMargin::none()));
              Space(SpaceProps{}.align(SpaceAlign::Center), [] {
                Text(u8"Left");
                Divider(DividerProps{}.type(DividerType::Vertical));
                Text(u8"Right");
              });
            }});
            auto &state = build.state<PageState>(root);
            state.pages = pages;
            services.layout().set_layout(
                build.root(root),
                layout::ComponentLayout{
                    [&](layout::LayoutEngine &engine, runtime::NodeId,
                        layout::Constraints limits) {
                      const auto &s =
                          *services.components().state<PageState>(root);
                      return engine.measure_child(
                          services.components().root(s.pages[s.selected]),
                          limits);
                    },
                    [&](layout::LayoutEngine &engine, runtime::NodeId,
                        runtime::Rect bounds) {
                      const auto &s =
                          *services.components().state<PageState>(root);
                      engine.place_child(
                          services.components().root(s.pages[s.selected]),
                          bounds);
                    }});
          }});
    }});
    detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
    detail::GlyphGpuResources glyphs{renderer};
    detail::RoundedEffectGpuResources effects{renderer};
    std::unique_ptr<graphics::QuadGpuBuffer> quads;
    const runtime::Size viewport{
        static_cast<float>(metrics.pixel_width) / scale,
        static_cast<float>(metrics.pixel_height) / scale};
    const runtime::Rect clip{0, 0, viewport.width, viewport.height};
    std::size_t frame_number{};
    const auto draw = [&](std::string name) {
      for (int settle = 0; settle < 2; ++settle) {
        (void)platform.poll_events();
        const auto time = animation::AnimationTime::microseconds(
            ++frame_number * 250'000);
        (void)services.tick_animations(time);
        require(services.layout_and_synchronize(viewport, clip, {16, 12}),
                "acceptance layout failed");
        require(renderer.begin_upload_batch(), "GPU upload begin failed");
        if (!quads)
          quads = std::make_unique<graphics::QuadGpuBuffer>(
              renderer, services.surfaces().instances());
        else
          services.surfaces().synchronize_gpu(*quads);
        glyphs.synchronize(scene.atlas(), scene.glyph_scene().instances());
        effects.synchronize(services.rounded_effects(),
                            {static_cast<std::uint32_t>(metrics.pixel_width),
                             static_cast<std::uint32_t>(metrics.pixel_height),
                             scale});
        require(renderer.finish_upload_batch(), "GPU upload finish failed");
        renderer.attach_scene(quads->handle(), glyphs,
                              services.scene_composer().ordered_scene(),
                              &effects);
        renderer.set_clear_color(
            resolve_theme(theme.get()).alias().color_background_container);
        require(renderer.submit_frame(time) !=
                    runtime::FrameSubmissionResult::failed,
                "D3D12 frame failed");
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
      }
      require(renderer.save_frame_bmp(directory / (name + ".bmp")),
              "GPU screenshot failed");
    };
    const auto select_page = [&](std::size_t selected) {
      services.components().state<PageState>(root)->selected = selected;
      for (std::size_t i = 0; i < pages.size(); ++i)
        services.components().set_branch_active(pages[i], i == selected);
      services.mark_scene_structure_dirty();
      dirty.invalidate(services.components().root(root),
                       runtime::DirtyFlags::Measure |
                           runtime::DirtyFlags::Layout |
                           runtime::DirtyFlags::Geometry);
    };
    const auto keyboard = [&](input::Key key, input::KeyAction action =
                                                  input::KeyAction::down) {
      services.focus().dispatch({key, action, input::KeyModifier::none, false,
                                 input::KeyModifier::control});
    };
    const auto activate = [&](input::InteractionId target) {
      (void)services.focus().request_focus(target,
                                           input::FocusModality::keyboard);
      require(services.focus().state().focused == target,
              "native action focus failed");
      keyboard(input::Key::enter);
      keyboard(input::Key::enter, input::KeyAction::up);
    };
    const auto actions = [&](runtime::ComponentId parent) {
      std::vector<input::InteractionId> result;
      for (auto id : services.interactions().declaration_order()) {
        auto component = services.interactions().require(id).component;
        for (auto current = services.components().parent(component); current;
             current = services.components().parent(*current))
          if (*current == parent) {
            result.push_back(id);
            break;
          }
      }
      return result;
    };
    select_page(0);
    draw("headings");
    select_page(1);
    draw("semantics");
    const auto mounted = services.typography().mounted();
    require(mounted.size() == (copy_only ? 2 : 3),
            "acceptance Typography inventory wrong");
    const auto single = mounted[0], multi = mounted[1];
    const auto single_actions = actions(single), multi_actions = actions(multi);
    require(services.typography().snapshot(single).truncated,
            "single-line ellipsis did not truncate");
    require(services.typography().snapshot(multi).truncated,
            "multi-line ellipsis did not truncate");
    const auto previous = platform.read_text();
    activate(single_actions[1]);
    const auto copied = platform.read_text();
    require(copied && *copied.text == original &&
                services.typography().snapshot(single).copied,
            "native clipboard did not copy full content");
    draw("copied");
    activate(single_actions[0]);
    require(services.typography().snapshot(single).expanded,
            "single expand failed");
    draw("expanded");
    activate(single_actions[0]);
    activate(multi_actions[0]);
    draw("multiline-expanded");
    activate(multi_actions[0]);
    if (input) {
      const auto editable = mounted[2];
      const auto edit_actions = actions(editable);
      const auto editor = input->mounted_inputs().front();
      activate(edit_actions[0]);
      require(services.focus().state().focused == editor.interaction &&
                  input->sessions().active().valid(),
              "native edit did not start IME session");
      draw("editing");
      require(input->layout_snapshot(editor.component).baseline > 20,
              "native heading edit lost typography");
      require(bool(input->dispatch(input::CompositionChanged{
                  String{u8"输入"}, {2, 0}, input->sessions().active()})),
              "native composition failed");
      keyboard(input::Key::escape);
      require(services.typography().snapshot(editable).editing,
              "IME Esc abandoned edit");
      keyboard(input::Key::escape);
      require(!services.typography().snapshot(editable).editing &&
                  !input->sessions().active().valid(),
              "native cancel retained session");
      activate(edit_actions[0]);
      (void)input->editors().require(editor.editor).select_all();
      require(bool(input->dispatch(input::TextCommitted{
                  String{u8"Committed / 已提交"}, input->sessions().active()})),
              "native draft update failed");
      keyboard(input::Key::enter);
      require(edits == 1 && edited.get() == String{u8"Committed / 已提交"} &&
                  !services.typography().snapshot(editable).editing,
              "native controlled commit failed");
      draw("committed");
    } else
      require(services.text_edit() == nullptr &&
                  services.input_runtime() == nullptr,
              "copy-only window constructed Input runtime");
    select_page(2);
    draw("dividers");
    std::vector<input::InteractionId> link_actions;
    for (auto id : services.interactions().declaration_order())
      if (services.components().parent(
              services.interactions().require(id).component) == pages[2])
        link_actions.push_back(id);
    require(link_actions.size() == 2, "Link acceptance inventory wrong");
    activate(link_actions[0]);
    require(links == 1, "native Link keyboard failed");
    const auto bounds =
        nodes.require(services.interactions().require(link_actions[0]).node)
            .bounds;
    const float x = bounds.x + bounds.width / 2,
                y = bounds.y + bounds.height / 2;
    services.pointer().dispatch({input::PointerIdentity::mouse(),
                                 input::PointerAction::move,
                                 input::PointerButton::none, x, y});
    services.pointer().dispatch({input::PointerIdentity::mouse(),
                                 input::PointerAction::down,
                                 input::PointerButton::primary, x, y});
    services.pointer().dispatch({input::PointerIdentity::mouse(),
                                 input::PointerAction::up,
                                 input::PointerButton::primary, x, y});
    require(links == 2 &&
                !services.interactions().require(link_actions[1]).eligible,
            "native Link pointer/disabled failed");
    (void)services.focus().request_focus(link_actions[0],
                                        input::FocusModality::keyboard);
    draw("link-focus");
    if (previous && previous.text)
      (void)platform.write_text(previous.text->view());
    auto regular = detail::resolve_platform_face(SystemFontFamily::ui_sans, 400,
                                                 false),
         strong = detail::resolve_platform_face(SystemFontFamily::ui_sans, 600,
                                                false),
         italic = detail::resolve_platform_face(SystemFontFamily::ui_sans, 400,
                                                true),
         mono = detail::resolve_platform_face(SystemFontFamily::ui_monospace,
                                              400, false);
    require(regular && strong && italic && mono,
            "platform face resolution missing");
    require(regular->source_path != strong->source_path ||
                regular->face_index != strong->face_index,
            "strong resolved to regular face");
    require(italic->italic && (regular->source_path != italic->source_path ||
                               regular->face_index != italic->face_index),
            "italic resolved to regular face");
    require(std::ranges::any_of(requests,
                                [](const auto &r) {
                                  return r.family ==
                                         SystemFontFamily::ui_monospace;
                                }),
            "code never requested monospace");
    std::cout << "typography_acceptance=passed gpu_driver="
              << renderer.gpu_driver()
              << " shader_format=" << renderer.shader_format()
              << " system_display_scale=" << metrics.display_scale
              << " render_scale=" << scale
              << " copy_only=" << (copy_only ? "true" : "false")
              << " theme=" << (dark ? "dark" : "light")
              << " clipboard=passed ellipsis=passed edit="
              << (input ? "passed" : "not-applicable")
              << " link=passed divider=passed submits="
              << renderer.counters().frame_submissions
              << " quad_draws=" << renderer.counters().quad_draws
              << " glyph_draws=" << renderer.counters().glyph_draws
              << " effect_draws=" << renderer.counters().effect_draws
              << " components=" << services.components().component_count()
              << " rasterizations=" << fonts->counters().rasterizations << "\n";
    for (const auto &pair :
         {std::pair{"regular", *regular}, std::pair{"strong", *strong},
          std::pair{"italic", *italic}, std::pair{"monospace", *mono}})
      std::cout << "font=" << pair.first
                << " family=" << pair.second.family_name
                << " path=" << pair.second.source_path.string()
                << " face_index=" << pair.second.face_index
                << " weight=" << pair.second.weight
                << " italic=" << pair.second.italic << '\n';
    for (const auto &note : font_chain.diagnostic_fallbacks)
      std::cout << "font_fallback=" << note << '\n';
    std::cout << "exit_code=0\n";
    services.dispose();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "typography_acceptance_error=" << error.what()
              << "\nexit_code=1\n";
    return 1;
  }
}
} // namespace rynui::example
