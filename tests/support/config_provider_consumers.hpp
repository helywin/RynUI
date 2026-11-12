#pragma once

#include "component/selection_component.hpp"
#include "i18n_components.hpp"

namespace ryn_test::config_consumers {

struct ConsumerFixture final : i18n_components::Fixture {
    ryn::detail::SelectionComponentHost selections{services};

    ~ConsumerFixture() {
        services.dispose();
    }
};

} // namespace ryn_test::config_consumers
