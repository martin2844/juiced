#pragma once
// The graph object (0x78 bytes; 0x004F8610 .. 0x004F8C30; docs/spec/14-career-windows.md "car_rolling"): series of
// values in a ring of points, drawn as lines over the screen. car_rolling draws its torque and power with it; CMSStats
// makes one it never draws.

#include "Render/RenderDevice.h"

#include <cstdint>
#include <string>
#include <vector>

namespace oj {

class Graph {
public:
    // A series (0x4C bytes): its name, its values (one a point), shown (+0x44, 1 when added) and its scale (+0x48:
    // the value's height in units of 480; its number as an integer when added).
    struct Series {
        std::wstring name;          // +0x00: 32 characters widened
        std::vector<float> values;  // +0x40
        bool shown = true;          // +0x44
        float scale = 0.0f;         // +0x48
    };

    // 0x004F8610: the name (32 characters widened from its bytes), the points (+0x40) and those drawn (+0x44), no
    // series, the next point (+0x4C) 0, not running (+0x48), +0x70 0 and +0x74 256.
    Graph(const std::string& name, uint32_t points);

    // 0x004F8780: a series of that name (its first 32 characters) with every value 0, shown, its scale the bits of
    // its number; its number on the order list (+0x60).
    void addSeries(const std::string& name);
    // 0x004F83D0: a series' scale (+0x48), nothing past the series.
    void setScale(uint32_t series, float scale);
    // 0x004F8900: while running, the value of a series at the next point.
    void set(uint32_t series, float value);
    // 0x004F8C30: while running, the next point the one after (round to 0 at the points' count) and every series' value
    // there 0.
    void advance();
    // 0x004F8940 without the legend (its font none): a line of 0x80FFFFFF from (+0x70, +0x74) to (2 x the points,
    // +0x74) in pixels (Stock problem: neither scaled to the screen nor moved by +0x70; the fixed build's from (x, y)
    // to (2 x the points + x, y) of 640 x 480, as the series); then each series of the order
    // list (0x004F8410), in its colour of [0x007191E0] (its number modulo 10) at 0xFF when shown, else 0x30.
    void draw(render::IRenderDevice& device, bool shadow) const;

    std::wstring name;              // +0x00
    uint32_t points = 0;            // +0x40
    uint32_t drawn = 0;             // +0x44: the points drawn (car_rolling raises it to the count)
    bool running = false;           // +0x48
    uint32_t next = 0;              // +0x4C: the point written next, the first drawn
    std::vector<Series> series;     // +0x50
    std::vector<uint32_t> order;    // +0x60: the series in the order drawn
    float x = 0.0f;                 // +0x70: the left (of 640)
    float y = 256.0f;               // +0x74: the bottom (of 480)

private:
    // 0x004F8410: from the next point on, a line between each two of the first `drawn` places, each place i at
    // (2 i + x) and (y - the value x the scale) of 640 x 480 of the screen; with `shadow` a value not 0 first gets the
    // line 3 to the right and 2 down (of 640 x 480) in black.
    void drawSeries(render::IRenderDevice& device, const Series& s, uint32_t argb, bool shadow) const;
};

}  // namespace oj
