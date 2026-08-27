#include "theme/theme_runtime.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace ryn::theme_runtime {
namespace {

constexpr std::size_t identity_count =
    static_cast<std::size_t>(TokenIdentity::count);

struct ReadCapture final {
    const ThemeScope* scope{};
    std::array<TokenIdentity, identity_count> identities{};
    std::size_t count{};
    ReadCapture* previous{};
};

thread_local ReadCapture* active_capture = nullptr;

class ReadCaptureGuard final {
public:
    explicit ReadCaptureGuard(ReadCapture& capture) noexcept
        : capture_(&capture) {
        capture.previous = active_capture;
        active_capture = &capture;
    }

    ~ReadCaptureGuard() {
        active_capture = capture_->previous;
    }

private:
    ReadCapture* capture_;
};

template <typename Value>
void append_if_changed(
    const Value& before,
    const Value& after,
    TokenIdentity identity,
    std::array<TokenIdentity, identity_count>& changed,
    std::size_t& count) {
    if (before != after) {
        changed[count++] = identity;
    }
}

std::size_t collect_changed(
    const ThemeSnapshot& before,
    const ThemeSnapshot& after,
    std::array<TokenIdentity, identity_count>& changed) {
    std::size_t count = 0;
    const auto& before_alias = before.alias();
    const auto& after_alias = after.alias();
    append_if_changed(before_alias.color_text, after_alias.color_text,
        TokenIdentity::alias_color_text, changed, count);
    append_if_changed(before_alias.color_text_secondary, after_alias.color_text_secondary,
        TokenIdentity::alias_color_text_secondary, changed, count);
    append_if_changed(before_alias.color_text_disabled, after_alias.color_text_disabled,
        TokenIdentity::alias_color_text_disabled, changed, count);
    append_if_changed(before_alias.color_background_container,
        after_alias.color_background_container,
        TokenIdentity::alias_color_background_container, changed, count);
    append_if_changed(before_alias.color_background_elevated,
        after_alias.color_background_elevated,
        TokenIdentity::alias_color_background_elevated, changed, count);
    append_if_changed(before_alias.color_background_container_disabled,
        after_alias.color_background_container_disabled,
        TokenIdentity::alias_color_background_container_disabled, changed, count);
    append_if_changed(before_alias.color_border, after_alias.color_border,
        TokenIdentity::alias_color_border, changed, count);
    append_if_changed(before_alias.color_border_secondary, after_alias.color_border_secondary,
        TokenIdentity::alias_color_border_secondary, changed, count);
    append_if_changed(before_alias.color_focus_outline, after_alias.color_focus_outline,
        TokenIdentity::alias_color_focus_outline, changed, count);
    append_if_changed(before_alias.line_width_focus, after_alias.line_width_focus,
        TokenIdentity::alias_line_width_focus, changed, count);
    append_if_changed(before_alias.focus_outline_offset, after_alias.focus_outline_offset,
        TokenIdentity::alias_focus_outline_offset, changed, count);
    append_if_changed(before_alias.box_shadow, after_alias.box_shadow,
        TokenIdentity::alias_box_shadow, changed, count);
    append_if_changed(before_alias.box_shadow_secondary, after_alias.box_shadow_secondary,
        TokenIdentity::alias_box_shadow_secondary, changed, count);
    append_if_changed(before_alias.box_shadow_tertiary, after_alias.box_shadow_tertiary,
        TokenIdentity::alias_box_shadow_tertiary, changed, count);

    const auto& before_map = before.map();
    const auto& after_map = after.map();
    append_if_changed(before_map.color_primary, after_map.color_primary,
        TokenIdentity::map_color_primary, changed, count);
    append_if_changed(before_map.color_primary_hover, after_map.color_primary_hover,
        TokenIdentity::map_color_primary_hover, changed, count);
    append_if_changed(before_map.color_primary_active, after_map.color_primary_active,
        TokenIdentity::map_color_primary_active, changed, count);
    append_if_changed(before_map.color_primary_border, after_map.color_primary_border,
        TokenIdentity::map_color_primary_border, changed, count);
    append_if_changed(before_map.color_success, after_map.color_success,
        TokenIdentity::map_color_success, changed, count);
    append_if_changed(before_map.color_warning, after_map.color_warning,
        TokenIdentity::map_color_warning, changed, count);
    append_if_changed(before_map.color_error, after_map.color_error,
        TokenIdentity::map_color_error, changed, count);
    append_if_changed(before_map.color_info, after_map.color_info,
        TokenIdentity::map_color_info, changed, count);
    append_if_changed(before_map.color_text_base, after_map.color_text_base,
        TokenIdentity::map_color_text_base, changed, count);
    append_if_changed(before_map.color_background_base, after_map.color_background_base,
        TokenIdentity::map_color_background_base, changed, count);
    append_if_changed(before_map.font_size_small, after_map.font_size_small,
        TokenIdentity::map_font_size_small, changed, count);
    append_if_changed(before_map.font_size, after_map.font_size,
        TokenIdentity::map_font_size, changed, count);
    append_if_changed(before_map.font_size_large, after_map.font_size_large,
        TokenIdentity::map_font_size_large, changed, count);
    append_if_changed(before_map.line_height_small, after_map.line_height_small,
        TokenIdentity::map_line_height_small, changed, count);
    append_if_changed(before_map.line_height, after_map.line_height,
        TokenIdentity::map_line_height, changed, count);
    append_if_changed(before_map.line_height_large, after_map.line_height_large,
        TokenIdentity::map_line_height_large, changed, count);
    append_if_changed(before_map.size_xs, after_map.size_xs,
        TokenIdentity::map_size_xs, changed, count);
    append_if_changed(before_map.size_small, after_map.size_small,
        TokenIdentity::map_size_small, changed, count);
    append_if_changed(before_map.size, after_map.size,
        TokenIdentity::map_size, changed, count);
    append_if_changed(before_map.size_large, after_map.size_large,
        TokenIdentity::map_size_large, changed, count);
    append_if_changed(before_map.control_height_small, after_map.control_height_small,
        TokenIdentity::map_control_height_small, changed, count);
    append_if_changed(before_map.control_height, after_map.control_height,
        TokenIdentity::map_control_height, changed, count);
    append_if_changed(before_map.control_height_large, after_map.control_height_large,
        TokenIdentity::map_control_height_large, changed, count);
    append_if_changed(before_map.border_radius_small, after_map.border_radius_small,
        TokenIdentity::map_border_radius_small, changed, count);
    append_if_changed(before_map.border_radius, after_map.border_radius,
        TokenIdentity::map_border_radius, changed, count);
    append_if_changed(before_map.border_radius_large, after_map.border_radius_large,
        TokenIdentity::map_border_radius_large, changed, count);
    append_if_changed(before_map.motion_unit, after_map.motion_unit,
        TokenIdentity::map_motion_unit, changed, count);
    append_if_changed(before_map.motion_base, after_map.motion_base,
        TokenIdentity::map_motion_base, changed, count);
    append_if_changed(before_map.motion, after_map.motion,
        TokenIdentity::map_motion_enabled, changed, count);

    const auto& before_button = before.button();
    const auto& after_button = after.button();
    const bool button_colors_changed =
        before_button.default_color != after_button.default_color
        || before_button.default_background != after_button.default_background
        || before_button.default_border_color != after_button.default_border_color
        || before_button.default_hover_color != after_button.default_hover_color
        || before_button.default_active_color != after_button.default_active_color
        || before_button.primary_color != after_button.primary_color
        || before_button.primary_background != after_button.primary_background
        || before_button.primary_hover_background != after_button.primary_hover_background
        || before_button.primary_active_background != after_button.primary_active_background
        || before_button.danger_color != after_button.danger_color
        || before_button.danger_background != after_button.danger_background
        || before_button.danger_hover_background != after_button.danger_hover_background
        || before_button.danger_active_background != after_button.danger_active_background
        || before_button.disabled_color != after_button.disabled_color
        || before_button.disabled_background != after_button.disabled_background
        || before_button.disabled_border_color != after_button.disabled_border_color;
    append_if_changed(false, button_colors_changed,
        TokenIdentity::button_colors, changed, count);
    append_if_changed(
        std::array{before_button.control_height_small, before_button.control_height,
            before_button.control_height_large},
        std::array{after_button.control_height_small, after_button.control_height,
            after_button.control_height_large},
        TokenIdentity::button_control_heights, changed, count);
    append_if_changed(
        std::array{before_button.padding_inline_small, before_button.padding_inline,
            before_button.padding_inline_large},
        std::array{after_button.padding_inline_small, after_button.padding_inline,
            after_button.padding_inline_large},
        TokenIdentity::button_padding_inline, changed, count);
    append_if_changed(
        std::array{before_button.content_font_size_small, before_button.content_font_size,
            before_button.content_font_size_large,
            before_button.content_line_height_small, before_button.content_line_height,
            before_button.content_line_height_large, before_button.loading_indicator_size,
            before_button.loading_opacity},
        std::array{after_button.content_font_size_small, after_button.content_font_size,
            after_button.content_font_size_large,
            after_button.content_line_height_small, after_button.content_line_height,
            after_button.content_line_height_large, after_button.loading_indicator_size,
            after_button.loading_opacity},
        TokenIdentity::button_typography, changed, count);
    append_if_changed(
        std::array{before_button.border_radius_small, before_button.border_radius,
            before_button.border_radius_large},
        std::array{after_button.border_radius_small, after_button.border_radius,
            after_button.border_radius_large},
        TokenIdentity::button_border_radius, changed, count);
    append_if_changed(before_button.border_width, after_button.border_width,
        TokenIdentity::button_border_width, changed, count);
    append_if_changed(before_button.icon_gap, after_button.icon_gap,
        TokenIdentity::button_icon_gap, changed, count);
    const bool button_shadows_changed =
        before_button.default_shadow != after_button.default_shadow
        || before_button.primary_shadow != after_button.primary_shadow
        || before_button.danger_shadow != after_button.danger_shadow;
    append_if_changed(false, button_shadows_changed,
        TokenIdentity::button_shadows, changed, count);

    const auto& before_text = before.text();
    const auto& after_text = after.text();
    append_if_changed(before_text.color, after_text.color,
        TokenIdentity::text_color, changed, count);
    append_if_changed(before_text.font_family, after_text.font_family,
        TokenIdentity::text_font_family, changed, count);
    append_if_changed(before_text.font_weight, after_text.font_weight,
        TokenIdentity::text_font_weight, changed, count);
    append_if_changed(before_text.font_size, after_text.font_size,
        TokenIdentity::text_font_size, changed, count);
    append_if_changed(before_text.line_height, after_text.line_height,
        TokenIdentity::text_line_height, changed, count);
    return count;
}

bool contains(std::span<const TokenIdentity> identities, TokenIdentity identity) {
    return std::find(identities.begin(), identities.end(), identity)
        != identities.end();
}

} // namespace

