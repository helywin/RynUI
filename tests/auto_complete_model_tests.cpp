#include "component/auto_complete_model.hpp"
#include <iostream>
#include <stdexcept>

using namespace ryn;
using namespace ryn::detail;

namespace {
void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(what);
    }
}

void run() {
    std::vector<AutoCompleteOption> options{
        {u8"a", u8"alpha", u8"Alpha"}, {u8"b", u8"beta", u8"Beta", true}, {u8"c", u8"gamma", u8"Gamma"}};
    AutoCompleteModel model{options};
    model.edit(u8"free text");
    check(model.value() == String{u8"free text"} && model.filtered().size() == 3, "free input/default no filter");
    model.move(1);
    check(model.active() == 2, "disabled item not skipped");
    model.backfill();
    check(model.display() == String{u8"gamma"} && model.value() == String{u8"free text"}, "backfill committed");
    model.restore();
    check(model.display() == String{u8"free text"}, "backfill cancellation");
    model.configure(AutoCompleteFilter{true}, true);
    model.edit(u8"alp");
    check(model.filtered().size() == 1 && model.active() == 0, "value filtering");
    std::swap(options[0], options[2]);
    model.set_options(options);
    check(model.active() == 2, "active key not retained across reorder");
    AutoCompleteModel controlled{options, u8"owner", true};
    controlled.edit(u8"intent");
    check(controlled.value() == String{u8"owner"} && controlled.query() == String{u8"intent"},
          "controlled input authority");
    controlled.request(u8"gamma");
    check(controlled.display() == String{u8"owner"}, "controlled selection authority");
    controlled.reconcile(u8"accepted");
    check(controlled.query() == String{u8"accepted"}, "controlled echo");
    auto invalid = options;
    invalid[1].key = invalid[0].key;
    bool rejected = false;
    try {
        controlled.set_options(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && controlled.options() == options, "duplicate key update not atomic");
    controlled.configure(AutoCompleteFilter{[](StringView q, const AutoCompleteOption& o) {
                             return q.empty() || o.key == String{u8"a"};
                         }},
                         false);
    check(!controlled.active() && controlled.filtered().size() == 1, "custom predicate/active first");
    controlled.move(1);
    check(controlled.active() == 2, "inactive first navigation");
    auto throw_filter = AutoCompleteFilter{
        [](StringView, const AutoCompleteOption&) -> bool { throw std::invalid_argument("predicate"); }};
    rejected = false;
    try {
        controlled.configure(throw_filter, true);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && controlled.filtered().size() == 1, "predicate failure changed model");
}
} // namespace

int main() {
    try {
        run();
        std::cout << "AutoComplete model passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
