#include "component/popover_model.hpp"
#include <iostream>

namespace {
void check(bool value) {
    if (!value) {
        throw std::runtime_error("Popover model contract failed");
    }
}

void contract() {
    using namespace ryn;
    using namespace ryn::detail;
    using Time = animation::AnimationTime;
    const auto delay = Duration::milliseconds(100);
    PopoverModel model{false, false};
    model.observe(true, false, Time{}, delay, delay);
    check(!model.due(Time::microseconds(99999)) && model.due(Time::microseconds(100000)) == true);
    check(model.request(true) && model.open() && !model.request(true));
    model.observe(false, false, Time::microseconds(200000), delay, delay);
    model.observe(true, false, Time::microseconds(250000), delay, delay);
    check(!model.due(Time::microseconds(300000)));
    model.dismiss();
    check(model.request(false) && !model.open());
    model.observe(true, false, Time::microseconds(400000), delay, delay);
    check(!model.deadline());
    model.observe(false, false, Time::microseconds(500000), delay, delay);
    model.observe(true, true, Time::microseconds(500000), delay, delay);
    check(model.due(Time::microseconds(500000)) == true);
    PopoverModel controlled{true, true};
    check(controlled.request(false) && controlled.open() && !controlled.request(false));
    check(!controlled.suppress() && !controlled.open());
    controlled.reconcile(true);
    check(controlled.open());
    static_cast<void>(controlled.suppress());
    controlled.observe(false, false, Time{}, delay, delay);
    controlled.observe(true, false, Time{}, delay, delay);
    check(controlled.due(Time::microseconds(100000)) == true);
    check(controlled.request(true) && controlled.open());
    check(popover_triggers(PopoverTriggerMode::HoverFocus) == PopoverTriggers{true, true, false});
    bool rejected{};
    try {
        validate_popover_delay(Duration::milliseconds(86401000));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected);
}
} // namespace

int main() {
    try {
        contract();
    } catch (const std::exception& e) {
        std::cerr << e.what();
        return 1;
    }
}
