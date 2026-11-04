#include "text/bidi_analysis.hpp"
#include "text/text_engine.hpp"

#include <SheenBidi/SheenBidi.h>

#include <algorithm>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ryn::text {
namespace {
template <class Ref, void (*Release)(Ref)> struct ReleaseObject final {
    void operator()(std::remove_pointer_t<Ref>* value) const noexcept {
        Release(value);
    }
};

using Algorithm =
    std::unique_ptr<std::remove_pointer_t<SBAlgorithmRef>, ReleaseObject<SBAlgorithmRef, SBAlgorithmRelease>>;
using Paragraph =
    std::unique_ptr<std::remove_pointer_t<SBParagraphRef>, ReleaseObject<SBParagraphRef, SBParagraphRelease>>;
using Line = std::unique_ptr<std::remove_pointer_t<SBLineRef>, ReleaseObject<SBLineRef, SBLineRelease>>;
using ScriptLocator = std::unique_ptr<std::remove_pointer_t<SBScriptLocatorRef>,
                                      ReleaseObject<SBScriptLocatorRef, SBScriptLocatorRelease>>;

[[nodiscard]] SBLevel base_level(TextDirection direction) noexcept {
    return direction == TextDirection::Auto ? SBLevelDefaultLTR : direction == TextDirection::RightToLeft ? 1 : 0;
}
} // namespace

struct BidiAnalysis::Data final {
    String source;
    TextDirection direction;
    std::vector<char32_t> codepoints;
    std::vector<std::size_t> scalar_offsets;
    Algorithm algorithm;
    std::vector<Paragraph> handles;
    std::vector<BidiParagraph> paragraphs;
    std::vector<ScriptRun> scripts;
    std::vector<std::uint8_t> levels;
    std::shared_ptr<const Data> origin;
    std::shared_ptr<const Data> suffix;
    std::size_t prefix_bytes{};

    Data(String value, TextDirection requested) : source(std::move(value)), direction(requested) {
        if (source.empty()) {
            paragraphs.push_back({0, 0, 0, static_cast<std::uint8_t>(requested == TextDirection::RightToLeft)});
            return;
        }
        const auto scalars = decode_utf8(source.view());
        codepoints.reserve(scalars.size());
        scalar_offsets.reserve(scalars.size() + 1);
        for (const auto& scalar : scalars) {
            codepoints.push_back(scalar.value);
            scalar_offsets.push_back(scalar.byte_start);
        }
        scalar_offsets.push_back(source.size_bytes());
        // Analyze Unicode scalars, then translate every result back to UTF-8.
        // L1 resets must not divide the continuation bytes of a control scalar.
        const SBCodepointSequence sequence{SBStringEncodingUTF32, codepoints.data(), codepoints.size()};
        algorithm.reset(SBAlgorithmCreate(&sequence));
        if (!algorithm) {
            throw std::bad_alloc{};
        }
        levels.resize(source.size_bytes());
        std::size_t offset{};
        while (offset < codepoints.size()) {
            SBUInteger length{};
            SBUInteger separator{};
            SBAlgorithmGetParagraphBoundary(algorithm.get(), offset, codepoints.size() - offset, &length, &separator);
            Paragraph paragraph{SBAlgorithmCreateParagraph(algorithm.get(), offset, length, base_level(direction))};
            if (!paragraph) {
                throw std::bad_alloc{};
            }
            const auto* paragraph_levels = SBParagraphGetLevelsPtr(paragraph.get());
            for (std::size_t scalar = offset; scalar < offset + length; ++scalar) {
                std::fill(levels.begin() + scalar_offsets[scalar], levels.begin() + scalar_offsets[scalar + 1],
                          paragraph_levels[scalar - offset]);
            }
            paragraphs.push_back({scalar_offsets[offset], scalar_offsets[offset + length - separator],
                                  scalar_offsets[offset + length], SBParagraphGetBaseLevel(paragraph.get())});
            handles.push_back(std::move(paragraph));
            offset += length;
        }
        if (paragraphs.back().content_end != paragraphs.back().byte_end) {
            paragraphs.push_back({source.size_bytes(), source.size_bytes(), source.size_bytes(),
                                  static_cast<std::uint8_t>(requested == TextDirection::RightToLeft)});
        }
        ScriptLocator locator{SBScriptLocatorCreate()};
        if (!locator) {
            throw std::bad_alloc{};
        }
        SBScriptLocatorLoadCodepoints(locator.get(), &sequence);
        while (SBScriptLocatorMoveNext(locator.get())) {
            const auto* script = SBScriptLocatorGetAgent(locator.get());
            scripts.push_back({scalar_offsets[script->offset], scalar_offsets[script->offset + script->length],
                               SBScriptGetUnicodeTag(script->script)});
        }
    }
};

