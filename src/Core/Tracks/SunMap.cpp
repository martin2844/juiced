#include "Core/Tracks/SunMap.h"

#include "Core/Base/ByteReader.h"

#include <cmath>

namespace oj {

std::optional<SunMap> parseSunMap(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    SunMap m;
    m.width = static_cast<int32_t>(r.u32());
    m.height = static_cast<int32_t>(r.u32());
    m.bias = r.u32();
    m.base = r.u32();
    m.step = r.f32();
    for (float& f : m.min) f = r.f32();
    for (float& f : m.max) f = r.f32();
    const uint32_t nodeCount = r.u32();
    const uint32_t blockCount = r.u32();
    if (!r.ok() || m.width <= 0 || m.height <= 0) return std::nullopt;
    m.tilesAcross = m.width / 64;
    m.tilesDown = m.height / 64;
    m.cellX = (m.max[0] - m.min[0]) / static_cast<float>(m.width);
    m.cellZ = (m.max[2] - m.min[2]) / static_cast<float>(m.height);
    const auto tileCount = static_cast<uint32_t>(m.tilesAcross * m.tilesDown);
    if (!r.fits(tileCount, 4)) return std::nullopt;
    m.tiles.resize(tileCount);
    for (uint32_t& t : m.tiles) t = r.u32();
    if (!r.fits(nodeCount, 8)) return std::nullopt;
    m.nodes.resize(nodeCount);
    for (auto& n : m.nodes) {
        for (uint16_t& c : n) c = r.u16();
    }
    if (!r.fits(blockCount, 16)) return std::nullopt;
    m.blocks.resize(blockCount);
    for (auto& b : m.blocks) {
        for (uint16_t& row : b) row = r.u16();
    }
    if (!r.ok() || r.left() != 0) return std::nullopt;
    return m;
}

uint8_t SunMap::value(int32_t row, int32_t column) const {
    const int32_t y0 = row % 64;
    const int32_t x0 = column % 64;
    const size_t tile = static_cast<size_t>((row / 64) * tilesAcross + column / 64);
    if (tile >= tiles.size()) return static_cast<uint8_t>(bias);
    // 0x00505240: down the tile's quarters, halving from 64; a quarter of 8 that is not one code is a block.
    int32_t x = x0, y = y0, size = 64;
    size_t at = tiles[tile] - 0xFFu;
    uint8_t code = 0;
    while (true) {
        if (at >= nodes.size()) {
            code = 0;
            break;
        }
        size /= 2;
        const auto& n = nodes[at];
        size_t quarter = 0;
        if (x >= size) quarter |= 1;
        if (y >= size) quarter |= 2;
        const uint16_t child = n[quarter];
        if (child < 0xFF) {
            code = static_cast<uint8_t>(child);
            break;
        }
        if (size == 8) {
            const size_t block = child - 0xFFu;
            const uint16_t bits = block < blocks.size() ? blocks[block][static_cast<size_t>(y & 7)] : 0;
            code = static_cast<uint8_t>((bits >> ((x & 7) * 2)) & 3);
            break;
        }
        if (x >= size) x -= size;
        if (y >= size) y -= size;
        at = child - 0xFFu;
    }
    const float v = static_cast<float>(static_cast<int8_t>(code)) / step;
    return static_cast<uint8_t>(static_cast<uint8_t>(bias) + static_cast<uint8_t>(static_cast<int32_t>(v)));
}

float SunMap::sample(const Vec4& point) const {
    auto chop = [](float f) { return static_cast<int32_t>(f); };
    int32_t column = chop((point[0] - min[0]) / cellX + 0.5f);
    int32_t row = chop((static_cast<float>(height) - (point[2] - min[2]) / cellZ) + 0.5f);
    if (column < 0) column = 0;
    if (row < 0) row = 0;
    if (column >= width) column = width - 1;
    if (row >= height) row = height - 1;
    return static_cast<float>(value(row, column)) * 0.003921568859368563f;
}

}  // namespace oj
