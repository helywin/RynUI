#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        ryn_test::input_component::Fixture fixture;
        ryn::detail::SelectionComponentHost selections{fixture.services};
        fixture.services.set_motion_preference(ryn::animation::MotionPreference::normal);
        fixture.buttons.mount(ryn::Content{[] {
            ryn::Input(ryn::InputProps{}.defaultValue(u8"Input"));
            ryn::Search(ryn::SearchProps{}.defaultValue(u8"Search")
                .layout(ryn::LayoutStyle{}.width(ryn::dp(220.0F))));
            ryn::Button(ryn::ButtonProps{}, ryn::ButtonContent{[] { ryn::Text(u8"Button"); }});
            ryn::Switch(ryn::SwitchProps{});
            ryn::Checkbox(ryn::CheckboxProps{},
                ryn::CheckboxLabel{[] { ryn::Text(u8"Checkbox"); }});
        }});
        fixture.synchronize();
        if (fixture.services.next_frame_deadline())
            throw std::runtime_error("idle Search window scheduled a frame");
        const auto mounts = fixture.services.components().mount_runs();
        const auto rebuilds = fixture.services.scene_composer().diagnostics().rebuilds;
        const auto surface_updates = fixture.services.surfaces().diagnostics().material_updates;
        constexpr std::size_t iterations = 10'000;
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < iterations; ++index) {
            if (fixture.services.next_frame_deadline()
                || fixture.services.tick_animations(
                    ryn::animation::AnimationTime::microseconds(
                        static_cast<std::int64_t>(index) * 16'000)) != 0) {
                throw std::runtime_error("idle Search window did animation work");
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started);
        if (fixture.services.components().mount_runs() != mounts
            || fixture.services.scene_composer().diagnostics().rebuilds != rebuilds
            || fixture.services.surfaces().diagnostics().material_updates != surface_updates) {
            throw std::runtime_error("idle Search window updated retained content");
        }
        std::cout << "idle_iterations=" << iterations
            << " elapsed_us=" << elapsed.count()
            << " animation_updates=0 scene_rebuilds=0 material_updates=0\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
