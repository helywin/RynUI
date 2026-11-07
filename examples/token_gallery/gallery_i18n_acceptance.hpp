#pragma once

#include "gallery_pages_host.hpp"

#include <functional>
#include <string_view>

namespace rynui::example {

struct GalleryI18nAcceptanceHooks final {
    std::function<void()> render;
    std::function<void(ryn::runtime::ComponentId)> click;
    std::function<void(ryn::runtime::ComponentId)> reveal;
    std::function<void(int)> resize;
    std::function<std::uint64_t(std::string_view, ryn::runtime::Rect)> capture;
    std::function<void(bool, const char*)> check;
};

void gallery_suppress_native_errors();

// Invoked only by the bounded SDL hidden/offscreen page-acceptance path.
// A capture callback must return a hash of an actual local GPU readback.
std::uint64_t run_gallery_i18n_acceptance(GalleryPagesHost& pages, ryn::detail::WindowComponentServices& services,
                                          ryn::detail::InputComponentHost& inputs,
                                          ryn::runtime::FrameRequestState& frames,
                                          const GalleryI18nAcceptanceHooks& hooks);

} // namespace rynui::example
