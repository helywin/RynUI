#include "gallery_pages_host.hpp"

#include "gallery_page_controller.hpp"
#include "gallery_page_viewport.hpp"
#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_source_panel.hpp"

#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rynui::example {
namespace {
using namespace ryn;

String text(std::string_view value) {
    return String::from_utf8(value).value();
}

bool has_algorithm(const ThemeConfig& theme, ThemeAlgorithm value) {
    return std::ranges::find(theme.algorithms, value) != theme.algorithms.end();
}

struct Section final {
    GalleryPageSection kind;
    std::string_view identity;
    std::string_view title;
};

constexpr std::array sections{
    Section{GalleryPageSection::usage, "gallery.page.usage", "用途"},
    Section{GalleryPageSection::examples, "gallery.page.examples", "示例"},
    Section{GalleryPageSection::api, "gallery.page.api", "C++ 与 API"},
    Section{GalleryPageSection::support, "gallery.page.support", "支持范围"},
    Section{GalleryPageSection::tokens, "gallery.page.tokens", "Design Token"},
    Section{GalleryPageSection::sources, "gallery.page.sources", "来源"},
};

const Section& section_info(GalleryPageSection kind) {
    const auto found = std::ranges::find(sections, kind, &Section::kind);
    if (found == sections.end()) {
        throw std::invalid_argument("Gallery page section is unknown");
    }
    return *found;
}

void paragraph(String value, Prop<LogicalLength> width, TextTone tone = TextTone::Primary, float size = 14,
               float line_height = 22, std::uint32_t weight = 400) {
    auto config = std::make_unique<ThemeConfig>();
    config->text.tokens.font_size = dp(size);
    config->text.tokens.line_height = dp(line_height);
    config->text.tokens.font_weight = weight;
    auto props = std::make_unique<ThemeProps>();
    props->config(std::move(*config));
    Theme(std::move(*props), ThemeContent{[value = std::move(value), width = std::move(width), tone] {
              Text(TextProps{}.content(value).tone(tone).layout(LayoutStyle{}.width(width)));
          }});
}

template <typename Token> std::string resolved_component_values(const Token& token) {
    std::ostringstream out;
    const auto number = [&out](std::string_view name, float value) {
        out << name << " · " << value << '\n';
    };
    const auto color = [&out](std::string_view name, Color value) {
        out << name << " · rgba(" << std::round(value.red() * 255) << ", " << std::round(value.green() * 255) << ", "
            << std::round(value.blue() * 255) << ", " << value.alpha() << ")\n";
    };
    if constexpr (requires { token.control_height; }) {
        number("control_height", token.control_height);
    }
    if constexpr (requires { token.control_height_small; }) {
        number("control_height_small", token.control_height_small);
    }
    if constexpr (requires { token.font_size; }) {
        number("font_size", token.font_size);
    }
    if constexpr (requires { token.content_font_size; }) {
        number("content_font_size", token.content_font_size);
    }
    if constexpr (requires { token.line_height; }) {
        number("line_height", token.line_height);
    }
    if constexpr (requires { token.border_radius; }) {
        number("border_radius", token.border_radius);
    }
    if constexpr (requires { token.padding_inline; }) {
        number("padding_inline", token.padding_inline);
    }
    if constexpr (requires { token.padding_block; }) {
        number("padding_block", token.padding_block);
    }
    if constexpr (requires { token.cell_padding_block; }) {
        number("cell_padding_block", token.cell_padding_block);
    }
    if constexpr (requires { token.cell_padding_inline; }) {
        number("cell_padding_inline", token.cell_padding_inline);
    }
    if constexpr (requires { token.title_font_size; }) {
        number("title_font_size", token.title_font_size);
    }
    if constexpr (requires { token.header_height; }) {
        number("header_height", token.header_height);
    }
    if constexpr (requires { token.item_height; }) {
        number("item_height", token.item_height);
    }
    if constexpr (requires { token.border_width; }) {
        number("border_width", token.border_width);
    }
    if constexpr (requires { token.color; }) {
        color("color", token.color);
    }
    if constexpr (requires { token.background; }) {
        color("background", token.background);
    }
    if constexpr (requires { token.border_color; }) {
        color("border_color", token.border_color);
    }
    if constexpr (requires { token.text_color; }) {
        color("text_color", token.text_color);
    }
    if constexpr (requires { token.primary_color; }) {
        color("primary_color", token.primary_color);
    }
    if constexpr (requires { token.primary_background; }) {
        color("primary_background", token.primary_background);
    }
    if constexpr (requires { token.default_background; }) {
        color("default_background", token.default_background);
    }
    if constexpr (requires { token.header_background; }) {
        color("header_background", token.header_background);
    }
    if constexpr (requires { token.item_selected_background; }) {
        color("item_selected_background", token.item_selected_background);
    }
    if constexpr (requires { token.track_background; }) {
        color("track_background", token.track_background);
    }
    if constexpr (requires { token.rail_background; }) {
        color("rail_background", token.rail_background);
    }
    if constexpr (requires { token.title_color; }) {
        color("title_color", token.title_color);
    }
    return out.str();
}

std::string component_values(const ThemeSnapshot& snapshot, std::string_view identity) {
    if (identity == "ant.component.switch") {
        return resolved_component_values(snapshot.switch_token());
    }
    if (identity == "ant.component.button") {
        return resolved_component_values(snapshot.button());
    }
    if (identity == "ant.component.text") {
        return resolved_component_values(snapshot.text());
    }
    if (identity == "ant.component.checkbox") {
        return resolved_component_values(snapshot.checkbox());
    }
    if (identity == "ant.component.radio") {
        return resolved_component_values(snapshot.radio());
    }
    if (identity == "ant.component.typography") {
        return resolved_component_values(snapshot.typography());
    }
    if (identity == "ant.component.divider") {
        return resolved_component_values(snapshot.divider());
    }
    if (identity == "ant.component.slider") {
        return resolved_component_values(snapshot.slider());
    }
    if (identity == "ant.component.tooltip") {
        return resolved_component_values(snapshot.tooltip());
    }
    if (identity == "ant.component.select") {
        return resolved_component_values(snapshot.select());
    }
    if (identity == "ant.component.time-picker") {
        return resolved_component_values(snapshot.time_picker());
    }
    if (identity == "ant.component.card") {
        return resolved_component_values(snapshot.card());
    }
    if (identity == "ant.component.tabs") {
        return resolved_component_values(snapshot.tabs());
    }
    if (identity == "ant.component.collapse") {
        return resolved_component_values(snapshot.collapse());
    }
    if (identity == "ant.component.segmented") {
        return resolved_component_values(snapshot.segmented());
    }
    if (identity == "ant.component.breadcrumb") {
        return resolved_component_values(snapshot.breadcrumb());
    }
    if (identity == "ant.component.steps") {
        return resolved_component_values(snapshot.steps());
    }
    if (identity == "ant.component.date-picker") {
        return resolved_component_values(snapshot.date_picker());
    }
    if (identity == "ant.component.form") {
        return resolved_component_values(snapshot.form());
    }
    if (identity == "ant.component.descriptions") {
        return resolved_component_values(snapshot.descriptions());
    }
    if (identity == "ant.component.message") {
        return resolved_component_values(snapshot.message());
    }
    if (identity == "ant.component.notification") {
        return resolved_component_values(snapshot.notification());
    }
    if (identity == "ant.component.modal") {
        return resolved_component_values(snapshot.modal());
    }
    if (identity == "ant.component.tag") {
        return resolved_component_values(snapshot.tag());
    }
    if (identity == "ant.component.input-number") {
        return resolved_component_values(snapshot.input_number());
    }
    if (identity == "ant.component.badge") {
        return resolved_component_values(snapshot.badge());
    }
    if (identity == "ant.component.drawer") {
        return resolved_component_values(snapshot.drawer());
    }
    if (identity == "ant.component.popconfirm") {
        return resolved_component_values(snapshot.popconfirm());
    }
    if (identity == "ant.component.popover") {
        return resolved_component_values(snapshot.popover());
    }
    if (identity == "ant.component.alert") {
        return resolved_component_values(snapshot.alert());
    }
    if (identity == "ant.component.spin") {
        return resolved_component_values(snapshot.spin());
    }
    if (identity == "ant.component.progress") {
        return resolved_component_values(snapshot.progress());
    }
    if (identity == "ant.component.menu") {
        return resolved_component_values(snapshot.menu());
    }
    if (identity == "ant.component.dropdown") {
        return resolved_component_values(snapshot.dropdown());
    }
    if (identity == "ant.component.pagination") {
        return resolved_component_values(snapshot.pagination());
    }
    if (identity == "ant.component.table") {
        return resolved_component_values(snapshot.table());
    }
    if (identity == "ant.component.empty") {
        return resolved_component_values(snapshot.empty());
    }
    if (identity == "ant.component.result") {
        return resolved_component_values(snapshot.result());
    }
    if (identity == "ant.component.skeleton") {
        return resolved_component_values(snapshot.skeleton());
    }
    if (identity == "ant.component.timeline") {
        return resolved_component_values(snapshot.timeline());
    }
    if (identity == "ant.component.avatar") {
        return resolved_component_values(snapshot.avatar());
    }
    if (identity == "ant.component.rate") {
        return resolved_component_values(snapshot.rate());
    }
    if (identity == "ant.component.statistic") {
        return resolved_component_values(snapshot.statistic());
    }
    if (identity == "ant.component.tree") {
        return resolved_component_values(snapshot.tree());
    }
    if (identity == "ant.component.auto-complete") {
        return resolved_component_values(snapshot.auto_complete());
    }
    if (identity == "ant.component.tree-select") {
        return resolved_component_values(snapshot.tree_select());
    }
    return {};
}

} // namespace

