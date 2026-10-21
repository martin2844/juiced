#include "Core/Cars/CarDamage.h"

#include <bit>
#include <climits>
#include <cmath>
#include <cstring>

namespace oj {

namespace {

const float kPi = std::bit_cast<float>(0x40490FDBu);         // 0x006FF9A8
const float kRows = std::bit_cast<float>(0x4022F983u);       // 8 / pi (0x006FFBF8)
const float kColumns = std::bit_cast<float>(0x40A2F983u);    // 16 / pi (0x006FFBF4)
const float kByte = std::bit_cast<float>(0x3B808081u);       // 1 / 255 (0x006FF918)
constexpr float kMost = 20.0f;                               // 0x006FF89C
constexpr float kSaveScale = 12.75f;                         // 0x006FFAE8
constexpr float kAlphaScale = 15.0f;                         // 0x006EDB50
const float kSeedScale = std::bit_cast<float>(0x3DCCCCCDu);  // 0.1 (0x006FF940)
const float kBend = std::bit_cast<float>(0x3D4CCCCDu);       // 0.05 (0x006FF994)
const float kEpsilon = std::bit_cast<float>(0x34000000u);    // 0x006FF99C
const float kRandomScale = std::bit_cast<float>(0x38000100u);  // 1 / 32767 (0x006FF7EC)

// A float made a whole number cut toward zero (x87 chop or SSE with the game's MXCSR); out of range the indefinite
// value.
int chop(float v) {
    if (!(v > -2147483648.0f && v < 2147483648.0f)) return INT_MIN;
    return static_cast<int>(v);
}

// 0x005556C0 on a vertex: the game's lagged generator seeded by its place ((x + y + z) x 0.1 rounded to nearest); its
// normal's x and y each moved by (r - 0.5) x 0.05 and the normal made unit by 1 / (length + 1.1920929e-7); its
// position moved by r x the direction x the weight in each axis (r of 0 .. 1, the generator's next).
void moveVertex(DamageVertex v, const Vec4& direction, float weight) {
    float f[6];
    uint8_t* at = v.part->bytes.data() + static_cast<size_t>(v.index) * v.part->stride;
    std::memcpy(f, at, sizeof(f));
    LaggedRandom& random = laggedRandom();
    const double sum = (static_cast<double>(f[0]) + f[1]) + f[2];
    random.seed(static_cast<uint32_t>(static_cast<int32_t>(std::lrint(sum * kSeedScale))));
    f[3] = (random.next() - 0.5f) * kBend + f[3];
    f[4] = (random.next() - 0.5f) * kBend + f[4];
    const double length = std::sqrt((static_cast<double>(f[3]) * f[3] + static_cast<double>(f[4]) * f[4]) +
                                    static_cast<double>(f[5]) * f[5]);
    const double unit = 1.0 / (static_cast<float>(length) + static_cast<double>(kEpsilon));
    f[3] = static_cast<float>(unit * f[3]);
    f[4] = static_cast<float>(unit * f[4]);
    f[5] = static_cast<float>(unit * f[5]);
    for (int k = 0; k < 3; ++k) {
        const double r = random.next();
        f[k] = static_cast<float>(r * direction[static_cast<size_t>(k)] * weight + f[k]);
    }
    std::memcpy(at, f, sizeof(f));
    v.part->changed = true;
}

}  // namespace

void DamageGrid::cellOf(const Vec4& p, int& i, int& j) {
    // fpatan leaves its result unrounded; the sums and products round to floats.
    const float across = static_cast<float>(std::atan2(static_cast<double>(p[0]), static_cast<double>(p[1])));
    const float round = static_cast<float>(std::atan2(static_cast<double>(p[0]), static_cast<double>(p[2])) +
                                           static_cast<double>(kPi));
    i = chop(round * kRows);
    j = chop((across + kPi) * kColumns);
    if (i < 0) {
        i = 0;
    } else if (i > 15) {
        i = 15;
        j = j - 16;
    } else if (i >= 8) {
        j = j - 16;
    }
    if (j < 0) {
        j = 0;
    } else if (j > 15) {
        j = 15;
    }
}

DamageCell& DamageGrid::cell(int i, int j) {
    std::unique_ptr<DamageCell>& c = m_cells[static_cast<size_t>(i * 16 + j)];
    if (!c) c = std::make_unique<DamageCell>();
    return *c;
}

DamageZone* DamageGrid::addNode(const std::vector<DamagePoint>& points, bool body) {
    m_zones.push_back(std::make_unique<DamageZone>());
    DamageZone* zone = m_zones.back().get();
    for (const DamagePoint& p : points) {
        int i = 0, j = 0;
        cellOf(p.place, i, j);
        DamageCell& c = cell(i, j);
        c.vertices = c.vertices + 1;
        if (p.vertex.part != nullptr) (body ? c.body : c.decals).push_back(p.vertex);
        c.zones.push_back(zone);
    }
    return zone;
}

DamageZone* DamageGrid::addNode(const std::vector<Vec4>& points) {
    std::vector<DamagePoint> placed(points.size());
    for (size_t k = 0; k < points.size(); ++k) placed[k].place = points[k];
    return addNode(placed, true);
}

void DamageGrid::finish(CrtRandom& random) {
    for (int i = 0; i < 16; ++i) {
        const int before = i - 1 < 0 ? i + 15 : i - 1;
        const int after = i + 1 > 15 ? i - 15 : i + 1;
        for (int j = 0; j < 16; ++j) {
            DamageCell* c = find(i, j);
            if (c == nullptr) continue;
            c->neighbours.clear();
            auto add = [c](DamageCell* n) {
                if (n != nullptr) c->neighbours.push_back(n);
            };
            add(find(before, j));
            add(find(after, j));
            if (j != 0) {
                add(find(i, j - 1));
                add(find(before, j - 1));
                add(find(after, j - 1));
            }
            if (j < 15) {
                add(find(i, j + 1));
                add(find(before, j + 1));
                add(find(after, j + 1));
            }
        }
    }
    for (std::unique_ptr<DamageCell>& c : m_cells) {
        if (!c) continue;
        for (DamageZone* z : c->zones) z->count = z->count + 1;
    }
    for (std::unique_ptr<DamageCell>& c : m_cells) {
        if (!c || c->saved == 0.0f) continue;
        // The kept damage dented again: a direction of three draws (each r / 32767 - 0.5: the second its x, the first
        // its y, the third its z; made unit by 1 / (length + 1.1920929e-7); it moves only the vertices), half the
        // damage, half of that to the neighbours.
        const float kept = c->saved;
        c->saved = 0.0f;
        const float a = static_cast<float>(random.next()) * kRandomScale - 0.5f;
        const float b = static_cast<float>(random.next()) * kRandomScale - 0.5f;
        const float z = static_cast<float>(random.next()) * kRandomScale - 0.5f;
        const float length = static_cast<float>(std::sqrt(
            (static_cast<double>(a) * a + static_cast<double>(b) * b) + static_cast<double>(z) * z));
        const double unit = 1.0 / (static_cast<double>(length) + kEpsilon);
        const Vec4 direction{static_cast<float>(b * unit), static_cast<float>(a * unit), static_cast<float>(z * unit),
                             0.0f};
        dent(*c, direction, kept * 0.5f, 0.5f);
        c->saved = kept;
    }
    for (std::unique_ptr<DamageCell>& c : m_cells) {
        if (c) c->damage = 0.0f;
    }
    for (std::unique_ptr<DamageZone>& z : m_zones) z->impact = z->impact * 0.2f;
}

void DamageGrid::dent(DamageCell& c, const Vec4& direction, float amount, float falloff) {
    if (falloff > 0.0f) {
        const float share = amount * falloff;
        for (DamageCell* n : c.neighbours) dent(*n, direction, share, 0.0f);
    }
    for (DamageZone* z : c.zones) z->impact = amount / static_cast<float>(z->count) + z->impact;
    const float sum = c.damage + c.saved;
    if (!(sum < kMost)) return;
    if (sum + amount > kMost) amount = kMost - sum;
    c.damage = amount + c.damage;
    uint32_t alpha = static_cast<uint32_t>(std::lrint((c.damage + c.saved) * kAlphaScale));
    if (alpha > 0xFF) alpha = 0xFF;
    for (DamageZone* z : c.zones) z->damage = c.damage / static_cast<float>(z->count) + z->damage;
    // The body's vertices: the colour's alpha raised to the damage's (or'ed into the colour); those with a blue
    // (how much they dent) moved by blue / 255 x the amount.
    for (const DamageVertex& v : c.body) {
        uint8_t* at = v.part->bytes.data() + static_cast<size_t>(v.index) * v.part->stride;
        uint32_t colour = 0;
        std::memcpy(&colour, at + 24, sizeof(colour));
        if ((colour >> 24) < alpha) {
            const uint32_t raised = alpha << 24 | colour;
            std::memcpy(at + 24, &raised, sizeof(raised));
            v.part->changed = true;
        }
        if ((colour & 0xFF) != 0) moveVertex(v, direction, static_cast<float>(colour & 0xFF) * kByte * amount);
    }
    // The decals' vertices: all moved by the amount, their colour as it is.
    for (const DamageVertex& v : c.decals) moveVertex(v, direction, amount);
}

void DamageGrid::hit(const Vec4& point, const Vec4& direction, float amount) {
    int i = 0, j = 0;
    cellOf(point, i, j);
    if (DamageCell* c = find(i, j)) dent(*c, direction, amount, 0.4f);
}

void DamageGrid::keep(const Vec4& point, float amount) {
    int i = 0, j = 0;
    cellOf(point, i, j);
    DamageCell& c = cell(i, j);
    c.saved = amount + c.saved;
    if (kMost < c.saved) c.saved = kMost;
}

void DamageGrid::keep(GridDamage& cells, const Vec4& point, float amount) {
    int i = 0, j = 0;
    cellOf(point, i, j);
    const auto k = static_cast<size_t>(i * 16 + j);
    if (!cells.made[k]) {
        cells.made[k] = true;
        cells.saved[k] = 0.0f;
        cells.damage[k] = 0.0f;
    }
    cells.saved[k] = amount + cells.saved[k];
    if (kMost < cells.saved[k]) cells.saved[k] = kMost;
}

GridDamage DamageGrid::readBytes(const std::array<uint8_t, 256>& bytes) {
    GridDamage out;
    for (size_t k = 0; k < 256; ++k) {
        const float v = static_cast<float>(bytes[k]) * kByte * kMost;
        if (v > 0.0f) {
            out.made[k] = true;
            out.saved[k] = v;
        }
    }
    return out;
}

std::array<uint8_t, 256> DamageGrid::writeBytes(GridDamage& cells) {
    std::array<uint8_t, 256> out{};
    for (size_t k = 0; k < 256; ++k) {
        float v = 0.0f;
        if (cells.made[k]) {
            cells.saved[k] = cells.damage[k] + cells.saved[k];
            cells.damage[k] = 0.0f;
            v = cells.saved[k];
        }
        out[k] = static_cast<uint8_t>(std::lrint(v * kSaveScale));
    }
    return out;
}

void DamageGrid::clear() {
    for (std::unique_ptr<DamageCell>& c : m_cells) {
        if (!c) continue;
        c->body.clear();
        c->decals.clear();
        c->zones.clear();
        c->saved = c->saved + c->damage;
        c->damage = 0.0f;
        if (c->saved == 0.0f && c->kept == 0.0f) c.reset();
    }
    m_zones.clear();
}

void DamageGrid::take(const GridDamage& cells) {
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            const auto k = static_cast<size_t>(i * 16 + j);
            if (!cells.made[k]) continue;
            DamageCell& c = cell(i, j);
            c.saved = cells.saved[k];
            c.damage = cells.damage[k];
        }
    }
}

