#include "component/input_material_transition.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ryn::detail {
namespace {

constexpr auto material_kinds() {
    std::array<animation::AnimationValueKind, input_material_color_count + 1> kinds{};
    kinds.fill(animation::AnimationValueKind::color);
    kinds.back() = animation::AnimationValueKind::scalar;
    return kinds;
}

} // namespace

InputMaterialTransition::InputMaterialTransition(animation::AnimationRuntime& runtime,
    InputMaterialValues initial, std::function<void()> changed)
    : runtime_(&runtime),
      targets_(runtime, *this, material_kinds(),
          animation::AnimationDirtyDomain::material | animation::AnimationDirtyDomain::animation),
      current_(initial), destination_(initial), changed_(std::move(changed)) {}
InputMaterialTransition::~InputMaterialTransition() = default;
void InputMaterialTransition::retarget(const InputMaterialValues& next,
    const animation::AnimationSpec& spec, animation::AnimationTime time) {
    const bool same_spec = spec_ && *spec_ == spec;
    for(std::size_t i = 0; i < input_material_color_count + 1; ++i) {
        const animation::AnimationValue target = i < input_material_color_count
            ? animation::AnimationValue{next.colors[i]} : animation::AnimationValue{next.shadow_opacity};
        const animation::AnimationValue previous = i < input_material_color_count
            ? animation::AnimationValue{destination_.colors[i]} : animation::AnimationValue{destination_.shadow_opacity};
        if(same_spec && target == previous) continue;
        const animation::AnimationValue current = i < input_material_color_count
            ? animation::AnimationValue{current_.colors[i]} : animation::AnimationValue{current_.shadow_opacity};
        animation::retarget_material_channel(*runtime_, animations_[i],
            targets_.target(i), current, target, spec, time);
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
    const auto& ids = targets_.targets();
    const auto found = std::find(ids.begin(), ids.end(), target);
    if(found == ids.end()) throw std::out_of_range("Input transition target is stale");
    const auto index = static_cast<std::size_t>(found - ids.begin());
    if(index < input_material_color_count) current_.colors[index] = std::get<Color>(value);
    else current_.shadow_opacity = std::get<float>(value);
    if(changed_) changed_();
}

} // namespace ryn::detail