struct Subscription::State final {
    bool active{true};
    std::vector<TokenIdentity> identities;
    ThemeScope::InvalidationCallback callback;
};

struct ThemeScope::Subscriber final {
    std::weak_ptr<Subscription::State> state;
};

Subscription::Subscription(std::shared_ptr<State> state) noexcept
    : state_(std::move(state)) {}

Subscription::Subscription(Subscription&& other) noexcept
    : state_(std::move(other.state_)) {}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
    if (this != &other) {
        reset();
        state_ = std::move(other.state_);
    }
    return *this;
}

Subscription::~Subscription() {
    reset();
}

void Subscription::reset() noexcept {
    if (state_) {
        state_->active = false;
        state_.reset();
    }
}

bool Subscription::active() const noexcept {
    return state_ && state_->active;
}

std::shared_ptr<ThemeScope> ThemeScope::create_default() {
    return std::shared_ptr<ThemeScope>(new ThemeScope(nullptr, ThemeConfig{}));
}

std::shared_ptr<ThemeScope> ThemeScope::create(
    std::shared_ptr<ThemeScope> parent,
    ThemeConfig config) {
    if (!parent) {
        throw std::invalid_argument("Nested ThemeScope requires a parent scope");
    }
    parent->ensure_owner_thread();
    auto scope = std::shared_ptr<ThemeScope>(
        new ThemeScope(parent, std::move(config)));
    parent->children_.push_back(scope);
    return scope;
}

