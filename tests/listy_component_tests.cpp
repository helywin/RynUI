#include "component/listy_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

std::vector<detail::MountedInputComponent> row_inputs(Fixture& f, runtime::ComponentId viewport) {
    std::vector<detail::MountedInputComponent> result;
    for (const auto& input : f.inputs.mounted_inputs()) {
        for (auto owner = std::optional{input.component}; owner; owner = f.services.components().parent(*owner)) {
            if (*owner == viewport) {
                result.push_back(input);
                break;
            }
        }
    }
    return result;
}

std::vector<ListyItem<String>> records() {
    return {{u8"a", u8"Alpha"}, {u8"b", u8"Beta"}, {u8"c", u8"Gamma"}, {u8"d", u8"Delta"}};
}

void keyed_data_grid_and_lifecycle() {
    Fixture f;
    Signal<std::vector<ListyItem<String>>> items{records()};
    Signal<std::size_t> columns{1};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
    int runs{};
    ListyRef ref;
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}
                  .items(items)
                  .columns(columns)
                  .height(100.0F)
                  .ref(ref)
                  .disabled(disabled)
                  .visible(visible)
                  .layout(LayoutStyle{}.width(dp(320)))
                  .itemRender([&](ListyItemView<String> view) {
                      ++runs;
                      Flex(FlexProps{}.vertical(true), FlexContent{[view] {
                               Text(TextProps{}.content(view.data()));
                               Input(InputProps{}.defaultValue(u8"draft").layout(LayoutStyle{}.width(dp(100))));
                           }});
                  }));
    }});
    f.synchronize();
    const auto id = f.services.listy().mounted().front();
    const auto original = f.services.listy().snapshot(id);
    const auto input = row_inputs(f, original.viewport).front();
    check(runs == 4 && ref.bound() && original.maximum > 0, "Listy did not mount real scrollable rows");
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard), "Listy editor focus failed");
    static_cast<void>(f.inputs.editors().require(input.editor).select_all());
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained"}, f.inputs.sessions().active()})),
          "Listy editor input failed");
    auto next = items.get();
    std::reverse(next.begin(), next.end());
    next.back().data(String{u8"Updated Alpha"});
    items.set(next);
    columns.set(2);
    f.synchronize();
    const auto grid = f.services.listy().snapshot(id);
    check(runs == 4 && grid.items.back().component == original.items.front().component &&
              grid.items[0].bounds.y == grid.items[1].bounds.y && grid.items[1].bounds.x > grid.items[0].bounds.x,
          "Listy grid/reorder remounted keys or failed actual placement");
    check(f.inputs.editors().require(input.editor).value() == String{u8"retained"}.bytes(), "Listy lost the draft");
    disabled.set(true);
    f.synchronize();
    check(!f.services.focus().state().focused && !f.inputs.sessions().active().valid() &&
              f.services.components().branch_active(grid.viewport) && !ref.scrollTo(0),
          "Listy disabled did not gate input while keeping content");
    disabled.set(false);
    visible.set(false);
    f.synchronize();
    check(!f.services.components().branch_active(id) && runs == 4,
          "Listy visible remounted or retained active content");
    visible.set(true);
    items.set({});
    f.synchronize();
    check(f.services.listy().snapshot(id).items.empty() && row_inputs(f, original.viewport).empty() &&
              f.services.components().branch_active(original.empty),
          "Listy empty state retained removed input");
    items.set(records());
    f.synchronize();
    check(runs == 8 && f.services.listy().snapshot(id).items.size() == 4, "Listy empty recovery failed");
    f.services.dispose();
    check(!ref.bound() && !ref.scrollTo(1) && f.services.interactions().size() == 0 && f.nodes.size() == 0,
          "Listy disposal leaked ref, interactions or layouts");
}

