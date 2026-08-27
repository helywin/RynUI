#include <ryn/component.hpp>

#include <concepts>
#include <utility>

namespace {

struct PanelContentSlot final {};
struct PanelPrefixSlot final {};
struct PanelFooterSlot final {};

using PanelContent = ryn::SlotContent<PanelContentSlot>;
using PanelPrefix = ryn::SlotContent<PanelPrefixSlot>;
using PanelFooter = ryn::SlotContent<PanelFooterSlot>;

struct ContentOnlyPanel final {
    void operator()(PanelContent) const {}
};

static_assert(std::invocable<ContentOnlyPanel, PanelContent>);
static_assert(!std::invocable<ContentOnlyPanel, PanelPrefix>);
static_assert(!std::invocable<ContentOnlyPanel, PanelFooter>);
static_assert(!std::constructible_from<PanelContent, PanelPrefix>);
static_assert(!std::constructible_from<PanelContent, ryn::Content>);

} // namespace

int main() {
    int runs = 0;
    ryn::Content content{[&] { ++runs; }};
    PanelContent panel_content{[&] { ++runs; }};
    static_cast<void>(content);
    ContentOnlyPanel{}(std::move(panel_content));
    return runs == 0 ? 0 : 1;
}
