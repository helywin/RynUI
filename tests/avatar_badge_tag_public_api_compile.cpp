#include <ryn/rynui.hpp>
#include <type_traits>

static_assert(static_cast<int>(ryn::ImageShape::Rectangle) == 0);
static_assert(static_cast<int>(ryn::ImageShape::Circle) == 1);
static_assert(!std::is_convertible_v<ryn::AvatarContent, ryn::AvatarIcon>);
static_assert(!std::is_convertible_v<ryn::TagContent, ryn::TagIcon>);

void native_status_examples(std::shared_ptr<const ryn::ImagePixels> pixels) {
    using namespace ryn;
    Signal<ImageSource> source{ImageSource{pixels}};
    Signal<std::vector<AvatarGroupItem>> members{
        std::vector{AvatarGroupItem{u8"a", u8"A"}, AvatarGroupItem{u8"b", u8"B"}}};
    Avatar(
        AvatarProps{}.source(source).size(Responsive<float>{24}.at(Breakpoint::Md, 40)).onError([](const ImageError&) {
            return false;
        }));
    auto popup = std::make_shared<AvatarGroupPopoverRenderer>();
    popup->render = [] {
        Input(InputProps{}.defaultValue(u8"retained overflow draft"));
    };
    AvatarGroup(AvatarGroupProps{}.items(members).max(AvatarGroupMax{1}).popoverRender(popup).rtl());

    Signal<BadgeCountValue> count{BadgeCountValue{String{u8"NEW"}}};
    auto rich = std::make_shared<BadgeRenderer>();
    rich->render = [] {
        Icon(IconProps{}.name(IconName::ClockCircleOutlined));
    };
    Badge(BadgeProps{}.countValue(count), BadgeContent{[] { Button(ButtonProps{}, [] { Text(u8"Inbox"); }); }});
    Badge(BadgeProps{}.countValue(BadgeCountValue{std::shared_ptr<const BadgeRenderer>{rich}}));
    BadgeRibbon(RibbonProps{}.textRender(rich).placement(RibbonPlacement::Start), BadgeContent{[] { Text(u8"Body"); }});

    Signal<TagValues> selected{TagValues{String{u8"a"}}};
    Signal<std::vector<TagGroupItem>> items{std::vector{TagGroupItem{u8"a", u8"A"}, TagGroupItem{u8"b", u8"B"}}};
    Tag(TagProps{}
            .closable()
            .variant(TagVariant::Filled)
            .onClose(std::function<void(TagCloseEvent&)>{[](TagCloseEvent& event) { event.preventDefault(); }}),
        TagContent{[] { Text(u8"Cancel close"); }});
    CheckableTag(CheckableTagProps{}.defaultChecked(), TagContent{[] { Text(u8"Default selection"); }});
    CheckableTagGroup(TagGroupProps{}.items(items).value(selected).onChange(
        [selected](TagValues next) { selected.set(std::move(next)); }));
    TagGroup(TagGroupProps{}.items(items).draggable().onReorder([items](TagValues order) {
        auto accepted = items.get();
        std::vector<TagGroupItem> next;
        for (const auto& key : order) {
            for (const auto& item : accepted) {
                if (item.key == key) {
                    next.push_back(item);
                }
            }
        }
        items.set(std::move(next));
    }));
}
