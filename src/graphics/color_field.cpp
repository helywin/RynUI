#include "graphics/color_field.hpp"
#include <stdexcept>

namespace ryn::graphics {
std::vector<QuadInstance> sampled_color_field(runtime::Rect b, runtime::Point translation, unsigned columns,
                                              unsigned rows, const std::function<Color(float, float)>& sample) {
    if (!columns || !rows || columns > 256 || rows > 256 || columns * rows > 4096) {
        throw std::invalid_argument("Logical color field sample count is invalid");
    }
    std::vector<QuadInstance> quads;
    quads.reserve(columns * rows);
    for (unsigned y = 0; y < rows; ++y) {
        for (unsigned x = 0; x < columns; ++x) {
            const float u = columns == 1 ? 0 : float(x) / float(columns - 1);
            const float v = rows == 1 ? 0 : float(y) / float(rows - 1);
            const auto color = sample(u, v);
            QuadInstance q;
            const auto left = b.x + b.width * float(x) / float(columns);
            const auto top = b.y + b.height * float(y) / float(rows);
            q.bounds = {left, top, b.x + b.width * float(x + 1) / float(columns) - left,
                        b.y + b.height * float(y + 1) / float(rows) - top};
            q.color = {color.red(), color.green(), color.blue(), color.alpha()};
            q.translation = {translation.x, translation.y};
            quads.push_back(q);
        }
    }
    return quads;
}
} // namespace ryn::graphics
