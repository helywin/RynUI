#pragma once

#include "gallery_page_state.hpp"

#include <ryn/rynui.hpp>

#include <charconv>
#include <concepts>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <system_error>
#include <type_traits>
#include <variant>
#include <vector>

namespace rynui::example {
namespace draft_detail {

template <typename T> struct Optional : std::false_type {};

template <typename T> struct Optional<std::optional<T>> : std::true_type {
    using Value = T;
};

template <typename T> struct Vector : std::false_type {};

template <typename T> struct Vector<std::vector<T>> : std::true_type {
    using Value = T;
};

template <typename T> struct Variant : std::false_type {};

template <typename... T> struct Variant<std::variant<T...>> : std::true_type {};

template <typename> inline constexpr bool unsupported = false;

} // namespace draft_detail

template <typename T> void gallery_draft_write(GalleryPageDraft& draft, const std::string& key, const T& value);
template <typename T> T gallery_draft_read(const GalleryPageDraft& draft, const std::string& key, const T& fallback);

namespace draft_detail {

template <typename T, std::size_t I = 0>
T variant_value(const GalleryPageDraft& draft, const std::string& key, std::size_t index) {
    if constexpr (I < std::variant_size_v<T>) {
        if (I == index) {
            using Value = std::variant_alternative_t<I, T>;
            return T{std::in_place_index<I>, gallery_draft_read(draft, key + "/value", Value{})};
        }
        return variant_value<T, I + 1>(draft, key, index);
    } else {
        throw std::invalid_argument("Gallery draft variant index is invalid");
    }
}

template <typename Tuple, std::size_t... I>
void write_fields(GalleryPageDraft& draft, const std::string& key, const Tuple& fields, std::index_sequence<I...>) {
    (gallery_draft_write(draft, key + "/" + std::to_string(I), std::get<I>(fields)), ...);
}

template <typename Tuple, std::size_t... I>
void read_fields(const GalleryPageDraft& draft, const std::string& key, Tuple fields, std::index_sequence<I...>) {
    ((std::get<I>(fields) = gallery_draft_read(draft, key + "/" + std::to_string(I), std::get<I>(fields))), ...);
}

} // namespace draft_detail

// These codecs copy resolved business values only. They never serialize Prop,
// Signal, refs, callbacks, component/editor IDs, or renderer objects.
template <typename T> void gallery_draft_write(GalleryPageDraft& draft, const std::string& key, const T& value) {
    if constexpr (std::same_as<T, ryn::String>) {
        draft.values[key] = std::string{value.bytes()};
    } else if constexpr (std::same_as<T, bool>) {
        draft.values[key] = value;
    } else if constexpr (std::same_as<T, ryn::SelectionOptionDisabled>) {
        // Keep the original bool slot and distinguish unset from explicit false.
        draft.values[key] = static_cast<bool>(value);
        draft.values[key + "/explicit"] = value.explicitValue().has_value();
    } else if constexpr (std::is_enum_v<T>) {
        draft.values[key] = static_cast<std::int64_t>(value);
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
        draft.values[key] = static_cast<std::int64_t>(value);
    } else if constexpr (std::is_integral_v<T>) {
        draft.values[key] = std::to_string(value);
    } else if constexpr (std::is_floating_point_v<T>) {
        draft.values[key] = static_cast<double>(value);
    } else if constexpr (draft_detail::Optional<T>::value) {
        draft.values[key + "/present"] = value.has_value();
        if (value) {
            gallery_draft_write(draft, key + "/value", *value);
        }
    } else if constexpr (draft_detail::Vector<T>::value) {
        gallery_draft_write(draft, key + "/size", value.size());
        for (std::size_t i = 0; i < value.size(); ++i) {
            gallery_draft_write(draft, key + "/" + std::to_string(i), value[i]);
        }
    } else if constexpr (std::same_as<T, ryn::Responsive<float>>) {
        gallery_draft_write(draft, key + "/fallback", value.fallback());
        for (std::size_t index = 0; index < value.overrides().size(); ++index) {
            gallery_draft_write(draft, key + "/breakpoint/" + std::to_string(index), value.overrides()[index]);
        }
    } else if constexpr (draft_detail::Variant<T>::value) {
        draft.values[key + "/index"] = static_cast<std::int64_t>(value.index());
        std::visit([&](const auto& item) { gallery_draft_write(draft, key + "/value", item); }, value);
    } else if constexpr (std::same_as<T, ryn::SelectValue> || std::same_as<T, ryn::StatisticValue>) {
        gallery_draft_write(draft, key + "/storage", value.storage());
    } else if constexpr (std::same_as<T, ryn::CivilDate>) {
        gallery_draft_write(draft, key + "/year", value.year());
        gallery_draft_write(draft, key + "/month", value.month());
        gallery_draft_write(draft, key + "/day", value.day());
    } else if constexpr (std::same_as<T, ryn::TimeOfDay>) {
        gallery_draft_write(draft, key + "/hour", value.hour());
        gallery_draft_write(draft, key + "/minute", value.minute());
        gallery_draft_write(draft, key + "/second", value.second());
    } else if constexpr (std::same_as<T, ryn::SliderRange>) {
        gallery_draft_write(draft, key + "/lower", value.lower);
        gallery_draft_write(draft, key + "/upper", value.upper);
    } else if constexpr (std::same_as<T, ryn::TableSort>) {
        gallery_draft_write(draft, key + "/column", value.column_key);
        gallery_draft_write(draft, key + "/order", value.order);
    } else if constexpr (std::same_as<T, ryn::SelectOption> || std::same_as<T, ryn::CheckboxOption> ||
                         std::same_as<T, ryn::RadioOption>) {
        gallery_draft_write(draft, key + "/value", value.value);
        gallery_draft_write(draft, key + "/label", value.label);
        gallery_draft_write(draft, key + "/disabled", value.disabled);
    } else if constexpr (requires { gallery_draft_fields(value); }) {
        const auto fields = gallery_draft_fields(value);
        draft_detail::write_fields(draft, key, fields, std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});
    } else {
        static_assert(draft_detail::unsupported<T>, "Gallery draft requires an explicit plain-value codec");
    }
}

