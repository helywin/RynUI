#pragma once
#include <ryn/component_locales.hpp>
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/file_service.hpp>
#include <ryn/image.hpp>
#include <ryn/input_types.hpp>
#include <ryn/layout_style.hpp>
#include <functional>
#include <optional>

namespace ryn {
struct FormControlGeometry;
class UploadRef;

namespace detail {
class UploadComponentHost;
struct UploadRefState;
struct FormFocusContext;
std::function<bool(const FormFocusContext&)> capture_form_upload(const UploadRef&);
std::function<std::optional<FormControlGeometry>(const FormFocusContext&)>
capture_form_upload_geometry(const UploadRef&);
} // namespace detail

using UploadUid = String;
enum class UploadFileStatus { Ready, Uploading, Done, Error, Removed, Cancelled };
enum class UploadErrorSource { Unspecified, Builtin, Literal };

struct UploadFile final {
    UploadUid uid;
    String name;
    String type;
    std::uint64_t size{};
    std::shared_ptr<const NativeFile> origin;
    UploadFileStatus status{UploadFileStatus::Ready};
    float percent{};
    String response;
    String error;
    String url;
    ImageSource thumbnail;
    // Trailing source metadata preserves the existing aggregate prefix.
    UploadErrorSource error_source{UploadErrorSource::Unspecified};
    friend bool operator==(const UploadFile&, const UploadFile&) = default;
};

using UploadFiles = std::vector<UploadFile>;
enum class UploadChangeReason { Selection, Start, Progress, Success, Failure, Remove, Cancel };

struct UploadChange final {
    UploadFile file;
    UploadFiles file_list;
    UploadChangeReason reason;
};

struct UploadAcceptRule final {
    String mime_type;
    std::vector<String> extensions;
    friend bool operator==(const UploadAcceptRule&, const UploadAcceptRule&) = default;
};

using UploadAccept = std::vector<UploadAcceptRule>;
[[nodiscard]] UploadAccept upload_accept(StringView csv);
[[nodiscard]] bool upload_accepts(const UploadAccept&, const NativeFile&);
using UploadCancel = std::function<void()>;

struct UploadResult final {
    String response;
    String url;
};

struct UploadRequest final {
    UploadFile file;
    std::function<bool(float)> progress;
    std::function<bool(UploadResult)> success;
    std::function<bool(String)> failure;
};

using UploadTransport = std::function<UploadCancel(const UploadRequest&)>;
enum class UploadBeforeDecision { Upload, Keep, Ignore };

struct UploadBeforeResult final {
    UploadBeforeDecision decision{UploadBeforeDecision::Upload};
    std::shared_ptr<const NativeFile> transformed;
};

using UploadBeforeCompletion = std::function<bool(UploadBeforeResult)>;
using UploadBefore = std::function<UploadCancel(const UploadFile&, const UploadFiles&, UploadBeforeCompletion)>;
using UploadRemoveCompletion = std::function<bool(bool)>;
using UploadRemove = std::function<UploadCancel(const UploadFile&, UploadRemoveCompletion)>;
enum class UploadListType { Text, Picture, PictureCard, PictureCircle };
enum class UploadType { Select, Drag };

struct UploadItemActions final {
    std::function<void()> preview;
    std::function<void()> download;
    std::function<void()> remove;
    std::function<void()> retry;
    std::function<void()> cancel;
};

struct UploadItemRenderer final {
    std::function<void(Signal<UploadFile>, const UploadItemActions&)> render;
};

struct UploadIconRenderer final {
    std::function<void(Signal<UploadFile>, Signal<UploadListType>)> render;
};

struct UploadTriggerSlot final {};

struct UploadActionIconSlot final {};

using UploadActionIcon = SlotContent<UploadActionIconSlot>;
using UploadTrigger = SlotContent<UploadTriggerSlot>;

struct UploadSlots final {
    std::optional<UploadTrigger> trigger;
    std::optional<UploadActionIcon> preview_icon;
    std::optional<UploadActionIcon> download_icon;
    std::optional<UploadActionIcon> remove_icon;
    std::optional<UploadActionIcon> retry_icon;
    std::optional<UploadActionIcon> cancel_icon;
};

struct UploadListActions final {
    bool show_list{true};
    bool preview{true};
    bool download{true};
    bool remove{true};
    bool retry{true};
    friend bool operator==(UploadListActions, UploadListActions) = default;
};

class UploadRef final {
public:
    UploadRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool open() const;
    [[nodiscard]] bool select(NativeFiles) const;
    [[nodiscard]] bool retry(const UploadUid&) const;
    [[nodiscard]] bool abort(const UploadUid&) const;

private:
    friend class detail::UploadComponentHost;
    friend std::function<bool(const detail::FormFocusContext&)> detail::capture_form_upload(const UploadRef&);
    friend std::function<std::optional<FormControlGeometry>(const detail::FormFocusContext&)>
    detail::capture_form_upload_geometry(const UploadRef&);
    std::shared_ptr<detail::UploadRefState> state_;
};

class UploadProps final {
public:
    UploadProps& fileList(Prop<UploadFiles> v) {
        files_ = std::move(v);
        return *this;
    }

