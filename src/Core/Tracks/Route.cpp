#include "Core/Tracks/Route.h"

#include "Core/Base/X87.h"

#include <cmath>

namespace oj {

namespace {

// 0x004A0FD0 on one axis: Catmull-Rom from v1 to v2 through v0 and v3, in the stock order of the sums.
float catmullRom(float v0, float v1, float v2, float v3, float t) {
    const float c3 = (((v3 * 0.5f - v2 * 1.5f) + v1 * 1.5f) - v0 * 0.5f) * t;
    const float c2 = (c3 + ((((v2 + v2) - v3 * 0.5f) - v1 * 2.5f) + v0)) * t;
    const float c1 = (c2 + (((v2 * 0.5f + v1 * 0.0f) + v3 * 0.0f) - v0 * 0.5f)) * t;
    return c1 + (((v2 + v0) * 0.0f + v3 * 0.0f) + v1);
}

// 0x004A11D0 on one axis: the curve's slope at t.
float catmullRomSlope(float v0, float v1, float v2, float v3, float t) {
    const float a = ((((v3 * 0.5f - v2 * 1.5f) + v1 * 1.5f) - v0 * 0.5f) * t) * 3.0f;
    float b = (((v2 + v2) - v3 * 0.5f) - v1 * 2.5f) + v0;
    b = b + b;
    const float c = (a + b) * t;
    return c + (((v3 + v1) * 0.0f + v2 * 0.5f) - v0 * 0.5f);
}

}  // namespace

Route::Route(const Spl& spl) {
    const std::vector<SplPoint>* read[3] = {&spl.fast, &spl.attack, &spl.centre};
    for (int l = 0; l < 3; ++l) {
        for (const SplPoint& p : *read[l]) {
            RoutePoint r;
            r.x = p.x;
            r.y = p.y;
            r.z = p.z;
            lines[l].push_back(r);
            if (p.y < lowest) lowest = p.y;
        }
    }
    pointToPoint = spl.pointToPoint;
    lastPoint = spl.lastPoint;
    minCornerSpeed = spl.minCornerSpeed;
    grid = spl.grid;
    measure();
    resetSectors();
    findCorners();
}

const std::vector<RoutePoint>& Route::line(int32_t i) const {
    return lines[static_cast<size_t>(i)].empty() ? lines[0] : lines[static_cast<size_t>(i)];
}

void Route::measure() {
    for (size_t l = 0; l < 3; ++l) {
        std::vector<RoutePoint>& points = lines[l];
        if (points.empty()) continue;
        const RoutePoint* previous = &points.back();
        bool first = true;
        float firstLength = 0.0f;
        for (RoutePoint& p : points) {
            p.direction = {p.x - previous->x, p.y - previous->y, p.z - previous->z, 0.0f};
            const Vec4& d = p.direction;
            p.length = std::sqrt((d[0] * d[0] + d[1] * d[1]) + d[2] * d[2]);
            const float unit = 1.0f / (std::sqrt((d[0] * d[0] + d[1] * d[1]) + d[2] * d[2]) + 1.1920929e-7f);
            p.direction[0] = unit * p.direction[0];
            p.direction[1] = unit * p.direction[1];
            p.direction[2] = unit * p.direction[2];
            if (firstLength == 0.0f) firstLength = p.length;
            if (first) {
                p.distance = 0.0f;
                first = false;
            } else {
                p.distance = previous->distance + p.length;
            }
            previous = &p;
        }
        lengths[l] = firstLength + points.back().distance;
    }
}

void Route::resetSectors() {
    // An unsigned fifth of the fast line's points.
    const auto fifth = static_cast<int32_t>(static_cast<uint32_t>(lines[0].size()) / 5u);
    for (int32_t s = 0; s < 5; ++s) {
        sectors[static_cast<size_t>(s)] = {fifth * s, 0.0f, -1};
    }
}

void Route::findCorners() {
    // A turn is a run of points turning the same way, each by more than 0.1 rad from the one before, less than
    // 150 m long; it is kept when it turns by more than 0.4 rad in all.
    auto kind = [](bool right, float angle) {  // 0x004A07A0
        const int32_t base = right ? 3 : 0;
        if (angle > 2.25f) return base + 3;
        if (angle > 1.1f) return base + 2;
        if (angle > 0.4f) return base + 1;
        return 0;
    };
    const std::vector<RoutePoint>& fast = lines[0];
    bool inTurn = false;
    bool right = false;
    int32_t start = 0;
    float angleSum = 0.0f;
    float lengthSum = 0.0f;
    auto close = [&](int32_t end) {  // 0x004A0A20
        if (angleSum > 0.4f) {
            RouteCorner c;
            c.first = start;
            c.middle = (end - start) / 2 + start;
            c.kind = kind(right, angleSum);
            c.distance = fast[static_cast<size_t>(start)].distance;
            corners.push_back(c);
        }
        inTurn = false;
        lengthSum = 0.0f;
        angleSum = 0.0f;
    };
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    for (size_t i = 0; i < fast.size(); ++i) {
        const Vec4& d = fast[i].direction;
        if (i == 0) {
            px = d[0];
            py = d[1];
            pz = d[2];
        }
        float dot = (py * d[1] + d[2] * pz) + d[0] * px;
        if (dot < -1.0f) {
            dot = -1.0f;
        } else if (dot > 1.0f) {
            dot = 1.0f;
        }
        const float angle = x87::acos(dot);
        const bool side = pz * d[0] - d[2] * px < 0.0f;
        const auto index = static_cast<int32_t>(i);
        if (angle > 0.1f && lengthSum < 15000.0f && !(side != right && inTurn)) {
            if (!inTurn) {
                inTurn = true;
                right = side;
                start = index;
            }
            angleSum = angle + angleSum;
            lengthSum = lengthSum + fast[i].length;
        } else if (inTurn) {
            close(index);
        } else {
            lengthSum = 0.0f;
            angleSum = 0.0f;
        }
        px = d[0];
        py = d[1];
        pz = d[2];
    }
    if (inTurn) close(static_cast<int32_t>(fast.size()));
}

void Route::curve(int32_t lineIndex, int32_t i, float t, Vec4& point, Vec4& direction) const {
    const std::vector<RoutePoint>& l = line(lineIndex);
    const int32_t i0 = before(i);
    const int32_t i2 = after(i);
    const int32_t i3 = after(i2);
    const RoutePoint& p0 = l[static_cast<size_t>(i0)];
    const RoutePoint& p1 = l[static_cast<size_t>(i)];
    const RoutePoint& p2 = l[static_cast<size_t>(i2)];
    const RoutePoint& p3 = l[static_cast<size_t>(i3)];
    point[0] = catmullRom(p0.x, p1.x, p2.x, p3.x, t);
    point[2] = catmullRom(p0.z, p1.z, p2.z, p3.z, t);
    point[1] = (p2.y - p1.y) * t + p1.y;
    direction[0] = catmullRomSlope(p0.x, p1.x, p2.x, p3.x, t);
    direction[2] = catmullRomSlope(p0.z, p1.z, p2.z, p3.z, t);
    direction[1] = 0.0f;
    normalize3(direction);
}

}  // namespace oj