ThemeScope::ThemeScope(std::shared_ptr<ThemeScope> parent, ThemeConfig config)
    : parent_(std::move(parent)),
      config_(std::move(config)),
      snapshot_(std::make_shared<ThemeSnapshot>(resolve_theme(
          config_,
          config_.inherit && parent_ ? &parent_->snapshot() : nullptr))),
      owner_thread_(std::this_thread::get_id()) {}

bool ThemeScope::update(const ThemeConfig& config) {
    ensure_owner_thread();
    if (config == config_) {
        ++diagnostics_.snapshot_reuses;
        changed_identity_count_ = 0;
        diagnostics_.changed_identity_count = 0;
        diagnostics_.dirty_phase = DirtyPhase::none;
        return false;
    }
    ThemeSnapshot next = resolve_theme(
        config,
        config.inherit && parent_ ? &parent_->snapshot() : nullptr);
    commit_snapshot(config, std::move(next));
    return true;
}

const ThemeSnapshot& ThemeScope::snapshot() const {
    ensure_owner_thread();
    return *snapshot_;
}

std::uint64_t ThemeScope::generation() const {
    ensure_owner_thread();
    return diagnostics_.generation;
}

Diagnostics ThemeScope::diagnostics() const {
    ensure_owner_thread();
    auto current = diagnostics_;
    current.subscriber_count = live_subscriber_count();
    return current;
}

