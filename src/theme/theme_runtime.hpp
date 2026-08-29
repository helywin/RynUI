#pragma once

#include "theme/theme_runtime_types.hpp"

#include <ryn/theme.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <thread>
#include <vector>

namespace ryn::theme_runtime {

class ThemeScope;

class Subscription final {
public:
    Subscription() = default;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&& other) noexcept;
    Subscription& operator=(Subscription&& other) noexcept;
    ~Subscription();

    void reset() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    friend class ThemeScope;
    struct State;

    explicit Subscription(std::shared_ptr<State> state) noexcept;
    std::shared_ptr<State> state_;
};

class ThemeScope final : public std::enable_shared_from_this<ThemeScope> {
public:
    using InvalidationCallback = std::function<void(DirtyPhase)>;

    [[nodiscard]] static std::shared_ptr<ThemeScope> create_default();
    [[nodiscard]] static std::shared_ptr<ThemeScope> create(
        std::shared_ptr<ThemeScope> parent,
        ThemeConfig config);

    ThemeScope(const ThemeScope&) = delete;
    ThemeScope& operator=(const ThemeScope&) = delete;

    bool update(const ThemeConfig& config);

    [[nodiscard]] const ThemeSnapshot& snapshot() const;
    [[nodiscard]] std::uint64_t generation() const;
    [[nodiscard]] Diagnostics diagnostics() const;
    [[nodiscard]] std::span<const TokenIdentity> changed_identities() const;

    Subscription capture(
        InvalidationCallback callback,
        const std::function<void()>& typed_token_reads);

    [[nodiscard]] const ThemeAliasToken& alias() const;
    [[nodiscard]] const ThemeMapToken& map() const;
    [[nodiscard]] const ButtonThemeToken& button() const;
    [[nodiscard]] const TextThemeToken& text() const;

    [[nodiscard]] Color text_color() const;
    [[nodiscard]] SystemFontFamily text_font_family() const;
    [[nodiscard]] std::uint32_t text_font_weight() const;
    [[nodiscard]] float text_font_size() const;
    [[nodiscard]] float text_line_height() const;
    [[nodiscard]] Color text_secondary_color() const;
    [[nodiscard]] Color text_disabled_color() const;

    [[nodiscard]] const ButtonThemeToken& button_colors() const;
    [[nodiscard]] const ButtonThemeToken& button_control_heights() const;
    [[nodiscard]] const ButtonThemeToken& button_padding_inline() const;
    [[nodiscard]] const ButtonThemeToken& button_typography() const;
    [[nodiscard]] const ButtonThemeToken& button_border_radius() const;
    [[nodiscard]] float button_border_width() const;
    [[nodiscard]] float button_icon_gap() const;
    [[nodiscard]] const ButtonThemeToken& button_shadows() const;
    [[nodiscard]] Color focus_outline_color() const;
    [[nodiscard]] float focus_outline_width() const;
    [[nodiscard]] float focus_outline_offset() const;

    [[nodiscard]] float layout_gap_small() const;
    [[nodiscard]] float layout_gap_middle() const;
    [[nodiscard]] float layout_gap_large() const;
    [[nodiscard]] Duration motion_unit() const;
    [[nodiscard]] Duration motion_base() const;
    [[nodiscard]] bool motion_enabled() const;

private:
    struct Subscriber;

    ThemeScope(std::shared_ptr<ThemeScope> parent, ThemeConfig config);
    void ensure_owner_thread() const;
    void record(TokenIdentity identity) const;
    void recompute_from_parent();
    void commit_snapshot(ThemeConfig config, ThemeSnapshot next);
    void notify_subscribers();
    void notify_children();
    [[nodiscard]] std::size_t live_subscriber_count() const noexcept;

    std::shared_ptr<ThemeScope> parent_;
    ThemeConfig config_;
    std::shared_ptr<const ThemeSnapshot> snapshot_;
    std::vector<std::weak_ptr<ThemeScope>> children_;
    std::vector<Subscriber> subscribers_;
    std::array<TokenIdentity, static_cast<std::size_t>(TokenIdentity::count)>
        changed_identities_{};
    std::size_t changed_identity_count_{0};
    Diagnostics diagnostics_;
    std::thread::id owner_thread_;
};

} // namespace ryn::theme_runtime
