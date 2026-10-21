// The cars' particles: made with the car, then each frame's impacts, flames, skids and smoke.

#include "Game/Race/TrackView/TrackView.h"

#include <bit>
#include <cmath>

namespace app {

void TrackView::makeCarParticles(RaceSlot& s) {
    oj::scene::ParticleSystem* system = scene->particles();
    if (system == nullptr) return;
    const oj::CarEffects& e = s.car.effects;
    CarParticles& p = s.particles;
    oj::render::Node* root = s.model->root();
    auto make = [&](const oj::EffectDef* const& def, bool light) -> std::shared_ptr<oj::scene::Emitter> {
        if (def == nullptr) return nullptr;
        std::shared_ptr<oj::scene::Emitter> made = system->make(def->name);
        if (!made) return nullptr;
        // 0x0050CE70's loop with the value drawn as the car's effects were made.
        if (sound3d) made->makeSound(*sound3d, e.soundDraw(&def));
        made->setNode(root);
        if (light) made->sampleLight();
        return made;
    };
    p.tracks = make(e.tracks, false);
    // 0x0048FED2: the tracks of the four wheels, the ring shared by the race's cars.
    if (auto* tracks = dynamic_cast<oj::scene::TracksEmitter*>(p.tracks.get())) {
        tracks->setup(4, static_cast<uint32_t>(cars.size()));
    }
    // The flames at the exhausts' nodes: EXHAUST_C, else EXHAUST_S and EXHAUST_O (0x00485A0E), in the car's neon
    // colour (+0x2C) with each of r, g and b at least 96, from alpha 191 to 0.
    const char* exhausts[2] = {s.car.exhausts[1] ? "EXHAUST_S" : (s.model->hasNode("EXHAUST_C") ? "EXHAUST_C" :
                                                                   "EXHAUST_S"),
                               "EXHAUST_O"};
    const oj::Car& record = s.car.car != nullptr ? *s.car.car : s.record;
    for (size_t i = 0; i < 2; ++i) {
        p.flames[i] = make(e.flames[i], false);
        if (!p.flames[i]) continue;
        p.flames[i]->setNode(s.model->node(exhausts[i]));
        auto channel = [&](uint32_t shift) {
            const auto c = static_cast<float>((record.neon >> shift) & 0xFF);
            return c < 96.0f ? 96.0f : c;
        };
        const oj::Vec4 rgb{channel(16), channel(8), channel(0), 191.0f};
        if (auto* sparks = dynamic_cast<oj::scene::SparksEmitter*>(p.flames[i].get())) {
            sparks->setColours(rgb, {rgb[0], rgb[1], rgb[2], 0.0f});
        }
        // 0x00556AE0: the flame's mesh (exhaust_flame, 0x00554D30 with the cars' folders in use: carmesh's) in a
        // mesh node under the exhaust's node (0x005348E0).
        if (oj::render::Node* exhaust = s.model->node(exhausts[i]); exhaust != nullptr) {
            oj::render::Mesh* mesh = nullptr;
            {
                const oj::scene::CarFolders folders(scene->engine());
                mesh = scene->engine().mesh("exhaust_flame");
            }
            s.flameNodes[i] = s.model->addMeshNode(*exhaust, mesh, e.flameLook[i]);
        }
    }
    p.heatHaze = make(e.heatHaze, false);
    p.explodeSparks = make(e.explodeSparks, true);
    p.explodeSmoke = make(e.explodeSmoke, true);
    p.chassisSmoke = make(e.chassisSmoke, true);
    p.scrapeSparks = make(e.scrapeSparks, true);
    p.flakes = make(e.flakes, true);
    for (size_t i = 0; i < 4; ++i) {
        p.tarmacSkid[i] = make(e.tarmacSkid[i], true);
        if (p.tarmacSkid[i]) p.tarmacSkid[i]->setLevel(0.0f);
        p.sandSkid[i] = make(e.sandSkid[i], true);
        if (p.sandSkid[i]) p.sandSkid[i]->setLevel(0.0f);
        p.spray[i] = make(e.spray[i], true);
    }
    p.tarmacSqueal = make(e.tarmacSqueal, true);
    if (p.tarmacSqueal) p.tarmacSqueal->setLevel(0.0f);
    p.sandSqueal = make(e.sandSqueal, true);
    if (p.sandSqueal) p.sandSqueal->setLevel(0.0f);
}

void TrackView::impactParticles(RaceSlot& s) {
    const oj::CarEffects& e = s.car.effects;
    if (!e.impactRan) return;
    CarParticles& p = s.particles;
    auto follow = [&s](oj::scene::Emitter& emitter) {
        emitter.origin = s.car.jolt.origin;
        emitter.velocity = s.car.jolt.velocity;
    };
    auto place = [&e](oj::scene::Emitter& emitter) {
        emitter.local = {e.impactArm[0], e.impactArm[1], e.impactArm[2], emitter.local[3]};
    };
    oj::scene::Emitter* sparks = p.explodeSparks.get();
    const auto* burst = dynamic_cast<const oj::scene::SparksEmitter*>(sparks);
    const bool explode = sparks != nullptr && (burst == nullptr || !burst->bursting());
    // Stock problem: with the explosion's sparks made, its smoke and the flakes are taken as made too, and the
    // chassis's smoke with the scrape's sparks; here a missing one is passed over.
    oj::scene::Emitter* smoke = p.explodeSmoke.get();
    oj::scene::Emitter* flakes = p.flakes.get();
    oj::scene::Emitter* scrape = p.scrapeSparks.get();
    oj::scene::Emitter* chassis = p.chassisSmoke.get();
    // The sounds' frequency of a hit: 200 x the speed against the other side + 30000.
    const float hz = 200.0f * e.impactSpeed + 30000.0f;
    if (explode) {
        if (e.impactHit) sparks->setFrequency(hz);
        for (oj::scene::Emitter* emitter : {sparks, smoke, flakes}) {
            if (emitter == nullptr) continue;
            // The flakes' sound level is over 3 (their one-shots by the level: colsmall, colmedium, colbig).
            emitter->setLevel(e.impactHit ? e.impactLevel : 0.0f, emitter == flakes ? 3.0f : 1.0f);
            if (!e.impactHit) {
                // No hit: the explosion's sound stopped while it sounds.
                if (emitter == sparks && sparks->sound != nullptr && sparks->sound->sounding()) {
                    sparks->sound->stop();
                }
                continue;
            }
            place(*emitter);
            follow(*emitter);
            if (emitter == sparks && sparks->sound != nullptr && !sparks->sound->sounding()) sparks->sound->play();
            if (emitter == flakes) {
                // A new one-shot only once the last one ended (the library clears +0x4C); from 800, group 3.
                if (flakes->sound == nullptr) flakes->oneShot(flakes->worldPlace(), oj::gameRandom());
                if (flakes->sound != nullptr) {
                    flakes->sound->setMinDistance(800.0f);  // [0x00717870]
                    flakes->sound->setGroup(3);
                }
            }
        }
    }
    if (scrape != nullptr) {
        if (e.impactHit) scrape->setFrequency(hz);
        for (oj::scene::Emitter* emitter : {scrape, chassis}) {
            if (emitter == nullptr) continue;
            if (e.impactHit) place(*emitter);
            emitter->setLevel(e.impactScrape);
            follow(*emitter);
        }
        // The scrape's loop: a hit puts it in group 3 and plays it; no hit stops it at a level of 0 (the level
        // above has just played it), else plays it.
        if (oj::audio::Instance* loop = scrape->sound) {
            if (e.impactHit) {
                loop->setGroup(3);
                if (!loop->sounding()) loop->play();
            } else if (e.impactScrape <= 0.0f) {
                if (loop->sounding()) loop->stop();
            } else if (!loop->sounding()) {
                loop->play();
            }
        }
    }
    for (oj::scene::Emitter* emitter : {sparks, scrape, flakes}) {
        if (emitter != nullptr) emitter->ground = s.car.lowest;
    }
}

void TrackView::flameParticles(RaceSlot& s) {
    CarParticles& p = s.particles;
    const oj::CarEffects& e = s.car.effects;
    for (size_t i = 0; i < 2; ++i) {
        oj::scene::Emitter* flame = p.flames[i].get();
        if (!s.car.exhausts[i] || flame == nullptr) continue;
        if (!e.flameOn[i]) {
            flame->setLevel(0.0f);
        } else {
            flame->local = {0.0f, 0.0f, -30.0f, 0.0f};
            flame->setLevel(e.flameEmit[i]);
            flame->origin = s.car.jolt.origin;
            flame->velocity = s.car.jolt.velocity;
        }
        flame->ground = s.car.lowest;
    }
    // 0x004914B9: the flames' meshes shown at their matrices while on, else hidden.
    for (size_t i = 0; i < 2; ++i) {
        oj::render::ModelNode* node = s.flameNodes[i];
        if (!s.car.exhausts[i] || node == nullptr) continue;
        if (e.flameShown[i]) {
            node->local = e.flameMatrix[i];
            node->invalidate();
            node->flags &= ~4u;
        } else {
            node->flags |= 4u;
        }
    }
    if (oj::scene::Emitter* haze = p.heatHaze.get(); haze != nullptr) {
        haze->local = {0.0f, 70.0f, 180.0f, haze->local[3]};
        haze->ground = s.car.lowest;
    }
}

void TrackView::carParticles(RaceSlot& s) {
    CarParticles& p = s.particles;
    const oj::CarEffects& e = s.car.effects;
    auto* tracks = dynamic_cast<oj::scene::TracksEmitter*>(p.tracks.get());
    if (tracks != nullptr) {
        for (size_t i = 0; i < 4; ++i) {
            if (e.tracksAsk[i]) tracks->askDefinition(i, e.tracksAsked[i]);
        }
        tracks->advance();
        tracks->setOrigin(s.car.jolt.origin);
    }
    for (size_t i = 0; i < 4; ++i) {
        oj::scene::Emitter* skid = nullptr;
        if (e.skid[i] != nullptr) skid = e.skid[i] == e.sandSkid[i] ? p.sandSkid[i].get() : p.tarmacSkid[i].get();
        if (skid != p.skid[i]) {
            if (p.skid[i] != nullptr) p.skid[i]->setLevel(0.0f);
            p.skid[i] = skid;
        }
        const WheelPose& w = s.wheels[i];
        if (!e.wheelOn[i]) {
            if (skid != nullptr) skid->setLevel(0.0f);
            if (tracks != nullptr && e.trackOff[i]) {
                tracks->mark(i, nullptr, 0.0f, 0.0f);
                tracks->setLevel(0.0f);
            }
            continue;
        }
        const oj::Vec4 at{w.base[0], w.base[1] - 25.0f, w.base[2], w.base[3]};
        if (skid != nullptr) {
            skid->local = at;
            skid->setLevel(e.skidLevel[i]);
            skid->origin = s.car.jolt.origin;
            skid->velocity = s.car.jolt.velocity;
        }
        // The suspension's crunch: with the car's sounds turned to the camera (+0x498's +0x2C, an int) and the
        // compression changed by 0.085 or more ([0x007254F0]), the wheel's instance not sounding takes
        // suspcrunch1 (round(0.04 x 3) is always 0) at the flag's bits as its volume (1.4e-45: not heard, stock),
        // rolloff 0.1, no random frequency ([0x0079E7DC] 0), from 350, played.
        const int32_t focus = s.sound && s.sound->focus() ? 1 : 0;
        const oj::WheelLook& look = s.car.wheels[i];
        if (focus != 0 && std::fabs(look.lastCompression - look.compression) >= 0.085f) {
            oj::audio::Instance* crunch = s.wheelSounds[i];
            if (crunch != nullptr && !crunch->sounding()) {
                crunch->setSet(suspensionCrunch[0].bank, suspensionCrunch[0].set);
                crunch->setVolume(std::bit_cast<float>(focus));
                crunch->setRolloff(0.1f);  // [0x007254E8]
                crunch->randomFrequency(0, oj::gameRandom());
                crunch->setMinDistance(350.0f);  // [0x00717874]
                crunch->play();
            }
        }
        if (oj::scene::Emitter* spray = p.spray[i].get(); spray != nullptr) {
            spray->local = at;
            spray->setLevel(e.sprayLevel[i]);
            spray->origin = at;
            spray->velocity = w.moved;
        }
    }
    if (tracks != nullptr) {
        for (size_t i = 0; i < 4; ++i) {
            if (!e.trackOn[i]) continue;
            tracks->mark(i, &s.car.wheels[i].point, e.trackLevels[i][0], e.trackLevels[i][1]);
        }
    }
    squealSound(s);
}

}  // namespace app
