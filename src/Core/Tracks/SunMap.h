#pragma once
// A ground map of a track (tracks\<Track>\<track>_<set>.sqh, the sun on the cars and the objects that move; .pqh, the
// puddles the wheels spray from in the wet; docs/spec/15-tracks.md, "The ground maps"): 2-bit codes over the track's
// box in tiles of 64 x 64 pixels, each tile a quadtree whose quarters are one code or another node, down to blocks of
// 8 x 8 pixels (the object 0x005053B0, 0x64 bytes).

#include "Core/Base/Matrix.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace oj {

struct SunMap {
    int32_t width = 0;                // +0x1C: pixels
    int32_t height = 0;               // +0x20
    uint32_t bias = 0;                // +0x24: added to every value
    uint32_t base = 0xFF;             // +0x28: read and not used (a reference of 0xFF or more is a node, 0x005053B0)
    float step = 1.0f / 85.0f;        // +0x2C: a value is the code over it
    Vec4 min{};                       // +0x30: the box (x, y, z)
    Vec4 max{};                       // +0x40
    int32_t tilesAcross = 0;          // +0x50: width / 64
    int32_t tilesDown = 0;            // +0x54: height / 64
    float cellX = 0.0f;               // +0x58: (max x - min x) / width
    float cellZ = 0.0f;               // +0x5C: (max z - min z) / height
    std::vector<uint32_t> tiles;      // +0x60: each tile's root node, row by row
    std::vector<std::array<uint16_t, 4>> nodes;  // +0x0C: the quarters (low x low y, high x, high y, both high)
    std::vector<std::array<uint16_t, 8>> blocks; // +0x14: 8 rows, 2 bits a pixel

    // 0x005055D0: the value of a pixel (row 0 at the far z edge): its code over the step, cut toward zero, plus the
    // bias, as a byte.
    uint8_t value(int32_t row, int32_t column) const;
    // 0x00505690: the value at a point of the world (x, z) over 255: the pixel nearest to it, within the map.
    float sample(const Vec4& point) const;
};

// 0x005053B0: none when the data is short or does not end with its blocks.
std::optional<SunMap> parseSunMap(const std::vector<uint8_t>& data);

}  // namespace oj
