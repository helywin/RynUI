#include "component/statistic_component.hpp"
#include "support/statistic_example.hpp"
#include "support/timeline_fixture.hpp"
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::StatisticPrefix, ryn::StatisticSuffix>);
static_assert(!std::is_convertible_v<ryn::StatisticTitle, ryn::StatisticValueContent>);

int main() {
    try {
        using namespace timeline_test;
        Fixture f;
        Signal<std::uint64_t> integer{42};
        f.services.mount(Content{[&] {
            StatisticExample();
            Statistic(StatisticProps{}.value(integer));
            Statistic(
                StatisticProps{}.value(bind([integer] { return static_cast<double>(integer.get()) + .123456789; })));
        }});
        sync(f);
        const auto all = f.services.statistic().mounted();
        check(all.size() == 5 &&
                  f.services.statistic().snapshot(all[0]).formatted_value.bytes() == "18,446,744,073,709,551,615" &&
                  f.services.statistic().snapshot(all[1]).formatted_value.bytes() == "-1,234.12345678" &&
                  f.services.statistic().snapshot(all[2]).formatted_value == String{u8"已完成"},
              "public example mismatch");
        integer.set(1234);
        sync(f);
        check(f.services.statistic().snapshot(all[3]).formatted_value.bytes() == "1,234" &&
                  f.services.statistic().snapshot(all[4]).formatted_value.bytes() == "1,234.123456789",
              "typed numeric Signal/Binding did not update");
        check(f.scene.glyph_scene().instances().size() > 0, "example emitted no real glyphs");
        std::cout << "Statistic public API and documentation example passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
