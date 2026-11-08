#include "component/app_component.hpp"
#include "component/card_component.hpp"
#include "component/modal_component.hpp"
#include "component/feedback_component.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;
using namespace ryn::graphics;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("unsupported backdrop accepted");
}

void sync(ryn_test::input_component::Fixture& f) {
    require(f.services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "Modal backdrop layout");
}

void actual_modal_and_service() {
    ryn_test::input_component::Fixture f;
    AppRef app;
    Signal<ModalMask> mask{ModalMask{true, true, true}};
    Signal<bool> modal_open{true};
    Signal<bool> spin{false};
    int mounts = 0;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                Card(CardProps{}.clipContent(true), CardContent{[&] {
                         Input(InputProps{}.defaultValue(u8"background"));
                         Modal(ModalProps{}.open(modal_open).mask(mask).title(u8"Declarative"), ModalContent{[&] {
                                   ++mounts;
                                   Input(InputProps{}.defaultValue(u8"dialog draft"));
                               }});
                     }});
                Spin(SpinProps{}.fullscreen(true).spinning(spin));
            }});
    }});
    sync(f);
    const auto modal = f.services.modal().mounted().front();
    const auto identity = f.services.modal().snapshot(modal).body;
    const auto check_program = [&](std::size_t backdrop_count) {
        const auto& scene = f.services.scene_composer().ordered_scene();
        require(bool(scene.composition()), "Card/Modal composition missing");
        unsigned depth = 0;
        std::size_t count = 0;
        for (const auto op : scene.composition()->operations) {
            if (op.kind == SceneCompositionOpKind::BeginMaskedGroup) {
                ++depth;
            }
            if (op.kind == SceneCompositionOpKind::EndMaskedGroup) {
                --depth;
            }
            if (op.kind == SceneCompositionOpKind::Backdrop) {
                require(depth == 0, "Modal failed to escape Card mask at actual painter boundary");
                ++count;
                const auto& request = scene.composition()->requests[op.request];
                require(request.backdrop && request.sigma == 4 && request.owner.valid(),
                        "native Theme sigma/owner missing");
            }
        }
        require(depth == 0 && count == backdrop_count, "actual Modal prefix count differs");
    };
    check_program(1);
    const auto background = f.inputs.mounted_inputs().front();
    require(!f.services.interactions().component_active(background.component),
            "Card clip escaped Modal input authority");
    const auto handle =
        app.modal().confirm(ModalProps{}.mask(mask).title(u8"Service"), ModalContent{[] { Text(u8"service dialog"); }});
    sync(f);
    check_program(2);
    RecordingRenderer renderer;
    SceneResources resources(renderer);
    const auto draw = [&] {
        const auto& scene = f.services.scene_composer().ordered_scene();
        require(resources.synchronize({&f.services.surfaces().instances(),
                                       f.scene.atlas(),
                                       f.scene.glyph_scene().instances(),
                                       &f.services.rounded_effects(),
                                       {720, 600, 1},
                                       &f.services.images(),
                                       &scene}),
                "service backdrop sync");
        require(renderer.attach_scene(resources.attach(scene)) &&
                    renderer.submit_frame({}) == runtime::FrameSubmissionResult::submitted,
                "service backdrop real interpreter");
    };
    draw();
    spin.set(true);
    sync(f);
    require(!f.services.interactions().component_active(modal), "Modal backdrop bypassed fullscreen Spin authority");
    draw();
    spin.set(false);
    mask.set({false, true, true});
    sync(f);
    check_program(0);
    require(mounts == 1 && f.services.modal().snapshot(modal).body == identity,
            "blur/mask toggle remounted Modal body");
    mask.set({true, true, true});
    sync(f);
    check_program(2);
    renderer.set_auto_complete_frames(false);
    draw();
    require(handle.destroy() && handle.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready,
            "GPU request delayed business close future");
    sync(f);
    check_program(1);
    renderer.complete_frames();
    renderer.set_auto_complete_frames(true);
    modal_open.set(false);
    sync(f);
    check_program(0);
    f.services.dispose();
    require(f.nodes.size() == 0 && f.services.interactions().size() == 0, "Modal backdrop owner/input leaked");
}

void theme_and_limits() {
    const auto original = resolve_theme();
    require(original.modal().effects.mask_blur_sigma == 4, "native default sigma");
    ThemeConfig config;
    config.modal.tokens.mask_blur_sigma = dp(0);
    const auto zero = resolve_theme(config);
    require(zero.modal().effects.mask_blur_sigma == 0 && zero.identity() != original.identity() &&
                zero.diagnostic_json().find("\"maskBlurSigma\":0") != std::string::npos &&
                zero.drawer() == original.drawer(),
            "native sigma identity/serialization leaked to Drawer");
    config.modal.tokens.mask_blur_sigma = dp(-1);
    rejects([&] { (void)resolve_theme(config); });
    OrderedScene scene;
    auto p = std::make_shared<SceneCompositionProgram>();
    p->scene_revision = scene.revision();
    p->requests.push_back({{1, 1}, {{0, 0, 32, 24}, 0}, {0, 0, 32, 24}, true, 4, {0, 0, 0, .4F}, 1});
    // Hidden live request has no executed op, but still requires the filter ABI.
    scene.set_composition(p);
    auto caps = recording_scene_capabilities();
    for (unsigned mode = 0; mode < 5; ++mode) {
        auto bad = caps;
        if (mode == 0) {
            bad.gaussian_backdrop = false;
        }
        if (mode == 1) {
            bad.backdrop_filter_abi_version = 0;
        }
        if (mode == 2) {
            bad.maximum_gaussian_radius = 1;
        }
        if (mode == 3) {
            bad.maximum_gaussian_weight_bytes = 16;
        }
        if (mode == 4) {
            bad.maximum_buffer_bytes = 16;
        }
        rejects([&] { (void)pack_composition(scene, {32, 24, 1}, bad); });
    }
    auto bad = *p;
    bad.requests[0].sigma = std::numeric_limits<float>::infinity();
    scene.set_composition(std::make_shared<SceneCompositionProgram>(bad));
    rejects([&] { (void)pack_composition(scene, {32, 24, 1}, caps); });
}
} // namespace

int main() {
    try {
        actual_modal_and_service();
        theme_and_limits();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
