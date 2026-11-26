#pragma once

#include <ryn/design_token.hpp>

namespace ryn::theme_runtime {

// Ant Design 6 uses the rc-virtual-list scrollbar recipe inside Select.
struct SelectScrollbarTokens final {
    static constexpr Color thumb{0, 0, 0, .5F};
    static constexpr float width{8};
    static constexpr float min_height{20};
};

} // namespace ryn::theme_runtime
