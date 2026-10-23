#include <ryn/alert.hpp>
#include <ryn/spin.hpp>
#include <ryn/progress.hpp>
#include <ryn/tag.hpp>
#include <ryn/theme.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::AlertIcon, ryn::AlertAction>);
static_assert(!std::is_convertible_v<ryn::SpinContent, ryn::SpinIndicator>);
static_assert(std::is_same_v<decltype(ryn::AlertProps{}.status(ryn::Status::Success)), ryn::AlertProps&>);

int main() {
    ryn::Signal<float> percent{25};
    ryn::Signal<bool> loading{true};
    ryn::Signal<ryn::Status> status{ryn::Status::Warning};
    auto alert = ryn::AlertProps{}
                     .status(status)
                     .message(u8"Warning")
                     .description(u8"Details")
                     .showIcon()
                     .closable()
                     .visible(loading)
                     .onClose([] {});
    auto spin = ryn::SpinProps{}.spinning(loading).description(u8"Loading").size(ryn::ControlSize::Small);
    auto progress = ryn::ProgressProps{}
                        .percent(percent)
                        .type(ryn::ProgressType::Dashboard)
                        .status(ryn::ProgressStatus::Exception)
                        .showInfo(false)
                        .size(ryn::ControlSize::Large);
    static_cast<void>(alert);
    static_cast<void>(spin);
    static_cast<void>(progress);
}
