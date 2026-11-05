#include <ryn/upload.hpp>
#include <ryn/form_upload.hpp>

void attachments(ryn::UploadTransport send) {
    ryn::Upload(
        ryn::UploadProps{}.accept(u8"image/png,image/jpeg,.bmp").multiple().maxCount(3).customRequest(std::move(send)));
}

void typed_attachment(const ryn::FormField<ryn::String>& field, ryn::UploadFormMapping<ryn::String> mapping,
                      const ryn::UploadRef& reference) {
    ryn::FormItem(ryn::FormItemProps{}.field(u8"attachment", field).control(reference),
                  ryn::FormItemContent{[=] { ryn::Upload(ryn::upload_form_props(field, mapping).ref(reference)); }});
}
