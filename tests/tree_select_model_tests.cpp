#include "component/tree_select_model.hpp"
#include <ryn/tree_select.hpp>
#include <ryn/form.hpp>
#include <iostream>
#include <type_traits>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    check(failed, "invalid TreeSelect topology was accepted");
}
} // namespace

int main() {
    try {
        static_assert(std::same_as<TreeSelectRef, InputRef>);
        Signal<String> value{String{u8"leaf"}};
        Signal<bool> disabled{false};
        FormField<String> field;
        const auto props = TreeSelectProps{}
                               .value(value)
                               .open(false)
                               .treeExpandedKeys(TreeKeys{u8"root"})
                               .placeholder(u8"Choose")
                               .allowClear()
                               .disabled(disabled)
                               .readOnly(field.readOnly())
                               .visible(true)
                               .ref(TreeSelectRef{})
                               .size(ControlSize::Small)
                               .variant(InputVariant::Filled)
                               .status(field.status())
                               .listHeight(128)
                               .onChange(field.onChange());
        static_cast<void>(props);
        std::vector<TreeSelectItem> items{
            TreeSelectItem{u8"root", u8"Root", {TreeSelectItem{u8"leaf", u8"Leaf"}.disabled(disabled)}}};
        static_cast<void>(items);
        const std::vector<TreeSelectNode> nodes{{u8"root", u8"Root"}, {u8"leaf", u8"Leaf", 0}};
        TreeSelectModel model{nodes};
        check(model.request_value(u8"leaf") && model.label() == u8"Leaf", "uncontrolled selection failed");
        check(!model.request_value(u8"leaf") && model.value() == u8"leaf", "same value toggled selection");
        check(model.request_value({}) && model.value().empty(), "clear failed");
        model.reconcile_value(u8"unknown");
        check(model.label() == u8"unknown" && !model.request_value(u8"unknown"), "unknown authority lost");
        model.reconcile_value(u8"leaf");
        model.presentation(1, u8"Renamed", true, false);
        check(model.label() == u8"Renamed" && model.value() == u8"leaf" && !model.request_value(u8"leaf"),
              "presentation changed authority or allowed disabled selection");
        TreeSelectModel controlled{nodes, {}, true, true, true};
        check(controlled.request_value(u8"leaf") && controlled.value().empty() && controlled.request_value(u8"leaf"),
              "controlled rejection prevented retry");
        controlled.reconcile_value(u8"leaf");
        check(!controlled.request_value(u8"leaf"), "accepted value repeated change");
        check(controlled.request_open(false) && controlled.open(), "controlled close overrode authority");
        check(controlled.suppress() && !controlled.open(), "forced suppression failed");
        controlled.reconcile_open(true);
        check(!controlled.open(), "unchanged controlled open resurrected suppressed popup");
        controlled.reconcile_open(false);
        controlled.reconcile_open(true);
        check(controlled.open(), "explicit authority reopen failed");
        check(controlled.suppress() && controlled.request_open(true) && controlled.open(), "user reopen failed");
        rejects([] { TreeSelectModel bad{{{u8"", u8"Empty"}}}; });
        rejects([] { TreeSelectModel bad{{{u8"a", u8"A"}, {u8"a", u8"Again"}}}; });
        rejects([] { TreeSelectModel bad{{{u8"a", u8"A", 0}}}; });
        rejects([] { TreeSelectModel bad{std::vector<TreeSelectNode>(1025)}; });
        std::vector<TreeSelectNode> deep;
        for (std::size_t i = 0; i < 64; ++i) {
            deep.push_back(
                {String::from_utf8(std::to_string(i)).value(), u8"Node", i == 0 ? tree_select_no_parent : i - 1});
        }
        TreeSelectModel valid{deep};
        deep.push_back({u8"extra", u8"Extra", 63});
        rejects([&] { TreeSelectModel bad{deep}; });
        std::cout << "TreeSelect API and model passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
