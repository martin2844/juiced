// The race's cars as the race manager drives them (the vehicle's frame 0x00488EE0 in part, the vehicles' manager
// 0x00497C50): docs/spec/18-racers.md, "The cars".

#include "Core/Cars/Car.h"
#include "Core/Physics/CarBody.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Career/Career.h"
#include "Core/Formats/Cld.h"
#include "Core/Physics/Contact.h"
#include "Core/Race/RaceSession.h"
#include "Core/Race/Racer.h"
#include "Core/Race/RaceSounds.h"
#include "Core/Tracks/SceneObjects.h"
#include "Core/Base/X87.h"

#include "Core/Tracks/Route.h"
#include <bit>
#include <cfloat>
#include <cmath>
#include <limits>

namespace oj {

namespace {

const float kRandomScale = std::bit_cast<float>(0x38000100u);  // 1 / 32767 (0x006FF7EC)

float clampTo(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// The pull of a rough surface by the zone's type (0x00725420): sand -20, grass -5.
float surfacePull(int32_t surface) {
    static constexpr float kPull[] = {0.0f, -20.0f, -5.0f, 0.0f, -10.0f, -5.0f, 0.0f, 0.0f};
    return surface >= 0 && surface < 8 ? kPull[surface] : 0.0f;
}

// 0x00407B20: the model's origin in centimetres (w as it is).
Vec4 originCm(const CarBody& body) {
    const Vec4 o = body.origin();
    return {o[0] * 100.0f, o[1] * 100.0f, o[2] * 100.0f, o[3]};
}

// 0x00498100: the race's car whose body is this one, none for another body (a scene object).
RaceCar* carOfBody(RaceState& race, const RigidBody* body) {
    if (body == nullptr) return nullptr;
    for (std::unique_ptr<Competitor>& c : race.competitors()) {
        if (c->car != nullptr && c->car->body == body) return c->car;
    }
    return nullptr;
}

// 0x004BAD40: a hit of the player's crew counted for the career (a career race only; a hit counts for the car `hit`
// of the player's crew): against no car, the racer's crash (2 over 0.5 of damage, else 1); against a car at its
// place or ahead going on at more than 5 m/s, the racer's ram and that car's crew's rams (10 or 2).
void countHit(RaceState& race, const RaceCar* other, const RaceCar& hit, float amount) {
    const RaceRules& rules = race.rules();
    if (rules.mode != RaceMode::Career || hit.crew != 0 || rules.session == nullptr) return;
    if (hit.record < 0 || static_cast<size_t>(hit.record) >= rules.session->racers.size()) return;
    Racer& racer = rules.session->racers[static_cast<size_t>(hit.record)];
    if (other == nullptr) {
        racer.crashed = amount > 0.5f ? 2 : 1;
        return;
    }
    if (other->competitor->place > hit.competitor->place) return;
    if (!(other->body->localVelocity[2] > 5.0f)) return;
    if (rules.career == nullptr || other->crew < 0 || static_cast<size_t>(other->crew) >= rules.career->crews.size()) {
        return;
    }
    Crew& crew = rules.career->crews[static_cast<size_t>(other->crew)];
    if (amount > 0.5f) {
        crew.rams = crew.rams + 10.0f;
        racer.rammed = 2;
    } else {
        crew.rams = crew.rams + 2.0f;
        racer.rammed = 1;
    }
}

// 0x004916F0 without the sounds: the side of the body's rigid contacts with the route or another body (their mean
// arm ahead of the centre or not), a contact ahead noted for the competitor, the lowest point touched; then the energy
// (twice the velocity's change, at most 100000, four times against a body) and, from 50 on, the hit: the emitters'
// levels (the explosion's 0.0004 x the energy, the scrape's 0.04 x the speed against the other side, kept while it is
// higher), three times upside down, over 400 (once the race is 0.25 s old) a contact drawn and the damage there.
static void impactContacts(RaceCar& car, ContactPools& pools, RaceState& race, CrtRandom& random) {
    CarEffects& e = car.effects;
    e.impactRan = true;
    e.impactHit = false;
    CarBody& body = *car.body;
    const size_t count = body.rigidCount;
    car.bodyContact = false;
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    uint32_t touching = 0;
    // Stock problem: the other body is read from the step's last rigid contact, touching or not (one that came apart
    // in the step counted as a body's, one beside it lost a body's). The fixed build reads the last touching contact
    // (the bodies' contacts come after the route's).
    const Contact* last = nullptr;
    for (size_t k = 0; k < count; ++k) {
        const Contact& c = pools.rigid[body.rigidFirst + k];
        if (c.compression > 0.0f) {
            ++touching;
            sx = c.arm[0] * 100.0f + sx;
            sy = c.arm[1] * 100.0f + sy;
            sz = c.arm[2] * 100.0f + sz;
            last = &c;
        }
        const float y = c.point[1] * 100.0f;
        if (y < car.lowest) car.lowest = y;
    }
    auto none = [&car] {
        car.contactSide = 0;
        car.impact = 0.0f;
        car.effects.impactScrape = car.effects.sparks;
    };
    if (touching == 0) {
        none();
        return;
    }
    const float share = 1.0f / static_cast<float>(touching);
    if (sz * share <= 0.0f) {
        car.contactSide = 2;
    } else {
        car.contactSide = 1;
        if (car.competitor != nullptr) car.competitor->frontContact = true;
    }
    float energy = 2.0f * car.jolt.change;
    if (energy > 100000.0f) energy = 100000.0f;
    // The speed against the other side (metres a second): the velocity of the frame before less the other body's.
    float rx = car.jolt.lastVelocity[0] * 0.01f;
    float ry = car.jolt.lastVelocity[1] * 0.01f;
    float rz = car.jolt.lastVelocity[2] * 0.01f;
    if (last->other != nullptr) {
        car.bodyContact = true;
        rx = rx - last->other->velocity[0];
        ry = ry - last->other->velocity[1];
        rz = rz - last->other->velocity[2];
        energy = 4.0f * energy;
    }
    const float against = std::sqrt((rx * rx + ry * ry) + rz * rz);
    if (energy < 50.0f) {
        none();
        return;
    }
    e.impactHit = true;
    e.impactSpeed = against;
    e.impactArm = {sx * share, sy * share, sz * share, 0.0f};
    e.impactLevel = clampTo(0.0004f * energy, 0.0f, 1.0f);
    const float scrape = clampTo(0.04f * against, 0.0f, 1.0f);
    if (e.sparks < scrape) e.sparks = scrape;
    e.impactScrape = e.sparks;
    if (rotationOf(body.orientation)[5] < 0.0f) energy = energy * 3.0f;
    if (!(energy > 400.0f)) return;
    Competitor& competitor = *car.competitor;
    if (competitor.finished) return;
    if (!(competitor.raceTime > 0.25f) && !race.rules().replayRace) return;
    RaceCar* other = carOfBody(race, last->other);
    const size_t drawn = static_cast<uint32_t>(random.next()) % count;
    const Contact& at = pools.rigid[body.rigidFirst + drawn];
    const Vec4 point{at.arm[0] * 100.0f, at.arm[1] * 100.0f, at.arm[2] * 100.0f, at.arm[3]};
    const Vec4 normal = at.localNormal;
    Vec4 direction = point;
    normalize3(direction);
    const float over = (energy - 400.0f) * 0.003f;
    float lift = normal[2] + 1.0f;
    if (lift < 1.0f) {
        lift = 1.0f;
    } else if (lift > 2.0f) {
        lift = 2.0f;  // Stock problem: past 2 the excess took the place of the bound; the fixed build's 2.
    }
    float amount = lift + over;
    if (race.rules().replayRace) {
        // A replay's race: 2 (1 while the replay runs fast, its +0x7C).
        amount = amount * 2.0f;
    } else if (car.kind == CarKind::Ai) {
        amount = amount * 1.5f;
    }
    countHit(race, other, car, amount);
    if (other != nullptr) countHit(race, &car, *other, amount);
    // Stock problem: the car's impact (+0x174, the camera's shake, the rumble, the wheel's jolt x 255, the wheels'
    // smoke) was set after the pieces, always the last one's 50. The fixed build's is the hit's amount, at most the
    // stock's 50 (a light hit a lighter jolt; the smoke, the rumble and the shake's size are full from 1 on, the
    // shake's period from 2.5).
    const float hit = amount < 50.0f ? amount : 50.0f;
    // Pieces of 50: the first takes what is left over them (all of it below 50).
    uint32_t pieces = static_cast<uint32_t>(x87::chop(amount * 0.02f));
    if (pieces < 1) {
        pieces = 1;
    } else {
        amount = amount - x87::asUnsigned(pieces) * 50.0f;
    }
    for (uint32_t k = 0; k < pieces; ++k) {
        const float x = (x87::unit(random.next()) - 0.5f) * 50.0f + point[0];
        const float y = (x87::unit(random.next()) - 0.5f) * 50.0f + point[1];
        const float z = (x87::unit(random.next()) - 0.5f) * 50.0f + point[2];
        // The dent along the contact's normal; the other car's at minus the point, along minus the normal.
        if (car.damage != nullptr) car.damage->hit({x, y, z, 0.0f}, normal, amount);
        if (other != nullptr && other->damage != nullptr) {
            other->damage->hit({-x, -y, -z, 0.0f}, {-normal[0], -normal[1], -normal[2], 0.0f}, amount);
        }
        amount = 50.0f;
    }
    car.impact = hit;
    car.hitPoint = direction;
    if (race.rules().type == RaceType::PerfectLap) {
        // A time trial ends at the first hit.
        race.rules().challenge = ChallengeOutcome::CarDamaged;
        race.finish(competitor);
    }
}

// 0x004907D0 without the sounds and the tyres' wear (0x0048EF70): a wheel whose surface changed takes the skid and the
// squeal of its surface (sand and grass those of sand, the rest those of tarmac); its spray falls by 0.05 a frame, and
// on dry sand it is the wheel's |spin| x 0.01 + 0.0008 x its node's speed within 0 .. 1. Then each wheel down in the
// race (and no replay going fast forward, [G+0x9C]+0x7C): its level (the car's impact + the tyre's smoke + its skid) x
// the effects' scale, or in the wet ((the puddles' map at its touch) + 0.05) x 0.04 x the road's wetness x the body's
// speed (over 0.01 its spray is 0.3 more), within 0 .. 1 (a quarter of it adds to +0xEC); a wheel up leaves the
// squeal's pitch to be drawn again: with a squeal and no fast forward, a pitch of 0 is drawn anew, (r / 32767 - 0.5) x
// 8000.
// 0x0048EF70 (the car not on the stand, +0x19C): the tyres' wear (the car's +0x74) grows by 0.0004 x the frame's
// time, within 0 .. 1 (0x0048F170), and every tyre's LateralMu is the car's own (+0x78) x (1 - 0.1 x the wear).
void wearTyres(RaceCar& car, float frameTime) {
    if (car.car == nullptr || car.onStand) return;
    Car& record = *car.car;
    const float wear = clampTo(frameTime * 0.0004f + std::bit_cast<float>(record.tyreWear), 0.0f, 1.0f);
    record.tyreWear = std::bit_cast<uint32_t>(wear);
    car.body->setTyreGrip((1.0f - wear * 0.1f) * std::bit_cast<float>(record.grip));
}

void tyreEffects(RaceCar& car, CrtRandom& random, float frameTime) {
    CarEffects& e = car.effects;
    CarBody& body = *car.body;
    e.tracksAsk.fill(false);
    for (size_t i = 0; i < 4; ++i) {
        WheelLook& w = car.wheels[i];
        int32_t surface = w.surface;
        if (w.surfaceChanged && surface >= 0) {
            w.surfaceChanged = false;
            if (e.wet && surface >= 0 && surface <= 2) surface = surface + 3;
            // The track of the wheel asks for the TRACKS of the surface's block.
            if (e.tracks != nullptr && surface < 10) {
                e.tracksAsked[i] = e.surfaceTracks[static_cast<size_t>(surface)];
                e.tracksAsk[i] = true;
            }
            const bool rough = w.surface == 1 || w.surface == 2;
            e.skid[i] = rough ? e.sandSkid[i] : e.tarmacSkid[i];
            e.squeal = rough ? e.sandSqueal : e.tarmacSqueal;
        }
        e.sprayLevel[i] = e.sprayLevel[i] - 0.05f;
        if (e.sprayLevel[i] < 0.0f) e.sprayLevel[i] = 0.0f;
        if (!e.wet && surface == 1) {
            const CarWheel* wheel = body.wheelOf(static_cast<int32_t>(i));
            const float spin = wheel != nullptr ? std::fabs(wheel->spin) : 0.0f;
            e.sprayLevel[i] = clampTo(0.0008f * w.poseSpeed + spin * 0.01f, 0.0f, 1.0f);
        }
    }
    const Vec4& v = body.velocity;
    const float speed = std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]);
    e.smoke = 0.0f;
    for (size_t i = 0; i < 4; ++i) {
        const WheelLook& w = car.wheels[i];
        e.wheelOn[i] = w.down && !e.fastForward;
        if (!e.wheelOn[i]) {
            e.squealPitch = 0.0f;
            e.skidLevel[i] = 0.0f;
            continue;
        }
        const float made = (car.impact + w.smoke + w.skid) * e.scale;
        float level = made;
        // The wear follows the level as made, dry or wet.
        const bool wears = made > 0.05f;
        if (e.wet) {
            float puddle = 0.05f;
            if (e.puddles != nullptr) puddle = e.puddles->sample(w.point) + 0.05f;
            level = puddle * 0.04f * e.roadWetness * speed;
            if (level > 0.01f) e.sprayLevel[i] = 0.3f + level;
        }
        level = clampTo(level, 0.0f, 1.0f);
        e.skidLevel[i] = level;
        e.smoke = level * 0.25f + e.smoke;
        if (wears) wearTyres(car, frameTime);
    }
    if (e.squeal != nullptr && !e.fastForward && e.squealPitch == 0.0f) {
        e.squealPitch = (x87::unit(random.next()) - 0.5f) * 8000.0f;
    }
}

// 0x004907D0's tracks part (with the tyre tracks' effect, +0xE8): of each wheel down (and the replay not playing) its
// skid and smoke within 0 .. 1; for each wheel down, with its axle's other wheel up or smoking, the mean of the two
// (the wheel's own then 0.5 more when the other is up), else its own: both over 0.5 with the cruise running keep the
// cruise's wheels-down time at 0.5 s (+0x3C8). The wheel's track is marked with the pair: the right wheels (1, 3) the
// mean then their own, the left wheels their own then the mean; a wheel put off is not in the race.
void cruiseGround(RaceCar& car, Cruise* cruise, bool replay) {
    CarEffects& e = car.effects;
    e.trackOn.fill(false);
    e.trackOff.fill(false);
    if (e.tracks == nullptr) return;
    for (size_t i = 0; i < 4; ++i) e.trackOff[i] = !e.wheelOn[i];
    std::array<float, 4> v{};
    for (size_t i = 0; i < 4; ++i) {
        if (!car.wheels[i].down || replay) continue;
        v[i] = clampTo(car.wheels[i].smoke + car.wheels[i].skid, 0.0f, 1.0f);
    }
    static constexpr std::array<size_t, 4> kOther = {1, 0, 3, 2};
    for (size_t i = 0; i < 4; ++i) {
        if (!car.wheels[i].down) continue;
        const WheelLook& other = car.wheels[kOther[i]];
        float pair = v[i];
        if (!other.down || other.smoke != 0.0f) {
            pair = (v[kOther[i]] + v[i]) * 0.5f;
            if (!other.down) v[i] = v[i] + 0.5f;
        }
        if (0.5f < pair && 0.5f < v[i] && cruise != nullptr && cruise->started != 0) cruise->ground = 0.5f;
        e.trackOn[i] = true;
        e.trackLevels[i] = kOther[i] == 0 || kOther[i] == 2 ? std::array<float, 2>{pair, v[i]}
                                                            : std::array<float, 2>{v[i], pair};
    }
}

// 0x004912D0 without the particles: on nitro the flames follow the engine's speed ((speed - 160) x 0.002 within 0 ..
// 1); at each exhaust, while they are above 0.005, they fade in by 3 a second, a value r is drawn (a rand() over
// 32767) for the flame's mesh, and they fall by 4 a second of the frame. The mesh's scales: across (c1 x r + 0.007) x
// the fade in and (c2 x r + 0.007) x the fade in, along -(r x c0 + the level x 0.01) x the fade in, c (3, 2, 2), in a
// replay's race ([G+0x6C]+0x68) (5, 3, 3).
void flameEffects(RaceCar& car, float dt, float frameTime, CrtRandom& random, bool replayRace) {
    CarEffects& e = car.effects;
    float level = (car.body->engineSpeed - 160.0f) * 0.002f;
    if (level < 0.0f) {
        level = 0.0f;
    } else if (level > 1.0f) {
        level = 1.0f;
    }
    if (car.nitroAsked) e.flameLevel = level;
    for (size_t i = 0; i < 2; ++i) {
        if (!car.exhausts[i]) continue;
        e.flameOn[i] = e.flameLevel > 0.005f;
        e.flameShown[i] = e.flameOn[i];
        if (!e.flameOn[i]) {
            e.flameRamp = 0.0f;
            continue;
        }
        e.flameEmit[i] = e.flameLevel * e.flameRamp;
        const double c0 = replayRace ? 5.0 : 3.0;
        const double c1 = replayRace ? 3.0 : 2.0;
        const double c2 = replayRace ? 3.0 : 2.0;
        e.flameRamp = 3.0f * dt + e.flameRamp;
        if (e.flameRamp > 1.0f) e.flameRamp = 1.0f;
        // The x87 keeps r and each sum before it stores a scale.
        const double r = static_cast<double>(random.next()) * static_cast<double>(kRandomScale);
        const double ramp = e.flameRamp;
        const auto sx = static_cast<float>((c1 * r + static_cast<double>(0.007f)) * ramp);
        const auto sy = static_cast<float>((c2 * r + static_cast<double>(0.007f)) * ramp);
        const auto sz =
            static_cast<float>(-((r * c0 + static_cast<double>(e.flameLevel) * static_cast<double>(0.01f)) * ramp));
        // The turn of a float's pi / 4 (0x006FFDD0) about z.
        const auto c = static_cast<float>(std::cos(0.7853981852531433));
        const auto s = static_cast<float>(std::sin(0.7853981852531433));
        const Mat34 turn{c, -s, 0.0f, 0.0f, s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        const Mat34 scale{sx, 0.0f, 0.0f, 0.0f, 0.0f, sy, 0.0f, 0.0f, 0.0f, 0.0f, sz, -35.0f};
        e.flameMatrix[i] = multiply(turn, scale);
        e.flameLevel = e.flameLevel - 4.0f * frameTime;
    }
}

}  // namespace

void makeEffects(RaceCar& car, const EffectLibrary& library, const SurfaceEffects& surfaces, RaceMode raceMode,
                 RaceType raceType, bool wet, CrtRandom& random, bool sound) {
    CarEffects e;
    e.made = true;
    e.wet = wet;
    e.scale = raceType == RaceType::Cruise ? 1.0f : 0.4f;
    auto surface = [&surfaces](size_t place) -> const SurfaceEffect* {
        return surfaces[place] ? &*surfaces[place] : nullptr;
    };
    auto made = [&](const EffectDef*& slot, const std::string& name) {
        int32_t draw = -1;
        slot = makeEmitter(library, name, random, sound, &draw);
        if (draw != -1) {
            e.soundDraws[static_cast<size_t>(reinterpret_cast<const char*>(&slot) - reinterpret_cast<const char*>(&e))] =
                draw;
        }
    };
    auto named = [&made](const EffectDef*& slot, const SurfaceEffect* s, std::string SurfaceEffect::*field) {
        if (s != nullptr) {
            made(slot, s->*field);
        } else {
            slot = nullptr;
        }
    };
    if (!wet) made(e.tracks, "tracks_tarmac");
    for (size_t k = 0; k < e.surfaceTracks.size(); ++k) {
        if (surfaces[k]) e.surfaceTracks[k] = library.find(surfaces[k]->tracks);
    }
    for (size_t i = 0; i < 2; ++i) {
        if (!car.exhausts[i]) continue;
        made(e.flames[i], "nitro");
        // The flame's mesh node (0x00556AE0 → 0x005550D0): four values for its look, as every mesh node draws.
        for (size_t k = 0; k < 4; ++k) e.flameLook[i][k] = static_cast<float>(random.next()) * kRandomScale;
    }
    made(e.heatHaze, "Heat_Haze");
    made(e.explodeSparks, "explode_sparks");
    made(e.explodeSmoke, "explode_smoke");
    made(e.chassisSmoke, "chassis_smoke");
    named(e.scrapeSparks, surface(0), &SurfaceEffect::sparks);
    named(e.flakes, surface(0), &SurfaceEffect::flakes);
    const SurfaceEffect* tarmac = surface(wet ? 3 : 0);
    const SurfaceEffect* sand = surface(wet ? 4 : 1);
    bool wheels = true;
    for (size_t i = 0; i < 4; ++i) {
        if (raceMode == RaceMode::SplitScreen && i < 2) {
            wheels = false;
        } else if (wheels) {
            named(e.tarmacSkid[i], tarmac, &SurfaceEffect::skid);
            named(e.sandSkid[i], sand, &SurfaceEffect::skid);
            made(e.spray[i], wet ? "wheel_spray" : "wheel_sand");
        }
        car.wheels[i].surfaceChanged = true;
    }
    if (wheels) {
        named(e.tarmacSqueal, tarmac, &SurfaceEffect::wheelspin);
        named(e.sandSqueal, sand, &SurfaceEffect::wheelspin);
    }
    car.effects = e;
}

void startJolt(RaceCar& car) {
    car.jolt = CarJolt{};
    car.jolt.origin = originCm(*car.body);
}

void damageEffects(RaceCar& car, CrtRandom& random) {
    car.steeringFault = false;
    car.turboLost = false;
    car.nitroLost = false;
    car.neonDamaged = false;
    auto level = [&car](size_t zone) { return car.zones[zone]->damage * 0.05f; };
    const bool front = car.zones[0] != nullptr;
    const bool rear = car.zones[1] != nullptr;
    const bool bonnet = car.zones[2] != nullptr;
    if (!car.headlightsOut && front && level(0) > 0.8f) car.headlightsOut = true;
    if (!car.rearLightsOut && rear && level(1) > 0.5f) car.rearLightsOut = true;
    if (!car.rearPartOut && rear && car.rearThreshold < level(1)) car.rearPartOut = true;
    if (car.car == nullptr) return;
    Car& kept = *car.car;
    CarBody& body = *car.body;
    if (kept.steeringPulls == 0) {
        if (front && rear && level(0) > 0.99f && (!bonnet || level(2) > 0.99f) && level(1) > 0.99f) {
            kept.steeringPulls = 1;
            car.steeringFault = true;
            car.pullSide = static_cast<int32_t>(static_cast<uint32_t>(random.next()) & 1u);
        }
    } else {
        // 0x004203E0: the steering held within -1 .. 1.
        body.steering = clampTo(car.pullSide == 0 ? body.steering - 0.05f : body.steering + 0.05f, -1.0f, 1.0f);
    }
    if (car.hasTurbo && kept.turboLost == 0 && front && level(0) > 0.99f && bonnet && level(2) > 0.99f && rear &&
        level(1) > 0.5f) {
        kept.turboLost = 1;
        car.turboLost = true;
        car.hasTurbo = false;
        body.loseTurbo();
    }
    if (car.hasNitro) {
        if (kept.nitroLeaks == 0) {
            if (front && level(0) > 0.99f && body.nitroLeft != 0.0f) {
                kept.nitroLeaks = 1;
                car.nitroLost = true;
            }
        } else if (car.kind == CarKind::Player) {
            body.nitroLeft = body.nitroLeft * 0.995f;
        }
    }
    if (static_cast<int8_t>(kept.body[9]) >= 0) {
        if (kept.neonFlickers == 0) {
            if (front && level(0) > 0.5f) {
                kept.neonFlickers = 1;
                car.neonDamaged = true;
            }
        } else if (front && level(0) > 0.99f) {
            kept.body[9] = 0xFF;
        }
    }
}

bool neonFlicker(RaceCar& car, CrtRandom& random) {
    if (car.car == nullptr || static_cast<int8_t>(car.car->body[9]) < 0 || car.car->neonFlickers != 1) return false;
    return static_cast<uint32_t>(random.next()) % 4 == 0;
}

void tiltGuard(RaceCar& car) {
    CarBody& body = *car.body;
    // Stock problem: the pitch (an arcsine, within ±pi / 2) over pi squared never passes 0.3, so its torque never
    // came, and the square lost its side. The fixed build takes the pitch over its own range, pi / 2 (as the roll
    // over pi, its atan2's), and keeps its side: past 49.3 degrees a torque towards level, growing to the 60 degrees
    // hold below.
    const float pitch = body.angles[0] * 0.63661975f;
    const float roll = body.angles[2] * 0.31830987f;
    float over = pitch * pitch - 0.3f;
    int32_t past = 0;
    if (over < 0.0f) {
        over = 0.0f;
    } else if (over > 0.0f) {
        past = pitch < 0.0f ? -1 : 1;
    }
    body.externalTorque[0] = (over * static_cast<float>(past)) * -40000.0f + body.externalTorque[0];
    body.externalTorque[2] = -40000.0f * roll + body.externalTorque[2];
    constexpr float kLimit = 1.0471975803375244f;
    Vec4 angles = body.angles;
    Vec4 momentum = body.angularMomentum;
    bool changed = false;
    if (body.angles[0] > kLimit) {
        angles[0] = kLimit;
        momentum[0] = 0.0f;
        changed = true;
    }
    if (angles[0] < -kLimit) {
        angles[0] = -kLimit;
        momentum[0] = 0.0f;
        changed = true;
    }
    if (body.angles[2] > kLimit) {
        angles[2] = kLimit;
        momentum[2] = 0.0f;
        changed = true;
    }
    if (angles[2] < -kLimit) {
        angles[2] = -kLimit;
        momentum[2] = 0.0f;
        changed = true;
    }
    if (!changed) return;
    body.setAngles(angles);
    body.setAngularMomentum(momentum);
}

void slipstream(RaceCar& car, const std::vector<RaceCar*>& cars) {
    car.body->dragFactor = 1.0f;
    for (RaceCar* other : cars) {
        if (other == &car) continue;
        const Vec4& v = other->body->velocity;
        const float scale = 600.0f / std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]);
        const Vec4 behind = originCm(*other->body);
        const Vec4 own = originCm(*car.body);
        const float dx = (behind[0] - v[0] * scale) - own[0];
        const float dy = (behind[1] - v[1] * scale) - own[1];
        const float dz = (behind[2] - v[2] * scale) - own[2];
        if (!((dz * dz + dy * dy) + dx * dx < 199809.0f)) continue;
        float cut = ((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]) * 7.0000002e-05f;
        if (cut < 0.0f) {
            cut = 0.0f;
        } else if (cut > 0.5f) {
            cut = 0.5f;
        }
        car.body->dragFactor = 1.0f - cut;
        car.followed = other;
        other->follower = &car;
        car.followGap = std::sqrt((dz * dz + dy * dy) + dx * dx) * 0.0022371365f;
        return;
    }
}

