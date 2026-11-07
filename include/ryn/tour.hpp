#pragma once
#include <ryn/component.hpp>
#include <ryn/component_locales.hpp>
#include <ryn/text_direction.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class TourComponentHost;
struct TourTargetState;
} // namespace detail

// Copyable reference to one native retained subtree. Binding is window and
// component generation specific; a retired binding never resolves by raw id.
class TourTargetRef final {
public:
    TourTargetRef();
    [[nodiscard]] bool bound() const;
    friend bool operator==(const TourTargetRef&, const TourTargetRef&) = default;

private:
    friend class detail::TourComponentHost;
    std::shared_ptr<detail::TourTargetState> state_;
};

struct TourTargetSlot final {};

struct TourTitleSlot final {};

struct TourDescriptionSlot final {};

struct TourCoverSlot final {};

using TourTargetContent = SlotContent<TourTargetSlot>;
using TourTitle = SlotContent<TourTitleSlot>;
using TourDescription = SlotContent<TourDescriptionSlot>;
using TourCover = SlotContent<TourCoverSlot>;

enum class TourPlacement { Top, Bottom, Start, End, Center };
enum class TourCloseReason { CloseButton, Escape, Finish };

class TourStep final {
public:
    TourStep(String key, String title) : key_(std::move(key)), title_(std::move(title)) {}

    TourStep& target(TourTargetRef value) {
        target_ = std::move(value);
        return *this;
    }

    TourStep& title(Prop<String> value) {
        title_ = std::move(value);
        touch();
        return *this;
    }

    TourStep& description(Prop<String> value) {
        description_ = std::move(value);
        touch();
        return *this;
    }

    template <std::size_t N> TourStep& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    TourStep& richTitle(TourTitle value) {
        title_slot_ = std::make_shared<const TourTitle>(std::move(value));
        touch();
        return *this;
    }

    TourStep& richDescription(TourDescription value) {
        description_slot_ = std::make_shared<const TourDescription>(std::move(value));
        touch();
        return *this;
    }

    TourStep& cover(TourCover value) {
        cover_ = std::make_shared<const TourCover>(std::move(value));
        touch();
        return *this;
    }

    TourStep& placement(TourPlacement value) {
        placement_ = value;
        return *this;
    }

    friend bool operator==(const TourStep& a, const TourStep& b) {
        return a.key_ == b.key_ && a.revision_ == b.revision_ && a.target_ == b.target_ && a.placement_ == b.placement_;
    }

private:
    friend class detail::TourComponentHost;

    void touch() {
        revision_ = std::make_shared<const int>(0);
    }

    String key_;
    Prop<String> title_;
    Prop<String> description_{String{}};
    std::optional<TourTargetRef> target_;
    TourPlacement placement_{TourPlacement::Bottom};
    std::shared_ptr<const TourTitle> title_slot_;
    std::shared_ptr<const TourDescription> description_slot_;
    std::shared_ptr<const TourCover> cover_;
    std::shared_ptr<const int> revision_{std::make_shared<const int>(0)};
};

class TourProps final {
public:
    TourProps& steps(Prop<std::vector<TourStep>> value) {
        steps_ = std::move(value);
        return *this;
    }

    TourProps& open(Prop<bool> value = true) {
        open_ = std::move(value);
        return *this;
    }

    TourProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    TourProps& current(Prop<std::size_t> value) {
        current_ = std::move(value);
        return *this;
    }

    TourProps& defaultCurrent(std::size_t value) {
        default_current_ = value;
        return *this;
    }

    TourProps& onChange(std::function<void(std::size_t)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    TourProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    TourProps& onClose(std::function<void(TourCloseReason)> value) {
        on_close_ = std::move(value);
        return *this;
    }

    TourProps& keyboard(Prop<bool> value) {
        keyboard_ = std::move(value);
        return *this;
    }

    TourProps& mask(Prop<bool> value) {
        mask_ = std::move(value);
        return *this;
    }

    TourProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    TourProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    TourProps& locale(Prop<TourLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

    TourProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TourComponentHost;
    Prop<std::vector<TourStep>> steps_{std::vector<TourStep>{}};
    std::optional<Prop<bool>> open_;
    bool default_open_{};
    std::optional<Prop<std::size_t>> current_;
    std::size_t default_current_{};
    Prop<bool> keyboard_{true};
    Prop<bool> mask_{true};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    std::optional<Prop<TourLocale>> locale_;
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    std::function<void(std::size_t)> on_change_;
    std::function<void(bool)> on_open_;
    std::function<void(TourCloseReason)> on_close_;
};

void TourTarget(TourTargetRef, TourTargetContent);
void Tour(TourProps);
} // namespace ryn
