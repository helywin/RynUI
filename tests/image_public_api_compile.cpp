#include <ryn/image.hpp>
#include <type_traits>

static_assert(std::is_constructible_v<ryn::ImageSource, std::shared_ptr<const ryn::ImagePixels>>);
static_assert(std::is_constructible_v<ryn::ImageSource, std::shared_ptr<const ryn::EncodedImage>>);
static_assert(std::is_constructible_v<ryn::ImageSource, std::shared_ptr<const ryn::ImageLoader>>);

void picture(std::shared_ptr<const ryn::EncodedImage> bytes) {
    ryn::Image(ryn::ImageProps{}
                   .source(ryn::ImageSource{std::move(bytes)})
                   .alt(u8"Inspection photograph")
                   .fit(ryn::ImageFit::Contain)
                   .layout(ryn::LayoutStyle{}.width(ryn::dp(240)).height(ryn::dp(160))));
}
