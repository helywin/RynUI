#ifndef RYNUI_ROUNDED_EFFECT_CAP_UNION
#define RYNUI_ROUNDED_EFFECT_CAP_UNION

#include "cap_number.hlsli"

// Fixed 4 primitives / 16 boundaries / 34 events. Global capability stays OFF
// until predicate/error certification, pinned roundtrip and real GPU results.
struct CapPrimitive {
    uint rectangle;
    uint reversed;
    uint largeSweep;
    CapPoint firstDirection;
    CapPoint lastDirection;
};

struct CapBoundary {
    uint circular;
    uint owner;
    uint support;
    int radiusCoefficient;
    int widthCoefficient;
    CapNumber radius;
    CapPoint first;
    CapPoint last;
    CapPoint outward;
    CapPoint firstDirection;
    CapPoint lastDirection;
    uint largeSweep;
};

struct CapEvent {
    CapNumber parameter;
    CapPoint direction;
};

bool CapPointZero(CapPoint point) {
    return CapZero(point.x) && CapZero(point.y);
}

bool CapInUnit(CapNumber value) {
    return value.negative == 0u && CapCompare(value, CapSmall(1.0)) <= 0;
}

bool CapAngularContains(CapPoint first, CapPoint last, bool largeSweep, CapPoint point, bool strict) {
    int a = CapCompare(CapCross(first, point), (CapNumber)0);
    int b = CapCompare(CapCross(point, last), (CapNumber)0);
    return largeSweep ? (strict ? a > 0 || b > 0 : a >= 0 || b >= 0) : (strict ? a > 0 && b > 0 : a >= 0 && b >= 0);
}

CapNumber CapRadialDelta(CapPoint point, CapNumber radius) {
    if (CapMagnitudeCompare(point.x, point.y) < 0) {
        CapNumber temporary = point.x;
        point.x = point.y;
        point.y = temporary;
    }
    CapNumber axis = CapAbs(point.x);
    CapNumber numerator =
        CapAdd(CapMultiply(CapSubtract(axis, radius), CapAdd(axis, radius)), CapMultiply(point.y, point.y));
    return CapDivide(numerator, CapAdd(CapLength(point), radius));
}

CapNumber CapCircleDelta(CapBoundary boundary, CapPoint point, CapNumber radius, CapNumber halfWidth) {
    CapNumber delta = CapRadialDelta(point, radius);
    if (boundary.radiusCoefficient == -1) {
        delta = CapAdd(delta, CapScale2(radius, 1));
    }
    if (boundary.widthCoefficient > 0) {
        return CapSubtract(delta, halfWidth);
    }
    return CapAdd(delta, halfWidth);
}

bool CapContains(CapPrimitive primitive, CapPoint point, CapNumber radius, CapNumber halfWidth, bool strict) {
    if (primitive.rectangle != 0u) {
        CapNumber radial = CapAbs(CapSubtract(CapDot(point, primitive.firstDirection), radius));
        CapNumber tangent = CapDot(point, primitive.lastDirection);
        int x = CapCompare(radial, halfWidth);
        CapNumber low = (CapNumber)0;
        CapNumber high = halfWidth;
        if (primitive.reversed == 0u) {
            low = CapNegate(halfWidth);
            high = (CapNumber)0;
        }
        int a = CapCompare(tangent, low);
        int b = CapCompare(tangent, high);
        return strict ? x < 0 && a > 0 && b < 0 : x <= 0 && a >= 0 && b <= 0;
    }
    CapNumber delta = CapRadialDelta(point, radius);
    if (primitive.reversed != 0u) {
        int edge = CapCompare(CapAdd(delta, CapScale2(radius, 1)), halfWidth);
        if (strict ? edge >= 0 : edge > 0) {
            return false;
        }
    } else {
        int outer = CapCompare(delta, halfWidth);
        bool hole = CapCompare(radius, halfWidth) > 0;
        int inner = CapCompare(delta, CapNegate(halfWidth));
        if ((strict ? outer >= 0 : outer > 0) || (hole && (strict ? inner <= 0 : inner < 0))) {
            return false;
        }
    }
    if (CapPointZero(point)) {
        return !strict;
    }
    return CapAngularContains(primitive.firstDirection, primitive.lastDirection, primitive.largeSweep != 0u, point,
                              strict);
}

CapPoint CapAt(CapBoundary boundary, CapEvent event) {
    if (boundary.circular != 0u) {
        return CapPointScale(event.direction, boundary.radius);
    }
    return CapPointAdd(boundary.first, CapPointScale(CapPointSubtract(boundary.last, boundary.first), event.parameter));
}

