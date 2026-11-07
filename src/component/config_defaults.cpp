#include "component/config_defaults.hpp"

#include <ryn/button.hpp>
#include <ryn/checkbox.hpp>
#include <ryn/input.hpp>
#include <ryn/input_number.hpp>
#include <ryn/password.hpp>
#include <ryn/radio.hpp>
#include <ryn/switch.hpp>
#include <ryn/text_area.hpp>

#include <ryn/auto_complete.hpp>
#include <ryn/mentions.hpp>
#include <ryn/select.hpp>
#include <ryn/cascader.hpp>
#include <ryn/color_picker.hpp>
#include <ryn/date_picker.hpp>
#include <ryn/time_picker.hpp>
#include <ryn/transfer.hpp>
#include <ryn/tree_select.hpp>
#include <ryn/tree.hpp>
#include <ryn/upload.hpp>
#include <ryn/otp.hpp>
#include <ryn/calendar.hpp>
#include <ryn/carousel.hpp>
#include <ryn/menu.hpp>
#include <ryn/dropdown.hpp>
#include <ryn/pagination.hpp>
#include <ryn/tabs.hpp>
#include <ryn/steps.hpp>
#include <ryn/segmented.hpp>
#include <ryn/rate.hpp>
#include <ryn/popover.hpp>
#include <ryn/tooltip.hpp>
#include <ryn/popconfirm.hpp>
#include <ryn/tour.hpp>
#include <ryn/tag.hpp>
#include <ryn/badge.hpp>
#include <ryn/image.hpp>
#include <ryn/avatar.hpp>
#include <ryn/card.hpp>
#include <ryn/collapse.hpp>
#include <ryn/descriptions.hpp>
#include <ryn/spin.hpp>
#include <ryn/skeleton.hpp>
#include <ryn/space.hpp>
#include <ryn/float_button.hpp>
#include <ryn/divider.hpp>
#include <ryn/breadcrumb.hpp>
#include <ryn/text.hpp>
#include <ryn/typography.hpp>
#include <ryn/drawer.hpp>
#include <ryn/timeline.hpp>
#include <ryn/flex.hpp>
#include <ryn/grid.hpp>

#include <ryn/search.hpp>
#include <ryn/slider.hpp>
#include <ryn/table.hpp>
#include <ryn/listy.hpp>
#include <ryn/masonry.hpp>