struct GalleryPagesHost::Impl final {
    struct Document final {
        GalleryPageRoute route;
        GalleryPageTicket ticket;
        GallerySamplePage samples;
        Signal<LogicalLength> width{dp(880)};
        std::vector<Section> sections;
    };

    ReferenceSurfaceHost& surfaces;
    ryn::detail::ButtonComponentHost& app;
    ryn::detail::InputComponentHost& inputs;
    ryn::detail::WindowComponentServices& services;
    runtime::FrameRequestState& frames;
    Copy copy;
    GalleryPageModel pages;
    std::unique_ptr<GalleryPageController> controller;
    std::shared_ptr<Document> document;
    GalleryPageViewport document_scroll;
    GalleryScrollRange navigation_scroll;
    GalleryScrollTranslation navigation_translation;
    GalleryScrollbarController navigation_bar;
    GalleryScrollbarController document_bar;
    GalleryScrollbarGeometry navigation_geometry;
    GalleryScrollbarGeometry document_geometry;
    GalleryPageLayout metrics;
    runtime::Size viewport{1440, 900};
    GalleryPagesWork work;
    std::map<std::string, runtime::ComponentId> shell_targets;
    runtime::ComponentId shell;
    runtime::ComponentId page_holder;
    runtime::ComponentId navigation_holder;
    runtime::ComponentId navigation_content;
    runtime::ComponentId navigation_mount;
    runtime::ComponentId toc_holder;
    runtime::ComponentId toc_mount;
    runtime::NodeId document_node;
    runtime::NodeId navigation_node;
    InputRef search_ref;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<String> query{String{}};
    Signal<std::uint64_t> navigation_revision{0};
    Signal<bool> narrow{false};
    Signal<bool> wide{true};
    Signal<bool> directory_open{false};
    Signal<bool> settings_open{false};
    Signal<bool> back_enabled{false};
    Signal<bool> forward_enabled{false};
    Signal<LogicalLength> window_width{dp(1440)};
    Signal<LogicalLength> header_height{dp(60)};
    Signal<LogicalLength> body_height{dp(808)};
    Signal<LogicalLength> outer{dp(24)};
    Signal<LogicalLength> gap{dp(24)};
    Signal<LogicalLength> navigation_width{dp(240)};
    Signal<LogicalLength> document_width{dp(880)};
    Signal<LogicalLength> document_margin{dp(56)};
    Signal<LogicalLength> document_right_margin{dp(56)};
    Signal<LogicalLength> toc_width{dp(160)};
    Signal<LogicalLength> navigation_track_height{dp(808)};
    Signal<LogicalLength> navigation_thumb_height{dp(808)};
    Signal<LogicalLength> document_track_height{dp(808)};
    Signal<LogicalLength> document_thumb_height{dp(808)};
    Signal<float> drawer_width{280};
    std::array<bool, 7> category_open{true, true, true, true, true, true, true};
    bool mounted{};
    bool stopped{};
    bool navigation_mode_narrow{};
    bool search_after_layout{};
    bool enter_after_layout{};
    bool jump_after_layout{};
    bool navigation_requested{};
    std::optional<GalleryPageAnchor> resize_anchor;
    std::uint64_t presented_generation{};

    Impl(ReferenceSurfaceHost& surface_host, ryn::detail::InputComponentHost& input_host,
         runtime::FrameRequestState& frame_state, Copy clipboard)
        : surfaces(surface_host), app(surfaces.application()), inputs(input_host), services(app.services()),
          frames(frame_state), copy(std::move(clipboard)) {}

    [[nodiscard]] bool within(runtime::ComponentId child, runtime::ComponentId owner) const;
    [[nodiscard]] runtime::ComponentId surface(std::string_view identity,
                                               std::optional<runtime::ComponentId> owner = {}) const;
    [[nodiscard]] runtime::ComponentId target(std::string_view identity) const;
    void invalidate_navigation();
    void update_metrics(runtime::Size size);
    void build_shell();
    void install_viewport(runtime::NodeId node);
    void build_header();
    void build_navigation();
    void rebuild_navigation();
    void build_toc();
    void toc_buttons(const std::shared_ptr<Document>& doc, bool inline_mode);
    GalleryPageContent build_page(const GalleryPageRoute& route, const GalleryPageDraft& draft);
    void build_document(const std::shared_ptr<Document>& doc);
    void build_overview(const std::shared_ptr<Document>& doc);
    void build_component(const std::shared_ptr<Document>& doc);
    void build_tokens(const std::shared_ptr<Document>& doc);
    void heading(const std::shared_ptr<Document>& doc, GalleryPageSection kind);
    void api(const std::shared_ptr<Document>& doc);
    void support(const std::shared_ptr<Document>& doc);
    void token_values(const std::shared_ptr<Document>& doc);
    void sources(const std::shared_ptr<Document>& doc);
    void related(const std::shared_ptr<Document>& doc);
    bool request(GalleryPageRoute route);
    bool process();
    bool synchronize(runtime::Size size);
    bool dispatch(const input::PlatformInputEvent& event);
    bool pointer(const input::PointerInputEvent& event);
    bool scroll(const input::ScrollInputEvent& event);
    bool keyboard(const input::KeyboardInputEvent& event);
    bool reveal_focus();
    void focus_search();
    void set_theme(const ThemeConfig& config);
    void jump_to_route();
    void place_bars();
    void shutdown() noexcept;
};

bool GalleryPagesHost::Impl::within(runtime::ComponentId child, runtime::ComponentId owner) const {
    for (auto id = std::optional{child}; id; id = services.components().parent(*id)) {
        if (*id == owner) {
            return true;
        }
    }
    return false;
}

runtime::ComponentId GalleryPagesHost::Impl::surface(std::string_view identity,
                                                     std::optional<runtime::ComponentId> owner) const {
    for (const auto& item : surfaces.mounted_surfaces()) {
        if (surfaces.snapshot(item.component).identity == identity && (!owner || within(item.component, *owner))) {
            return item.component;
        }
    }
    return {};
}

runtime::ComponentId GalleryPagesHost::Impl::target(std::string_view identity) const {
    if (document) {
        const auto found = document->samples.target ? document->samples.target(identity) : runtime::ComponentId{};
        if (found.valid()) {
            return found;
        }
    }
    const auto found = shell_targets.find(std::string{identity});
    if (found != shell_targets.end() && services.components().contains(found->second)) {
        return found->second;
    }
    return surface(identity, controller ? std::optional{controller->root()} : std::nullopt);
}

void GalleryPagesHost::Impl::invalidate_navigation() {
    navigation_revision.set(navigation_revision.get() + 1);
    frames.request_frame();
}

void GalleryPagesHost::Impl::update_metrics(runtime::Size size) {
    metrics = gallery_page_layout(size);
    viewport = size;
    narrow.set(metrics.narrow);
    wide.set(metrics.wide);
    window_width.set(dp(size.width));
    header_height.set(dp(metrics.header_height));
    body_height.set(dp(metrics.document.height));
    outer.set(dp(size.width >= 960 ? 24 : 16));
    gap.set(dp(metrics.gap));
    navigation_width.set(dp(metrics.navigation_width));
    document_width.set(dp(metrics.document_width));
    document_margin.set(dp(metrics.content.x - outer.get().value() - metrics.navigation_width));
    document_right_margin.set(dp(metrics.wide ? metrics.toc.x - metrics.content.x - metrics.content.width : 0));
    toc_width.set(dp(metrics.wide ? 160 : 0));
    drawer_width.set(std::min(280.0F, std::max(1.0F, size.width - 32)));
    if (document) {
        document->width.set(dp(metrics.document_width));
        document->samples.set_width(metrics.document_width);
    }
}

