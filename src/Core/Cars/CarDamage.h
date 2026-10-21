#pragma once
// A car's damage (docs/spec/19-damage.md): the 16 x 16 grid around the body (the car's +0xB4, vtable 0x006FA1D8)
// whose cells take the dents, and the zones (the body parts) that add up the damage of the cells their vertices lie
// in. A dent moves the cell's vertices in their mesh parts' bytes; the model sends the parts that changed to the
// device.

#include "Core/Base/Matrix.h"
#include "Core/Base/Random.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace oj {

// A mesh part's vertices as a dent moves them (the stock locks the part's vertex buffer, +0x6C of the part's 0x74
// bytes): every vertex its position (3 floats at 0), its normal (3 floats at 12) and its colour (ARGB at 24; blue how
// much it dents, alpha how much the scratches show) in `stride` bytes; `changed` once a dent wrote it.
struct DamageVertices {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> original;  // as the mesh was read (the meshes made again, 0x00489440)
    uint32_t stride = 0;
    bool changed = false;
};

// A cell's entry of a vertex (0x00555E30, 12 bytes: the mesh, the part, the vertex).
struct DamageVertex {
    DamageVertices* part = nullptr;
    uint32_t index = 0;
};

// A vertex of a node added to the grid: its place in the car (the position plus the node's offset) and its entry.
struct DamagePoint {
    Vec4 place{};
    DamageVertex vertex;
};

// A zone of the grid (0x00557260, 0x18 bytes): a node of the car's model whose vertices were added.
struct DamageZone {
    float damage = 0.0f;  // +0x0C: the damage of its cells, each weighed by the zone's vertices in it
    float impact = 0.0f;  // +0x10: the dents' amounts, likewise
    uint32_t count = 0;   // +0x14: its vertices' entries in the cells
};

// A cell of the grid (0x54 bytes, vtable 0x006FA1CC).
struct DamageCell {
    std::vector<DamageZone*> zones;       // +0x04: one entry per vertex of a zone in the cell
    std::vector<DamageCell*> neighbours;  // +0x14: the eight around it (fewer at the poles)
    float saved = 0.0f;                   // +0x24: the damage kept with the car
    float damage = 0.0f;                  // +0x28: the damage of this race
    float kept = 0.0f;                    // +0x2C
    uint32_t vertices = 0;                // +0x30
    std::vector<DamageVertex> body;       // +0x34: the vertices of the nodes added with the flag 1
    std::vector<DamageVertex> decals;     // +0x44: those added with the flag 0 (DECAL, SPONS)
};

// The cells' damage of a car's grid object (the car's +0xB4) as it stays with the car between races: of each cell, when
// made, the damage kept (+0x24) and the race's (+0x28).
struct GridDamage {
    std::array<bool, 256> made{};
    std::array<float, 256> saved{};
    std::array<float, 256> damage{};
};

class DamageGrid {
public:
    // 0x00557B00 on a car's grid: a byte b keeps b / 255 x 20 (a cell made for each byte over 0).
    static GridDamage readBytes(const std::array<uint8_t, 256>& bytes);
    // 0x00557B80 on a car's grid: the race's damage added to the kept damage, a byte each (x 12.75, rounded to
    // nearest).
    static std::array<uint8_t, 256> writeBytes(GridDamage& cells);

    // The race on the car's grid object: its cells as the car keeps them, and given back to the car after the race.
    void take(const GridDamage& cells);
    void give(GridDamage& cells) const;

    // 0x00557180: the cell of a point of the car (centimetres from its centre), as the main thread computes it:
    // i = (atan2(x, z) + pi) x 8 / pi and j = (atan2(x, y) + pi) x 16 / pi cut toward zero; from i = 8 on, j less 16;
    // both within 0 .. 15.
    static void cellOf(const Vec4& p, int& i, int& j);

    // +0x18 (0x00557AA0) on the cells a car keeps: the damage added to the point's cell (made when there is none), at
    // most 20.
    static void keep(GridDamage& cells, const Vec4& point, float amount);

