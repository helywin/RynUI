#pragma once
#include <ryn/form.hpp>
#include <ryn/upload.hpp>
#include <stdexcept>

namespace ryn {
// Form stores an application domain value. Mapping preserves actual file metadata/handles in the application.
template <FormFieldType T> struct UploadFormMapping final {
    std::function<UploadFiles(const T&)> decode;
    std::function<T(const UploadFiles&)> encode;
};

// Obtain inside the FormItem body, so onChange belongs to the exact field registration.
template <FormFieldType T>
UploadProps upload_form_props(const FormField<T>& field, UploadFormMapping<T> mapping, UploadProps props = {}) {
    if (!mapping.decode || !mapping.encode) {
        throw std::invalid_argument("Upload Form mapping requires both typed directions");
    }
    Signal<std::uint64_t> revision{0};
    const auto counter = std::make_shared<std::uint64_t>(0);
    return props
        .fileList(bind([field, revision, decode = std::move(mapping.decode)] {
            static_cast<void>(revision.get());
            return decode(field.get());
        }))
        .onChange([change = field.onChange(), revision, counter,
                   encode = std::move(mapping.encode)](const UploadChange& event) {
            change(encode(event.file_list));
            revision.set(++*counter);
        })
        .disabled(field.disabled())
        .readOnly(field.readOnly())
        .status(field.status());
}
} // namespace ryn
