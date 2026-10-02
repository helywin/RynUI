#include "font/icon_vector_font.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ryn::font {
namespace {
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t byte_limit = 2 * 1024 * 1024;

void u16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

void u32(Bytes& bytes, std::uint32_t value) {
    u16(bytes, static_cast<std::uint16_t>(value >> 16));
    u16(bytes, static_cast<std::uint16_t>(value));
}

void patch16(Bytes& bytes, std::size_t offset, std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value >> 8);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value);
}

void patch32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    patch16(bytes, offset, static_cast<std::uint16_t>(value >> 16));
    patch16(bytes, offset + 2, static_cast<std::uint16_t>(value));
}

void append(Bytes& bytes, const Bytes& more) {
    if (more.size() > byte_limit || bytes.size() > byte_limit - more.size()) {
        throw std::invalid_argument("Icon vector exceeds the 2 MiB font limit");
    }
    bytes.insert(bytes.end(), more.begin(), more.end());
}

Bytes ascii(std::string_view text) {
    return {text.begin(), text.end()};
}

Bytes index(const std::vector<Bytes>& records) {
    Bytes result;
    u16(result, static_cast<std::uint16_t>(records.size()));
    if (records.empty()) {
        return result;
    }
    result.push_back(4);
    std::uint32_t offset = 1;
    u32(result, offset);
    for (const auto& record : records) {
        offset += static_cast<std::uint32_t>(record.size());
        u32(result, offset);
    }
    for (const auto& record : records) {
        append(result, record);
    }
    return result;
}

void dict_integer(Bytes& bytes, std::int32_t value) {
    bytes.push_back(29);
    u32(bytes, static_cast<std::uint32_t>(value));
}

void char_integer(Bytes& bytes, std::int32_t value) {
    if (value >= -107 && value <= 107) {
        bytes.push_back(static_cast<std::uint8_t>(value + 139));
    } else if (value >= 108 && value <= 1131) {
        const auto offset = value - 108;
        bytes.push_back(static_cast<std::uint8_t>(247 + offset / 256));
        bytes.push_back(static_cast<std::uint8_t>(offset % 256));
    } else if (value >= -1131 && value <= -108) {
        const auto offset = -value - 108;
        bytes.push_back(static_cast<std::uint8_t>(251 + offset / 256));
        bytes.push_back(static_cast<std::uint8_t>(offset % 256));
    } else {
        bytes.push_back(28);
        u16(bytes, static_cast<std::uint16_t>(value));
    }
}

void char_fixed(Bytes& bytes, std::int64_t value) {
    if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument("Icon vector exceeds the Type2 coordinate range");
    }
    if (value % 65536 == 0) {
        char_integer(bytes, static_cast<std::int32_t>(value / 65536));
    } else {
        bytes.push_back(255);
        u32(bytes, static_cast<std::uint32_t>(value));
    }
}

void matrix_scale(Bytes& bytes) {
    // Exact DICT decimal 1/1024. The CFF default is 1/1000.
    constexpr std::array<std::uint8_t, 7> scale{30, 0x0A, 0x00, 0x09, 0x76, 0x56, 0x25};
    bytes.insert(bytes.end(), scale.begin(), scale.end());
    bytes.push_back(0xFF);
}

struct Point final {
    std::int64_t x{};
    std::int64_t y{};
};

struct Geometry final {
    IconViewBox box;
    double scale;
    double pad_x;
    double pad_y;
    double left{};
    double bottom{-128};
    double right{1024};
    double top{896};

    explicit Geometry(IconViewBox value) : box(value) {
        if (!std::isfinite(box.x) || !std::isfinite(box.y) || !std::isfinite(box.width) || !std::isfinite(box.height) ||
            box.width <= 0 || box.height <= 0) {
            throw std::invalid_argument("Icon vector viewBox must be finite and positive");
        }
        scale = 1024.0 / std::max(box.width, box.height);
        pad_x = (1024 - static_cast<double>(box.width) * scale) / 2;
        pad_y = (1024 - static_cast<double>(box.height) * scale) / 2;
    }

