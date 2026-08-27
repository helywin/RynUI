#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace ryn {
namespace detail {

struct SlotContentAccess;

} // namespace detail

template <typename SlotTag>
class SlotContent final {
public:
    template <typename Function>
        requires(
            std::invocable<Function&>
            && std::same_as<std::invoke_result_t<Function&>, void>
            && !std::same_as<std::remove_cvref_t<Function>, SlotContent>)
    SlotContent(Function&& function)
        : function_(std::forward<Function>(function)) {}

private:
    friend struct detail::SlotContentAccess;

    std::function<void()> function_;
};

struct RootContentSlot final {};
using Content = SlotContent<RootContentSlot>;

} // namespace ryn