CapNumber CapLineParameter(CapBoundary boundary, CapPoint point) {
    CapPoint axis = CapPointSubtract(boundary.last, boundary.first);
    return CapDivide(CapDot(CapPointSubtract(point, boundary.first), axis), CapDot(axis, axis));
}

bool CapAngleLess(CapPoint origin, CapPoint first, CapPoint second) {
    int a = CapCompare(CapCross(origin, first), (CapNumber)0);
    int b = CapCompare(CapCross(origin, second), (CapNumber)0);
    int ax = CapCompare(CapDot(origin, first), (CapNumber)0);
    int bx = CapCompare(CapDot(origin, second), (CapNumber)0);
    uint firstHalf = a < 0 || (a == 0 && ax < 0) ? 1u : 0u;
    uint secondHalf = b < 0 || (b == 0 && bx < 0) ? 1u : 0u;
    return firstHalf != secondHalf ? firstHalf < secondHalf : CapCompare(CapCross(first, second), (CapNumber)0) > 0;
}

bool CapEventLess(CapBoundary boundary, CapEvent first, CapEvent second) {
    return boundary.circular != 0u ? CapAngleLess(boundary.firstDirection, first.direction, second.direction)
                                   : CapCompare(first.parameter, second.parameter) < 0;
}

CapEvent CapMidpoint(CapBoundary boundary, CapEvent first, CapEvent last) {
    CapEvent result = (CapEvent)0;
    if (boundary.circular == 0u) {
        result.parameter = CapScale2(CapAdd(first.parameter, last.parameter), -1);
        return result;
    }
    CapPoint sum = CapPointAdd(first.direction, last.direction);
    if (CapPointZero(sum)) {
        result.direction = CapPerpendicular(first.direction);
    } else {
        result.direction = CapUnit(sum);
        if (CapCompare(CapCross(first.direction, last.direction), (CapNumber)0) < 0) {
            result.direction = CapPointNegate(result.direction);
        }
    }
    return result;
}

CapNumber CapClosest(CapBoundary boundary, CapPoint point, CapEvent low, CapEvent high, CapNumber radius,
                     CapNumber halfWidth) {
    CapNumber nearest = CapMin(CapLength(CapPointSubtract(point, CapAt(boundary, low))),
                               CapLength(CapPointSubtract(point, CapAt(boundary, high))));
    if (boundary.circular != 0u) {
        bool large = CapCompare(CapCross(low.direction, high.direction), (CapNumber)0) < 0;
        if (CapPointZero(point) || CapAngularContains(low.direction, high.direction, large, point, false)) {
            nearest = CapMin(nearest, CapAbs(CapCircleDelta(boundary, point, radius, halfWidth)));
        }
    } else {
        CapEvent event = (CapEvent)0;
        event.parameter = CapMax(low.parameter, CapMin(high.parameter, CapLineParameter(boundary, point)));
        nearest = CapMin(nearest, CapLength(CapPointSubtract(point, CapAt(boundary, event))));
    }
    return nearest;
}

void CapAddLineEvent(inout CapEvent events[34], inout uint count, CapNumber parameter) {
    if (count < 34u && CapInUnit(parameter)) {
        CapEvent event = (CapEvent)0;
        event.parameter = parameter;
        events[count++] = event;
    }
}

void CapAddCircleEvent(CapBoundary boundary, inout CapEvent events[34], inout uint count, CapPoint direction) {
    if (count < 34u && CapAngularContains(boundary.firstDirection, boundary.lastDirection, boundary.largeSweep != 0u,
                                          direction, false)) {
        CapEvent event = (CapEvent)0;
        event.direction = direction;
        events[count++] = event;
    }
}

bool CapSameLine(CapBoundary first, CapBoundary second) {
    if (first.support == second.support) {
        return true;
    }
    CapPoint axis = CapPointSubtract(first.last, first.first);
    return CapZero(CapCross(axis, CapPointSubtract(second.last, second.first))) &&
           CapZero(CapCross(axis, CapPointSubtract(second.first, first.first)));
}