    UploadProps& defaultFileList(UploadFiles v) {
        default_files_ = std::move(v);
        return *this;
    }

    UploadProps& accept(Prop<UploadAccept> v) {
        accept_ = std::move(v);
        return *this;
    }

    UploadProps& accept(StringView v) {
        return accept(upload_accept(v));
    }

    template <std::size_t N> UploadProps& accept(const char8_t (&v)[N]) {
        return accept(String{v}.view());
    }

    UploadProps& multiple(Prop<bool> v = true) {
        multiple_ = std::move(v);
        return *this;
    }

    UploadProps& directory(Prop<bool> v = true) {
        directory_ = std::move(v);
        return *this;
    }

    UploadProps& pastable(Prop<bool> v = true) {
        pastable_ = std::move(v);
        return *this;
    }

    UploadProps& maxCount(Prop<std::optional<std::size_t>> v) {
        max_count_ = std::move(v);
        return *this;
    }

    UploadProps& maxCount(std::size_t v) {
        return maxCount(std::optional{v});
    }

    UploadProps& beforeUpload(UploadBefore v) {
        before_ = std::move(v);
        return *this;
    }

    UploadProps& beforeUpload(std::function<UploadBeforeResult(const UploadFile&, const UploadFiles&)> v) {
        before_ = [v = std::move(v)](const UploadFile& f, const UploadFiles& files, UploadBeforeCompletion done) {
            done(v(f, files));
            return UploadCancel{};
        };
        return *this;
    }

    UploadProps& transformFile(UploadBefore v) {
        transform_ = std::move(v);
        return *this;
    }

    UploadProps& customRequest(UploadTransport v) {
        transport_ = std::move(v);
        return *this;
    }

    UploadProps& onRemove(UploadRemove v) {
        remove_ = std::move(v);
        return *this;
    }

    UploadProps& onRemove(std::function<bool(const UploadFile&)> v) {
        remove_ = [v = std::move(v)](const UploadFile& f, UploadRemoveCompletion done) {
            done(v(f));
            return UploadCancel{};
        };
        return *this;
    }

    UploadProps& onChange(std::function<void(const UploadChange&)> v) {
        on_change_ = std::move(v);
        return *this;
    }

    UploadProps& onPreview(std::function<void(const UploadFile&)> v) {
        on_preview_ = std::move(v);
        return *this;
    }

    UploadProps& onDownload(std::function<void(const UploadFile&)> v) {
        on_download_ = std::move(v);
        return *this;
    }

    UploadProps& onDrop(std::function<void(const NativeFiles&)> v) {
        on_drop_ = std::move(v);
        return *this;
    }