std::span<const TokenIdentity> ThemeScope::changed_identities() const {
    ensure_owner_thread();
    return {changed_identities_.data(), changed_identity_count_};
}

Subscription ThemeScope::capture(
    InvalidationCallback callback,
    const std::function<void()>& typed_token_reads) {
    ensure_owner_thread();
    if (!callback || !typed_token_reads) {
        throw std::invalid_argument(
            "Theme token capture requires reads and an invalidation callback");
    }
    ReadCapture capture{.scope = this};
    {
        ReadCaptureGuard guard(capture);
        typed_token_reads();
    }
    if (capture.count == 0) {
        throw std::logic_error(
            "Theme token capture did not read a typed token accessor");
    }
    auto state = std::make_shared<Subscription::State>();
    state->identities.assign(
        capture.identities.begin(),
        capture.identities.begin() + static_cast<std::ptrdiff_t>(capture.count));
    state->callback = std::move(callback);
    subscribers_.push_back({state});
    ++diagnostics_.subscription_allocations;
    diagnostics_.subscriber_count = live_subscriber_count();
    return Subscription(std::move(state));
}

const ThemeAliasToken& ThemeScope::alias() const {
    ensure_owner_thread();
    constexpr std::array identities{
        TokenIdentity::alias_color_text,
        TokenIdentity::alias_color_text_secondary,
        TokenIdentity::alias_color_text_disabled,
        TokenIdentity::alias_color_background_container,
        TokenIdentity::alias_color_background_elevated,
        TokenIdentity::alias_color_background_container_disabled,
        TokenIdentity::alias_color_border,
        TokenIdentity::alias_color_border_secondary,
        TokenIdentity::alias_color_focus_outline,
        TokenIdentity::alias_line_width_focus,
        TokenIdentity::alias_focus_outline_offset,
        TokenIdentity::alias_box_shadow,
        TokenIdentity::alias_box_shadow_secondary,
        TokenIdentity::alias_box_shadow_tertiary,
    };
    for (const auto identity : identities) {
        record(identity);
    }
    return snapshot_->alias();
}

const ThemeMapToken& ThemeScope::map() const {
    ensure_owner_thread();
    for (auto identity = TokenIdentity::map_color_primary;
         identity <= TokenIdentity::map_motion_enabled;
         identity = static_cast<TokenIdentity>(static_cast<std::uint8_t>(identity) + 1U)) {
        record(identity);
    }
    return snapshot_->map();
}