void CapIntersections(CapBoundary first, CapBoundary second, inout CapEvent events[34], inout uint count,
                      CapNumber radius, CapNumber halfWidth) {
    if (first.circular == 0u && second.circular == 0u) {
        CapPoint axis = CapPointSubtract(first.last, first.first);
        CapPoint other = CapPointSubtract(second.last, second.first);
        CapPoint offset = CapPointSubtract(second.first, first.first);
        if (first.support == second.support) {
            CapAddLineEvent(events, count, CapLineParameter(first, second.first));
            CapAddLineEvent(events, count, CapLineParameter(first, second.last));
            return;
        }
        CapNumber determinant = CapCross(axis, other);
        if (!CapZero(determinant)) {
            CapNumber alongOther = CapDivide(CapCross(offset, axis), determinant);
            if (CapInUnit(alongOther)) {
                CapAddLineEvent(events, count, CapDivide(CapCross(offset, other), determinant));
            }
        } else if (CapSameLine(first, second)) {
            CapAddLineEvent(events, count, CapLineParameter(first, second.first));
            CapAddLineEvent(events, count, CapLineParameter(first, second.last));
        }
        return;
    }
    if (first.circular != 0u && second.circular != 0u) {
        // Same center; only identical RadiusExpr is a common circle.
        if (first.support == second.support) {
            CapAddCircleEvent(first, events, count, second.firstDirection);
            CapAddCircleEvent(first, events, count, second.lastDirection);
        }
        return;
    }
    CapBoundary circle = first;
    CapBoundary line = second;
    if (first.circular == 0u) {
        circle = second;
        line = first;
    }
    CapPoint axis = CapPointSubtract(line.last, line.first);
    CapNumber length = CapLength(axis);
    CapPoint direction = CapPointScale(axis, CapDivide(CapSmall(1.0), length));
    CapPoint normal = CapPerpendicular(direction);
    CapNumber offset = CapDot(line.first, normal);
    CapNumber projection = CapDot(line.first, direction);
    // (rho-|d|)*(rho+|d|); preserve RadiusExpr across cancellation.
    CapNumber gap = CapSubtract(CapMultiply(CapSmall((float)circle.radiusCoefficient), radius), CapAbs(offset));
    if (circle.widthCoefficient > 0) {
        gap = CapAdd(gap, halfWidth);
    } else {
        gap = CapSubtract(gap, halfWidth);
    }
    if (gap.negative != 0u) {
        return;
    }
    CapNumber chord = CapSqrt(CapMultiply(gap, CapAdd(circle.radius, CapAbs(offset))));
    [loop] for (uint side = 0u; side < 2u; ++side) {
        CapNumber along = chord;
        if (side != 0u) {
            along = CapNegate(chord);
        }
        CapNumber parameter = CapDivide(CapSubtract(along, projection), length);
        if (CapInUnit(parameter)) {
            CapPoint point = CapPointAdd(CapPointScale(normal, offset), CapPointScale(direction, along));
            if (CapAngularContains(circle.firstDirection, circle.lastDirection, circle.largeSweep != 0u, point,
                                   false)) {
                if (first.circular != 0u) {
                    CapAddCircleEvent(first, events, count, CapUnit(point));
                } else {
                    CapAddLineEvent(events, count, parameter);
                }
            }
        }
    }
}

void CapLine(inout CapBoundary boundaries[16], inout uint count, uint owner, uint support, CapPoint first,
             CapPoint last, CapPoint outward) {
    if (!CapPointZero(CapPointSubtract(first, last))) {
        CapBoundary boundary = (CapBoundary)0;
        boundary.owner = owner;
        boundary.support = support;
        boundary.first = first;
        boundary.last = last;
        boundary.outward = outward;
        boundaries[count++] = boundary;
    }
}

void CapSector(inout CapPrimitive primitives[4], inout uint primitiveCount, inout CapBoundary boundaries[16],
               inout uint boundaryCount, CapNumber radius, CapNumber halfWidth, CapPoint first, CapPoint last,
               bool largeSweep, bool reversed) {
    uint owner = primitiveCount++;
    CapPrimitive primitive = (CapPrimitive)0;
    primitive.firstDirection = first;
    primitive.lastDirection = last;
    primitive.largeSweep = largeSweep ? 1u : 0u;
    primitive.reversed = reversed ? 1u : 0u;
    primitives[owner] = primitive;
    CapNumber inner = CapMax((CapNumber)0, CapSubtract(radius, halfWidth));
    CapNumber outer = CapAdd(radius, halfWidth);
    if (reversed) {
        inner = (CapNumber)0;
        outer = CapSubtract(halfWidth, radius);
    }
    CapBoundary boundary = (CapBoundary)0;
    boundary.circular = 1u;
    boundary.owner = owner;
    boundary.support = reversed ? 3u : 1u;
    boundary.radius = outer;
    boundary.radiusCoefficient = reversed ? -1 : 1;
    boundary.widthCoefficient = 1;
    boundary.firstDirection = first;
    boundary.lastDirection = last;
    boundary.largeSweep = primitive.largeSweep;
    boundaries[boundaryCount++] = boundary;
    if (!CapZero(inner)) {
        boundary.support = 2u;
        boundary.radius = inner;
        boundary.widthCoefficient = -1;
        boundaries[boundaryCount++] = boundary;
    }
    CapLine(boundaries, boundaryCount, owner, 10u, CapPointScale(first, inner), CapPointScale(first, outer),
            CapPointNegate(CapPerpendicular(first)));
    CapLine(boundaries, boundaryCount, owner, 11u, CapPointScale(last, inner), CapPointScale(last, outer),
            CapPerpendicular(last));
}