namespace ryn::detail {

void ConfigDefaults::apply(AutoCompleteProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_, true);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(MentionsProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(SelectProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_, true);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(CascaderProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_, true);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(ColorPickerProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_, true);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(DatePickerProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_, true);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(DateRangePickerProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    apply(props.picker_);
}

void ConfigDefaults::apply(DateTimeRangePickerProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    apply(props.picker_);
}

void ConfigDefaults::apply(TimePickerProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_, true);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TimeRangePickerProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    apply(props.picker_);
}

void ConfigDefaults::apply(TransferProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TreeSelectProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_, true);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(TreeProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(UploadProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(OTPProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(CalendarProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(CarouselProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(MenuProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(PaginationProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TabsProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(StepsProps& props) {
    props.size_ = config_default(props.size_, props.explicit_size_, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_size
                   ? std::optional{*value.explicit_size == ControlSize::Small ? StepsSize::Small : StepsSize::Default}
                   : std::nullopt;
    });
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(SegmentedProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(RateProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(PopoverProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TooltipProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TourProps& props) {
    props.direction_ = config_layout_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TagProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(CheckableTagProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(TagGroupProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(BadgeProps& props) {
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(RibbonProps& props) {
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(ImageProps& props) {
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(PreviewGroupProps& props) {
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(AvatarProps& props) {
    props.size_ = config_default(props.size_, props.explicit_size_, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_size
                   ? std::optional{AvatarSizeValue{*value.explicit_size == ControlSize::Small   ? AvatarSize::Small
                                                   : *value.explicit_size == ControlSize::Large ? AvatarSize::Large
                                                                                                : AvatarSize::Medium}}
                   : std::nullopt;
    });
}

void ConfigDefaults::apply(AvatarGroupProps& props) {
    props.size_ = config_default(props.size_, props.explicit_size_, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_size
                   ? std::optional{AvatarSizeValue{*value.explicit_size == ControlSize::Small   ? AvatarSize::Small
                                                   : *value.explicit_size == ControlSize::Large ? AvatarSize::Large
                                                                                                : AvatarSize::Medium}}
                   : std::nullopt;
    });
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(CardProps& props) {
    props.size_ = config_default(props.size_, props.explicit_size_, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_size
                   ? std::optional{*value.explicit_size == ControlSize::Small ? CardSize::Small : CardSize::Default}
                   : std::nullopt;
    });
}

void ConfigDefaults::apply(CollapseProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(DescriptionsProps& props) {
    props.size_ = config_default(props.size_, props.explicit_size_, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_size
                   ? std::optional{*value.explicit_size == ControlSize::Small    ? DescriptionsSize::Small
                                   : *value.explicit_size == ControlSize::Middle ? DescriptionsSize::Middle
                                                                                 : DescriptionsSize::Default}
                   : std::nullopt;
    });
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(SpinProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
}

void ConfigDefaults::apply(SkeletonAvatarProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
}

void ConfigDefaults::apply(SkeletonButtonProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
}

void ConfigDefaults::apply(SkeletonInputProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
}

void ConfigDefaults::apply(SpaceProps& props) {
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(SpaceCompactProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(SpaceAddonProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    if (!props.size_) {
        props.size_ = config_size(Prop<ControlSize>{ControlSize::Middle}, false, true);
    }
}

void ConfigDefaults::apply(FloatButtonProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
}

void ConfigDefaults::apply(FloatButtonGroupProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
}

void ConfigDefaults::apply(DividerProps& props) {
    if (!props.size_) {
        props.size_ = config_size(Prop<ControlSize>{ControlSize::Large}, false);
    }
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(BreadcrumbProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TextProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(TypographyProps& props) {
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(DrawerProps& props) {
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(TimelineProps& props) {
    props.rtl_ = config_rtl(props.rtl_, props.explicit_rtl_);
}

void ConfigDefaults::apply(FlexProps& props) {
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(RowProps& props) {
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(ButtonProps& props) {
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
}

void ConfigDefaults::apply(InputPropsData& props) {
    if (props.config_resolved_) {
        return;
    }
    props.config_resolved_ = true;
    props.size_ = config_size(props.size_, props.explicit_size_);
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(InputProps& props) {
    apply(props.common_);
}

void ConfigDefaults::apply(InputNumberProps& props) {
    apply(props.input_);
}

void ConfigDefaults::apply(TextAreaProps& props) {
    apply(props.common_);
}

void ConfigDefaults::apply(PasswordProps& props) {
    apply(props.common_);
}

void ConfigDefaults::apply(SwitchProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
    props.size_ = config_default(props.size_, props.explicit_size_, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_size
                   ? std::optional{*value.explicit_size == ControlSize::Small ? SwitchSize::Small : SwitchSize::Middle}
                   : std::nullopt;
    });
}

void ConfigDefaults::apply(CheckboxProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    if (props.direction_) {
        props.direction_ = config_binary_direction(*props.direction_, true);
    }
    // An unset member direction belongs to its Group. The mount path supplies
    // a Provider fallback only for a standalone member.
}

void ConfigDefaults::apply(CheckboxGroupProps& props) {
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
}

void ConfigDefaults::apply(RadioProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    if (props.direction_) {
        props.direction_ = config_binary_direction(*props.direction_, true);
    }
}

void ConfigDefaults::apply(RadioGroupProps& props) {
    props.direction_ = config_binary_direction(props.direction_, props.explicit_direction_);
    props.size_ = config_control_size(props.size_, props.explicit_size_);
}

void ConfigDefaults::apply(TitleProps& props) {
    apply(props.typography_);
}

void ConfigDefaults::apply(SearchProps& props) {
    apply(props.common_);
}

void ConfigDefaults::apply(TableOptions& props) {
    props.size = config_size(props.size, props.explicit_size);
    props.direction = config_layout_direction(props.direction, props.explicit_direction);
}

void ConfigDefaults::apply(ListyOptions& props) {
    props.rtl = config_rtl(props.rtl, props.explicit_rtl);
    props.disabled = config_disabled(props.disabled, props.explicit_disabled);
}

void ConfigDefaults::apply(MasonryOptions& props) {
    props.direction = config_binary_direction(props.direction, props.explicit_direction);
}

void ConfigDefaults::apply(SliderProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    const auto rtl = config_rtl(Prop<bool>{false}, false);
    props.reverse_ = bind([reverse = props.reverse_, orientation = props.orientation_, rtl] {
        return read_prop(reverse) != (read_prop(orientation) == SliderOrientation::Horizontal && read_prop(rtl));
    });
}

void ConfigDefaults::apply(RangeSliderProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    const auto rtl = config_rtl(Prop<bool>{false}, false);
    props.reverse_ = bind([reverse = props.reverse_, orientation = props.orientation_, rtl] {
        return read_prop(reverse) != (read_prop(orientation) == SliderOrientation::Horizontal && read_prop(rtl));
    });
}

void ConfigDefaults::apply(MultiSliderProps& props) {
    props.disabled_ = config_disabled(props.disabled_, props.explicit_disabled_);
    const auto rtl = config_rtl(Prop<bool>{false}, false);
    props.reverse_ = bind([reverse = props.reverse_, orientation = props.orientation_, rtl] {
        return read_prop(reverse) != (read_prop(orientation) == SliderOrientation::Horizontal && read_prop(rtl));
    });
}

} // namespace ryn::detail
