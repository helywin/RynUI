#include "component/card_component.hpp"
#include "component/empty_result_component.hpp"
#include "component/image_component.hpp"
#include "component/tabs_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void card_grid_meta_and_tabs() {
    Fixture f;
    Signal<std::size_t> columns{2};
    Signal<String> active{String{u8"a"}};
    Signal<String> description{String{u8"Meta description"}};
    auto a = TabItem{u8"a", u8"Alpha", TabContent{[] { Input(InputProps{}.defaultValue(u8"tab draft")); }}};
    auto b = TabItem{u8"b", u8"Beta", TabContent{[] { Text(u8"Second panel"); }}};
    Signal<std::vector<TabItem>> tabs{std::vector{a, b}};
    int slot_runs{};
    f.services.mount(Content{[&] {
        Card(CardProps{}
                 .title(u8"Grid and tabs")
                 .gridColumns(columns)
                 .tabList(tabs)
                 .activeTabKey(active)
                 .layout(LayoutStyle{}.width(dp(420))),
             CardContent{[&] {
                 for (int i = 0; i < 3; ++i) {
                     CardGrid(CardGridProps{}.hoverable(i != 1), CardGridContent{[&] {
                                  ++slot_runs;
                                  CardMeta(CardMetaProps{}.title(u8"Account").description(description),
                                           {.avatar = CardMetaAvatar{[] { Text(u8"A"); }}});
                              }});
                 }
             }});
    }});
    f.synchronize();
    const auto tab_id = f.services.tabs().mounted().front();
    const auto tab = f.services.tabs().snapshot(tab_id);
    const auto input = f.inputs.mounted_inputs().front();
    const auto cells = f.services.card().mounted();
    check(cells.size() == 4 && slot_runs == 3, "Card Grid/Meta were not real retained components");
    const auto first = f.nodes.require(f.services.components().root(cells[0])).bounds;
    const auto second = f.nodes.require(f.services.components().root(cells[1])).bounds;
    const auto third = f.nodes.require(f.services.components().root(cells[2])).bounds;
    check(first.y == second.y && second.x > first.x && third.y > first.y, "Card Grid did not lay out rows");
    columns.set(1);
    description.set(String{u8"Changed metadata"});
    tabs.set({b, a});
    f.synchronize();
    check(slot_runs == 3 && f.services.tabs().snapshot(tab_id).panels[1] == tab.panels[0] &&
              f.inputs.mounted_inputs().front().editor == input.editor,
          "Card updates rebuilt grid or keyed tab content");
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.interactions().size() == 0, "Card completion resources leaked");
}

void empty_dynamic_regions_and_image_cancellation() {
    Fixture f;
    Signal<String> description{String{u8"No records"}};
    int image_runs{};
    int description_runs{};
    int extra_runs{};
    EmptyDynamicContent content;
    content.image = std::make_shared<const EmptyImageContent>([&] {
        ++image_runs;
        Text(u8"Artwork");
    });
    content.description = std::make_shared<const EmptyDescription>([&] {
        ++description_runs;
        Text(u8"Rich description");
    });
    content.extra = std::make_shared<const EmptyExtra>([&] {
        ++extra_runs;
        Input(InputProps{}.defaultValue(u8"retained extra"));
    });
    Signal<EmptyDynamicContent> dynamic{content};
    f.services.mount(Content{[&] { Empty(EmptyProps{}.description(description).dynamicContent(dynamic)); }});
    f.synchronize();
    const auto id = f.services.empty_result().empties().front();
    const auto input = f.inputs.mounted_inputs().front();
    description.set(String{u8"Updated fallback"});
    content.description.reset();
    dynamic.set(content);
    f.synchronize();
    check(image_runs == 1 && description_runs == 1 && extra_runs == 1 &&
              f.inputs.mounted_inputs().front().editor == input.editor,
          "Empty dynamic description rebuilt unrelated image/extra regions");
    content.extra.reset();
    dynamic.set(content);
    f.synchronize();
    check(f.inputs.mounted_inputs().empty() &&
              !f.services.components().branch_active(f.services.empty_result().snapshot(id).extra),
          "Empty dynamic extra removal retained input");
    f.services.dispose();
    Fixture image_fixture;
    int cancels{};
    ImageCompletion complete;
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion value) {
        complete = std::move(value);
        return [&] {
            ++cancels;
        };
    };
    image_fixture.services.mount(Content{[&] { Empty(EmptyProps{}.source(ImageSource{loader})); }});
    image_fixture.synchronize();
    check(bool(complete) && !image_fixture.services.image().mounted().empty(),
          "Empty did not use the common Image loader");
    image_fixture.services.dispose();
    check(cancels == 1 && !complete(ImageDecodeResult{{}, {ImageErrorCode::loader_failure, "late"}}),
          "Empty image teardown did not cancel/reject stale completion");
}

void animation_and_illustration_lifecycle() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> loading{true};
    Signal<bool> illustration{true};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Card(CardProps{}.loading(loading).layout(LayoutStyle{}.width(dp(300))),
                      CardContent{[] { Text(u8"Ready"); }});
                 Empty(EmptyProps{}.illustration(illustration));
             }});
    }});
    f.synchronize();
    auto& cards = f.services.card();
    const auto time = animation::AnimationTime::microseconds(200000);
    cards.tick_auxiliary(time);
    check(cards.next_auxiliary_deadline().has_value(), "Card loading did not schedule a real pulse");
    check(cards.tick_auxiliary(animation::AnimationTime::microseconds(600000)) > 0,
          "Card loading tick did not change presentation");
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    check(!cards.next_auxiliary_deadline(), "reduced motion retained Card animation deadline");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    loading.set(false);
    illustration.set(false);
    f.synchronize();
    check(!cards.next_auxiliary_deadline(), "idle Card retained animation deadline");
    const auto count = f.services.components().component_count();
    illustration.set(true);
    f.synchronize();
    check(count == f.services.components().component_count(), "Empty illustration toggle rebuilt retained layers");
    f.services.dispose();
    check(!cards.next_auxiliary_deadline() && f.nodes.size() == 0, "Card/illustration retirement leaked work");
}
} // namespace

int main() {
    try {
        card_grid_meta_and_tabs();
        empty_dynamic_regions_and_image_cancellation();
        animation_and_illustration_lifecycle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