void CapSquare(inout CapPrimitive primitives[4], inout uint primitiveCount, inout CapBoundary boundaries[16],
               inout uint boundaryCount, CapNumber radius, CapNumber halfWidth, CapPoint radial, bool end) {
    uint owner = primitiveCount++;
    CapPoint tangent = CapPerpendicular(radial);
    CapNumber along = halfWidth;
    if (!end) {
        along = CapNegate(halfWidth);
    }
    CapPrimitive primitive = (CapPrimitive)0;
    primitive.rectangle = 1u;
    primitive.reversed = end ? 1u : 0u;
    primitive.firstDirection = radial;
    primitive.lastDirection = tangent;
    primitives[owner] = primitive;
    CapNumber inner = CapSubtract(radius, halfWidth);
    CapNumber outer = CapAdd(radius, halfWidth);
    CapPoint a = CapPointScale(radial, inner);
    CapPoint b = CapPointScale(radial, outer);
    CapPoint offset = CapPointScale(tangent, along);
    CapPoint c = CapPointAdd(b, offset);
    CapPoint d = CapPointAdd(a, offset);
    uint join = end ? 11u : 10u;
    uint radialSupport = end ? 22u : 20u;
    CapPoint outwardJoin = tangent;
    if (end) {
        outwardJoin = CapPointNegate(tangent);
    }
    CapLine(boundaries, boundaryCount, owner, join, a, b, outwardJoin);
    CapLine(boundaries, boundaryCount, owner, radialSupport + 1u, b, c, radial);
    CapLine(boundaries, boundaryCount, owner, end ? 13u : 12u, c, d, CapPointNegate(outwardJoin));
    CapLine(boundaries, boundaryCount, owner, radialSupport, d, a, CapPointNegate(radial));
}

bool CapExposed(CapPrimitive primitives[4], uint primitiveCount, CapBoundary boundaries[16], uint boundaryCount,
                uint index, CapPoint point, CapNumber radius, CapNumber halfWidth) {
    CapBoundary boundary = boundaries[index];
    [loop] for (uint owner = 0u; owner < primitiveCount; ++owner) {
        if (owner != boundary.owner && CapContains(primitives[owner], point, radius, halfWidth, true)) {
            return false;
        }
    }
    if (boundary.circular == 0u) {
        [loop] for (uint other = 0u; other < boundaryCount; ++other) {
            if (boundaries[other].owner != boundary.owner && boundaries[other].circular == 0u &&
                CapSameLine(boundary, boundaries[other]) && CapInUnit(CapLineParameter(boundaries[other], point)) &&
                CapCompare(CapDot(boundary.outward, boundaries[other].outward), (CapNumber)0) < 0) {
                return false;
            }
        }
    }
    return true;
}

float CapCoverage(CapNumber signedDistance) {
    if (CapCompare(signedDistance, CapSmall(0.5)) >= 0) {
        return 0.0;
    }
    if (CapCompare(signedDistance, CapSmall(-0.5)) <= 0) {
        return 1.0;
    }
    return GaussianEdge(CapToFloat(signedDistance), 0.0, 1.0);
}