void DamageGrid::give(GridDamage& cells) const {
    for (size_t k = 0; k < 256; ++k) {
        const DamageCell* c = m_cells[k].get();
        cells.made[k] = c != nullptr;
        cells.saved[k] = c != nullptr ? (m_frozen ? c->kept : c->saved) : 0.0f;
        cells.damage[k] = c != nullptr && !m_frozen ? c->damage : 0.0f;
    }
}

void DamageGrid::read(const std::array<uint8_t, 256>& bytes) {
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            const float v = static_cast<float>(bytes[static_cast<size_t>(i * 16 + j)]) * kByte * kMost;
            if (v > 0.0f) cell(i, j).saved = v;
        }
    }
}

std::array<uint8_t, 256> DamageGrid::write() {
    std::array<uint8_t, 256> out{};
    for (size_t k = 0; k < 256; ++k) {
        float v = 0.0f;
        if (DamageCell* c = m_cells[k].get()) {
            c->saved = c->damage + c->saved;
            c->damage = 0.0f;
            v = c->saved;
        }
        out[k] = static_cast<uint8_t>(std::lrint(v * kSaveScale));
    }
    return out;
}

void DamageGrid::repair() {
    for (std::unique_ptr<DamageCell>& c : m_cells) {
        if (!c) continue;
        c->damage = 0.0f;
        c->saved = 0.0f;
    }
}

void DamageGrid::clearRace() {
    for (std::unique_ptr<DamageCell>& c : m_cells) {
        if (c) c->damage = 0.0f;
    }
}

}  // namespace oj
