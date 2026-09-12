#include "input/platform_input.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn::input;
using ryn::String;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class E, class F> void rejects(F action) {
    try { action(); } catch (const E&) { return; }
    throw std::runtime_error("Expected rejection");
}
void events() {
    require(!String::from_utf8(std::string_view("\xC0\xAF", 2)), "invalid UTF8 accepted");
    CompositionChanged composition{String(u8"中😀e\u0301"), {1, 2}};
    require(is_valid(composition), "scalar range rejected");
    composition.selection = {4, 0};
    require(is_valid(composition), "end scalar rejected");
    composition.selection = {4, 1};
    require(!is_valid(composition), "byte range accepted as scalars");
    composition.selection = {1, std::numeric_limits<std::size_t>::max()};
    require(!is_valid(composition), "range overflow accepted");
    require(is_valid(CompositionChanged{}), "empty cancellation rejected");
    require(is_valid(CompositionChanged{{}, {0, 0, false}}), "unknown range rejected");
    require(!is_valid(CompositionChanged{{}, {1, 0, false}}), "noncanonical unknown range");
    CandidatesChanged candidates{{String(u8"你好"), String(u8"你好吗")}, 1,
        CandidateOrientation::horizontal};
    require(is_valid(candidates), "candidate snapshot rejected");
    candidates.selected = 2;
    require(!is_valid(candidates), "out of range candidate");
    candidates.selected.reset();
    candidates.orientation = static_cast<CandidateOrientation>(255);
    require(!is_valid(candidates), "invalid orientation");
    candidates.orientation = CandidateOrientation::vertical;
    candidates.candidates.resize(text_event_max_candidates + 1);
    require(!is_valid(candidates), "unbounded candidate count");
    auto large = String::from_utf8(std::string(text_event_max_bytes + 1, 'a'));
    require(!is_valid(TextCommitted{std::move(large).value()}), "unbounded text bytes");
}
void batches() {
    PlatformInputBatch batch(5, 20);
    batch.reserve(5);
    const TextInputSessionStamp stamp{{2, 3}, 9};
    TextCommitted commit{String(u8"你好"), stamp};
    CandidatesChanged candidates{{String(u8"好")}, 0, CandidateOrientation::vertical, stamp};
    batch.append(KeyboardInputEvent{Key::tab, KeyAction::down});
    batch.append(commit);
    batch.append(candidates);
    commit.text = String(u8"changed");
    candidates.candidates.clear();
    batch.append(CompositionChanged{{}, {0, 0}, stamp});
    batch.append(WindowInputEvent{WindowInputAction::focus_lost});
    require(std::holds_alternative<KeyboardInputEvent>(batch.events()[0]), "event order");
    require(std::get<TextCommitted>(batch.events()[1]).text == String(u8"你好"), "borrowed text");
    require(std::get<TextCommitted>(batch.events()[1]).session == stamp, "lost session identity");
    require(std::get<CandidatesChanged>(batch.events()[2]).candidates.size() == 1, "borrowed candidates");
    require(batch.payload_size_bytes() == 9, "payload accounting");
    rejects<std::length_error>([&] { batch.append(TextCommitted{}); });
    require(batch.size() == 5 && batch.payload_size_bytes() == 9, "capacity failure mutated queue");
    batch.clear();
    require(batch.empty() && batch.payload_size_bytes() == 0 && batch.capacity() >= 5, "clear lost reserve");
    batch.append(TextCommitted{String(u8"12345678901234567890")});
    rejects<std::length_error>([&] { batch.append(TextCommitted{String(u8"a")}); });
    rejects<std::invalid_argument>([&] { batch.append(CompositionChanged{{}, {1, 0}}); });
    require(batch.size() == 1 && batch.payload_size_bytes() == 20, "failed append changed bytes");
    rejects<std::length_error>([&] { batch.reserve(6); });
    PlatformInputBatch zero(0, 0);
    rejects<std::length_error>([&] { zero.append(TextCommitted{}); });
    PlatformInputBatch moves(1, 0);
    moves.append(PointerInputEvent{PointerIdentity::mouse(), PointerAction::move});
    require(!moves.append(PointerInputEvent{PointerIdentity::mouse(), PointerAction::move,
        PointerButton::none, 5, 7}), "bounded queue lost move coalescing");
}
}
int main() {
    try { events(); batches(); std::cout << "Text event ownership, range, order and capacity passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
