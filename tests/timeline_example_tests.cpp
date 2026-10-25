#include "support/timeline_fixture.hpp"
#include "../examples/timeline_example.hpp"

int main() {
    using namespace timeline_test;
    try {
        Fixture f;
        Signal<bool> reverse{false};
        Signal<String> title{String{u8"Created"}};
        InputRef ref;
        f.services.mount(Content{[&] { ryn_examples::timeline(reverse, title, ref); }});
        sync(f);
        const auto input = f.inputs.mounted_inputs()[0];
        reverse.set(true);
        title.set(u8"Updated");
        sync(f);
        check(ref.focus() && f.inputs.mounted_inputs()[0].editor == input.editor,
              "documented example lost Input identity");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
