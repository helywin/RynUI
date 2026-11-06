#include "component/avatar_component.hpp"
#include "component/image_component.hpp"
#include "component/popover_component.hpp"
#include "component/form_component.hpp"
#include "support/timeline_fixture.hpp"
#include <thread>
#include <iostream>
using namespace timeline_test;

namespace {
int checks{};

void expect(bool value, const char* why) {
    ++checks;
    check(value, why);
}

std::shared_ptr<const ImagePixels> pixels() {
    const std::array<std::byte, 16> rgba{std::byte{255}, std::byte{0},   std::byte{0},   std::byte{255},
                                         std::byte{0},   std::byte{255}, std::byte{0},   std::byte{255},
                                         std::byte{0},   std::byte{0},   std::byte{255}, std::byte{255},
                                         std::byte{255}, std::byte{255}, std::byte{0},   std::byte{255}};
    return ImagePixels::copy(2, 2, 8, rgba);
}

void images() {
    Fixture f;
    std::vector<ImageCompletion> pending;
    int cancels{};
    int errors{};
    int loads{};
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion complete) {
        pending.push_back(complete);
        return [&] {
            ++cancels;
        };
    };
    Signal<ImageSource> source{ImageSource{loader}};
    Signal<bool> visible{true};
    Signal<AvatarShape> shape{AvatarShape::Circle};
    bool permit = true;
    f.services.mount(Content{[&] {
        Avatar(AvatarProps{}
                   .source(source)
                   .visible(visible)
                   .shape(shape)
                   .text(u8"AB")
                   .onError([&](const ImageError&) {
                       ++errors;
                       return permit;
                   })
                   .onLoad([&] { ++loads; }));
    }});
    sync(f);
    const auto id = f.services.avatar().mounted().front();
    const auto image = f.services.avatar().snapshot(id).image;
    expect(pending.size() == 1, "Avatar loader did not start through Image");
    bool foreign = true;
    std::thread worker([&] { foreign = pending[0]({pixels(), {}}); });
    worker.join();
    expect(!foreign, "Foreign Avatar completion accepted");
    visible.set(false);
    sync(f);
    expect(cancels == 1 && !pending[0]({pixels(), {}}), "Hidden Avatar accepted stale completion");
    visible.set(true);
    sync(f);
    expect(pending.size() == 2 && pending[1]({pixels(), {}}), "Avatar image failed to resume");
    sync(f);
    auto view = f.services.image().snapshot(image);
    expect(loads == 1 && view.image && f.services.avatar().snapshot(id).image_visible,
           "Real colored Avatar image missing");
    auto slot = f.services.images().find(*view.image);
    expect(slot && slot->instance.pixels->bytes()[0] == std::byte{255}, "Avatar did not retain actual RGBA pixels");
    const auto hit = f.services.interactions().find(view.interaction);
    expect(hit && !hit->eligible && !hit->focusable, "Avatar preview stole input");
    const auto covered = [&](runtime::Point point) {
        for (const auto& part : f.services.images().slots()) {
            if (!part.live || !contains(part.instance.clip, point) || !contains(part.instance.bounds, point)) {
                continue;
            }
            if (part.instance.circle_clip) {
                const auto circle = *part.instance.circle_clip;
                const float x = (point.x - circle.x - circle.width / 2) / (circle.width / 2);
                const float y = (point.y - circle.y - circle.height / 2) / (circle.height / 2);
                if (x * x + y * y > 1) {
                    continue;
                }
            }
            return true;
        }
        return false;
    };
    const auto circle_bounds = bounds(f, id);
    expect(!covered({circle_bounds.x + .01F, circle_bounds.y + .01F}) &&
               covered({circle_bounds.x + circle_bounds.width / 2, circle_bounds.y + circle_bounds.height / 2}),
           "Circle Avatar pixels escaped the public shape contract");
    shape.set(AvatarShape::Square);
    sync(f);
    const auto square_bounds = bounds(f, id);
    const auto square_resources = f.services.images().live_count();
    expect(!covered({square_bounds.x + .01F, square_bounds.y + .01F}) &&
               covered({square_bounds.x + square_bounds.width / 2, square_bounds.y + square_bounds.height / 2}),
           "Square Avatar pixels did not obey actual rounded clip");
    source.set(ImageSource{});
    sync(f);
    source.set(ImageSource{loader});
    sync(f);
    expect(pending.back()({{}, {ImageErrorCode::loader_failure, "synthetic failure"}}),
           "Live error completion rejected");
    sync(f);
    expect(errors == 1 && !f.services.avatar().snapshot(id).image_visible &&
               f.services.components().branch_active(f.services.avatar().snapshot(id).content),
           "Avatar error did not show text fallback");
    source.set(ImageSource{});
    sync(f);
    permit = false;
    source.set(ImageSource{loader});
    sync(f);
    pending.back()({{}, {ImageErrorCode::loader_failure, "synthetic refusal"}});
    sync(f);
    expect(errors == 2 && f.services.avatar().snapshot(id).image_visible &&
               !f.services.components().branch_active(f.services.avatar().snapshot(id).content),
           "onError false did not suppress default fallback");
    source.set(ImageSource{pixels()});
    sync(f);
    expect(loads == 2 && f.services.images().live_count() == square_resources, "Avatar retry failed or orphaned image");
    ImageCompletion reentrant_pending;
    auto reentrant = std::make_shared<ImageLoader>();
    reentrant->start = [&](ImageCompletion complete) {
        reentrant_pending = complete;
        return [&] {
            source.set(ImageSource{pixels()});
        };
    };
    source.set(ImageSource{reentrant});
    sync(f);
    visible.set(false);
    sync(f);
    expect(!reentrant_pending({pixels(), {}}) && f.services.images().live_count() == 0,
           "Cancel reentry revived hidden Avatar pixels");
    visible.set(true);
    sync(f);
    expect(loads == 3 && f.services.images().live_count() == square_resources,
           "Cancel reentry lost newest Avatar source");
    f.services.dispose();
    expect(f.services.images().live_count() == 0 && !f.services.next_frame_deadline(),
           "Avatar retired image/deadline leaked");
    Fixture failure;
    auto failing = std::make_shared<ImageLoader>();
    failing->start = [](ImageCompletion complete) {
        complete({{}, {ImageErrorCode::loader_failure, "synthetic"}});
        return ImageCancel{};
    };
    failure.services.mount(Content{[&] {
        Avatar(AvatarProps{}.source(ImageSource{failing}).onError([](const ImageError&) -> bool {
            throw std::runtime_error("synthetic Avatar callback");
        }));
    }});
    bool threw = false;
    try {
        sync(failure);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw && failure.services.avatar().mounted().empty() && failure.services.images().live_count() == 0,
           "Avatar error callback exception did not retire owner");
}