void GalleryPagesHost::Impl::build_shell() {
    auto props = std::make_unique<ThemeProps>();
    props->config(theme);
    Theme(std::move(*props), [this] {
        Flex(
            FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(0)).layout(LayoutStyle{}.width(window_width)),
            [this] {
                ReferenceSurface(ReferenceSurfaceProps{}
                                     .identity("gallery.shell.header")
                                     .role(ReferenceSurfaceRole::site_header)
                                     .layout(LayoutStyle{}.width(window_width).height(header_height)),
                                 [this] { build_header(); });
                Flex(FlexProps{}
                         .align(FlexAlign::Start)
                         .gap(dp(0))
                         .layout(LayoutStyle{}
                                     .width(bind([w = window_width, x = outer] {
                                         return dp(w.get().value() - 2 * x.get().value());
                                     }))
                                     .margin_left(outer)
                                     .margin_top(dp(16))),
                     [this] {
                         ReferenceSurface(ReferenceSurfaceProps{}
                                              .identity("gallery.shell.navigation.viewport")
                                              .role(ReferenceSurfaceRole::document_note)
                                              .visible(bind([value = narrow] { return !value.get(); }))
                                              .layout(LayoutStyle{}.width(navigation_width).height(body_height)),
                                          [this] {
                                              Flex(FlexProps{}
                                                       .vertical(true)
                                                       .align(FlexAlign::Start)
                                                       .layout(LayoutStyle{}
                                                                   .width(bind([w = navigation_width] {
                                                                       return dp(std::max(1.0F, w.get().value() - 16));
                                                                   }))
                                                                   .flex_shrink(0)),
                                                   [] {});
                                          });
                         ReferenceSurface(ReferenceSurfaceProps{}
                                              .identity("gallery.shell.document.viewport")
                                              .role(ReferenceSurfaceRole::document_note)
                                              .layout(LayoutStyle{}
                                                          .width(document_width)
                                                          .height(body_height)
                                                          .margin_left(document_margin)
                                                          .margin_right(document_right_margin)),
                                          [this] {
                                              Flex(FlexProps{}
                                                       .vertical(true)
                                                       .align(FlexAlign::Start)
                                                       .layout(LayoutStyle{}.width(document_width).flex_shrink(0)),
                                                   [] {});
                                          });
                         ReferenceSurface(ReferenceSurfaceProps{}
                                              .identity("gallery.shell.toc")
                                              .role(ReferenceSurfaceRole::document_note)
                                              .visible(wide)
                                              .layout(LayoutStyle{}.width(toc_width).height(body_height)),
                                          [] {});
                     });
                const auto bar = [](std::string_view identity, ReferenceSurfaceRole role, Signal<LogicalLength> height,
                                    Prop<bool> visible) {
                    ReferenceSurface(ReferenceSurfaceProps{}
                                         .identity(identity)
                                         .role(role)
                                         .visible(std::move(visible))
                                         .layout(LayoutStyle{}.width(dp(6)).height(height)),
                                     [] {});
                };
                bar("gallery.shell.navigation.track", ReferenceSurfaceRole::scrollbar_track, navigation_track_height,
                    bind([value = narrow] { return !value.get(); }));
                bar("gallery.shell.navigation.thumb", ReferenceSurfaceRole::scrollbar_thumb, navigation_thumb_height,
                    bind([value = narrow] { return !value.get(); }));
                bar("gallery.shell.document.track", ReferenceSurfaceRole::scrollbar_track, document_track_height, true);
                bar("gallery.shell.document.thumb", ReferenceSurfaceRole::scrollbar_thumb, document_thumb_height, true);
            });
    });
}

void GalleryPagesHost::Impl::build_header() {
    Flex(FlexProps{}
             .wrap(true)
             .align(FlexAlign::Center)
             .gap(dp(8))
             .layout(
                 LayoutStyle{}
                     .width(bind([w = window_width, x = outer] { return dp(w.get().value() - 2 * x.get().value()); }))
                     .margin_left(outer)
                     .margin_top(dp(12))),
         FlexContent{[this] {
             paragraph(String{u8"RynUI"}, dp(80), TextTone::Primary, 20, 28, 600);
             const auto command = [this](std::string_view identity, String caption, std::function<void()> click,
                                         Prop<bool> visible = true) {
                 ReferenceSurface(
                     ReferenceSurfaceProps{}
                         .identity(identity)
                         .role(ReferenceSurfaceRole::document_note)
                         .visible(visible)
                         .layout(LayoutStyle{}
                                     .width(bind(
                                         [visible] { return ryn::detail::read_prop(visible) ? auto_length : dp(0); }))
                                     .height(bind(
                                         [visible] { return ryn::detail::read_prop(visible) ? auto_length : dp(0); }))),
                     [caption, click = std::move(click)] {
                         Button(ButtonProps{}.type(ButtonType::Text).size(ControlSize::Small).onClick(click),
                                [caption] { Text(caption); });
                     });
             };
             command("gallery.shell.directory", String{u8"组件目录"}, [this] { directory_open.set(true); }, narrow);
             command(
                 "gallery.shell.components", String{u8"组件"}, [this] { request({}); },
                 bind([value = narrow] { return !value.get(); }));
             command(
                 "gallery.shell.tokens", String{u8"Token"}, [this] { request({GalleryPageKind::tokens}); },
                 bind([value = narrow] { return !value.get(); }));
             ReferenceSurface(
                 ReferenceSurfaceProps{}
                     .role(ReferenceSurfaceRole::document_note)
                     .visible(wide)
                     .layout(LayoutStyle{}
                                 .width(bind([value = wide] { return value.get() ? auto_length : dp(0); }))
                                 .height(bind([value = wide] { return value.get() ? auto_length : dp(0); }))),
                 [] { Text(TextProps{}.content(u8"Ant Design 6.6.5").tone(TextTone::Secondary)); });
             command("gallery.shell.theme", String{u8"Default / Dark"}, [this] {
                 auto config = theme.get();
                 const bool dark = has_algorithm(config, ThemeAlgorithm::Dark);
                 const bool compact = has_algorithm(config, ThemeAlgorithm::Compact);
                 config.algorithms = {dark ? ThemeAlgorithm::Default : ThemeAlgorithm::Dark};
                 if (compact) {
                     config.algorithms.push_back(ThemeAlgorithm::Compact);
                 }
                 set_theme(config);
             });
             ReferenceSurface(
                 ReferenceSurfaceProps{}.identity("gallery.shell.compact").role(ReferenceSurfaceRole::document_note),
                 [this] {
                     Checkbox(CheckboxProps{}
                                  .checked(bind(
                                      [value = theme] { return has_algorithm(value.get(), ThemeAlgorithm::Compact); }))
                                  .onChange([this](bool compact) {
                                      auto config = theme.get();
                                      config.algorithms = {has_algorithm(config, ThemeAlgorithm::Dark)
                                                               ? ThemeAlgorithm::Dark
                                                               : ThemeAlgorithm::Default};
                                      if (compact) {
                                          config.algorithms.push_back(ThemeAlgorithm::Compact);
                                      }
                                      set_theme(config);
                                  }),
                              CheckboxLabel{[] { Text(u8"Compact"); }});
                 });
             Popover(
                 PopoverProps{}.open(settings_open).trigger(PopoverTriggerMode::Click).onOpenChange([this](bool value) {
                     settings_open.set(value);
                 }),
                 PopoverTrigger{[command] { command("gallery.shell.settings", String{u8"主题设置"}, [] {}); }},
                 PopoverSlots{.content = PopoverContent{[this] {
                                  Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), [this] {
                                      Text(u8"品牌色");
                                      Space(SpaceProps{}.wrap(true), [this] {
                                          for (const auto& [label, color] :
                                               std::array{std::pair{u8"蓝色", Color::rgba8(22, 119, 255)},
                                                          std::pair{u8"紫色", Color::rgba8(114, 46, 209)},
                                                          std::pair{u8"绿色", Color::rgba8(0, 153, 102)}}) {
                                              Button(ButtonProps{}.size(ControlSize::Small).onClick([this, color] {
                                                  auto config = theme.get();
                                                  config.seed.color_primary = color;
                                                  set_theme(config);
                                              }),
                                                     [label] { Text(String::from_utf8(label).value()); });
                                          }
                                      });
                                      Checkbox(CheckboxProps{}
                                                   .checked(bind([value = theme] {
                                                       return value.get().seed.motion.value_or(true);
                                                   }))
                                                   .onChange([this](bool value) {
                                                       auto config = theme.get();
                                                       config.seed.motion = value;
                                                       set_theme(config);
                                                   }),
                                               CheckboxLabel{[] { Text(u8"启用动效"); }});
                                  });
                              }}});
         }});
}