bool valid_text_direction(TextDirection value) noexcept {
    return value == TextDirection::Auto || value == TextDirection::LeftToRight || value == TextDirection::RightToLeft;
}

bool BidiAnalysis::assign(String source, TextDirection direction) {
    if (!valid_text_direction(direction)) {
        return false;
    }
    if (data_ && data_->source == source && data_->direction == direction) {
        return true;
    }
    auto next = std::make_shared<Data>(std::move(source), direction);
    data_ = std::move(next);
    return true;
}

StringView BidiAnalysis::source() const noexcept {
    return data_ ? data_->source.view() : StringView{};
}

TextDirection BidiAnalysis::direction() const noexcept {
    return data_ ? data_->direction : TextDirection::Auto;
}

std::span<const BidiParagraph> BidiAnalysis::paragraphs() const noexcept {
    return data_ ? std::span<const BidiParagraph>{data_->paragraphs} : std::span<const BidiParagraph>{};
}

std::span<const ScriptRun> BidiAnalysis::scripts() const noexcept {
    return data_ ? std::span<const ScriptRun>{data_->scripts} : std::span<const ScriptRun>{};
}

std::span<const std::uint8_t> BidiAnalysis::levels() const noexcept {
    return data_ ? std::span<const std::uint8_t>{data_->levels} : std::span<const std::uint8_t>{};
}

bool BidiAnalysis::scalar_boundary(std::size_t byte) const noexcept {
    const auto text = source().bytes();
    return data_ && byte <= text.size() &&
           (byte == text.size() || (static_cast<unsigned char>(text[byte]) & 0xC0) != 0x80);
}

std::optional<std::uint8_t> BidiAnalysis::level_at(std::size_t byte) const noexcept {
    return data_ && byte < data_->levels.size() && scalar_boundary(byte) ? std::optional{data_->levels[byte]}
                                                                         : std::nullopt;
}

