#include "Core/Physics/CarBody.h"

#include "Core/Cars/CarPhysics.h"
#include "Core/Physics/PhysicsWorld.h"

#include <cmath>
#include <cstring>

namespace oj {

namespace {

// -1 below 0, 1 above, else 0 (a NaN too).
float signOf(float v) {
    if (v < 0.0f) return -1.0f;
    if (v > 0.0f) return 1.0f;
    return 0.0f;
}

// 0x004F3830: where x lies from a to b, as a share of b - a, x held within them.
float ramp(float a, float x, float b) {
    if (x < a) return (a - a) / (b - a);
    if (x > b) return (b - a) / (b - a);
    return (x - a) / (b - a);
}

float inverseOr0(float v) {
    return v != 0.0f ? 1.0f / v : 0.0f;
}

// 0x004F5D80: the brake, the handbrake and the axle friction against the wheel's spin.
void wheelBrakes(CarWheel& w) {
    w.brakeTorque = w.brakeTorque - (w.def->brakeMu * w.brake) * signOf(w.spin);
    w.brakeTorque = w.brakeTorque - (w.def->handbrakeMu * w.handbrake) * signOf(w.spin);
    w.brakeTorque = w.brakeTorque - signOf(w.spin) * w.def->axleMu;
}

}  // namespace

CarBody::CarBody(const CarDynamics& def, const Vec4& origin) : CarBodyDef(def), RigidBody(carDef.body, origin) {
    wheels.resize(carDef.wheels.size());
    for (size_t i = 0; i < wheels.size(); ++i) wheels[i].def = &carDef.wheels[i];
    nitroLeft = carDef.nitroTime;
    m_fittedTurbo = carDef.turbo;
    state.reserve(state.size() + 5);
    engineIndex = state.size();
    state.push_back(0.0f);
    for (CarWheel& w : wheels) {
        w.index = state.size();
        state.push_back(0.0f);
    }
    // 0x004F5A70: the inertias of the engine and of the driven wheels.
    drivenInertia = 0.0f;
    for (const CarWheel& w : wheels) {
        if (w.def->drive != 0) drivenInertia = w.def->inertia + drivenInertia;
    }
    engineInertiaInverse = inverseOr0(carDef.engineInertia);
    drivenInertiaInverse = inverseOr0(drivenInertia);
    updatePeakTorque();
}

void CarBody::updatePeakTorque() {
    peakTorque = !carDef.torqueSpeeds.empty() && !carDef.torqueFactors.empty()
                     ? carDef.torqueFactors.back() * carDef.maxTorque * 2.8f
                     : 0.0f;
}

CarWheel* CarBody::wheelOf(int32_t id) {
    for (CarWheel& w : wheels) {
        if (w.def->id == id) return &w;
    }
    return nullptr;
}

void CarBody::setState(std::vector<float>& s) {
    RigidBody::setState(s);
    engineSpeed = s[engineIndex];
    for (CarWheel& w : wheels) w.spin = s[w.index];
    if (clutchIn && gear != 1) {
        for (CarWheel& w : wheels) {
            if (w.def->drive == 0) continue;
            w.spin = w.def->finalDriveInverse * ratioInverse * engineSpeed;
            s[w.index] = w.spin;
        }
    }
}

void CarBody::derivative(std::vector<float>& out, float dt) {
    RigidBody::derivative(out, dt);
    if (!clutchIn || gear == 1) {
        out[engineIndex] = engineTorque * engineInertiaInverse;
    } else {
        // The engine turns the driven wheels with it: their inertias through the gear join the engine's.
        float inertia = carDef.engineInertia;
        for (const CarWheel& w : wheels) {
            if (w.def->drive != 0) inertia = std::fabs(ratioInverse) * w.def->inertia * w.def->finalDriveInverse + inertia;
        }
        out[engineIndex] = inertia != 0.0f ? engineTorque / inertia : 0.0f;
    }
    for (const CarWheel& w : wheels) out[w.index] = w.torque * w.def->inertiaInverse;
}

void CarBody::forces(float dt) {
    // The tyres' values for their forces (0x004F9090 reads them).
    for (size_t i = tyreCount; i-- > 0;) {
        Contact& r = tyre(i);
        // Stock problem: a contact without a wheel of its ID reads through a null pointer; left out here.
        const CarWheel* w = wheelOf(r.def->id);
        if (w == nullptr) continue;
        r.wheelSpin = w->spin;
        r.radius = w->def->radius;
        r.steer = w->def->maxSteerAngle * steering;
        r.gravity = carDef.body.localGravity;
        r.gravityInverse = inverseOr0(r.gravity);
        // Stock problem: the drive is that of the wheel at the contact's place in the list, not of the contact's own
        // wheel (and past the wheels the memory after them). The fixed build takes the contact's own wheel's.
        const bool driven = w->def->drive != 0;
        r.throttle = driven ? throttle : 0.0f;
    }

    RigidBody::forces(dt);

    engineTorque = 0.0f;
    for (size_t i = wheels.size(); i-- > 0;) {
        wheels[i].brakeTorque = 0.0f;
        wheels[i].tyreTorque = 0.0f;
    }
    engineDrive();
    for (size_t i = wheels.size(); i-- > 0;) wheelBrakes(wheels[i]);

    // The tyre's force along the wheel's direction pulls at the rim. The sine and cosine are the x87's, at full
    // precision until their products round.
    for (size_t i = tyreCount; i-- > 0;) {
        const Contact& r = tyre(i);
        CarWheel* w = wheelOf(r.def->id);
        if (w == nullptr) continue;
        const auto angle = static_cast<double>(-(w->def->maxSteerAngle * steering));
        const float along = (productToFloat(std::cos(angle), r.friction[2]) + r.friction[1] * 0.0f) +
                            productToFloat(-std::sin(angle), r.friction[0]);
        w->tyreTorque = w->tyreTorque - along * w->def->radius;
    }
    for (size_t i = wheels.size(); i-- > 0;) wheels[i].torque = wheels[i].tyreTorque + wheels[i].brakeTorque;

    // With the clutch in the driven wheels' torques reach the engine through the gear.
    if (clutchIn && gear != 1) {
        for (size_t i = wheels.size(); i-- > 0;) {
            const CarWheel& w = wheels[i];
            if (w.def->drive != 0) engineTorque = w.def->finalDriveInverse * w.torque * ratioInverse + engineTorque;
        }
    }
    // A slipping clutch passes a torque by the difference of the spins: 500 in reverse and neutral, the gear's ratio
    // x 160 in the others, less within 10 rad/s.
    if (engineOn && !clutchIn && gear != 1) {
        for (size_t i = wheels.size(); i-- > 0;) {
            CarWheel& w = wheels[i];
            if (w.def->drive == 0) continue;
            const float difference = engineSpeed - std::fabs(ratio) * w.def->finalDrive * w.spin;
            float passed = gear <= 1 ? signOf(difference) * 500.0f : signOf(difference) * ratio * 160.0f;
            if (std::fabs(difference) < 10.0f) passed = passed * std::fabs(difference) * 0.1f;
            engineTorque = engineTorque - passed;
            w.torque = signOf(ratio) * passed + w.torque;
        }
    }
    // A wheel is not taken past the spin its tyre rolls at (or past standing by its brake) within the step.
    for (size_t i = tyreCount; i-- > 0;) {
        const Contact& r = tyre(i);
        CarWheel* w = wheelOf(r.def->id);
        if (w == nullptr) continue;
        const float target = std::fabs(w->brakeTorque) < std::fabs(w->tyreTorque) ? r.groundSpin / w->def->radius : 0.0f;
        const float change = dt * w->def->inertiaInverse * w->torque;
        if (change == 0.0f) {
            w->torque = 0.0f;
            continue;
        }
        const float share = (target - w->spin) / change;
        if (!(share < 0.0f || std::isnan(share)) && share < 1e-7f) {
            w->torque = 0.0f;
        } else if (share < 1.0f && share > 0.0f) {
            w->torque = share * w->torque;
        }
    }
}

void CarBody::engineDrive() {
    if (!engineOn || clutchIn || gear == 1) {
        const float full = torqueFactor(carDef, engineSpeed) * carDef.maxTorque * throttleOut * 2.8f;
        float drive = ((carDef.turbo - 1.0f) * boost() + 1.0f) * full;
        if (nitroOn && nitroLeft > 0.0f) drive = drive * carDef.nitro;
        engineTorque = drive + engineTorque;
    }
    // The engine brakes off the throttle by its speed against the last speed of its curve.
    const float braking = (1.0f - throttleOut) * carDef.maxTorque;
    const float top = carDef.torqueSpeeds.empty() ? 0.0f : carDef.torqueSpeeds.back();
    engineTorque = engineTorque - engineSpeed / top * (braking + braking);
    engineTorqueOut = engineTorque;
}

float CarBody::boost() const {
    uint32_t bits;
    std::memcpy(&bits, &carDef.turbo, 4);
    if (bits == 0x3F800000u) return 0.0f;
    // The turbo comes in over the rpm from PowerRangeMin to a tenth of the way to PowerRangeMax.
    const float low = carDef.powerRangeMin, high = carDef.powerRangeMax;
    const float onset = (high - low) * 0.1f + low;
    const float rpm = engineSpeed * 9.549296f;
    if (rpm < onset) return ramp(0.0f, rpm, high) * ramp(low, rpm, onset);
    return ramp(0.0f, rpm, high);
}

void CarBody::constrain(std::vector<float>& s, float dt) {
    // Stock problem: an empty TorqueCurveX reads and writes the memory around the list; 0 and nothing written here.
    const std::vector<float>& speeds = carDef.torqueSpeeds;
    const float first = speeds.empty() ? 0.0f : speeds.front();
    const float last = speeds.empty() ? 0.0f : speeds.back();
    float& spin = s[engineIndex];
    if (spin < first) {
        spin = first;
    } else if (spin > last) {
        spin = last;
    }
    // The redline moves 45 % up while the nitro burns.
    if (!carDef.torqueSpeeds.empty()) {
        carDef.torqueSpeeds.back() = nitroOn ? carDef.topSpeed * 1.45f : carDef.topSpeed;
    }
    updatePeakTorque();
    RigidBody::constrain(s, dt);
}

void CarBody::updateClutch(float dt) {
    if (throttle > 0.0f) {
        if (!engineOn) {
            engineOn = true;
            clutch = 0.0f;
            clutchIn = false;
        }
    } else {
        // Stock problem: an empty TorqueCurveX reads before the list; 0 here.
        const float idle = carDef.torqueSpeeds.empty() ? 0.0f : carDef.torqueSpeeds.front();
        if (idle * 9.644789f > engineSpeed * 9.549296f) {
            engineOn = false;
            clutch = 0.0f;
            clutchIn = false;
        }
    }
    if (!engineOn || clutchIn) return;
    // Taken up at 5 a second in first gear and after a shift down, else at 2.5.
    const float time = dt < 0.0f ? 0.0f : dt;
    const float taken = gear == 2 || previousGear > gear ? time * 5.0f : time * 2.5f;
    clutch = taken + clutch;
    if (clutch < 0.0f) {
        clutch = 0.0f;
    } else if (clutch > 1.0f) {
        clutch = 1.0f;
    }
    if (clutch > 0.99f) clutchIn = true;
}

void CarBody::automaticGearbox(float dt) {
    if (!automatic) return;
    shiftTimer = shiftTimer - dt;
    if (shiftTimer < 0.0f) shiftTimer = 0.0f;
    // The rpm bounds are the variables 0x00725688 (0.75) and 0x0072568C (0.6), the waits 0x00725684 (1.5) and 0.5.
    if (throttle > 0.0f && gear != 0 && clutchIn) {
        const float rpm = engineSpeed * 9.549296f;
        const auto gears = static_cast<uint32_t>(carDef.forwardRatios.size() + 2);  // 0x004203C0
        if (carDef.powerRangeMax * 1.02f < rpm && gear < gears - 1) {
            if (gear == 2 && std::fabs(steering) > 0.5f && localVelocity[2] < 12.0f) return;
            shiftTimer = 1.5f;
            if (localVelocity[2] > 0.894f || gear < 2) shift(gear + 1);
            return;
        }
        if (throttle > 0.6f && 0.75f * carDef.powerRangeMax > rpm && gear > 2 && shiftTimer == 0.0f) shift(gear - 1);
        return;
    }
    if (throttle < 0.1f && 0.6f * carDef.powerRangeMax > engineSpeed * 9.549296f && gear > 2 && clutchIn &&
        shiftTimer == 0.0f) {
        shiftTimer = 0.5f;
        shift(gear - 1);
    }
}

void CarBody::addTouch(const Touch& t) {
    RigidBody::addTouch(t);
    const ContactDef* d = contactDefOf(t.id, t.point);
    if (d == nullptr || d->type != ContactDef::Tyre || pools == nullptr || pools->tyreUsed >= pools->tyre.size() ||
        tyreCount >= 8) {
        return;
    }
    size_t i = 0;
    while (i < tyreCount && tyre(i).def->id != t.id) ++i;
    Contact* c = nullptr;
    if (i == tyreCount) {
        c = &pools->tyre[pools->tyreUsed++];
        c->begin(t, *this, *d);
        contacts.push_back(c);
        ++tyreCount;
    } else {
        c = &tyre(i);
        if (!(t.depth * 0.01f > c->compression)) return;
        c->begin(t, *this, *d);
    }
    CarWheel* w = wheelOf(c->def->id);
    if (w != nullptr && w->touched < c->compression) {
        w->touched = c->maxCompression() < c->compression ? c->maxCompression() : c->compression;
    }
}

void CarBody::setTyreGrip(float lateralMu) {
    for (ContactDef& c : carDef.body.contacts) {
        if (c.type != ContactDef::Tyre) continue;
        c.lateralMu = lateralMu;
        const float divisor = lateralMu * c.longitudinalStiffness;
        c.muRatio = divisor == 0.0f ? 0.0f : (c.longitudinalMu * c.lateralStiffness) / divisor;
    }
}

std::array<float, 4> CarBody::restWheels(const std::array<Vec4, 4>& bases) {
    std::array<float, 4> sag{};
    BodyDef& b = carDef.body;
    for (LineCollider& l : b.collision.lines) {
        if (l.id < 0 || l.id >= 4) continue;
        const auto i = static_cast<size_t>(l.id);
        l.point[0] = bases[i][0];
        l.point[2] = bases[i][2];
        const ContactDef* tyre = nullptr;
        for (const ContactDef& c : b.contacts) {
            if (c.id == l.id) {
                tyre = &c;
                break;
            }
        }
        if (tyre == nullptr || tyre->type != ContactDef::Tyre || !(tyre->stiffness > 0.0f)) continue;
        sag[i] = b.localGravity / tyre->stiffness * tyre->mass * 1000.0f;
        l.length = sag[i] + l.point[1];
    }
    return sag;
}

float CarBody::tyreGrip() const {
    float grip = 0.0f;
    for (const ContactDef& c : carDef.body.contacts) {
        if (c.type == ContactDef::Tyre) grip = c.lateralMu;
    }
    return grip;
}

void CarBody::shift(uint32_t to) {
    if (to == gear) return;
    if (engineOn) {
        clutch = 0.0f;
        clutchIn = false;
    }
    setGear(to);
}

void CarBody::setGear(uint32_t to) {
    if (to > 7) to = 7;
    previousGear = gear;
    gear = to;
    if (to == 0) {
        ratio = carDef.reverseRatio;
    } else if (to == 1) {
        ratio = 0.0f;
    } else if (to - 2 < carDef.forwardRatios.size()) {
        ratio = carDef.forwardRatios[to - 2];
    } else {
        // Stock problem: a gear past the car's reads after the list of ratios; neutral here.
        ratio = 0.0f;
    }
    ratioInverse = inverseOr0(ratio);
}

void CarBody::resetForStart() {
    setLocalAngularVelocity({});
    setLocalVelocity({});
    setAngles({});
    previousGear = gear;
    gear = 2;
    // Stock problem: a car without forward gears reads before the list; neutral's ratio here.
    ratio = carDef.forwardRatios.empty() ? 0.0f : carDef.forwardRatios.front();
    ratioInverse = inverseOr0(ratio);
    if (!engineOn) {
        engineOn = true;
        clutch = 0.0f;
        clutchIn = false;
    }
    engineSpeed = 0.0f;
    state[engineIndex] = 0.0f;
    throttle = 0.0f;
    for (int32_t id = 0; id < 4; ++id) {
        if (CarWheel* w = wheelOf(id)) {
            w->spin = 0.0f;
            state[w->index] = 0.0f;
        }
    }
}

void settleOnGrid(CarBody& car, ContactPools& pools, const Vec4& place, float heading, int& firstSteps,
                  PhysicsWorld* world, bool refill) {
    car.resetForStart();
    if (refill) car.nitroLeft = car.dynamics().nitroTime;
    car.automatic = false;
    if (world != nullptr) world->makeGrid();
    uint32_t headingBits;
    std::memcpy(&headingBits, &heading, 4);
    Vec4 centre{place[0] * 0.01f, place[1] * 0.01f, place[2] * 0.01f, 0.0f};
    std::memcpy(&centre[3], &headingBits, 4);
    car.setPosition(centre);
    car.setAngles({0.0f, heading, 0.0f, 0.0f});
    auto settle = [&](int steps, bool turnBack) {
        for (int i = 0; i < steps; ++i) {
            car.previousGear = car.gear;
            car.gear = 1;
            car.ratio = 0.0f;
            car.ratioInverse = 0.0f;
            car.brake = 0.0f;
            car.throttle = 0.0f;
            pools.empty();
            car.step(0.02f);
            car.syncNode();
            if (!turnBack) continue;
            for (size_t k = car.tyreCount; k-- > 0;) {
                if (car.tyre(k).normal[1] > 0.5f) {
                    turnBack = false;
                    car.setLocalVelocity({});
                    car.setLocalAngularVelocity({});
                    car.setAngles({car.angles[0], heading, car.angles[2], car.angles[3]});
                }
            }
        }
    };
    settle(firstSteps, true);
    car.setLocalVelocity({});
    car.setLocalAngularVelocity({});
    car.setAngles({car.angles[0], heading, car.angles[2], car.angles[3]});
    firstSteps = 50;
    settle(50, false);
    car.setLocalVelocity({});
    car.setLocalAngularVelocity({});
}

void CarBody::step(float dt) {
    dt = dt * carDef.timeFactor;
    if (nitroOn) {
        nitroLeft = nitroLeft - dt * nitroRate;
        if (nitroLeft < 0.0f) nitroLeft = 0.0f;
    }

    // The ABS: over sqrt(200) m/s with the brake on, a wheel whose tyre locks (slip under -0.15) lets go, the others
    // follow the pedal at 15 a second, never under 0.05.
    absActive = false;
    const Vec4& v = localVelocity;
    const float speed2 = (v[2] * v[2] + v[1] * v[1]) + v[0] * v[0];
    if (!carDef.abs || !(speed2 > 200.0f) || !(brake > 0.0f)) {
        for (CarWheel& w : wheels) w.brake = brake;
    } else {
        for (size_t i = 0; i < tyreCount; ++i) {
            const Contact* r = &tyre(i);
            CarWheel* w = wheelOf(r->def->id);
            if (w == nullptr) continue;
            if (r->slip < -0.15f) {
                w->brake = 0.0f;
                absActive = true;
            } else {
                w->brake = (brake - w->brake) * dt * 15.0f + w->brake;
            }
            if (w->brake < 0.0f) {
                w->brake = 0.0f;
            } else if (w->brake > 1.0f) {
                w->brake = 1.0f;
            }
            if (w->brake < 0.05f) w->brake = 0.05f;
        }
    }

    // A driven wheel that spins on the throttle is braked by the pedal's rest, then the brake eases with the throttle.
    for (CarWheel& w : wheels) w.handbrake = handbrake;
    for (size_t i = 0; i < tyreCount; ++i) {
        const Contact* r = &tyre(i);
        CarWheel* w = wheelOf(r->def->id);
        if (w == nullptr || w->def->drive == 0) continue;
        if (r->slip > 0.05f) {
            const float rest = 1.0f - throttle;
            w->brake = rest < w->brake ? w->brake : rest;
        }
        w->brake = (1.0f - throttle * 0.5f) * w->brake;
    }

    // The traction control by the largest slip of the driven wheels.
    float slip = 0.0f;
    for (size_t i = 0; i < tyreCount; ++i) {
        // Stock problem: the wheel at the contact's place in the list decides, past the wheels a null pointer is
        // read. The fixed build takes the contact's own wheel (none: not driven).
        const CarWheel* own = wheelOf(tyre(i).def->id);
        if (own == nullptr || own->def->drive == 0) continue;
        if (slip < tyre(i).slip) slip = tyre(i).slip;
    }
    asrActive = false;
    if (carDef.asr && speed2 > 1.0f && throttle > 0.0f &&
        (std::fabs(steering) < 0.75f || localVelocity[2] > 8.94f) && carDef.asrFactor > 0.0f) {
        float cut = 0.0f;
        if (asrMode) {
            if (slip > (1.0f - carDef.asrFactor) * 3.0f + 0.2f) {
                asrActive = true;
                cut = slip < 1.0f ? slip : 1.0f;
            }
        } else if (slip > 0.6f) {
            asrActive = true;
            const float over = slip - 0.6f;
            cut = over < 1.0f ? over : 1.0f;
        }
        if (asrActive) {
            const float allowed = 1.0f - cut;
            throttle = allowed > throttle ? throttle : allowed;
        }
    }
    throttleOut = throttle;
    if (slip < 0.8f) automaticGearbox(dt);

    for (CarWheel& w : wheels) w.touched = 0.0f;
    tyreFirst = pools != nullptr ? pools->tyreUsed : 0;
    tyreCount = 0;
    RigidBody::step(dt);
    updateClutch(dt);
}

}  // namespace oj
