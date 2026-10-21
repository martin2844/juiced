// TRACKS: the tyres' marks on the road.

#include "Game/Scene/Particles/TracksEmitter.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oj::scene {

using render::Primitive;

using namespace particles;

namespace {

uint8_t lowByte(float v) {
    return static_cast<uint8_t>(nearest(v) & 0xFF);
}

float floatAt(const uint8_t* p, size_t offset) {
    float f;
    std::memcpy(&f, p + offset, 4);
    return f;
}

}  // namespace

TracksEmitter::TracksEmitter(ParticleSystem& system, const EffectDef& d) : Emitter(system, d) {}

TracksEmitter::~TracksEmitter() {
    for (Track& t : m_tracks) {
        if (t.buffer != 0) m_system.engine().device().destroyBuffer(t.buffer);
    }
}

void TracksEmitter::setup(uint32_t wheels, uint32_t cars) {
    if (cars == 0) return;
    m_entries = def.points / cars;
    m_strips = 0xF0u / cars;
    m_stride = wheels * 8 + 0x10;
    if (m_entries < 2 || m_strips < 2) return;
    m_ring.assign(static_cast<size_t>(m_entries) * m_stride, 0);
    m_current = 0;
    m_last = static_cast<int32_t>(m_entries) - 1;
    m_tracks.resize(wheels);
    const std::vector<uint8_t> zeros(0x36 * sizeof(Vertex));
    for (size_t i = 0; i < wheels; ++i) {
        Track& t = m_tracks[i];
        t.def = &def;
        t.wheel = i;
        t.strips.assign(m_strips, Strip{});
        t.buffer = m_system.engine().device().createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
    }
    resetTracks();
    m_moved = false;
}

void TracksEmitter::resetTracks() {
    for (Track& t : m_tracks) {
        t.current = 0;
        t.oldest = 0;
        t.asked = t.def;
        t.count = 0;
        t.last = {0.0f, 0.0f, 0.0f, t.last[3]};
        t.strips[0].first = -1;
        t.strips[0].count = 0;
        t.wasOn = false;
        t.open = false;
        t.entry = -1;
        t.on = false;
        t.pending = -1;
        t.kept = 0;
        t.drawn = 0;
        t.switchIn = -1;
    }
}

int32_t TracksEmitter::nextEntry(int32_t index) const {
    const int32_t n = index + 1;
    return n == m_last ? 0 : n;
}

void TracksEmitter::askDefinition(size_t wheel, const EffectDef* d) {
    if (wheel >= m_tracks.size()) return;
    Track& t = m_tracks[wheel];
    t.asked = d;
    if (d != t.def) t.switchIn = 1;
}

void TracksEmitter::advance() {
    for (Track& t : m_tracks) {
        if (t.switchIn > -1) {
            t.switchIn = t.switchIn - 1;
            if (t.switchIn == 0) t.def = t.asked;
        }
    }
    if (!m_moved || m_ring.empty()) return;
    const int32_t next = nextEntry(m_current);
    for (Track& t : m_tracks) advanceTrack(t, next);
    m_current = next;
    m_moved = false;
}

void TracksEmitter::setOrigin(const Vec4& carOrigin) {
    if (m_ring.empty()) return;
    std::memcpy(entryAt(m_current), carOrigin.data(), 12);
}

