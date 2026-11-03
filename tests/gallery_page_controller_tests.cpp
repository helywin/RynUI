#include "gallery_page_controller.hpp"
#include "component/tree_select_component.hpp"
#include "support/input_fixture.hpp"
#include "support/input_counting_gpu.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

using namespace ryn;
using namespace rynui::example;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GalleryPageModel model;
    runtime::ComponentId shell;
    runtime::ComponentId holder;
    std::unique_ptr<GalleryPageController> pages;
    std::shared_ptr<Signal<String>> value;
    InputRef editor;
    TreeSelectRef tree;
    std::size_t builds{};
    std::size_t saves{};
    bool throw_mount{};
    bool reenter_retire{};
    bool capture_anchor{};
    GalleryScrollbarController scrollbar;
    std::size_t activations{};
    Signal<ThemeConfig> theme{ThemeConfig{}};

    Fixture() {
        surfaces.mount(Content{[this] {
                           Theme(ThemeProps{}.config(theme), ThemeContent{[] {
                                     Flex(FlexProps{}.vertical(true), [] {
                                         Button(ButtonProps{}, [] { Text(u8"目录入口"); });
                                         Flex(FlexProps{}.vertical(true), [] {});
                                     });
                                 }});
                       }},
                       &inputs);
        shell = services.components().root_components().front();
        holder = services.components().children(shell).back();
        pages = std::make_unique<GalleryPageController>(
            surfaces, model, frames, holder,
            [this](const GalleryPageRoute& route, const GalleryPageDraft& draft) -> GalleryPageContent {
                ++builds;
                if (throw_mount) {
                    return {Content{[] {
                                ReferenceSurface(ReferenceSurfaceProps{}.identity("half-mounted"), [] {
                                    Text(u8"还未完成");
                                    throw std::runtime_error("deliberate page failure");
                                });
                            }},
                            {},
                            {}};
                }
                if (route.component_identity == "ant.component.input") {
                    value = std::make_shared<Signal<String>>(
                        String::from_utf8(draft.get<std::string>("value", "initial")).value());
                    editor = InputRef{};
                    const auto current = value;
                    return {Content{[current, ref = editor] {
                                ReferenceSurface(ReferenceSurfaceProps{}.identity("input-sample"), [current, ref] {
                                    Input(InputProps{}
                                              .value(*current)
                                              .onChange([current](String v) { current->set(std::move(v)); })
                                              .ref(ref));
                                });
                            }},
                            [this, current](GalleryPageDraft& target) {
                                ++saves;
                                target.values["value"] = std::string(current->get().bytes());
                            },
                            [this] {
                                static_cast<void>(scrollbar.reset());
                                if (capture_anchor) {
                                    pages->set_anchor({"retired", 17});
                                }
                                if (reenter_retire) {
                                    require(!pages->ticket().valid() && !pages->process(),
                                            "retirement allowed reentrant mounting");
                                    pages->request(model.find("ant.component.upload")->route());
                                }
                            }};
                }
                if (route.component_identity == "ant.component.tree-select") {
                    tree = TreeSelectRef{};
                    return {Content{[ref = tree] {
                                TreeSelect(TreeSelectProps{}.open(true).ref(ref),
                                           {TreeSelectItem{u8"guide", u8"使用指南"}});
                            }},
                            {},
                            {}};
                }
                return {Content{[this] {
                            Button(ButtonProps{}.onClick([this] { ++activations; }), [] { Text(u8"页面内容"); });
                        }},
                        {},
                        {}};
            });
    }

    void show(std::string_view identity, GalleryInputFamily family = GalleryInputFamily::input) {
        require(pages->request(model.find(identity)->route(family)) && pages->process(), "page navigation failed");
        synchronize(800, {0, 0, 800, 240});
        require(!pages->failed(), "page factory failed");
    }
};