    Point fixed(double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y) || x < -32768 || x > 32767 || y < -32768 || y > 32767) {
            throw std::invalid_argument("Icon vector coordinate cannot be represented in the font");
        }
        left = std::min(left, x);
        right = std::max(right, x);
        bottom = std::min(bottom, y);
        top = std::max(top, y);
        return {static_cast<std::int64_t>(std::llround(x * 65536)), static_cast<std::int64_t>(std::llround(y * 65536))};
    }

    Point map(IconPoint point) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            throw std::invalid_argument("Icon vector path coordinates must be finite");
        }
        return fixed((static_cast<double>(point.x) - box.x) * scale + pad_x,
                     896 - ((static_cast<double>(point.y) - box.y) * scale + pad_y));
    }
};

void line(Bytes& bytes, Point from, Point to) {
    char_fixed(bytes, to.x - from.x);
    char_fixed(bytes, to.y - from.y);
    bytes.push_back(5);
}

void cubic(Bytes& bytes, Point from, Point control1, Point control2, Point to) {
    char_fixed(bytes, control1.x - from.x);
    char_fixed(bytes, control1.y - from.y);
    char_fixed(bytes, control2.x - control1.x);
    char_fixed(bytes, control2.y - control1.y);
    char_fixed(bytes, to.x - control2.x);
    char_fixed(bytes, to.y - control2.y);
    bytes.push_back(8);
}

Bytes encode_path(const IconPath& path, Geometry& geometry, std::vector<Bytes>& subroutines) {
    if ((path.color != IconColorRole::Primary && path.color != IconColorRole::Secondary) ||
        !std::isfinite(path.opacity) || path.opacity < 0 || path.opacity > 1) {
        throw std::invalid_argument("Icon vector color role or opacity is invalid");
    }
    Bytes main;
    Bytes chunk;
    Point current;
    Point start;
    bool active{};
    bool drawn{};
    std::size_t contours{};
    auto flush = [&] {
        if (chunk.empty()) {
            return;
        }
        // Bounded chunks avoid the Type2 65535-byte charstring limit even
        // for one contour containing all 4096 cubic commands.
        chunk.push_back(11); // return
        char_integer(main, static_cast<std::int32_t>(subroutines.size()) - 107);
        main.push_back(29); // callgsubr (bias 107 below 1240 subroutines)
        subroutines.push_back(std::move(chunk));
        chunk = {};
    };
    for (const auto& command : path.commands) {
        Bytes encoded;
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, IconMove>) {
                    if (active) {
                        throw std::invalid_argument("Icon vector contour must close before Move");
                    }
                    const auto next = geometry.map(value.to);
                    char_fixed(encoded, next.x - current.x);
                    char_fixed(encoded, next.y - current.y);
                    encoded.push_back(21); // rmoveto
                    current = next;
                    start = next;
                    active = true;
                    drawn = false;
                } else {
                    if (!active) {
                        throw std::invalid_argument("Icon vector command requires an open contour");
                    }
                    if constexpr (std::is_same_v<T, IconClose>) {
                        if (!drawn) {
                            throw std::invalid_argument("Icon vector contour has no segments");
                        }
                        line(encoded, current, start);
                        current = start;
                        active = false;
                        ++contours;
                    } else if constexpr (std::is_same_v<T, IconLine>) {
                        const auto next = geometry.map(value.to);
                        line(encoded, current, next);
                        current = next;
                        drawn = true;
                    } else if constexpr (std::is_same_v<T, IconCubic>) {
                        const auto first = geometry.map(value.control1);
                        const auto second = geometry.map(value.control2);
                        const auto next = geometry.map(value.to);
                        cubic(encoded, current, first, second, next);
                        current = next;
                        drawn = true;
                    } else if constexpr (std::is_same_v<T, IconQuadratic>) {
                        const auto control = geometry.map(value.control);
                        const auto next = geometry.map(value.to);
                        const auto first = geometry.fixed((current.x + (control.x - current.x) * (2.0 / 3)) / 65536,
                                                          (current.y + (control.y - current.y) * (2.0 / 3)) / 65536);
                        const auto second = geometry.fixed((next.x + (control.x - next.x) * (2.0 / 3)) / 65536,
                                                           (next.y + (control.y - next.y) * (2.0 / 3)) / 65536);
                        cubic(encoded, current, first, second, next);
                        current = next;
                        drawn = true;
                    }
                }
            },
            command);
        if (chunk.size() + encoded.size() > 4000) {
            flush();
        }
        append(chunk, encoded);
    }
    if (active || contours == 0) {
        throw std::invalid_argument("Icon vector paths require complete closed contours");
    }
    flush();
    main.push_back(14); // endchar; width comes from Private defaultWidthX
    return main;
}

