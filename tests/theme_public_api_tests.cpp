#include <ryn/theme.hpp>

#include <concepts>
#include <type_traits>

namespace {

template <typename Value>
concept ThemeConfigValue = requires(ryn::ThemeProps props, Value value) {
    props.config(value);
};

static_assert(ThemeConfigValue<ryn::ThemeConfig>);
static_assert(ThemeConfigValue<ryn::Prop<ryn::ThemeConfig>>);
static_assert(ThemeConfigValue<ryn::Signal<ryn::ThemeConfig>>);
static_assert(!ThemeConfigValue<const char*>);
static_assert(!ThemeConfigValue<int>);
static_assert(!std::constructible_from<ryn::ThemeContent, ryn::Content>);
static_assert(!std::constructible_from<ryn::Content, ryn::ThemeContent>);
static_assert(std::is_same_v<decltype(&ryn::Theme),
    void (*)(ryn::ThemeProps, ryn::ThemeContent)>);

} // namespace

int main() {
    ryn::ThemeConfig config;
    config.algorithms = {ryn::ThemeAlgorithm::Dark, ryn::ThemeAlgorithm::Compact};
    ryn::ThemeProps props;
    props.config(config);
    ryn::ThemeContent content{[] {}};
    static_cast<void>(props);
    static_cast<void>(content);
    return 0;
}