void GalleryPagesHost::Impl::build_navigation() {
    const auto width = bind([this] {
        return dp(
            std::max(1.0F, navigation_mode_narrow ? drawer_width.get() - 48 : navigation_width.get().value() - 16));
    });
    Flex(
        FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)).layout(LayoutStyle{}.width(width).flex_shrink(0)),
        [this, width] {
            Input(InputProps{}
                      .value(query)
                      .ref(search_ref)
                      .placeholder(u8"搜索组件 · Ctrl+K")
                      .allowClear(true)
                      .onChange([this](String value) {
                          query.set(value);
                          pages.set_query(std::string{value.bytes()});
                          invalidate_navigation();
                      })
                      .onSubmit([this](String) {
                          const auto results = pages.results();
                          if (!results.empty()) {
                              request(results.front().page->route(results.front().family));
                          }
                      })
                      .layout(LayoutStyle{}.width(width)));
            std::vector<SelectOption> filters;
            constexpr std::array<std::string_view, 7> labels{"全部状态", "已实现", "部分支持", "规划中",
                                                             "仅 Web",   "已弃用", "不在范围"};
            for (std::size_t i = 0; i < labels.size(); ++i) {
                const auto count = i == 0 ? pages.pages().size() : pages.counts().status[i - 1];
                filters.push_back(
                    {static_cast<std::int64_t>(i), text(std::string{labels[i]} + " · " + std::to_string(count))});
            }
            Select(SelectProps{}
                       .options(std::move(filters))
                       .value(bind([this] {
                           static_cast<void>(navigation_revision.get());
                           return SelectSelection{static_cast<std::int64_t>(pages.filter())};
                       }))
                       .onChange([this](SelectSelection value) {
                           if (value) {
                               const auto* key = std::get_if<std::int64_t>(&value->storage());
                               if (key && *key >= 0 && *key <= 6) {
                                   pages.set_filter(static_cast<GallerySupportFilter>(*key));
                                   invalidate_navigation();
                               }
                           }
                       })
                       .layout(LayoutStyle{}.width(width)));
            const auto link = [this, width](String label, GalleryPageRoute route) {
                Button(ButtonProps{}
                           .type(ButtonType::Text)
                           .size(ControlSize::Small)
                           .onClick([this, route] { request(route); })
                           .layout(LayoutStyle{}.width(width).height(dp(32))),
                       [label] { Text(label); });
            };
            link(String{u8"组件总览"}, {});
            link(String{u8"设计介绍"}, {GalleryPageKind::introduction});
            link(String{u8"Design Token"}, {GalleryPageKind::tokens});
            ReferenceSurface(
                ReferenceSurfaceProps{}
                    .identity("gallery.navigation.unmatched")
                    .role(ReferenceSurfaceRole::document_note)
                    .visible(bind([this] {
                        static_cast<void>(navigation_revision.get());
                        return !pages.current_matches();
                    }))
                    .layout(LayoutStyle{}.width(width).height(bind([this] {
                        static_cast<void>(navigation_revision.get());
                        return pages.current_matches() ? dp(0) : auto_length;
                    }))),
                [width] { paragraph(String{u8"当前页不在筛选结果中"}, width, TextTone::Secondary, 12, 20); });
            ReferenceSurface(ReferenceSurfaceProps{}
                                 .identity("gallery.navigation.empty")
                                 .role(ReferenceSurfaceRole::document_note)
                                 .visible(bind([this] {
                                     static_cast<void>(navigation_revision.get());
                                     return pages.results().empty();
                                 }))
                                 .layout(LayoutStyle{}.width(width).height(bind([this] {
                                     static_cast<void>(navigation_revision.get());
                                     return pages.results().empty() ? auto_length : dp(0);
                                 }))),
                             [this, width] {
                                 paragraph(String{u8"没有匹配的组件"}, width, TextTone::Secondary);
                                 Button(ButtonProps{}.size(ControlSize::Small).onClick([this] {
                                     query.set(String{});
                                     pages.set_query("");
                                     pages.set_filter(GallerySupportFilter::all);
                                     invalidate_navigation();
                                 }),
                                        [] { Text(u8"清除条件"); });
                             });
            for (const auto& category : ant_design_reference_categories()) {
                const auto index = static_cast<std::size_t>(category.category);
                const auto category_visible = bind([this, kind = category.category] {
                    static_cast<void>(navigation_revision.get());
                    return std::ranges::any_of(pages.results(),
                                               [kind](const auto& item) { return item.page->entry->category == kind; });
                });
                ReferenceSurface(
                    ReferenceSurfaceProps{}
                        .identity(category.identity)
                        .role(ReferenceSurfaceRole::document_note)
                        .visible(category_visible)
                        .layout(LayoutStyle{}.width(width).height(
                            bind([category_visible] { return category_visible.get() ? auto_length : dp(0); }))),
                    [this, category, index, width] {
                        Flex(FlexProps{}
                                 .vertical(true)
                                 .align(FlexAlign::Start)
                                 .gap(dp(0))
                                 .layout(LayoutStyle{}.width(width)),
                             [this, category, index, width] {
                                 Button(ButtonProps{}
                                            .type(ButtonType::Text)
                                            .size(ControlSize::Small)
                                            .onClick([this, index] {
                                                category_open[index] = !category_open[index];
                                                invalidate_navigation();
                                            })
                                            .layout(LayoutStyle{}.width(width).height(dp(32))),
                                        [this, category, index] {
                                            Text(TextProps{}
                                                     .content(bind([this, category, index] {
                                                         static_cast<void>(navigation_revision.get());
                                                         return text(std::string{category_open[index] ? "▾ " : "▸ "} +
                                                                     std::string{category.chinese_name});
                                                     }))
                                                     .tone(TextTone::Secondary));
                                        });
                                 for (const auto& page : pages.pages()) {
                                     const auto* entry = page.entry;
                                     if (entry->category != category.category) {
                                         continue;
                                     }
                                     const auto shown = bind([this, entry, index] {
                                         static_cast<void>(navigation_revision.get());
                                         const auto results = pages.results();
                                         return (category_open[index] || !query.get().empty()) &&
                                                std::ranges::any_of(results, [entry](const auto& result) {
                                                    return result.page->entry == entry;
                                                });
                                     });
                                     ReferenceSurface(
                                         ReferenceSurfaceProps{}
                                             .identity(entry->identity)
                                             .role(ReferenceSurfaceRole::document_note)
                                             .visible(shown)
                                             .layout(LayoutStyle{}.width(width).height(
                                                 bind([shown] { return shown.get() ? dp(36) : dp(0); }))),
                                         [this, entry, width] {
                                             auto config = bind([this, entry] {
                                                 static_cast<void>(navigation_revision.get());
                                                 ThemeConfig result;
                                                 result.text.tokens.font_size = dp(12);
                                                 result.text.tokens.line_height = dp(20);
                                                 if (pages.current().component_identity == entry->identity) {
                                                     const auto color = resolve_theme(theme.get()).map().color_primary;
                                                     result.button.tokens.text_color = color;
                                                     result.button.tokens.text_background =
                                                         Color{color.red(), color.green(), color.blue(), 0.08F};
                                                 }
                                                 return result;
                                             });
                                             auto props = std::make_unique<ThemeProps>();
                                             props->config(config);
                                             Theme(std::move(*props), [this, entry, width] {
                                                 Button(ButtonProps{}
                                                            .type(ButtonType::Text)
                                                            .size(ControlSize::Small)
                                                            .layout(LayoutStyle{}.width(width).height(dp(36)))
                                                            .onClick([this, entry] {
                                                                auto family = GalleryInputFamily::input;
                                                                for (const auto& result : pages.results()) {
                                                                    if (result.page->entry == entry) {
                                                                        family = result.family;
                                                                        break;
                                                                    }
                                                                }
                                                                request(pages.find(entry->identity)->route(family));
                                                            }),
                                                        [entry] {
                                                            Text(text(std::string{entry->english_name} + " " +
                                                                      std::string{entry->chinese_name}));
                                                        });
                                             });
                                         });
                                 }
                             });
                    });
            }
        });
}

GalleryPageContent GalleryPagesHost::Impl::build_page(const GalleryPageRoute& route, const GalleryPageDraft& draft) {
    auto doc = std::make_shared<Document>();
    doc->route = route;
    doc->ticket = controller->ticket();
    doc->width.set(dp(metrics.document_width));
    doc->sections.push_back(section_info(GalleryPageSection::usage));
    const bool examples = std::ranges::any_of(gallery_samples(), [&route](const auto& sample) {
        return sample.kind == route.kind && sample.component_identity == route.component_identity &&
               sample.family == route.family;
    });
    if (examples) {
        doc->sections.push_back(section_info(GalleryPageSection::examples));
        doc->sections.push_back(section_info(GalleryPageSection::api));
    }
    if (route.kind == GalleryPageKind::component) {
        doc->sections.push_back(section_info(GalleryPageSection::support));
    }
    if (examples || route.kind == GalleryPageKind::tokens ||
        (route.kind == GalleryPageKind::component && pages.find(route.component_identity)->native_scope())) {
        doc->sections.push_back(section_info(GalleryPageSection::tokens));
    }
    doc->sections.push_back(section_info(GalleryPageSection::sources));
    GallerySamplePageOptions options;
    options.readable_labels = true;
    options.footer = [this, weak = std::weak_ptr{doc}](const GallerySampleDescriptor& descriptor,
                                                       Signal<LogicalLength> width) {
        const auto locked = weak.lock();
        if (!locked || !locked->ticket.valid()) {
            return;
        }
        if (const auto* source = gallery_sample_source(descriptor.identity)) {
            gallery_source_panel(*source, width, copy);
        }
    };
    doc->samples = make_gallery_sample_page(route, draft, std::move(options));
    doc->samples.set_width(metrics.document_width);
    doc->samples.set_theme(theme.get());
    document = doc;
    ++work.page_mounts;
    return {
        Content{[this, doc] { build_document(doc); }},
        [doc](GalleryPageDraft& saved) { doc->samples.save(saved); },
        [this, weak = std::weak_ptr{doc}] {
            if (const auto doc = weak.lock()) {
                controller->set_anchor(document_scroll.capture());
            }
            static_cast<void>(document_bar.reset());
        },
    };
}

void GalleryPagesHost::Impl::heading(const std::shared_ptr<Document>& doc, GalleryPageSection kind) {
    const auto section = section_info(kind);
    ReferenceSurface(ReferenceSurfaceProps{}
                         .identity(section.identity)
                         .role(ReferenceSurfaceRole::document_heading)
                         .layout(LayoutStyle{}.width(doc->width).margin_top(dp(24)).margin_bottom(dp(8))),
                     [doc, section] { paragraph(text(section.title), doc->width, TextTone::Primary, 22, 30, 600); });
}