// 0x0050C7D0: the wheel's mark on the current entry. With a touch it is on unless the levels' sum is below the asked
// definition's trigger; a change of on or off, or a touch over 70.7 cm from the last change across the ground, asks the
// ring to advance. On: the touch from the origin in steps of 2.2 within -127 .. 127, the alphas (the levels with
// MIN_AMOUNT over the trigger, x (1 - BRIGHTNESS_RANGE + a draw x BRIGHTNESS_RANGE)), the side (the move's normal
// across the ground, 117 .. 127 long by a draw each); the entry it went on at takes the side of the next and the alphas
// MIN_AMOUNT rounded.
void TracksEmitter::mark(size_t wheel, const Vec4* touch, float a, float b) {
    if (wheel >= m_tracks.size() || m_ring.empty()) return;
    Track& t = m_tracks[wheel];
    uint8_t* e = entryAt(m_current);
    t.entry = m_current;
    t.origin = {floatAt(e, 0), floatAt(e, 4), floatAt(e, 8), t.origin[3]};
    bool changed = false;
    if (touch != nullptr) {
        t.on = true;
        const float dz = (*touch)[2] - t.last[2];
        const float dx = (*touch)[0] - t.last[0];
        if (5000.0f < dz * dz + dx * dx) changed = true;
    } else {
        t.on = false;
    }
    const float trigger = t.asked != nullptr ? t.asked->triggerLevel : 0.0f;
    if (a + b < trigger) t.on = false;
    if (t.wasOn != t.on) changed = true;
    if (t.on && t.def != nullptr && touch != nullptr) {
        const EffectDef& d = *t.def;
        uint8_t* slot = e + 0x10 + wheel * 8;
        const float bLevel = b + d.minAmount;
        float sx = ((*touch)[0] - t.origin[0]) * 0.45454544f;
        float sy = ((*touch)[1] - t.origin[1]) * 0.45454544f;
        float sz = ((*touch)[2] - t.origin[2]) * 0.45454544f;
        const float inverse = 1.0f / (1.0f - trigger);
        ParticleRandom& random = m_system.random();
        const float r1 = random.next();
        float alphaA =
            ((1.0f - d.brightnessRange) + r1 * d.brightnessRange) * (((a + d.minAmount) - trigger) * inverse);
        const float r2 = random.next();
        float alphaB = (bLevel - trigger) * inverse * ((1.0f - d.brightnessRange) + r2 * d.brightnessRange);
        alphaA = std::clamp(alphaA, 0.0f, 1.0f);
        alphaB = std::clamp(alphaB, 0.0f, 1.0f);
        sx = std::clamp(sx, -127.0f, 127.0f);
        sy = std::clamp(sy, -127.0f, 127.0f);
        sz = std::clamp(sz, -127.0f, 127.0f);
        slot[3] = lowByte(alphaA * 255.0f);
        slot[4] = lowByte(alphaB * 255.0f);
        const float mx = (*touch)[0] - t.last[0];
        const float mz = (*touch)[2] - t.last[2];
        const float over = 1.0f / (std::sqrt(mz * mz + mx * mx) + 1.1920929e-07f);
        const float r3 = random.next();
        const float r4 = random.next();
        slot[6] = lowByte((over * mz) * (r3 * 10.0f + 117.0f));
        slot[5] = lowByte(-((mx * over) * (r4 * 10.0f + 117.0f)));
        if (!t.wasOn) {
            t.pending = m_current;
        } else if (t.pending >= 0) {
            uint8_t* before = entryAt(t.pending) + 0x10 + wheel * 8;
            before[6] = slot[6];
            before[5] = slot[5];
            before[3] = lowByte(d.minAmount);  // 0x00424230: MIN_AMOUNT rounded, not scaled
            before[4] = lowByte(d.minAmount);
            t.pending = -1;
        }
        slot[0] = lowByte(sx);
        slot[1] = lowByte(sy);
        slot[2] = lowByte(sz);
    }
    if (changed) {
        if (touch != nullptr) t.last = *touch;
        if (t.wasOn || t.on) {
            t.wasOn = t.on;
            m_moved = true;
        }
    }
}

