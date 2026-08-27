#include <ryn/design_token.hpp>

int main() {
    ryn::ShadowList shadow{"0 6px 16px rgba(0, 0, 0, 0.08)"};
    return static_cast<int>(shadow.size());
}
