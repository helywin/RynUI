#pragma once
#include <ryn/string.hpp>
#include <cstddef>
#include <span>
#include <vector>

namespace ryn::detail {
struct DescriptionsModelItem final {
    String key;
    int span{1};
    bool visible{true};
};

struct DescriptionsCell final {
    std::size_t item{};
    int column{};
    int span{};
    friend bool operator==(const DescriptionsCell&, const DescriptionsCell&) = default;
};

using DescriptionsRows = std::vector<std::vector<DescriptionsCell>>;
DescriptionsRows pack_descriptions(int columns, std::span<const DescriptionsModelItem> items);
} // namespace ryn::detail
