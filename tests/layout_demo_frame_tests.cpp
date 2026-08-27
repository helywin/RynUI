#include "component/button_component.hpp"
#include "component/flex_component.hpp"
#include "component/space_component.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float actual, float expected, float tolerance = 0.25F) {
    return std::fabs(actual - expected) <= tolerance;
}

struct Fixture final {
    Fixture()
        : fonts(create_runtime()),
          engine(*fonts),
          layout(nodes),
          dirty(nodes, &frames),
          text_scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Layout demo frame fonts failed to load");
        host = std::make_unique<ryn::detail::ButtonComponentHost>(
            nodes,
            layout,
            dirty,
            text_scene,
            std::vector<ryn::font::FontIdentity>{latin.font, cjk.font},
            frames);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::DirtyQueues dirty;
    ryn::detail::TextSceneService text_scene;
    std::unique_ptr<ryn::detail::ButtonComponentHost> host;
};

class ControlledEvents final : public ryn::runtime::FrameEventSource {
public:
    ControlledEvents(
        ryn::detail::ButtonComponentHost& host,
        ryn::runtime::FrameRequestState& frames) noexcept
        : host_(&host), frames_(&frames) {}

    std::uint64_t now_milliseconds() const noexcept override { return now_; }
    bool poll_frame_event() noexcept override { return dispatch_next(); }
    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now_ += timeout;
        return dispatch_next();
    }

    void push(ryn::input::PlatformInputEvent event) {
        events_.push_back(std::move(event));
    }

    [[nodiscard]] std::uint64_t dispatched() const noexcept { return dispatched_; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }

private:
    bool dispatch_next() noexcept {
        if (next_ >= events_.size()) {
            return false;
        }
        try {
            std::visit([this](const auto& event) { dispatch(event); }, events_[next_++]);
            ++dispatched_;
            return frames_->pending();
        } catch (...) {
            failed_ = true;
            return true;
        }
    }

    void dispatch(const ryn::input::PointerInputEvent& event) {
        host_->pointer().dispatch(event);
    }

    void dispatch(const ryn::input::KeyboardInputEvent& event) {
        host_->focus().dispatch(event);
    }

    void dispatch(const ryn::input::WindowInputEvent&) {
        throw std::invalid_argument("Window events are not used by this fixture");
    }

    ryn::detail::ButtonComponentHost* host_;
    ryn::runtime::FrameRequestState* frames_;
    std::vector<ryn::input::PlatformInputEvent> events_;
    std::size_t next_{};
    std::uint64_t now_{};
    std::uint64_t dispatched_{};
    bool failed_{};
};

class LayoutSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    LayoutSubmitter(
        ryn::detail::ButtonComponentHost& host,
        ryn::runtime::FrameRequestState& frames) noexcept
        : host_(&host), frames_(&frames) {}

    void set_viewport(ryn::runtime::Size viewport) {
        viewport_ = viewport;
        frames_->request_frame();
    }

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        return host_->layout_and_synchronize(
                   viewport_,
                   {0.0F, 0.0F, viewport_.width, viewport_.height},
                   {24.0F, 20.0F},
                   0.0F)
            ? ryn::runtime::FrameSubmissionResult::submitted
            : ryn::runtime::FrameSubmissionResult::failed;
    }

private:
    ryn::detail::ButtonComponentHost* host_;
    ryn::runtime::FrameRequestState* frames_;
    ryn::runtime::Size viewport_{720.0F, 480.0F};
};

ryn::input::PointerInputEvent pointer_event(
    ryn::input::PointerAction action,
    ryn::runtime::Point point,
    ryn::input::PointerButton button = ryn::input::PointerButton::none) {
    return {
        ryn::input::PointerIdentity::mouse(),
        action,
        button,
        point.x,
        point.y,
    };
}

ryn::input::KeyboardInputEvent key_event(
    ryn::input::Key key,
    ryn::input::KeyAction action) {
    return {key, action, ryn::input::KeyModifier::none, false};
}

void require_event_step(
    ControlledEvents& events,
    ryn::runtime::OnDemandFrameLoop& loop,
    ryn::input::PlatformInputEvent event,
    ryn::runtime::FrameLoopStep expected,
    const char* message) {
    events.push(std::move(event));
    require(loop.step() == expected, message);
}

