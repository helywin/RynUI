#pragma once
#include <ryn/component_locales.hpp>
#include <ryn/component.hpp>
#include <ryn/image_source.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <variant>

namespace ryn {
namespace detail {
class ImageComponentHost;
struct ImageRefState;
} // namespace detail

using ImageCancel = std::function<void()>;
using ImageCompletion = std::function<bool(ImageDecodeResult)>;

// Application IO/worker boundary. Complete on the Runtime owner thread (or post there).
// A false completion rejects foreign-thread, duplicate, cancelled or stale results.
struct ImageLoader final {
    std::function<ImageCancel(ImageCompletion)> start;
};

struct ImageSource final {
    using Value = std::variant<std::monostate, std::shared_ptr<const ImagePixels>, std::shared_ptr<const EncodedImage>,
                               std::shared_ptr<const ImageLoader>>;
    Value value;
    ImageSource() = default;

    ImageSource(std::shared_ptr<const ImagePixels> v) : value(std::move(v)) {}

    ImageSource(std::shared_ptr<const EncodedImage> v) : value(std::move(v)) {}

    ImageSource(std::shared_ptr<const ImageLoader> v) : value(std::move(v)) {}

    friend bool operator==(const ImageSource&, const ImageSource&) = default;
};
enum class ImageShape { Rectangle, Circle, RoundedRectangle };
enum class ImageFit { Contain, Cover, Fill, None, ScaleDown };
enum class ImageStatus { Empty, Loading, Ready, Error, Fallback };
enum class ImageAction { ZoomIn, ZoomOut, RotateLeft, RotateRight, FlipX, FlipY, Reset, Previous, Next, Drag, Close };

struct ImageTransform final {
    float scale{1};
    float rotate{};
    float x{};
    float y{};
    bool flip_x{};
    bool flip_y{};
    friend bool operator==(ImageTransform, ImageTransform) = default;
};

struct ImagePreviewOptions final {
    bool enabled{true};
    bool wheel{true};
    bool movable{true};
    bool keyboard{true};
    bool mask_closable{true};
    bool focus_trap{true};
    float min_scale{1};
    float max_scale{50};
    float scale_step{.5F};
    friend bool operator==(ImagePreviewOptions, ImagePreviewOptions) = default;
};

struct ImagePreviewActions final {
    std::function<void(ImageAction)> invoke;
    Signal<ImageTransform> transform;
    Signal<std::size_t> current;
    Signal<std::size_t> total;
};

struct ImagePlaceholderSlot final {};

struct ImageErrorSlot final {};

struct ImageCoverSlot final {};

using ImagePlaceholder = SlotContent<ImagePlaceholderSlot>;
using ImageErrorContent = SlotContent<ImageErrorSlot>;
using ImageCover = SlotContent<ImageCoverSlot>;

struct ImageActionsRenderer final {
    std::function<void(const ImagePreviewActions&)> render;
};

struct ImageSlots final {
    std::optional<ImagePlaceholder> placeholder;
    std::optional<ImageErrorContent> error;
    std::optional<ImageCover> cover;
};

class ImageRef final {
public:
    ImageRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool preview(bool open = true) const;
    [[nodiscard]] bool action(ImageAction) const;

private:
    friend class detail::ImageComponentHost;
    std::shared_ptr<detail::ImageRefState> state_;
};

class ImageProps final {
public:
    ImageProps& source(Prop<ImageSource> v) {
        source_ = std::move(v);
        return *this;
    }

    ImageProps& previewSource(Prop<ImageSource> v) {
        preview_source_ = std::move(v);
        return *this;
    }

    ImageProps& fallback(Prop<ImageSource> v) {
        fallback_ = std::move(v);
        return *this;
    }

    ImageProps& alt(Prop<String> v) {
        alt_ = std::move(v);
        return *this;
    }

    template <std::size_t N> ImageProps& alt(const char8_t (&v)[N]) {
        return alt(String{v});
    }

    ImageProps& shape(Prop<ImageShape> v) {
        shape_ = std::move(v);
        return *this;
    }

    ImageProps& fit(Prop<ImageFit> v) {
        fit_ = std::move(v);
        return *this;
    }

    ImageProps& preview(Prop<ImagePreviewOptions> v) {
        preview_ = std::move(v);
        return *this;
    }

    ImageProps& preview(bool v) {
        ImagePreviewOptions o;
        o.enabled = v;
        return preview(o);
    }

    ImageProps& open(Prop<bool> v) {
        open_ = std::move(v);
        return *this;
    }

    ImageProps& defaultOpen(bool v) {
        default_open_ = v;
        return *this;
    }

    ImageProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    ImageProps& visible(Prop<bool> v = true) {
        visible_ = std::move(v);
        return *this;
    }