void carFrame(RaceCar& car, float dt, float frameTime, RaceType raceType) {
    CarBody& body = *car.body;
    if (car.hasNitro) {
        body.nitroOn = car.nitroAsked && body.nitroLeft > 0.0f;
        car.nitroLook = car.nitroAsked ? 1.0f : 0.0f;
    }
    car.impact = car.impact - dt * 5.0f;
    if (car.impact < 0.0f) car.impact = 0.0f;
    if (car.car != nullptr) {
        Car& record = *car.car;
        // 0x0048F210: the car keeps the nitro left, within 0 .. 15.
        if (car.hasNitro) record.nitro = std::bit_cast<uint32_t>(clampTo(body.nitroLeft, 0.0f, 15.0f));
        // 0x0048F080 while braking (the car not on the stand, +0x19C, nor in a race of type 9): the brakes' wear (the
        // car's +0x7C) grows by 0.01 x the frame's time within 0 .. 1 (0x0048F1C0), and every wheel's BrakeMu is the
        // car's own (+0x80) / (the wear + 1).
        if (body.brake != 0.0f && !car.onStand && raceType != RaceType::TestTrack) {
            const float wear = clampTo(frameTime * 0.01f + std::bit_cast<float>(record.brakeWear), 0.0f, 1.0f);
            record.brakeWear = std::bit_cast<uint32_t>(wear);
            body.setBrakeGrip(std::bit_cast<float>(record.brake) / (wear + 1.0f));
        }
    }
    if (car.ghostTime > 0.0f) {
        car.ghostTime = car.ghostTime - dt;
        car.ghostTime = clampTo(car.ghostTime, 0.0f, 3.0f);
    }
    bool route = false;
    for (size_t k = 0; k < body.rigidCount && !route; ++k) {
        if (body.pools->rigid[body.rigidFirst + k].other == nullptr) route = true;
    }
    if (!route) return;
    const Vec4& v = body.localVelocity;
    const float length = std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]);
    const float unit = 1.0f / (1.1920929e-7f + length);
    const float ux = v[0] * unit;
    const float uy = v[1] * unit;
    const float uz = v[2] * unit;
    if (!(length > 20.0f)) return;
    float cut = dt * 30.0f;
    if (cut < 0.0f) cut = 0.0f;
    if (cut > length) cut = length;
    const float left = length - cut;
    body.setLocalVelocity({ux * left, uy * left, uz * left, v[3]});
}

