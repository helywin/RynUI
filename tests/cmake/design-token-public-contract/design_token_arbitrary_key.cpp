#include <ryn/design_token.hpp>

int main() {
    ryn::TokenMetadata metadata{"colorPrimary", "#1677ff"};
    return static_cast<int>(metadata.stable_id);
}