void GalleryPagesHost::Impl::build_document(const std::shared_ptr<Document>& doc) {
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(doc->width).flex_shrink(0)),
         [this, doc] {
             ReferenceSurface(
                 ReferenceSurfaceProps{}
                     .identity("gallery.page.usage")
                     .role(ReferenceSurfaceRole::document_heading)
                     .layout(LayoutStyle{}.width(doc->width)),
                 [this, doc] {
                     Space(SpaceProps{}.size(dp(8)).wrap(true), [this] {
                         Button(ButtonProps{}
                                    .type(ButtonType::Text)
                                    .size(ControlSize::Small)
                                    .disabled(bind([enabled = back_enabled] { return !enabled.get(); }))
                                    .onClick([this] {
                                        if (controller->back()) {
                                            navigation_requested = true;
                                        }
                                    }),
                                [] { Text(u8"← 后退"); });
                         Button(ButtonProps{}
                                    .type(ButtonType::Text)
                                    .size(ControlSize::Small)
                                    .disabled(bind([enabled = forward_enabled] { return !enabled.get(); }))
                                    .onClick([this] {
                                        if (controller->forward()) {
                                            navigation_requested = true;
                                        }
                                    }),
                                [] { Text(u8"前进 →"); });
                     });
                     if (doc->route.kind == GalleryPageKind::component) {
                         const auto* entry = pages.find(doc->route.component_identity)->entry;
                         paragraph(text(gallery_category_title(entry->category)), doc->width, TextTone::Secondary, 12,
                                   20);
                         paragraph(text(std::string{entry->english_name} + " " + std::string{entry->chinese_name}),
                                   doc->width, TextTone::Primary, metrics.narrow ? 28 : 32, 40, 600);
                         Tag(TagProps{}.color(
                                 entry->support_status == GallerySupportStatus::implemented ? TagColor{Status::Success}
                                 : entry->support_status == GallerySupportStatus::partial   ? TagColor{Status::Warning}
                                                                                            : TagColor{}),
                             TagContent{[entry] { Text(text(gallery_page_status_label(entry->support_status))); }});
                         paragraph(String{u8"按声明的原生范围"}, doc->width, TextTone::Secondary, 12, 20);
                         paragraph(text(entry->summary), doc->width);
                     } else {
                         const auto title = doc->route.kind == GalleryPageKind::overview ? "组件总览"
                                            : doc->route.kind == GalleryPageKind::tokens ? "Design Token"
                                                                                         : "设计介绍";
                         paragraph(text(title), doc->width, TextTone::Primary, metrics.narrow ? 28 : 32, 40, 600);
                         paragraph(String{u8"RynUI · 基于 Ant Design 6.6.5 的原生 C++ 组件与设计约定"}, doc->width,
                                   TextTone::Secondary);
                     }
                 });
             ReferenceSurface(ReferenceSurfaceProps{}
                                  .identity("gallery.page.inline-toc")
                                  .role(ReferenceSurfaceRole::document_note)
                                  .visible(bind([value = wide] { return !value.get(); }))
                                  .layout(LayoutStyle{}.width(doc->width).height(bind([value = wide] {
                                      return value.get() ? dp(0) : auto_length;
                                  }))),
                              [this, doc] { toc_buttons(doc, true); });
             ReferenceSurface(
                 ReferenceSurfaceProps{}.identity("gallery.page.enter").role(ReferenceSurfaceRole::document_note),
                 [this, doc] {
                     Button(ButtonProps{}.type(ButtonType::Text).size(ControlSize::Small).onClick([this, doc] {
                         if (doc->ticket.valid()) {
                             auto route = doc->route;
                             route.section = std::ranges::find(doc->sections, GalleryPageSection::examples,
                                                               &Section::kind) != doc->sections.end()
                                                 ? GalleryPageSection::examples
                                                 : doc->sections.back().kind;
                             request(route);
                         }
                     }),
                            [] { Text(u8"进入页面内容 ↓"); });
                 });
             switch (doc->route.kind) {
             case GalleryPageKind::overview:
                 build_overview(doc);
                 break;
             case GalleryPageKind::introduction:
                 for (const auto& value : gallery_design_values()) {
                     paragraph(text(std::string{value.english_name} + " / " + std::string{value.chinese_name}),
                               doc->width, TextTone::Primary, 18, 28, 500);
                     paragraph(text(value.summary), doc->width, TextTone::Secondary);
                 }
                 break;
             case GalleryPageKind::tokens:
                 build_tokens(doc);
                 break;
             case GalleryPageKind::component:
                 build_component(doc);
                 break;
             }
             sources(doc);
             Flex(FlexProps{}.layout(LayoutStyle{}.height(dp(32))), [] {});
         });
}

void GalleryPagesHost::Impl::build_overview(const std::shared_ptr<Document>& doc) {
    std::string counts = "目录状态";
    for (std::size_t i = 0; i < pages.counts().status.size(); ++i) {
        counts += " · " + std::string{gallery_page_status_label(static_cast<GallerySupportStatus>(i))} + " " +
                  std::to_string(pages.counts().status[i]);
    }
    paragraph(text(counts), doc->width, TextTone::Secondary);
    paragraph(String{u8"状态来自声明范围，选择组件查看真实示例、公开 API 和缺失能力。"}, doc->width);
    for (const auto& category : ant_design_reference_categories()) {
        paragraph(text(gallery_category_title(category.category)), doc->width, TextTone::Primary, 18, 28, 500);
        for (const auto& page : pages.pages()) {
            const auto* entry = page.entry;
            if (entry->category != category.category) {
                continue;
            }
            const auto shown = bind([this, entry] {
                static_cast<void>(navigation_revision.get());
                return std::ranges::any_of(pages.results(),
                                           [entry](const auto& result) { return result.page->entry == entry; });
            });
            ReferenceSurface(
                ReferenceSurfaceProps{}
                    .identity(entry->identity)
                    .role(ReferenceSurfaceRole::document_note)
                    .visible(shown)
                    .layout(LayoutStyle{}.width(doc->width).height(bind([shown] {
                        return shown.get() ? auto_length : dp(0);
                    }))),
                [this, doc, entry] {
                    Button(ButtonProps{}.type(ButtonType::Text).onClick([this, entry] {
                        request(pages.find(entry->identity)->route());
                    }),
                           [entry] {
                               Text(text(std::string{entry->english_name} + " " + std::string{entry->chinese_name}));
                           });
                    paragraph(text(std::string{gallery_page_status_label(entry->support_status)} + " · " +
                                   std::string{entry->summary}),
                              doc->width, TextTone::Secondary, 12, 20);
                });
        }
    }
}

void GalleryPagesHost::Impl::build_component(const std::shared_ptr<Document>& doc) {
    if (doc->route.component_identity == "ant.component.input") {
        Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(doc->width)), [this, doc] {
            for (const auto family :
                 {GalleryInputFamily::input, GalleryInputFamily::search, GalleryInputFamily::password,
                  GalleryInputFamily::text_area, GalleryInputFamily::otp}) {
                Button(ButtonProps{}
                           .type(family == doc->route.family ? ButtonType::Primary : ButtonType::Default)
                           .size(ControlSize::Small)
                           .onClick([this, doc, family] {
                               if (doc->ticket.valid()) {
                                   auto route = doc->route;
                                   route.family = family;
                                   route.sample_identity.clear();
                                   route.section.reset();
                                   request(route);
                               }
                           }),
                       [family] { Text(text(gallery_input_family_name(family))); });
            }
        });
    }
    const bool examples =
        std::ranges::find(doc->sections, GalleryPageSection::examples, &Section::kind) != doc->sections.end();
    if (examples) {
        heading(doc, GalleryPageSection::examples);
    }
    ryn::detail::SlotContentAccess::function(doc->samples.content)();
    related(doc);
    if (examples) {
        api(doc);
    }
    support(doc);
    if (std::ranges::find(doc->sections, GalleryPageSection::tokens, &Section::kind) != doc->sections.end()) {
        token_values(doc);
    }
}

void GalleryPagesHost::Impl::build_tokens(const std::shared_ptr<Document>& doc) {
    paragraph(String{u8"Default / Dark 决定颜色，Compact 可叠加为紧凑密度。品牌色与动效使用顶栏的主题设置。"},
              doc->width);
    heading(doc, GalleryPageSection::examples);
    ryn::detail::SlotContentAccess::function(doc->samples.content)();
    api(doc);
    token_values(doc);
}