// 0x004916F0: the contacts' impact, then the scrape's level falling by 5 a second.
static void impactFrame(RaceCar& car, ContactPools& pools, RaceState& race, CrtRandom& random, float frameTime) {
    impactContacts(car, pools, race, random);
    CarEffects& e = car.effects;
    e.sparks = e.sparks - 5.0f * frameTime;
    if (e.sparks < 0.0f) e.sparks = 0.0f;
}

bool wheelEffects(RaceCar& car, float dt, float frameTime, CrtRandom& random, ContactPools& pools, RaceState* race,
                  bool impacts) {
    bool putBack = false;
    car.effects.impactRan = false;
    CarBody& body = *car.body;
    const Vec4 lv = body.localVelocity;
    const float speed = std::sqrt((lv[0] * lv[0] + lv[2] * lv[2]) + lv[1] * lv[1]);
    // The tyre of each wheel: the first of the step's contacts with its ID.
    std::array<Contact*, 4> tyres{};
    for (size_t k = body.tyreCount; k-- > 0;) {
        Contact& t = body.tyre(k);
        const int32_t wheel = t.def != nullptr ? t.def->id : -1;
        if (wheel >= 0 && wheel < 4) tyres[static_cast<size_t>(wheel)] = &t;
    }
    const int32_t wereDown = car.wheelsDown;
    car.wheelSmoke = 0.0f;
    car.wheelSkid = 0.0f;
    car.wheelSlide = 0.0f;
    car.maxSkid = 0.0f;
    car.maxSmoke = 0.0f;
    car.maxSlide = 0.0f;
    car.wheelsDown = 4;
    float lowest = FLT_MAX;
    car.rumbleShare = 0.0f;
    car.jolted = false;
    for (size_t i = 0; i < 4; ++i) {
        WheelLook& w = car.wheels[i];
        if (w.surface != w.lastSurface) w.surfaceChanged = true;
        w.lastSurface = w.surface;
        Contact* t = tyres[i];
        if (t == nullptr || t->normal[1] < 0.5f) {
            w.down = false;
            w.skid = 0.0f;
            w.smoke = 0.0f;
            w.slide = 0.0f;
            car.wheelsDown = car.wheelsDown - 1;
            continue;
        }
        float threshold = 100.0f;
        if (t->zone != nullptr) {
            const int32_t surface = t->zone->flag;
            w.down = true;
            w.surface = surface;
            w.point = {t->point[0] * 100.0f, t->point[1] * 100.0f, t->point[2] * 100.0f, t->point[3]};
            w.lastCompression = w.compression;
            w.compression = t->compression;
            if (t->zone->rangeMin > -50.0f) w.point[1] = w.point[1] - t->zone->rangeMin;
            if (surface != 0) {
                // A rough surface: a drag growing toward the surface's pull (0.3 of it on the front wheels) and, by
                // chance (the speed / 5 x dt), a jolt of up to ±250 in each direction.
                threshold = 10.0f;
                const float share = clampTo((speed / 5.0f) * dt, 0.0f, 1.0f);
                car.rumbleShare = share;
                const float pull = surfacePull(surface);
                if (pull < 0.0f && pull < car.rumble) car.rumble = car.rumble - 0.1f;
                float g = car.rumble;
                if (i == 0 || i == 1) g = g * 0.3f;
                float fx = (lv[0] * g) * speed;
                float fy = (lv[1] * g) * speed;
                float fz = (lv[2] * g) * speed;
                if (x87::unit(random.next()) < share) {
                    car.jolted = true;
                    random.next();
                    random.next();
                    const float jolt = (x87::unit(random.next()) - 0.5f) * 500.0f;
                    fx = fx + jolt;
                    fy = fy + jolt;
                    fz = fz + jolt;
                }
                const Vec4& p = t->point;
                const float tx = (p[1] * fz - p[2] * fy) * -0.002f;
                const float ty = (p[2] * fx - fz * p[0]) * 0.0002f;
                const float tz = (fy * p[0] - p[1] * fx) * -0.002f;
                body.externalForce[0] = fx + body.externalForce[0];
                body.externalForce[1] = fy + body.externalForce[1];
                body.externalForce[2] = fz + body.externalForce[2];
                body.externalTorque[0] = tx + body.externalTorque[0];
                body.externalTorque[1] = ty + body.externalTorque[1];
                body.externalTorque[2] = tz + body.externalTorque[2];
            } else {
                car.rumble = 0.0f;
            }
        }
        if (w.point[1] < lowest) lowest = w.point[1];
        // The slip of the tyre's patch against the speed of its point: the slide, the skid and the smoke.
        const Vec4& cv = t->velocity;
        const float length = std::sqrt((cv[0] * cv[0] + cv[1] * cv[1]) + cv[2] * cv[2]);
        const float load = t->load;
        const Vec4& s = t->slipVelocity;
        const float root = 1.0f / std::sqrt(length);
        const float a = s[0] * root;
        const float b = s[1] * root;
        const float c = s[2] * root;
        float rubbing = std::sqrt((a * a + c * c) + b * b) * 30.0f;
        if (rubbing < threshold) rubbing = 0.0f;
        const float inverse = 1.0f / length;
        const float a2 = s[0] * inverse;
        const float b2 = s[1] * inverse;
        const float c2 = s[2] * inverse;
        float scrub = std::sqrt((a2 * a2 + c2 * c2) + b2 * b2) * 40.0f;
        if (scrub < 100.0f) scrub = 0.0f;
        float slide = 0.0f;
        if (length > 4.0f) slide = clampTo(std::fabs(a2), 0.0f, 1.0f);
        w.slide = slide;
        car.wheelSlide = slide + car.wheelSlide;
        if (slide > car.maxSlide) car.maxSlide = slide;
        const float off = length - 8.0f;
        const float peak = 10.0f / (off * off + 10.0f);
        float skid = clampTo((peak * ((scrub - 4.0f) / 20000.0f)) * load, 0.0f, 1.0f);
        if (length < 0.15f) skid = 0.0f;
        w.skid = 0.5f * w.skid + skid * 0.5f;
        car.wheelSkid = skid + car.wheelSkid;
        if (skid > car.maxSkid) car.maxSkid = skid;
        float smoke = (((1.0f / 80.0f) * length) * ((rubbing - 2.0f) / 30000.0f)) * load;
        if (length < 1.0f) smoke = 0.0f;
        smoke = clampTo(smoke - w.skid, 0.0f, 1.0f);
        w.smoke = w.smoke * 0.3f + smoke * 0.7f;
        car.wheelSmoke = smoke + car.wheelSmoke;
        if (smoke > car.maxSmoke) car.maxSmoke = smoke;
        car.lowest = lowest;
    }
    car.tipped = car.wheelsDown == 2 && ((!car.wheels[0].down && !car.wheels[2].down) ||
                                         (!car.wheels[1].down && !car.wheels[3].down));
    if (car.wheelsDown < 2) {
        car.upsideTime = dt + car.upsideTime;
    } else {
        car.upsideTime = 0.0f;
    }
    if (car.upsideTime > 4.0f && !car.netPaused && race != nullptr) {
        respawn(car, race->rules(), pools);
        putBack = true;
    }
    car.wheelSmoke = car.wheelSmoke * 0.25f;
    car.wheelSkid = car.wheelSkid * 0.25f;
    car.landed = wereDown < car.wheelsDown;
    car.wheelSlide = car.wheelSlide * 0.25f;

    // 0x004924A0, for a car with its effects object: the origin's velocity over the frame (the move itself when the
    // frame took no time) and the length of its change.
    if (!car.effects.made) return putBack;
    CarJolt& j = car.jolt;
    const Vec4 origin = originCm(body);
    float vx = origin[0] - j.origin[0];
    float vy = origin[1] - j.origin[1];
    float vz = origin[2] - j.origin[2];
    if (dt != 0.0f) {
        const float inverse = 1.0f / dt;
        vx = vx * inverse;
        vy = vy * inverse;
        vz = vz * inverse;
    }
    j.delta = {vx - j.velocity[0], vy - j.velocity[1], vz - j.velocity[2], origin[3]};
    j.change = std::sqrt((j.delta[0] * j.delta[0] + j.delta[1] * j.delta[1]) + j.delta[2] * j.delta[2]);
    j.lastVelocity = j.velocity;
    j.velocity = {vx, vy, vz, origin[3]};
    j.origin = origin;
    // 0x004925F0: a change over 750 with the cruise running and the car's race time over 1 s is a crash of the
    // cruise's (0x004AB7C0).
    Cruise* cruise = race != nullptr && race->rules().career != nullptr ? &race->rules().career->cruise : nullptr;
    if (j.change > 750.0f && cruise != nullptr && cruise->started != 0 && car.competitor != nullptr &&
        car.competitor->raceTime > 1.0f) {
        cruise->crash(j.change, &car, frameTime);
    }
    tyreEffects(car, random, frameTime);
    cruiseGround(car, cruise, race != nullptr && race->rules().replay);
    flameEffects(car, dt, frameTime, random, race != nullptr && race->rules().replayRace);
    if (impacts && race != nullptr) impactFrame(car, pools, *race, random, frameTime);
    return putBack;
}

