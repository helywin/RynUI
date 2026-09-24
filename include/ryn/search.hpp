#pragma once

#include <ryn/input.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail { struct SearchPropsAccess; }

enum class SearchSource { Input };

class SearchProps final {
public:
    SearchProps& value(Prop<String> value) { value_ = std::move(value); return *this; }
    template<std::size_t N> SearchProps& value(const char8_t (&value)[N]) { return this->value(String{value}); }
    SearchProps& defaultValue(String value) { default_value_ = std::move(value); return *this; }
    template<std::size_t N> SearchProps& defaultValue(const char8_t (&value)[N]) { return defaultValue(String{value}); }
    SearchProps& placeholder(Prop<String> value) { placeholder_ = std::move(value); return *this; }
    template<std::size_t N> SearchProps& placeholder(const char8_t (&value)[N]) { return placeholder(String{value}); }
    SearchProps& size(Prop<ControlSize> value) { size_ = std::move(value); return *this; }
    SearchProps& status(Prop<InputStatus> value) { status_ = std::move(value); return *this; }
    SearchProps& disabled(Prop<bool> value) { disabled_ = std::move(value); return *this; }
    SearchProps& readOnly(Prop<bool> value) { read_only_ = std::move(value); return *this; }
    SearchProps& maxLength(Prop<std::size_t> value) { max_length_ = std::move(value); return *this; }
    SearchProps& loading(Prop<bool> value) { loading_ = std::move(value); return *this; }
    SearchProps& enterButton(Prop<bool> value) { enter_button_ = std::move(value); return *this; }
    SearchProps& onChange(std::function<void(String)> callback) { on_change_ = std::move(callback); return *this; }
    SearchProps& onSearch(std::function<void(String, SearchSource)> callback) { on_search_ = std::move(callback); return *this; }
    SearchProps& layout(LayoutStyle value) { layout_ = std::move(value); return *this; }
private:
    friend struct detail::SearchPropsAccess;
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<String> placeholder_{String{}};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<bool> disabled_{false}, read_only_{false}, loading_{false}, enter_button_{false};
    std::optional<Prop<std::size_t>> max_length_;
    std::function<void(String)> on_change_;
    std::function<void(String, SearchSource)> on_search_;
    LayoutStyle layout_;
};

struct SearchButtonContentSlot final {};
using SearchButtonContent = SlotContent<SearchButtonContentSlot>;

void Search(SearchProps props, std::optional<SearchButtonContent> button = {});

} // namespace ryn