void test_deferred_navigation_drafts_and_session_retirement() {
    Fixture f;
    f.show("ant.component.input");
    const auto old_root = f.pages->root();
    const auto old_ticket = f.pages->ticket();
    const auto old_editor = f.inputs.mounted_inputs().front();
    require(f.services.focus().request_focus(old_editor.interaction, input::FocusModality::keyboard),
            "editor focus failed");
    f.services.focus().dispatch({input::Key::a, input::KeyAction::down, input::KeyModifier::control});
    const auto session = f.inputs.sessions().active();
    require(session.valid() && bool(f.inputs.dispatch(input::TextCommitted{String{u8"saved draft"}, session})),
            "real editor commit failed");
    require(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {}, session})),
            "composition setup failed");
    f.pages->set_anchor({"support", 12});
    require(f.pages->request(f.model.find("ant.component.button")->route()) &&
                f.services.components().contains(old_root) && f.pages->ticket().valid(),
            "request destroyed a page inside its callback boundary");
    require(f.pages->process(), "deferred switch failed");
    require(!f.services.components().contains(old_root) && !old_ticket.valid() &&
                !f.inputs.sessions().active().valid() && !f.inputs.editors().find(old_editor.editor),
            "retired page retained editor/session/ticket");
    require(!bool(f.inputs.dispatch(input::TextCommitted{String{u8"late"}, session})),
            "old session changed the new page");
    require(!bool(f.inputs.dispatch(input::CompositionChanged{String{u8"迟到候选"}, {}, session})),
            "old composition entered the new page");
    f.show("ant.component.input");
    require(f.value->get() == String{u8"saved draft"} && f.pages->draft().anchor.identity == "support" &&
                f.pages->draft().anchor.distance == 12 && f.pages->root() != old_root,
            "committed draft or anchor was not restored with a new owner");
    f.capture_anchor = true;
    f.show("ant.component.button");
    f.capture_anchor = false;
    f.show("ant.component.input");
    require(f.pages->draft().anchor.identity == "retired" && f.pages->draft().anchor.distance == 17,
            "retirement stored the old anchor under the incoming route");
    const auto identity = f.pages->root();
    const auto builds = f.builds;
    auto route = f.model.current();
    route.section = GalleryPageSection::api;
    require(f.pages->request(route) && f.pages->process() && f.pages->root() == identity && f.builds == builds,
            "TOC change remounted a page");
    require(f.pages->back() && f.pages->process() && f.pages->can_forward(), "page history back failed");
    require(f.pages->forward() && f.pages->process() && f.model.current().component_identity == "ant.component.input",
            "page history forward failed");
}

void test_sensitive_groups_and_owner_cleanup() {
    Fixture f;
    f.show("ant.component.input", GalleryInputFamily::password);
    f.pages->set_anchor({"examples", 24});
    f.value->set(String{u8"do not cache"});
    const auto saves = f.saves;
    f.show("ant.component.tree-select");
    require(f.saves == saves, "Password called its persistence hook");
    require(!f.services.tree_select().mounted().empty(), "TreeSelect was not mounted");
    const auto tree = f.services.tree_select().mounted().front();
    require(f.services.tree_select().snapshot(tree).open, "controlled popup did not open");
    f.show("ant.component.upload");
    require(f.services.tree_select().mounted().empty() && !f.services.components().contains(tree),
            "controlled open popup survived owner retirement");
    f.show("ant.component.input", GalleryInputFamily::password);
    require(f.value->get() == String{u8"initial"} && f.pages->draft().values.empty() &&
                f.pages->draft().anchor.identity == "examples",
            "sensitive text or independent page anchor was lost");
    f.show("ant.component.input", GalleryInputFamily::otp);
    f.value->set(String{u8"123456"});
    f.show("ant.component.upload");
    f.show("ant.component.input", GalleryInputFamily::otp);
    require(f.value->get() == String{u8"initial"}, "OTP survived a page round trip");
    const auto ticket = f.pages->ticket();
    f.pages->shutdown();
    require(!ticket.valid() && !f.pages->root().valid() && !f.pages->back(), "shutdown left a live owner");
}

void test_coalescing_failure_rollback_and_recovery() {
    Fixture f;
    require(f.pages->request(f.model.find("ant.component.input")->route()) &&
                f.pages->request(f.model.find("ant.component.button")->route()) && f.pages->process() && f.builds == 1,
            "navigation requests mounted intermediate pages");
    const auto base = f.services.components().component_count();
    for (int i = 0; i < 5; ++i) {
        f.show("ant.component.input");
        f.show("ant.component.button");
        require(f.services.components().component_count() == base && f.surfaces.mounted_surfaces().empty(),
                "round trips accumulated live page components or surfaces");
    }
    f.throw_mount = true;
    require(f.pages->request(f.model.find("ant.component.input")->route()) && f.pages->process() && f.pages->failed() &&
                f.pages->last_error() == "deliberate page failure" && f.surfaces.mounted_surfaces().empty(),
            "failed mount did not prune its partial ReferenceSurface");
    require(!f.pages->pending() && !f.pages->process(), "failure created an immediate retry loop");
    const auto failed_ticket = f.pages->ticket();
    f.throw_mount = false;
    f.show("ant.component.input");
    require(!failed_ticket.valid() && f.surfaces.mounted_surfaces().size() == 1 &&
                f.services.components().contains(f.shell),
            "retry failed or changed the persistent shell");
}