void respawn(RaceCar& car, const RaceRules& rules, ContactPools& pools) {
    CarBody& body = *car.body;
    const float heading = body.angles[1];
    body.resetForStart();
    car.ghostTime = 3.0f;
    car.upsideTime = 0.0f;
    Competitor& c = *car.competitor;
    const Route& route = *c.route;
    int32_t i = route.before(c.next);
    if (route.pointToPoint && route.lastPoint < i) i = route.count() - 1;
    const RoutePoint& p = route.line(c.line)[static_cast<size_t>(i)];
    body.placeAt({p.x * 0.01f, p.y * 0.01f + 0.3f, p.z * 0.01f, 0.0f});
    float facing = heading;
    if (rules.type != RaceType::Cruise) {
        // 0x0049F3F0: the line's direction as a heading.
        facing = x87::acos(p.direction[2]);
        if (p.direction[0] < 0.0f) facing = -facing;
    }
    body.setAngles({0.0f, facing, 0.0f, 0.0f});
    for (int k = 0; k < 45; ++k) {
        pools.empty();
        body.step(0.05f);
        body.syncNode();
    }
    c.respawnAsked = false;
    placeTracker(car);
    car.node = body.modelMatrix();
}

void placeTracker(RaceCar& car) {
    if (car.centreLine == nullptr) return;
    const Vec4 o = car.body->origin();
    car.lineIndex = static_cast<int32_t>(nearestPathPoint(*car.centreLine, o[0] * 100.0f, o[1] * 100.0f,
                                                          o[2] * 100.0f));
}

