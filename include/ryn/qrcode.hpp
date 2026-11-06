#pragma once
#include <ryn/component.hpp>
#include <ryn/image.hpp>
#include <ryn/qrcode_data.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
namespace detail {
class QRCodeComponentHost;
struct QRCodeRefState;
} // namespace detail
enum class QRCodeStatus { Active, Expired, Loading, Scanned };

struct QRCodeIconSize final {
    float width{40};
    float height{40};
    friend bool operator==(QRCodeIconSize, QRCodeIconSize) = default;
};

struct QRCodeLocale final {
    String expired{u8"QR code expired"};
    String refresh{u8"Refresh"};
    String loading{u8"Loading"};
    String scanned{u8"Scanned"};
    friend bool operator==(const QRCodeLocale&, const QRCodeLocale&) = default;
};
enum class QRCodeErrorCode { Encoding, Paint, Logo };

struct QRCodeError final {
    QRCodeErrorCode code{};
    std::string message;
    friend bool operator==(const QRCodeError&, const QRCodeError&) = default;
};

struct QRCodeStatusContext final {
    Signal<QRCodeStatus> status;
    Signal<QRCodeLocale> locale;
    Signal<bool> disabled;
    std::function<bool()> refresh;
};

struct QRCodeStatusSlot final {};

using QRCodeStatusContent = SlotContent<QRCodeStatusSlot>;

struct QRCodeStatusRenderer final {
    std::function<QRCodeStatusContent(const QRCodeStatusContext&)> render;
};

class QRCodeRef final {
public:
    QRCodeRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool refresh() const;
    [[nodiscard]] std::optional<QRCodeData> data() const;
    [[nodiscard]] std::vector<std::byte> exportPng(std::uint32_t pixel_size) const;
    [[nodiscard]] std::string exportSvg() const;

private:
    friend class detail::QRCodeComponentHost;
    std::shared_ptr<detail::QRCodeRefState> state_;
};

class QRCodeProps final {
public:
    QRCodeProps& value(Prop<QRCodeValue> v) {
        value_ = std::move(v);
        return *this;
    }

    QRCodeProps& errorLevel(Prop<QRCodeErrorLevel> v) {
        errorLevel_ = std::move(v);
        return *this;
    }

    QRCodeProps& boostLevel(Prop<bool> v) {
        boostLevel_ = std::move(v);
        return *this;
    }

    QRCodeProps& minVersion(Prop<int> v) {
        minVersion_ = std::move(v);
        return *this;
    }

    QRCodeProps& marginSize(Prop<unsigned> v) {
        marginSize_ = std::move(v);
        return *this;
    }

    QRCodeProps& size(Prop<float> v) {
        size_ = std::move(v);
        return *this;
    }

    QRCodeProps& status(Prop<QRCodeStatus> v) {
        status_ = std::move(v);
        return *this;
    }

    QRCodeProps& bordered(Prop<bool> v) {
        bordered_ = std::move(v);
        return *this;
    }

    QRCodeProps& icon(Prop<ImageSource> v) {
        icon_ = std::move(v);
        return *this;
    }

    QRCodeProps& iconSize(Prop<QRCodeIconSize> v) {
        iconSize_ = std::move(v);
        return *this;
    }

    QRCodeProps& locale(Prop<QRCodeLocale> v) {
        locale_ = std::move(v);
        return *this;
    }

    QRCodeProps& visible(Prop<bool> v) {
        visible_ = std::move(v);
        return *this;
    }

    QRCodeProps& disabled(Prop<bool> v) {
        disabled_ = std::move(v);
        return *this;
    }

    template <std::size_t N> QRCodeProps& value(const char8_t (&v)[N]) {
        return value(QRCodeValue{v});
    }

    QRCodeProps& color(Prop<Color> v) {
        color_ = std::move(v);
        return *this;
    }

    QRCodeProps& bgColor(Prop<Color> v) {
        background_ = std::move(v);
        return *this;
    }

    QRCodeProps& statusRender(std::shared_ptr<const QRCodeStatusRenderer> v) {
        return statusRender(Prop<std::shared_ptr<const QRCodeStatusRenderer>>{std::move(v)});
    }

    QRCodeProps& statusRender(Prop<std::shared_ptr<const QRCodeStatusRenderer>> v) {
        renderer_ = std::move(v);
        return *this;
    }

    QRCodeProps& onRefresh(std::function<void()> v) {
        on_refresh_ = std::move(v);
        return *this;
    }

    QRCodeProps& onError(std::function<void(const QRCodeError&)> v) {
        on_error_ = std::move(v);
        return *this;
    }

    QRCodeProps& onIconLoad(std::function<void()> v) {
        on_icon_load_ = std::move(v);
        return *this;
    }

    QRCodeProps& ref(QRCodeRef v) {
        ref_ = std::move(v);
        return *this;
    }

    QRCodeProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::QRCodeComponentHost;
    Prop<QRCodeValue> value_{QRCodeValue{}};
    Prop<QRCodeErrorLevel> errorLevel_{QRCodeErrorLevel::M};
    Prop<bool> boostLevel_{true};
    Prop<int> minVersion_{1};
    Prop<unsigned> marginSize_{4};
    Prop<float> size_{160};
    Prop<QRCodeStatus> status_{QRCodeStatus::Active};
    Prop<bool> bordered_{true};
    Prop<ImageSource> icon_{ImageSource{}};
    Prop<QRCodeIconSize> iconSize_{QRCodeIconSize{}};
    Prop<QRCodeLocale> locale_{QRCodeLocale{}};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    std::optional<Prop<Color>> color_;
    std::optional<Prop<Color>> background_;
    Prop<std::shared_ptr<const QRCodeStatusRenderer>> renderer_{std::shared_ptr<const QRCodeStatusRenderer>{}};
    std::function<void()> on_refresh_;
    std::function<void(const QRCodeError&)> on_error_;
    std::function<void()> on_icon_load_;
    std::optional<QRCodeRef> ref_;
    LayoutStyle layout_;
};

void QRCode(QRCodeProps props);
} // namespace ryn
