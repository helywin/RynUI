#pragma once

#include "icons/bundled_icon_catalog.hpp"
#include <ryn/icon.hpp>
#include <cstddef>
#include <memory>
#include <vector>

namespace ryn::detail {
struct IconVectorData final {
    IconViewBox view_box;
    std::vector<IconPath> paths;
    std::vector<BundledIconLayer> layers;
    std::vector<std::byte> font_bytes;
};

struct IconSourceAccess final {
    static const std::shared_ptr<const IconVectorData>& vector(const IconSource& source) noexcept {
        return source.vector_;
    }
};
} // namespace ryn::detail
