#include <ryn/border_beam.hpp>
#include <ryn/affix.hpp>
#include <ryn/watermark.hpp>
#include <ryn/theme.hpp>
#include <concepts>

static_assert(!std::constructible_from<ryn::AffixContent, ryn::WatermarkContent>);
static_assert(!std::constructible_from<ryn::WatermarkContent, ryn::BorderBeamContent>);
static_assert(std::same_as<decltype(&ryn::Affix), void (*)(ryn::AffixProps, ryn::AffixContent)>);
static_assert(std::same_as<decltype(&ryn::Watermark), void (*)(ryn::WatermarkProps, ryn::WatermarkContent)>);
static_assert(std::same_as<decltype(&ryn::BorderBeam), void (*)(ryn::BorderBeamProps, ryn::BorderBeamContent)>);

void decoration_public_api_example(ryn::Signal<ryn::AffixTarget> target, ryn::Signal<ryn::ImageSource> source,
                                   ryn::Signal<float> rotation, ryn::Signal<ryn::Duration> duration) {
    ryn::Affix(ryn::AffixProps{}.target(target).offsetTop(12).onChange([](bool) {}), ryn::AffixContent{[] {}});
    ryn::Watermark(ryn::WatermarkProps{}.image(source).rotate(rotation).content(u8"Native"),
                   ryn::WatermarkContent{[] {}});
    ryn::BorderBeam(ryn::BorderBeamProps{}.duration(duration).direction(ryn::BorderBeamDirection::Counterclockwise),
                    ryn::BorderBeamContent{[] {}});
}
