#include "Core/Cars/Car.h"

#include "Core/Base/X87.h"

#include <algorithm>

namespace oj {

namespace {

void put32(std::vector<uint8_t>& out, uint32_t v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
}

void put16(std::vector<uint8_t>& out, uint16_t v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + 2);
}

class Reader {
public:
    Reader(const std::vector<uint8_t>& data, size_t& offset) : m_data(data), m_offset(offset) {}
    bool ok() const { return m_ok; }
    void bytes(void* out, size_t n) {
        if (!m_ok || m_offset + n > m_data.size()) {
            m_ok = false;
            return;
        }
        std::memcpy(out, m_data.data() + m_offset, n);
        m_offset += n;
    }
    uint32_t u32() {
        uint32_t v = 0;
        bytes(&v, 4);
        return v;
    }
    uint16_t u16() {
        uint16_t v = 0;
        bytes(&v, 2);
        return v;
    }

private:
    const std::vector<uint8_t>& m_data;
    size_t& m_offset;
    bool m_ok = true;
};

}  // namespace

Car::Car()
    : metallicAmount(bitsOf(0.5f)),
      pearlAmount(bitsOf(0.1f)),
      gearing(bitsOf(0.5f)),
      torqueFactor(bitsOf(1.0f)),
      dynoRun{bitsOf(-1.0f), bitsOf(-1.0f), bitsOf(-1.0f)} {
    body.fill(0xFF);
    mods.fill(0xFF);
}

void Car::write(std::vector<uint8_t>& out) const {
    out.insert(out.end(), model.begin(), model.end());
    for (uint32_t v : {base, metallic, metallicAmount, pearl, pearlAmount, neon, dialColour, dialTextColour,
                       decalColour, sponsorColour, wheelColour, wheelsColoured}) {
        put32(out, v);
    }
    out.insert(out.end(), body.begin(), body.end());
    out.insert(out.end(), mods.begin(), mods.end());
    put32(out, lowering);
    put32(out, gearing);
    put16(out, static_cast<uint16_t>(static_cast<int16_t>(x87::nearest(asFloat(tint) * 100.0f))));
    for (uint16_t v : raceCounts) put16(out, v);
    for (uint32_t v : {spent, winnings, horn, metallicBought, pearlBought, brakeWear, brake, tyreWear, grip, nitro,
                       rating, dynoRun[0], dynoRun[1], dynoRun[2]}) {
        put32(out, v);
    }
    put32(out, grid ? 1 : 0);
    if (grid) {
        const std::array<uint8_t, 256> bytes = DamageGrid::writeBytes(*grid);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    for (uint32_t v : {torqueFactor, steeringPulls, turboLost, neonFlickers, nitroLeaks}) put32(out, v);
    put32(out, sponsorDecals);
}

bool Car::read(const std::vector<uint8_t>& data, size_t& offset) {
    Reader r(data, offset);
    r.bytes(model.data(), model.size());
    for (uint32_t* v : {&base, &metallic, &metallicAmount, &pearl, &pearlAmount, &neon, &dialColour, &dialTextColour,
                        &decalColour, &sponsorColour, &wheelColour, &wheelsColoured}) {
        *v = r.u32();
    }
    r.bytes(body.data(), body.size());
    r.bytes(mods.data(), mods.size());
    lowering = r.u32();
    gearing = r.u32();
    tint = bitsOf(static_cast<float>(static_cast<int16_t>(r.u16())) * 0.01f);
    for (uint16_t& v : raceCounts) v = r.u16();
    for (uint32_t* v : {&spent, &winnings, &horn, &metallicBought, &pearlBought, &brakeWear, &brake, &tyreWear,
                        &grip, &nitro, &rating, &dynoRun[0], &dynoRun[1], &dynoRun[2]}) {
        *v = r.u32();
    }
    if (r.u32() != 0) {
        std::array<uint8_t, 256> g{};
        r.bytes(g.data(), g.size());
        grid = DamageGrid::readBytes(g);
    } else {
        grid.reset();
    }
    for (uint32_t* v : {&torqueFactor, &steeringPulls, &turboLost, &neonFlickers, &nitroLeaks}) *v = r.u32();
    sponsorDecals = r.u32();
    return r.ok();
}

}  // namespace oj