    ImageProps& rtl(Prop<bool> v = true) {
        rtl_ = std::move(v);
        return *this;
    }

    ImageProps& locale(Prop<ImageLocale> v) {
        locale_ = std::move(v);
        return *this;
    }

    ImageProps& actionsRender(Prop<std::shared_ptr<const ImageActionsRenderer>> v) {
        actions_ = std::move(v);
        return *this;
    }

    ImageProps& ref(ImageRef v) {
        ref_ = std::move(v);
        return *this;
    }

    ImageProps& onOpenChange(std::function<void(bool)> v) {
        on_open_ = std::move(v);
        return *this;
    }

    ImageProps& onLoad(std::function<void()> v) {
        on_load_ = std::move(v);
        return *this;
    }

    ImageProps& onError(std::function<void(const ImageError&)> v) {
        on_error_ = std::move(v);
        return *this;
    }

    ImageProps& onTransform(std::function<void(ImageTransform, ImageAction)> v) {
        on_transform_ = std::move(v);
        return *this;
    }

    ImageProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::ImageComponentHost;
    Prop<ImageSource> source_{ImageSource{}};
    Prop<ImageSource> preview_source_{ImageSource{}};
    Prop<ImageSource> fallback_{ImageSource{}};
    std::optional<Prop<String>> alt_;
    // PreviewGroup's legacy aggregate uses an empty alt as an omitted label.
    bool inherit_empty_alt_{};
    Prop<ImageShape> shape_{ImageShape::Rectangle};
    Prop<ImageFit> fit_{ImageFit::Contain};
    Prop<ImagePreviewOptions> preview_{ImagePreviewOptions{}};
    std::optional<Prop<bool>> open_;
    bool default_open_{};
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    Prop<bool> rtl_{false};
    std::optional<Prop<ImageLocale>> locale_;
    Prop<std::shared_ptr<const ImageActionsRenderer>> actions_{std::shared_ptr<const ImageActionsRenderer>{}};
    std::optional<ImageRef> ref_;
    std::function<void(bool)> on_open_;
    std::function<void()> on_load_;
    std::function<void(const ImageError&)> on_error_;
    std::function<void(ImageTransform, ImageAction)> on_transform_;
    LayoutStyle layout_;
};

struct ImagePreviewItem final {
    String key;
    ImageSource source;
    String alt;
    friend bool operator==(const ImagePreviewItem&, const ImagePreviewItem&) = default;
};

class PreviewGroupProps final {
public:
    PreviewGroupProps& thumbnails(Prop<bool> v = true) {
        thumbnails_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& items(Prop<std::vector<ImagePreviewItem>> v) {
        items_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& fallback(Prop<ImageSource> v) {
        fallback_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& current(Prop<std::size_t> v) {
        current_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& defaultCurrent(std::size_t v) {
        default_current_ = v;
        return *this;
    }

    PreviewGroupProps& open(Prop<bool> v) {
        open_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& defaultOpen(bool v) {
        default_open_ = v;
        return *this;
    }

    PreviewGroupProps& preview(Prop<ImagePreviewOptions> v) {
        preview_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& visible(Prop<bool> v = true) {
        visible_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& rtl(Prop<bool> v = true) {
        rtl_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& locale(Prop<ImageLocale> v) {
        locale_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& actionsRender(Prop<std::shared_ptr<const ImageActionsRenderer>> v) {
        actions_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& onOpenChange(std::function<void(bool, std::size_t)> v) {
        on_open_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& onChange(std::function<void(std::size_t, std::size_t)> v) {
        on_change_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& onTransform(std::function<void(ImageTransform, ImageAction)> v) {
        on_transform_ = std::move(v);
        return *this;
    }

    PreviewGroupProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::ImageComponentHost;
    Prop<bool> thumbnails_{true};
    Prop<std::vector<ImagePreviewItem>> items_{std::vector<ImagePreviewItem>{}};
    Prop<ImageSource> fallback_{ImageSource{}};
    std::optional<Prop<std::size_t>> current_;
    std::optional<Prop<bool>> open_;
    std::size_t default_current_{};
    bool default_open_{};
    Prop<ImagePreviewOptions> preview_{ImagePreviewOptions{}};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    Prop<bool> rtl_{false};
    std::optional<Prop<ImageLocale>> locale_;
    Prop<std::shared_ptr<const ImageActionsRenderer>> actions_{std::shared_ptr<const ImageActionsRenderer>{}};
    std::function<void(bool, std::size_t)> on_open_;
    std::function<void(std::size_t, std::size_t)> on_change_;
    std::function<void(ImageTransform, ImageAction)> on_transform_;
    LayoutStyle layout_;
};

void Image(ImageProps props, ImageSlots slots = {});
void PreviewGroup(PreviewGroupProps props);
} // namespace ryn
