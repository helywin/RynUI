#include "input/text_input_session.hpp"
#include "platform/sdl/sdl_event_adapter.hpp"
#include "support/allocation_probe.hpp"

#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn::input;
void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
template<class Prepare, class Check> std::size_t inject(Prepare prepare, Check verify) {
    std::size_t failures = 0;
    for(std::size_t point = 0; point < 256; ++point) {
        if(prepare(point, verify)) break;
        ++failures;
    }
    check(failures > 0 && failures < 256, "allocation failure coverage missing");
    return failures;
}
void run() {
    const std::string large(4096, 'x');
    const auto text = ryn::String::from_utf8(large).value();
    const CompositionChanged change{text, {0, 4096}};
    const CandidatesChanged candidates{{text, text}, 1, CandidateOrientation::horizontal};
    const auto composition_failures = inject([&](std::size_t point, auto verify) {
        TextEditorStore store;
        auto& editor = store.require(store.create("old"));
        check(bool(editor.update_composition({ryn::String(u8"before"), {1, 2}})), "setup");
        ryn_test::allocation::begin(point);
        const auto result = editor.update_composition(change);
        ryn_test::allocation::end();
        if(result) return true;
        check(result.error == TextEditError::allocation_failure, "unexpected failure");
        verify(editor); return false;
    }, [](const auto& editor) {
        check(editor.value() == "old" && editor.revision() == 0
            && editor.composition().text == "before"
            && editor.composition().selection == TextScalarRange{1, 2}, "preedit failure not atomic");
    });
    const auto candidate_failures = inject([&](std::size_t point, auto verify) {
        TextEditorStore store;
        auto& editor = store.require(store.create("old"));
        check(bool(editor.update_candidates({{ryn::String(u8"before")}, 0})), "setup");
        ryn_test::allocation::begin(point);
        const auto result = editor.update_candidates(candidates);
        ryn_test::allocation::end();
        if(result) return true;
        verify(editor); return false;
    }, [](const auto& editor) {
        const auto view = editor.composition();
        check(view.candidates.size() == 1 && view.candidates[0] == ryn::String(u8"before")
            && view.selected_candidate == 0 && view.orientation == CandidateOrientation::vertical
            && editor.value() == "old" && editor.revision() == 0, "candidate failure not atomic");
    });
    const auto adapter_failures = inject([&](std::size_t point, auto verify) {
        ryn::detail::PlatformEvents events;
        events.input.append(TextCommitted{ryn::String(u8"before")});
        const char* borrowed[]{large.c_str(), large.c_str()};
        SDL_Event event{};
        event.type = SDL_EVENT_TEXT_EDITING_CANDIDATES;
        event.edit_candidates.candidates = borrowed;
        event.edit_candidates.num_candidates = 2;
        event.edit_candidates.selected_candidate = 0;
        ryn::detail::SdlWindowMetrics metrics;
        ryn_test::allocation::begin(point);
        try { ryn::detail::SdlEventAdapter::merge(events, event, metrics); }
        catch(const std::bad_alloc&) {
            ryn_test::allocation::end(); verify(events); return false;
        }
        ryn_test::allocation::end(); return true;
    }, [](const auto& events) {
        check(events.input.size() == 1 && events.input.payload_size_bytes() == 6
            && std::get<TextCommitted>(events.input.events()[0]).text == ryn::String(u8"before"),
            "adapter failure changed prior queue");
    });
    std::cout << "atomic_preedit_points=" << composition_failures << " atomic_candidate_points="
        << candidate_failures << " atomic_adapter_points=" << adapter_failures << '\n';
}
}
int main() {
    try { run(); }
    catch(const std::exception& error) {
        ryn_test::allocation::end(); std::cerr << error.what() << '\n'; return 1;
    }
}
