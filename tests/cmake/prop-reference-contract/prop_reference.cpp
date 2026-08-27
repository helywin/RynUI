#include <ryn/prop.hpp>

void borrow_stack_value() {
    int value = 7;
    ryn::Prop<int&> borrowed{value};
    static_cast<void>(borrowed);
}
