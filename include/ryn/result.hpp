#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/status_types.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <variant>

namespace ryn {
namespace detail {
class EmptyResultComponentHost;
}

enum class ResultException { Forbidden = 403, NotFound = 404, ServerError = 500 };
using ResultStatus = std::variant<Status, ResultException>;

struct ResultIconSlot final {};

struct ResultContentSlot final {};

struct ResultExtraSlot final {};

using ResultIcon = SlotContent<ResultIconSlot>;
using ResultContent = SlotContent<ResultContentSlot>;
using ResultExtra = SlotContent<ResultExtraSlot>;

struct ResultSlots final {
    std::optional<ResultIcon> icon;
    std::optional<ResultContent> content;
    std::optional<ResultExtra> extra;
};

class ResultProps final {
public:
    ResultProps& status(Prop<ResultStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    ResultProps& status(Status value) {
        return status(ResultStatus{value});
    }

    ResultProps& status(ResultException value) {
        return status(ResultStatus{value});
    }

    ResultProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> ResultProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    ResultProps& subtitle(Prop<String> value) {
        subtitle_ = std::move(value);
        return *this;
    }

    template <std::size_t N> ResultProps& subtitle(const char8_t (&value)[N]) {
        return subtitle(String{value});
    }

    ResultProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::EmptyResultComponentHost;
    Prop<ResultStatus> status_{ResultStatus{Status::Default}};
    Prop<String> title_{String{}};
    Prop<String> subtitle_{String{}};
    LayoutStyle layout_;
};

void Result(ResultProps props = {}, ResultSlots slots = {});
} // namespace ryn