void test_responsive_layout_demo_frame_contract() {
    Fixture fixture;
    ryn::Signal<bool> vertical{false};
    ryn::Signal<bool> wrap{true};
    ryn::Signal<ryn::FlexJustify> justify{ryn::FlexJustify::Start};
    ryn::Signal<ryn::FlexAlign> align{ryn::FlexAlign::Center};
    ryn::Signal<ryn::LayoutGap> gap{
        ryn::LayoutGap{ryn::dp(10.0F), ryn::dp(14.0F)}};
    ryn::Signal<float> grow{1.0F};
    ryn::Signal<int> order{1};
    std::uint64_t content_runs = 0;
    std::uint64_t pointer_activations = 0;
    std::uint64_t keyboard_activations = 0;
    std::size_t dirty_measure_roots = 0;
    std::size_t dirty_place_roots = 0;

    const auto toggle_item = [&] {
        ++pointer_activations;
        wrap.set(false);
        gap.set(ryn::LayoutGap{ryn::dp(18.0F), ryn::dp(22.0F)});
        grow.set(2.0F);
        order.set(-1);
        dirty_measure_roots = fixture.dirty.layout_roots().size();
        dirty_place_roots = fixture.dirty.placement_roots().size();
    };
    const auto toggle_container = [&] {
        ++keyboard_activations;
        vertical.set(true);
        justify.set(ryn::FlexJustify::End);
        align.set(ryn::FlexAlign::Stretch);
        dirty_measure_roots = fixture.dirty.layout_roots().size();
        dirty_place_roots = fixture.dirty.placement_roots().size();
    };

    fixture.host->mount(ryn::Content{[&] {
        ++content_runs;
        ryn::Flex(
            ryn::FlexProps{}.vertical(true).gap(ryn::dp(12.0F)),
            [&] {
                ryn::Text(u8"Responsive Flex / 响应式布局");
                ryn::Flex(
                    ryn::FlexProps{}
                        .vertical(vertical)
                        .wrap(wrap)
                        .justify(justify)
                        .align(align)
                        .gap(gap)
                        .layout(ryn::LayoutStyle{}.width(ryn::dp(672.0F))),
                    [&] {
                        ryn::Button(
                            ryn::ButtonProps{}
                                .layout(
                                    ryn::LayoutStyle{}
                                        .min_width(ryn::dp(80.0F))
                                        .flex_basis(ryn::dp(180.0F))
                                        .flex_grow(grow)
                                        .flex_shrink(1.0F)
                                        .order(order))
                                .onClick(toggle_item),
                            [] { ryn::Text(u8"Pointer item / 指针"); });
                        ryn::Button(
                            ryn::ButtonProps{}
                                .layout(
                                    ryn::LayoutStyle{}
                                        .min_width(ryn::dp(80.0F))
                                        .flex_basis(ryn::dp(180.0F))
                                        .flex_grow(1.0F)
                                        .flex_shrink(1.0F)
                                        .order(0))
                                .onClick(toggle_container),
                            [] { ryn::Text(u8"Keyboard item / 键盘"); });
                    });
                ryn::Space(
                    ryn::SpaceProps{}
                        .wrap(true)
                        .align(ryn::SpaceAlign::Center)
                        .size(ryn::dp(8.0F), ryn::dp(16.0F)),
                    [&] {
                        ryn::Text(u8"Space A");
                        ryn::Text(u8"Space 项目 B");
                    });
            });
    }});

    auto& components = fixture.host->components();
    const auto roots_before = std::vector(
        components.root_components().begin(), components.root_components().end());
    require(roots_before.size() == 1, "Layout demo must expose one public root");
    const auto root = roots_before.front();
    const auto root_children_before = components.children(root);
    require(root_children_before.size() == 3,
            "Layout demo root did not retain Text, Flex, and Space children");
    const auto responsive = root_children_before[1];
    const auto space = root_children_before[2];
    const auto responsive_node = components.root(responsive);
    const auto component_count = components.component_count();
    const auto responsive_children_before = components.children(responsive);

    ControlledEvents events(*fixture.host, fixture.frames);
    LayoutSubmitter submitter(*fixture.host, fixture.frames);
    ryn::runtime::OnDemandFrameLoop loop(fixture.frames, events, submitter, 5);

    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "initial wide layout frame was not submitted");
    const auto wide_lines =
        fixture.layout.flex_layout_diagnostics(responsive_node).line_count;
    require(wide_lines == 1, "wide viewport did not keep Flex on one line");
    const auto first = fixture.host->mounted_buttons()[0];
    const auto second = fixture.host->mounted_buttons()[1];
    const auto wide_first = fixture.nodes.require(first.node).bounds;
    const auto wide_second = fixture.nodes.require(second.node).bounds;
    const auto* left = &wide_first;
    const auto* right = &wide_second;
    if (left->x > right->x) {
        std::swap(left, right);
    }
    require(near(right->x - (left->x + left->width), 10.0F),
            "wide Flex did not apply the main-axis custom gap");

    submitter.set_viewport({260.0F, 480.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "narrow wrapped layout frame was not submitted");
    const auto narrow_lines =
        fixture.layout.flex_layout_diagnostics(responsive_node).line_count;
    require(narrow_lines == 2,
            "narrow viewport did not form two deterministic Flex lines");
    const auto narrow_first = fixture.nodes.require(first.node).bounds;
    const auto narrow_second = fixture.nodes.require(second.node).bounds;
    const auto* upper = &narrow_first;
    const auto* lower = &narrow_second;
    if (upper->y > lower->y) {
        std::swap(upper, lower);
    }
    require(near(lower->y - (upper->y + upper->height), 14.0F),
            "narrow Flex did not apply the cross-axis custom gap");
    require(fixture.layout.flex_layout_diagnostics(components.root(space)).line_count >= 1,
            "nested Space did not participate in responsive measurement");

    wrap.set(false);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "no-wrap shrink transition did not submit");
    const auto shrunk_first = fixture.nodes.require(first.node).bounds;
    const auto shrunk_second = fixture.nodes.require(second.node).bounds;
    require(shrunk_first.width < 180.0F && shrunk_second.width < 180.0F,
            "negative free space did not shrink both Flex items");

    wrap.set(true);
    submitter.set_viewport({720.0F, 480.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "wide layout restoration did not submit");
    const auto pointer_bounds = fixture.nodes.require(first.node).bounds;
    const ryn::runtime::Point pointer_inside{
        pointer_bounds.x + pointer_bounds.width * 0.5F,
        pointer_bounds.y + pointer_bounds.height * 0.5F,
    };
    require_event_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::down,
            pointer_inside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::submitted,
        "pointer down did not submit the pressed layout state");
    require_event_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::up,
            pointer_inside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::submitted,
        "pointer activation did not submit changed layout Props");
    const auto grown_first = fixture.nodes.require(first.node).bounds;
    const auto grown_second = fixture.nodes.require(second.node).bounds;
    require(pointer_activations == 1,
            "pointer activation did not invoke the public Button callback");
    require(grown_first.x < grown_second.x,
            "reactive order did not move the first declared item before its sibling");
    require(grown_first.width > grown_second.width,
            "reactive grow did not allocate more positive free space to the first item");
    require(dirty_measure_roots > 0,
            "pointer layout updates did not record a measure dirty root");

    require_event_step(
        events,
        loop,
        key_event(ryn::input::Key::tab, ryn::input::KeyAction::down),
        ryn::runtime::FrameLoopStep::submitted,
        "Tab did not move focus in declaration order");
    require_event_step(
        events,
        loop,
        key_event(ryn::input::Key::enter, ryn::input::KeyAction::down),
        ryn::runtime::FrameLoopStep::submitted,
        "keyboard activation did not submit container Props");
    const auto* responsive_state =
        components.state<ryn::detail::FlexComponentState>(responsive);
    require(keyboard_activations == 1 && responsive_state != nullptr
                && responsive_state->model.direction ==
                    ryn::layout::FlexDirection::vertical
                && responsive_state->model.justify == ryn::layout::FlexJustify::end
                && responsive_state->model.align == ryn::layout::FlexAlign::stretch
                && (dirty_measure_roots > 0 || dirty_place_roots > 0),
            "keyboard activation did not update direction and alignment Props");

    require(content_runs == 1
                && components.component_count() == component_count
                && std::vector(
                       components.root_components().begin(),
                       components.root_components().end()) == roots_before
                && components.children(root) == root_children_before
                && components.children(responsive) == responsive_children_before
                && fixture.host->scene_composer().diagnostics().rebuilds == 1,
            "ordinary layout Props reran content or rebuilt retained topology");

    const auto submissions = loop.counters().submissions;
    for (int index = 0; index < 40; ++index) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                "stable responsive layout continued submitting frames");
    }
    require(loop.counters().submissions == submissions
                && loop.counters().idle_waits >= 40
                && events.dispatched() == 4
                && !events.failed(),
            "layout headless evidence missed input, submit, or idle counts");
}

} // namespace

int main() {
    try {
        test_responsive_layout_demo_frame_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