void controlled_paging_and_scroll() {
    Fixture f;
    Signal<std::size_t> page{1};
    Signal<ListyDirection> direction{ListyDirection::Vertical};
    Signal<bool> paginated{true};
    Signal<std::vector<ListyItem<String>>> items{records()};
    ListyRef ref;
    int requests{};
    int runs{};
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}
                  .items(items)
                  .current(page)
                  .pageSize(2)
                  .pagination(paginated)
                  .direction(direction)
                  .height(40.0F)
                  .ref(ref)
                  .onChange([&](auto, auto) { ++requests; })
                  .layout(LayoutStyle{}.width(dp(220)))
                  .itemRender([&](ListyItemView<String> view) {
                      ++runs;
                      Input(InputProps{}.defaultValue(view.key).layout(LayoutStyle{}.width(dp(160))));
                  }));
    }});
    f.synchronize();
    const auto id = f.services.listy().mounted().front();
    const auto first = f.services.listy().snapshot(id);
    check(first.items[0].shown && !first.items[2].shown, "Listy paging did not gate rows");
    const auto viewport_bounds = f.nodes.require(f.services.components().root(first.viewport)).bounds;
    const auto pager_bounds = f.nodes.require(f.services.components().root(first.pagination)).bounds;
    const auto section_gap = f.services.components().theme_scope(id)->snapshot().map().size_small;
    check(pager_bounds.y >= viewport_bounds.y + viewport_bounds.height + section_gap,
          "Listy pagination touched the clipped row viewport");
    // The actual Pagination input path must request a page and respect application rejection.
    bool clicked{};
    for (const auto& button : f.buttons.mounted_buttons()) {
        const auto rect = f.nodes.require(f.services.components().root(button.component)).bounds;
        if (rect.width > 0 && rect.height > 0) {
            f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                           rect.x + rect.width / 2, rect.y + rect.height / 2});
            f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary,
                                           rect.x + rect.width / 2, rect.y + rect.height / 2});
            if (requests) {
                clicked = true;
                break;
            }
        }
    }
    check(clicked && f.services.listy().snapshot(id).current == 1,
          "Listy controlled request was missing or changed authority");
    page.set(2);
    f.synchronize();
    check(f.services.listy().snapshot(id).items[2].shown && !f.services.listy().snapshot(id).items[0].shown &&
              runs == 4,
          "Listy page change rebuilt rows or did not update shown rows");
    items.set({records()[0]});
    f.synchronize();
    check(f.services.listy().snapshot(id).current == 1 && page.get() == 2,
          "Listy total shrink changed controlled application state");
    items.set(records());
    paginated.set(false);
    direction.set(ListyDirection::Horizontal);
    f.synchronize();
    check(ref.scrollTo(String{u8"d"}, ListyScrollAlign::End) && f.services.listy().snapshot(id).offset > 0,
          "Listy horizontal scrollTo key failed");
}

void validation_and_failed_renderer() {
    Fixture f;
    Signal<std::vector<ListyItem<String>>> items{records()};
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}.items(items).itemRender([](ListyItemView<String> view) {
            Input(InputProps{});
            if (view.key == String{u8"fail"}) {
                throw std::runtime_error("renderer failure");
            }
        }));
    }});
    f.synchronize();
    bool rejected{};
    try {
        items.set({{u8"duplicate", u8"One"}, {u8"duplicate", u8"Two"}});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    const auto viewport = f.services.listy().snapshot(f.services.listy().mounted().front()).viewport;
    check(rejected && row_inputs(f, viewport).size() == 4, "Listy invalid keys mutated live rows");
    items.set({{u8"fail", u8"Failure"}});
    rejected = false;
    try {
        f.synchronize();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected && f.inputs.mounted_inputs().empty() && f.nodes.size() == 0 && f.services.interactions().size() == 0,
          "Listy renderer failure left partial UI or input");
}
} // namespace

int main() {
    try {
        keyed_data_grid_and_lifecycle();
        controlled_paging_and_scroll();
        validation_and_failed_renderer();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
