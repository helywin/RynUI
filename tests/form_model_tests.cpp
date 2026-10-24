#include "component/form_model.hpp"

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

void values_validation_reset() {
    auto model = std::make_shared<FormModel>();
    auto text = make_form_field(String{});
    auto boolean = make_form_field(false);
    auto number = make_form_field(std::optional<double>{});
    auto selection = make_form_field(SelectSelection{});
    for (auto field : {text, boolean, number, selection}) {
        configure_form_required(field, String{u8"Required"});
    }
    model->add(String{u8"name"}, text);
    model->add(String{u8"agree"}, boolean);
    model->add(String{u8"count"}, number);
    model->add(String{u8"role"}, selection);
    int failed{};
    int finished{};
    model->failed = [&](FormResult result) {
        ++failed;
        check(result.errors.size() == 4 && !result.valid(), "required result mismatch");
        check(!model->submit() && !model->reset() && !model->validate(), "recursive command accepted");
    };
    check(model->submit() && failed == 1, "required failure did not submit");
    write_form_field(text, String{u8"RynUI"});
    write_form_field(boolean, true);
    write_form_field(number, std::optional<double>{0});
    write_form_field(selection, SelectSelection{SelectValue{u8"native"}});
    check(text->error.get().empty(), "value update retained stale error");
    model->finish = [&](FormResult result) {
        ++finished;
        check(result.valid() && result.values.size() == 4, "success values mismatch");
    };
    check(model->submit() && finished == 1, "success callback missing");
    check(model->reset() && std::get<String>(read_form_field(text)).empty(), "reset failed");
    check(std::get<std::optional<double>>(read_form_field(number)) == std::nullopt, "numeric reset failed");
    bool rejected{};
    try {
        write_form_field(number, std::optional<double>{std::numeric_limits<double>::infinity()});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "nonfinite number accepted");
}

void registration_and_focus() {
    auto model = std::make_shared<FormModel>();
    auto field = make_form_field(String{});
    configure_form_required(field, String{u8"Required"});
    auto generation = model->add(
        String{u8"name"}, field, [] { return false; },
        []() -> bool { throw std::runtime_error("ineligible field focused"); });
    auto old = form_change_handler(field);
    int focus{};
    auto second = make_form_field(false);
    configure_form_required(second, String{u8"Required"});
    model->add(
        String{u8"second"}, second, [] { return true; },
        [&] {
            ++focus;
            return true;
        });
    check(model->submit() && focus == 1, "first eligible invalid focus missing");
    bool rejected{};
    try {
        model->add(String{u8"name"}, make_form_field(true));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "duplicate field name accepted");
    model->remove(field, generation);
    check(field->error.get().empty() && model->validate()->values.size() == 1, "removed field leaked errors/value");
    generation = model->add(String{u8"name"}, field);
    old(String{u8"late"});
    check(std::get<String>(read_form_field(field)).empty(), "old callback changed new registration");
    auto current = form_change_handler(field);
    current(String{u8"kept"});
    model->remove(field, generation);
    model->add(String{u8"name"}, field);
    check(std::get<String>(read_form_field(field)) == String{u8"kept"}, "remount lost retained value");
    model->dispose();
    current(String{u8"late"});
    check(!model->submit() && !model->reset() && !model->validate(), "disposed model accepted command");
}

void controlled_and_reentrancy() {
    Signal<String> authority{String{u8"external"}};
    auto field = make_form_field(String{u8"initial"}, bind([=] { return FormValue{authority.get()}; }),
                                 [=](FormValue value) { authority.set(std::get<String>(value)); });
    auto model = std::make_shared<FormModel>();
    model->add(String{u8"name"}, field);
    check(model->reset() && authority.get() == String{u8"initial"}, "controlled reset request missing");
    auto no_echo =
        make_form_field(String{u8"ignored"}, bind([=] { return FormValue{authority.get()}; }), [](FormValue) {});
    model->add(String{u8"no-echo"}, no_echo);
    check(model->reset() && std::get<String>(read_form_field(no_echo)) == String{u8"initial"},
          "reset forged controlled authority");
    field->validator = [&](const FormValue&) {
        authority.set(String{u8"changed in validator"});
        return String{};
    };
    int failures{};
    model->failed = [&](FormResult result) {
        check(result.out_of_date, "changed transaction accepted");
        ++failures;
    };
    model->finish = [](FormResult) {
        throw std::runtime_error("stale success emitted");
    };
    check(model->submit() && failures == 1 && field->error.get().empty(), "stale validation published errors");
    field->validator = [&](const FormValue&) {
        model->dispose();
        return String{u8"late"};
    };
    check(!model->submit() && failures == 1 && field->error.get().empty(), "destroyed callback leaked");
    auto throwing = std::make_shared<FormModel>();
    auto item = make_form_field(false);
    item->validator = [](const FormValue&) -> String {
        throw std::runtime_error("validator");
    };
    throwing->add(String{u8"throw"}, item);
    try {
        static_cast<void>(throwing->validate());
    } catch (const std::runtime_error&) {
    }
    check(!throwing->busy && item->error.get().empty(), "exception left partial validation/busy");
}

void error_observer_invalidates_publication() {
    auto model = std::make_shared<FormModel>();
    auto text = make_form_field(String{u8"name"});
    auto confirmed = make_form_field(false);
    configure_form_validator(text, [confirmed](const FormValue&) {
        return std::get<bool>(read_form_field(confirmed)) ? String{} : String{u8"Confirm first"};
    });
    model->add(String{u8"name"}, text);
    model->add(String{u8"confirmed"}, confirmed);
    Scope observers;
    auto observer = effect(observers, [&] {
        const auto error = text->error.get();
        if (!error.empty()) {
            write_form_field(confirmed, true);
        }
    });
    const auto result = model->validate();
    check(result && result->out_of_date && std::get<bool>(read_form_field(confirmed)) && text->error.get().empty(),
          "error observer invalidated validation but left stale errors published");
    check(model->validate()->valid(), "validation did not recover after error observer changed a value");
    observers.dispose();
    model->dispose();
    write_form_field(confirmed, false);
    auto focus_model = std::make_shared<FormModel>();
    focus_model->add(
        String{u8"name"}, text, [] { return true; },
        [confirmed] {
            write_form_field(confirmed, true);
            return true;
        });
    focus_model->add(String{u8"confirmed"}, confirmed);
    int failures{};
    focus_model->failed = [&](FormResult result) {
        check(result.out_of_date, "focus mutation accepted old errors");
        ++failures;
    };
    check(focus_model->submit() && failures == 1 && text->error.get().empty(),
          "focus callback invalidated validation but left stale errors published");
}
} // namespace

int main() {
    try {
        values_validation_reset();
        registration_and_focus();
        controlled_and_reentrancy();
        error_observer_invalidates_publication();
        std::cout << "Form model tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
