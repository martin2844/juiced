// A particle, the wind the particles drift in, and the emitters' base classes: an effect's emitter on a node and one of
// particles kept.

#include "Game/Scene/Particles/Emitter.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <cmath>

namespace oj::scene {

using namespace particles;

namespace {

Vec4 placeOf(render::Node& node) {
    const Mat34& w = node.world();
    return {w[3], w[7], w[11], 0.0f};
}

// 0x005059B0: a particle caught in a car's wake turns about the line through `centre` along `axis`: its radius the
// distance from the line (at least 10), its turn's frequency the car's speed x 20 / (radius² x its distance from the
// centre + 5), at most 2.5, none below 0.05 (0 then, and 0 is returned; else the radius).
float catchInWake(Particle& p, const Vec4& centre, const Vec4& axis, float speed, float sign) {
    const float dx = p.place[0] - centre[0];
    const float dy = p.place[1] - centre[1];
    const float dz = p.place[2] - centre[2];
    const float d = std::sqrt((dz * dz + dy * dy) + dx * dx);
    const float inv = 1.0f / d;
    const float nx = dx * inv, ny = dy * inv, nz = dz * inv;
    const float dot = (nx * axis[0] + nz * axis[2]) + ny * axis[1];
    const float px = nx - axis[0] * dot;
    const float py = ny - axis[1] * dot;
    const float pz = nz - axis[2] * dot;
    const float cx = pz * axis[1] - py * axis[2];
    const float cy = px * axis[2] - pz * axis[0];
    const float cz = py * axis[0] - px * axis[1];
    const float q = 1.0f / (std::sqrt((pz * pz + py * py) + px * px) + kEpsilon);
    const float r = 1.0f / (std::sqrt((cz * cz + cy * cy) + cx * cx) + kEpsilon);
    const float along = (dx * axis[0] + dz * axis[2]) + dy * axis[1];
    const float ex = p.place[0] - (centre[0] + axis[0] * along);
    const float ey = p.place[1] - (centre[1] + axis[1] * along);
    const float ez = p.place[2] - (centre[2] + axis[2] * along);
    float m = std::sqrt((ez * ez + ey * ey) + ex * ex);
    if (m < 10.0f) m = 10.0f;
    p.across = {px * q * -1.0f, py * q * -1.0f, pz * q * -1.0f, p.place[3]};
    p.phase = 0.0f;
    p.along = {cx * r * sign, cy * r * sign, cz * r * sign, 0.0f};
    p.swirl = m;
    p.frequency = speed * 20.0f / (m * m * d + 5.0f);
    if (p.frequency > 2.5f) p.frequency = 2.5f;
    if (0.5f * 0.1f > p.frequency) {
        p.frequency = 0.0f;
        return 0.0f;
    }
    return m;
}

// The set of a SOUND_EFFECT entry by its name in the 3D player (bank -1, set -1 when none has it).
audio::SetRef soundSet(audio::World& world, const EffectSound& e) {
    audio::SetRef ref{-1, -1};
    world.player().find(e.name.c_str(), ref);
    return ref;
}

}  // namespace

void ParticleWind::addSource(render::Node& node) {
    Source s;
    s.node = &node;
    s.place = placeOf(node);
    m_sources.push_back(s);
}

void ParticleWind::step(float dt) {
    for (Source& s : m_sources) {
        const Vec4 now = placeOf(*s.node);
        s.velocity = {now[0] - s.place[0], now[1] - s.place[1], now[2] - s.place[2], s.velocity[3]};
        s.place = now;
        if (dt > 0.0f) {
            const float over = 1.0f / dt;
            s.velocity = {s.velocity[0] * over, s.velocity[1] * over, s.velocity[2] * over, s.velocity[3]};
        }
        const Vec4& v = s.velocity;
        if (std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]) > 10000.0f) {
            s.velocity = {0.0f, 0.0f, 0.0f, s.velocity[3]};
        }
    }
}

