#include "input/text_editor.hpp"

#include <new>
#include <stdexcept>
#include <utility>

namespace ryn::input {

TextEditorState::TextEditorState(TextInputOwnerId id, std::string_view initial, TextEditorLimits limits)
    : id_(id), limits_(limits) {
    const auto result = set_value(initial);
    if(!result) { throw std::invalid_argument("Invalid initial text editor value"); }
    revision_ = 0;
    diagnostics_ = {};
}

void TextEditorState::ensure_owner_thread() const {
    if(owner_thread_ != std::this_thread::get_id()) {
        throw std::logic_error("TextEditorState accessed from non-owner thread");
    }
}
std::string_view TextEditorState::value() const { ensure_owner_thread(); return value_; }
TextSelection TextEditorState::selection() const { ensure_owner_thread(); return selection_; }
const TextBoundaryMap& TextEditorState::boundaries() const { ensure_owner_thread(); return boundaries_; }
std::uint64_t TextEditorState::revision() const { ensure_owner_thread(); return revision_; }
const TextEditorDiagnostics& TextEditorState::diagnostics() const { ensure_owner_thread(); return diagnostics_; }
TextEditorLimits TextEditorState::limits() const { ensure_owner_thread(); return limits_; }
bool TextEditorState::disabled() const { ensure_owner_thread(); return disabled_; }
bool TextEditorState::read_only() const { ensure_owner_thread(); return read_only_; }
void TextEditorState::set_eligibility(bool disabled, bool read_only) {
    ensure_owner_thread();
    if(disabled_ == disabled && read_only_ == read_only) return;
    disabled_ = disabled; read_only_ = read_only;
    if(disabled_ || read_only_) cancel_composition();
    if(observer_) observer_->eligibility_changed(id_);
}
std::size_t TextEditorState::retained_capacity() const {
    ensure_owner_thread();
    std::size_t candidate_capacity = candidates_.capacity() * sizeof(String);
    for(const auto& candidate : candidates_) candidate_capacity += candidate.size_bytes();
    return value_.capacity() + pending_value_.capacity() + normalized_.capacity()
        + history_.retained_capacity() + history_navigation_.capacity()
        + emitted_value_.capacity() + pending_emitted_value_.capacity()
        + composition_text_.capacity() + pending_composition_text_.capacity() + candidate_capacity
        + sizeof(std::size_t) * (boundaries_.retained_capacity()
            + pending_boundaries_.retained_capacity() + inserted_boundaries_.retained_capacity());
}
void TextEditorState::reserve(std::size_t bytes) {
    ensure_owner_thread();
    value_.reserve(bytes);
    pending_value_.reserve(bytes);
    normalized_.reserve(bytes);
    composition_text_.reserve(bytes);
    pending_composition_text_.reserve(bytes);
    boundaries_.reserve(bytes);
    pending_boundaries_.reserve(bytes);
    inserted_boundaries_.reserve(bytes);
    history_.reserve(bytes);
    history_navigation_.reserve(bytes);
    emitted_value_.reserve(bytes);
    pending_emitted_value_.reserve(bytes);
}
TextEditResult TextEditorState::reject(TextEditError error) {
    ++diagnostics_.rejected;
    return {error};
}
TextEditResult TextEditorState::publish_selection(TextSelection selection) {
    const bool changed = selection_ != selection;
    selection_ = selection;
    if(changed) { ++diagnostics_.selections; }
    return {TextEditError::none, false, changed};
}

TextEditResult TextEditorState::replace(TextSelection range, std::string_view text, bool authoritative,
    bool merge_typing, std::optional<TextSelection> history_selection) {
    ensure_owner_thread();
    if(!authoritative && disabled_) { return reject(TextEditError::disabled); }
    if(!authoritative && read_only_) { return reject(TextEditError::read_only); }
    if(!boundaries_.is_boundary(range.anchor) || !boundaries_.is_boundary(range.caret)) {
        return reject(TextEditError::invalid_range);
    }
    try {
        // Copy into a separate buffer before touching value_, so aliases into
        // the existing value (paste/replace snapshots) remain safe.
        normalized_.clear();
        Utf8ScalarIterator iterator(text);
        while(const auto scalar = iterator.next()) {
            if(scalar->value != U'\r' && scalar->value != U'\n') {
                normalized_.append(text.substr(scalar->byte_begin, scalar->byte_end - scalar->byte_begin));
            }
        }
        if(!iterator.valid()) { return reject(TextEditError::invalid_utf8); }
        if(!inserted_boundaries_.assign(normalized_)) { return reject(TextEditError::invalid_utf8); }
        const auto kept_scalars = boundaries_.scalar_count()
            - (*boundaries_.byte_to_scalar(range.end()) - *boundaries_.byte_to_scalar(range.begin()));
        const auto available = limits_.max_scalars > kept_scalars ? limits_.max_scalars - kept_scalars : 0;
        std::size_t accepted = normalized_.size();
        if(inserted_boundaries_.scalar_count() > available) {
            accepted = inserted_boundaries_.floor(*inserted_boundaries_.scalar_to_byte(available));
        }
        const bool truncated = accepted != normalized_.size();
        const auto kept_bytes = value_.size() - (range.end() - range.begin());
        if(kept_bytes > limits_.max_bytes || accepted > limits_.max_bytes - kept_bytes) {
            return reject(TextEditError::capacity_exceeded);
        }
        pending_value_.assign(value_, 0, range.begin());
        pending_value_.append(normalized_, 0, accepted);
        pending_value_.append(value_, range.end(), std::string::npos);
        if(!pending_boundaries_.assign(pending_value_)) { return reject(TextEditError::invalid_utf8); }
        const bool changed = pending_value_ != value_;
        if(changed && revision_ == std::numeric_limits<std::uint64_t>::max()) {
            return reject(TextEditError::revision_exhausted);
        }
        TextSelection next_selection;
        if(authoritative) {
            next_selection = {pending_boundaries_.floor(selection_.anchor), pending_boundaries_.floor(selection_.caret)};
        } else {
            const auto caret = accepted == 0 ? pending_boundaries_.floor(range.begin())
                : pending_boundaries_.ceil(range.begin() + accepted);
            next_selection = {caret, caret};
        }
        if(changed && !authoritative) {
            history_.prepare(value_, history_selection.value_or(selection_), pending_value_, next_selection,
                merge_epoch_, merge_typing && merge_epoch_ != std::numeric_limits<std::uint64_t>::max());
        }
        // The commit tail cannot allocate or fail.
        value_.swap(pending_value_);
        boundaries_.swap(pending_boundaries_);
        auto result = publish_selection(next_selection);
        if(changed) { ++revision_; ++diagnostics_.mutations; }
        if(changed && !authoritative) {
            history_.commit();
            if(!merge_typing) break_history_merge();
        }
        if(truncated) { ++diagnostics_.truncated; }
        result.value_changed = changed;
        result.truncated = truncated;
        return result;
    } catch(const std::bad_alloc&) {
        return reject(TextEditError::allocation_failure);
    } catch(const std::length_error&) {
        return reject(TextEditError::capacity_exceeded);
    }
}

TextEditResult TextEditorState::set_value(std::string_view text) {
    ensure_owner_thread();
    const auto result = replace({0, value_.size()}, text, true);
    if(result && result.value_changed) {
        cancel_composition();
        history_.clear();
        break_history_merge();
        emitted_echo_.reset();
        emitted_value_.clear();
    }
    return result;
}
TextEditResult TextEditorState::set_limits(TextEditorLimits limits) {
    ensure_owner_thread();
    const auto previous = limits_;
    limits_ = limits;
    const auto result = set_value(value_);
    if(!result) { limits_ = previous; }
    else if(previous.max_scalars != limits.max_scalars || previous.max_bytes != limits.max_bytes) {
        history_.clear();
        break_history_merge();
    }
    return result;
}
TextEditResult TextEditorState::replace_selection(std::string_view text) {
    ensure_owner_thread();
    return replace_range(selection_, text);
}
TextEditResult TextEditorState::replace_range(TextSelection range, std::string_view text) {
    ensure_owner_thread();
    const auto result = replace(range, text, false, false, range);
    if(result) cancel_composition();
    return result;
}
TextEditResult TextEditorState::erase_backward() {
    ensure_owner_thread();
    auto range = selection_;
    if(range.empty()) { range.anchor = boundaries_.previous(range.caret); }
    const auto result = replace(range, {}, false);
    if(result) cancel_composition();
    return result;
}
TextEditResult TextEditorState::erase_forward() {
    ensure_owner_thread();
    auto range = selection_;
    if(range.empty()) { range.caret = boundaries_.next(range.caret); }
    const auto result = replace(range, {}, false);
    if(result) cancel_composition();
    return result;
}
TextEditResult TextEditorState::select(TextSelection selection) {
    ensure_owner_thread();
    if(disabled_) { return reject(TextEditError::disabled); }
    const auto result = publish_selection({boundaries_.floor(selection.anchor), boundaries_.floor(selection.caret)});
    if(result.selection_changed) { cancel_composition(); break_history_merge(); }
    return result;
}
TextEditResult TextEditorState::place(std::size_t byte, bool extend) {
    ensure_owner_thread();
    return select({extend ? selection_.anchor : byte, byte});
}
TextEditResult TextEditorState::move(TextCaretMove direction, bool extend) {
    ensure_owner_thread();
    auto caret = selection_.caret;
    switch(direction) {
    case TextCaretMove::left:
        caret = !extend && !selection_.empty() ? selection_.begin() : boundaries_.previous(caret);
        break;
    case TextCaretMove::right:
        caret = !extend && !selection_.empty() ? selection_.end() : boundaries_.next(caret);
        break;
    case TextCaretMove::home: caret = 0; break;
    case TextCaretMove::end: caret = value_.size(); break;
    default: return reject(TextEditError::invalid_range);
    }
    return place(caret, extend);
}
TextEditResult TextEditorState::select_all() { ensure_owner_thread(); return select({0, value_.size()}); }
TextWordClass TextEditorState::word_class_at(std::size_t byte) const noexcept {
    Utf8ScalarIterator iterator(std::string_view(value_).substr(byte));
    const auto scalar = iterator.next();
    return scalar ? text_word_class(scalar->value) : TextWordClass::symbol;
}
TextEditResult TextEditorState::select_word(std::size_t byte) {
    ensure_owner_thread();
    if(value_.empty()) { return select({}); }
    auto begin = boundaries_.floor(byte);
    if(begin == value_.size()) { begin = boundaries_.previous(begin); }
    auto end = boundaries_.next(begin);
    const auto category = word_class_at(begin);
    if(category != TextWordClass::symbol) {
        while(begin > 0 && word_class_at(boundaries_.previous(begin)) == category) {
            begin = boundaries_.previous(begin);
        }
        while(end < value_.size() && word_class_at(end) == category) { end = boundaries_.next(end); }
    }
    return select({begin, end});
}

bool TextEditorStore::is_owner_thread() const noexcept { return owner_thread_ == std::this_thread::get_id(); }
void TextEditorStore::ensure_owner_thread() const {
    if(!is_owner_thread()) { throw std::logic_error("TextEditorStore accessed from non-owner thread"); }
}
void TextEditorStore::reserve(std::size_t owners) { ensure_owner_thread(); slots_.reserve(owners); }
TextInputOwnerId TextEditorStore::create(std::string_view initial, TextEditorLimits limits) {
    ensure_owner_thread();
    std::size_t index = 0;
    while(index < slots_.size() && (slots_[index].state || slots_[index].generation == 0)) { ++index; }
    if(index >= TextInputOwnerId::invalid_index) { throw std::length_error("Text editor slots exhausted"); }
    const TextInputOwnerId id{static_cast<std::uint32_t>(index), index < slots_.size() ? slots_[index].generation : 1};
    auto state = std::unique_ptr<TextEditorState>(new TextEditorState(id, initial, limits));
    if(index == slots_.size()) { slots_.emplace_back(); }
    slots_[index].state = std::move(state);
    slots_[index].state->observer_ = observer_;
    ++size_;
    return id;
}
bool TextEditorStore::destroy(TextInputOwnerId id) {
    ensure_owner_thread();
    if(find(id) == nullptr) { return false; }
    if(observer_) observer_->before_destroy(id);
    auto& slot = slots_[id.index];
    slot.state.reset();
    ++slot.generation; // zero permanently retires an exhausted generation.
    --size_;
    return true;
}
TextEditorState* TextEditorStore::find(TextInputOwnerId id) {
    return const_cast<TextEditorState*>(std::as_const(*this).find(id));
}
const TextEditorState* TextEditorStore::find(TextInputOwnerId id) const {
    ensure_owner_thread();
    if(!id.valid() || id.index >= slots_.size() || slots_[id.index].generation != id.generation) { return nullptr; }
    return slots_[id.index].state.get();
}
TextEditorState& TextEditorStore::require(TextInputOwnerId id) {
    if(auto* state = find(id)) { return *state; }
    throw std::invalid_argument("Stale or invalid text input owner");
}
std::size_t TextEditorStore::size() const { ensure_owner_thread(); return size_; }
std::size_t TextEditorStore::capacity() const { ensure_owner_thread(); return slots_.capacity(); }

void TextEditorStore::attach_observer(TextEditorObserver& observer) {
    ensure_owner_thread();
    if(observer_ && observer_ != &observer) throw std::logic_error("Text editor store already has a session host");
    observer_ = &observer;
    for(auto& slot : slots_) if(slot.state) slot.state->observer_ = observer_;
}
void TextEditorStore::detach_observer(TextEditorObserver& observer) {
    ensure_owner_thread();
    if(observer_ != &observer) return;
    observer_ = nullptr;
    for(auto& slot : slots_) if(slot.state) slot.state->observer_ = nullptr;
}

TextCompositionView TextEditorState::composition() const {
    ensure_owner_thread();
    return {composition_text_, composition_selection_, composition_replacement_, candidates_,
        selected_candidate_, candidate_orientation_, composing_};
}
void TextEditorState::cancel_composition() {
    ensure_owner_thread();
    if(composing_ || !candidates_.empty()) break_history_merge();
    composition_text_.clear();
    candidates_.clear();
    composition_selection_ = {};
    composition_replacement_ = {};
    selected_candidate_.reset();
    candidate_orientation_ = CandidateOrientation::vertical;
    composing_ = false;
}
TextEditResult TextEditorState::update_composition(const CompositionChanged& event) {
    ensure_owner_thread();
    if(disabled_) return reject(TextEditError::disabled);
    if(read_only_) return reject(TextEditError::read_only);
    if(!is_valid(event)) return reject(TextEditError::invalid_range);
    if(event.text.empty()) { cancel_composition(); return {}; }
    try {
        pending_composition_text_.assign(event.text.bytes());
        composition_text_.swap(pending_composition_text_);
        composition_selection_ = event.selection;
        if(!composing_) { composition_replacement_ = selection_; break_history_merge(); }
        composing_ = true;
        return {};
    } catch(const std::bad_alloc&) { return reject(TextEditError::allocation_failure); }
    catch(const std::length_error&) { return reject(TextEditError::capacity_exceeded); }
}
TextEditResult TextEditorState::update_candidates(const CandidatesChanged& event) {
    ensure_owner_thread();
    if(disabled_) return reject(TextEditError::disabled);
    if(read_only_) return reject(TextEditError::read_only);
    if(!is_valid(event)) return reject(TextEditError::invalid_range);
    try {
        auto pending = event.candidates;
        candidates_.swap(pending);
        selected_candidate_ = event.selected;
        candidate_orientation_ = event.orientation;
        return {};
    } catch(const std::bad_alloc&) { return reject(TextEditError::allocation_failure); }
    catch(const std::length_error&) { return reject(TextEditError::capacity_exceeded); }
}
TextEditResult TextEditorState::commit_text(std::string_view text) {
    ensure_owner_thread();
    // An empty platform commit is a cancellation, not deletion of selected text.
    if(disabled_) return reject(TextEditError::disabled);
    if(read_only_) return reject(TextEditError::read_only);
    if(text.empty()) { cancel_composition(); return {}; }
    const auto result = replace(composing_ ? composition_replacement_ : selection_, text, false, !composing_);
    if(result) cancel_composition();
    return result;
}

TextHistorySnapshot TextEditorState::history() const { ensure_owner_thread(); return history_.snapshot(); }
void TextEditorState::break_history_merge() {
    ensure_owner_thread();
    if(merge_epoch_ != std::numeric_limits<std::uint64_t>::max()) ++merge_epoch_;
}
TextEditResult TextEditorState::navigate_history(bool forward) {
    ensure_owner_thread();
    if(disabled_) return reject(TextEditError::disabled);
    if(read_only_) return reject(TextEditError::read_only);
    try {
        TextSelection selection;
        const bool available = forward ? history_.read_redo(history_navigation_, selection)
                                       : history_.read_undo(history_navigation_, selection);
        if(!available) return {};
        auto result = replace({0, value_.size()}, history_navigation_, true);
        if(!result) return result;
        const auto restored = publish_selection({boundaries_.floor(selection.anchor), boundaries_.floor(selection.caret)});
        result.selection_changed = result.selection_changed || restored.selection_changed;
        if(forward) history_.commit_redo(); else history_.commit_undo();
        cancel_composition();
        break_history_merge();
        return result;
    } catch(const std::bad_alloc&) { return reject(TextEditError::allocation_failure); }
    catch(const std::length_error&) { return reject(TextEditError::capacity_exceeded); }
}
TextEditResult TextEditorState::undo() { return navigate_history(false); }
TextEditResult TextEditorState::redo() { return navigate_history(true); }

TextEditEcho TextEditorState::edit_echo() const { ensure_owner_thread(); return {id_, revision_}; }
TextEditResult TextEditorState::note_emitted_value() {
    ensure_owner_thread();
    const auto echo = edit_echo();
    if(emitted_echo_ == echo && emitted_value_ == value_) return {};
    try {
        pending_emitted_value_.assign(value_);
        emitted_value_.swap(pending_emitted_value_);
        emitted_echo_ = echo;
        return {};
    } catch(const std::bad_alloc&) { return reject(TextEditError::allocation_failure); }
    catch(const std::length_error&) { return reject(TextEditError::capacity_exceeded); }
}
TextReconcileResult TextEditorState::reconcile(std::string_view text, std::optional<TextEditEcho> echo) {
    ensure_owner_thread();
    if(echo && echo->owner != id_) return {reject(TextEditError::stale_owner), false, true};
    if(echo && echo->revision < revision_) return {{}, false, true};
    if(echo && echo->revision > revision_) return {reject(TextEditError::revision_conflict), false, false};
    if(emitted_echo_ && (!echo || *echo == *emitted_echo_) && text == emitted_value_ && text == value_)
        return {{}, true, false};
    // An untagged differing authoritative value is a new external baseline.
    // It must not be guessed to be a delayed echo of some older value.
    return {set_value(text), false, false};
}

} // namespace ryn::input
