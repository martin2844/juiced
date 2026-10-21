#pragma once
// A race's route (0x004A0C20, 0x100 bytes; docs/spec/18-racers.md, "The route"): the three racing lines of the
// route's .spl with each point's direction, length and distance along its line, the lap's five sectors and the
// corners of the fast line; the curve through a line's points.

#include "Core/Base/Matrix.h"
#include "Core/Tracks/Track.h"

#include <array>
#include <cfloat>
#include <cstdint>
#include <vector>

namespace oj {

// A point of a line (0x30 bytes; read by POINT 0x004A1630, the rest by 0x004A0620). Centimetres of the world.
struct RoutePoint {
    float x = 0.0f, y = 0.0f, z = 0.0f;  // +0x00
    Vec4 direction{};                    // +0x10: from the previous point (the last for point 0), of unit length
    float length = 0.0f;                 // +0x20: from the previous point
    float distance = 0.0f;               // +0x28: along the line from point 0
};

// A fifth of the lap (0x0C bytes from the route's +0xB4; 0x004A0AF0): its first point of the fast line, the race
// time it was last entered at, the racer that entered it.
struct RouteSector {
    int32_t first = 0;
    float time = 0.0f;
    int32_t racer = -1;
};

// A corner of the fast line (0x10 bytes, the route's +0xF0; 0x004A0840 / 0x004A0A20).
struct RouteCorner {
    int32_t first = 0;      // the first point in the turn
    int32_t middle = 0;     // halfway to the point after the turn
    int32_t kind = 0;       // 0x004A07A0: 1 .. 3 one way, 4 .. 6 the other, by how far it turns
    float distance = 0.0f;  // of the first point along the line
};

class Route {
public:
    enum { kFast = 0, kAttack = 1, kCentre = 2 };

    // 0x004A0C20 after the .spl is read: 0x004A0620 (the points' directions, lengths, distances, the lines'
    // lengths), 0x004A0AF0 (the sectors), 0x004A0840 (the corners).
    explicit Route(const Spl& spl);

    // A line by its number; an empty line is the fast line.
    const std::vector<RoutePoint>& line(int32_t i) const;

    // The points of a lap: every index into the lines wraps by the fast line's count.
    int32_t count() const { return static_cast<int32_t>(lines[0].size()); }
    int32_t before(int32_t i) const { return i - 1 < 0 ? count() - 1 : i - 1; }
    int32_t after(int32_t i) const { return i + 1 > count() - 1 ? 0 : i + 1; }

    // 0x004A1370: the point at t from point i toward point i + 1 of a line (Catmull-Rom in x and z through the
    // points i - 1 .. i + 2, straight in y; 0x004A0FD0) and the curve's direction there, flat and of unit length
    // (0x004A11D0). A racer passes the point before the one it heads for.
    void curve(int32_t line, int32_t i, float t, Vec4& point, Vec4& direction) const;

    // 0x004A0AF0: the sectors' first points at fifths of the fast line, no times, no racers.
    void resetSectors();

    std::array<std::vector<RoutePoint>, 3> lines;  // +0x00: FAST, ATTACK, CENTRE
    std::array<float, 3> lengths{};                // +0x30: of each line; the fast line's is the lap's
    float unused = 100.0f;                         // +0x3C: 100 when made (0x004A0F68), read by nothing found
    bool pointToPoint = false;                     // +0x40
    int32_t lastPoint = 0;                         // +0x44: the finish of a point-to-point route
    float minCornerSpeed = 13.41f;                 // +0x48 (m/s)
    std::vector<Spl::GridPoint> grid;              // +0x4C
    float lowest = FLT_MAX;                        // +0xB0: the lowest point's height
    std::array<RouteSector, 5> sectors{};          // +0xB4
    std::vector<RouteCorner> corners;              // +0xF0

private:
    void measure();      // 0x004A0620
    void findCorners();  // 0x004A0840
};

}  // namespace oj
