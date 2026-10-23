#include <ryn/collapse.hpp>
#include <ryn/rynui.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::CollapseTitle, ryn::CollapseBody>);
static_assert(!std::is_convertible_v<ryn::CollapseExtra, ryn::CollapseTitle>);

int main() {
    ryn::Signal<ryn::CollapseKeys> keys{ryn::CollapseKeys{ryn::String{u8"settings"}}};
    ryn::Signal<bool> disabled{false};
    ryn::CollapseProps props;
    props.activeKeys(keys)
        .accordion(false)
        .size(ryn::ControlSize::Small)
        .layout(ryn::LayoutStyle{}.width(ryn::dp(320)))
        .onChange([](ryn::CollapseKeys) {});
    ryn::CollapsePanel panel{u8"settings", ryn::CollapseTitle{[] { ryn::Text(u8"Settings"); }},
                             ryn::CollapseBody{[] { ryn::Input(ryn::InputProps{}); }}};
    panel.disabled(disabled).extra(
        ryn::CollapseExtra{[] { ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"Save"); }); }});
    bool rejected{};
    try {
        ryn::Collapse(props, {panel});
    } catch (const std::logic_error&) {
        rejected = true;
    }
    return rejected ? 0 : 1;
}
