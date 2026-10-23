#include "component/popconfirm_model.hpp"
#include <stdexcept>

int main() {
    using ryn::detail::PopconfirmModel;
    const auto check = [](bool value) {
        if (!value) {
            throw std::runtime_error("Popconfirm visibility contract failed");
        }
    };
    PopconfirmModel local{false, false};
    check(!local.open() && local.request(true) && local.open() && !local.request(true));
    check(local.request(false) && !local.open());
    PopconfirmModel controlled{true, true};
    check(controlled.request(false) && controlled.open() && !controlled.request(false));
    // An ignored close must never bypass subsequent boundary suppression.
    check(!controlled.suppress() && !controlled.open() && !controlled.suppress());
    check(controlled.request(true) && controlled.open());
    check(controlled.suppress() && !controlled.open());
    controlled.reconcile(true);
    check(controlled.open() && controlled.request(false));
    controlled.reconcile(false);
    check(!controlled.open() && controlled.request(true) && !controlled.open());
}