    // +0x04 (0x005554A0): the cell, made when there is none.
    DamageCell& cell(int i, int j);
    DamageCell* find(int i, int j) { return m_cells[static_cast<size_t>(i * 16 + j)].get(); }

    // +0x0C (0x00556130, before a car's model is given to the grid again, 0x0048A700): each cell's vertex lists and
    // zone entries dropped, the race's damage added to the kept, a cell with neither kept nor +0x2C freed; then the
    // zones freed (0x00557330).
    void clear();

    // 0x00557C00 / 0x00557C90: every cell's damage of the race (+0x28) cleared, with `all` also the damage kept with
    // the car (+0x24). The zones keep theirs.
    void repair(bool all) {
        for (const auto& c : m_cells) {
            if (!c) continue;
            c->damage = 0.0f;
            if (all) c->saved = 0.0f;
        }
    }

    // 0x00557D10: each cell made takes the kept and the race's damage of the same cell of another car's cells, 0
    // where that car has none.
    void restore(const GridDamage& from) {
        for (size_t k = 0; k < m_cells.size(); ++k) {
            DamageCell* c = m_cells[k].get();
            if (c == nullptr) continue;
            c->saved = from.made[k] ? from.saved[k] : 0.0f;
            c->damage = from.made[k] ? from.damage[k] : 0.0f;
        }
    }

    // 0x00556FB0: frozen (+0x418), each cell keeping its damage as it is (+0x2C = +0x24 + +0x28): what the car takes
    // back after the race (0x00557070 in give: the kept damage the cells' damage, the race's 0) is this.
    void freeze() {
        m_frozen = true;
        for (const auto& c : m_cells) {
            if (c) c->kept = c->saved + c->damage;
        }
    }
    bool frozen() const { return m_frozen; }

    // +0x08 (0x00555E30): a node's vertices (each its position plus the node's offset): a new zone, and each vertex
    // counted in its cell with an entry of the zone; a vertex with a part kept in the cell's list of the flag (1 the
    // body's, 0 the decals').
    DamageZone* addNode(const std::vector<DamagePoint>& points, bool body);
    DamageZone* addNode(const std::vector<Vec4>& points);

    // +0x10 (0x005574B0): every cell knows its neighbours, every zone its entries; a cell with kept damage is dented
    // again in a drawn direction (three rand() made unit, half the damage, half to the neighbours); the race's damage
    // is cleared and the zones' impacts fall to a fifth.
    void finish(CrtRandom& random);

    // +0x14 (0x00557A30): a hit at a point of the car: the point's cell, when there is one, dented along the
    // direction (the contact's normal in the car's frame) by the amount with 0.4 of it to its neighbours.
    void hit(const Vec4& point, const Vec4& direction, float amount);

    // +0x18 (0x00557AA0): damage kept, added to the point's cell (made when there is none), at most 20.
    void keep(const Vec4& point, float amount);

    // +0x1C (0x00557B00): the 256 bytes of a saved car (i outer, j inner): a byte b keeps b / 255 x 20.
    void read(const std::array<uint8_t, 256>& bytes);

    // +0x20 (0x00557B80): the race's damage added to the kept damage, a byte each (x 12.75, rounded to nearest).
    std::array<uint8_t, 256> write();

    // 0x00557C00: repaired, every cell's damage 0. 0x00557C90: the race's damage 0.
    void repair();
    void clearRace();

    const std::vector<std::unique_ptr<DamageZone>>& zones() const { return m_zones; }
    size_t cellCount() const {
        size_t n = 0;
        for (const auto& c : m_cells) n += c ? 1 : 0;
        return n;
    }

private:
    // 0x005556C0: the neighbours first (0x00556DE0), then the zones' impacts, the cell's damage (within 20 with the
    // kept damage), the zones' damage and the cell's vertices moved along the direction.
    void dent(DamageCell& c, const Vec4& direction, float amount, float falloff);

    std::array<std::unique_ptr<DamageCell>, 256> m_cells;  // +0x18
    bool m_frozen = false;                                 // +0x418
    std::vector<std::unique_ptr<DamageZone>> m_zones;      // +0x08
};

}  // namespace oj
