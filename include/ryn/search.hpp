#pragma once

#include <ryn/input.hpp>

namespace ryn {
namespace detail {
struct SearchPropsAccess;
}

enum class SearchSource { Input };

class SearchProps final : public InputPropsBase<SearchProps> {
public:
    SearchProps& loading(Prop<bool> value) {
        loading_ = std::move(value);
        return *this;
    }

    SearchProps& enterButton(Prop<bool> value) {
        enter_button_ = std::move(value);
        return *this;
    }

    SearchProps& onSearch(std::function<void(String, SearchSource)> callback) {
        on_search_ = std::move(callback);
        return *this;
    }

private:
    friend struct detail::SearchPropsAccess;
    Prop<bool> loading_{false};
    Prop<bool> enter_button_{false};
    std::function<void(String, SearchSource)> on_search_;
};

struct SearchButtonContentSlot final {};

using SearchButtonContent = SlotContent<SearchButtonContentSlot>;

void Search(SearchProps props, std::optional<SearchButtonContent> button = {});

} // namespace ryn
