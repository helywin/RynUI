#pragma once

#include "animation/runtime.hpp"

#include <array>
#include <functional>
#include <optional>

namespace ryn::detail {

// Background, border, foreground, affix, caret, placeholder, selection background,
// selection foreground, then the eight typed shadow colors.
inline constexpr std::size_t input_material_color_count = 8 + ShadowList::capacity;
struct InputMaterialValues final {
    std::array<Color, input_material_color_count> colors;
    float shadow_opacity{};
    friend constexpr bool operator==(const InputMaterialValues&, const InputMaterialValues&) = default;
};

class InputMaterialTransition final : private animation::AnimationTargetSink {
public:
    InputMaterialTransition(animation::AnimationRuntime&, InputMaterialValues initial, std::function<void()> changed);
    ~InputMaterialTransition();
    InputMaterialTransition(const InputMaterialTransition&) = delete;
    InputMaterialTransition& operator=(const InputMaterialTransition&) = delete;
    void retarget(const InputMaterialValues&, const animation::AnimationSpec&, animation::AnimationTime);
    [[nodiscard]] const InputMaterialValues& value() const noexcept { return current_; }
    [[nodiscard]] std::size_t active_count() const;
private:
    void apply(animation::AnimationId, animation::AnimationTargetId, const animation::AnimationValue&,
        animation::AnimationDirtyDomain) override;
    animation::AnimationRuntime* runtime_;
    animation::AnimationScopeId scope_;
    std::array<animation::AnimationTargetId, input_material_color_count + 1> targets_;
    std::array<animation::AnimationId, input_material_color_count + 1> animations_;
    InputMaterialValues current_, destination_;
    std::optional<animation::AnimationSpec> spec_;
    std::function<void()> changed_;
};

} // namespace ryn::detail
