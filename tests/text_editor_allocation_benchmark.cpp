#include "input/text_editor.hpp"
#include "support/allocation_probe.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace ryn::input;
namespace {
void check(bool value, const char* message) { if(!value) { throw std::runtime_error(message); } }
void cycle(TextEditorState& state) {
    check(static_cast<bool>(state.select_all()), "select");
    check(static_cast<bool>(state.replace_selection("alpha_beta  text")), "replace");
    check(static_cast<bool>(state.move(TextCaretMove::home)), "home");
    check(static_cast<bool>(state.move(TextCaretMove::right, true)), "shift");
    check(static_cast<bool>(state.replace_selection("A")), "selection replace");
    check(static_cast<bool>(state.select_word(3)), "word");
    check(static_cast<bool>(state.move(TextCaretMove::end)), "end");
    check(static_cast<bool>(state.erase_backward()), "backspace");
    check(static_cast<bool>(state.replace_selection("t")), "insert");
}
}
int main() {
    try {
        TextEditorStore store;
        store.reserve(1);
        const auto owner = store.create("alpha_beta  text");
        auto& state = store.require(owner);
        state.reserve(256);
        for(int i = 0; i < 10; ++i) { cycle(state); }
        const auto capacity = state.retained_capacity();
        const auto owner_capacity = store.capacity();
        const auto start = std::chrono::steady_clock::now();
        ryn_test::allocation::begin();
        for(int i = 0; i < 10000; ++i) { cycle(state); }
        const auto allocations = ryn_test::allocation::end();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        check(allocations == 0, "editor hot path allocation");
        check(state.retained_capacity() == capacity && store.capacity() == owner_capacity, "editor capacity growth");
        check(store.size() == 1 && state.id() == owner, "editor identity growth");

        // Inject every allocation failure encountered while preparing a larger
        // transaction. All observable committed state must remain unchanged.
        const std::string large(4096, 'x');
        std::size_t failures = 0;
        for(std::size_t fail = 0; fail < 200; ++fail) {
            TextEditorStore failing_store;
            auto& failing = failing_store.require(failing_store.create("old"));
            check(static_cast<bool>(failing.select({1,2})), "initial selection");
            ryn_test::allocation::begin(fail);
            const auto result = failing.replace_selection(large);
            ryn_test::allocation::end();
            if(result) { break; }
            ++failures;
            check(result.error == TextEditError::allocation_failure, "injection error kind");
            check(failing.value() == "old" && failing.selection() == TextSelection{1,2}
                && failing.revision() == 0 && failing.boundaries().scalar_count() == 3, "allocation failure not atomic");
        }
        check(failures > 10 && failures < 200, "failure injection did not cover preparation");
        std::cout << "editor_cycles=10000 operations=90000 allocations=" << allocations
                  << " retained_capacity=" << capacity << " owners=1 elapsed_us=" << elapsed
                  << " atomic_failure_points=" << failures << '\n';
    } catch(const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n'; return 1;
    }
}
