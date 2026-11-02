struct VertexOutput {
    float4 position : SV_Position;
    nointerpolation float4 shapeRect : TEXCOORD0;
    nointerpolation float4 clipBounds : TEXCOORD1;
    nointerpolation float4 color : TEXCOORD2;
    nointerpolation float4 shadowParams : TEXCOORD3;
    nointerpolation float4 effectParams : TEXCOORD4;
    nointerpolation float4 materialParams : TEXCOORD5;
};

VertexOutput VSMain(uint vertexId : SV_VertexID, float4 clipRect : TEXCOORD0, float4 shapeRect : TEXCOORD1,
                    float4 clipBounds : TEXCOORD2, float4 color : TEXCOORD3, float4 shadowParams : TEXCOORD4,
                    float4 effectParams : TEXCOORD5, float4 materialParams : TEXCOORD6) {
    static const float2 corners[6] = {float2(0.0, 0.0), float2(1.0, 0.0), float2(1.0, 1.0),
                                      float2(0.0, 0.0), float2(1.0, 1.0), float2(0.0, 1.0)};

    VertexOutput output;
    output.position = float4(clipRect.xy + corners[vertexId] * clipRect.zw, 0.0, 1.0);
    output.shapeRect = shapeRect;
    output.clipBounds = clipBounds;
    output.color = color;
    output.shadowParams = shadowParams;
    output.effectParams = effectParams;
    output.materialParams = materialParams;
    return output;
}

