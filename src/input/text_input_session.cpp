#include "input/text_input_session.hpp"

#include <cmath>
#include <stdexcept>

namespace ryn::input {
namespace {
bool valid_rect(TextInputRect rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y)
        && std::isfinite(rect.width) && std::isfinite(rect.height)
        && rect.width >= 0 && rect.height >= 0;
}
bool valid_properties(const TextInputProperties& properties) noexcept {
    return properties.type <= TextInputType::number
        && properties.capitalization <= TextCapitalization::letters;
}
}

std::optional<WindowTextInputArea> map_text_input_area(const TextInputAreaGeometry& g) noexcept {
    if(!valid_rect(g.bounds) || !valid_rect(g.clip)
        || !std::isfinite(g.translation_x) || !std::isfinite(g.translation_y)
        || !std::isfinite(g.caret_x) || !std::isfinite(g.logical_to_window_scale)
        || g.logical_to_window_scale <= 0 || g.window_width <= 0 || g.window_height <= 0) return {};
    // Clip is in window-logical space, while bounds/caret are local to the input.
    const auto clip_right = g.clip.x + g.clip.width;
    const auto clip_bottom = g.clip.y + g.clip.height;
    if(!std::isfinite(clip_right) || !std::isfinite(clip_bottom)) return {};
    const auto left = std::clamp(g.bounds.x + g.translation_x, g.clip.x, clip_right);
    const auto top = std::clamp(g.bounds.y + g.translation_y, g.clip.y, clip_bottom);
    const auto right = std::max(left, std::min(g.bounds.x + g.translation_x + g.bounds.width,
        g.clip.x + g.clip.width));
    const auto bottom = std::max(top, std::min(g.bounds.y + g.translation_y + g.bounds.height,
        g.clip.y + g.clip.height));
    const auto scale = g.logical_to_window_scale;
    const double values[]{left * scale, top * scale, right * scale, bottom * scale,
        (g.caret_x + g.translation_x) * scale};
    for(const auto value : values) if(!std::isfinite(value)) return {};
    const auto x = static_cast<int>(std::floor(std::clamp(values[0], 0.0, double(g.window_width))));
    const auto y = static_cast<int>(std::floor(std::clamp(values[1], 0.0, double(g.window_height))));
    const auto end_x = right == left ? x
        : static_cast<int>(std::ceil(std::clamp(values[2], double(x), double(g.window_width))));
    const auto end_y = bottom == top ? y
        : static_cast<int>(std::ceil(std::clamp(values[3], double(y), double(g.window_height))));
    const auto caret = static_cast<int>(std::round(std::clamp(values[4], double(x), double(end_x))));
    return WindowTextInputArea{x, y, end_x - x, end_y - y, caret - x};
}