Vec4 ParticleWind::at(Particle& p, const Vec4& emitterVelocity, float time, float* gain, float* speed,
                      Vec4* kick) const {
    float vx = m_still[0], vy = m_still[1], vz = m_still[2];
    const float vw = m_still[3];
    constexpr float kRange = 200.0f;  // [0x007256F4]
    for (const Source& s : m_sources) {
        const Mat34& w = s.node->world();
        const float dx = p.place[0] - w[3];
        const float dy = p.place[1] - w[7];
        const float dz = p.place[2] - w[11];
        float f = ((dx * dx + dz * dz) + dy * dy) * 3.0f;
        if (!(f < kRange * kRange * 100.0f)) continue;
        f = f - kRange * kRange;
        if (f < 1.0f) f = 1.0f;
        const float rx = s.velocity[0] - emitterVelocity[0];
        const float ry = s.velocity[1] - emitterVelocity[1];
        const float rz = s.velocity[2] - emitterVelocity[2];
        const float rel = std::sqrt((rx * rx + ry * ry) + rz * rz);
        float g = rel / f * 50.0f;
        if (g > 15.0f) g = 15.0f;
        if (speed != nullptr) *speed = rel;
        if (gain != nullptr) *gain = g;
        vx = dx * g + vx;
        vy = dy * g + vy;
        vz = dz * g + vz;
        if (!(p.phase < 0.0f)) continue;
        // Its wake: behind the car (its z axis back, a tenth of its y axis down) at 200; a particle ahead of that point
        // along the wake turns about a line 20 above it and 40 to the side it is on.
        const Vec4 axis{w[1] * -0.1f + -w[2], w[5] * -0.1f + -w[6], w[9] * -0.1f + -w[10], 0.0f};
        Vec4 centre{axis[0] * kRange + w[3], axis[1] * kRange + w[7], axis[2] * kRange + w[11], 0.0f};
        const float ex = p.place[0] - centre[0];
        const float ey = p.place[1] - centre[1];
        const float ez = p.place[2] - centre[2];
        if (!((ex * axis[0] + ez * axis[2]) + ey * axis[1] > 0.0f)) continue;
        const float sign = (w[4] * ey + w[8] * ez) + ex * w[0] >= 0.0f ? -1.0f : 1.0f;
        centre = {w[1] * 20.0f + centre[0], w[5] * 20.0f + centre[1], w[9] * 20.0f + centre[2], 0.0f};
        centre = {w[0] * -40.0f * sign + centre[0], w[4] * -40.0f * sign + centre[1], w[8] * -40.0f * sign + centre[2],
                  0.0f};
        const Vec4& sv = s.velocity;
        const float carSpeed = std::sqrt((sv[0] * sv[0] + sv[1] * sv[1]) + sv[2] * sv[2]);
        const float m = catchInWake(p, centre, axis, carSpeed, sign);
        if (kick != nullptr && m < kRange && m > 0.0f && carSpeed > 2500.0f) {
            // ((axis x speed) x [0x007256EC]) x the draw.
            const float r = x87::unit(gameRandom().next());
            *kick = {axis[0] * carSpeed * -0.95f * r, axis[1] * carSpeed * -0.95f * r, axis[2] * carSpeed * -0.95f * r,
                     axis[3]};
        }
    }
    const float sway = m_sway;
    const float c = std::cos(time * 1.0f + p.place[0] * 0.002f);
    const float sn = std::sin(time * 1.5f + p.place[2] * 0.003f);
    return {c * sway + vx, 0.0f * sway + vy, sway * sn + vz, vw};
}

ParticleEmitter::ParticleEmitter(ParticleSystem& system, const EffectDef& d) : Emitter(system, d) {
    m_points = d.points;
    m_life = d.lifetime > 0.0f ? d.lifetime : static_cast<float>(m_points) * d.emitInterval;
    const Vec4& start = d.colourStart;
    const Vec4& sun = system.sunColour;
    const Vec4& shade = system.shadeColour;
    m_sunColour = {(sun[0] + start[0]) * 0.5f, (sun[1] + start[1]) * 0.5f, (sun[2] + start[2]) * 0.5f, start[3]};
    m_shadeColour = {(shade[0] + start[0]) * 0.5f, (shade[1] + start[1]) * 0.5f, (shade[2] + start[2]) * 0.5f,
                     start[3]};
    m_material = system.material(d.material);
    m_particles.resize(m_points);
    setLevel(1.0f);
}

Emitter::Emitter(ParticleSystem& system, const EffectDef& d)
    : def(d), soundVolume(d.soundVolume), soundWorld(system.soundWorld), m_system(system) {}

Emitter::~Emitter() {
    if (sound != nullptr) sound->release();
}

void Emitter::setNode(render::Node* n) {
    node = n;
    if (sound != nullptr) sound->setNode(n);
}

