#include "Core/Input/ForceFeedback.h"

#include "Core/Base/Log.h"
#include "Core/Base/X87.h"

#include <sys/timeb.h>

#include <cmath>
#include <format>
#include <utility>

namespace oj::ff {

namespace {

constexpr int32_t kFailed = static_cast<int32_t>(0x80004005u);  // E_FAIL
constexpr uint32_t kPlaying = 1;                                // DIEGES_PLAYING
constexpr int32_t kForever = -1;

// 0x00596D00: the message to the debugger (with "\r\n").
void say(const std::string& text) {
    log::debugger(text);
}

// The two messages of a force asked for on a channel without a device. Stock problem: the first one's %d takes no
// argument (whatever the stack holds); here the channel.
void noDevice(int ch) {
    say(std::format("ERROR: trying to play a force on channel {} but no", ch));
    say("wheel/joystick opened.\n");
}

}  // namespace

std::array<int32_t, 360> sineTable() {
    // The x87 unit in single precision ([0x006EDB40] 3.141592654, [0x006FF790] 1 / 180, [0x006FF788] 1000): fsin of
    // the product, the result x 1000 made whole toward zero.
    std::array<int32_t, 360> t{};
    for (int i = 0; i < 360; ++i) {
        const float a = static_cast<float>(static_cast<double>(i) * 3.141592654);
        const float x = static_cast<float>(static_cast<double>(a) * (1.0 / 180.0));
        const float s = static_cast<float>(std::sin(static_cast<double>(x)) * 1000.0);
        t[static_cast<size_t>(i)] = x87::chop(s);
    }
    return t;
}

Time Wheels::now() const {
    if (clock) return clock();
    __timeb64 tb{};
    _ftime64_s(&tb);
    return {static_cast<int32_t>(tb.time), static_cast<int32_t>(tb.millitm)};
}

void Wheels::open(const std::array<WheelDevice*, 2>& devices, const std::array<uint32_t, 2>& ffAxes) {
    // 0x00596700, 0x00596790: the reader's channels; 0x005963E0 for the five groups; 0x00591EA0.
    m_channels = {};
    m_groups = {};
    const Time t = now();
    for (int ch = 0; ch < kChannels; ++ch) {
        Channel& c = m_channels[static_cast<size_t>(ch)];
        c.device = devices[static_cast<size_t>(ch)];
        c.axes = c.device != nullptr ? ffAxes[static_cast<size_t>(ch)] : 0;
        c.frontalTime = t;
        c.sideTime = t;
    }
    m_sine = sineTable();
    say("EZ Wheel Wrapper v4.30.001\n");
}

void Wheels::releaseGroup(Group g) {
    for (int ch = 0; ch < kChannels; ++ch) {
        WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
        for (int s = 0; s < 8; ++s) {
            Slot& sl = slot(g, s, ch);
            if (sl.effect == 0) continue;
            if (d != nullptr) d->releaseEffect(sl.effect);
            sl.effect = 0;
        }
    }
}

void Wheels::releaseAll() {
    for (int g = 0; g < kGroups; ++g) releaseGroup(static_cast<Group>(g));
}

void Wheels::shutdown() {
    for (int ch = 0; ch < kChannels; ++ch) {
        if (!hasDevice(ch)) continue;
        if (gainPercent(ch) != 100) setGainPercent(ch, 100);
    }
    releaseGroup(kConstants);
    releaseGroup(kPeriodics);
    releaseGroup(kSpare);
    releaseGroup(kConditions);
    for (Channel& c : m_channels) {
        if (c.device == nullptr) continue;
        c.device->release();
        c.device = nullptr;
    }
}

// 0x00596AE0: Poll; failing, Acquire (and nothing more this frame), with the actuators turned on; else the actuators
// on when they are not (with force feedback axes); the state kept as the last one and read again. A failed read
// noted; unplugged (DIERR_UNPLUGGED) the channel loses its device (Stock problem: not released).
int32_t Wheels::readChannel(int ch) {
    Channel& c = m_channels[static_cast<size_t>(ch)];
    if (c.device == nullptr) {
        say(std::format("ERROR: trying to read a wheel on channel {} that doesn't have a handle\n", ch));
        return 0;
    }
    if (c.device->poll() < 0) {
        if (c.device->acquire() < 0) return 0;
        const int32_t hr = c.device->actuatorsOn();
        if (hr < 0) {
            say(std::format("ERROR: ReadChannel {}: failed to set actuators to ON\n", ch));
            c.actuators = false;
            return hr;
        }
        c.actuators = true;
        return 0;
    }
    if (!c.actuators && c.axes != 0) {
        const int32_t hr = c.device->actuatorsOn();
        if (hr < 0) {
            say(std::format("ERROR: ReadChannel {}: failed to set actuators to ON\n", ch));
            c.actuators = false;
            return hr;
        }
        c.actuators = true;
    }
    c.previous = c.state;
    const int32_t hr = c.device->state(c.state);
    if (hr >= 0) return 0;
    say(std::format("ERROR: could not retrieve positional information for wheel on channel {}\n", ch));
    if (static_cast<uint32_t>(hr) == kDeviceUnplugged) {
        say(std::format("Wheel got unplugged on channel {}!\n", ch));
        c.device = nullptr;
    }
    return hr;
}

void Wheels::read() {
    for (int ch = 0; ch < kChannels; ++ch) {
        if (hasDevice(ch)) readChannel(ch);
    }
}

bool Wheels::playing(Group g, int s, int ch) const {
    const Slot& sl = slot(g, s, ch);
    const WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
    if (sl.effect == 0 || d == nullptr) return false;
    uint32_t status = 0;
    if (const_cast<WheelDevice*>(d)->status(sl.effect, status) < 0) return false;
    return status == kPlaying;
}

bool Wheels::flag(Group g, int s, int ch) const {
    return slot(g, s, ch).playing;
}

int32_t Wheels::start(Group g, int s, int ch) {
    Slot& sl = slot(g, s, ch);
    WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
    if (sl.effect == 0 || d == nullptr) {
        say(std::format("ERROR: Trying to start force effect on channel {} but we have an invalid effectid\n", ch));
        return 0;
    }
    int32_t hr = d->acquire();
    if (hr < 0) return hr;
    hr = d->start(sl.effect);
    if (hr < 0) {
        say(std::format("ERROR: Failed to start force effect on channel {}\n", ch));
        return hr;
    }
    sl.playing = true;
    return hr;
}

int32_t Wheels::stop(Group g, int s, int ch) {
    Slot& sl = slot(g, s, ch);
    if (!sl.playing) return 0;
    WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
    if (sl.effect == 0 || d == nullptr) {
        say(std::format("ERROR: Trying to stop force effect on channel {} but we have an invalid effectid\n", ch));
        return 0;
    }
    d->acquire();
    const int32_t hr = d->stop(sl.effect);
    if (hr < 0) {
        say(std::format("ERROR: Failed to stop force effect on channel {}\n", ch));
        return hr;
    }
    sl.playing = false;
    return hr;
}

// Stock problem: the effect is unloaded and forgotten, not released.
int32_t Wheels::unload(Group g, int s, int ch) {
    Slot& sl = slot(g, s, ch);
    WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
    if (sl.effect == 0 || d == nullptr) {
        say(std::format("ERROR: Trying to unload force effect on channel {} but we have an invalid effectid\n", ch));
        return 0;
    }
    d->acquire();
    const int32_t hr = d->unload(sl.effect);
    if (hr < 0) {
        say(std::format("ERROR: Failed to unload force effect on channel {}\n", ch));
        return hr;
    }
    sl.playing = false;
    sl.effect = 0;
    return hr;
}

// The slot's effect unloaded first when it has one; with the channel's device made (CreateEffect); the result an
// error when no effect came.
int32_t Wheels::create(Group g, int s, int ch, const EffectParams& p) {
    Slot& sl = slot(g, s, ch);
    if (sl.effect != 0) unload(g, s, ch);
    int32_t hr = 0;
    if (WheelDevice* d = m_channels[static_cast<size_t>(ch)].device; d != nullptr) {
        hr = d->createEffect(p, sl.effect);
        if (hr < 0) {
            const char* kind = g == kConditions ? "condition" : g == kConstants ? "constant" : "periodic";
            say(std::format("ERROR: Failed to create {} effect on channel {}\n", kind, ch));
            return hr;
        }
    }
    return sl.effect == 0 ? kFailed : hr;
}

int32_t Wheels::change(Group g, int s, int ch, const EffectParams& p) {
    Slot& sl = slot(g, s, ch);
    WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
    if (sl.effect == 0 || d == nullptr) return 0;
    const int32_t hr = d->setParameters(sl.effect, p);
    if (hr < 0) {
        const char* kind = g == kConditions ? "condition" : g == kConstants ? "constant" : "periodic";
        say(std::format("ERROR: Failed to update {} force on channel {}\n", kind, ch));
    }
    return hr;
}

EffectParams Wheels::constantParams(int ch, uint32_t duration, int32_t magnitude) const {
    EffectParams p;
    p.type = EffectType::Constant;
    p.duration = duration;
    p.axisCount = m_channels[static_cast<size_t>(ch)].axes;
    p.directions = {1, 0};
    p.envelope = true;
    p.magnitude = magnitude;
    return p;
}

EffectParams Wheels::periodicParams(int ch, EffectType type, uint32_t duration, int32_t magnitude, uint32_t period,
                                    uint32_t fadeTime) const {
    EffectParams p;
    p.type = type;
    p.duration = duration;
    p.axisCount = m_channels[static_cast<size_t>(ch)].axes;
    p.directions = {1, 0};
    p.envelope = true;
    p.fadeTime = fadeTime;
    p.magnitude = magnitude;
    p.period = period;
    return p;
}

EffectParams Wheels::conditionParams(int ch, EffectType type, std::array<int32_t, 2> directions, int32_t offset,
                                     int32_t coefficient, int32_t saturation) const {
    EffectParams p;
    p.type = type;
    p.axisCount = m_channels[static_cast<size_t>(ch)].axes;
    p.directions = directions;
    p.conditions[0] = {offset, coefficient, coefficient, static_cast<uint32_t>(saturation),
                       static_cast<uint32_t>(saturation), 0};
    p.conditions[1] = {0, 8000, 8000, 8000, 8000, 500};
    return p;
}

bool Wheels::ready(int ch) {
    const Channel& c = m_channels[static_cast<size_t>(ch)];
    if (c.device == nullptr) {
        noDevice(ch);
        return false;
    }
    return !c.airborne && c.axes != 0;
}

// The spring (the conditions' slot 0, directions 1, 1): nothing while the negative damper (slot 2) plays; playing,
// changed when its offset, saturation or coefficient (+0x4DF8 ..) differ; else made, or changed, and started.
void Wheels::spring(int ch, int32_t offset, int32_t saturation, int32_t coefficient) {
    if (playing(kConditions, 2, ch)) return;
    if (!ready(ch)) return;
    Channel& c = m_channels[static_cast<size_t>(ch)];
    const EffectParams p = conditionParams(ch, EffectType::Spring, {1, 1}, offset, coefficient, saturation);
    const bool same = c.springOffset == offset && c.springSaturation == saturation &&
                      c.springCoefficient == coefficient;
    const auto save = [&] {
        c.springOffset = offset;
        c.springSaturation = saturation;
        c.springCoefficient = coefficient;
    };
    if (playing(kConditions, 0, ch)) {
        if (same) return;
        if (change(kConditions, 0, ch, p) >= 0) save();
        return;
    }
    if (slot(kConditions, 0, ch).effect == 0) {
        if (create(kConditions, 0, ch, p) >= 0) save();
    } else if (!same) {
        if (change(kConditions, 0, ch, p) >= 0) save();
    }
    start(kConditions, 0, ch);
}

// The constant force (the constant forces' slot 0, endless): the same way by its magnitude (+0x4E10).
void Wheels::constant(int ch, int32_t magnitude) {
    if (!ready(ch)) return;
    Channel& c = m_channels[static_cast<size_t>(ch)];
    const EffectParams p = constantParams(ch, static_cast<uint32_t>(kForever), magnitude);
    if (playing(kConstants, 0, ch)) {
        if (c.constantMagnitude == magnitude) return;
        if (change(kConstants, 0, ch, p) >= 0) c.constantMagnitude = magnitude;
        return;
    }
    if (slot(kConstants, 0, ch).effect == 0) {
        if (create(kConstants, 0, ch, p) >= 0) c.constantMagnitude = magnitude;
    } else if (c.constantMagnitude != magnitude) {
        if (change(kConstants, 0, ch, p) >= 0) c.constantMagnitude = magnitude;
    }
    start(kConstants, 0, ch);
}

// The magnitude x the sine table's entry of the angle (% 360) / 1000, cut toward zero.
void Wheels::constantAt(int ch, int32_t angle, int32_t magnitude) {
    const int32_t m = m_sine[static_cast<size_t>(((angle % 360) + 360) % 360)] * magnitude / 1000;
    constant(ch, m);
}

void Wheels::stopConstant(int ch) {
    if (m_channels[static_cast<size_t>(ch)].axes != 0 && playing(kConstants, 0, ch)) stop(kConstants, 0, ch);
}

// The damper (the conditions' slot 1, directions 1, 0, saturation 10000): as the spring by its coefficient (+0x4E20).
void Wheels::damper(int ch, int32_t coefficient) {
    if (playing(kConditions, 2, ch)) return;
    if (!ready(ch)) return;
    Channel& c = m_channels[static_cast<size_t>(ch)];
    const EffectParams p = conditionParams(ch, EffectType::Damper, {1, 0}, 0, coefficient, 10000);
    if (playing(kConditions, 1, ch)) {
        if (c.damperCoefficient == coefficient) return;
        if (change(kConditions, 1, ch, p) >= 0) c.damperCoefficient = coefficient;
        return;
    }
    if (slot(kConditions, 1, ch).effect == 0) {
        if (create(kConditions, 1, ch, p) >= 0) c.damperCoefficient = coefficient;
    } else if (c.damperCoefficient != coefficient) {
        if (change(kConditions, 1, ch, p) >= 0) c.damperCoefficient = coefficient;
    }
    start(kConditions, 1, ch);
}

// A jolt from the side (the constant forces' slot 1, 150 ms): the magnitudes of jolts within 150 ms of the last one
// add up (__ftime's seconds and milliseconds); with no device the two messages, without force feedback axes nothing
// (it is played in flight too); else made or changed by the magnitude (+0x4E28) and started again each time.
void Wheels::sideCollision(int ch, int32_t magnitude) {
    Channel& c = m_channels[static_cast<size_t>(ch)];
    const Time t = now();
    const int32_t seconds = t.seconds - c.sideTime.seconds;
    if ((seconds == 0 && t.millis - c.sideTime.millis <= 150) ||
        (seconds == 1 && t.millis - c.sideTime.millis + 1000 <= 150)) {
        magnitude += c.sideMagnitude;
    }
    c.sideTime = t;
    c.sideMagnitude = magnitude;
    if (c.device == nullptr) {
        noDevice(ch);
        return;
    }
    if (c.axes == 0) return;
    const EffectParams p = constantParams(ch, 150000, magnitude);
    if (playing(kConstants, 1, ch)) {
        if (c.sideMade != magnitude && change(kConstants, 1, ch, p) >= 0) c.sideMade = magnitude;
    } else if (slot(kConstants, 1, ch).effect == 0) {
        if (create(kConstants, 1, ch, p) >= 0) c.sideMade = magnitude;
    } else if (c.sideMade != magnitude) {
        if (change(kConstants, 1, ch, p) >= 0) c.sideMade = magnitude;
    }
    start(kConstants, 1, ch);
}

void Wheels::sideCollisionAt(int ch, int32_t angle, int32_t magnitude) {
    const int32_t m = m_sine[static_cast<size_t>(((angle % 360) + 360) % 360)] * magnitude / 1000;
    sideCollision(ch, m);
}

// A jolt from ahead or behind (the periodic ones' slot 0: a square of 75 ms for 150 ms, faded over 19.999 ms): the
// same way, with its own 150 ms (+0x4DC8 ..) and magnitude (+0x4E38).
void Wheels::frontalCollision(int ch, int32_t magnitude) {
    Channel& c = m_channels[static_cast<size_t>(ch)];
    const Time t = now();
    const int32_t seconds = t.seconds - c.frontalTime.seconds;
    if ((seconds == 0 && t.millis - c.frontalTime.millis <= 150) ||
        (seconds == 1 && t.millis - c.frontalTime.millis + 1000 <= 150)) {
        magnitude += c.frontalMagnitude;
    }
    c.frontalTime = t;
    c.frontalMagnitude = magnitude;
    if (c.device == nullptr) {
        noDevice(ch);
        return;
    }
    if (c.axes == 0) return;
    const EffectParams p = periodicParams(ch, EffectType::Square, 150000, magnitude, 75000, 19999);
    if (playing(kPeriodics, 0, ch)) {
        if (c.frontalMade != magnitude && change(kPeriodics, 0, ch, p) >= 0) c.frontalMade = magnitude;
    } else if (slot(kPeriodics, 0, ch).effect == 0) {
        if (create(kPeriodics, 0, ch, p) >= 0) c.frontalMade = magnitude;
    } else if (c.frontalMade != magnitude) {
        if (change(kPeriodics, 0, ch, p) >= 0) c.frontalMade = magnitude;
    }
    start(kPeriodics, 0, ch);
}

// A sine of 65 ms (the periodic ones' slot 1, endless): as the constant force by its magnitude (+0x4E40). Played by
// nothing but a landing's giving back.
void Wheels::sine(int ch, int32_t magnitude) {
    if (!ready(ch)) return;
    Channel& c = m_channels[static_cast<size_t>(ch)];
    const EffectParams p = periodicParams(ch, EffectType::Sine, static_cast<uint32_t>(kForever), magnitude, 65000, 0);
    if (playing(kPeriodics, 1, ch)) {
        if (c.sineMagnitude == magnitude) return;
        if (change(kPeriodics, 1, ch, p) >= 0) c.sineMagnitude = magnitude;
        return;
    }
    if (slot(kPeriodics, 1, ch).effect == 0) {
        if (create(kPeriodics, 1, ch, p) >= 0) c.sineMagnitude = magnitude;
    } else if (c.sineMagnitude != magnitude) {
        if (change(kPeriodics, 1, ch, p) >= 0) c.sineMagnitude = magnitude;
    }
    start(kPeriodics, 1, ch);
}

void Wheels::stopSine(int ch) {
    if (m_channels[static_cast<size_t>(ch)].axes != 0 && playing(kPeriodics, 1, ch)) stop(kPeriodics, 1, ch);
}

// A square of 100 ms (the periodic ones' slot 2, endless) by its magnitude (+0x4E48); played by nothing but a landing.
void Wheels::square(int ch, int32_t magnitude) {
    if (!ready(ch)) return;
    Channel& c = m_channels[static_cast<size_t>(ch)];
    const EffectParams p =
        periodicParams(ch, EffectType::Square, static_cast<uint32_t>(kForever), magnitude, 100000, 0);
    if (playing(kPeriodics, 2, ch)) {
        if (c.squareMagnitude == magnitude) return;
        if (change(kPeriodics, 2, ch, p) >= 0) c.squareMagnitude = magnitude;
        return;
    }
    if (slot(kPeriodics, 2, ch).effect == 0) {
        if (create(kPeriodics, 2, ch, p) >= 0) c.squareMagnitude = magnitude;
    } else if (c.squareMagnitude != magnitude) {
        if (change(kPeriodics, 2, ch, p) >= 0) c.squareMagnitude = magnitude;
    }
    start(kPeriodics, 2, ch);
}

void Wheels::stopSquare(int ch) {
    if (m_channels[static_cast<size_t>(ch)].axes != 0 && playing(kPeriodics, 2, ch)) stop(kPeriodics, 2, ch);
}

// A negative damper (the conditions' slot 2: a damper of -magnitude): the damper and the spring playing are stopped
// first (0x00592090 with 2 and 0, their flags cleared) and noted (+0x4810, +0x4812) to be given back; then as the
// damper by its magnitude (+0x4E50). Played by nothing but a landing.
void Wheels::slippery(int ch, int32_t magnitude) {
    Channel& c = m_channels[static_cast<size_t>(ch)];
    if (playing(kConditions, 1, ch)) {
        if (c.axes != 0) stopForce(ch, 2);
        slot(kConditions, 1, ch).playing = false;
        c.damperDisplaced = true;
    }
    if (playing(kConditions, 0, ch)) {
        if (c.axes != 0) stopForce(ch, 0);
        slot(kConditions, 0, ch).playing = false;
        c.springDisplaced = true;
    }
    if (!ready(ch)) return;
    const EffectParams p = conditionParams(ch, EffectType::Damper, {1, 0}, 0, -magnitude, 10000);
    if (playing(kConditions, 2, ch)) {
        if (c.slipperyMagnitude == magnitude) return;
        if (change(kConditions, 2, ch, p) >= 0) c.slipperyMagnitude = magnitude;
        return;
    }
    if (slot(kConditions, 2, ch).effect == 0) {
        if (create(kConditions, 2, ch, p) >= 0) c.slipperyMagnitude = magnitude;
    } else if (c.slipperyMagnitude != magnitude) {
        if (change(kConditions, 2, ch, p) >= 0) c.slipperyMagnitude = magnitude;
    }
    start(kConditions, 2, ch);
}

// The negative damper stopped and the damper and the spring it stopped given back (their flags set again).
void Wheels::stopSlippery(int ch) {
    Channel& c = m_channels[static_cast<size_t>(ch)];
    if (c.axes == 0) return;
    if (playing(kConditions, 2, ch)) stop(kConditions, 2, ch);
    if (c.damperDisplaced) {
        damper(ch, c.damperCoefficient);
        slot(kConditions, 1, ch).playing = true;
        c.damperDisplaced = false;
    }
    if (c.springDisplaced) {
        spring(ch, c.springOffset, c.springSaturation, c.springCoefficient);
        slot(kConditions, 0, ch).playing = true;
        c.springDisplaced = false;
    }
}

bool Wheels::sameSurface(int ch, int32_t type, int32_t magnitude, int32_t period) const {
    const Channel& c = m_channels[static_cast<size_t>(ch)];
    return c.surfaceType == type && c.surfaceMagnitude == magnitude && c.surfacePeriod == period;
}

// The surface (the periodic ones' slot 3, endless): the type a sine, a square, a triangle, a sawtooth up or down (0 ..
// 4), the period in ms at most 150. Playing: nothing when the same (+0x4E58 ..), changed when of the same type, else
// made anew and started. Not playing: made, the same started again, changed or made anew, and started.
void Wheels::surface(int ch, int32_t type, int32_t magnitude, int32_t period) {
    if (period > 150) period = 150;
    if (!ready(ch)) return;
    Channel& c = m_channels[static_cast<size_t>(ch)];
    static constexpr EffectType kTypes[] = {EffectType::Sine, EffectType::Square, EffectType::Triangle,
                                            EffectType::SawtoothUp, EffectType::SawtoothDown};
    const EffectType kind = type >= 0 && type <= 4 ? kTypes[type] : EffectType::None;
    const EffectParams p = periodicParams(ch, kind, static_cast<uint32_t>(kForever), magnitude,
                                          static_cast<uint32_t>(period * 1000), 0);
    const auto save = [&] {
        c.surfaceType = type;
        c.surfaceMagnitude = magnitude;
        c.surfacePeriod = period;
    };
    if (playing(kPeriodics, 3, ch)) {
        if (sameSurface(ch, type, magnitude, period)) return;
        int32_t hr = 0;
        if (type == c.surfaceType) {
            hr = change(kPeriodics, 3, ch, p);
        } else {
            unload(kPeriodics, 3, ch);
            hr = create(kPeriodics, 3, ch, p);
            start(kPeriodics, 3, ch);
        }
        if (hr >= 0) save();
        return;
    }
    if (slot(kPeriodics, 3, ch).effect == 0) {
        if (create(kPeriodics, 3, ch, p) >= 0) save();
    } else if (!sameSurface(ch, type, magnitude, period)) {
        int32_t hr = 0;
        if (type == c.surfaceType) {
            hr = change(kPeriodics, 3, ch, p);
        } else {
            unload(kPeriodics, 3, ch);
            hr = create(kPeriodics, 3, ch, p);
        }
        if (hr >= 0) save();
    }
    start(kPeriodics, 3, ch);
}

void Wheels::stopSurface(int ch) {
    if (m_channels[static_cast<size_t>(ch)].axes != 0 && playing(kPeriodics, 3, ch)) stop(kPeriodics, 3, ch);
}

// The car leaves the ground: once (+0x2220 set), each force playing stopped and noted: the spring, the constant force
// (0x005927D0), the damper, the sine, the square, the negative damper (0x00593460, giving back what it stopped) and
// the surface. The collisions' jolts are left.
void Wheels::airborneStart(int ch) {
    Channel& c = m_channels[static_cast<size_t>(ch)];
    if (c.device == nullptr) {
        noDevice(ch);
        return;
    }
    if (c.airborne || c.axes == 0) return;
    c.airborne = true;
    if (playing(kConditions, 0, ch)) {
        if (c.axes != 0 && playing(kConditions, 0, ch)) stop(kConditions, 0, ch);
        c.stopped[0] = true;
    }
    if (playing(kConstants, 0, ch)) {
        stopConstant(ch);
        c.stopped[1] = true;
    }
    if (playing(kConditions, 1, ch)) {
        if (c.axes != 0 && playing(kConditions, 1, ch)) stop(kConditions, 1, ch);
        c.stopped[2] = true;
    }
    if (playing(kPeriodics, 1, ch)) {
        stopSine(ch);
        c.stopped[5] = true;
    }
    if (playing(kPeriodics, 2, ch)) {
        stopSquare(ch);
        c.stopped[6] = true;
    }
    if (playing(kConditions, 2, ch)) {
        stopSlippery(ch);
        c.stopped[7] = true;
    }
    if (playing(kPeriodics, 3, ch)) {
        stopSurface(ch);
        c.stopped[8] = true;
    }
}

// The car lands (with force feedback axes and +0x2220 set): +0x2220 cleared and what the flight stopped given back
// with the values kept. Stock problem: the constant force is given back through the sine table with the angle at
// +0x4E14, which nothing writes, so at 0.
void Wheels::airborneEnd(int ch) {
    Channel& c = m_channels[static_cast<size_t>(ch)];
    if (c.axes == 0 || !c.airborne) return;
    c.airborne = false;
    if (c.stopped[0]) spring(ch, c.springOffset, c.springSaturation, c.springCoefficient);
    if (c.stopped[1]) constantAt(ch, c.constantAngle, c.constantMagnitude);
    if (c.stopped[2]) damper(ch, c.damperCoefficient);
    if (c.stopped[5]) sine(ch, c.sineMagnitude);
    if (c.stopped[6]) square(ch, c.squareMagnitude);
    if (c.stopped[7]) slippery(ch, c.slipperyMagnitude);
    if (c.stopped[8]) surface(ch, c.surfaceType, c.surfaceMagnitude, c.surfacePeriod);
    c.stopped = {};
}

// By kind: 0 the spring, 1 the constant force, 2 the damper, 3 the side jolt, 4 the frontal jolt, 5 the sine, 6 the
// square, 7 the negative damper (and what it stopped given back), 8 the surface stopped when playing; 9 a landing's
// giving back as 0x00593B30 does (without its force feedback axes' check).
void Wheels::stopForce(int ch, int kind) {
    static constexpr std::pair<Group, int> kSlots[] = {
        {kConditions, 0}, {kConstants, 0}, {kConditions, 1}, {kConstants, 1}, {kPeriodics, 0},
        {kPeriodics, 1},  {kPeriodics, 2}, {kConditions, 2}, {kPeriodics, 3},
    };
    if (kind < 0 || kind > 9) return;
    Channel& c = m_channels[static_cast<size_t>(ch)];
    if (kind == 7) {
        if (playing(kConditions, 2, ch)) stop(kConditions, 2, ch);
        if (c.damperDisplaced) {
            damper(ch, c.damperCoefficient);
            slot(kConditions, 1, ch).playing = true;
            c.damperDisplaced = false;
        }
        if (c.springDisplaced) {
            spring(ch, c.springOffset, c.springSaturation, c.springCoefficient);
            slot(kConditions, 0, ch).playing = true;
            c.springDisplaced = false;
        }
        return;
    }
    if (kind == 9) {
        if (!c.airborne) return;
        c.airborne = false;
        if (c.stopped[0]) spring(ch, c.springOffset, c.springSaturation, c.springCoefficient);
        if (c.stopped[1]) constantAt(ch, c.constantAngle, c.constantMagnitude);
        if (c.stopped[2]) damper(ch, c.damperCoefficient);
        if (c.stopped[5]) sine(ch, c.sineMagnitude);
        if (c.stopped[6]) square(ch, c.squareMagnitude);
        if (c.stopped[7]) slippery(ch, c.slipperyMagnitude);
        if (c.stopped[8]) surface(ch, c.surfaceType, c.surfaceMagnitude, c.surfacePeriod);
        c.stopped = {};
        return;
    }
    const auto [g, s] = kSlots[kind];
    if (playing(g, s, ch)) stop(g, s, ch);
}

void Wheels::letGo(int ch) {
    if (!hasDevice(ch)) return;
    airborneEnd(ch);
    if (ffAxes(ch) != 0) {
        if (playing(kConstants, 0, ch)) stop(kConstants, 0, ch);
        if (ffAxes(ch) != 0 && playing(kConditions, 1, ch)) stop(kConditions, 1, ch);
    }
    stopSine(ch);
    stopSlippery(ch);
    stopSurface(ch);
    spring(ch, 0, 5000, 5000);
}

int32_t Wheels::gainPercent(int ch) {
    WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
    uint32_t value = 0;
    if (d == nullptr || d->gain(value) < 0) {
        say("ERROR: Failed to get overall force gain\n");
        return -1;
    }
    return static_cast<int32_t>(value * 100u / 10000u);
}

void Wheels::setGainPercent(int ch, uint32_t percent) {
    if (percent > 100) percent = 100;
    say(std::format("Setting gain to {}\n", percent));
    WheelDevice* d = m_channels[static_cast<size_t>(ch)].device;
    if (d == nullptr || d->setGain(percent * 10000u / 100u) < 0) say("ERROR: Failed to set overall force gain\n");
}

void playerForces(Wheels& w, int ch, float strength, const FeedbackCar& car, float& maxLoad) {
    // The pitch: the front wheels' travel against the rear's, within 0.5 .. 1.2 (0x00407EC0).
    float pitch = (((car.travel[1] + car.travel[0]) * 0.5f + 10.0f) - (car.travel[3] + car.travel[2]) * 0.5f) * 0.1f -
                  0.25f;
    if (pitch < 0.5f) {
        pitch = 0.5f;
    } else if (pitch > 1.2f) {
        pitch = 1.2f;
    }
    // The grip: the front wheels' skid and smoke, by the strength (+0xA4) and the pitch, within 0 .. 100.
    const float grip = (2.0f - (car.smoke[1] + car.skid[1])) + (2.0f - (car.smoke[0] + car.skid[0]));
    float load = (strength + 0.5f) * (grip * 0.25f) * (pitch + pitch - 0.5f);
    if (load < 0.0f) {
        load = 0.0f;
    } else if (load > 100.0f) {
        load = 100.0f;
    }
    // The spring by the speed ([0x007252E0] 16; saturation [0x007252DC] 40 x the load, coefficient [0x007252D8] 300).
    const float speed = car.speed;
    const float share = speed / 16.0f;
    const int32_t coefficient = x87::chop(300.0f * share * 50.0f);
    const int32_t saturation = x87::chop(40.0f * share * load * 50.0f);
    w.spring(ch, 0, saturation, coefficient);
    // The steering's pull: the front wheels' travel apart x (DATAVAL of the part of kind 0 + 1) x 0.5; its side the
    // angle (90 for a positive one, else 270), its size / 12 ([0x007252D4]) x 255, at most 200, x the load x 50.
    const float pull = (car.travel[0] - car.travel[1]) * ((car.part0 + 1.0f) * 0.5f);
    const int32_t angle = pull > 0.0f ? 90 : 270;
    const float size = std::fabs(pull);
    if (size > maxLoad) maxLoad = size;
    int32_t level = x87::chop(size / 12.0f * 255.0f);
    if (level > 200) level = 200;
    w.constantAt(ch, angle, x87::chop(static_cast<float>(level) * load * 50.0f));
    // The damper: (20 - the speed) / 20 ([0x007252CC]) x 70 ([0x007252D0]) within 0 .. 70 (0x00407F00), x the load x 50.
    float damping = (20.0f - speed) / 20.0f * 70.0f;
    if (damping < 0.0f) {
        damping = 0.0f;
    } else if (damping > 70.0f) {
        damping = 70.0f;
    }
    w.damper(ch, x87::chop(damping * load * 50.0f));
    // A hit (what is left of it over 0.1): by its point's forward part (its x and y x 0 added: Stock problem, a NaN
    // there spoils it) over 0.95 a frontal jolt of the hit x 255, under -0.95 of the hit x 180, else a jolt from the
    // side (90 for a positive x, else 270) of the hit x 255.
    if (car.impact > 0.1f) {
        const float ahead = (car.hitPoint[1] + car.hitPoint[0]) * 0.0f + car.hitPoint[2];
        if (ahead > 0.95f) {
            w.frontalCollision(ch, x87::chop(car.impact * 255.0f));
        } else if (!(ahead >= -0.95f) && !std::isnan(ahead)) {
            w.frontalCollision(ch, x87::chop(car.impact * 180.0f));
        } else {
            const int32_t side = !(car.hitPoint[0] < 0.0f) ? 90 : 270;
            w.sideCollisionAt(ch, side, x87::chop(car.impact * 255.0f));
        }
    }
    // Off the road (a front wheel's surface not tarmac) at 1 m/s or more: a triangle of the load x 3000 with the period
    // 200 - 9 x the speed ms within 20 .. 200 ([0x007252C0], [0x007252C4], [0x007252C8]), at most 150; else stopped.
    float period = ((20.0f - 200.0f) * speed) / 20.0f + 200.0f;
    if (period < 20.0f) {
        period = 20.0f;
    } else if (period > 200.0f) {
        period = 200.0f;
    }
    const bool tarmac = car.surface[0] == 0 && car.surface[1] == 0;
    if (tarmac || speed < 1.0f) {
        w.stopSurface(ch);
    } else {
        int32_t ms = x87::chop(period);
        if (ms > 150) ms = 150;
        w.surface(ch, 2, x87::chop(load * 3000.0f), ms);
    }
    // Both front wheels off the ground: the flight (0x00593980); else a flight ended (0x00593B30).
    if (!car.down[0] && !car.down[1]) {
        w.airborneStart(ch);
    } else if (w.airborne(ch)) {
        w.airborneEnd(ch);
    }
    // The nitro on with the engine turning: a triangle of (DATAVAL of the part of kind 2 x 140 - 110) x the load x 20
    // ([0x007252BC]), 30 ms.
    if (car.engineSpeed > 0.0f && car.nitroOn) {
        const float nitroLoad = 20.0f * load;
        w.surface(ch, 2, x87::chop((car.part2 * 140.0f - 110.0f) * nitroLoad), 30);
    }
}

}  // namespace oj::ff
