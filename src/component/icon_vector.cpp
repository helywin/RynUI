#include <ryn/icon.hpp>
#include "font/icon_vector_font.hpp"
#include "icons/icon_vector_data.hpp"

#include <utility>

namespace ryn {
IconVector::IconVector(IconViewBox view_box, std::vector<IconPath> paths) {
    auto bytes = font::build_icon_vector_font(view_box, paths);
    auto data = std::make_shared<detail::IconVectorData>();
    data->view_box = view_box;
    data->paths = std::move(paths);
    data->font_bytes = std::move(bytes);
    data->layers.reserve(data->paths.size());
    for (std::size_t index = 0; index < data->paths.size(); ++index) {
        const auto& path = data->paths[index];
        data->layers.push_back(
            {static_cast<char32_t>(0xE000 + index), path.color == IconColorRole::Secondary, path.opacity});
    }
    data_ = std::move(data);
}

IconViewBox IconVector::view_box() const noexcept {
    return data_ ? data_->view_box : IconViewBox{};
}

std::span<const IconPath> IconVector::paths() const noexcept {
    return data_ ? std::span<const IconPath>{data_->paths} : std::span<const IconPath>{};
}
} // namespace ryn
