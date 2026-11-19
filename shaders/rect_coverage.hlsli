#ifndef RYNUI_RECT_COVERAGE
#define RYNUI_RECT_COVERAGE

float RectSampleCoverage(float2 centerOffset, float2 extent, float width) {
    float2 overlap = max(0.0, min(min(width.xx, extent), (extent + width.xx) * 0.5 - abs(centerOffset)));
    return (overlap.x / width) * (overlap.y / width);
}

float RectOutlineSampleCoverage(float2 centerOffset, float2 extent, float stroke, float outset, float width) {
    float outer = RectSampleCoverage(centerOffset, extent + (2.0 * (outset + stroke)).xx, width);
    float inner = RectSampleCoverage(centerOffset, extent + (2.0 * outset).xx, width);
    return saturate(outer - inner);
}

#endif