const ButtonThemeToken& ThemeScope::button() const {
    ensure_owner_thread();
    for (auto identity = TokenIdentity::button_colors;
         identity <= TokenIdentity::button_shadows;
         identity = static_cast<TokenIdentity>(static_cast<std::uint8_t>(identity) + 1U)) {
        record(identity);
    }
    return snapshot_->button();
}

const TextThemeToken& ThemeScope::text() const {
    ensure_owner_thread();
    for (auto identity = TokenIdentity::text_color;
         identity <= TokenIdentity::text_line_height;
         identity = static_cast<TokenIdentity>(static_cast<std::uint8_t>(identity) + 1U)) {
        record(identity);
    }
    return snapshot_->text();
}

Color ThemeScope::text_color() const {
    ensure_owner_thread();
    record(TokenIdentity::text_color);
    return snapshot_->text().color;
}

SystemFontFamily ThemeScope::text_font_family() const {
    ensure_owner_thread();
    record(TokenIdentity::text_font_family);
    return snapshot_->text().font_family;
}

std::uint32_t ThemeScope::text_font_weight() const {
    ensure_owner_thread();
    record(TokenIdentity::text_font_weight);
    return snapshot_->text().font_weight;
}

float ThemeScope::text_font_size() const {
    ensure_owner_thread();
    record(TokenIdentity::text_font_size);
    return snapshot_->text().font_size;
}

float ThemeScope::text_line_height() const {
    ensure_owner_thread();
    record(TokenIdentity::text_line_height);
    return snapshot_->text().line_height;
}

Color ThemeScope::text_secondary_color() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_color_text_secondary);
    return snapshot_->alias().color_text_secondary;
}

Color ThemeScope::text_disabled_color() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_color_text_disabled);
    return snapshot_->alias().color_text_disabled;
}

const ButtonThemeToken& ThemeScope::button_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::button_colors);
    return snapshot_->button();
}

const ButtonThemeToken& ThemeScope::button_control_heights() const {
    ensure_owner_thread();
    record(TokenIdentity::button_control_heights);
    return snapshot_->button();
}

const ButtonThemeToken& ThemeScope::button_padding_inline() const {
    ensure_owner_thread();
    record(TokenIdentity::button_padding_inline);
    return snapshot_->button();
}

const ButtonThemeToken& ThemeScope::button_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::button_typography);
    return snapshot_->button();
}

const ButtonThemeToken& ThemeScope::button_border_radius() const {
    ensure_owner_thread();
    record(TokenIdentity::button_border_radius);
    return snapshot_->button();
}

float ThemeScope::button_border_width() const {
    ensure_owner_thread();
    record(TokenIdentity::button_border_width);
    return snapshot_->button().border_width;
}

float ThemeScope::button_icon_gap() const {
    ensure_owner_thread();
    record(TokenIdentity::button_icon_gap);
    return snapshot_->button().icon_gap;
}

const ButtonThemeToken& ThemeScope::button_shadows() const {
    ensure_owner_thread();
    record(TokenIdentity::button_shadows);
    return snapshot_->button();
}

Color ThemeScope::focus_outline_color() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_color_focus_outline);
    return snapshot_->alias().color_focus_outline;
}

float ThemeScope::focus_outline_width() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_line_width_focus);
    return snapshot_->alias().line_width_focus;
}

float ThemeScope::focus_outline_offset() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_focus_outline_offset);
    return snapshot_->alias().focus_outline_offset;
}

float ThemeScope::layout_gap_small() const {
    ensure_owner_thread();
    record(TokenIdentity::map_size_xs);
    return snapshot_->map().size_xs;
}

float ThemeScope::layout_gap_middle() const {
    ensure_owner_thread();
    record(TokenIdentity::map_size);
    return snapshot_->map().size;
}

float ThemeScope::layout_gap_large() const {
    ensure_owner_thread();
    record(TokenIdentity::map_size_large);
    return snapshot_->map().size_large;
}

void ThemeScope::ensure_owner_thread() const {
    if (std::this_thread::get_id() != owner_thread_) {
        throw std::logic_error("ThemeScope can only be used on its owner thread");
    }
}

