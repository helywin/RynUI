#pragma once
#include <ryn/component.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <memory>
#include <optional>
#include <functional>
#include <cstdint>
#include <future>

namespace ryn {
class AppRef;
class MessageProps;
class NotificationProps;
struct MessageSlots;
struct NotificationSlots;
enum class NoticeStatus : std::uint8_t { Success, Info, Warning, Error, Loading };
enum class NotificationPlacement : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };
enum class NoticeCloseResult : std::uint8_t { Closed, Cancelled, Failed };

struct NoticeStack final {
    bool enabled{false};
    std::size_t threshold{3};
    friend bool operator==(NoticeStack, NoticeStack) = default;
};

namespace detail {
class NoticeComponentHost;
struct NoticeRefState;
struct NoticeCompletion;

struct NoticeOptions final {
    std::optional<Prop<String>> title;
    std::optional<Prop<String>> description;
    std::optional<Prop<NoticeStatus>> status;
    std::optional<Prop<NotificationPlacement>> placement;
    std::optional<Prop<double>> duration;
    std::optional<Prop<bool>> closable;
    std::optional<Prop<bool>> pause_on_hover;
    std::optional<Prop<bool>> show_progress;
    std::optional<Prop<bool>> motion;
    std::optional<std::function<void()>> on_close;
    std::shared_ptr<NoticeCompletion> completion;
};
} // namespace detail

// The future is safe to observe on another thread. UI requests stay on the
// holder's owner thread; destroying that holder resolves pending work Cancelled.
class NoticeHandle final {
public:
    NoticeHandle();
    [[nodiscard]] std::shared_future<NoticeCloseResult> closed() const;
    [[nodiscard]] bool close() const;
    [[nodiscard]] bool update(MessageProps props) const;
    [[nodiscard]] bool update(NotificationProps props) const;
    [[nodiscard]] bool update(MessageProps props, MessageSlots slots) const;
    [[nodiscard]] bool update(NotificationProps props, NotificationSlots slots) const;

private:
    friend class MessageRef;
    friend class NotificationRef;
    std::shared_ptr<detail::NoticeCompletion> state_;
};

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

    MessageProps& pauseOnHover(Prop<bool> value = true) {
        options_.pause_on_hover = std::move(value);
        return *this;
    }

    MessageProps& showProgress(Prop<bool> value = true) {
        options_.show_progress = std::move(value);
        return *this;
    }

    MessageProps& motion(Prop<bool> value = true) {
        options_.motion = std::move(value);
        return *this;
    }

private:
    friend class MessageRef;
    friend class NoticeHandle;
    detail::NoticeOptions options_;
};

class MessageRef final {
public:
    MessageRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool open(String key, MessageProps props, MessageSlots slots = {}) const;
    [[nodiscard]] NoticeHandle openAsync(String key, MessageProps props, MessageSlots slots = {}) const;
    [[nodiscard]] bool close(String key) const;
    [[nodiscard]] bool clear() const;

private:
    friend class MessageHolderProps;
    friend class AppRef;
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

    MessageHolderProps& stack(Prop<NoticeStack> value) {
        stack_ = std::move(value);
        return *this;
    }

private:
    friend class detail::NoticeComponentHost;
    std::shared_ptr<detail::NoticeRefState> reference_;
    Prop<std::size_t> max_count_{std::size_t{8}};
    Prop<NoticeStack> stack_{NoticeStack{}};
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

    NotificationProps& pauseOnHover(Prop<bool> value = true) {
        options_.pause_on_hover = std::move(value);
        return *this;
    }

    NotificationProps& showProgress(Prop<bool> value = true) {
        options_.show_progress = std::move(value);
        return *this;
    }

    NotificationProps& motion(Prop<bool> value = true) {
        options_.motion = std::move(value);
        return *this;
    }

private:
    friend class NotificationRef;
    friend class NoticeHandle;
    detail::NoticeOptions options_;
};

class NotificationRef final {
public:
    NotificationRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool open(String key, NotificationProps props, NotificationSlots slots = {}) const;
    [[nodiscard]] NoticeHandle openAsync(String key, NotificationProps props, NotificationSlots slots = {}) const;
    [[nodiscard]] bool close(String key) const;
    [[nodiscard]] bool clear() const;

private:
    friend class NotificationHolderProps;
    friend class AppRef;
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

    NotificationHolderProps& stack(Prop<NoticeStack> value) {
        stack_ = std::move(value);
        return *this;
    }

private:
    friend class detail::NoticeComponentHost;
    std::shared_ptr<detail::NoticeRefState> reference_;
    Prop<std::size_t> max_count_{std::size_t{8}};
    Prop<NoticeStack> stack_{NoticeStack{}};
};

void NotificationHolder(NotificationHolderProps props);
} // namespace ryn
