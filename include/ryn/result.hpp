#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/status_types.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <memory>
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

struct ResultTitleSlot final {};

struct ResultSubtitleSlot final {};

using ResultIcon = SlotContent<ResultIconSlot>;
using ResultContent = SlotContent<ResultContentSlot>;
using ResultExtra = SlotContent<ResultExtraSlot>;
using ResultTitle = SlotContent<ResultTitleSlot>;
using ResultSubtitle = SlotContent<ResultSubtitleSlot>;

struct ResultSlots final {
    std::optional<ResultIcon> icon;
    std::optional<ResultContent> content;
    std::optional<ResultExtra> extra;
    std::optional<ResultTitle> title;
    std::optional<ResultSubtitle> subtitle;
};

class ResultProps final {
public:
    ResultProps& iconRender(Prop<std::shared_ptr<const ResultIcon>> value) {
        icon_ = std::move(value);
        return *this;
    }

    ResultProps& titleRender(Prop<std::shared_ptr<const ResultTitle>> value) {
        title_slot_ = std::move(value);
        return *this;
    }

    ResultProps& subtitleRender(Prop<std::shared_ptr<const ResultSubtitle>> value) {
        subtitle_slot_ = std::move(value);
        return *this;
    }

    ResultProps& extraRender(Prop<std::shared_ptr<const ResultExtra>> value) {
        extra_ = std::move(value);
        return *this;
    }

    ResultProps& contentRender(Prop<std::shared_ptr<const ResultContent>> value) {
        content_ = std::move(value);
        return *this;
    }

    ResultProps& animated(Prop<bool> value) {
        animated_ = std::move(value);
        return *this;
    }

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
    std::optional<Prop<std::shared_ptr<const ResultIcon>>> icon_;
    std::optional<Prop<std::shared_ptr<const ResultTitle>>> title_slot_;
    std::optional<Prop<std::shared_ptr<const ResultSubtitle>>> subtitle_slot_;
    std::optional<Prop<std::shared_ptr<const ResultExtra>>> extra_;
    std::optional<Prop<std::shared_ptr<const ResultContent>>> content_;
    Prop<bool> animated_{true};
    LayoutStyle layout_;
};

void Result(ResultProps props = {}, ResultSlots slots = {});
} // namespace ryn