void responsive() {
    Fixture f;
    const auto sizes = Responsive<float>{20}.at(Breakpoint::Sm, 32).at(Breakpoint::Lg, 64);
    Signal<AvatarSizeValue> size{AvatarSizeValue{sizes}};
    f.services.mount(Content{[&] { Avatar(AvatarProps{}.size(size).text(u8"WWWWWW")); }});
    const auto id = f.services.avatar().mounted()[0];
    for (const auto [width, expected] :
         std::array<std::pair<float, float>, 4>{{{400, 20}, {800, 32}, {1200, 64}, {1, 1}}}) {
        expect(f.services.layout_and_synchronize({width, 100}, {0, 0, width, 100}), "Responsive Avatar layout failed");
        expect(bounds(f, id).width == expected, "Avatar used container or pixel width instead of logical window width");
    }
    expect(f.services.layout_and_synchronize({1200, 100}, {0, 0, 1200, 100}), "Avatar zero-size restoration failed");
    expect(bounds(f, id).width == 64, "Avatar did not restore responsive extent");
    auto invalid = Responsive<float>{30}.at(Breakpoint::Lg, -1);
    bool rejected = false;
    try {
        size.set(AvatarSizeValue{invalid});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected && bounds(f, id).width == 64, "Invalid responsive update changed retained geometry");
    f.services.dispose();
}