// 0x0050C5E0: a strip from the marked entry, two entries long, its bounds the entry's origin with a pad of 200; the
// next entry takes the origin and the wheel's slot.
void TracksEmitter::startStrip(Track& t, int32_t next) {
    Strip& s = t.strips[t.current];
    s.first = t.entry;
    s.count = 2;
    s.def = t.def;
    s.middle = t.origin;
    s.top = {t.origin[0], t.origin[1], t.origin[2], 0.0f};
    s.bottom = s.top;
    s.radius = 200.0f;
    s.pad = 200.0f;
    t.count = t.count + 1;
    t.open = true;
    uint8_t* to = entryAt(next);
    std::memcpy(to, t.origin.data(), 12);
    std::memcpy(to + 0x10 + t.wheel * 8, entryAt(t.entry) + 0x10 + t.wheel * 8, 8);
}

// 0x0050C670: the next strip place emptied (the oldest strip dropped when it is there).
void TracksEmitter::closeStrip(Track& t) {
    const size_t end = t.strips.size() - 1;
    size_t next = t.current + 1;
    if (next == end) next = 0;
    t.open = false;
    if (next == t.oldest) {
        size_t o = t.oldest + 1;
        if (o == end) o = 0;
        t.oldest = o;
    }
    t.current = next;
    t.strips[next].first = -1;
    t.strips[next].count = 0;
}

// 0x0050C6C0: the ring moves on to `next`. The oldest strip starting where the entry after it is loses that entry
// (at 3 entries or less it is dropped); then a strip starts (on), closes (off), closes and starts again (26 entries,
// or the definition changing this frame), or grows by the entry (its bounds, 0x0050D0C0), the next entry taking the
// origin and the wheel's slot.
void TracksEmitter::advanceTrack(Track& t, int32_t next) {
    const size_t end = t.strips.size() - 1;
    const int32_t after = nextEntry(next);
    Strip& old = t.strips[t.oldest];
    if (old.first == after) {
        old.first = nextEntry(old.first);
        if (old.count > 3) {
            old.count = old.count - 1;
        } else {
            size_t o = t.oldest + 1;
            if (o == end) o = 0;
            t.count = t.count - 1;
            t.oldest = o;
        }
    }
    if (!t.open) {
        if (t.on) startStrip(t, next);
        return;
    }
    if (!t.on) {
        closeStrip(t);
        return;
    }
    Strip& s = t.strips[t.current];
    if (s.count == 0x1A || t.switchIn == 0) {
        closeStrip(t);
        startStrip(t, next);
        return;
    }
    const Vec4& p = t.origin;
    for (size_t k = 0; k < 3; ++k) {
        if (p[k] > s.top[k]) s.top[k] = p[k];
        if (p[k] < s.bottom[k]) s.bottom[k] = p[k];
    }
    for (size_t k = 0; k < 3; ++k) s.middle[k] = (s.bottom[k] + s.top[k]) * 0.5f;
    const float dz = s.top[2] - s.bottom[2];
    const float dy = s.top[1] - s.bottom[1];
    const float dx = s.top[0] - s.bottom[0];
    s.radius = std::sqrt((dx * dx + dy * dy) + dz * dz) * 0.5f + s.pad;
    s.count = s.count + 1;
    if (t.kept < m_entries) t.kept = t.kept + 1;
    uint8_t* to = entryAt(next);
    std::memcpy(to, t.origin.data(), 12);
    std::memcpy(to + 0x10 + t.wheel * 8, entryAt(t.entry) + 0x10 + t.wheel * 8, 8);
}

// 0x0050C560: the camera's distance across the ground from the strip's middle (squared, less the radius squared) x
// the sine of half its field of view within the definition's LOD_OFF_MUL, and the strip's sphere in the view.
bool TracksEmitter::visible(const Track& t, const Strip& s, render::Camera& camera) const {
    const Mat34& w = camera.world();
    const float dx = w[3] - s.middle[0];
    const float dz = w[11] - s.middle[2];
    const float r = s.radius;
    const float far = (dz * dz + dx * dx) - r * r;
    if (!(camera.fovSin() * far <= t.def->lodOffMul)) return false;
    return camera.sphereVisible(s.middle, s.radius);
}

