#include <ryn/rynui.hpp>
#include <type_traits>

using namespace ryn;

static_assert(requires(Signal<String> title, Signal<CardSize> size, Signal<CardVariant> variant, Signal<bool> loading,
                       CardContent body) {
    Card(CardProps{}.title(u8"Card").size(CardSize::Small).variant(CardVariant::Borderless).hoverable().loading(),
         body);
    Card(CardProps{}.title(title).size(size).variant(variant).loading(loading).layout(LayoutStyle{}.width(dp(320))),
         body,
         CardSlots{.title = CardTitle{[] {}},
                   .extra = CardExtra{[] {}},
                   .cover = CardCover{[] {}},
                   .actions = CardActions{[] {}}});
});
static_assert(!std::is_convertible_v<CardTitle, CardContent>);
static_assert(!std::is_convertible_v<CardExtra, CardTitle>);
static_assert(!std::is_convertible_v<CardCover, CardActions>);

int main() {
    return 0;
}
