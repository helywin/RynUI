#include "component/otp_model.hpp"

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

template <class F> void rejects(F&& operation) {
    bool threw = false;
    try {
        operation();
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "invalid operation accepted");
}

void unicode_and_capacity() {
    const String input{u8"e\u0301\r\n中👨‍👩‍👧‍👦xy"};
    OTPModel model{3, input.bytes()};
    const auto cells = model.projection();
    check(cells.size() == 3 && cells[0] == String{u8"e\u0301"} && cells[1] == String{u8"中"} &&
              cells[2] == String{u8"👨‍👩‍👧‍👦"},
          "grapheme or newline split");
    check(model.set_length(5) && model.value() == String{u8"e\u0301中👨‍👩‍👧‍👦xy"},
          "hidden suffix lost");
    model.set_length(3);
    auto candidate = model.prepare(1, String{u8"你🙂ab"}.bytes());
    check(candidate && candidate->next_index == 2 && candidate->cells[2] == String{u8"🙂"}, "paste capacity");
    check(model.commit(std::move(*candidate)), "candidate commit");
    model.set_length(5);
    check(model.projection()[3].empty(), "user edit retained hidden suffix");
    const auto revision = model.revision();
    rejects([&] { model.reconcile(std::string_view{"\xC0\xAF", 2}); });
    rejects([&] { static_cast<void>(model.prepare(0, std::string_view{"\xF0\x9F", 2})); });
    rejects([&] { model.set_length(0); });
    rejects([&] { model.set_length(1025); });
    check(revision == model.revision(), "invalid operation modified model");
    rejects([] { OTPModel invalid{0}; });
    OTPModel limit{1024, std::string(2048, 'x')};
    check(limit.value().size_bytes() == 1024, "external maximum capacity");
}

void edits_and_echo() {
    OTPModel model{4, "abcd"};
    auto single = model.prepare(1, "x");
    check(single && single->cells[2] == String{u8"c"} && single->complete_changed, "single replaced tail");
    check(model.commit(*single), "single commit");
    const auto revision = model.revision();
    check(!model.reconcile("axcd") && model.revision() == revision, "equal controlled echo revised model");
    check(!model.commit(*single), "stale candidate committed twice");
    const auto equal = model.prepare(0, "a");
    check(equal && !equal->complete_changed, "equal edit completed again");
    auto tail = model.prepare(0, "ay");
    check(tail && tail->cells[0] == String{u8"a"} && tail->cells[1] == String{u8"y"} && tail->cells[2].empty() &&
              !tail->complete_changed,
          "same local character lost tail edit");
    check(model.commit(*tail), "tail commit");
    check(model.first_empty() == 2, "first empty");
    auto hole = model.prepare(0, "");
    check(hole && hole->cells[0].empty() && hole->cells[1] == String{u8"y"}, "delete shifted cells");
    model.commit(*hole);
    const auto before_echo = model.revision();
    check(!model.reconcile("y") && model.first_empty() == 0 && model.revision() == before_echo,
          "controlled flattened echo collapsed holes");
    OTPModel other{4};
    check(!other.commit(*hole), "foreign candidate accepted");
    auto pending = model.prepare(2, "z");
    model.reconcile("new");
    check(pending && !model.commit(*pending), "authoritative change accepted stale edit");
    pending = model.prepare(0, "a");
    model.retire();
    check(pending && !model.commit(*pending) && !model.prepare(0, "a"), "retired edit accepted");
}

void formatting() {
    bool fail = false;
    int calls = 0;
    String received;
    OTPModel model{4, "ab", [&](String text) {
                       ++calls;
                       received = text;
                       if (fail) {
                           throw std::runtime_error("formatter failure");
                       }
                       return text;
                   }};
    check(calls == 1, "initial formatter missing");
    auto candidate = model.prepare(3, "x");
    check(candidate && received == String{u8"ab x"} && candidate->cells[2].empty(), "formatter holes");
    model.commit(*candidate);
    const auto revision = model.revision();
    fail = true;
    rejects([&] { static_cast<void>(model.prepare(1, "c")); });
    check(model.revision() == revision && model.value() == String{u8"abx"}, "throw changed published model");
    model.reconcile("z");
    check(calls == 3 && model.value() == String{u8"z"}, "authoritative invoked formatter");
    OTPModel* pointer = nullptr;
    OTPModel reentrant{3, "", [&](String text) {
                           if (pointer) {
                               pointer->set_length(8);
                               pointer->reconcile("external");
                           }
                           return text;
                       }};
    pointer = &reentrant;
    check(!reentrant.prepare(0, "a") && reentrant.value() == String{u8"external"}, "formatter reentry overwritten");
    OTPModel* retiring = nullptr;
    OTPModel retired{3, "", [&](String text) {
                         if (retiring) {
                             retiring->retire();
                         }
                         return text;
                     }};
    retiring = &retired;
    check(!retired.prepare(0, "a"), "retiring formatter candidate survived");
}
} // namespace

int main() {
    try {
        unicode_and_capacity();
        edits_and_echo();
        formatting();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
