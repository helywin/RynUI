#include "component/timeline_model.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    using namespace ryn;
    using namespace ryn::detail;
    try {
        std::vector<TimelineModelItem> items{{u8"a"}, {u8"b"}, {u8"c", TimelinePlacement::End}};
        const auto check = [](bool ok) {
            if (!ok) {
                throw std::runtime_error("Timeline model drifted");
            }
        };
        check(timeline_order(TimelineMode::Alternate, false, items) ==
              std::vector<TimelineModelEntry>{
                  {0, TimelinePlacement::Start}, {1, TimelinePlacement::End}, {2, TimelinePlacement::End}});
        items[1].visible = false;
        check(timeline_order(TimelineMode::Alternate, true, items) ==
              std::vector<TimelineModelEntry>{{2, TimelinePlacement::End}, {0, TimelinePlacement::Start}});
        for (int invalid = 0; invalid < 4; ++invalid) {
            auto bad = items;
            if (invalid == 0) {
                bad[0].key = u8"";
            }
            if (invalid == 1) {
                bad[1].key = bad[0].key;
            }
            if (invalid == 2) {
                bad[0].placement = static_cast<TimelinePlacement>(9);
            }
            if (invalid == 3) {
                bad[0].tone = static_cast<TimelineTone>(9);
            }
            bool rejected = false;
            try {
                static_cast<void>(timeline_order(TimelineMode::Start, false, bad));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            check(rejected);
        }
        for (auto& item : items) {
            item.visible = false;
        }
        check(timeline_order(TimelineMode::End, false, items).empty());
        bool rejected = false;
        try {
            static_cast<void>(timeline_order(static_cast<TimelineMode>(9), false, items));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        std::cout << "Timeline model passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