bool TracksEmitter::frame(render::Camera& camera) {
    for (Track& t : m_tracks) drawTrack(t, camera);
    return true;
}

// 0x0050D490: from the oldest strip, each of two entries or more in sight drawn as a triangle strip in its
// definition's colour: each entry's place (the origin + the steps x 2.2) 10 up, its side x START_SIZE to either side,
// alphas the entry's bytes x the level, u 0 and 1 by turns, v 0 and 1 across. The level starts at (entries - the
// entries drawn last frame) x the step (COLOUR_START's alpha / the ring's entries) and grows by the step to the alpha
// / 255 after each entry. Stock problem: the first entry drawn takes the level before it is held to that, its alphas
// wrapping past a byte.
bool TracksEmitter::drawTrack(Track& t, render::Camera& camera) {
    if (t.def == nullptr || m_ring.empty()) return true;
    const float step = t.def->colourStart[3] / static_cast<float>(m_entries);
    float fade = (static_cast<float>(m_entries) - static_cast<float>(t.drawn)) * step;
    if (!(fade >= 0.0f)) fade = 0.0f;
    t.drawn = 0;
    render::IRenderDevice& device = m_system.engine().device();
    render::Renderer3d& r = m_system.engine().renderer();
    const size_t end = t.strips.size() - 1;
    size_t index = t.oldest;
    for (uint32_t k = 0; k < t.count; ++k) {
        const Strip& s = t.strips[index];
        const uint32_t n = s.count;
        if (n > 1 && s.def != nullptr && s.first >= 0 && visible(t, s, camera)) {
            const EffectDef& d = *s.def;
            t.drawn = t.drawn + n;
            const uint32_t rgb = static_cast<uint32_t>(lowByte(d.colourStart[0])) << 16 |
                                 static_cast<uint32_t>(lowByte(d.colourStart[1])) << 8 | lowByte(d.colourStart[2]);
            m_vertices.clear();
            float u = 0.0f;
            int32_t entry = s.first;
            for (uint32_t j = 0; j < n; ++j) {
                const uint8_t* e = entryAt(entry);
                const uint8_t* slot = e + 0x10 + t.wheel * 8;
                const float px = static_cast<float>(static_cast<int8_t>(slot[0])) * 2.2f + floatAt(e, 0);
                const float py = static_cast<float>(static_cast<int8_t>(slot[1])) * 2.2f + floatAt(e, 4);
                const float pz = static_cast<float>(static_cast<int8_t>(slot[2])) * 2.2f + floatAt(e, 8);
                const float fa = static_cast<float>(slot[3]) * fade;
                const float fb = static_cast<float>(slot[4]) * fade;
                fade = fade + step;
                const float cap = d.colourStart[3] * 0.003921569f;
                if (!(fade <= cap)) fade = cap;
                const float wx = static_cast<float>(static_cast<int8_t>(slot[6])) * d.startSize;
                const float wz = static_cast<float>(static_cast<int8_t>(slot[5])) * d.startSize;
                m_vertices.push_back({px + wx, py + t.lift, pz + wz, static_cast<uint32_t>(lowByte(fb)) << 24 | rgb,
                                      u, 0.0f});
                m_vertices.push_back({px - wx, py + t.lift, pz - wz, static_cast<uint32_t>(lowByte(fa)) << 24 | rgb,
                                      u, 1.0f});
                u = 1.0f - u;
                entry = nextEntry(entry);
            }
            device.updateVertexBuffer(t.buffer, m_vertices.data(),
                                      static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
            if (render::Material* material = m_system.material(d.material)) {
                r.beginMaterialPass(*material, 0);
                device.setStream(t.buffer, sizeof(Vertex));
                device.draw(Primitive::TriangleStrip, 0, n * 2 - 2);
            }
        }
        index = index + 1 == end ? 0 : index + 1;
    }
    return true;
}

}  // namespace oj::scene