void GalleryPagesHost::Impl::api(const std::shared_ptr<Document>& doc) {
    heading(doc, GalleryPageSection::api);
    std::set<std::string_view> shown;
    for (const auto& descriptor : gallery_samples()) {
        if (descriptor.kind != doc->route.kind || descriptor.component_identity != doc->route.component_identity ||
            descriptor.family != doc->route.family) {
            continue;
        }
        const auto* source = gallery_sample_source(descriptor.identity);
        if (!source || !shown.insert(source->public_header).second) {
            continue;
        }
        paragraph(
            text("#include <" + std::string{source->public_header.substr(std::string_view{"include/"}.size())} + ">"),
            doc->width, TextTone::Primary, 14, 22, 500);
        if (!source->public_props.empty()) {
            paragraph(text("typed Props · " + std::string{source->public_props}), doc->width);
        }
        if (!source->public_methods.empty()) {
            paragraph(text("常用成员 · " + std::string{source->public_methods}), doc->width, TextTone::Secondary, 12,
                      20);
        }
        if (!source->public_slots.empty()) {
            paragraph(text("typed slots · " + std::string{source->public_slots}), doc->width, TextTone::Secondary, 12,
                      20);
        }
    }
    paragraph(String{u8"代码卡展示实际样例源码；完整约束以公开头文件与组件支持范围为准。"}, doc->width,
              TextTone::Secondary);
}

void GalleryPagesHost::Impl::support(const std::shared_ptr<Document>& doc) {
    heading(doc, GalleryPageSection::support);
    const auto* entry = pages.find(doc->route.component_identity)->entry;
    paragraph(String{u8"已支持"}, doc->width, TextTone::Primary, 16, 24, 500);
    paragraph(text(entry->supported_scope.empty() ? "尚无已声明的原生能力。" : entry->supported_scope), doc->width);
    paragraph(String{u8"缺失与范围边界"}, doc->width, TextTone::Primary, 16, 24, 500);
    paragraph(text(entry->missing_scope.empty() ? "没有额外登记项；状态按声明范围解释。" : entry->missing_scope),
              doc->width);
}

void GalleryPagesHost::Impl::related(const std::shared_ptr<Document>& doc) {
    for (const auto& entry : gallery_related_samples()) {
        if (entry.component_identity == doc->route.component_identity) {
            Button(ButtonProps{}.type(ButtonType::Link).onClick([this, doc, route = entry.route()] {
                if (doc->ticket.valid()) {
                    request(route);
                }
            }),
                   [label = entry.title] { Text(text(std::string{label} + " →")); });
        }
    }
}

void GalleryPagesHost::Impl::token_values(const std::shared_ptr<Document>& doc) {
    heading(doc, GalleryPageSection::tokens);
    std::set<std::string_view> shown;
    for (const auto& descriptor : gallery_samples()) {
        if (descriptor.kind == doc->route.kind && descriptor.component_identity == doc->route.component_identity &&
            descriptor.family == doc->route.family) {
            const auto* source = gallery_sample_source(descriptor.identity);
            if (source && !source->theme_path.empty() && shown.insert(source->theme_path).second) {
                paragraph(text(source->theme_path), doc->width, TextTone::Primary, 14, 22, 500);
                if (!source->token_names.empty()) {
                    paragraph(text(source->token_names), doc->width, TextTone::Secondary, 12, 20);
                }
            }
        }
    }
    if (doc->route.kind == GalleryPageKind::component &&
        !component_values(resolve_theme(theme.get()), doc->route.component_identity).empty()) {
        paragraph(String{u8"当前有效组件 Token（常用字段）"}, doc->width, TextTone::Primary, 16, 24, 500);
        Text(TextProps{}
                 .content(bind([this, identity = doc->route.component_identity] {
                     return text(component_values(resolve_theme(theme.get()), identity));
                 }))
                 .layout(LayoutStyle{}.width(doc->width)));
    }
    paragraph(String{u8"当前有效全局 Token"}, doc->width, TextTone::Primary, 16, 24, 500);
    Text(TextProps{}
             .content(bind([this] {
                 const auto snapshot = resolve_theme(theme.get());
                 const auto rgba = [](Color color) {
                     std::ostringstream out;
                     out << "rgba(" << static_cast<int>(std::round(color.red() * 255)) << ", "
                         << static_cast<int>(std::round(color.green() * 255)) << ", "
                         << static_cast<int>(std::round(color.blue() * 255)) << ", " << std::setprecision(3)
                         << color.alpha() << ')';
                     return out.str();
                 };
                 return text("colorPrimary · " + rgba(snapshot.map().color_primary) + "\ncolorText · " +
                             rgba(snapshot.alias().color_text) + "\ncolorBgContainer · " +
                             rgba(snapshot.alias().color_background_container) + "\nfontSize · " +
                             std::to_string(snapshot.map().font_size) + "\ncontrolHeight · " +
                             std::to_string(snapshot.map().control_height) + "\nborderRadius · " +
                             std::to_string(snapshot.map().border_radius));
             }))
             .layout(LayoutStyle{}.width(doc->width)));
    Button(ButtonProps{}.type(ButtonType::Link).onClick([this, doc] {
        if (doc->ticket.valid()) {
            request({GalleryPageKind::tokens});
        }
    }),
           [] { Text(u8"查看全局 Design Token →"); });
}

void GalleryPagesHost::Impl::sources(const std::shared_ptr<Document>& doc) {
    heading(doc, GalleryPageSection::sources);
    const auto inner = bind([width = doc->width] { return dp(std::max(1.0F, width.get().value() - 32)); });
    Collapse(CollapseProps{}.layout(LayoutStyle{}.width(doc->width)),
             {CollapsePanel{String{u8"source"}, CollapseTitle{[] { Text(u8"锁定版本与参考来源"); }},
                            CollapseBody{[this, doc, inner] {
                                paragraph(text("Ant Design " + std::string{ant_design_reference_version()}), inner);
                                paragraph(text(ant_design_reference_commit()), inner, TextTone::Secondary, 12, 20);
                                if (doc->route.kind == GalleryPageKind::component) {
                                    const auto* entry = pages.find(doc->route.component_identity)->entry;
                                    paragraph(text(entry->identity), inner, TextTone::Secondary, 12, 20);
                                    paragraph(text(entry->source_path), inner, TextTone::Secondary, 12, 20);
                                    paragraph(text(entry->chinese_source_path), inner, TextTone::Secondary, 12, 20);
                                    paragraph(text(entry->evidence_identifiers), inner, TextTone::Secondary, 12, 20);
                                } else {
                                    for (const auto& entry : ant_design_reference_sources()) {
                                        paragraph(text(entry.title), inner, TextTone::Primary, 14, 22, 500);
                                        paragraph(text(entry.official_url), inner, TextTone::Secondary, 12, 20);
                                    }
                                }
                                paragraph(String{u8"目录声明与来源标识不代表当前平台的自动验收结果。"}, inner,
                                          TextTone::Secondary, 12, 20);
                            }}}});
}

