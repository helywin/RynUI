#pragma once
#include "gallery_pages_host.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/selection_component.hpp"
#include <functional>

namespace rynui::example {
struct ExistingDataEntryActions final {
    ryn::detail::ButtonComponentHost& app;
    ryn::detail::InputComponentHost& inputs;
    GalleryPagesHost& pages;
    ryn::detail::SelectionComponentHost& selection;
    std::function<void(bool, const char*)> check;
    std::function<void()> render;
    std::function<void(ryn::runtime::ComponentId)> reveal;
    std::function<void(ryn::runtime::ComponentId)> activate;
    std::function<void(ryn::runtime::ComponentId)> click;
    std::function<void(ryn::input::PointerAction, ryn::runtime::Point)> pointer;
    std::function<void(ryn::input::Key)> press;
};

void accept_existing_data_entry(const GalleryPageRoute&, ExistingDataEntryActions);
} // namespace rynui::example
