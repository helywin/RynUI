#include "component/select_model.hpp"
#include "input/text_boundary.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using ryn::detail::SelectModel;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void selection_and_reconciliation() {
    SelectModel model{{{u8"1", u8"String one"}, {1, u8"Integer one"}, {2, u8"Unavailable", true}}};
    check(!model.value() && model.label().empty(), "initial empty selection");
    auto intent = model.request(1);
    check(intent && *intent == SelectSelection{1} && model.value() == *intent &&
              model.label() == String{u8"Integer one"},
          "integer selection");
    check(!model.request(2) && !model.request(100), "invalid or disabled selection");
    check(model.value() == SelectSelection{1}, "rejected selection changed value");
    intent = model.request(0);
    check(intent && *intent == SelectSelection{u8"1"} && model.label() == String{u8"String one"},
          "typed string and integer conflated");
    intent = model.request({});
    check(intent && !*intent && !model.value(), "clear intent confused with rejection");
    const auto unknown = std::numeric_limits<std::int64_t>::min();
    check(model.reconcile(unknown) && model.label() == String{u8"-9223372036854775808"}, "unknown integer lost");
    check(!model.reconcile(unknown), "equal value revised");
    model.set_options({{unknown, u8"Loaded later"}});
    check(model.value() == SelectSelection{unknown} && model.label() == String{u8"Loaded later"},
          "dynamic options did not resolve unknown label");
    model.set_options({});
    check(model.label() == String{u8"-9223372036854775808"}, "removed option lost value");
    model.reconcile(SelectValue{u8"external"});
    check(model.label() == String{u8"external"}, "unknown string display");
    SelectModel controlled{{{1, u8"One"}, {2, u8"Two"}}, 1, true};
    intent = controlled.request(1);
    check(intent && *intent == SelectSelection{2} && controlled.value() == SelectSelection{1} &&
              controlled.label() == String{u8"One"},
          "controlled refusal changed display");
    controlled.reconcile(*intent);
    check(controlled.label() == String{u8"Two"}, "controlled echo missing");
    intent = controlled.request({});
    check(intent && !*intent && controlled.value() == SelectSelection{2}, "controlled clear committed implicitly");
}

void unicode_search_and_navigation() {
    SelectModel model{{{0, u8"Straße"}, {1, u8"CAFÉ"}, {2, u8"中文🙂"}, {3, u8"Σίσυφος"}, {4, u8"Disabled", true}}};
    check(model.search(u8"STRASSE") && model.filtered().size() == 1 && model.filtered()[0] == 0, "full case folding");
    model.search(u8"cafe\u0301");
    check(model.filtered().size() == 1 && model.filtered()[0] == 1, "canonical normalization");
    model.search(u8"文🙂");
    check(model.filtered().size() == 1 && model.filtered()[0] == 2 && !model.value(), "CJK search selected value");
    model.search(u8"ΣΊΣΥΦΟΣ");
    check(model.filtered().size() == 1 && model.filtered()[0] == 3, "Greek case folding");
    model.search(u8"no matches");
    check(model.filtered().empty() && !model.active() && !model.move(1) && !model.boundary(true),
          "empty result has activity");
    model.search({});
    check(model.boundary(true) && model.active() == 3 && model.move(1) && model.active() == 0,
          "navigation did not skip disabled or wrap");
    check(model.move(-1) && model.active() == 3 && model.boundary(false) && model.active() == 0,
          "reverse or boundary navigation");
    check(!model.activate(4) && !model.move(0), "disabled or zero navigation accepted");
    model.search(u8"disabled");
    check(!model.active() && !model.move(1), "all disabled activated");
    model.search({});
    model.reconcile(2);
    check(model.active() == 2, "selection not used as keyboard starting point");
    const String nul{u8"A\0B"};
    model.set_options({{0, nul}});
    model.search(String{u8"\0b"});
    check(model.filtered().size() == 1, "embedded NUL truncated");
    check(input::text_search_key("").empty(), "empty normalization");
    bool invalid = false;
    try {
        static_cast<void>(input::text_search_key(std::string_view{"\xC0\xAF", 2}));
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    check(invalid, "invalid Unicode accepted");
}

void atomic_options() {
    SelectModel model{{{1, u8"First"}, {u8"1", u8"Other type"}}, 1};
    model.search(u8"First");
    const auto check_rejection = [&](std::vector<SelectOption> options) {
        bool rejected = false;
        try {
            model.set_options(std::move(options));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && model.options().size() == 2 && model.label() == String{u8"First"} &&
                  model.filtered().size() == 1 && model.active() == 0 && model.query() == String{u8"First"},
              "duplicate option update published partial state");
    };
    check_rejection({{2, u8"New"}, {2, u8"Duplicate"}});
    check_rejection({{u8"a", u8"New"}, {u8"a", u8"Duplicate", true}});
    check(!model.set_options(model.options()), "equal options rebuilt");
    model.set_options({{1, u8"Changed"}, {2, u8"First result"}});
    check(model.value() == SelectSelection{1} && model.label() == String{u8"Changed"} && model.active() == 1,
          "dynamic option search or selected value drift");
}
} // namespace

int main() {
    try {
        selection_and_reconciliation();
        unicode_search_and_navigation();
        atomic_options();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
