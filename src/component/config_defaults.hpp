#pragma once

#include "runtime/component_host.hpp"
#include "runtime/config_context.hpp"
#include "runtime/prop_connection.hpp"
#include "component/space_compact.hpp"
#include <ryn/selection_option_disabled.hpp>

namespace ryn {
class TitleProps;
class SearchProps;
class SliderProps;
class RangeSliderProps;
class MultiSliderProps;
class ButtonProps;
class InputProps;
class InputNumberProps;
class TextAreaProps;
class PasswordProps;
class SwitchProps;
class CheckboxProps;
class CheckboxGroupProps;
class RadioProps;
class RadioGroupProps;

class AutoCompleteProps;
class MentionsProps;
class SelectProps;
class CascaderProps;
class ColorPickerProps;
class DatePickerProps;
class DateRangePickerProps;
class DateTimeRangePickerProps;
class TimePickerProps;
class TimeRangePickerProps;
class TransferProps;
class TreeSelectProps;
class TreeProps;
class UploadProps;
class OTPProps;
class CalendarProps;
class CarouselProps;
class MenuProps;
class PaginationProps;
class TabsProps;
class StepsProps;
class SegmentedProps;
class RateProps;
class PopoverProps;
class TooltipProps;
class TourProps;
class TagProps;
class CheckableTagProps;
class TagGroupProps;
class BadgeProps;
class RibbonProps;
class ImageProps;
class PreviewGroupProps;
class AvatarProps;
class AvatarGroupProps;
class CardProps;
class CollapseProps;
class DescriptionsProps;
class SpinProps;
class SkeletonAvatarProps;
class SkeletonButtonProps;
class SkeletonInputProps;
class SpaceProps;
class SpaceCompactProps;
class SpaceAddonProps;
class FloatButtonProps;
class FloatButtonGroupProps;
class DividerProps;
class BreadcrumbProps;
class TextProps;
class TypographyProps;
class DrawerProps;
class TimelineProps;
class FlexProps;
class RowProps;

namespace detail {
struct InputPropsData;
struct TableOptions;
struct ListyOptions;
struct MasonryOptions;

struct ConfigDefaults final {
    static void apply(AutoCompleteProps&);
    static void apply(MentionsProps&);
    static void apply(SelectProps&);
    static void apply(CascaderProps&);
    static void apply(ColorPickerProps&);
    static void apply(DatePickerProps&);
    static void apply(DateRangePickerProps&);
    static void apply(DateTimeRangePickerProps&);
    static void apply(TimePickerProps&);
    static void apply(TimeRangePickerProps&);
    static void apply(TransferProps&);
    static void apply(TreeSelectProps&);
    static void apply(TreeProps&);
    static void apply(UploadProps&);
    static void apply(OTPProps&);
    static void apply(CalendarProps&);
    static void apply(CarouselProps&);
    static void apply(MenuProps&);
    static void apply(PaginationProps&);
    static void apply(TabsProps&);
    static void apply(StepsProps&);
    static void apply(SegmentedProps&);
    static void apply(RateProps&);
    static void apply(PopoverProps&);
    static void apply(TooltipProps&);
    static void apply(TourProps&);
    static void apply(TagProps&);
    static void apply(CheckableTagProps&);
    static void apply(TagGroupProps&);
    static void apply(BadgeProps&);
    static void apply(RibbonProps&);
    static void apply(ImageProps&);
    static void apply(PreviewGroupProps&);
    static void apply(AvatarProps&);
    static void apply(AvatarGroupProps&);
    static void apply(CardProps&);
    static void apply(CollapseProps&);
    static void apply(DescriptionsProps&);
    static void apply(SpinProps&);
    static void apply(SkeletonAvatarProps&);
    static void apply(SkeletonButtonProps&);
    static void apply(SkeletonInputProps&);
    static void apply(SpaceProps&);
    static void apply(SpaceCompactProps&);
    static void apply(SpaceAddonProps&);
    static void apply(FloatButtonProps&);
    static void apply(FloatButtonGroupProps&);
    static void apply(DividerProps&);
    static void apply(BreadcrumbProps&);
    static void apply(TextProps&);
    static void apply(TypographyProps&);
    static void apply(DrawerProps&);
    static void apply(TimelineProps&);
    static void apply(FlexProps&);
    static void apply(RowProps&);
    static void apply(TitleProps&);
    static void apply(SearchProps&);
    static void apply(SliderProps&);
    static void apply(RangeSliderProps&);
    static void apply(MultiSliderProps&);
    static void apply(TableOptions&);
    static void apply(ListyOptions&);
    static void apply(MasonryOptions&);
    static void apply(ButtonProps&);
    static void apply(InputPropsData&);
    static void apply(InputProps&);
    static void apply(InputNumberProps&);
    static void apply(TextAreaProps&);
    static void apply(PasswordProps&);
    static void apply(SwitchProps&);
    static void apply(CheckboxProps&);
    static void apply(CheckboxGroupProps&);
    static void apply(RadioProps&);
    static void apply(RadioGroupProps&);
};

// Preserve provenance in Props. A value equal to the old default is still an
// explicit override. Resolve before composing local Form/Group/Compact gates.
template <typename T, typename Project> Prop<T> config_default(Prop<T> local, bool explicit_value, Project project) {
    auto& build = runtime::require_component_build_context();
    const auto context = build.capture_context();
    if (explicit_value && PropAccess::static_value(local)) {
        return local;
    }
    return bind([local = std::move(local), explicit_value, project, context, host = &build.host()] {
        const auto live = [&] {
            return !context.owner.expired() && context.config->active() &&
                   (!context.component_owner || host->contains(*context.component_owner));
        };
        if (!live()) {
            throw I18nException({I18nErrorCode::StaleContext, {}, {}});
        }
        if (!explicit_value) {
            if (const auto inherited = project(context.config->snapshot())) {
                return *inherited;
            }
        }
        // A getter may retire its owner. Native prop sinks check their owner
        // again before applying the computed value; do not revisit its captured
        // context or turn normal retirement into a new exception here.
        return read_prop(local);
    });
}

inline Prop<ControlSize> config_size(Prop<ControlSize> local, bool explicit_value, bool compact_item = false) {
    if (!explicit_value && compact_item) {
        if (const auto compact = nearest_compact(runtime::require_component_build_context())) {
            return compact->size_source;
        }
    }
    return config_default(std::move(local), explicit_value,
                          [](const runtime::ConfigSnapshot& value) { return value.explicit_size; });
}

inline Prop<TextDirection> config_direction(Prop<TextDirection> local, bool explicit_value) {
    return config_default(std::move(local), explicit_value,
                          [](const runtime::ConfigSnapshot& value) { return value.explicit_direction; });
}

inline Prop<TextDirection> config_layout_direction(Prop<TextDirection> local, bool explicit_value) {
    return config_default(std::move(local), explicit_value, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_direction
                   ? std::optional{*value.explicit_direction == TextDirection::RightToLeft ? TextDirection::RightToLeft
                                                                                           : TextDirection::LeftToRight}
                   : std::nullopt;
    });
}

inline Prop<bool> config_disabled(Prop<bool> local, bool explicit_value) {
    return config_default(std::move(local), explicit_value,
                          [](const runtime::ConfigSnapshot& value) { return std::optional{value.disabled}; });
}

inline Prop<bool> config_selection_disabled(Prop<SelectionOptionDisabled> source) {
    return config_default(Prop<bool>{false}, false, [source](const runtime::ConfigSnapshot& value) {
        return std::optional{read_prop(source).valueOr(value.disabled)};
    });
}

inline Prop<bool> config_rtl(Prop<bool> local, bool explicit_value) {
    return config_default(std::move(local), explicit_value, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_direction ? std::optional{*value.explicit_direction == TextDirection::RightToLeft}
                                        : std::nullopt;
    });
}

template <typename T> Prop<T> config_binary_direction(Prop<T> local, bool explicit_value) {
    return config_default(std::move(local), explicit_value, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_direction
                   ? std::optional{*value.explicit_direction == TextDirection::RightToLeft ? T::RightToLeft
                                                                                           : T::LeftToRight}
                   : std::nullopt;
    });
}

template <typename T> Prop<T> config_control_size(Prop<T> local, bool explicit_value) {
    return config_default(std::move(local), explicit_value, [](const runtime::ConfigSnapshot& value) {
        return value.explicit_size ? std::optional{*value.explicit_size == ControlSize::Small   ? T::Small
                                                   : *value.explicit_size == ControlSize::Large ? T::Large
                                                                                                : T::Middle}
                                   : std::nullopt;
    });
}

} // namespace detail
} // namespace ryn
