#pragma once
#include <functional>

namespace ryn::detail {
// Isolate one-time structural setup; nested bindings still track normally.
void run_untracked(const std::function<void()>& callback);
} // namespace ryn::detail
