#pragma once
#include "component/input_component.hpp"
#include <ryn/rynui.hpp>
#include <map>
#include <memory>
#include <stdexcept>

namespace ryn_test::input_component {
using namespace ryn;
using namespace ryn::input;
struct Platform final : TextInputPlatform, TextClipboard {
    int starts{}, stops{};
    bool start(TextInputSessionStamp, const TextInputProperties&) noexcept override { ++starts; return true; }
    bool stop() noexcept override { ++stops; return true; }
    bool cancel() noexcept override { return true; }
    bool set_area(const WindowTextInputArea&) noexcept override { return true; }
    ClipboardReadResult read_text() override { return {ClipboardError::no_text, {}}; }
    ClipboardError write_text(StringView) override { return ClipboardError::none; }
    ClipboardAvailability has_text() const noexcept override { return {}; }
};
struct Fixture {
    runtime::NodeStore nodes;
    runtime::FrameRequestState frames;
    runtime::DirtyQueues dirty{nodes, &frames};
    layout::LayoutEngine layout{nodes};
    std::unique_ptr<font::FontRuntime> fonts = std::move(font::FontRuntime::create().runtime);
    text::TextEngine engine{*fonts};
    detail::TextSceneService scene{*fonts, engine, frames};
    float font_scale{1.0F};
    std::map<std::uint32_t, std::vector<font::FontIdentity>> chains;
    std::vector<font::FontIdentity> resolve(std::uint32_t pixels) {
        if(auto found = chains.find(pixels); found != chains.end()) return found->second;
        const auto latin = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, font::FontRasterConfig{pixels, font_scale});
        const auto cjk = fonts->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, font::FontRasterConfig{pixels, font_scale});
        if(!latin || !cjk) throw std::runtime_error("Input validation fonts failed to load");
        auto chain = std::vector<font::FontIdentity>{latin.font, cjk.font};
        chains.emplace(pixels, chain); return chain;
    }
    detail::ButtonComponentHost buttons{nodes, layout, dirty, scene,
        [this](SystemFontFamily, std::uint32_t, std::uint32_t pixels) { return resolve(pixels); }, frames};
    Platform platform;
    detail::InputComponentHost inputs{buttons, platform, platform};
    void synchronize(float width = 320, runtime::Rect clip = {0, 0, 320, 240}) {
        if(!buttons.layout_and_synchronize({width, 240}, clip)) throw std::runtime_error("layout synchronization failed");
    }
};
}
