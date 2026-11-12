#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rynui::example {

// Internal, bounded file protocol. This is never part of Core or the public API.
struct GalleryDemoJson final {
    using Array = std::vector<GalleryDemoJson>;
    using Object = std::map<std::string, GalleryDemoJson, std::less<>>;
    std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object> value{nullptr};

    GalleryDemoJson() = default;

    GalleryDemoJson(std::nullptr_t) : value(nullptr) {}

    GalleryDemoJson(bool item) : value(item) {}

    GalleryDemoJson(int item) : value(std::int64_t{item}) {}

    GalleryDemoJson(std::int64_t item) : value(item) {}

    GalleryDemoJson(double item) : value(item) {}

    GalleryDemoJson(const char* item) : value(std::string{item}) {}

    GalleryDemoJson(std::string item) : value(std::move(item)) {}

    GalleryDemoJson(std::string_view item) : value(std::string{item}) {}

    GalleryDemoJson(Array item) : value(std::move(item)) {}

    GalleryDemoJson(Object item) : value(std::move(item)) {}

    [[nodiscard]] const GalleryDemoJson& at(std::string_view key) const;
    [[nodiscard]] GalleryDemoJson& at(std::string_view key);
    [[nodiscard]] const Object& object() const;
    [[nodiscard]] const Array& array() const;
    [[nodiscard]] const std::string& string() const;
    [[nodiscard]] double number() const;
    [[nodiscard]] std::int64_t integer() const;
    [[nodiscard]] bool boolean() const;
    [[nodiscard]] bool contains(std::string_view key) const;
    [[nodiscard]] std::string dump() const;
    static GalleryDemoJson parse(std::string_view input);
};

void gallery_demo_fields(const GalleryDemoJson&, std::initializer_list<std::string_view> required);

} // namespace rynui::example
