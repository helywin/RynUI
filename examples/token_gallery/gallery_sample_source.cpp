#include "gallery_sample_source.hpp"

#include <algorithm>
#include <array>

namespace rynui::example {
namespace {
#include "generated_gallery_source.inc"
} // namespace

std::span<const GallerySampleSource> gallery_sample_sources() noexcept {
    return sample_sources;
}

const GallerySampleSource* gallery_sample_source(std::string_view identity) noexcept {
    const auto found = std::ranges::find(sample_sources, identity, &GallerySampleSource::identity);
    return found == sample_sources.end() ? nullptr : &*found;
}

} // namespace rynui::example