std::optional<std::vector<BidiRun>> BidiAnalysis::line_runs(std::size_t begin, std::size_t end) const {
    if (end < begin || !scalar_boundary(begin) || !scalar_boundary(end)) {
        return {};
    }
    std::vector<BidiRun> result;
    if (begin == end) {
        return result;
    }
    const auto found = std::upper_bound(data_->paragraphs.begin(), data_->paragraphs.end(), begin,
                                        [](auto byte, const auto& paragraph) { return byte < paragraph.byte_begin; });
    if (found == data_->paragraphs.begin()) {
        return {};
    }
    const auto paragraph_index = static_cast<std::size_t>(found - data_->paragraphs.begin() - 1);
    if (end > data_->paragraphs[paragraph_index].byte_end || paragraph_index >= data_->handles.size()) {
        return {};
    }
    const auto scalar_begin =
        static_cast<std::size_t>(std::lower_bound(data_->scalar_offsets.begin(), data_->scalar_offsets.end(), begin) -
                                 data_->scalar_offsets.begin());
    const auto scalar_end =
        static_cast<std::size_t>(std::lower_bound(data_->scalar_offsets.begin(), data_->scalar_offsets.end(), end) -
                                 data_->scalar_offsets.begin());
    if (data_->origin) {
        BidiAnalysis original;
        original.data_ = data_->origin;
        if (begin < data_->prefix_bytes) {
            const auto retained = original.line_runs(begin, std::min(end, data_->prefix_bytes));
            if (!retained) {
                return {};
            }
            result = *retained;
        }
        if (end > data_->prefix_bytes) {
            const auto base = data_->paragraphs[paragraph_index].base_level;
            BidiAnalysis suffix;
            suffix.data_ = data_->suffix;
            auto runs =
                suffix.line_runs(std::max(begin, data_->prefix_bytes) - data_->prefix_bytes, end - data_->prefix_bytes);
            if (!runs) {
                return {};
            }
            for (auto& run : *runs) {
                run.byte_begin += data_->prefix_bytes;
                run.byte_end += data_->prefix_bytes;
            }
            if (base & 1) {
                result.insert(result.begin(), runs->begin(), runs->end());
            } else {
                result.insert(result.end(), runs->begin(), runs->end());
            }
        }
        return result;
    }
    Line line{SBParagraphCreateLine(data_->handles[paragraph_index].get(), scalar_begin, scalar_end - scalar_begin)};
    if (!line) {
        throw std::bad_alloc{};
    }
    const auto count = SBLineGetRunCount(line.get());
    result.reserve(count);
    const auto* runs = SBLineGetRunsPtr(line.get());
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back({data_->scalar_offsets[runs[index].offset],
                          data_->scalar_offsets[runs[index].offset + runs[index].length], runs[index].level});
    }
    return result;
}

BidiAnalysis BidiAnalysis::prefix(std::size_t bytes, String suffix) const {
    if (!scalar_boundary(bytes)) {
        throw std::invalid_argument("Bidi prefix must end on a scalar boundary");
    }
    auto text = std::string(source().bytes().substr(0, bytes));
    text += suffix.bytes();
    auto next = std::make_shared<Data>(String::from_utf8(text).value(), direction());
    next->origin = data_;
    next->prefix_bytes = bytes;
    std::copy_n(data_->levels.begin(), bytes, next->levels.begin());
    std::uint8_t base{};
    for (const auto& paragraph : data_->paragraphs) {
        if (paragraph.byte_begin <= bytes) {
            base = paragraph.base_level;
        }
    }
    next->suffix =
        std::make_shared<Data>(std::move(suffix), base & 1 ? TextDirection::RightToLeft : TextDirection::LeftToRight);
    std::copy(next->suffix->levels.begin(), next->suffix->levels.end(), next->levels.begin() + bytes);
    for (auto& paragraph : next->paragraphs) {
        for (const auto& original : data_->paragraphs) {
            if (original.byte_begin <= paragraph.byte_begin) {
                paragraph.base_level = original.base_level;
            }
        }
    }
    std::vector<ScriptRun> scripts;
    for (auto run : data_->scripts) {
        run.byte_end = std::min(run.byte_end, bytes);
        if (run.byte_begin < run.byte_end) {
            scripts.push_back(run);
        }
    }
    for (auto run : next->suffix->scripts) {
        run.byte_begin += bytes;
        run.byte_end += bytes;
        scripts.push_back(run);
    }
    next->scripts = std::move(scripts);
    BidiAnalysis result;
    result.data_ = std::move(next);
    return result;
}

bool operator==(const BidiAnalysis& left, const BidiAnalysis& right) noexcept {
    return left.data_ == right.data_ ||
           (left.assigned() == right.assigned() && left.source() == right.source() &&
            left.direction() == right.direction() && std::ranges::equal(left.levels(), right.levels()) &&
            std::ranges::equal(left.paragraphs(), right.paragraphs()) &&
            std::ranges::equal(left.scripts(), right.scripts()));
}
} // namespace ryn::text
