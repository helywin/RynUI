#include <ryn/input.hpp>
#include <string>
void contract() {
#if INPUT_CASE == 0
    ryn::Input(ryn::InputProps{}.value(u8"valid"));
#elif INPUT_CASE == 1
    ryn::InputProps{}.value("narrow");
#elif INPUT_CASE == 2
    ryn::InputProps{}.onChange([](std::string) {});
#elif INPUT_CASE == 3
    ryn::Input(ryn::InputProps{}, ryn::InputSuffix{[] {}});
#elif INPUT_CASE == 4
    ryn::InputProps{}.renderer(nullptr);
#elif INPUT_CASE == 5
    ryn::InputProps{}.modifier(nullptr);
#elif INPUT_CASE == 6
    ryn::InputProps{}.window(static_cast<SDL_Window*>(nullptr));
#else
#error Invalid contract case
#endif
}