TextInputSessionHost::TextInputSessionHost(TextEditorStore& store, TextInputPlatform& platform)
    : store_(&store), platform_(&platform) {
    ensure_thread();
    if(platform.session_host_) throw std::logic_error("Window already has a text input session host");
    store_->attach_observer(*this);
    platform.session_host_ = this;
}
TextInputSessionHost::~TextInputSessionHost() {
    // Same owner-thread lifetime rule as the store and native window.
    if(!store_->is_owner_thread()) std::terminate();
    focused_ = {};
    static_cast<void>(synchronize_impl());
    store_->detach_observer(*this);
    platform_->session_host_ = nullptr;
}
void TextInputSessionHost::ensure_thread() const {
    if(!store_->is_owner_thread()) throw std::logic_error("Text input session accessed from non-owner thread");
}
bool TextInputSessionHost::focus(TextInputOwnerId id, TextInputProperties properties) {
    ensure_thread();
    if(!store_->find(id) || !valid_properties(properties)) return false;
    if(focused_ != id) { requested_area_.reset(); }
    focused_ = id;
    properties_ = properties;
    return synchronize_impl();
}
bool TextInputSessionHost::blur() {
    ensure_thread(); focused_ = {}; requested_area_.reset(); return synchronize_impl();
}
bool TextInputSessionHost::set_window_active(bool value) {
    ensure_thread(); window_active_ = value; return synchronize_impl();
}
bool TextInputSessionHost::synchronize() { ensure_thread(); return synchronize_impl(); }
bool TextInputSessionHost::synchronize_impl() noexcept {
    auto* desired = store_->find(focused_);
    const bool eligible = desired && window_active_ && !desired->disabled() && !desired->read_only();
    const bool unchanged = eligible && active_.valid() && active_.owner == focused_
        && started_properties_ == properties_;
    if(active_.valid() && !unchanged) {
        if(auto* previous = store_->find(active_.owner)) previous->cancel_composition();
        active_ = {};
        applied_area_.reset();
        ++diagnostics_.cancels;
        if(!platform_->cancel()) ++diagnostics_.failures;
        stop_pending_ = true;
    }
    if(stop_pending_) {
        ++diagnostics_.stops;
        if(!platform_->stop()) { ++diagnostics_.failures; return false; }
        stop_pending_ = false;
    }
    if(eligible && !active_.valid()) {
        if(epoch_ == std::numeric_limits<std::uint64_t>::max()) { ++diagnostics_.failures; return false; }
        const TextInputSessionStamp stamp{focused_, ++epoch_};
        ++diagnostics_.starts;
        if(!platform_->start(stamp, properties_)) {
            ++diagnostics_.failures;
            ++diagnostics_.stops;
            stop_pending_ = !platform_->stop();
            if(stop_pending_) ++diagnostics_.failures;
            return false;
        }
        active_ = stamp;
        started_properties_ = properties_;
    }
    if(active_.valid() && requested_area_ && requested_area_ != applied_area_) {
        ++diagnostics_.areas;
        if(!platform_->set_area(*requested_area_)) { ++diagnostics_.failures; return false; }
        applied_area_ = requested_area_;
    }
    return true;
}
bool TextInputSessionHost::cancel_composition() {
    ensure_thread();
    if(auto* state = store_->find(focused_)) state->cancel_composition();
    if(!active_.valid()) return true;
    ++diagnostics_.cancels;
    if(!platform_->cancel()) { ++diagnostics_.failures; return false; }
    return true;
}
bool TextInputSessionHost::set_input_area(const TextInputAreaGeometry& geometry) {
    ensure_thread();
    const auto mapped = map_text_input_area(geometry);
    if(!mapped) return false;
    requested_area_ = mapped;
    return synchronize_impl();
}
void TextInputSessionHost::before_destroy(TextInputOwnerId id) noexcept {
    if(focused_ == id) { focused_ = {}; requested_area_.reset(); }
    static_cast<void>(synchronize_impl());
}
void TextInputSessionHost::eligibility_changed(TextInputOwnerId) noexcept {
    static_cast<void>(synchronize_impl());
}
TextEditorState* TextInputSessionHost::recipient(TextInputSessionStamp stamp) {
    ensure_thread();
    static_cast<void>(synchronize_impl());
    if(!active_.valid() || active_ != stamp) { ++diagnostics_.stale_events; return nullptr; }
    return store_->find(stamp.owner);
}
TextEditResult TextInputSessionHost::dispatch(const TextCommitted& event) {
    if(auto* editor = recipient(event.session)) {
        if(!is_valid(event)) return {TextEditError::invalid_range};
        return editor->commit_text(event.text.bytes());
    }
    return {TextEditError::invalid_range};
}
TextEditResult TextInputSessionHost::dispatch(const CompositionChanged& event) {
    if(auto* editor = recipient(event.session)) return editor->update_composition(event);
    return {TextEditError::invalid_range};
}
TextEditResult TextInputSessionHost::dispatch(const CandidatesChanged& event) {
    if(auto* editor = recipient(event.session)) return editor->update_candidates(event);
    return {TextEditError::invalid_range};
}
TextInputSessionStamp TextInputSessionHost::active() const { ensure_thread(); return active_; }
const TextInputSessionDiagnostics& TextInputSessionHost::diagnostics() const { ensure_thread(); return diagnostics_; }

} // namespace ryn::input
