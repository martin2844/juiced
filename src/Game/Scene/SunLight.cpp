#include "Game/Scene/SunLight.h"

#include "Core/Base/Random.h"
#include "Core/Base/X87.h"

#include <bit>

#include <cmath>

namespace oj::scene {

namespace {

Vec4 placeOf(render::Node& node) {
    const Mat34& w = node.world();
    return {w[3], w[7], w[11], 0.0f};
}

// The light eased toward the sample by at most 0.00004 x the speed (at least 500), then within 0 .. 1.
float ease(float light, float sample, float speed) {
    float change = sample - light;
    const float most = 3.9999999e-05f * (speed < 500.0f ? 500.0f : speed);
    if (change > most) change = most;
    if (change < -most) change = -most;
    return change + light;
}

float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

}  // namespace

void LightStep::update(const SunMap* map, const SceneObjects& objects) {
    const float dx = place[0] - sampledAt[0];
    const float dy = place[1] - sampledAt[1];
    const float dz = place[2] - sampledAt[2];
    const float moved = std::sqrt((dz * dz + dy * dy) + dx * dx);
    switch (kind) {
        case Kind::Part:
            light = chassis != nullptr ? chassis->light : light;
            return;
        case Kind::Eased:
            if (moved > 100.0f) {
                sampledAt = place;
                if (map != nullptr) sample = map->sample(place);
            }
            light = clamp01(ease(light, sample, speed));
            return;
        case Kind::Chassis:
            if (moved > 100.0f) {
                // Four samples 140 along the node's own x and z axes (its matrix under the car's, so the world's x
                // and z whatever the car's heading), averaged; none in a tunnel.
                const Mat34& l = node->local;
                const Vec4 x{l[0] * 140.0f, l[4] * 140.0f, l[8] * 140.0f, 0.0f};
                const Vec4 z{l[2] * 140.0f, l[6] * 140.0f, l[10] * 140.0f, 0.0f};
                sampledAt = place;
                if (map != nullptr) {
                    auto at = [&](const Vec4& d, float sign) {
                        return map->sample({place[0] + sign * d[0], place[1] + sign * d[1], place[2] + sign * d[2],
                                            0.0f});
                    };
                    sample = at(x, 1.0f);
                    sample = at(x, -1.0f) + sample;
                    sample = at(z, 1.0f) + sample;
                    sample = (at(z, -1.0f) + sample) * 0.25f;
                    if (objects.inTunnel(place)) sample = 0.0f;
                }
            }
            light = light == -1.0f ? sample : ease(light, sample, speed);
            light = clamp01(light);
            return;
    }
}

void LightStep::start(const SunMap* map, const SceneObjects& objects, bool atNight) {
    if (started) return;
    started = true;
    own = node->local;
    place = placeOf(*node);
    update(map, objects);
    night = atNight;
}

void LightStep::release() {
    if (kind == Kind::Eased || node == nullptr) return;
    node->local = own;
    node->invalidate();
    if (part.attached) return;
    // 0x00419730 (never reached: nothing clears +0xD8).
    if (parent != nullptr && node->parent() == nullptr) parent->attach(node);
    part.attached = true;
    if (auto* mesh = dynamic_cast<render::ModelNode*>(node); mesh != nullptr) mesh->flags &= ~0x10u;
    if (modelParts != nullptr) *modelParts = static_cast<uint8_t>(*modelParts & ~part.bit);
}

void LightStep::frame(const SunMap* map, const SceneObjects& objects, float dt) {
    auto* mesh = dynamic_cast<render::ModelNode*>(node);
    if (kind != Kind::Eased) {
        // 0x00419780.
        if (!part.attached) {
            if (mesh != nullptr) mesh->flags &= ~4u;
        } else {
            part.wobble(dt, zone, speed, sideways, change);
        }
    }
    const Vec4 now = placeOf(*node);
    const float over = dt > 0.0f ? 1.0f / dt : dt;
    const Vec4 v{(now[0] - place[0]) * over, (now[1] - place[1]) * over, (now[2] - place[2]) * over, 0.0f};
    change = {v[0] - velocity[0], v[1] - velocity[1], v[2] - velocity[2], change[3]};
    speed = std::sqrt((velocity[2] * velocity[2] + velocity[1] * velocity[1]) + velocity[0] * velocity[0]);
    sideways = std::sqrt(change[2] * change[2] + change[0] * change[0]);
    velocity = v;
    place = now;
    if (sideways > 200.0f) {  // [0x007252A4]
        if (mesh != nullptr) mesh->flags |= 8;
        if (sound != nullptr) {
            float loud = 1.0f * sideways;  // [0x007252A0]
            loud = loud < 0.0f ? 0.0f : (loud > 1.0f ? 1.0f : loud);
            sound->setVolume(loud);
            sound->setPlayPosition(0);
            sound->play();
            const float f = static_cast<float>(gameRandom().next()) * std::bit_cast<float>(0x3D7A01F4u) +
                            baseFrequency;  // [0x006FFE20]
            sound->setFrequency(static_cast<uint32_t>(x87::chop(f)));
        }
    }
    update(map, objects);
    if (!night && mesh != nullptr) mesh->lightScale = light;
    // 0x0041B050: the parts' rates (+0x6C .. +0x74) and nod are 0, their angles the wobble's.
    // Stock problem: the turn is given 1 / the step's time (0x0041B2B4 writes it over the argument); with rates of 0
    // it changes nothing.
    if (part.axes != 0) {
        node->local = multiply(own, turnObject(part.axes, part.angles, {0.0f, 0.0f, 0.0f}, 0.0f, over));
        node->invalidate();
    }
}

}  // namespace oj::scene
