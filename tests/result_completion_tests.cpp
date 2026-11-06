#include "component/empty_result_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void artwork_and_retained_slots() {
    Fixture f;
    Signal<ResultStatus> status{ResultStatus{ResultException::Forbidden}};
    Signal<std::shared_ptr<const ResultExtra>> extra{std::make_shared<const ResultExtra>(
        ResultExtra{[] { Input(InputProps{}.defaultValue(u8"temporary action editor")); }})};
    InputRef draft;
    InputRef action;
    int body_mounts{};
    f.services.mount(Content{[&] {
        Result(ResultProps{}.status(status).title(u8"Result").extraRender(extra),
               {.content = ResultContent{[&] {
                    ++body_mounts;
                    Input(InputProps{}.ref(draft).defaultValue(u8"saved draft"));
                }}});
    }});
    const auto sync = [&] {
        check(f.services.layout_and_synchronize({600, 900}, {0, 0, 600, 900}), "Result layout failed");
    };
    sync();
    const auto id = f.services.empty_result().results()[0];
    const auto initial = f.services.empty_result().snapshot(id);
    std::shared_ptr<const ImagePixels> previous;
    for (auto exception : {ResultException::Forbidden, ResultException::NotFound, ResultException::ServerError}) {
        status.set(ResultStatus{exception});
        sync();
        check(f.services.empty_result().snapshot(id).illustration && f.services.images().live_count() == 1,
              "exception is still a small fallback icon or duplicated artwork");
        std::shared_ptr<const ImagePixels> pixels;
        for (const auto& slot : f.services.images().slots()) {
            if (slot.live) {
                pixels = slot.instance.pixels;
            }
        }
        check(pixels && pixels->height() == 588 && pixels->width() >= 500 && pixels != previous,
              "full pinned exception artwork/source identity missing");
        previous = pixels;
        check(f.services.empty_result().snapshot(id).content == initial.content && body_mounts == 1 && draft.bound(),
              "Result status update rebuilt its retained body");
    }
    extra.set(std::make_shared<const ResultExtra>(ResultExtra{[&] { Input(InputProps{}.ref(action)); }}));
    sync();
    check(action.focus(), "dynamic action region did not mount an actual input");
    extra.set({});
    sync();
    check(!action.bound() && !f.services.focus().state().focused && draft.bound() && body_mounts == 1,
          "removing a Result slot leaked focus or destroyed an unrelated draft");
    status.set(ResultStatus{Status::Success});
    sync();
    check(!f.services.empty_result().snapshot(id).illustration && !f.services.next_frame_deadline(),
          "reduced Result scheduled an animation or kept exception mode");
    check(f.services.destroy(id) && f.services.images().live_count() == 0 && f.services.interactions().size() == 0,
          "Result teardown leaked shared image or input state");
}

void dynamic_reentry_failure_and_motion() {
    Fixture f;
    Signal<std::shared_ptr<const ResultExtra>> extra{nullptr};
    Signal<ResultStatus> status{ResultStatus{Status::Success}};
    int final_mounts{};
    f.services.mount(Content{[&] {
        Result(ResultProps{}.title(u8"Retained").status(status).extraRender(extra));
        Button({}, [] { Text(u8"Unrelated survivor"); });
    }});
    f.synchronize();
    const auto id = f.services.empty_result().results()[0];
    const auto root = f.services.empty_result().snapshot(id).extra;
    const auto final = std::make_shared<const ResultExtra>(ResultExtra{[&] {
        ++final_mounts;
        Text(u8"Final slot");
    }});
    extra.set(std::make_shared<const ResultExtra>(ResultExtra{[&] {
        Text(u8"Transient");
        extra.set(final);
    }}));
    f.synchronize();
    check(final_mounts == 1 && f.services.empty_result().snapshot(id).extra == root,
          "reentrant dynamic slot lost latest content or region identity");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    status.set(ResultStatus{Status::Warning});
    f.synchronize();
    check(f.services.next_frame_deadline().has_value(), "Result finite visual transition did not start");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
    f.synchronize();
    check(!f.services.next_frame_deadline() && f.services.empty_result().snapshot(id).visual_motion == 1,
          "Result transition never settled");
    try {
        extra.set(std::make_shared<const ResultExtra>(ResultExtra{[] {
            Input(InputProps{});
            throw std::runtime_error("replacement slot failure");
        }}));
    } catch (const std::runtime_error&) {
    }
    f.synchronize();
    check(f.services.empty_result().results().empty() && f.buttons.mounted_buttons().size() == 1 &&
              f.services.interactions().size() == 1 && f.services.images().live_count() == 0,
          "failed replacement did not retire exactly the failed Result owner");
}
} // namespace

int main() {
    try {
        artwork_and_retained_slots();
        dynamic_reentry_failure_and_motion();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
