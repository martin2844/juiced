// Unit tests: a spline's path (Core/Tracks/SceneObjects.h, 0x0041B720).

#include "Framework/Test.h"

#include "Core/Tracks/SceneObjects.h"

#include <cmath>

namespace {

using oj::test::check;

// A straight segment from a to b (its handles a third and two thirds along).
oj::SplineSegment straight(const oj::Vec4& a, const oj::Vec4& b) {
    oj::SplineSegment s;
    s.p0 = a;
    s.p3 = b;
    for (size_t k = 0; k < 3; ++k) {
        s.p1[k] = a[k] + (b[k] - a[k]) / 3.0f;
        s.p2[k] = a[k] + (b[k] - a[k]) * 2.0f / 3.0f;
    }
    return s;
}

}  // namespace

// An open spline of two segments along x (0 .. 100 .. 300) and a closed one round a square of 100: the points at the
// knots moved by the node's place, each length to the next (an open spline's last back to the first). The stock's
// length summed the way back too (an open spline 600, its EVEN_SPACING spread over it); the fixed build's open spline
// ends at its last point (300).
OJ_UNIT_TEST(SplinePath, OpenSplineLength) {
    oj::SceneSpline open;
    open.segments = {straight({0, 0, 0, 1}, {100, 0, 0, 1}), straight({100, 0, 0, 1}, {300, 0, 0, 1})};
    oj::makeSplinePath(open, 1000.0f, 0.0f, 0.0f);
    bool ok = open.path.size() == 3 && open.path[0].position[0] == 1000.0f && open.path[2].position[0] == 1300.0f &&
              std::fabs(open.path[0].length - 100.0f) < 1e-3f && std::fabs(open.path[1].length - 200.0f) < 1e-3f &&
              std::fabs(open.path[2].length - 300.0f) < 1e-3f && std::fabs(open.path[2].distance - 300.0f) < 1e-3f &&
              std::fabs(open.length - 300.0f) < 1e-3f;
    oj::SceneSpline square;
    square.closed = true;
    square.segments = {straight({0, 0, 0, 1}, {100, 0, 0, 1}), straight({100, 0, 0, 1}, {100, 0, 100, 1}),
                       straight({100, 0, 100, 1}, {0, 0, 100, 1}), straight({0, 0, 100, 1}, {0, 0, 0, 1})};
    oj::makeSplinePath(square, 0.0f, 0.0f, 0.0f);
    ok = ok && square.path.size() == 4 && std::fabs(square.path[3].length - 100.0f) < 1e-3f &&
         std::fabs(square.length - 400.0f) < 1e-3f;
    check(ok, "spline path (fixed): an open spline's length ends at its last point, a closed one's goes round");
}
