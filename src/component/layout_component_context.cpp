#include "component/layout_component_context.hpp"

#include <stdexcept>

namespace ryn::detail {
namespace {

thread_local LayoutComponentServices* active_layout_services = nullptr;

} // namespace

ActiveLayoutComponentServices::ActiveLayoutComponentServices(
    LayoutComponentServices& services) noexcept
    : previous_(active_layout_services) {
    active_layout_services = &services;
}

ActiveLayoutComponentServices::~ActiveLayoutComponentServices() {
    active_layout_services = previous_;
}

LayoutComponentServices& require_layout_component_services() {
    if (active_layout_services == nullptr) {
        throw std::logic_error(
            "A layout component can only be declared inside an active component Host");
    }
    return *active_layout_services;
}

} // namespace ryn::detail