void test_capture_keyboard_theme_and_idle() {
    Fixture f;
    f.show("ant.component.button");
    const auto button = f.buttons.mounted_buttons().back();
    const auto bounds = f.nodes.require(button.node).bounds;
    const auto mouse = input::PointerIdentity::mouse();
    f.services.pointer().dispatch({mouse, input::PointerAction::down, input::PointerButton::primary,
                                   bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    require(f.services.pointer().state(mouse)->capture == button.interaction, "button did not capture pointer");
    f.show("ant.component.input");
    const auto state = f.services.pointer().state(mouse);
    require(!state->capture && !state->primary_down && !state->press_origin,
            "page retirement retained pointer capture/press");
    f.services.pointer().dispatch({mouse, input::PointerAction::up, input::PointerButton::primary,
                                   bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    require(f.activations == 0, "late pointer up activated a retired page");
    const auto root = f.pages->root();
    const auto editor = f.inputs.mounted_inputs().front();
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    f.theme.set(dark);
    f.synchronize(480, {0, 0, 480, 240});
    require(f.pages->root() == root && f.inputs.mounted_inputs().front().editor == editor.editor,
            "theme/resize replaced a mounted page/editor");
    f.reenter_retire = true;
    const GalleryScrollbarGeometry geometry{{780, 0, 10, 240}, {780, 0, 10, 24}, 1000};
    const auto drag = f.scrollbar.dispatch({mouse, input::PointerAction::down, input::PointerButton::primary, 785, 10},
                                           geometry, 240, 0);
    require(drag.consumed && f.scrollbar.dragging(), "scrollbar drag setup failed");
    f.show("ant.component.button");
    require(!f.scrollbar.dragging(), "retirement did not cancel the page scrollbar drag");
    require(f.pages->pending() && f.pages->process() && f.model.current().component_identity == "ant.component.upload",
            "reentrant request was lost");
    f.synchronize();
    const auto target = f.buttons.mounted_buttons().back();
    require(f.services.focus().request_focus(target.interaction, input::FocusModality::keyboard),
            "button keyboard focus failed");
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    require(f.services.focus().state().keyboard_pressed, "keyboard press setup failed");
    f.show("ant.component.input");
    f.services.focus().dispatch({input::Key::space, input::KeyAction::up});
    require(!f.services.focus().state().keyboard_pressed && f.activations == 0,
            "late keyboard release activated a retired page");
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    const auto active = f.inputs.mounted_inputs().front();
    require(f.services.focus().request_focus(active.interaction, input::FocusModality::keyboard), "caret focus failed");
    require(f.inputs.next_caret_deadline().has_value(), "normal-motion editor did not request a caret deadline");
    f.show("ant.component.upload");
    f.services.tick_animations(animation::AnimationTime::microseconds(10000000));
    f.synchronize();
    require(!f.services.next_frame_deadline(), "retired editor/animation kept a deadline alive");
    ryn_test::input_component::CountingGpu gpu;
    ryn::detail::QuadGpuBuffer quads{gpu, f.buttons.button_scene().instances(), {320, 240, 1}};
    ryn::detail::GlyphGpuResources glyphs{gpu};
    ryn::detail::RoundedEffectGpuResources effects{gpu};
    glyphs.synchronize(f.scene.atlas(), f.scene.glyph_scene().instances(), {320, 240, 1});
    effects.synchronize(f.buttons.rounded_effects(), {320, 240, 1});
    const auto uploads = gpu.quad_uploads + gpu.glyph_uploads + gpu.texture_uploads + gpu.effect_uploads;
    static_cast<void>(f.frames.consume_request());
    f.synchronize();
    quads.synchronize(f.buttons.button_scene().instances(), {320, 240, 1});
    glyphs.synchronize(f.scene.atlas(), f.scene.glyph_scene().instances(), {320, 240, 1});
    effects.synchronize(f.buttons.rounded_effects(), {320, 240, 1});
    require(uploads == gpu.quad_uploads + gpu.glyph_uploads + gpu.texture_uploads + gpu.effect_uploads &&
                !f.frames.pending(),
            "settled page kept uploading or requesting frames");
}

} // namespace

int main() {
    try {
        test_deferred_navigation_drafts_and_session_retirement();
        test_sensitive_groups_and_owner_cleanup();
        test_coalescing_failure_rollback_and_recovery();
        test_capture_keyboard_theme_and_idle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