void trackLine(RaceCar& car, const Vec4& place) {
    if (car.centreLine == nullptr) return;
    const std::vector<SplinePathPoint>& path = *car.centreLine;
    // Stock problem: a centre line of no points is read out of its list; here the tracker stays.
    if (path.empty()) return;
    const float x = place[0], z = place[2];
    if (car.lineIndex < 0) car.lineIndex = static_cast<int32_t>(nearestPathPoint(path, x, place[1], z));
    const auto count = static_cast<uint32_t>(path.size());
    const auto at = static_cast<uint32_t>(car.lineIndex);
    int32_t before = car.lineIndex - 1;
    if (before < 0) before = static_cast<int32_t>(count) - 1;
    const SplinePathPoint& p = path[at >= count ? count - 1 : at];
    const SplinePathPoint& a = path[static_cast<uint32_t>(before) >= count ? count - 1 : static_cast<uint32_t>(before)];
    const double behind = (static_cast<double>(a.direction[1]) * 0.0 + static_cast<double>(x - a.position[0]) *
                           a.direction[0]) + static_cast<double>(z - a.position[2]) * a.direction[2];
    if (behind < 0.0) car.lineIndex = before;
    const double ahead = (static_cast<double>(x - p.position[0]) * p.direction[0] +
                          static_cast<double>(p.direction[1]) * 0.0) + static_cast<double>(z - p.position[2]) *
                         p.direction[2];
    float share = static_cast<float>((ahead + a.length + a.distance) / path.back().distance);
    if (!(0.0f <= share)) {
        share = 0.0f;
    } else if (share > 1.0f) {
        share = 1.0f;
    }
    car.lineShare = share;
    if (ahead > 0.0) {
        car.lineIndex = car.lineIndex + 1;
        if (static_cast<int32_t>(count) - 1 < car.lineIndex) car.lineIndex = 0;
    }
}

