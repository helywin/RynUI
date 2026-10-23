#include "component/input_number_model.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F action) {
    bool rejected{};
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid InputNumber value/config accepted");
}

void arithmetic_and_drafts() {
    InputNumberModel model{{-10, 10, .1, {}}, .1, false};
    for (int i = 0; i < 9; ++i) {
        model.accept(model.stepped(InputNumberStepDirection::Up));
    }
    check(model.value() == 1.0 && InputNumberModel::format(model.value()) == String{u8"1"}, "decimal step drifted");
    model.reconcile(.2);
    check(model.stepped(InputNumberStepDirection::Up) == .3, "0.2 + 0.1 drifted");
    model.reconcile(-.2);
    check(model.stepped(InputNumberStepDirection::Down) == -.3, "negative step drifted");
    model.reconcile({});
    check(model.stepped(InputNumberStepDirection::Down) == -.1, "empty step did not start at zero");
    for (const auto draft : {"-", "+", ".", "-.", "1.", "1e", "1x", "nan", "1e3", " 1"}) {
        check(!model.parse(draft), "partial/invalid draft published");
    }
    check(model.parse("") && !*model.parse(""), "empty draft is not nullopt");
    check(model.parse("-.25") == std::optional<InputNumberValue>{-.25}, "signed fractional draft rejected");
    check(model.parse("1.", true) == std::optional<InputNumberValue>{1.0}, "tail decimal did not finish");
    check(!model.parse("11") && model.parse("11", true) == std::optional<InputNumberValue>{10.0},
          "range commit failed");
    model.reconcile(20);
    check(model.stepped(InputNumberStepDirection::Up) == 20 && model.stepped(InputNumberStepDirection::Down) == 10,
          "controlled out-of-range direction failed");
}

void precision_and_atomicity() {
    InputNumberModel model{{.15, .95, .1, 1}, .2, false};
    check(model.parse(".15", true) == std::optional<InputNumberValue>{.2} &&
              model.parse(".95", true) == std::optional<InputNumberValue>{.9},
          "precision rounding escaped range");
    model.configure({-1, 1, .01, 2});
    check(model.parse("-.125") == std::optional<InputNumberValue>{-.13}, "negative half rounding failed");
    const auto before = model.options();
    rejects([&] { model.configure({1, -1, 1, {}}); });
    rejects([&] { model.configure({0, 1, 0, {}}); });
    rejects([&] { model.configure({0, 1, -1, {}}); });
    rejects([&] { model.configure({0, 1, 1, 13}); });
    rejects([&] { model.configure({.11, .19, .1, 1}); });
    rejects([&] { model.reconcile(std::numeric_limits<double>::quiet_NaN()); });
    rejects([&] { model.reconcile(std::numeric_limits<double>::infinity()); });
    rejects([&] { model.reconcile(1e-13); });
    rejects([&] { model.reconcile(9007199254740992.0); });
    check(model.options() == before && model.value() == .2, "invalid configuration partially applied");
    model.configure({-9007199254740991.0, 9007199254740991.0, 1, {}});
    model.reconcile(9007199254740991.0);
    check(model.stepped(InputNumberStepDirection::Up) == model.value() &&
              model.stepped(InputNumberStepDirection::Down) == 9007199254740990.0,
          "large integer arithmetic overflowed");
    InputNumberModel controlled{{}, 2, true};
    controlled.accept(3);
    check(controlled.value() == 2, "controlled model accepted refusal");
    controlled.reconcile(3);
    check(controlled.value() == 3, "controlled writeback failed");
}
} // namespace

int main() {
    try {
        arithmetic_and_drafts();
        precision_and_atomicity();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
