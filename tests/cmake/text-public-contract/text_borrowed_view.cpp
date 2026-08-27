#include <ryn/text.hpp>

void declare_invalid_text(ryn::StringView borrowed) {
    ryn::Text(borrowed);
}