void ThemeScope::record(TokenIdentity identity) const {
    if (active_capture == nullptr || active_capture->scope != this) {
        return;
    }
    const auto present = std::find(
        active_capture->identities.begin(),
        active_capture->identities.begin()
            + static_cast<std::ptrdiff_t>(active_capture->count),
        identity);
    if (present == active_capture->identities.begin()
            + static_cast<std::ptrdiff_t>(active_capture->count)) {
        active_capture->identities[active_capture->count++] = identity;
    }
}

void ThemeScope::recompute_from_parent() {
    ensure_owner_thread();
    if (!config_.inherit) {
        return;
    }
    ThemeSnapshot next = resolve_theme(config_, &parent_->snapshot());
    commit_snapshot(config_, std::move(next));
}

void ThemeScope::commit_snapshot(ThemeConfig config, ThemeSnapshot next) {
    const auto changed = collect_changed(*snapshot_, next, changed_identities_);
    config_ = std::move(config);
    if (next == *snapshot_) {
        ++diagnostics_.snapshot_reuses;
        changed_identity_count_ = 0;
        diagnostics_.changed_identity_count = 0;
        diagnostics_.dirty_phase = DirtyPhase::none;
        return;
    }
    snapshot_ = std::make_shared<ThemeSnapshot>(std::move(next));
    ++diagnostics_.snapshot_allocations;
    ++diagnostics_.generation;
    changed_identity_count_ = changed;
    diagnostics_.changed_identity_count = changed;
    diagnostics_.dirty_phase = DirtyPhase::none;
    for (std::size_t index = 0; index < changed; ++index) {
        diagnostics_.dirty_phase |= dirty_phase_for(changed_identities_[index]);
    }
    notify_subscribers();
    notify_children();
}

void ThemeScope::notify_subscribers() {
    const auto changed = changed_identities();
    std::erase_if(subscribers_, [](const Subscriber& subscriber) {
        const auto state = subscriber.state.lock();
        return !state || !state->active;
    });
    for (const auto& subscriber : subscribers_) {
        const auto state = subscriber.state.lock();
        if (!state || !state->active) {
            continue;
        }
        DirtyPhase phase = DirtyPhase::none;
        for (const auto identity : state->identities) {
            if (contains(changed, identity)) {
                phase |= dirty_phase_for(identity);
            }
        }
        if (phase != DirtyPhase::none) {
            ++diagnostics_.notifications;
            state->callback(phase);
        }
    }
    diagnostics_.subscriber_count = live_subscriber_count();
}

void ThemeScope::notify_children() {
    std::erase_if(children_, [](const auto& child) { return child.expired(); });
    for (const auto& weak_child : children_) {
        if (const auto child = weak_child.lock()) {
            child->recompute_from_parent();
        }
    }
}

std::size_t ThemeScope::live_subscriber_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        subscribers_.begin(),
        subscribers_.end(),
        [](const Subscriber& subscriber) {
            const auto state = subscriber.state.lock();
            return state && state->active;
        }));
}

std::string_view token_identity_name(TokenIdentity identity) noexcept {
    constexpr std::array names{
        "alias.colorText", "alias.colorTextSecondary", "alias.colorTextDisabled",
        "alias.colorBgContainer", "alias.colorBgElevated",
        "alias.colorBgContainerDisabled", "alias.colorBorder",
        "alias.colorBorderSecondary", "alias.colorFocusOutline",
        "alias.lineWidthFocus", "alias.focusOutlineOffset", "alias.boxShadow",
        "alias.boxShadowSecondary", "alias.boxShadowTertiary", "map.colorPrimary",
        "map.colorPrimaryHover", "map.colorPrimaryActive", "map.colorPrimaryBorder",
        "map.colorSuccess", "map.colorWarning", "map.colorError", "map.colorInfo",
        "map.colorTextBase", "map.colorBgBase", "map.fontSizeSM",
        "map.fontSize", "map.fontSizeLG", "map.lineHeightSM", "map.lineHeight",
        "map.lineHeightLG", "map.sizeXS", "map.sizeSM", "map.size",
        "map.sizeLG", "map.controlHeightSM", "map.controlHeight",
        "map.controlHeightLG", "map.borderRadiusSM", "map.borderRadius",
        "map.borderRadiusLG", "map.motionUnit", "map.motionBase", "map.motion",
        "Button.colors", "Button.controlHeights", "Button.paddingInline",
        "Button.typography", "Button.borderRadius", "Button.borderWidth",
        "Button.iconGap", "Button.shadows", "Text.color", "Text.fontFamily",
        "Text.fontWeight", "Text.fontSize", "Text.lineHeight",
    };
    static_assert(names.size() == static_cast<std::size_t>(TokenIdentity::count));
    const auto index = static_cast<std::size_t>(identity);
    return index < names.size() ? names[index] : std::string_view{"invalid"};
}