float NonroundArcCoverageRaw(float2 samplePosition, uint4 rect, uint4 strokeScale, uint startBits, uint sweepBits,
                             uint cap) {
    // Decode only after integer zero/positive checks; positive subnormals survive.
    if ((rect.z & 0x7fffffffu) == 0u || (strokeScale.x & 0x7fffffffu) == 0u || (sweepBits & 0x7fffffffu) == 0u) {
        return 0.0;
    }
    CapNumber deviceScale = CapFromBits(strokeScale.y);
    CapNumber radius = CapMultiply(CapScale2(CapFromBits(rect.z), -1), deviceScale);
    CapNumber halfWidth = CapMultiply(CapScale2(CapFromBits(strokeScale.x), -1), deviceScale);
    CapPoint point;
    point.x = CapSubtract(
        CapSubtract(CapFromBits(asuint(samplePosition.x)), CapMultiply(CapFromBits(rect.x), deviceScale)), radius);
    point.y = CapSubtract(
        CapSubtract(CapFromBits(asuint(samplePosition.y)), CapMultiply(CapFromBits(rect.y), deviceScale)), radius);
    if (sweepBits == asuint(6.28318530717958647692)) {
        CapNumber delta = CapRadialDelta(point, radius);
        if (CapCompare(radius, halfWidth) > 0) {
            return CapCoverage(CapSubtract(CapAbs(delta), halfWidth));
        }
        return CapCoverage(CapSubtract(delta, halfWidth));
    }
    CapNumber start = CapFromBits(startBits);
    CapNumber sweep = CapFromBits(sweepBits);
    CapPoint first = CapDirection(start);
    CapPoint last = CapDirection(CapAdd(start, sweep));
    bool largeSweep = CapCompare(sweep, CapPi()) > 0;
    CapPrimitive primitives[4];
    CapBoundary boundaries[16];
    uint primitiveCount = 0u;
    uint boundaryCount = 0u;
    CapSector(primitives, primitiveCount, boundaries, boundaryCount, radius, halfWidth, first, last, largeSweep, false);
    if (CapCompare(halfWidth, radius) > 0) {
        CapSector(primitives, primitiveCount, boundaries, boundaryCount, radius, halfWidth, CapPointNegate(first),
                  CapPointNegate(last), largeSweep, true);
    }
    if (cap == 2u) {
        CapSquare(primitives, primitiveCount, boundaries, boundaryCount, radius, halfWidth, first, false);
        CapSquare(primitives, primitiveCount, boundaries, boundaryCount, radius, halfWidth, last, true);
    }
    bool inside = false;
    CapNumber primitiveNearest = CapScale2(CapSmall(1.0), 1024);
    [loop] for (uint owner = 0u; owner < primitiveCount; ++owner) {
        bool contained = CapContains(primitives[owner], point, radius, halfWidth, false);
        inside = inside || contained;
        CapNumber nearest = CapScale2(CapSmall(1.0), 1024);
        [loop] for (uint index = 0u; index < boundaryCount; ++index) {
            if (boundaries[index].owner == owner) {
                CapEvent low = (CapEvent)0;
                CapEvent high = (CapEvent)0;
                high.parameter = CapSmall(1.0);
                low.direction = boundaries[index].firstDirection;
                high.direction = boundaries[index].lastDirection;
                nearest = CapMin(nearest, CapClosest(boundaries[index], point, low, high, radius, halfWidth));
            }
        }
        primitiveNearest = CapMin(primitiveNearest, nearest);
        // Saturated interior, with a full physical pixel margin. This is not
        // min-SDF union distance; total arithmetic error certification is pending.
        if (contained && CapCompare(nearest, CapSmall(1.0)) >= 0) {
            return 1.0;
        }
    }
    if (!inside) {
        return CapCoverage(primitiveNearest);
    }
    CapNumber nearest = CapScale2(CapSmall(1.0), 1024);
    [loop] for (uint index = 0u; index < boundaryCount; ++index) {
        CapEvent events[34];
        events[0] = (CapEvent)0;
        events[1] = (CapEvent)0;
        events[0].direction = boundaries[index].firstDirection;
        events[1].direction = boundaries[index].lastDirection;
        events[1].parameter = CapSmall(1.0);
        uint count = 2u;
        [loop] for (uint other = 0u; other < boundaryCount; ++other) {
            if (other != index) {
                CapIntersections(boundaries[index], boundaries[other], events, count, radius, halfWidth);
            }
        }
        [loop] for (uint entry = 1u; entry < count; ++entry) {
            CapEvent value = events[entry];
            uint cursor = entry;
            [loop] for (uint step = 0u; step < 33u && cursor > 0u; ++step) {
                if (!CapEventLess(boundaries[index], value, events[cursor - 1u])) {
                    break;
                }
                events[cursor] = events[cursor - 1u];
                --cursor;
            }
            events[cursor] = value;
        }
        [loop] for (uint part = 1u; part < count; ++part) {
            CapEvent low = events[part - 1u];
            CapEvent high = events[part];
            if (CapEventLess(boundaries[index], low, high)) {
                CapPoint middle = CapAt(boundaries[index], CapMidpoint(boundaries[index], low, high));
                if (CapExposed(primitives, primitiveCount, boundaries, boundaryCount, index, middle, radius,
                               halfWidth)) {
                    nearest = CapMin(nearest, CapClosest(boundaries[index], point, low, high, radius, halfWidth));
                }
            }
        }
    }
    return CapCoverage(CapNegate(nearest));
}

#endif
