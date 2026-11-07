#pragma once

#include <ryn/i18n.hpp>

#include <span>
#include <string_view>

namespace rynui::example {

// Application-owned declarations, shared by the shell, retained pages and tests.
// No locale or window is stored here; ConfigProvider owns the live configuration.
struct GalleryMessageDefinition final {
    ryn::MessageKey key;
    ryn::MessagePattern chinese;
};

struct GalleryMessage final {
    GalleryMessage(std::string_view identity, ryn::MessageArgs arguments = {});
    ryn::MessageKey key;
    ryn::MessageArgs args;
    friend bool operator==(const GalleryMessage&, const GalleryMessage&) = default;
};

// Gallery event feedback stores its source, rather than a translated snapshot.
// Explicit business String values remain literal. The live handle is mount-owned.
class GalleryFeedback final {
public:
    explicit GalleryFeedback(GalleryMessage initial) : value_(std::move(initial)) {}

    GalleryFeedback(ryn::String literal, ryn::I18n context)
        : value_(std::move(literal)), context_(std::move(context)) {}

    GalleryFeedback(GalleryMessage initial, ryn::I18n context)
        : value_(std::move(initial)), context_(std::move(context)) {}

    void context(ryn::I18n context);
    bool set(GalleryMessage message) const;
    bool set(ryn::String literal) const;

    template <std::size_t N> bool set(const char8_t (&literal)[N]) const {
        return set(ryn::String{literal});
    }

    [[nodiscard]] ryn::String get() const;
    [[nodiscard]] bool is(std::string_view identity) const;
    [[nodiscard]] std::variant<ryn::String, GalleryMessage> source() const;
    bool restore(std::variant<ryn::String, GalleryMessage> source) const;
    [[nodiscard]] operator ryn::Prop<ryn::String>() const;

private:
    ryn::Signal<std::variant<ryn::String, GalleryMessage>> value_;
    ryn::I18n context_;
};

[[nodiscard]] std::span<const GalleryMessageDefinition> gallery_messages();
[[nodiscard]] const ryn::MessageKey& gallery_message(std::string_view identity);
[[nodiscard]] const ryn::MessageCatalog& gallery_message_catalog();
[[nodiscard]] ryn::Binding<ryn::String> gallery_text(std::string_view identity,
                                                     ryn::Prop<ryn::MessageArgs> args = ryn::MessageArgs{});
void gallery_label(std::string_view identity);
[[nodiscard]] ryn::Binding<ryn::String> gallery_field(std::string_view identity, std::string_view field);

} // namespace rynui::example
