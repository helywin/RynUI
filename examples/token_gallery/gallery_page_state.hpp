#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace rynui::example {

struct GalleryPageAnchor final {
    std::string identity;
    float distance{};
};

// Deliberately excludes component IDs, callbacks, Prop/Signal and renderer/editor handles.
using GalleryDraftValue = std::variant<std::string, bool, std::int64_t, double, std::vector<std::string>>;

struct GalleryPageDraft final {
    std::map<std::string, GalleryDraftValue> values;
    GalleryPageAnchor anchor;

    template <typename T> [[nodiscard]] T get(const std::string& key, T fallback) const {
        const auto found = values.find(key);
        return found == values.end() ? std::move(fallback) : std::get<T>(found->second);
    }
};

} // namespace rynui::example
