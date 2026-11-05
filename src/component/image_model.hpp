#pragma once
#include <ryn/image.hpp>
#include "runtime/geometry.hpp"

namespace ryn::detail {
void validate_image_preview(ImagePreviewOptions);
[[nodiscard]] runtime::Rect fit_image(runtime::Rect, runtime::Size, ImageFit);
[[nodiscard]] ImageTransform clamp_image_transform(ImageTransform, runtime::Size image, runtime::Size viewport,
                                                   ImagePreviewOptions);
[[nodiscard]] ImageTransform image_transform_action(ImageTransform, ImageAction, runtime::Size image,
                                                    runtime::Size viewport, ImagePreviewOptions);
} // namespace ryn::detail
