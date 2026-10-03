#include "text/bidi_analysis.hpp"
#include "text/text_engine.hpp"

#include <SheenBidi/SheenBidi.h>

#include <algorithm>
#include <new>
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

bool operator==(const BidiAnalysis& left, const BidiAnalysis& right) noexcept {
    return left.data_ == right.data_ || (left.assigned() == right.assigned() && left.source() == right.source() &&
                                         left.direction() == right.direction());
}
} // namespace ryn::text