void GalleryPagesHost::Impl::install_viewport(runtime::NodeId node) {
    app.nodes().require(node).clip_content = true;
    app.layout().set_layout(
        node,
        ryn::layout::ComponentLayout{
            [this](ryn::layout::LayoutEngine& engine, runtime::NodeId owner, ryn::layout::Constraints constraints) {
                auto unbounded = constraints;
                unbounded.min_height = 0;
                unbounded.max_height = std::numeric_limits<float>::infinity();
                for (const auto child : app.nodes().require(owner).children) {
                    static_cast<void>(engine.measure_child(child, unbounded));
                }
                return constraints.constrain({constraints.max_width, constraints.max_height});
            },
            [this](ryn::layout::LayoutEngine& engine, runtime::NodeId owner, runtime::Rect bounds) {
                for (const auto child : app.nodes().require(owner).children) {
                    const auto size = app.nodes().require(child).layout_size;
                    engine.place_child(child, {bounds.x, bounds.y, size.width, size.height});
                }
            }});
    app.dirty().invalidate(node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
}

void GalleryPagesHost::Impl::rebuild_navigation() {
    if (app.components().contains(navigation_mount)) {
        static_cast<void>(surfaces.destroy(navigation_mount));
    }
    search_ref = InputRef{};
    navigation_mode_narrow = metrics.narrow;
    const auto holder = metrics.narrow ? shell : navigation_holder;
    const auto previous = app.components().children(holder).size();
    surfaces.append_slot(holder, Content{[this] {
                             if (navigation_mode_narrow) {
                                 Drawer(DrawerProps{}
                                            .open(directory_open)
                                            .title(u8"组件目录")
                                            .placement(DrawerPlacement::Left)
                                            .width(drawer_width)
                                            .onCancel([this](DrawerCancelReason) { directory_open.set(false); }),
                                        DrawerContent{[this] { build_navigation(); }});
                             } else {
                                 build_navigation();
                             }
                         }});
    const auto& children = app.components().children(holder);
    if (children.size() != previous + 1) {
        throw std::logic_error("Gallery navigation requires one owned root");
    }
    navigation_mount = children.back();
    navigation_content = navigation_mount;
    if (!metrics.narrow) {
        navigation_translation.invalidate();
    }
    static_cast<void>(navigation_bar.reset());
}

void GalleryPagesHost::Impl::toc_buttons(const std::shared_ptr<Document>& doc, bool inline_mode) {
    const auto width = inline_mode ? Prop<LogicalLength>{doc->width} : Prop<LogicalLength>{toc_width};
    Flex(FlexProps{}
             .vertical(!inline_mode)
             .wrap(inline_mode)
             .align(FlexAlign::Start)
             .gap(dp(4))
             .layout(LayoutStyle{}.width(width)),
         [this, doc] {
             for (const auto section : doc->sections) {
                 Button(ButtonProps{}.type(ButtonType::Text).size(ControlSize::Small).onClick([this, doc, section] {
                     if (doc->ticket.valid()) {
                         auto route = pages.current();
                         route.section = section.kind;
                         route.sample_identity.clear();
                         request(std::move(route));
                     }
                 }),
                        [section] { Text(text(section.title)); });
             }
         });
}

void GalleryPagesHost::Impl::build_toc() {
    if (app.components().contains(toc_mount)) {
        static_cast<void>(surfaces.destroy(toc_mount));
    }
    if (!document || controller->failed()) {
        return;
    }
    const auto previous = app.components().children(toc_holder).size();
    surfaces.append_slot(toc_holder, Content{[this, doc = document] { toc_buttons(doc, false); }});
    const auto& children = app.components().children(toc_holder);
    if (children.size() != previous + 1) {
        throw std::logic_error("Gallery TOC requires one owned root");
    }
    toc_mount = children.back();
}

bool GalleryPagesHost::Impl::request(GalleryPageRoute route) {
    if (!controller || stopped || !controller->request(std::move(route))) {
        return false;
    }
    navigation_requested = true;
    return true;
}

bool GalleryPagesHost::Impl::process() {
    if (!controller || stopped || !controller->pending()) {
        return false;
    }
    const auto generation = controller->generation();
    const bool changed = controller->process();
    if (generation != controller->generation()) {
        if (controller->failed()) {
            document.reset();
        }
        build_toc();
        enter_after_layout = true;
    }
    jump_after_layout = navigation_requested;
    navigation_requested = false;
    directory_open.set(false);
    back_enabled.set(controller->can_back());
    forward_enabled.set(controller->can_forward());
    invalidate_navigation();
    return changed;
}

void GalleryPagesHost::Impl::jump_to_route() {
    if (!document || controller->failed()) {
        return;
    }
    const auto& route = pages.current();
    std::string identity = route.sample_identity;
    if (identity.empty() && route.section) {
        identity = section_info(*route.section).identity;
    }
    if (!identity.empty()) {
        if (const auto anchor = document_scroll.anchor(identity)) {
            static_cast<void>(document_scroll.jump(*anchor));
        }
    }
}

bool GalleryPagesHost::Impl::synchronize(runtime::Size size) {
    if (stopped || !mounted) {
        return false;
    }
    const auto started = std::chrono::steady_clock::now();
    const bool resized = viewport != size;
    if (resized && document && presented_generation == controller->generation()) {
        resize_anchor = document_scroll.capture();
    }
    const bool was_narrow = metrics.narrow;
    update_metrics(size);
    if (was_narrow != metrics.narrow) {
        directory_open.set(false);
        rebuild_navigation();
    }
    if (document) {
        if (auto config = document->samples.take_theme_request()) {
            set_theme(*config);
        }
    }
    const runtime::Rect clip{0, 0, size.width, size.height};
    const auto layout = [this, size, clip] {
        ++work.layout_calls;
        return app.layout_and_synchronize(size, clip, {}, 0, true);
    };
    if (!layout()) {
        return false;
    }
    if (!metrics.narrow && app.components().contains(navigation_content)) {
        const auto root = app.components().root(navigation_content);
        static_cast<void>(
            navigation_scroll.set_extents(metrics.navigation.height, app.nodes().require(root).bounds.height));
        static_cast<void>(
            navigation_translation.apply(root, navigation_scroll.snapshot().offset, app.nodes(), app.dirty()));
    }
    if (document && !controller->failed()) {
        const bool new_page = presented_generation != controller->generation();
        if (new_page) {
            std::vector<GalleryPageAnchorNode> anchors;
            for (const auto section : document->sections) {
                const auto id = surface(section.identity, controller->root());
                if (!id.valid()) {
                    throw std::logic_error("Gallery section anchor is not mounted");
                }
                anchors.push_back({std::string{section.identity}, app.components().root(id)});
            }
            for (const auto& sample : gallery_samples()) {
                if (sample.kind == document->route.kind &&
                    sample.component_identity == document->route.component_identity &&
                    sample.family == document->route.family) {
                    const auto id = document->samples.target(sample.identity);
                    if (id.valid()) {
                        anchors.push_back({std::string{sample.identity}, app.components().root(id)});
                    }
                }
            }
            static_cast<void>(
                document_scroll.set_page(controller->generation(), app.components().root(controller->root()), anchors));
            presented_generation = controller->generation();
        }
        static_cast<void>(document_scroll.refresh(metrics.document.height, app.nodes()));
        if (new_page) {
            static_cast<void>(document_scroll.restore(controller->draft().anchor));
        }
        if (resize_anchor) {
            static_cast<void>(document_scroll.restore(*resize_anchor));
            resize_anchor.reset();
        }
        if (jump_after_layout) {
            jump_to_route();
            jump_after_layout = false;
        }
        static_cast<void>(document_scroll.apply(app.nodes(), app.dirty()));
    }
    navigation_geometry = gallery_scrollbar_geometry(metrics.navigation_track, navigation_scroll.snapshot());
    document_geometry = gallery_scrollbar_geometry(metrics.document_track, document_scroll.snapshot());
    navigation_track_height.set(dp(navigation_geometry.track.height));
    navigation_thumb_height.set(dp(navigation_geometry.thumb.height));
    document_track_height.set(dp(document_geometry.track.height));
    document_thumb_height.set(dp(document_geometry.thumb.height));
    if (!layout()) {
        return false;
    }
    place_bars();
    if (search_after_layout) {
        search_after_layout = false;
        static_cast<void>(search_ref.focus());
    }
    if (enter_after_layout) {
        enter_after_layout = false;
        const auto entry = target("gallery.page.enter");
        for (const auto id : services.interactions().declaration_order()) {
            const auto* record = services.interactions().find(id);
            if (record && record->focusable && within(record->component, entry)) {
                static_cast<void>(app.focus().request_focus(id, input::FocusModality::keyboard));
                break;
            }
        }
    }
    work.layout_sync_nanoseconds +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
    return true;
}

void GalleryPagesHost::Impl::place_bars() {
    const auto place = [this](std::string_view identity, runtime::Rect rect, GalleryScrollbarVisualState visual) {
        const auto id = shell_targets.at(std::string{identity});
        static_cast<void>(gallery_place_scrollbar(app.components().root(id), rect, app.nodes(), app.dirty()));
        static_cast<void>(surfaces.set_scrollbar_visual_state(id, visual));
    };
    if (!metrics.narrow) {
        place("gallery.shell.navigation.track", navigation_geometry.track, navigation_bar.visual_state());
        place("gallery.shell.navigation.thumb", navigation_geometry.thumb, navigation_bar.visual_state());
    }
    place("gallery.shell.document.track", document_geometry.track, document_bar.visual_state());
    place("gallery.shell.document.thumb", document_geometry.thumb, document_bar.visual_state());
    // Publish translated chrome in the same retained scene generation.
    ++work.layout_calls;
    if (!app.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {}, 0, true)) {
        throw std::runtime_error("Gallery scrollbar scene synchronization failed");
    }
}

bool GalleryPagesHost::Impl::pointer(const input::PointerInputEvent& event) {
    const bool boundary = services.interactions().input_boundary().has_value();
    if (!boundary) {
        const auto handle = [this, &event](GalleryScrollbarController& bar, const GalleryScrollbarGeometry& geometry,
                                           GalleryScrollRangeSnapshot range, auto&& scroll_to) {
            const auto action = bar.dispatch(event, geometry, range.viewport_extent, range.offset);
            if (action.requested_offset && scroll_to(*action.requested_offset)) {
                frames.request_frame();
            }
            return action.consumed;
        };
        bool consumed = false;
        if (!metrics.narrow) {
            consumed = handle(navigation_bar, navigation_geometry, navigation_scroll.snapshot(),
                              [this](float value) { return navigation_scroll.scroll_to(value); });
        }
        if (!consumed) {
            consumed = handle(document_bar, document_geometry, document_scroll.snapshot(),
                              [this](float value) { return document_scroll.scroll_to(value); });
        }
        if (consumed) {
            frames.request_frame();
            return true;
        }
    }
    app.pointer().dispatch(event);
    return true;
}

bool GalleryPagesHost::Impl::scroll(const input::ScrollInputEvent& event) {
    ++work.wheel_events;
    if (inputs.dispatch(event)) {
        ++work.component_wheels;
        return true;
    }
    if (services.interactions().input_boundary()) {
        return true;
    }
    const float delta = -48 * (event.delta_y != 0 ? event.delta_y : event.delta_x);
    bool changed = false;
    switch (gallery_scroll_target(event.x, event.y, metrics.narrow, metrics.navigation, metrics.document)) {
    case GalleryScrollTarget::navigation:
        ++work.navigation_wheels;
        changed = navigation_scroll.scroll_by(delta);
        break;
    case GalleryScrollTarget::document:
        ++work.document_wheels;
        changed = document_scroll.scroll_by(delta);
        break;
    case GalleryScrollTarget::none:
        return false;
    }
    if (changed) {
        frames.request_frame();
    }
    return true;
}

void GalleryPagesHost::Impl::focus_search() {
    if (metrics.narrow) {
        directory_open.set(true);
    } else {
        static_cast<void>(navigation_scroll.scroll_to(0));
    }
    search_after_layout = true;
    frames.request_frame();
}

