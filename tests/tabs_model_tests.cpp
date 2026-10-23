#include "component/tabs_model.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void selection_and_navigation() {
    TabsModel model{{{u8"disabled", true}, {u8"one"}, {u8"two", true}, {u8"three"}}};
    check(model.value() == String{u8"one"} && model.active() == 1 && !model.controlled(),
          "Tabs initial enabled selection");
    check(model.first_enabled() == 1 && model.last_enabled() == 3 && model.next_enabled(1, 1) == 3 &&
              model.next_enabled(3, 1) == 1 && model.next_enabled(1, -1) == 3 && model.next_enabled({}, 1) == 1 &&
              model.next_enabled({}, -1) == 3,
          "Tabs navigation did not wrap/skip disabled");
    check(!model.request(2) && !model.request(99) && !model.request(1), "Tabs emitted invalid/repeated change");
    check(model.request(3) == String{u8"three"} && model.value() == String{u8"three"} && model.active() == 3,
          "Tabs uncontrolled request did not commit");
    model.set_disabled(3, true);
    check(model.active() == 3 && model.value() == String{u8"three"} && !model.request(3) &&
              model.next_enabled(3, 1) == 1,
          "Tabs disabled update implicitly changed selection");
    model.set_disabled(1, true);
    check(!model.first_enabled() && !model.last_enabled() && !model.next_enabled(3, 1),
          "all-disabled Tabs remained navigable");
    model.set_disabled(2, false);
    check(model.next_enabled(3, -1) == 2 && model.request(2) == String{u8"two"},
          "Tabs dynamic enabled state not applied");
}

void controlled_and_unknown_values() {
    TabsModel model{{{u8"one"}, {u8"two"}, {u8"disabled", true}}, String{u8"one"}};
    check(model.controlled() && model.request(1) == String{u8"two"} && model.value() == String{u8"one"},
          "Tabs controlled request ignored refusal");
    model.reconcile(String{u8"two"});
    check(model.active() == 1 && !model.request(1), "Tabs controlled echo failed");
    model.reconcile(String{u8"unknown"});
    check(!model.active() && model.value() == String{u8"unknown"} && model.request(0) == String{u8"one"} &&
              !model.active(),
          "Tabs unknown controlled key was lost");
    model.reconcile(String{u8"disabled"});
    check(model.active() == 2 && !model.request(2), "Tabs explicit disabled active key not authoritative");
    model.reconcile(String{});
    check(!model.active() && model.value().empty(), "Tabs explicit empty active key was defaulted");
    TabsModel unknown_default{{{u8"one"}}, {}, String{u8"later"}};
    check(!unknown_default.active() && unknown_default.request(0) == String{u8"one"},
          "Tabs unknown default key request failed");
    TabsModel empty{{}};
    check(empty.value().empty() && !empty.active() && !empty.first_enabled() && !empty.next_enabled({}, -1),
          "empty Tabs model");
    TabsModel disabled{{{u8"one", true}}};
    check(!disabled.active() && disabled.value().empty(), "Tabs default selected a disabled item");
}

void validation_and_identity() {
    for (const auto& options : {std::vector<TabOption>{{String{}}}, std::vector<TabOption>{{u8"one"}, {u8"one"}}}) {
        bool rejected = false;
        try {
            TabsModel invalid{options};
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "Tabs accepted an empty or duplicate key");
    }
    bool rejected = false;
    try {
        TabsModel invalid{{{u8"one"}}, String{u8"one"}, String{u8"one"}};
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "Tabs accepted controlled/default conflict");
    TabsModel identities{{{u8"A"}, {u8"a"}, {u8"a\0b"}, {u8"中文"}}};
    check(identities.options().size() == 4 && identities.request(2) == String{u8"a\0b"} && identities.active() == 2,
          "Tabs key identity lost case or embedded NUL");
    rejected = false;
    try {
        identities.set_disabled(9, true);
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    check(rejected && identities.active() == 2, "Tabs invalid disabled index partially updated selection");
    rejected = false;
    try {
        static_cast<void>(identities.next_enabled(0, 0));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "Tabs accepted invalid navigation direction");
}
} // namespace

int main() {
    try {
        selection_and_navigation();
        controlled_and_unknown_values();
        validation_and_identity();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
