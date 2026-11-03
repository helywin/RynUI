#pragma once

#include <span>
#include <string_view>

namespace rynui::example {

// Frozen migration inventory. A caption or token belongs to its named sample;
// interactive tests additionally resolve the component through target(identity).
struct GalleryLegacyTestTarget final {
    std::string_view identity;
    std::string_view sample_identity;
    std::string_view replacement;
};

[[nodiscard]] std::span<const GalleryLegacyTestTarget> gallery_legacy_test_targets() noexcept;
[[nodiscard]] std::span<const std::string_view> gallery_legacy_test_ids() noexcept;

} // namespace rynui::example