void keyed() {
    Fixture f;
    Signal<std::vector<AvatarGroupItem>> items{
        std::vector{AvatarGroupItem{u8"a", u8"A"}, AvatarGroupItem{u8"b", u8"B"}, AvatarGroupItem{u8"c", u8"C"}}};
    Signal<bool> rtl{false};
    int builds{};
    bool throw_open = false;
    InputRef ref;
    auto renderer = std::make_shared<AvatarGroupPopoverRenderer>();
    renderer->render = [&] {
        ++builds;
        Input(InputProps{}.defaultValue(u8"retained").ref(ref));
    };
    f.services.mount(Content{[&] {
        AvatarGroup(AvatarGroupProps{}
                        .items(items)
                        .rtl(rtl)
                        .max(AvatarGroupMax{1, PopoverTriggerMode::Click})
                        .popoverRender(renderer)
                        .onOpenChange([&](bool open) {
                            if (open && throw_open) {
                                throw std::runtime_error("synthetic overflow callback");
                            }
                        }));
    }});
    sync(f);
    const auto group = f.services.avatar().groups()[0];
    const auto before = f.services.avatar().group_snapshot(group);
    click(f, before.overflow);
    expect(ref.bound() && builds == 1, "Custom Avatar overflow content missing");
    items.set(
        std::vector{AvatarGroupItem{u8"c", u8"C2"}, AvatarGroupItem{u8"a", u8"A2"}, AvatarGroupItem{u8"d", u8"D"}});
    sync(f);
    const auto after = f.services.avatar().group_snapshot(group);
    expect(after.items[0].inline_avatar == before.items[2].inline_avatar &&
               after.items[1].inline_avatar == before.items[0].inline_avatar,
           "Keyed Avatar reorder rebuilt stable keys");
    expect(!f.services.components().contains(before.items[1].inline_avatar) && builds == 1 && ref.bound(),
           "Avatar key removal or retained popover cleanup failed");
    rtl.set(true);
    sync(f);
    expect(bounds(f, after.items[0].inline_avatar).x > bounds(f, after.popover).x,
           "RTL AvatarGroup did not mirror geometry");
    bool rejected = false;
    try {
        items.set(std::vector{AvatarGroupItem{u8"x", u8"X"}, AvatarGroupItem{u8"x", u8"Y"}});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected && f.services.avatar().group_snapshot(group).items[0].inline_avatar == after.items[0].inline_avatar,
           "Duplicate Avatar update was not atomic");
    click(f, after.overflow);
    throw_open = true;
    bool threw_open = false;
    try {
        click(f, after.overflow);
    } catch (const std::runtime_error&) {
        threw_open = true;
    }
    expect(threw_open && !f.services.components().contains(group) && !ref.bound(),
           "Overflow callback exception left stale AvatarGroup");
    f.services.dispose();
    expect(!ref.bound() && f.services.images().live_count() == 0 && !f.services.next_frame_deadline(),
           "AvatarGroup cleanup leaked");
}

void reentrant_group() {
    Fixture f;
    ImageCompletion pending;
    bool replace = false;
    auto loader = std::make_shared<ImageLoader>();
    Signal<std::vector<AvatarGroupItem>> items{std::vector<AvatarGroupItem>{}};
    loader->start = [&](ImageCompletion c) {
        pending = c;
        return [&] {
            if (replace) {
                items.set(std::vector{AvatarGroupItem{u8"b", u8"B"}, AvatarGroupItem{u8"c", u8"C"}});
            }
        };
    };
    items.set(std::vector{AvatarGroupItem{u8"a", u8"A"}.source(ImageSource{loader}), AvatarGroupItem{u8"b", u8"B"}});
    f.services.mount(Content{[&] { AvatarGroup(AvatarGroupProps{}.items(items).max(AvatarGroupMax{2})); }});
    sync(f);
    const auto id = f.services.avatar().groups()[0];
    const auto retained = f.services.avatar().group_snapshot(id).items[1].inline_avatar;
    replace = true;
    items.set(std::vector{AvatarGroupItem{u8"a", u8"A"}.source(ImageSource{pixels()}), AvatarGroupItem{u8"b", u8"B"}});
    sync(f);
    const auto latest = f.services.avatar().group_snapshot(id);
    expect(latest.items.size() == 2 && latest.items.front().key == String{u8"b"} &&
               latest.items.front().inline_avatar == retained,
           "Avatar cancel reentry lost latest keyed data");
    expect(!pending({pixels(), {}}) && f.services.images().live_count() == 0,
           "Reentrant Avatar data accepted retired image completion");
    replace = false;
    f.services.dispose();
}
} // namespace

int main() {
    try {
        images();
        responsive();
        keyed();
        reentrant_group();
        std::cout << checks << " Avatar native checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
