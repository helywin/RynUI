#include <ryn/upload.hpp>
#include <ryn/form_upload.hpp>
#include <type_traits>

static_assert(std::is_aggregate_v<ryn::UploadFile> && std::is_aggregate_v<ryn::UploadLocale>);

ryn::UploadFile legacy_upload_aggregate() {
    return {u8"legacy", u8"file.txt", u8"text/plain",    4,  {}, ryn::UploadFileStatus::Error,
            0,          {},           u8"Upload failed", {}, {}};
}

void attachments(ryn::UploadTransport send) {
    ryn::Upload(
        ryn::UploadProps{}.accept(u8"image/png,image/jpeg,.bmp").multiple().maxCount(3).customRequest(std::move(send)));
}

void typed_attachment(const ryn::FormField<ryn::String>& field, ryn::UploadFormMapping<ryn::String> mapping,
                      const ryn::UploadRef& reference) {
    ryn::FormItem(ryn::FormItemProps{}.field(u8"attachment", field).control(reference),
                  ryn::FormItemContent{[=] { ryn::Upload(ryn::upload_form_props(field, mapping).ref(reference)); }});
}
