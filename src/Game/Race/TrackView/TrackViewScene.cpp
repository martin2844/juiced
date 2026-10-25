// The race's scene each frame: the lights on the cars, the textures laid on the route, the wheels, the atmosphere's
// zones, the exhausts and the weather.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/X87.h"

#include <cmath>
#include <iterator>
#include <limits>

namespace app {

void TrackView::makeLightSteps(RaceSlot& s) {
    using oj::scene::LightStep;
    s.lights.clear();
    // Room for every step (the parts keep the chassis's place: no reallocation), the fixed build's four more.
    s.lights.reserve(20);
    if (s.chassis == nullptr) return;
    const oj::render::AtmosphereSet* set = scene->atmosphere();
    const bool night = set != nullptr && set->current != nullptr && set->current->night != 0;
    const oj::GameFiles files(std::filesystem::current_path());
    auto add = [&](LightStep::Kind kind, oj::render::Node* node, size_t table) {
        s.lights.emplace_back(kind, node);
        LightStep& l = s.lights.back();
        if (kind != LightStep::Kind::Eased) {
            const std::string* name = s.model->nameOf(node);
            l.part = oj::CarPartObject::make(name != nullptr ? *name : std::string());
            l.parent = node->parent();
            if (auto* mesh = dynamic_cast<oj::render::ModelNode*>(node); mesh != nullptr) mesh->flags &= ~0x10u;
            if (l.part.axes != 0) {
                l.physics = scene->sceneObjects().makePartPhysics(l.part.files, node->world(), &files);
            }
        }
        if (kind == LightStep::Kind::Part) {
            l.zone = table < s.car.zones.size() ? s.car.zones[table] : nullptr;
            l.chassis = &s.lights.front();
            l.modelParts = &s.car.modelParts;
        }
        l.start(scene->sunMap(), scene->sceneObjects(), night);
    };
    add(LightStep::Kind::Chassis, s.chassis, 0);
    for (size_t i = 0; i < 4; ++i) {
        if (s.wheels[i].node != nullptr) add(LightStep::Kind::Eased, s.wheels[i].node, 0);
    }
    static const char* const kTable[] = {"FBUMP",    "RBUMP",     "BONNET",    "SPOILER",   "SSKIRT_L",
                                         "SSKIRT_R", "EXHAUST_C", "EXHAUST_S", "EXHAUST_O", "DECAL"};
    for (size_t i = 0; i < std::size(kTable); ++i) {
        oj::render::ModelNode* node = s.model->meshNode(kTable[i]);
        if (node != nullptr) add(LightStep::Kind::Part, node, i);
    }
    // Stock problem: the wing mirrors and the lamps (WMIR_L, WMIR_R, HEADLIGHTS, REARLIGHTS; car parts by the scene
    // objects' names too) get no step, so their nodes keep the full light in the shade while the body darkens. The
    // fixed build gives them the chassis's light as the table's parts (no zone, no wobble).
    for (const char* name : {"WMIR_L", "WMIR_R", "HEADLIGHTS", "REARLIGHTS"}) {
        oj::render::ModelNode* node = s.model->meshNode(name);
        if (node != nullptr) add(LightStep::Kind::Part, node, SIZE_MAX);
    }
}

bool TrackView::chassisBox(const RaceSlot& s, oj::Vec4& low, oj::Vec4& high) {
    const auto* node = dynamic_cast<const oj::render::ModelNode*>(s.chassis);
    if (node == nullptr || node->mesh == nullptr) return false;
    const auto& a = node->mesh->boundsMin();
    const auto& b = node->mesh->boundsMax();
    low = {a[0], a[1], a[2], 0.0f};
    high = {b[0], b[1], b[2], 0.0f};
    return true;
}

void TrackView::makeProjectors(RaceSlot& s, const oj::Car& car) {
    // 0x004861CC: the silhouette shadows before the projectors: by day (the atmosphere's +0x170 and +0x184 0, or
    // none) the sun's, else those of the first and the second light nearest the car.
    if (scene != nullptr && s.chassis != nullptr) {
        const oj::render::AtmosphereSet* set = scene->atmosphere();
        const bool day = set == nullptr || set->current == nullptr ||
                         (set->current->night == 0 && set->current->worldShadowAdditive == 0);
        RaceSlot* slot = &s;
        auto drawCar = [slot](oj::render::Renderer3d& r, oj::render::Camera& c) {
            if (slot->model) slot->model->drawBelow(slot->chassis, r, c, 0);
        };
        if (day) {
            s.silhouettes[0] = scene->addSilhouette(*s.chassis, 0, drawCar);
        } else {
            s.silhouettes[0] = scene->addSilhouette(*s.chassis, 1, drawCar);
            s.silhouettes[1] = scene->addSilhouette(*s.chassis, 2, drawCar);
        }
    }
    oj::Vec4 low{}, high{};
    if (scene == nullptr || !chassisBox(s, low, high)) return;
    oj::scene::Engine& engine = scene->engine();
    // The race's cars load with the cars' folders in use (0x00495DE0): the textures are cartextures'.
    const oj::scene::CarFolders folders(engine);
    const oj::render::GameTexture* shadow = engine.texture("gen_shadow");
    const oj::render::GameTexture* neon =
        static_cast<int8_t>(car.body[9]) > -1 ? engine.texture("gen_neon") : nullptr;
    const oj::Vec4 shadowLow{low[0] * 1.25f, low[1], low[2] * 1.1f, low[3]};
    const oj::Vec4 shadowHigh{high[0] * 1.25f, high[1], high[2] * 1.1f, high[3]};
    s.shadow = scene->addProjector(*s.chassis, shadowLow, shadowHigh, shadow, neon);
    if (neon != nullptr) s.shadow->setColour(car.neon);
    const oj::render::AtmosphereSet* set = scene->atmosphere();
    if (set == nullptr || set->current == nullptr || set->current->night == 0) return;
    const oj::render::GameTexture* beam = engine.texture("headlight_b");
    const oj::Vec4 beamLow{low[0] * 7.0f, low[1], high[2] * 0.8f, low[3]};
    const oj::Vec4 beamHigh{high[0] * 7.0f, high[1], (high[2] - low[2]) * 3.0f + high[2], high[3]};
    s.headBeam = scene->addProjector(*s.chassis, beamLow, beamHigh, nullptr, beam);
    s.headBeam->setTexture(true, beam, false, true);
    s.headBeam->setColour(0xFF62607Cu);
}

void TrackView::projectorsFrame(RaceSlot& s, oj::CrtRandom& random) {
    oj::RaceCar& car = s.car;
    const oj::Car& record = car.car != nullptr ? *car.car : s.record;
    uint32_t colour = 0;
    if (static_cast<int8_t>(record.body[9]) > -1) {
        const float sun = s.lights.empty() ? 1.0f : s.lights.front().light;
        const float fade = sun >= 0.7f ? 1.0f : sun / 0.7f;
        const oj::Mat34 turn = oj::rotationOf(s.body->orientation);
        float tilt = (1.0f - turn[5]) * 4.0f;
        if (tilt < 0.0f) {
            tilt = 0.0f;
        } else if (tilt > 1.0f) {
            tilt = 1.0f;
        }
        colour = record.neon;
        auto times = [&colour](float f) {
            // Each channel to the nearest whole number (0x005CA370).
            for (uint32_t shift : {16u, 8u, 0u}) {
                const auto byte =
                    static_cast<uint32_t>(oj::x87::nearest(static_cast<float>((colour >> shift) & 0xFF) * f));
                colour = (colour & ~(0xFFu << shift)) | ((byte & 0xFF) << shift);
            }
        };
        const oj::render::AtmosphereSet* set = scene->atmosphere();
        if (set != nullptr && set->current != nullptr) {
            if (set->current->night == 0) times(0.4f - fade * 0.1f);
            colour = (colour & 0x00FFFFFFu) | 0x80000000u;
        }
        times(1.0f - tilt);
        if (oj::neonFlicker(car, random)) colour = 0;
    }
    const oj::Cld* cld = scene->collision();
    if (s.shadow != nullptr) {
        s.shadow->setColour(colour);
        // The projectors' [1] takes the fade below (the shadow's) or 1, a value they do not read.
        if (cld != nullptr) s.shadow->update(*cld);
    }
    if (s.headBeam != nullptr && cld != nullptr) s.headBeam->update(*cld);
    // The silhouettes' [1]: the first the chassis's light over 0.7 (1 from 0.7 on, 1 without lights), the second 1.
    if (cld == nullptr) return;
    const float sun = s.lights.empty() ? 1.0f : s.lights.front().light;
    const float fade = sun >= 0.7f ? 1.0f : sun / 0.7f;
    const oj::render::AtmosphereSet* set = scene->atmosphere();
    if (s.silhouettes[0] != nullptr) s.silhouettes[0]->update(fade, set, nearestLights, *cld);
    if (s.silhouettes[1] != nullptr) s.silhouettes[1]->update(1.0f, set, nearestLights, *cld);
}

void TrackView::wheelsFrame(RaceSlot& s, float dt) {
    const oj::Car& record = s.car.car != nullptr ? *s.car.car : s.record;
    constexpr bool onStand = false;  // the car's +0x19C (car_rolling's dyno)
    std::array<float, 4> speeds{};
    oj::wheelsFrame(s.wheels, *s.body, *s.model, record.steeringPulls != 0, s.car.pullSide, onStand, dt, &speeds);
    for (size_t i = 0; i < 4; ++i) {
        if (s.wheels[i].node != nullptr) s.car.wheels[i].poseSpeed = speeds[i];
    }
}

void TrackView::atmosZonesFrame(RacePlayer& player) {
    oj::render::AtmosphereSet* set = scene != nullptr ? scene->atmosphere() : nullptr;
    if (set == nullptr || set->list.empty()) return;
    const std::vector<oj::SceneAtmosZone>& zones = scene->sceneObjects().atmosZones();
    if (zones.empty()) return;
    const oj::Mat34& w = player.camera.world();
    const oj::Vec4 at{w[3], w[7], w[11], 0.0f};
    const oj::SceneAtmosZone* nearest = nullptr;
    float best = std::numeric_limits<float>::max();
    for (const oj::SceneAtmosZone& z : zones) {
        const float d = oj::zoneDistance(z, at);
        if (d < best) {
            best = d;
            nearest = &z;
        }
    }
    if (nearest == nullptr) return;
    const float d = oj::zoneDistance(*nearest, at);
    if (!(d < nearest->radius)) {
        // The fixed build: out of every zone no blend (the atmosphere in use alone).
        set->blend = 0.0f;
        return;
    }
    const float inner = nearest->radius * 0.75f;
    float blend = 1.0f;
    if (!(d <= inner)) {
        blend = static_cast<float>(1.0 - (static_cast<double>(d) - inner) /
                                             (static_cast<double>(nearest->radius) - inner));
    }
    set->current = &set->list.front();
    set->next = nearest->atmosphere;
    set->blend = blend;
}

void TrackView::exhaustsFrame(RaceSlot& s) {
    float lift = -(s.body->localAcceleration[1] * 0.2f);
    if (lift < -6.0f) {
        lift = -6.0f;
    } else if (lift > 6.0f) {
        lift = 6.0f;
    }
    for (size_t i = 0; i < 2; ++i) {
        oj::render::Node* n = s.exhaustNodes[i];
        if (n == nullptr) continue;
        const oj::Vec4& at = s.exhaustPlaces[i];
        n->local[3] = at[0];
        n->local[7] = at[1] + lift;
        n->local[11] = at[2];
        n->invalidate();
    }
}

void TrackView::carLights(RaceSlot& s) {
    if (s.brakeLight != nullptr && !s.car.rearLightsOut) {
        const bool braking = s.body->brake != 0.0f;
        if (braking) {
            s.brakeLight->flags &= ~4u;
        } else {
            s.brakeLight->flags |= 4u;
        }
        for (oj::scene::CarFlare* f : s.brakeFlares) {
            if (f != nullptr) f->on = braking;
        }
    }
    if (s.reverseLight != nullptr && !s.car.rearPartOut) {
        if (s.body->gear == 0) {
            s.reverseLight->flags &= ~4u;
        } else {
            s.reverseLight->flags |= 4u;
        }
    }
}

void TrackView::dropCameraWeather(RacePlayer& player) {
    player.cameraRain.reset();
    player.cameraSplash.reset();
    player.cameraBlobs.reset();
}

void TrackView::makeCameraWeather(RacePlayer& player) {
    dropCameraWeather(player);
    player.cameraInTunnel = false;
    const oj::render::AtmosphereSet* set = scene->atmosphere();
    const oj::Atmosphere* a = set != nullptr ? set->current : nullptr;
    player.cameraWet = a != nullptr && a->wet != 0;
    // 0x00401A4C: in the wet the camera's rain sound (volume group 0, at the listener), looping, rolloff 0.0001,
    // played.
    player.rainSound = nullptr;
    player.rainTarget = 0.0f;
    player.rainLevel = 0.0f;
    if (player.cameraWet && sound3d) {
        player.rainSound = sound3d->createGrouped("rain", 4);
        if (player.rainSound != nullptr) {
            player.rainSound->setLooped(1);
            player.rainSound->setRolloff(0.0001f);
            player.rainSound->play();
        }
    }
    player.cameraLightBlur = a != nullptr ? a->lightBlur : 0.5f;
    oj::scene::ParticleSystem* system = scene->particles();
    if (system == nullptr) return;
    const bool night = a != nullptr && a->night != 0;
    auto follow = [&](const char* name, int32_t value) {
        std::shared_ptr<oj::scene::Emitter> e = system->make(name, &loading);
        // 0x0050CE70: an effect whose sound loops draws its set as it is made (rain1's rain).
        if (e && sound && e->def.hasSound && e->def.soundTrigger == 1 && sound3d) {
            e->makeSound(*sound3d, loading.next());
        }
        auto* c = dynamic_cast<oj::scene::CameraEmitter*>(e.get());
        if (c != nullptr) c->attach(&player.camera, value, night);
        return e;
    };
    if (player.cameraWet) {
        player.cameraRain = follow("rain1", 0);
        player.cameraSplash = follow("rainsplash1", 1);
        player.cameraBlobs = system->make("rainblob1", &loading);
    } else {
        player.cameraRain = follow("ground_rush_street", 0);
        player.cameraSplash = follow("ground_rush_scrub", 1);
    }
}

void TrackView::lightBlurFrame(RacePlayer& player) {
    oj::render::AtmosphereSet* set = scene != nullptr ? scene->atmosphere() : nullptr;
    if (set == nullptr || set->current == nullptr) return;
    const RaceSlot* followed = followedSlot(player);
    if (followed == nullptr) return;
    const oj::RaceCar& car = followed->car;
    set->current->lightBlur = car.nitroAsked ? car.nitroLook * 0.7f : player.cameraLightBlur;
}

void TrackView::weatherFrame(RacePlayer& player) {
    if (scene == nullptr) return;
    const oj::Mat34& w = player.camera.world();
    player.cameraInTunnel = scene->sceneObjects().inTunnel({w[3], w[7], w[11], 0.0f});
    weatherEmitters(player);
    // 0x0040686C: the rain's sound follows its target by 0.2 of the way, in [0, 1].
    if (player.rainSound != nullptr) {
        if (player.rainTarget != player.rainLevel) {
            player.rainLevel = player.rainLevel - (player.rainLevel - player.rainTarget) * 0.2f;
        }
        player.rainLevel = player.rainLevel < 0.0f ? 0.0f : (1.0f < player.rainLevel ? 1.0f : player.rainLevel);
        player.rainSound->setVolume(player.rainLevel);
    }
}

void TrackView::weatherEmitters(RacePlayer& player) {
    player.rainTarget = 0.0f;
    if (!player.cameraWet) return;
    oj::render::AtmosphereSet* set = scene->atmosphere();
    if (set == nullptr || set->current == nullptr) return;
    const oj::Atmosphere& a = *set->current;
    auto amount = [](const std::shared_ptr<oj::scene::Emitter>& e, float v) {
        if (auto* c = dynamic_cast<oj::scene::CameraEmitter*>(e.get()); c != nullptr) c->setAmount(v);
    };
    auto* blobs = dynamic_cast<oj::scene::SmearEmitter*>(player.cameraBlobs.get());
    if (player.cameraInTunnel) {
        amount(player.cameraRain, 0.0f);
        amount(player.cameraSplash, 0.0f);
        if (blobs != nullptr) blobs->setAmount(0.0f);
        return;
    }
    amount(player.cameraRain, a.rain);
    // The rain's sound by the camera's height over the ground: (1 - clamp(|y - ground| x 0.001, 0, 1)) x the rain.
    if (player.cameraRain) {
        float h = std::fabs(player.camera.world()[7] - player.raceCamera.ground()) * 0.001f;
        h = h < 0.0f ? 0.0f : (1.0f < h ? 1.0f : h);
        player.rainTarget = (1.0f - h) * a.rain;
    }
    amount(player.cameraSplash, a.rain);
    if (blobs != nullptr) {
        float shown = a.rain * 0.2f;
        const RaceSlot* followed = followedSlot(player);
        if (followed != nullptr && followed->car.followed != nullptr) {
            const oj::RaceCar& car = followed->car;
            shown = (((1.0f - car.followGap) * car.followed->effects.smoke) * a.roadWetness) * 0.8f + shown;
        }
        blobs->setAmount(shown);
    }
}

}  // namespace app
