#pragma once
#include <ryn/component.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <memory>
#include <optional>
#include <functional>
#include <cstdint>

namespace ryn {
enum class NoticeStatus : std::uint8_t { Success, Info, Warning, Error, Loading };
enum class NotificationPlacement : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

namespace detail {
class NoticeComponentHost;
struct NoticeRefState;

struct NoticeOptions final {
    std::optional<Prop<String>> title;
    std::optional<Prop<String>> description;
    std::optional<Prop<NoticeStatus>> status;
    std::optional<Prop<NotificationPlacement>> placement;
    std::optional<Prop<double>> duration;
    std::optional<Prop<bool>> closable;
    std::optional<std::function<void()>> on_close;
};
} // namespace detail

struct MessageContentSlot final {};

using MessageContent = SlotContent<MessageContentSlot>;

struct MessageSlots final {
    std::optional<MessageContent> content;
};

struct NotificationTitleSlot final {};

struct NotificationDescriptionSlot final {};

struct NotificationActionsSlot final {};

using NotificationTitle = SlotContent<NotificationTitleSlot>;
using NotificationDescription = SlotContent<NotificationDescriptionSlot>;
using NotificationActions = SlotContent<NotificationActionsSlot>;

struct NotificationSlots final {
    std::optional<NotificationTitle> title;
    std::optional<NotificationDescription> description;
    std::optional<NotificationActions> actions;
};

class MessageProps final {
public:
    MessageProps& content(Prop<String> value) {
        options_.title = std::move(value);
        return *this;
    }

    template <std::size_t N> MessageProps& content(const char8_t (&value)[N]) {
        return content(String{value});
    }

    MessageProps& status(Prop<NoticeStatus> value) {
        options_.status = std::move(value);
        return *this;
    }

    MessageProps& duration(Prop<double> value) {
        options_.duration = std::move(value);
        return *this;
    }

    MessageProps& onClose(std::function<void()> value) {
        options_.on_close = std::move(value);
        return *this;
    }

private:
    friend class MessageRef;
    detail::NoticeOptions options_;
};

class MessageRef final {
public:
    MessageRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool open(String key, MessageProps props, MessageSlots slots = {}) const;
    [[nodiscard]] bool close(String key) const;
    [[nodiscard]] bool clear() const;

private:
    friend class MessageHolderProps;
    std::shared_ptr<detail::NoticeRefState> state_;
};

class MessageHolderProps final {
public:
    MessageHolderProps& ref(const MessageRef& value) {
        reference_ = value.state_;
        return *this;
    }

    MessageHolderProps& maxCount(Prop<std::size_t> value) {
        max_count_ = std::move(value);
        return *this;
    }

private:
    friend class detail::NoticeComponentHost;
    std::shared_ptr<detail::NoticeRefState> reference_;
    Prop<std::size_t> max_count_{std::size_t{8}};
};

void MessageHolder(MessageHolderProps props);

class NotificationProps final {
public:
    NotificationProps& title(Prop<String> value) {
        options_.title = std::move(value);
        return *this;
    }

    template <std::size_t N> NotificationProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    NotificationProps& description(Prop<String> value) {
        options_.description = std::move(value);
        return *this;
    }

    template <std::size_t N> NotificationProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    NotificationProps& status(Prop<NoticeStatus> value) {
        options_.status = std::move(value);
        return *this;
    }

    NotificationProps& placement(Prop<NotificationPlacement> value) {
        options_.placement = std::move(value);
        return *this;
    }

    NotificationProps& duration(Prop<double> value) {
        options_.duration = std::move(value);
        return *this;
    }

    NotificationProps& closable(Prop<bool> value) {
        options_.closable = std::move(value);
        return *this;
    }

    NotificationProps& onClose(std::function<void()> value) {
        options_.on_close = std::move(value);
        return *this;
    }

private:
    friend class NotificationRef;
    detail::NoticeOptions options_;
};

class NotificationRef final {
public:
    NotificationRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool open(String key, NotificationProps props, NotificationSlots slots = {}) const;
    [[nodiscard]] bool close(String key) const;
    [[nodiscard]] bool clear() const;

private:
    friend class NotificationHolderProps;
    std::shared_ptr<detail::NoticeRefState> state_;
};

class NotificationHolderProps final {
public:
    NotificationHolderProps& ref(const NotificationRef& value) {
        reference_ = value.state_;
        return *this;
    }

    NotificationHolderProps& maxCount(Prop<std::size_t> value) {
        max_count_ = std::move(value);
        return *this;
    }

private:
    friend class detail::NoticeComponentHost;
    std::shared_ptr<detail::NoticeRefState> reference_;
    Prop<std::size_t> max_count_{std::size_t{8}};
};

void NotificationHolder(NotificationHolderProps props);
} // namespace ryn
