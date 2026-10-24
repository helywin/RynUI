#include "component/descriptions_model.hpp"
#include <algorithm>
#include <stdexcept>

namespace ryn::detail {
DescriptionsRows pack_descriptions(int columns, std::span<const DescriptionsModelItem> items) {
    if (columns <= 0) {
        throw std::invalid_argument("Descriptions column must be positive");
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].key.empty() || items[i].span <= 0) {
            throw std::invalid_argument("Descriptions requires non-empty keys and positive spans");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (items[j].key == items[i].key) {
                throw std::invalid_argument("Descriptions keys must be unique");
            }
        }
    }
    DescriptionsRows result;
    int used = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (!items[i].visible) {
            continue;
        }
        if (used == 0) {
            result.emplace_back();
        }
        const int span = std::min(items[i].span, columns - used);
        result.back().push_back({i, used, span});
        used += span;
        if (used == columns) {
            used = 0;
        }
    }
    if (used != 0) {
        result.back().back().span += columns - used;
    }
    return result;
}
} // namespace ryn::detail