DirtyPhase dirty_phase_for(TokenIdentity identity) noexcept {
    switch (identity) {
    case TokenIdentity::alias_color_text:
    case TokenIdentity::alias_color_text_secondary:
    case TokenIdentity::alias_color_text_disabled:
    case TokenIdentity::alias_color_background_container:
    case TokenIdentity::alias_color_background_elevated:
    case TokenIdentity::alias_color_background_container_disabled:
    case TokenIdentity::alias_color_border:
    case TokenIdentity::alias_color_border_secondary:
    case TokenIdentity::map_color_primary:
    case TokenIdentity::map_color_primary_hover:
    case TokenIdentity::map_color_primary_active:
    case TokenIdentity::map_color_primary_border:
    case TokenIdentity::map_color_success:
    case TokenIdentity::map_color_warning:
    case TokenIdentity::map_color_error:
    case TokenIdentity::map_color_info:
    case TokenIdentity::map_color_text_base:
    case TokenIdentity::map_color_background_base:
    case TokenIdentity::button_colors:
    case TokenIdentity::text_color:
        return DirtyPhase::paint_material;
    case TokenIdentity::alias_color_focus_outline:
        return DirtyPhase::paint_material;
    case TokenIdentity::alias_line_width_focus:
    case TokenIdentity::alias_focus_outline_offset:
    case TokenIdentity::alias_box_shadow:
    case TokenIdentity::alias_box_shadow_secondary:
    case TokenIdentity::alias_box_shadow_tertiary:
    case TokenIdentity::button_border_radius:
    case TokenIdentity::button_border_width:
    case TokenIdentity::button_shadows:
        return DirtyPhase::geometry | DirtyPhase::paint_material;
    case TokenIdentity::map_font_size_small:
    case TokenIdentity::map_font_size:
    case TokenIdentity::map_font_size_large:
    case TokenIdentity::map_line_height_small:
    case TokenIdentity::map_line_height:
    case TokenIdentity::map_line_height_large:
    case TokenIdentity::button_typography:
    case TokenIdentity::text_font_family:
    case TokenIdentity::text_font_weight:
    case TokenIdentity::text_font_size:
    case TokenIdentity::text_line_height:
        return DirtyPhase::text | DirtyPhase::measure_layout;
    case TokenIdentity::map_size_xs:
    case TokenIdentity::map_size_small:
    case TokenIdentity::map_size:
    case TokenIdentity::map_size_large:
    case TokenIdentity::map_control_height_small:
    case TokenIdentity::map_control_height:
    case TokenIdentity::map_control_height_large:
    case TokenIdentity::button_control_heights:
    case TokenIdentity::button_padding_inline:
    case TokenIdentity::button_icon_gap:
        return DirtyPhase::measure_layout | DirtyPhase::geometry
            | DirtyPhase::hit_test;
    case TokenIdentity::map_border_radius_small:
    case TokenIdentity::map_border_radius:
    case TokenIdentity::map_border_radius_large:
        return DirtyPhase::geometry | DirtyPhase::paint_material;
    case TokenIdentity::map_motion_unit:
    case TokenIdentity::map_motion_base:
    case TokenIdentity::map_motion_enabled:
        return DirtyPhase::animation;
    case TokenIdentity::count:
        return DirtyPhase::none;
    }
    return DirtyPhase::none;
}

} // namespace ryn::theme_runtime
