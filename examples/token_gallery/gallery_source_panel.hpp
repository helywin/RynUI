#pragma once

#include "gallery_sample_source.hpp"

#include <ryn/layout_style.hpp>
#include <ryn/reactive.hpp>
#include <ryn/string.hpp>

#include <functional>

namespace rynui::example {

void gallery_source_panel(const GallerySampleSource& source, ryn::Signal<ryn::LogicalLength> width,
                          std::function<void(const ryn::String&)> copy = {});

} // namespace rynui::example