template <typename T> T gallery_draft_read(const GalleryPageDraft& draft, const std::string& key, const T& fallback) {
    if constexpr (std::same_as<T, ryn::String>) {
        return ryn::String::from_utf8(draft.get<std::string>(key, std::string{fallback.bytes()})).value();
    } else if constexpr (std::same_as<T, bool>) {
        return draft.get<bool>(key, fallback);
    } else if constexpr (std::same_as<T, ryn::SelectionOptionDisabled>) {
        if (!draft.values.contains(key)) {
            return fallback;
        }
        const auto value = draft.get<bool>(key, static_cast<bool>(fallback));
        // An untagged legacy bool represents an explicit stored value.
        return draft.get<bool>(key + "/explicit", true) ? T{value} : T{};
    } else if constexpr (std::is_enum_v<T>) {
        return static_cast<T>(draft.get<std::int64_t>(key, static_cast<std::int64_t>(fallback)));
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
        const auto value = draft.get<std::int64_t>(key, static_cast<std::int64_t>(fallback));
        if (value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max()) {
            throw std::out_of_range("Gallery draft integer is out of range");
        }
        return static_cast<T>(value);
    } else if constexpr (std::is_integral_v<T>) {
        const auto text = draft.get<std::string>(key, std::to_string(fallback));
        T result{};
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            throw std::invalid_argument("Gallery draft unsigned integer is invalid");
        }
        return result;
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(draft.get<double>(key, static_cast<double>(fallback)));
    } else if constexpr (draft_detail::Optional<T>::value) {
        const auto found = draft.values.find(key + "/present");
        if (found == draft.values.end()) {
            return fallback;
        }
        if (!std::get<bool>(found->second)) {
            return std::nullopt;
        }
        using Value = typename draft_detail::Optional<T>::Value;
        return T{gallery_draft_read(draft, key + "/value", fallback.value_or(Value{}))};
    } else if constexpr (draft_detail::Vector<T>::value) {
        if (!draft.values.contains(key + "/size")) {
            return fallback;
        }
        const auto size = gallery_draft_read(draft, key + "/size", std::size_t{});
        T result;
        result.reserve(size);
        using Value = typename draft_detail::Vector<T>::Value;
        for (std::size_t i = 0; i < size; ++i) {
            result.push_back(gallery_draft_read(draft, key + "/" + std::to_string(i), Value{}));
        }
        return result;
    } else if constexpr (std::same_as<T, ryn::Responsive<float>>) {
        T result{gallery_draft_read(draft, key + "/fallback", fallback.fallback())};
        for (std::size_t index = 0; index < fallback.overrides().size(); ++index) {
            const auto value =
                gallery_draft_read(draft, key + "/breakpoint/" + std::to_string(index), fallback.overrides()[index]);
            if (value) {
                result.at(static_cast<ryn::Breakpoint>(index), *value);
            }
        }
        return result;
    } else if constexpr (draft_detail::Variant<T>::value) {
        if (!draft.values.contains(key + "/index")) {
            return fallback;
        }
        const auto index = draft.get<std::int64_t>(key + "/index", -1);
        if (index < 0) {
            throw std::invalid_argument("Gallery draft variant index is invalid");
        }
        return draft_detail::variant_value<T>(draft, key, static_cast<std::size_t>(index));
    } else if constexpr (std::same_as<T, ryn::SelectValue> || std::same_as<T, ryn::StatisticValue>) {
        const auto storage = gallery_draft_read(draft, key + "/storage", fallback.storage());
        return std::visit([](const auto& value) { return T{value}; }, storage);
    } else if constexpr (std::same_as<T, ryn::CivilDate>) {
        return T{gallery_draft_read(draft, key + "/year", fallback.year()),
                 gallery_draft_read(draft, key + "/month", fallback.month()),
                 gallery_draft_read(draft, key + "/day", fallback.day())};
    } else if constexpr (std::same_as<T, ryn::TimeOfDay>) {
        return T{gallery_draft_read(draft, key + "/hour", fallback.hour()),
                 gallery_draft_read(draft, key + "/minute", fallback.minute()),
                 gallery_draft_read(draft, key + "/second", fallback.second())};
    } else if constexpr (std::same_as<T, ryn::SliderRange>) {
        return T{gallery_draft_read(draft, key + "/lower", fallback.lower),
                 gallery_draft_read(draft, key + "/upper", fallback.upper)};
    } else if constexpr (std::same_as<T, ryn::TableSort>) {
        return T{gallery_draft_read(draft, key + "/column", fallback.column_key),
                 gallery_draft_read(draft, key + "/order", fallback.order)};
    } else if constexpr (std::same_as<T, ryn::SelectOption> || std::same_as<T, ryn::CheckboxOption> ||
                         std::same_as<T, ryn::RadioOption>) {
        return T{gallery_draft_read(draft, key + "/value", fallback.value),
                 gallery_draft_read(draft, key + "/label", fallback.label),
                 gallery_draft_read(draft, key + "/disabled", fallback.disabled)};
    } else if constexpr (requires(T value) { gallery_draft_fields(value); }) {
        auto result = fallback;
        auto fields = gallery_draft_fields(result);
        draft_detail::read_fields(draft, key, fields, std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});
        return result;
    } else {
        static_assert(draft_detail::unsupported<T>, "Gallery draft requires an explicit plain-value codec");
    }
}

} // namespace rynui::example
