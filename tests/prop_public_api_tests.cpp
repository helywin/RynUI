#include <ryn/prop.hpp>

#include <concepts>

namespace {

template <typename T>
concept SupportsProp = requires {
    typename ryn::Prop<T>;
};

struct DecadeEqual final {
    bool operator()(int left, int right) const noexcept {
        return left / 10 == right / 10;
    }
};

static_assert(SupportsProp<int>);
static_assert(!SupportsProp<int&>);
static_assert(!SupportsProp<const int>);

} // namespace

int main() {
    ryn::Signal<int, DecadeEqual> source{10, DecadeEqual{}};
    ryn::Prop<int> static_prop{4};
    ryn::Prop<int> signal_prop{source};
    ryn::Prop<int> binding_prop{ryn::bind([source] {
        return source.get() * 2;
    })};

    static_cast<void>(static_prop);
    static_cast<void>(signal_prop);
    static_cast<void>(binding_prop);
    return source.set(20) ? 0 : 1;
}