bool GalleryPagesHost::Impl::keyboard(const input::KeyboardInputEvent& event) {
    const auto session = inputs.sessions().active();
    const auto* editor = inputs.editors().find(session.owner);
    if (event.key == input::Key::k && input::has_modifier(event.modifiers, input::KeyModifier::control) &&
        !input::has_modifier(event.modifiers, input::KeyModifier::alt) && !services.interactions().input_boundary() &&
        (!editor || !editor->composition().active)) {
        if (event.action == input::KeyAction::down && !event.repeat) {
            focus_search();
        }
        return true;
    }
    const auto focus = app.focus().state().focused;
    const auto* record = focus ? services.interactions().find(*focus) : nullptr;
    const bool page_focus =
        record && (within(record->component, target("gallery.page.enter")) || within(record->component, toc_holder) ||
                   within(record->component, target("gallery.page.inline-toc")));
    const bool editor_focus = record && std::ranges::any_of(inputs.mounted_inputs(), [record](const auto& item) {
                                  return item.component == record->component;
                              });
    if (event.action == input::KeyAction::down && !editor_focus && !services.interactions().input_boundary() &&
        (page_focus || !record)) {
        bool changed = false;
        switch (event.key) {
        case input::Key::page_down:
            changed = document_scroll.scroll_by(metrics.document.height * 0.9F);
            break;
        case input::Key::page_up:
            changed = document_scroll.scroll_by(-metrics.document.height * 0.9F);
            break;
        case input::Key::home:
            changed = document_scroll.scroll_to(0);
            break;
        case input::Key::end:
            changed = document_scroll.scroll_to(document_scroll.snapshot().maximum_offset);
            break;
        default:
            app.focus().dispatch(event);
            return reveal_focus();
        }
        if (changed) {
            frames.request_frame();
        }
        return true;
    }
    app.focus().dispatch(event);
    static_cast<void>(reveal_focus());
    return true;
}

bool GalleryPagesHost::Impl::reveal_focus() {
    const auto focused = app.focus().state().focused;
    const auto* record = focused ? services.interactions().find(*focused) : nullptr;
    if (!record || services.interactions().input_boundary()) {
        return false;
    }
    const auto& node = app.nodes().require(record->node);
    const auto y = node.bounds.y + node.translation.y;
    bool changed = false;
    if (document && within(record->component, controller->root())) {
        if (y < metrics.document.y + 8) {
            changed = document_scroll.scroll_by(y - metrics.document.y - 8);
        } else if (y + node.bounds.height > metrics.document.y + metrics.document.height - 8) {
            changed =
                document_scroll.scroll_by(y + node.bounds.height - metrics.document.y - metrics.document.height + 8);
        }
    } else if (!metrics.narrow && within(record->component, navigation_mount)) {
        if (y < metrics.navigation.y + 8) {
            changed = navigation_scroll.scroll_by(y - metrics.navigation.y - 8);
        } else if (y + node.bounds.height > metrics.navigation.y + metrics.navigation.height - 8) {
            changed = navigation_scroll.scroll_by(y + node.bounds.height - metrics.navigation.y -
                                                  metrics.navigation.height + 8);
        }
    }
    if (changed) {
        frames.request_frame();
    }
    return changed;
}

bool GalleryPagesHost::Impl::dispatch(const input::PlatformInputEvent& event) {
    if (!mounted || stopped || !input::is_valid(event)) {
        return false;
    }
    const auto started = std::chrono::steady_clock::now();
    ++work.input_events;
    const bool consumed = std::visit(
        [this](const auto& value) -> bool {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, input::PointerInputEvent>) {
                return pointer(value);
            } else if constexpr (std::is_same_v<T, input::ScrollInputEvent>) {
                return scroll(value);
            } else if constexpr (std::is_same_v<T, input::KeyboardInputEvent>) {
                return keyboard(value);
            } else if constexpr (std::is_same_v<T, input::WindowInputEvent>) {
                if (value.action == input::WindowInputAction::focus_gained ||
                    value.action == input::WindowInputAction::focus_lost) {
                    app.set_window_active(value.action == input::WindowInputAction::focus_gained);
                    inputs.set_window_active(value.action == input::WindowInputAction::focus_gained);
                    if (value.action == input::WindowInputAction::focus_lost) {
                        static_cast<void>(navigation_bar.reset());
                        static_cast<void>(document_bar.reset());
                    }
                }
                return true;
            } else if constexpr (std::is_same_v<T, input::ClipboardChanged>) {
                return false;
            } else {
                static_cast<void>(inputs.dispatch(value));
                return true;
            }
        },
        event);
    // No component callback remains on the stack at this safe boundary.
    static_cast<void>(process());
    work.input_nanoseconds +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
    return consumed;
}

void GalleryPagesHost::Impl::set_theme(const ThemeConfig& config) {
    theme.set(config);
    if (document) {
        document->samples.set_theme(config);
    }
    frames.request_frame();
}

void GalleryPagesHost::Impl::shutdown() noexcept {
    if (stopped) {
        return;
    }
    stopped = true;
    if (controller) {
        controller->shutdown();
    }
    if (app.components().contains(shell)) {
        static_cast<void>(surfaces.destroy(shell));
    }
    document.reset();
}

GalleryPagesHost::GalleryPagesHost(ReferenceSurfaceHost& surfaces, ryn::detail::InputComponentHost& inputs,
                                   runtime::FrameRequestState& frames, Copy copy)
    : impl_(std::make_unique<Impl>(surfaces, inputs, frames, std::move(copy))) {}

GalleryPagesHost::~GalleryPagesHost() {
    shutdown();
}

void GalleryPagesHost::mount(runtime::Size viewport) {
    auto& s = *impl_;
    if (s.mounted || s.stopped) {
        throw std::logic_error("Gallery pages host can only be mounted once");
    }
    s.update_metrics(viewport);
    s.surfaces.mount(Content{[&s] { s.build_shell(); }}, &s.inputs);
    s.shell = s.app.components().root_components().front();
    for (const auto& item : s.surfaces.mounted_surfaces()) {
        const auto identity = s.surfaces.snapshot(item.component).identity;
        if (!identity.empty()) {
            s.shell_targets.emplace(identity, item.component);
        }
    }
    const auto nav = s.shell_targets.at("gallery.shell.navigation.viewport");
    const auto doc = s.shell_targets.at("gallery.shell.document.viewport");
    s.navigation_node = s.app.components().root(nav);
    s.document_node = s.app.components().root(doc);
    s.navigation_holder = s.app.components().children(nav).front();
    s.page_holder = s.app.components().children(doc).front();
    s.toc_holder = s.shell_targets.at("gallery.shell.toc");
    ryn::layout::FlexLayout header_layout;
    header_layout.direction = ryn::layout::FlexDirection::vertical;
    header_layout.item_policy = ryn::layout::FlexItemPolicy::sequential;
    s.app.layout().set_layout(s.app.components().root(s.shell_targets.at("gallery.shell.header")), header_layout);
    s.install_viewport(s.navigation_node);
    s.install_viewport(s.document_node);
    s.controller = std::make_unique<GalleryPageController>(
        s.surfaces, s.pages, s.frames, s.page_holder,
        [&s](const auto& route, const auto& draft) { return s.build_page(route, draft); });
    s.mounted = true;
    s.rebuild_navigation();
    static_cast<void>(s.request({}));
    static_cast<void>(s.process());
}

bool GalleryPagesHost::request(GalleryPageRoute route) {
    return impl_->request(std::move(route));
}

bool GalleryPagesHost::process() {
    return impl_->process();
}

bool GalleryPagesHost::dispatch(const input::PlatformInputEvent& event) {
    return impl_->dispatch(event);
}

bool GalleryPagesHost::synchronize(runtime::Size viewport) {
    return impl_->synchronize(viewport);
}

void GalleryPagesHost::set_theme(const ThemeConfig& theme) {
    impl_->set_theme(theme);
}

ThemeConfig GalleryPagesHost::theme() const {
    return impl_->theme.get();
}

GalleryPagesSnapshot GalleryPagesHost::snapshot() const {
    const auto& s = *impl_;
    return {s.pages.current(),
            s.metrics,
            s.navigation_scroll.snapshot(),
            s.document_scroll.snapshot(),
            s.controller ? s.controller->generation() : 0,
            s.directory_open.get(),
            s.controller && s.controller->failed(),
            s.shell,
            s.controller ? s.controller->root() : runtime::ComponentId{}};
}

const GalleryPagesWork& GalleryPagesHost::work() const noexcept {
    return impl_->work;
}

const GalleryPageModel& GalleryPagesHost::model() const noexcept {
    return impl_->pages;
}

runtime::ComponentId GalleryPagesHost::target(std::string_view identity) const {
    return impl_->target(identity);
}

std::uint64_t GalleryPagesHost::sample_runs(std::string_view identity) const {
    return impl_->document && impl_->document->samples.sample_runs ? impl_->document->samples.sample_runs(identity) : 0;
}

bool GalleryPagesHost::pending() const noexcept {
    return impl_->controller && impl_->controller->pending();
}

void GalleryPagesHost::shutdown() noexcept {
    impl_->shutdown();
}

} // namespace rynui::example
