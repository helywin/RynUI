#include "component/otp_model.hpp"
#include "input/text_boundary.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {
std::atomic<std::uint64_t> next_owner{1};

OTPCells split(std::string_view bytes) {
    input::Utf8ScalarIterator scalars{bytes};
    std::string normalized;
    normalized.reserve(bytes.size());
    while (const auto scalar = scalars.next()) {
        if (scalar->value != '\r' && scalar->value != '\n') {
            normalized.append(bytes.substr(scalar->byte_begin, scalar->byte_end - scalar->byte_begin));
        }
    }
    if (!scalars.valid()) {
        throw std::invalid_argument("Invalid OTP UTF-8");
    }
    input::TextBoundaryMap boundaries;
    if (!boundaries.assign(normalized)) {
        throw std::invalid_argument("Invalid OTP grapheme boundaries");
    }
    const auto stops = boundaries.grapheme_bytes();
    const auto count = std::min<std::size_t>(boundaries.grapheme_count(), 1024);
    OTPCells result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back(
            String::from_utf8(std::string_view{normalized}.substr(stops[i], stops[i + 1] - stops[i])).value());
    }
    return result;
}

String join(std::span<const String> cells, bool holes) {
    std::string bytes;
    for (const auto& cell : cells) {
        if (holes && cell.empty()) {
            bytes.push_back(' ');
        } else {
            bytes.append(cell.bytes());
        }
    }
    return String::from_utf8(bytes).value();
}
} // namespace

void OTPModel::validate_length(std::size_t length) {
    if (length == 0 || length > 1024) {
        throw std::invalid_argument("OTP length must be in 1..1024");
    }
}

OTPModel::OTPModel(std::size_t length, std::string_view initial, OTPFormatter formatter)
    : formatter_(std::move(formatter)), length_(length), owner_key_(next_owner.fetch_add(1)) {
    validate_length(length);
    const auto normalized = join(split(initial), false);
    source_ = split(formatter_ ? formatter_(normalized).bytes() : normalized.bytes());
}

void OTPModel::require_owner() const {
    if (std::this_thread::get_id() != thread_) {
        throw std::logic_error("OTP model must be used on its owner thread");
    }
}

OTPCells OTPModel::projection() const {
    require_owner();
    OTPCells result(length_);
    std::copy_n(source_.begin(), std::min(length_, source_.size()), result.begin());
    return result;
}

String OTPModel::value() const {
    require_owner();
    return join(std::span<const String>{source_}.first(std::min(length_, source_.size())), false);
}

std::size_t OTPModel::first_empty() const noexcept {
    for (std::size_t i = 0; i < length_; ++i) {
        if (i >= source_.size() || source_[i].empty()) {
            return i;
        }
    }
    return length_;
}

bool OTPModel::reconcile(std::string_view bytes) {
    require_owner();
    if (retired_) {
        return false;
    }
    auto next = split(bytes);
    // A controlled echo of the visible value must also preserve hidden external
    // suffixes and the transaction revision used by callback/advance guards.
    if (next == source_ || join(next, false) == value()) {
        return false;
    }
    source_.swap(next);
    ++revision_;
    return true;
}

bool OTPModel::set_length(std::size_t length) {
    require_owner();
    validate_length(length);
    if (retired_ || length == length_) {
        return false;
    }
    length_ = length;
    ++revision_;
    return true;
}

std::optional<OTPCandidate> OTPModel::prepare(std::size_t index, std::string_view bytes) {
    require_owner();
    if (retired_ || index >= length_) {
        return {};
    }
    const auto revision = revision_;
    auto incoming = split(bytes);
    auto before = projection();
    auto next = before;
    if (incoming.size() <= 1) {
        next[index] = incoming.empty() ? String{} : incoming.front();
    } else {
        for (std::size_t i = index; i < length_; ++i) {
            next[i] = i - index < incoming.size() ? incoming[i - index] : String{};
        }
    }
    if (formatter_) {
        auto last = next.size();
        while (last && next[last - 1].empty()) {
            --last;
        }
        // Copy the callback: it may retire or modify the model synchronously.
        auto formatter = formatter_;
        auto formatted = split(formatter(join(std::span<const String>{next}.first(last), true)).bytes());
        if (retired_ || revision != revision_) {
            return {};
        }
        formatted.resize(length_);
        for (std::size_t i = 0; i < length_; ++i) {
            if (next[i].empty() && formatted[i] == String{u8" "}) {
                formatted[i] = String{};
            }
        }
        next.swap(formatted);
    }
    if (retired_ || revision != revision_) {
        return {};
    }
    const bool complete = next != before && std::ranges::none_of(next, [](const String& cell) { return cell.empty(); });
    return OTPCandidate{std::move(next), std::min(index + incoming.size(), length_ - 1), complete, revision,
                        owner_key_};
}

bool OTPModel::commit(OTPCandidate candidate) {
    require_owner();
    if (retired_ || candidate.owner_key != owner_key_ || candidate.source_revision != revision_ ||
        candidate.cells.size() != length_) {
        return false;
    }
    source_.swap(candidate.cells);
    ++revision_;
    return true;
}

void OTPModel::retire() noexcept {
    retired_ = true;
}
} // namespace ryn::detail