float RoundedRectDistance(float2 samplePosition, float4 rect, float radius) {
    radius = clamp(radius, 0.0, 0.5 * min(rect.z, rect.w));
    float2 center = rect.xy + 0.5 * rect.zw;
    float2 halfExtent = 0.5 * rect.zw;
    float2 q = abs(samplePosition - center) - (halfExtent - radius.xx);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float ErfApprox(float value) {
    const float signValue = value < 0.0 ? -1.0 : 1.0;
    const float absoluteValue = abs(value);
    const float t = 1.0 / (1.0 + 0.3275911 * absoluteValue);
    const float polynomial =
        (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t;
    return signValue * (1.0 - polynomial * exp(-absoluteValue * absoluteValue));
}

float GaussianEdge(float signedDistance, float sigma, float antialiasWidth) {
    if (sigma <= 0.0) {
        float halfAntialias = 0.5 * antialiasWidth;
        return 1.0 - smoothstep(-halfAntialias, halfAntialias, signedDistance);
    }
    return saturate(0.5 * (1.0 - ErfApprox(signedDistance * 0.7071067811865475 / sigma)));
}

float PositiveMod(float value, float period) {
    float result = fmod(value, period);
    return result < 0.0 ? result + period : result;
}

void BoundaryCandidate(float2 samplePoint, float2 candidate, float along, inout float squareDistance,
                       inout float coordinate) {
    float2 delta = samplePoint - candidate;
    float square = dot(delta, delta);
    if (square < squareDistance) {
        squareDistance = square;
        coordinate = along;
    }
}

void BoundaryLine(float2 samplePoint, float2 first, float2 last, inout float squareDistance, inout float coordinate,
                  inout float perimeter) {
    float2 delta = last - first;
    float extent = length(delta);
    float t = extent > 0.0 ? saturate(dot(samplePoint - first, delta) / (extent * extent)) : 0.0;
    BoundaryCandidate(samplePoint, first + t * delta, perimeter + t * extent, squareDistance, coordinate);
    perimeter += extent;
}

void BoundaryArc(float2 samplePoint, float2 center, float radius, float start, inout float squareDistance,
                 inout float coordinate, inout float perimeter) {
    if (radius <= 0.0) {
        return;
    }
    const float pi = 3.14159265358979323846;
    BoundaryCandidate(samplePoint, center + radius * float2(cos(start), sin(start)), perimeter, squareDistance,
                      coordinate);
    float end = start + pi * 0.5;
    BoundaryCandidate(samplePoint, center + radius * float2(cos(end), sin(end)), perimeter + radius * pi * 0.5,
                      squareDistance, coordinate);
    float2 relative = samplePoint - center;
    float angle = PositiveMod(atan2(relative.y, relative.x) - start, 2.0 * pi);
    if (angle <= pi * 0.5) {
        float theta = start + angle;
        BoundaryCandidate(samplePoint, center + radius * float2(cos(theta), sin(theta)), perimeter + radius * angle,
                          squareDistance, coordinate);
    }
    perimeter += radius * pi * 0.5;
}

void RoundedPathNearest(float2 samplePoint, float4 rect, float radius, uint mask, out float coordinate,
                        out float perimeter) {
    const float pi = 3.14159265358979323846;
    float4 r = float4((mask & 1u) != 0u ? radius : 0.0, (mask & 2u) != 0u ? radius : 0.0,
                      (mask & 4u) != 0u ? radius : 0.0, (mask & 8u) != 0u ? radius : 0.0);
    float x = rect.x;
    float y = rect.y;
    float right = x + rect.z;
    float bottom = y + rect.w;
    float squareDistance = 3.402823466e+38;
    coordinate = 0.0;
    perimeter = 0.0;
    BoundaryLine(samplePoint, float2(x + r.x, y), float2(right - r.y, y), squareDistance, coordinate, perimeter);
    BoundaryArc(samplePoint, float2(right - r.y, y + r.y), r.y, -pi * 0.5, squareDistance, coordinate, perimeter);
    BoundaryLine(samplePoint, float2(right, y + r.y), float2(right, bottom - r.z), squareDistance, coordinate,
                 perimeter);
    BoundaryArc(samplePoint, float2(right - r.z, bottom - r.z), r.z, 0.0, squareDistance, coordinate, perimeter);
    BoundaryLine(samplePoint, float2(right - r.z, bottom), float2(x + r.w, bottom), squareDistance, coordinate,
                 perimeter);
    BoundaryArc(samplePoint, float2(x + r.w, bottom - r.w), r.w, pi * 0.5, squareDistance, coordinate, perimeter);
    BoundaryLine(samplePoint, float2(x, bottom - r.w), float2(x, y + r.x), squareDistance, coordinate, perimeter);
    BoundaryArc(samplePoint, float2(x + r.x, y + r.x), r.x, pi, squareDistance, coordinate, perimeter);
}

float CircularArcDistance(float2 samplePoint, float4 shape, float width, float start, float sweep) {
    const float fullTurn = 6.28318530717958647692;
    if (sweep <= 0.0 || width <= 0.0) {
        return 1.0e30;
    }
    float radius = shape.z * 0.5;
    float2 relative = samplePoint - (shape.xy + shape.zw * 0.5);
    float distance = abs(length(relative) - radius);
    if (distance > width * 0.5 + 0.5) {
        return distance - width * 0.5;
    }
    float angle = PositiveMod(atan2(relative.y, relative.x) - start, fullTurn);
    if (sweep < fullTurn && angle > sweep) {
        float2 first = radius * float2(cos(start), sin(start));
        float end = start + sweep;
        float2 last = radius * float2(cos(end), sin(end));
        distance = min(length(relative - first), length(relative - last));
    }
    return distance - width * 0.5;
}

float DashedOutlineCoverage(float2 samplePoint, float4 shape, float radius, float width, float3 dash, uint mask,
                            float antialiasWidth) {
    float2 center = shape.xy + shape.zw * 0.5;
    bool right = samplePoint.x >= center.x;
    bool bottom = samplePoint.y >= center.y;
    uint corner = bottom ? (right ? 2u : 3u) : (right ? 1u : 0u);
    float cornerRadius = (mask & (1u << corner)) != 0u ? radius : 0.0;
    float outer = GaussianEdge(RoundedRectDistance(samplePoint, shape, cornerRadius), 0.0, antialiasWidth);
    float4 inner = float4(shape.xy + width.xx, shape.zw - 2.0 * width.xx);
    float inside =
        min(inner.z, inner.w) > 0.0
            ? GaussianEdge(RoundedRectDistance(samplePoint, inner, max(0.0, cornerRadius - width)), 0.0, antialiasWidth)
            : 0.0;
    float band = outer * (1.0 - inside);
    if (band == 0.0 || dash.y == 0.0) {
        return band;
    }
    float4 centerline = float4(shape.xy + 0.5 * width.xx, shape.zw - width.xx);
    float coordinate;
    float perimeter;
    RoundedPathNearest(samplePoint, centerline, max(0.0, radius - width * 0.5), mask, coordinate, perimeter);
    // The packer fitted the period to the complete logical perimeter once.
    float period = dash.x + dash.y;
    float phase = PositiveMod(coordinate + dash.z, period);
    float distance = phase <= dash.x ? min(phase, dash.x - phase) : -min(phase - dash.x, period - phase);
    return band * smoothstep(-0.5 * antialiasWidth, 0.5 * antialiasWidth, distance);
}

float TooltipDistance(float2 samplePoint, float4 shape, float radius, float3 arrow, uint side) {
    const float pi = 3.14159265358979323846;
    samplePoint -= shape.xy;
    float width = shape.z;
    float height = shape.w;
    float center = arrow.x;
    if (side == 1u) {
        samplePoint = float2(width, height) - samplePoint;
        center = width - center;
    } else if (side == 2u) {
        samplePoint = float2(height - samplePoint.y, samplePoint.x);
        center = height - center;
        float swap = width;
        width = height;
        height = swap;
    } else if (side == 3u) {
        samplePoint = float2(samplePoint.y, width - samplePoint.x);
        float swap = width;
        width = height;
        height = swap;
    }
    float bodyDistance = RoundedRectDistance(samplePoint, float4(0.0, 0.0, width, height), radius);
    if (bodyDistance <= -0.5) {
        return bodyDistance;
    }
    float squareDistance = 3.402823466e+38;
    float coordinate = 0.0;
    float perimeter = 0.0;
    BoundaryLine(samplePoint, float2(radius, 0.0), float2(width - radius, 0.0), squareDistance, coordinate, perimeter);
    BoundaryArc(samplePoint, float2(width - radius, radius), radius, -pi * 0.5, squareDistance, coordinate, perimeter);
    BoundaryLine(samplePoint, float2(width, radius), float2(width, height - radius), squareDistance, coordinate,
                 perimeter);
    BoundaryArc(samplePoint, float2(width - radius, height - radius), radius, 0.0, squareDistance, coordinate,
                perimeter);
    BoundaryLine(samplePoint, float2(width - radius, height), float2(center + arrow.y, height), squareDistance,
                 coordinate, perimeter);
    BoundaryLine(samplePoint, float2(center + arrow.y, height), float2(center, height + arrow.z), squareDistance,
                 coordinate, perimeter);
    BoundaryLine(samplePoint, float2(center, height + arrow.z), float2(center - arrow.y, height), squareDistance,
                 coordinate, perimeter);
    BoundaryLine(samplePoint, float2(center - arrow.y, height), float2(radius, height), squareDistance, coordinate,
                 perimeter);
    BoundaryArc(samplePoint, float2(radius, height - radius), radius, pi * 0.5, squareDistance, coordinate, perimeter);
    BoundaryLine(samplePoint, float2(0.0, height - radius), float2(0.0, radius), squareDistance, coordinate, perimeter);
    BoundaryArc(samplePoint, float2(radius, radius), radius, pi, squareDistance, coordinate, perimeter);
    bool inBody = bodyDistance <= 0.0;
    bool inArrow = samplePoint.y >= height && samplePoint.y <= height + arrow.z &&
                   abs(samplePoint.x - center) <= arrow.y * (1.0 - (samplePoint.y - height) / arrow.z);
    float distance = sqrt(squareDistance);
    return inBody || inArrow ? -distance : distance;
}

float4 PSMain(VertexOutput input) : SV_Target0 {
    float2 samplePosition = input.position.xy;
    float clipDistance = min(min(samplePosition.x - input.clipBounds.x, input.clipBounds.z - samplePosition.x),
                             min(samplePosition.y - input.clipBounds.y, input.clipBounds.w - samplePosition.y));
    clip(clipDistance);

    float4 shape = input.shapeRect;
    float radius = input.shadowParams.x;
    float2 offset = input.shadowParams.yz;
    float blur = input.shadowParams.w;
    float spread = input.effectParams.x;
    float outlineWidth = input.effectParams.y;
    float outlineOffset = input.effectParams.z;
    float kind = input.effectParams.w;
    float coverage = 0.0;

    if (kind < 0.5) {
        shape.xy -= spread.xx;
        shape.zw += 2.0 * spread.xx;
        clip(min(shape.z, shape.w));
        radius = clamp(radius + spread, 0.0, 0.5 * min(shape.z, shape.w));
        shape.xy += offset;
        coverage = GaussianEdge(RoundedRectDistance(samplePosition, shape, radius), 0.5 * blur, input.materialParams.y);
    } else if (kind < 1.5) {
        float surfaceDistance = RoundedRectDistance(samplePosition, shape, radius);
        float surfaceCoverage = GaussianEdge(surfaceDistance, 0.0, input.materialParams.y);
        shape.xy += offset;
        float distanceInside = -RoundedRectDistance(samplePosition, shape, radius) - spread;
        coverage = surfaceCoverage * GaussianEdge(distanceInside, 0.5 * blur, input.materialParams.y);
    } else if (kind < 2.5) {
        float distance = RoundedRectDistance(samplePosition, shape, radius);
        float halfAntialias = 0.5 * input.materialParams.y;
        float inner = smoothstep(outlineOffset - halfAntialias, outlineOffset + halfAntialias, distance);
        float outer = 1.0 - smoothstep(outlineOffset + outlineWidth - halfAntialias,
                                       outlineOffset + outlineWidth + halfAntialias, distance);
        coverage = saturate(inner * outer);
    } else if (kind < 3.5) {
        coverage = GaussianEdge(CircularArcDistance(samplePosition, shape, outlineWidth, offset.x, offset.y), 0.0,
                                input.materialParams.y);
    } else if (kind < 4.5) {
        coverage = DashedOutlineCoverage(samplePosition, shape, radius, outlineWidth, input.shadowParams.yzw,
                                         (uint)input.materialParams.z, input.materialParams.y);
    } else {
        coverage = GaussianEdge(
            TooltipDistance(samplePosition, shape, radius, input.shadowParams.yzw, (uint)input.materialParams.z), 0.0,
            input.materialParams.y);
    }

    return float4(input.color.rgb, input.color.a * input.materialParams.x * coverage);
}
