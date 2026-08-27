#include <ryn/button.hpp>

void declare_invalid_callback() {
    ryn::ButtonProps{}.onClick([](int) {});
}
