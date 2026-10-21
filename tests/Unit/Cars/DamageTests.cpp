// Unit tests: Damage.

#include "Framework/Test.h"

#include "Core/Base/Matrix.h"
#include "Core/Base/Random.h"
#include "Core/Cars/CarDamage.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The damage grid: ten vertices of a zone in one cell; a hit of 30 there is held to 20 and counted in the
// zone; written, the cell is 255; read into another grid, the build dents it again by half (three rand()).
OJ_UNIT_TEST(Damage, GridDentHeld) {
    oj::DamageGrid grid;
    const std::vector<oj::Vec4> points(10, oj::Vec4{100.0f, 0.0f, 0.0f, 0.0f});
    oj::DamageZone* zone = grid.addNode(points);
    oj::CrtRandom draws;
    grid.finish(draws);
    grid.hit(points[0], {0.0f, 0.0f, 1.0f, 0.0f}, 30.0f);
    const bool held = zone->count == 10 && zone->damage == 20.0f && zone->impact == 30.0f;
    const auto written = grid.write();
    size_t full = 0;
    for (uint8_t b : written) full += b == 255 ? 1 : 0;
    oj::DamageGrid reread;
    reread.read(written);
    oj::DamageZone* kept = reread.addNode(points);
    reread.finish(draws);
    const bool redented = draws.draws() == 3 && kept->damage == 10.0f && reread.write() == written;
    check(held && full == 1 && redented, "damage grid: a dent held to 20, written and read back, dented again");
}
// A dent's vertices (0x005556C0): a body vertex with blue 255 takes the alpha of the damage (8 x 15) and moves
// along the direction by up to the amount, its normal stays unit; one with blue 0 only takes the alpha; a
// decal vertex moves by up to the amount and keeps its colour.
OJ_UNIT_TEST(Damage, DentVertices) {
    oj::DamageVertices part;
    part.stride = 36;
    part.bytes.assign(36 * 3, 0);
    const uint32_t colours[3] = {0x000000FFu, 0x00808000u, 0x00000000u};
    for (uint32_t v = 0; v < 3; ++v) {
        const float f[6] = {100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        std::memcpy(part.bytes.data() + v * 36, f, sizeof(f));
        std::memcpy(part.bytes.data() + v * 36 + 24, &colours[v], 4);
    }
    oj::DamageGrid grid;
    const oj::Vec4 at{100.0f, 0.0f, 0.0f, 0.0f};
    grid.addNode({{at, {&part, 0}}, {at, {&part, 1}}}, true);
    grid.addNode({{at, {&part, 2}}}, false);
    oj::CrtRandom draws;
    grid.finish(draws);
    grid.hit(at, {0.0f, 0.0f, 1.0f, 0.0f}, 8.0f);
    auto vertex = [&part](uint32_t v, float* f, uint32_t& colour) {
        std::memcpy(f, part.bytes.data() + v * 36, 24);
        std::memcpy(&colour, part.bytes.data() + v * 36 + 24, 4);
    };
    float a[6], b[6], c[6];
    uint32_t ca = 0, cb = 0, cc = 0;
    vertex(0, a, ca);
    vertex(1, b, cb);
    vertex(2, c, cc);
    const float n = std::sqrt(a[3] * a[3] + a[4] * a[4] + a[5] * a[5]);
    const bool body = ca == 0x780000FFu && a[0] == 100.0f && a[1] == 0.0f && a[2] >= 0.0f && a[2] <= 8.0f &&
                      std::fabs(n - 1.0f) < 1e-4f && cb == 0x78808000u && b[2] == 0.0f && b[5] == 1.0f;
    const bool decal = cc == 0 && c[2] >= 0.0f && c[2] <= 8.0f && c[2] != 0.0f;
    check(part.changed && body && decal, "damage grid: a dent's vertices moved and their alpha raised");
}
// A car made ready again (0x004894C0): the grid cleared (0x00556130: the race's damage kept, the empty cells
// freed, the zones dropped), the model given again and finished: the neighbours linked once more (not twice),
// the kept damage dented again with three draws.
OJ_UNIT_TEST(Damage, CarMadeReady) {
    oj::GridDamage kept;
    kept.made[8 * 16 + 8] = true;
    kept.saved[8 * 16 + 8] = 4.0f;
    oj::DamageGrid grid;
    grid.take(kept);
    std::vector<oj::Vec4> points;
    for (float x : {-30.0f, 0.0f, 30.0f}) {
        for (float y : {-30.0f, 0.0f, 30.0f}) points.push_back({x, y, 100.0f, 0.0f});
    }
    grid.addNode(points);
    oj::CrtRandom draws, expected;
    grid.finish(draws);
    for (int i = 0; i < 3; ++i) expected.next();
    const oj::DamageCell* c = grid.find(8, 8);
    const size_t first = c != nullptr ? c->neighbours.size() : 0;
    const size_t cells = grid.cellCount();
    grid.clear();
    const bool cleared = grid.zones().empty() && grid.find(8, 8) != nullptr && grid.find(8, 8)->zones.empty();
    grid.addNode(points);
    grid.finish(draws);
    for (int i = 0; i < 3; ++i) expected.next();
    c = grid.find(8, 8);
    const bool relinked = c != nullptr && c->neighbours.size() == first && c->saved == 4.0f &&
                       grid.cellCount() == cells && draws.next() == expected.next();
    check(first != 0 && cleared && relinked, "damage grid: cleared and given the model again, linked once");
}