Bytes cff(Geometry& geometry, std::span<const IconPath> paths) {
    std::vector<Bytes> subroutines;
    std::vector<Bytes> glyphs{{14}};
    std::vector<Bytes> names;
    Bytes charset{0};
    for (std::size_t number = 0; number < paths.size(); ++number) {
        glyphs.push_back(encode_path(paths[number], geometry, subroutines));
        names.push_back(ascii("layer" + std::to_string(number)));
        u16(charset, static_cast<std::uint16_t>(391 + number));
    }
    if (subroutines.size() >= 1240) {
        throw std::invalid_argument("Icon vector exceeds the bounded subroutine limit");
    }
    Bytes private_dict;
    dict_integer(private_dict, 1024);
    private_dict.push_back(20); // defaultWidthX
    auto top_dict = [&](std::int32_t charset_offset, std::int32_t strings_offset, std::int32_t private_offset) {
        Bytes top;
        for (const auto value : {geometry.left, geometry.bottom, geometry.right, geometry.top}) {
            dict_integer(top, static_cast<std::int32_t>(value < 0 ? std::floor(value) : std::ceil(value)));
        }
        top.push_back(5); // FontBBox
        matrix_scale(top);
        dict_integer(top, 0);
        dict_integer(top, 0);
        matrix_scale(top);
        dict_integer(top, 0);
        dict_integer(top, 0);
        top.push_back(12);
        top.push_back(7); // FontMatrix
        dict_integer(top, charset_offset);
        top.push_back(15);
        dict_integer(top, strings_offset);
        top.push_back(17); // CharStrings
        dict_integer(top, static_cast<std::int32_t>(private_dict.size()));
        dict_integer(top, private_offset);
        top.push_back(18);
        return top;
    };
    const auto name_index = index({ascii("RynUIVector")});
    const auto string_index = index(names);
    const auto subr_index = index(subroutines);
    const auto glyph_index = index(glyphs);
    const auto top_index = index({top_dict(0, 0, 0)});
    const auto charset_offset =
        static_cast<std::int32_t>(4 + name_index.size() + top_index.size() + string_index.size() + subr_index.size());
    const auto strings_offset = charset_offset + static_cast<std::int32_t>(charset.size());
    const auto private_offset = strings_offset + static_cast<std::int32_t>(glyph_index.size());
    Bytes result{1, 0, 4, 4};
    append(result, name_index);
    append(result, index({top_dict(charset_offset, strings_offset, private_offset)}));
    append(result, string_index);
    append(result, subr_index);
    append(result, charset);
    append(result, glyph_index);
    append(result, private_dict);
    return result;
}

std::uint32_t checksum(const Bytes& bytes) {
    std::uint32_t sum{};
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        std::uint32_t word{};
        for (std::size_t index = 0; index < 4; ++index) {
            word = (word << 8) | (offset + index < bytes.size() ? bytes[offset + index] : 0);
        }
        sum += word;
    }
    return sum;
}

Bytes font_name() {
    constexpr std::string_view family = "RynUIVector";
    constexpr std::array<std::uint16_t, 5> ids{1, 2, 3, 4, 6};
    Bytes result;
    u16(result, 0);
    u16(result, static_cast<std::uint16_t>(ids.size()));
    u16(result, static_cast<std::uint16_t>(6 + ids.size() * 12));
    Bytes strings;
    for (const auto id : ids) {
        const auto text = id == 2 ? std::string_view{"Regular"} : family;
        u16(result, 3);
        u16(result, 1);
        u16(result, 0x409);
        u16(result, id);
        u16(result, static_cast<std::uint16_t>(text.size() * 2));
        u16(result, static_cast<std::uint16_t>(strings.size()));
        for (const auto character : text) {
            u16(strings, static_cast<std::uint16_t>(character));
        }
    }
    append(result, strings);
    return result;
}

struct Table final {
    std::string_view tag;
    Bytes bytes;
};
} // namespace

