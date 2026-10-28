#include "component/tree_model.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

void run() {
    TreeModel m;
    m.nodes = {{u8"root", tree_no_parent, 0, 6}, {u8"a", 0, 1, 2},           {u8"b", 0, 1, 3},
               {u8"disabled", 0, 1, 5, true},    {u8"independent", 3, 2, 5}, {u8"last", 0, 1, 6}};
    m.validate();
    check(m.shown() == std::vector<std::size_t>{0}, "collapsed descendants visible");
    m.expanded = {u8"root", u8"disabled"};
    check(m.shown().size() == 6, "preorder expansion lost disabled branch children");
    m.nodes[3].visible = false;
    check(m.shown() == std::vector<std::size_t>{0, 1, 2, 5}, "hidden subtree occupied flow");
    m.nodes[3].visible = true;
    m.checkable = true;
    m.checked = {u8"root"};
    check(m.checks(m.checked).checked == TreeKeys{u8"root", u8"a", u8"b", u8"last"},
          "disabled conduction boundary leaked");
    m.checked = m.toggle_check(1).checked;
    auto c = m.checks(m.checked);
    check(c.checked == TreeKeys{u8"b", u8"last"} && c.half_checked == TreeKeys{u8"root"},
          "deselect did not remove ancestor aggregate");
    m.checked = m.toggle_check(4).checked;
    c = m.checks(m.checked);
    check(std::ranges::find(c.checked, String{u8"independent"}) != c.checked.end() &&
              std::ranges::find(c.checked, String{u8"disabled"}) == c.checked.end(),
          "independent child changed disabled ancestor");
    m.controlled_expanded = true;
    const auto before = m.expanded;
    check(m.expand(0) == TreeKeys{u8"disabled"} && m.expanded == before, "candidate mutated controlled model");
    m.selected = {u8"missing", u8"a", u8"b", u8"a"};
    check(m.normalize(m.selected, true) == TreeKeys{u8"a"} && m.select(1).empty() && m.select(2) == TreeKeys{u8"b"},
          "single selection normalization failed");
    m.disabled = true;
    check(m.select(2) == TreeKeys{u8"a"} && m.expand(0) == before, "disabled tree accepted action");
    m.visible = false;
    check(m.shown().empty(), "hidden Tree has visible rows");
    for (auto kind : {0, 1, 2, 3}) {
        TreeModel invalid;
        invalid.nodes = m.nodes;
        if (kind == 0) {
            invalid.nodes[1].key = u8"root";
        } else if (kind == 1) {
            invalid.nodes[0].key = String{};
        } else if (kind == 2) {
            invalid.nodes[1].depth = 64;
        } else {
            invalid.nodes.resize(1025);
        }
        bool rejected = false;
        try {
            invalid.validate();
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "invalid topology accepted");
    }
    TreeModel max;
    for (std::size_t i = 0; i < 1024; ++i) {
        max.nodes.push_back({String::from_utf8(std::to_string(i)).value(), tree_no_parent, 0, i + 1});
    }
    max.validate();
    check(max.shown().size() == 1024, "maximum valid nodes were truncated");
    TreeModel deep;
    for (std::size_t i = 0; i < 64; ++i) {
        const auto key = String::from_utf8(std::to_string(i)).value();
        deep.nodes.push_back({key, i ? i - 1 : tree_no_parent, i, 64});
        deep.expanded.push_back(key);
    }
    deep.validate();
    check(deep.shown().size() == 64, "maximum valid depth was truncated");
}
} // namespace

int main() {
    try {
        run();
        std::cout << "Tree model passed: topology, visibility, single selection, disabled conduction, limits\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
