#include "Core/Race/Replay.h"

#include "Core/Physics/CarBody.h"
#include "Core/Race/Racer.h"
#include "Core/Saves/SaveDevice.h"
#include "Core/Base/X87.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace oj {

namespace {

constexpr float kFadeLength = 1.4f;              // [0x006FFB3C]
constexpr float kFadeRate = 0.71428573f;         // [0x006FFE1C]: 1 / 1.4
constexpr uint32_t kNoBlock = 999;               // 0x3E7: a block's next before it is followed by one


uint32_t u32At(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

void putU32(uint8_t* p, uint32_t v) {
    std::memcpy(p, &v, 4);
}

float f32At(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

// 0x004288B0 / 0x00429A10: a replay's USERDATA from its header (the cars' and the type's bytes, the track, the
// route), the date's text and the driver ([G+0x64] slot 7, +0x1C), written u32 cars, u32 type, char[32], char[32],
// char[17], wchar_t[16]. The record (0x00429A70) is cleared but for the last byte of the date's text, of the track
// and of the route and the driver's last character, which hold what the stack held (zeros here); the texts are
// copied whole (strcpy, wcscpy; Stock problem: a driver of 16 characters or more runs past its field, cut here).
std::vector<uint8_t> replayUserData(const ReplayHeader& h, const std::string& date, const std::wstring& driver) {
    std::vector<uint8_t> out(0x79, 0);
    putU32(out.data() + 0x00, h.cars);
    putU32(out.data() + 0x04, h.type);
    std::memcpy(out.data() + 0x08, h.track.data(), std::min<size_t>(h.track.size(), 0x20));
    std::memcpy(out.data() + 0x28, h.route.data(), std::min<size_t>(h.route.size(), 0x20));
    std::memcpy(out.data() + 0x48, date.data(), std::min<size_t>(date.size(), 0x11));
    for (size_t i = 0; i < driver.size() && i < 16; ++i) {
        const uint16_t c = static_cast<uint16_t>(driver[i]);
        std::memcpy(out.data() + 0x59 + i * 2, &c, 2);
    }
    return out;
}

// 0x00427D30: the Catmull-Rom point between p1 and p2 by t, p0 before and p3 after; w of p1.
Vec4 catmullRom(const Vec4& p0, const Vec4& p1, const Vec4& p2, const Vec4& p3, float t) {
    const double t2 = static_cast<double>(t) * t;
    const double t3 = t2 * t;
    Vec4 out{};
    for (int i = 0; i < 3; ++i) {
        const double a = static_cast<double>(p1[i]) + p1[i];
        const double b = (static_cast<double>(-p0[i]) + p2[i]) * t;
        const double c = ((static_cast<double>(p0[i]) + p0[i]) - p1[i] * 5.0 + p2[i] * 4.0) - p3[i];
        const double d = ((static_cast<double>(-p0[i]) + p1[i] * 3.0) - p2[i] * 3.0) + p3[i];
        out[static_cast<size_t>(i)] = static_cast<float>((a + b + c * t2 + d * t3) * 0.5);
    }
    out[3] = p1[3];
    return out;
}

bool anyNonZero(const Vec4& v) {
    return v[3] != 0.0f || v[0] != 0.0f || v[1] != 0.0f || v[2] != 0.0f;
}

// (b - a) x t + a at the x87's precision, rounded once.
float lerpOnce(float a, float b, float t) {
    return static_cast<float>((static_cast<double>(b) - a) * t + a);
}

// A wheel's spin blended; within ±5 rad/s it stands.
float wheelBlend(float a, float b, float t) {
    const double v = (static_cast<double>(b) - a) * t + a;
    if (v <= -5.0 || v >= 5.0) return static_cast<float>(v);
    return 0.0f;
}

// [0x0079F1F8] with its flag [0x0079F1FC]: once the ring is full the block written next, from the cars' count on
// (kept from race to race).
uint32_t g_ringNext = 0;
bool g_ringStarted = false;
// [0x0075ECE4]: the interest of the block before, 0.6 of it carried into the next.
float g_blockInterest = 0.0f;

}  // namespace

// 0x004283E0 on the main thread: each value a float, cut toward zero (the game's MXCSR, or the x87's chop).
PackedFrame packFrame(const ReplayFrame& f) {
    PackedFrame p{};
    std::memcpy(p.data(), &f.time, 4);
    for (size_t i = 0; i < 4; ++i) {
        const auto v = static_cast<uint16_t>(x87::chop((f.orientation[i] + 1.0f) * 32767.0f));
        std::memcpy(p.data() + 4 + i * 2, &v, 2);
    }
    std::memcpy(p.data() + 0x0C, &f.position[0], 4);
    std::memcpy(p.data() + 0x10, &f.position[1], 4);
    std::memcpy(p.data() + 0x14, &f.position[2], 4);
    for (size_t i = 0; i < 4; ++i) {
        p[0x18 + i] = static_cast<uint8_t>(x87::chop((f.wheels[i] + 100.0f) * 0.6375f));
    }
    p[0x1C] = static_cast<uint8_t>(x87::chop(f.rpm * 255.0f * 0.0001f));
    const auto throttle = static_cast<uint8_t>(x87::chop(f.throttle * 3.0f));
    p[0x1D] = static_cast<uint8_t>(f.parts * 4) | throttle;
    const auto steering = static_cast<uint8_t>(x87::chop((f.steering + 1.0f) * 15.0f));
    p[0x1E] = steering | static_cast<uint8_t>(f.car << 5);
    uint8_t flags = static_cast<uint8_t>(f.gear & 0x0F);
    if (f.brake != 0) flags |= 0x10;
    if (f.nitro != 0) flags |= 0x20;
    p[0x1F] = flags;
    auto sound = static_cast<uint32_t>(x87::chop(f.engineSound * 127.0f));
    if (sound > 0x7F) sound = 0x7F;
    p[0x20] = static_cast<uint8_t>(f.horn << 7) | static_cast<uint8_t>(sound);
    return p;
}

ReplayFrame unpackFrame(const uint8_t* p) {
    ReplayFrame f;
    std::memcpy(&f.time, p, 4);
    for (size_t i = 0; i < 4; ++i) {
        uint16_t v;
        std::memcpy(&v, p + 4 + i * 2, 2);
        f.orientation[i] = static_cast<float>(static_cast<double>(v) * 3.0519906e-05 - 1.0);
    }
    std::memcpy(&f.position[0], p + 0x0C, 4);
    std::memcpy(&f.position[1], p + 0x10, 4);
    std::memcpy(&f.position[2], p + 0x14, 4);
    for (size_t i = 0; i < 4; ++i) {
        f.wheels[i] = static_cast<float>(static_cast<double>(p[0x18 + i]) * 1.5686275 - 100.0);
    }
    f.rpm = static_cast<float>(static_cast<double>(p[0x1C]) * 10000.0 * 0.003921569);
    // Stock problem: the throttle is read from the whole byte, the parts' bits with it: with any part off it is 1.
    // The fixed build reads its two bits.
    float throttle = static_cast<float>(static_cast<double>(p[0x1D] & 3) * 0.33333334);
    if (!(0.0f <= throttle)) {
        throttle = 0.0f;
    } else if (1.0f < throttle) {
        throttle = 1.0f;
    }
    f.throttle = throttle;
    f.parts = static_cast<uint8_t>(p[0x1D] >> 2);
    f.car = p[0x1E] >> 5;
    f.steering = static_cast<float>(static_cast<double>(p[0x1E] & 0x1F) * 0.06666667 - 1.0);
    f.nitro = p[0x1F] & 0x20;
    f.gear = p[0x1F] & 0x0F;
    f.brake = p[0x1F] & 0x10;
    f.horn = p[0x20] >> 7;
    f.engineSound = static_cast<float>(static_cast<double>(p[0x20] & 0x7F) * 0.007874016);
    return f;
}

ReplayFrame captureFrame(const RaceCar& car) {
    ReplayFrame f;
    const CarBody& body = *car.body;
    f.orientation = body.orientation;
    f.position = body.position;
    // 0x00489030: the wheels of the IDs 0 .. 3 (0x004F7500), 0 for one the car has not.
    for (int32_t i = 0; i < 4; ++i) {
        const CarWheel* w = const_cast<CarBody&>(body).wheelOf(i);
        f.wheels[static_cast<size_t>(i)] = w != nullptr ? w->spin : 0.0f;
    }
    f.steering = body.steering;
    f.rpm = static_cast<float>(static_cast<double>(body.engineSpeed) * 9.549296);
    f.throttle = body.throttle;
    f.brake = body.brake > 0.1f ? 1 : 0;
    f.parts = car.modelParts;
    f.nitro = car.nitroAsked ? 1 : 0;
    f.gear = body.gear;
    // The horn's sound sounding (+0x4C0, its slot +0x48) and the engine's load (+0x498, its slot +0x30; 0.5 without
    // it), as the game's sound objects left them.
    f.horn = car.hornSounding ? 1 : 0;
    f.engineSound = car.engineLoad;
    return f;
}

void applyFrame(RaceCar& car, const ReplayFrame& f, bool blend, bool replayRace) {
    CarBody& body = *car.body;
    if (blend) {
        Quat to = f.orientation;
        normalize(to);
        const Vec4 at = lerp(body.position, f.position, std::bit_cast<float>(0x3F333333u));
        const Quat turned = slerp(body.orientation, to, std::bit_cast<float>(0x3F333333u));
        body.setPosition(at);
        body.setOrientation(turned, true);
    } else {
        Vec4 at = f.position;
        at[3] = body.position[3];
        body.setPosition(at);
        body.setOrientation(f.orientation, true);
    }
    // 0x00489060: the wheels' spins, also in the state.
    for (int32_t i = 0; i < 4; ++i) {
        if (CarWheel* w = body.wheelOf(i)) {
            w->spin = f.wheels[static_cast<size_t>(i)];
            body.state[w->index] = w->spin;
        }
    }
    // 0x00429CF0: the steering within -1 .. 1.
    body.steering = std::clamp(f.steering, -1.0f, 1.0f);
    body.engineSpeed = static_cast<float>(static_cast<double>(f.rpm) * 0.10471976);
    body.state[body.engineIndex] = body.engineSpeed;
    body.throttle = f.throttle * f.throttle;
    // 0x00488FA0: the nitro asked while the car has some left or the race is a replay's.
    const bool was = car.nitroAsked;
    car.nitroAsked = f.nitro != 0 && (body.nitroLeft > 0.0f || replayRace);
    if (car.nitroAsked && car.nitroAsked != was) car.nitroFlash = 3;
    if (car.nitroFlash != 0) car.nitroFlash = car.nitroFlash - 1;
    car.nitroStarted = car.nitroAsked != was;
    body.setGear(f.gear);
    car.modelParts = f.parts;
    const float brake = f.brake != 0 ? 0.5f : 0.0f;
    body.brake = brake * brake * brake;
    body.engineOn = false;
    body.clutch = 0.0f;
    body.clutchIn = false;
    // The horn's sound (+0x4C0: from 1300, sounding by the frame unless the race is a replay's of mode 4, else
    // quiet; the game's sound objects take it) and the engine sound's value (+0x508).
    car.replayHorn = f.horn;
    car.replayEngineSound = f.engineSound;
}

std::array<uint8_t, ReplayHeader::kSize> ReplayHeader::write() const {
    std::array<uint8_t, kSize> out{};
    putU32(out.data() + 0x00, lastBlock);
    putU32(out.data() + 0x04, lastFrames);
    std::memcpy(out.data() + 0x08, &length, 4);
    putU32(out.data() + 0x0C, bytes);
    std::memcpy(out.data() + 0x10, track.data(), std::min<size_t>(track.size(), 31));
    std::memcpy(out.data() + 0x30, route.data(), std::min<size_t>(route.size(), 31));
    putU32(out.data() + 0x50, timeOfDay);
    putU32(out.data() + 0x54, weather);
    out[0x58] = cars;
    out[0x59] = mode;
    out[0x5A] = type;
    putU32(out.data() + 0x5C, carsBytes);
    putU32(out.data() + 0x60, laps);
    return out;
}

ReplayHeader ReplayHeader::read(const uint8_t* p) {
    ReplayHeader h;
    h.lastBlock = u32At(p + 0x00);
    h.lastFrames = u32At(p + 0x04);
    h.length = f32At(p + 0x08);
    h.bytes = u32At(p + 0x0C);
    h.track.assign(reinterpret_cast<const char*>(p + 0x10), strnlen(reinterpret_cast<const char*>(p + 0x10), 0x20));
    h.route.assign(reinterpret_cast<const char*>(p + 0x30), strnlen(reinterpret_cast<const char*>(p + 0x30), 0x20));
    h.timeOfDay = u32At(p + 0x50);
    h.weather = u32At(p + 0x54);
    h.cars = p[0x58];
    h.mode = p[0x59];
    h.type = p[0x5A];
    h.carsBytes = u32At(p + 0x5C);
    h.laps = u32At(p + 0x60);
    return h;
}

Replay::Replay() : buffer(kBlocks * kBlockSize) {
    next[0].time = -1.0f;
}

uint32_t Replay::blockNumber(uint32_t b) const {
    return u32At(blockAt(b));
}

uint32_t Replay::blockNext(uint32_t b) const {
    return u32At(blockAt(b) + 4);
}

float Replay::frameTime(uint32_t b, uint32_t f) const {
    return f32At(blockAt(b) + 0x0C + static_cast<size_t>(f) * 0x21);
}

void Replay::reset() {
    block = 0;
    frame = 0;
    sequence = 0;
    recordTimer = 0.0f;
    time = 0.0f;
    readTime = 0.0f;
    firstRead = 1;
    playing = 1;
    restartAsked = 1;
    finishing = 0;
    fast = 0;
    fadeDue = 0;
    fadingOut = 0;
    fadingIn = 0;
    ended = 0;
    next[0].time = -1.0f;
    fade = 0.0f;
    marks = {};
}

void Replay::begin(const ReplayRace& race, const std::string& track, const std::string& route, uint32_t timeOfDay,
                   uint32_t weather, uint32_t laps) {
    state = ReplayState::Recording;
    block = 0;
    sequence = 0;
    written = 1;
    frame = 0;
    playing = 0;
    blockOpen = 0;
    fast = 0;
    loaded = 0;
    finishing = 0;
    recordTimer = 0.0f;
    time = 0.0f;
    length = 0.0f;
    clear();
    // 0x00426870 (not in type 3): the header from the race.
    if (race.type != RaceType::Solo) {
        replayRace = 0;
        cars = static_cast<uint32_t>(race.cars.size());
        header.mode = static_cast<uint8_t>(race.mode);
        header.cars = static_cast<uint8_t>(cars);
        header.type = static_cast<uint8_t>(race.type);
        header.laps = laps;
        header.track = track;
        header.route = route;
        header.timeOfDay = timeOfDay;
        header.weather = weather;
        header.length = 0.0f;
        header.lastBlock = 0;
        header.lastFrames = 0;
    }
    interest = 0.0f;
    interestLines = 0;
    marks = {};
    period = race.type == RaceType::Cruise ? std::bit_cast<float>(0x3E0F5C29u) : 0.25f;
}

bool Replay::step(const ReplayRace& race, float dt, bool& pause) {
    pause = false;
    if (race.type == RaceType::Solo || enabled == 0) return false;
    if (cars != race.cars.size()) {
        cars = static_cast<uint32_t>(race.cars.size());
        header.cars = static_cast<uint8_t>(cars);
    }
    if (playing != 0 && state == ReplayState::Stopped) {
        pause = true;
        return false;
    }
    bool first = time < 0.001f;
    if (fast != 0) {
        const float was = time;
        time = readTime + 0.001f;
        if (dt > 0.0f) timeScale = (time - was) / dt;
        dt = time - was;
    } else {
        time = dt + time;
    }
    if (fadeDue != 0 && fadingIn == 0) {
        fadeWait = fadeWait - dt;
        if (fadeWait < 0.0f && playing != 0) {
            fadeDue = 0;
            fadingOut = 1;
            fadeTime = fadeLength;
        }
    } else if (fadingOut != 0) {
        fadeTime = fadeTime - dt;
        fade = 1.0f - fadeTime * kFadeRate;
        if (fadeTime <= 0.0f) {
            if (block == last) {
                fade = 1.0f;
                state = ReplayState::Stopped;
            } else {
                fadingOut = 0;
                time = nextTime - dt;
                fade = 1.0f;
                fadeTime = kFadeLength;
                fadingIn = 1;
            }
        }
    } else if (fadingIn != 0) {
        if (fadeTime == kFadeLength) first = true;
        fadeTime = fadeTime - dt;
        if (fadeDue != 0) fadeWait = fadeWait - dt;
        fade = fadeTime * kFadeRate;
        if (fadeTime <= 0.0f) {
            fadingIn = 0;
            fade = 0.0f;
        }
    }
    bool restart = false;
    if (state == ReplayState::Recording) {
        if (finishing != 0) {
            endWait = endWait - dt;
            if (endWait < 0.0f) {
                stop();
                finishing = 0;
                recordTimer = period;
            }
        }
        recordTimer = recordTimer - dt;
        if (recordTimer < 0.0f) recordTimer = 0.0f;
        record(race);
    } else if (state == ReplayState::Playing) {
        playFrames(race);
        restart = first;
    }
    if (playing != 0 && state == ReplayState::Stopped && last == 0) {
        pause = true;
        fade = 1.0f;
    }
    return restart;
}

void Replay::record(const ReplayRace& race) {
    if (recordTimer != 0.0f) return;
    if (finishing == 0 && ((race.type == RaceType::Cruise && race.cruiseTime <= 0.0f) || race.allFinished)) {
        finishing = 1;
        endWait = kFrames - frame < cars * 25 ? std::bit_cast<float>(0x40CCCCCDu) : 5.0f;
    }
    recordTimer = period;
    for (uint32_t i = 0; i < cars; ++i) {
        const RaceCar* car = i < race.cars.size() ? race.cars[i] : nullptr;
        if (car == nullptr) continue;
        ReplayFrame f = captureFrame(*car);
        f.car = static_cast<int32_t>(i);
        // [5] 0x00428F80: packed, the replay's time in its first 4 bytes.
        f.time = time;
        const PackedFrame p = packFrame(f);
        write(p.data());
        previous[i] = f;
        if (race.type == RaceType::Cruise) {
            // [4] 0x00428F60: the showoff's lines of the frame, 0x18 bytes of a 33-byte frame (the 9 after them are
            // the object's next bytes in the stock game, 0 here).
            uint8_t raw[0x21]{};
            std::memcpy(raw, marks.data(), marks.size());
            write(raw);
            marks = {};
        }
    }
}

void Replay::playFrames(const ReplayRace& race) {
    if (readTime < time) {
        int rounds = firstRead == 0 ? 1 : 2;
        while (rounds-- > 0) {
            if (firstRead != 0) firstRead = 0;
            std::array<bool, 6> seen{};
            while (true) {
                ReplayFrame f;
                f.time = -1.0f;
                readFrame(f, 0);
                if (f.time < 0.0f) {
                    state = ReplayState::Stopped;
                    ended = 1;
                    fade = 1.0f;
                    break;
                }
                const auto c = static_cast<size_t>(f.car);
                if (c >= seen.size() || seen[c]) break;
                seen[c] = true;
                before[c] = previous[c].position;
                previous[c] = next[c];
                next[c] = f;
                ReplayFrame ahead;
                const uint32_t step = race.type == RaceType::Cruise ? 2u : header.cars;
                if (readFrame(ahead, step)) {
                    after[c] = ahead.position;
                } else {
                    after[c] = {};
                }
                readTime = f.time;
                advance();
                if (race.type == RaceType::Cruise) {
                    if (readRaw(marks.data(), marks.size(), 0)) advance();
                }
            }
        }
    }
    for (uint32_t i = 0; i < cars; ++i) {
        RaceCar* car = i < race.cars.size() ? race.cars[i] : nullptr;
        if (ended == 0) {
            const float t = (time - previous[i].time) / (next[i].time - previous[i].time);
            ReplayFrame f = (t < 0.0f || t > 1.0f) ? next[i] : blend(i, t);
            shown[i] = f;
            if (car != nullptr) applyFrame(*car, f, timeScale == 1.0f, true);
        } else if (car != nullptr) {
            applyFrame(*car, previous[i], timeScale == 1.0f, true);
        }
    }
}

ReplayFrame Replay::blend(size_t car, float t) {
    // The frames' quaternions are made unit where they are kept (0x00429DC0).
    Quat from = previous[car].orientation;
    Quat to = next[car].orientation;
    normalize(from);
    normalize(to);
    previous[car].orientation = from;
    next[car].orientation = to;
    ReplayFrame f = next[car];
    f.orientation = slerp(from, to, t);
    if (f.orientation[0] == 0.0f || f.orientation[1] == 0.0f || f.orientation[2] == 0.0f ||
        f.orientation[3] == 0.0f) {
        Quat q = slerp(from, to, t);
        const double size = std::sqrt(static_cast<double>(q[3]) * q[3] + static_cast<double>(q[2]) * q[2] +
                                        static_cast<double>(q[1]) * q[1] + static_cast<double>(q[0]) * q[0]);
        const double inverse = 1.0 / (size + 1.1920929e-07);
        for (float& v : q) v = static_cast<float>(v * inverse);
        f.orientation = q;
    }
    const ReplayFrame& a = previous[car];
    const ReplayFrame& b = next[car];
    if (anyNonZero(before[car]) && anyNonZero(after[car])) {
        f.position = catmullRom(before[car], a.position, b.position, after[car], t);
    } else {
        f.position[0] = lerpOnce(a.position[0], b.position[0], t);
        f.position[1] = lerpOnce(a.position[1], b.position[1], t);
        f.position[2] = lerpOnce(a.position[2], b.position[2], t);
    }
    for (size_t w = 0; w < 4; ++w) f.wheels[w] = wheelBlend(a.wheels[w], b.wheels[w], t);
    f.steering = lerpOnce(a.steering, b.steering, t);
    f.rpm = lerpOnce(a.rpm, b.rpm, t);
    f.throttle = lerpOnce(a.throttle, b.throttle, t);
    const ReplayFrame& held = t >= 0.5f ? b : a;
    f.brake = held.brake;
    f.nitro = held.nitro;
    f.gear = held.gear;
    f.horn = a.horn;
    f.engineSound = lerpOnce(a.engineSound, b.engineSound, t);
    f.parts = a.parts;
    return f;
}

bool Replay::read(uint8_t* out, uint32_t ahead) const {
    if (block == last && frame >= lastFrames) return false;
    uint32_t b = block;
    uint32_t f = frame + ahead;
    if (f >= kFrames) {
        b = blockNext(b);
        f = f - kFrames;
        if (b >= kBlocks) return false;
    }
    std::memcpy(out, blockAt(b) + 0x0C + static_cast<size_t>(f) * 0x21, 0x21);
    return true;
}

bool Replay::readFrame(ReplayFrame& f, uint32_t ahead) const {
    uint8_t raw[0x21];
    if (!read(raw, ahead)) return false;
    f = unpackFrame(raw);
    return true;
}

bool Replay::readRaw(uint8_t* out, size_t size, uint32_t ahead) const {
    uint8_t raw[0x21];
    if (!read(raw, ahead)) return false;
    std::memcpy(out, raw, size);
    return true;
}

void Replay::advance() {
    if (last == 0 && frame == 0) {
        const float span = frameTime(block, lastFrames - 1) - frameTime(block, 0);
        fadeLength = span >= kFadeLength ? kFadeLength : span;
        fadeDue = 1;
        fadeWait = span - kFadeLength;
    }
    if (frame >= kFrames) {
        const uint32_t was = last;
        block = blockNext(block);
        frame = 0;
        const bool joined = block != was && block < kBlocks && blockNext(block) < kBlocks &&
                            blockNumber(block) + 1 == blockNumber(blockNext(block));
        if (!joined && block < kBlocks) {
            fadeDue = 1;
            const float start = frameTime(block, 0);
            float end;
            if (header.type == 4) {
                end = block == was ? frameTime(block, lastFrames - 2) : frameTime(block, kFrames - 2);
            } else {
                end = block == was ? frameTime(block, lastFrames - 1) : frameTime(block, kFrames - 1);
            }
            const float span = end - start;
            fadeLength = span >= kFadeLength ? kFadeLength : span;
            fadeWait = span - kFadeLength;
            if (block != was && blockNext(block) < kBlocks) nextTime = frameTime(blockNext(block), 0);
        }
    }
    frame = frame + 1;
}

void Replay::write(const uint8_t* bytes) {
    if (blockOpen == 0) {
        uint8_t* b = blockAt(block);
        putU32(b, sequence);
        putU32(b + 4, kNoBlock);
        if (block == 0) {
            putU32(buffer.data() + 8, 0);
            g_blockInterest = 0.0f;
        } else {
            const auto v = static_cast<float>(static_cast<double>(g_blockInterest) * 0.6000000238418579);
            std::memcpy(b + 8, &v, 4);
        }
        blockOpen = 1;
    }
    if (frame >= kFrames) {
        uint8_t* b = blockAt(block);
        const float v = f32At(b + 8) + interest;
        std::memcpy(b + 8, &v, 4);
        g_blockInterest = v;
        // The marks' lines are made into texts and dropped (their debug output not followed).
        interest = 0.0f;
        interestLines = 0;
        sequence = sequence + 1;
        const uint32_t to = nextBlock();
        putU32(blockAt(block) + 4, to);
        written = written + 1;
        blockOpen = 0;
        block = to;
        frame = 0;
    }
    std::memcpy(blockAt(block) + 0x0C + static_cast<size_t>(frame) * 0x21, bytes, 0x21);
    frame = frame + 1;
}

uint32_t Replay::nextBlock() {
    if (block + 1 < kBlocks && sequence < kBlocks) return block + 1;
    uint32_t v = g_ringStarted ? g_ringNext : cars;
    g_ringStarted = true;
    const uint32_t chosen = v;
    v = v + 1;
    g_ringNext = v < kBlocks ? v : cars;
    return chosen;
}

void Replay::clear() {
    std::fill(buffer.begin(), buffer.end(), uint8_t{0});
    saved = 0;
}

bool Replay::stop() {
    if (state != ReplayState::Recording) {
        state = ReplayState::Stopped;
        return false;
    }
    last = block;
    lastFrames = frame;
    length = time;
    sortBlocks();
    sequence = 0;
    frame = 0;
    block = 0;
    header.lastBlock = last;
    header.lastFrames = lastFrames;
    header.length = length;
    state = ReplayState::Stopped;
    return true;
}

bool Replay::play() {
    marks = {};
    if (written == 0 && header.lastBlock != 0) return false;
    state = ReplayState::Playing;
    for (size_t i = 0; i < 6; ++i) {
        before[i] = {};
        after[i] = {};
        previous[i].position = {};
        next[i].position = {};
    }
    replayRace = 1;
    reset();
    return true;
}

void Replay::resume() {
    fast = 0;
    if (ended == 0) {
        state = ReplayState::Playing;
        timeScale = 1.0f;
        return;
    }
    restart();
}

void Replay::restart() {
    fast = 0;
    timeScale = 1.0f;
    play();
}

bool Replay::fastForward(float speed) {
    timeScale = 1.0f;
    fast = fast == 0 ? 1 : 0;
    if (fast != 0) fastSpeed = speed;
    return fast != 0;
}

void Replay::slow() {
    fast = 0;
    if (state == ReplayState::Playing) timeScale = 0.25f;
}

float Replay::progress() const {
    const float v = time / length;
    return 1.0f <= v ? 1.0f : v;
}

void Replay::sortBlocks() {
    const uint32_t lastNumber = blockNumber(last);
    for (uint32_t b = 1; b < kBlocks; ++b) {
        if (blockNumber(b) == 0) putU32(blockAt(b), kNoBlock);
    }
    std::vector<std::array<uint8_t, kBlockSize>> blocks(kBlocks);
    for (uint32_t b = 0; b < kBlocks; ++b) std::memcpy(blocks[b].data(), blockAt(b), kBlockSize);
    std::stable_sort(blocks.begin(), blocks.end(), [](const auto& a, const auto& b) {
        return static_cast<int32_t>(u32At(a.data()) - u32At(b.data())) < 0;
    });
    for (uint32_t b = 0; b < kBlocks; ++b) {
        std::memcpy(blockAt(b), blocks[b].data(), kBlockSize);
        putU32(blockAt(b) + 4, b + 1);
    }
    last = lastNumber == last ? lastNumber : 0x2F;
}

bool Replay::readCars(size_t* used) {
    savedCars.clear();
    size_t at = 0;
    bool ok = true;
    for (uint32_t i = 0; i < header.cars; ++i) {
        Car c;
        if (!c.read(carsStream, at)) {
            ok = false;
            break;
        }
        savedCars.push_back(std::move(c));
    }
    if (used != nullptr) *used = at;
    return ok;
}

bool Replay::load(const std::vector<uint8_t>& hdr, const std::vector<uint8_t>& dat) {
    saved = 1;
    if (hdr.size() < ReplayHeader::kSize) return false;
    header = ReplayHeader::read(hdr.data());
    carsStream.assign(hdr.begin() + ReplayHeader::kSize, hdr.end());
    if (!readCars()) return false;
    if (dat.size() < header.bytes || header.bytes > buffer.size()) return false;
    std::copy(dat.begin(), dat.begin() + header.bytes, buffer.begin());
    // 0x004269A0: the race's type, laps, track, route, time of day and weather from the header (to the caller); its
    // cars, end, last block.
    cars = header.cars;
    length = header.length;
    last = header.lastBlock;
    lastFrames = header.lastFrames;
    written = header.lastBlock;
    saved = 1;
    loaded = 1;
    return play();
}

bool Replay::saveTo(save::SaveSource& source, const std::string& date, const std::wstring& driver) {
    source.files["USERDATA"] = replayUserData(header, date, driver);
    header.bytes = bytes();
    source.files["replay.dat"].assign(buffer.begin(), buffer.begin() + header.bytes);
    header.carsBytes = static_cast<uint32_t>(carsStream.size());
    const std::array<uint8_t, ReplayHeader::kSize> head = header.write();
    std::vector<uint8_t>& hdr = source.files["replay.hdr"];
    hdr.assign(head.begin(), head.end());
    hdr.insert(hdr.end(), carsStream.begin(), carsStream.end());
    return true;
}

bool Replay::loadFrom(const save::SaveSource& source) {
    saved = 1;
    const auto hdr = source.files.find("replay.hdr");
    if (hdr == source.files.end()) return false;
    const std::vector<uint8_t>& file = hdr->second;
    std::array<uint8_t, ReplayHeader::kSize> head = header.write();
    const size_t got = std::min(file.size(), ReplayHeader::kSize);
    std::memcpy(head.data(), file.data(), got);
    header = ReplayHeader::read(head.data());
    carsStream.assign(file.begin() + static_cast<std::ptrdiff_t>(got), file.end());
    size_t used = 0;
    readCars(&used);
    carsStream.resize(used);
    const auto dat = source.files.find("replay.dat");
    if (dat == source.files.end()) return false;
    // The header's bytes read into the buffer (Stock problem: more than its 48 blocks run past it; cut to it here).
    const size_t n = std::min({static_cast<size_t>(header.bytes), dat->second.size(), buffer.size()});
    std::copy_n(dat->second.begin(), n, buffer.begin());
    // 0x004269A0: the replay's part of the header.
    cars = header.cars;
    length = header.length;
    last = header.lastBlock;
    lastFrames = header.lastFrames;
    saved = 1;
    loaded = 0;
    written = header.lastBlock;
    play();
    return true;
}

uint32_t Replay::bytes() const {
    return static_cast<uint32_t>((written > kBlocks ? kBlocks : written) * kBlockSize);
}

}  // namespace oj
