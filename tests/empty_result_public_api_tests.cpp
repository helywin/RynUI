#include <ryn/empty.hpp>
#include <ryn/result.hpp>
#include <ryn/theme.hpp>
#include <type_traits>

using namespace ryn;
static_assert(!std::is_convertible_v<EmptyExtra, ResultExtra>);
static_assert(!std::is_convertible_v<ResultIcon, ResultContent>);
static_assert(std::is_same_v<decltype(Empty()), void>);
static_assert(std::is_same_v<decltype(Result()), void>);
static_assert(std::is_same_v<decltype(EmptyProps{}.description(String{}).image(EmptyImage::Compact)), EmptyProps&>);
static_assert(
    std::is_same_v<decltype(ResultProps{}.status(Status::Success).status(ResultException::NotFound)), ResultProps&>);
static_assert(std::is_same_v<decltype(ResultProps{}.title(u8"Done").subtitle(u8"Ready")), ResultProps&>);
static_assert(std::is_same_v<ResultStatus, std::variant<Status, ResultException>>);

int main() {
    Signal<String> label{String{u8"Reactive"}};
    Signal<EmptyImage> image{EmptyImage::Default};
    Signal<ResultStatus> status{ResultStatus{Status::Success}};
    auto empty = EmptyProps{}.description(label).image(image).layout(LayoutStyle{}.width(dp(300)));
    auto result = ResultProps{}.status(status).title(label).subtitle(bind([label] { return label.get(); }));
    EmptySlots empty_slots{.image = EmptyImageContent{[] {}}, .extra = EmptyExtra{[] {}}};
    ResultSlots result_slots{.icon = ResultIcon{[] {}}, .content = ResultContent{[] {}}, .extra = ResultExtra{[] {}}};
    ThemeConfig config;
    config.empty.tokens.image_size = dp(80);
    config.result.tokens.title_font_size = dp(28);
    (void)empty;
    (void)result;
    (void)empty_slots;
    (void)result_slots;
    return resolve_theme(config).result().metrics.title_font_size == 28 ? 0 : 1;
}