void Emitter::makeSound(audio::World& world, int32_t draw) {
    if (!def.hasSound || def.soundTrigger != 1 || def.sounds.empty() || draw < 0) return;
    const EffectSound& e = def.sounds[0];
    audio::SetRef ref = soundSet(world, e);
    // Stock problem: a name not found with a count over 1 gives set -1 + r % count of bank -1 (no set).
    ref.set = ref.set + draw % e.value;
    sound = world.createRef(ref, 0);
    if (sound == nullptr) return;
    sound->setRolloff(def.soundRolloff);
    sound->setNode(node);
    sound->setDoppler(def.soundDoppler);
    sound->setLooped(def.soundTrigger);
    soundVolume = def.soundVolume;
    sound->setGroup(-1);
    sound->setMinDistance(def.soundMinDist);
    sound->stop();
}

void Emitter::setLevel(float value, float mul) {
    level = value;
    if (value > def.triggerLevel && value < def.minAmount) level = def.minAmount;
    if (!def.hasSound) return;
    const float t = def.soundTriggerLevel;
    float v = 0.0f;
    if (value > t) {
        v = (value / mul - t) * (1.0f / (1.0f - t)) + def.soundMinVol;
        v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
    soundNow = v * soundVolume;
    if (sound != nullptr && def.soundTrigger == 1) {
        if (!sound->sounding()) sound->play();
        sound->setVolume(soundNow);
    }
}

audio::Instance* Emitter::oneShot(const Vec4& place, CrtRandom& random) {
    if (!def.hasSound || soundWorld == nullptr || def.soundTrigger != 0 || def.sounds.empty()) return sound;
    const size_t n = def.sounds.size();
    size_t i = 0;
    if (n > 1) i = static_cast<size_t>(x87::chop(static_cast<float>(n - 1) * level));
    if (i >= n) i = n - 1;
    const EffectSound& e = def.sounds[i];
    audio::SetRef ref = soundSet(*soundWorld, e);
    ref.set = ref.set + random.next() % e.value;
    soundWorld->playOnceRef(ref, {place[0], place[1], place[2], place[3]}, &sound);
    if (sound != nullptr) {
        sound->setRolloff(def.soundRolloff);
        sound->setDoppler(def.soundDoppler);
        sound->setLooped(def.soundTrigger);
        sound->setVolume(soundNow);
        sound->setGroup(0);
        sound->setMinDistance(def.soundMinDist);
    }
    return sound;
}

void ParticleEmitter::setFrequency(float hz) {
    soundFrequency(sound, hz);
}

void ParticleEmitter::clear() {
    for (Particle& p : m_particles) p.age = -1.0f;
}

Vec4 Emitter::world() const {
    if (node == nullptr) return local;
    return transformPoint(node->world(), local);
}

void ParticleEmitter::sampleLight() {
    const Vec4 p = node != nullptr ? placeOf(*node) : world();
    // Stock problem: the map is read without a check (a route without one stops the game here).
    if (m_system.sunMap != nullptr) m_light = m_system.sunMap->sample(p);
}

bool ParticleEmitter::prepare(Vec4& axis1, Vec4& axis2, Vec4* axis3) {
    axis1 = m_system.billboard[0];
    axis2 = m_system.billboard[1];
    if (axis3 != nullptr) *axis3 = m_system.billboard[2];
    if (!(m_system.dt > 0.0f)) return true;
    if (m_alive <= 0 && dying) return false;
    if (!active && !dying) return true;
    const Vec4 p = world();
    if (node != nullptr) {
        const float dx = m_sampledAt[0] - p[0];
        const float dz = m_sampledAt[2] - p[2];
        if (dz * dz + dx * dx > 90000.0f) {
            if (m_system.sunMap != nullptr) m_light = m_system.sunMap->sample(p);
            m_sampledAt = p;
        }
    } else {
        velocity = {0.0f, 0.0f, 0.0f, p[3]};
    }
    const float q = m_system.quality;
    if (!(level < def.triggerLevel)) m_timer = m_timer - 0.02f;
    int32_t n = burst();
    float spread = 0.01f;
    while (q * def.triggerLevel <= level) {
        if (!(m_timer <= 0.0f) || n <= 0) break;
        m_timer = def.emitInterval + m_timer;
        Particle& slot = m_particles[m_next];
        ++m_next;
        letOut(slot, spread);
        spread = 0.007f + spread;
        if (m_next == m_points) m_next = 0;
        --n;
    }
    return true;
}

void ParticleEmitter::letOut(Particle& p, float spread) {
    const Vec4 w = world();
    p.place = {w[0] - velocity[0] * spread, w[1] - velocity[1] * spread, w[2] - velocity[2] * spread, w[3]};
    p.level = level;
    p.lit = m_light > 0.5f ? 1 : 0;
    p.frequency = 0.0f;
    p.phase = -1.0f;
    p.age = 0.0f;
}

}  // namespace oj::scene