namespace {

// 0x0049F010: the distance from p to the segment a .. b: to a while p lies behind it, to b while past it, else to its
// nearest point.
float segmentDistance(const Vec4& p, const RoutePoint& a, const RoutePoint& b) {
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float vz = b.z - a.z;
    const float wx = p[0] - a.x;
    const float wy = p[1] - a.y;
    const float wz = p[2] - a.z;
    const auto t = static_cast<float>((static_cast<double>(wz) * vz + static_cast<double>(wy) * vy) +
                                      static_cast<double>(wx) * vx);
    double dx = wx;
    double dy = wy;
    double dz = wz;
    if (t > 0.0f) {
        const double vv = (static_cast<double>(vz) * vz + static_cast<double>(vy) * vy) + static_cast<double>(vx) * vx;
        if (vv > t) {
            const Vec4 q = add({a.x, a.y, a.z, 0.0f}, scale({vx, vy, vz, 0.0f}, static_cast<float>(t / vv)));
            dx = static_cast<double>(q[0]) - p[0];
            dy = static_cast<double>(q[1]) - p[1];
            dz = static_cast<double>(q[2]) - p[2];
        } else {
            dx = static_cast<double>(p[0]) - b.x;
            dy = static_cast<double>(p[1]) - b.y;
            dz = static_cast<double>(p[2]) - b.z;
        }
    }
    const auto squared = static_cast<float>((dz * dz + dy * dy) + dx * dx);
    return std::sqrt(squared);
}

}  // namespace

