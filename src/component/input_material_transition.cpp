#include "component/input_material_transition.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ryn::detail {

InputMaterialTransition::InputMaterialTransition(animation::AnimationRuntime& runtime,
    InputMaterialValues initial, std::function<void()> changed)
    : runtime_(&runtime), current_(initial), destination_(initial), changed_(std::move(changed)) {
    scope_ = runtime_->create_scope();
    try {
        for(std::size_t i = 0; i < targets_.size(); ++i)
            targets_[i] = runtime_->register_target(scope_, *this,
                i < input_material_color_count ? animation::AnimationValueKind::color : animation::AnimationValueKind::scalar,
                animation::AnimationDirtyDomain::material | animation::AnimationDirtyDomain::animation);
    } catch(...) {
        runtime_->dispose_scope(scope_);
        throw;
    }
}
InputMaterialTransition::~InputMaterialTransition() {
    try { runtime_->dispose_scope(scope_); } catch(...) {}
}
void InputMaterialTransition::retarget(const InputMaterialValues& next,
    const animation::AnimationSpec& spec, animation::AnimationTime time) {
    const bool same_spec = spec_ && *spec_ == spec;
    for(std::size_t i = 0; i < targets_.size(); ++i) {
        const animation::AnimationValue target = i < input_material_color_count
            ? animation::AnimationValue{next.colors[i]} : animation::AnimationValue{next.shadow_opacity};
        const animation::AnimationValue previous = i < input_material_color_count
            ? animation::AnimationValue{destination_.colors[i]} : animation::AnimationValue{destination_.shadow_opacity};
        if(same_spec && target == previous) continue;
        const animation::AnimationValue current = i < input_material_color_count
            ? animation::AnimationValue{current_.colors[i]} : animation::AnimationValue{current_.shadow_opacity};
        if(runtime_->contains(animations_[i])) {
            runtime_->retarget(animations_[i], target,
                current == target ? animation::AnimationSpec{{}, {}, spec.easing} : spec, time);
        } else if(current != target) {
            animations_[i] = runtime_->play(targets_[i], current, target, spec, time);
        }
    }
    destination_ = next;
    spec_ = spec;
}
std::size_t InputMaterialTransition::active_count() const {
    return std::count_if(animations_.begin(), animations_.end(), [&](auto id) { return runtime_->contains(id); });
}
void InputMaterialTransition::apply(animation::AnimationId, animation::AnimationTargetId target,
    const animation::AnimationValue& value, animation::AnimationDirtyDomain dirty) {
    if(!animation::has_any(dirty, animation::AnimationDirtyDomain::material)
        || !animation::has_any(dirty, animation::AnimationDirtyDomain::animation))
        throw std::logic_error("Input transition lost material/animation domain");
    const auto found = std::find(targets_.begin(), targets_.end(), target);
    if(found == targets_.end()) throw std::out_of_range("Input transition target is stale");
    const auto index = static_cast<std::size_t>(found - targets_.begin());
    if(index < input_material_color_count) current_.colors[index] = std::get<Color>(value);
    else current_.shadow_opacity = std::get<float>(value);
    if(changed_) changed_();
}

} // namespace ryn::detail
