#include <ryn/confirm_action.hpp>

ryn::ConfirmAction public_confirm_action() {
    return ryn::ConfirmAction::synchronous(
        [] { return ryn::ConfirmActionResult{ryn::ConfirmActionDecision::KeepOpen}; });
}