void placeOnLine(Competitor& c, const Vec4& point) {
    if (c.route == nullptr) return;
    const Route& route = *c.route;
    const std::vector<RoutePoint>& l = route.line(c.line);
    if (l.empty()) return;
    const size_t count = route.pointToPoint ? static_cast<size_t>(route.lastPoint) : route.lines[0].size();
    float best = std::numeric_limits<float>::max();
    int32_t nearest = 0;
    for (size_t i = 1; i < count && i < l.size(); ++i) {
        const float d = segmentDistance(point, l[i - 1], l[i]);
        if (d < best) {
            best = d;
            nearest = static_cast<int32_t>(i);
        }
    }
    if (!route.pointToPoint && count >= 1 && count <= l.size()) {
        if (segmentDistance(point, l[count - 1], l[0]) < best) {
            c.next = 0;
            return;
        }
    }
    c.next = nearest;
}

void askNitro(RaceCar& car, bool on, bool replayRace) {
    const bool was = car.nitroAsked;
    car.nitroAsked = on && (car.body->nitroLeft > 0.0f || replayRace);
    if (car.nitroAsked && car.nitroAsked != was) car.nitroFlash = 3;
    if (car.nitroFlash != 0) car.nitroFlash = car.nitroFlash - 1;
    car.nitroStarted = car.nitroAsked != was;
}