    UploadProps& onSelectionError(std::function<void(FileServiceError, String)> v) {
        on_selection_error_ = std::move(v);
        return *this;
    }

    UploadProps& previewFile(std::function<ImageSource(const UploadFile&)> v) {
        preview_file_ = std::move(v);
        return *this;
    }

    UploadProps& isImageFile(std::function<bool(const UploadFile&)> v) {
        is_image_ = std::move(v);
        return *this;
    }

    UploadProps& itemRender(Prop<std::shared_ptr<const UploadItemRenderer>> v) {
        renderer_ = std::move(v);
        return *this;
    }

    UploadProps& iconRender(Prop<std::shared_ptr<const UploadIconRenderer>> v) {
        icon_renderer_ = std::move(v);
        return *this;
    }

    UploadProps& listType(Prop<UploadListType> v) {
        list_type_ = std::move(v);
        return *this;
    }

    UploadProps& type(Prop<UploadType> v) {
        type_ = std::move(v);
        return *this;
    }

    UploadProps& showUploadList(Prop<UploadListActions> v) {
        list_actions_ = std::move(v);
        return *this;
    }

    UploadProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    UploadProps& readOnly(Prop<bool> v = true) {
        read_only_ = std::move(v);
        return *this;
    }

    UploadProps& visible(Prop<bool> v = true) {
        visible_ = std::move(v);
        return *this;
    }

    UploadProps& rtl(Prop<bool> v = true) {
        rtl_ = std::move(v);
        return *this;
    }

    UploadProps& size(Prop<ControlSize> v) {
        size_ = std::move(v);
        return *this;
    }

    UploadProps& status(Prop<InputStatus> v) {
        status_ = std::move(v);
        return *this;
    }

    UploadProps& locale(Prop<UploadLocale> v) {
        locale_ = std::move(v);
        return *this;
    }

    UploadProps& openFileDialogOnClick(Prop<bool> v = true) {
        dialog_on_click_ = std::move(v);
        return *this;
    }

    UploadProps& ref(UploadRef v) {
        ref_ = std::move(v);
        return *this;
    }

    UploadProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::UploadComponentHost;
    std::optional<Prop<UploadFiles>> files_;
    UploadFiles default_files_;
    Prop<UploadAccept> accept_{UploadAccept{}};
    Prop<bool> multiple_{false};
    Prop<bool> directory_{false};
    Prop<bool> pastable_{false};
    Prop<std::optional<std::size_t>> max_count_{std::optional<std::size_t>{}};
    UploadBefore before_;
    UploadBefore transform_;
    UploadTransport transport_;
    UploadRemove remove_;
    std::function<void(const UploadChange&)> on_change_;
    std::function<void(const UploadFile&)> on_preview_;
    std::function<void(const UploadFile&)> on_download_;
    std::function<void(const NativeFiles&)> on_drop_;
    std::function<void(FileServiceError, String)> on_selection_error_;
    std::function<ImageSource(const UploadFile&)> preview_file_;
    std::function<bool(const UploadFile&)> is_image_;
    Prop<std::shared_ptr<const UploadItemRenderer>> renderer_{std::shared_ptr<const UploadItemRenderer>{}};
    Prop<std::shared_ptr<const UploadIconRenderer>> icon_renderer_{std::shared_ptr<const UploadIconRenderer>{}};
    Prop<UploadListType> list_type_{UploadListType::Text};
    Prop<UploadType> type_{UploadType::Select};
    Prop<UploadListActions> list_actions_{UploadListActions{}};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<bool> visible_{true};
    Prop<bool> rtl_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputStatus> status_{InputStatus::Default};
    std::optional<Prop<UploadLocale>> locale_;
    Prop<bool> dialog_on_click_{true};
    std::optional<UploadRef> ref_;
    LayoutStyle layout_;
};

void Upload(UploadProps props = {}, UploadSlots slots = {});
} // namespace ryn