std::vector<std::byte> build_icon_vector_font(IconViewBox box, std::span<const IconPath> paths) {
    if (paths.empty() || paths.size() > 64) {
        throw std::invalid_argument("Icon vector requires 1-64 paths");
    }
    std::size_t command_count{};
    for (const auto& path : paths) {
        if (path.commands.size() > 4096 - command_count) {
            throw std::invalid_argument("Icon vector exceeds 4096 commands");
        }
        command_count += path.commands.size();
    }
    Geometry geometry{box};
    auto outlines = cff(geometry, paths);
    const auto glyph_count = static_cast<std::uint16_t>(paths.size() + 1);
    Bytes head(54);
    patch32(head, 0, 0x00010000);
    patch32(head, 4, 0x00010000);
    patch32(head, 12, 0x5F0F3CF5);
    patch16(head, 18, 1024);
    patch16(head, 36, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::floor(geometry.left))));
    patch16(head, 38, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::floor(geometry.bottom))));
    patch16(head, 40, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::ceil(geometry.right))));
    patch16(head, 42, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::ceil(geometry.top))));
    patch16(head, 46, 8);
    patch16(head, 48, 2);
    Bytes hhea(36);
    patch32(hhea, 0, 0x00010000);
    patch16(hhea, 4, 896);
    patch16(hhea, 6, static_cast<std::uint16_t>(-128));
    patch16(hhea, 10, 1024);
    patch16(hhea, 16, static_cast<std::uint16_t>(std::ceil(geometry.right)));
    patch16(hhea, 18, 1);
    patch16(hhea, 34, glyph_count);
    Bytes hmtx;
    for (std::uint16_t glyph = 0; glyph < glyph_count; ++glyph) {
        u16(hmtx, 1024);
        u16(hmtx, 0);
    }
    Bytes maxp;
    u32(maxp, 0x00005000);
    u16(maxp, glyph_count);
    Bytes cmap;
    u16(cmap, 0);
    u16(cmap, 1);
    u16(cmap, 3);
    u16(cmap, 10);
    u32(cmap, 12);
    u16(cmap, 12);
    u16(cmap, 0);
    u32(cmap, 28);
    u32(cmap, 0);
    u32(cmap, 1);
    u32(cmap, 0xE000);
    u32(cmap, 0xE000 + static_cast<std::uint32_t>(paths.size()) - 1);
    u32(cmap, 1);
    Bytes os2(78);
    patch16(os2, 2, 1024);
    patch16(os2, 4, 400);
    patch16(os2, 6, 5);
    os2[58] = 'R';
    os2[59] = 'Y';
    os2[60] = 'N';
    os2[61] = 'U';
    patch16(os2, 62, 0x40);
    patch16(os2, 64, 0xE000);
    patch16(os2, 66, static_cast<std::uint16_t>(0xE000 + paths.size() - 1));
    patch16(os2, 68, 896);
    patch16(os2, 70, static_cast<std::uint16_t>(-128));
    patch16(os2, 74, static_cast<std::uint16_t>(std::ceil(geometry.top)));
    patch16(os2, 76, static_cast<std::uint16_t>(-std::floor(geometry.bottom)));
    Bytes post(32);
    patch32(post, 0, 0x00030000);
    std::array<Table, 9> tables{{{"CFF ", std::move(outlines)},
                                 {"OS/2", std::move(os2)},
                                 {"cmap", std::move(cmap)},
                                 {"head", std::move(head)},
                                 {"hhea", std::move(hhea)},
                                 {"hmtx", std::move(hmtx)},
                                 {"maxp", std::move(maxp)},
                                 {"name", font_name()},
                                 {"post", std::move(post)}}};
    Bytes result = ascii("OTTO");
    u16(result, 9);
    u16(result, 128);
    u16(result, 3);
    u16(result, 16);
    std::uint32_t offset = 12 + 9 * 16;
    std::uint32_t head_offset{};
    for (const auto& table : tables) {
        append(result, ascii(table.tag));
        u32(result, checksum(table.bytes));
        u32(result, offset);
        u32(result, static_cast<std::uint32_t>(table.bytes.size()));
        if (table.tag == "head") {
            head_offset = offset;
        }
        offset += static_cast<std::uint32_t>((table.bytes.size() + 3) & ~std::size_t{3});
    }
    for (const auto& table : tables) {
        append(result, table.bytes);
        while (result.size() % 4 != 0) {
            result.push_back(0);
        }
    }
    patch32(result, head_offset + 8, 0xB1B0AFBA - checksum(result));
    if (result.size() > byte_limit) {
        throw std::invalid_argument("Icon vector exceeds the 2 MiB font limit");
    }
    std::vector<std::byte> bytes;
    bytes.reserve(result.size());
    for (const auto value : result) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}
} // namespace ryn::font