void playerNitro(RaceCar& car, bool asked, float count, bool unlimited, bool replayRace) {
    CarBody& body = *car.body;
    const auto gate = [&] {
        const bool on = car.hasNitro && body.throttle > 0.9f && body.brake == 0.0f && body.gear >= 2;
        askNitro(car, on, replayRace);
    };
    if (unlimited) {
        if (count == 0.0f) {
            body.nitroLeft = 15.0f;
            gate();
        }
    } else if (asked && count == 0.0f) {
        gate();
    } else if (car.hasNitro) {
        askNitro(car, false, replayRace);
    }
    if (car.hasNitro && car.nitroAsked && (body.throttle < 0.8f || body.brake > 0.35f)) {
        askNitro(car, false, replayRace);
    }
}

void applyControls(RaceCar& car, RaceState& race, CrtRandom& random, float frameTime) {
    Competitor& c = *car.competitor;
    CarBody& body = *car.body;
    if (!((car.kind == CarKind::Ai && !race.rules().replay) || race.count > 5.0f)) {
        // [0x007689CC], which nothing sets: every other car held on its brake.
        if (kHoldCars) {
            body.throttle = 0.0f;
            body.steering = 0.0f;
            body.brake = 1.0f;
            body.handbrake = 0.0f;
        }
        return;
    }
    if (c.reverseTime <= 0.0f) {
        if (body.gear < 2) body.shift(2);
    } else if (body.gear != 0) {
        body.shift(0);
    }
    body.throttle = c.throttle * c.throttle;
    body.steering = clampTo(c.steering, -1.0f, 1.0f);
    body.brake = c.brake * c.brake * c.brake;
    body.handbrake = c.handbrake;
    // 0x00497E8E: the horn by the AI's wish (its rand() first); a replay's race keeps it quiet.
    const bool horn = !race.rules().replayRace && hornSounds(c, frameTime, random);
    if (RaceSounds* sounds = raceSounds()) sounds->horn(car, horn);
    const bool was = car.nitroAsked;
    if (!c.nitro) {
        car.nitroAsked = false;
        if (car.nitroFlash != 0) car.nitroFlash = car.nitroFlash - 1;
        car.nitroStarted = was;
    } else {
        car.nitroAsked = body.nitroLeft > 0.0f || race.rules().replayRace;
        if (car.nitroAsked && car.nitroAsked != was) car.nitroFlash = 3;
        if (car.nitroFlash != 0) car.nitroFlash = car.nitroFlash - 1;
        car.nitroStarted = car.nitroAsked != was;
    }
}

}  // namespace oj
