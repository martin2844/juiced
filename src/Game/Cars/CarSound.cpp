#include "Game/Cars/CarSound.h"

#include "Core/Cars/Car.h"
#include "Core/Physics/CarBody.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Random.h"
#include "Core/Race/Racer.h"
#include "Core/Base/X87.h"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace oj {

namespace {

constexpr float kRpm = 9.549296f;          // [0x006FF990]: rad/s to rpm
constexpr float kEngineRolloff = 0.75f;    // [0x00768964] .. [0x00768974] (from [0x0071787C])
constexpr float kEngineDistance = 1000.0f;  // [0x00768960] (from [0x00717880])
constexpr float kLow = 0.4f;               // [0x0071795C]

// 0x00554740: a value of a curve of n knots (clamped to its ends; a knot's twin gives the left value).
float curve(const float* values, const float* knots, int n, float x) {
    const float v = x < knots[0] ? knots[0] : (x > knots[n - 1] ? knots[n - 1] : x);
    int i = 0;
    while (i < n - 2 && !(v <= knots[i + 1])) ++i;
    if (knots[i + 1] == knots[i]) return values[i] + (values[i + 1] - values[i]) * 0.0f;
    return values[i] + (v - knots[i]) / (knots[i + 1] - knots[i]) * (values[i + 1] - values[i]);
}

// 0x00407850
float sign(float x) {
    return x < 0.0f ? -1.0f : (x > 0.0f ? 1.0f : 0.0f);
}

}  // namespace

CarSound::VolTables& CarSound::tablesOf(const CarDef* def) {
    static std::map<const CarDef*, VolTables> tables;
    return tables[def];
}

audio::Instance* CarSound::make(const char* name) {
    audio::Instance* v = m_world.create(name, nullptr, 0);
    if (v != nullptr) v->attachTo(m_masterInstance);
    return v;
}

CarSound::CarSound(audio::World& world, const GameFiles& files, RaceCar& veh, const Car& rec,
                   const PartCatalogue& catalogue, const void* node, int nearest, bool automod, long automodValue)
    : m_world(world), m_veh(veh), m_rec(rec) {
    std::string name = rec.def != nullptr ? rec.def->engineSound : std::string();
    if (rec.mods[kExhaust] != kNoMod) name += "b";
    m_vol = &tablesOf(rec.def);
    const CarDynamics& dyn = veh.body->dynamics();
    m_idle = (dyn.torqueSpeeds.empty() ? 0.0f : dyn.torqueSpeeds[0]) * kRpm;
    m_max = (dyn.torqueSpeeds.empty() ? 0.0f : dyn.torqueSpeeds.back()) * kRpm + 500.0f;  // [0x006FFBE0]
    m_clutchShare = 0.25f;                                                                 // [0x00717914]
    m_forwardGears = static_cast<uint32_t>(dyn.forwardRatios.size());
    m_master = 1.0f;     // [0x00717910]
    m_offFactor = 1.0f;
    m_dipScale = (dyn.powerRangeMax - dyn.powerRangeMin) / -200.0f;  // [0x0071788C]
    m_splutterChance = 0.8f;  // [0x007178F0]
    m_voices.reserve(0x11);
    m_splutterCooldown = 0.001f;

    // The master: no set, on the car's node 90 up, played.
    m_masterInstance = world.manager().allocate(0, nullptr);
    if (m_masterInstance == nullptr) return;
    m_masterInstance->setNode(node);
    m_masterInstance->setVolume(1.0f);
    m_masterInstance->setRolloff(1.0f);
    m_masterInstance->setOffset({0.0f, 90.0f, 0.0f, 0.0f});  // [0x007A28BC]
    m_masterInstance->setSet(-1, -1);
    m_masterInstance->play();
    m_masterInstance->setNearest(nearest);

    // "%s_%s" of the names [0x0071792C]: idle, min, low, mid, high.
    static const char* const kNames[5] = {"idle", "min", "low", "mid", "high"};
    char text[128];
    std::snprintf(text, sizeof(text), "%s_%s", name.c_str(), kNames[0]);
    if (audio::Instance* v = make(text)) {
        m_voices.push_back(v);
        v->setLooped(1);
        v->setRolloff(kEngineRolloff);
        v->setVolume(1.5f);  // [0x007178D0]
        v->setMinDistance(kEngineDistance);
        v->play();
        m_on[0] = v;
        v->addRef();
        m_slots += 1;
        m_off[0] = v;
    }
    for (int j = 0; j <= 1; ++j) {
        // An "_off" without its "_on" shifts the off slots (on and off are filled each on its own; stock).
        int k = 1;
        for (int n = 1; n <= 4; ++n) {
            std::snprintf(text, sizeof(text), j == 0 ? "%s_%s_on" : "%s_%s_off", name.c_str(), kNames[n]);
            audio::Instance* v = make(text);
            if (v == nullptr) continue;
            m_voices.push_back(v);
            if (j == 0) m_slots += 1;
            v->setLooped(1);
            v->setRolloff(kEngineRolloff);
            v->setVolume(1.5f);
            v->setMinDistance(kEngineDistance);
            v->play();
            if (k < 5) (j == 0 ? m_on : m_off)[static_cast<size_t>(k)] = v;
            ++k;
        }
    }
    // The exhaust's level over 1 (sports and up) splutters.
    if (rec.mods[kExhaust] != kNoMod) {
        const std::vector<CarPart>& list = catalogue.lists[kExhaust].parts;
        if (!list.empty()) {
            const CarPart& p = rec.mods[kExhaust] < list.size() ? list[rec.mods[kExhaust]] : list[0];
            m_splutterOn = 1 < static_cast<int8_t>(p.level) ? 1 : 0;
        }
    }
    if (m_splutterOn != 0) {
        for (int i = 1; i <= 3; ++i) {
            std::snprintf(text, sizeof(text), "splutter%d", i);
            audio::Instance* v = make(text);
            m_splutters[static_cast<size_t>(i - 1)] = v;
            if (v == nullptr) continue;  // the stock goes on with none (the sets are in audio\3d)
            v->setLooped(0);
            v->setVolume(1.0f);         // [0x007178F4]
            v->setRolloff(0.07f);       // [0x007178E8]
            v->setMinDistance(150.0f);  // [0x007178EC]
            m_voices.push_back(v);
        }
    }
    for (int t = 0; t < 2; ++t) {
        audio::Instance* v = make(t == 0 ? "trans_on" : "trans_off");
        (t == 0 ? m_transOn : m_transOff) = v;
        if (v == nullptr) continue;
        v->setLooped(1);
        v->setRolloff(kEngineRolloff);
        v->setVolume(0.0f);
        v->setMinDistance(350.0f);  // [0x0071789C]
        v->play();
        m_voices.push_back(v);
    }
    loadTables(files);
    m_nitroOff = 0.0f;
    m_nitroRpm = 0.0f;
    rebuildKnots();

    if (veh.hasTurbo) {
        m_turboWhine = make("turbowhine");
        if (m_turboWhine != nullptr) {
            m_turboWhine->setLooped(1);
            m_turboWhine->setRolloff(kEngineRolloff);
            m_turboWhine->setMinDistance(250.0f);  // [0x00717890]
            m_turboWhine->setVolume(1.5f);
            m_turboWhine->play();
            m_voices.push_back(m_turboWhine);
        }
        if (automod) {
            if (automodValue != 0) m_dumpLevel = static_cast<uint32_t>(automodValue);
        } else if (rec.mods[kTurbo] != kNoMod) {
            const std::vector<CarPart>& list = catalogue.lists[kTurbo].parts;
            if (!list.empty()) {
                const CarPart& p = rec.mods[kTurbo] < list.size() ? list[rec.mods[kTurbo]] : list[0];
                m_dumpLevel = static_cast<uint32_t>(p.level + 1);
            }
        } else {
            m_dumpLevel = 0;
        }
        if (m_dumpLevel > 4) m_dumpLevel = 4;
        if (m_dumpLevel != 0) {
            std::snprintf(text, sizeof(text), "dump%u", m_dumpLevel);
            for (int t = 0; t < 2; ++t) {
                audio::Instance* v = make(t == 0 ? text : "dumpmed");
                (t == 0 ? m_dump : m_dumpMed) = v;
                if (v != nullptr) {
                    v->setLooped(0);
                    v->setRolloff(kEngineRolloff);
                    v->setVolume(1.5f * 0.75f);
                    v->setDoppler(0.0f);
                    v->setMinDistance(250.0f);
                    m_voices.push_back(v);
                }
                if (t == 0) m_dumpVolume = 0.64f;  // [0x007178F8]
            }
        }
        m_dumpArmed = 0;
    }
    m_gearUp = make("gearup");
    if (m_gearUp != nullptr) {
        m_gearUp->setLooped(0);
        m_gearUp->setRolloff(kEngineRolloff);
        m_gearUp->setVolume(1.0f);       // [0x007178A0]
        m_gearUp->setMinDistance(75.0f);  // [0x00717894]
        m_voices.push_back(m_gearUp);
    }
    m_gearDown = make("geardown");
    if (m_gearDown != nullptr) {
        m_gearDown->setLooped(0);
        m_gearDown->setVolume(1.0f);
        m_gearDown->setRolloff(kEngineRolloff);
        m_gearDown->setMinDistance(75.0f);
        m_voices.push_back(m_gearDown);
    }
    if (veh.hasNitro) {
        for (int t = 0; t < 2; ++t) {
            audio::Instance* v = make(t == 0 ? "nitrostart" : "nitroloop");
            (t == 0 ? m_nitroStart : m_nitroLoop) = v;
            if (v == nullptr) continue;
            v->setLooped(t);
            v->setVolume(1.0f);          // [0x007178E4]
            v->setRolloff(kEngineRolloff);
            v->setMinDistance(200.0f);   // [0x007178CC]
            m_voices.push_back(v);
        }
    }
    for (audio::Instance* v : m_voices) {
        v->setNearest(nearest);
        v->setRolloffScale(2.0f);  // [0x00717904]
        v->setGroup(2);
    }
    m_nitroTimer = 0.0f;
}

CarSound::~CarSound() {
    for (uint32_t k = 0; k < m_slots && k < 5; ++k) {
        if (m_on[k] != nullptr) m_on[k]->release();
        if (m_off[k] != nullptr) m_off[k]->release();
    }
    for (audio::Instance* v : {m_gearDown, m_gearUp, m_turboWhine, m_dump, m_dumpMed, m_nitroStart, m_nitroLoop,
                               m_transOn, m_transOff, m_splutters[0], m_splutters[1], m_splutters[2],
                               m_masterInstance}) {
        if (v != nullptr) v->release();
    }
}

void CarSound::loadTables(const GameFiles& files) {
    std::string name = m_rec.def != nullptr ? m_rec.def->engineSound : std::string();
    if (m_rec.mods[kExhaust] != kNoMod) name += "b";
    const std::filesystem::path dir = std::filesystem::path("audio") / "cars" / name;
    const size_t slots = m_slots < 5 ? m_slots : 5;
    if (const auto rev = files.read(dir / (name + ".rev"))) {
        size_t at = 0;
        for (auto& table : m_rev) {
            const size_t n = slots * sizeof(Rev);
            if (at + n > rev->size()) break;
            std::memcpy(table.data(), rev->data() + at, n);
            at += n;
        }
    }
    if (const auto vol = files.read(dir / (name + ".vol"))) {
        size_t at = 0;
        for (auto& table : *m_vol) {
            const size_t n = slots * sizeof(Vol);
            if (at + n > vol->size()) break;
            std::memcpy(table.data(), vol->data() + at, n);
            at += n;
        }
    }
    for (auto& table : m_rev) {
        for (size_t i = 0; i < slots; ++i) table[i].c = 48000.0f / table[i].c;  // [0x006FFCA0]
    }
}

void CarSound::rebuildKnots() {
    const int s = static_cast<int>(m_slots < 5 ? m_slots : 5);
    if (s == 0) return;
    for (int t = 0; t < 3; ++t) {
        auto& r = m_rev[static_cast<size_t>(t)];
        auto& v = (*m_vol)[static_cast<size_t>(t)];
        if (s < 2) continue;  // the stock reads before the table (no stock car has one slot)
        int d = x87::chop(r[0].b - r[1].a);
        if (d > 0) d >>= 1;
        v[0].x[0] = r[0].a;
        v[0].x[1] = r[0].a;
        if (t != 2 && d < 0) {
            r[0].b = r[1].a;
            d = 0;
        }
        v[0].x[2] = static_cast<float>(d) + r[1].a;
        v[0].x[3] = r[0].b;
        int k = 1;
        if (t != 2) {
            for (int i = 1; i < s - 1; ++i) {
                v[i].x[0] = r[i].a;
                v[i].x[1] = r[i - 1].b - static_cast<float>(d);
                int e = x87::chop(r[i].b - r[i + 1].a);
                if (e < 0) {
                    r[i].b = r[i + 1].a;
                    e = 0;
                }
                d = e >> 1;
                v[i].x[2] = static_cast<float>(d) + r[i + 1].a;
                v[i].x[3] = r[i].b;
            }
            k = s - 1;
        }
        v[k].x[0] = r[k].a;
        v[k].x[1] = r[k - 1].b - static_cast<float>(d);
        v[k].x[2] = r[k].b;
        v[k].x[3] = r[k].b;
    }
}

void CarSound::frame(const Frame& f, CrtRandom& random) {
    CarBody& body = *m_veh.body;
    if (!m_focus) {
        m_master = 1.5f;  // [0x007178FC]
    } else {
        m_master = 1.0f;  // [0x00717910]
    }
    m_slippingBefore = m_slipping;
    m_slipping = (body.engineOn && !body.clutchIn) ? 1 : 0;
    m_rpm = body.engineSpeed * kRpm;
    const float thr = body.throttle;
    if (thr != 0.0f) m_blipTracking = 0;
    if (m_blipTracking != 0 && thr == 0.0f) {
        if (m_blipRpm == 0.0f) {
            m_blipRpm = m_rpm;
        } else if (m_blipRpm > m_rpm) {
            m_blipTracking = 0;
            m_blip = 1.25f;  // [0x007178B0]
        } else {
            m_blip += x87::asUnsigned(body.gear) * 0.5f;  // [0x007178B8]
            if (m_blip > 1.25f) m_blip = 1.25f;
            m_blipRpm = m_rpm;
        }
    } else if (m_blip > 0.0f && thr == 0.0f) {
        m_blip -= 0.5f * f.dt;  // [0x007178B4]
        if (m_blip < 0.0f) m_blip = 0.0f;
    }
    if (m_offFactor > 1.0f) {
        m_offFactor -= 0.5f * f.dt;  // [0x007178C0]
        if (m_offFactor < 1.0f) m_offFactor = 1.0f;
    }
    if (body.asrActive) {
        const float diff = thr - m_throttle;
        if (std::fabs(diff) > 0.5f) m_throttle += 2.0f * f.dt * diff;  // [0x00725538], [0x00717944] / [0x00717940]
    } else {
        m_throttle = thr;
    }
    m_throttle += m_blip;
    if (m_throttle < 0.0f) m_throttle = 0.0f;
    if (m_throttle > 1.0f) m_throttle = 1.0f;
    if (m_focus != 0 && m_splutterOn != 0) splutter(f, random);
    m_throttleMean = (thr + m_throttle) * 0.5f;
    m_clutchShare = 0.25f;  // [0x00717900] / [0x00717914]
    volumes(f);
    transmission();
    pitch(f, random);
    turbo(random);
    nitro(f);
}

void CarSound::volumes(const Frame& f) {
    weights();
    loadMix(f);
    if (m_on[0] != nullptr) m_on[0]->setVolume(m_weights[0] * m_master);
    for (uint32_t k = 1; k < m_slots && k < 5; ++k) {
        if (m_on[k] != nullptr) m_on[k]->setVolume(m_weights[k] * m_master * m_onWeight);
        if (m_off[k] != nullptr) m_off[k]->setVolume(m_weights[k] * m_master * m_offWeight);
    }
}

void CarSound::weights() {
    const float r = m_rpmNitro;
    const uint32_t slots = m_slots < 5 ? m_slots : 5;
    if (slots == 0) return;
    for (uint32_t k = 0; k < slots; ++k) m_weights[k] = 0.0f;
    const auto& vol = (*m_vol)[0];
    for (uint32_t i = 0; i + 1 < slots; ++i) {
        if (r < m_rev[0][i].b) {
            m_weights[i] = curve(vol[i].y, vol[i].x, 4, r);
            m_weights[i + 1] = curve(vol[i + 1].y, vol[i + 1].x, 4, r);
            return;
        }
    }
    m_weights[slots - 1] = vol[slots - 1].y[1];
}

void CarSound::loadMix(const Frame& f) {
    if (f.replayRace) {
        m_load = f.replayLoad;
    } else {
        m_loadTarget = m_throttle;
        // [0x0075EF3C] is never set: the load by the rpm's change is not reached.
        const float clutch = m_veh.body->clutch;
        if (m_veh.onStand || !f.raceState || f.count <= 0.0f) {
            if (m_slipping != 0 && m_downShifted == 0) m_loadTarget = m_clutchShare * m_loadTarget + clutch * 0.25f;
        }
        if (m_loadTarget < kLow) m_loadTarget = kLow;
        const float d = (m_loadTarget - m_load) * f.dt;
        m_load += d > 0.0f ? d * 4.0f : d * 130.0f;  // [0x00717960], [0x00717964]
        m_loadTarget = m_loadTarget < kLow ? kLow : (m_loadTarget > 1.0f ? 1.0f : m_loadTarget);
        m_load = m_load < kLow ? kLow : (m_load > m_loadTarget ? m_loadTarget : m_load);
    }
    const float x = m_load * 10000.0f;  // [0x006FF784]
    const auto& vol = (*m_vol)[2];
    m_offWeight = curve(vol[0].y, vol[0].x, 4, x);
    m_onWeight = curve(vol[1].y, vol[1].x, 4, x);
    if (m_focus == 0) {
        if (m_offWeight > 0.5f) {
            m_onWeight = 0.0f;
        } else {
            m_offWeight = 0.0f;
        }
    }
}

void CarSound::transmission() {
    CarBody& body = *m_veh.body;
    float g = (*m_vol)[2][2].y[0];
    if (g == 0.0f) g = 0.64f;  // [0x007178F8]
    const float base = 0.25f * g;  // [0x00725530]
    CarWheel* w2 = body.wheelOf(2);
    CarWheel* w3 = body.wheelOf(3);
    const float avg = ((w2 != nullptr ? w2->spin : 0.0f) + (w3 != nullptr ? w3->spin : 0.0f)) * 0.5f;
    float lvl = std::fabs(avg) - 40.0f;  // [0x00725528]
    lvl = lvl < 0.0f ? 0.0f : (lvl > 5.0f ? 5.0f : lvl);
    if (m_transOff == nullptr || m_transOn == nullptr) return;
    float k = 0.0f;
    if (m_focus != 0) {
        const uint32_t f = static_cast<uint32_t>(x87::chop(std::fabs(avg) * 250.0f + 48000.0f));  // [0x006FFC44]
        m_transOn->setFrequency(f);
        m_transOff->setFrequency(f);
        const float s = (sign(avg) < 0.0f && body.gear == 0) ? 2.0f * base : base;  // [0x00725524]
        k = s * (lvl / 10.0f);  // [0x0072552C]
    }
    m_transOn->setVolume(m_onWeight * m_master * k);
    m_transOff->setVolume(m_offFactor * m_offWeight * m_master * k);
}

void CarSound::pitch(const Frame& f, CrtRandom& random) {
    CarBody& body = *m_veh.body;
    m_rpm = m_nitroRpm + m_rpm;
    m_rpmBefore = m_rpmNitro;
    m_rpmNitro = m_rpm;
    const uint32_t g = body.gear;
    if (m_slippingBefore == 1 && m_slipping == 0 && m_downShifted == 0) {
        m_dipFalling = 1;
        m_dipRising = 0;
        m_dipsLeft = g < m_forwardGears - 1 ? 1 : 0;
        m_dipDelay = 0.15f;  // [0x007178A4]
        m_dip = 0.0f;
        m_dipTarget = (x87::unit(random.next()) + 1.5f) * m_dipScale;  // [0x006FF748]
    }
    if (m_dipFalling != 0) {
        m_dipDelay -= f.dt;
        if (m_dipDelay < 0.0f) {
            if (m_dip > m_dipTarget) {
                m_dip -= 8000.0f * f.dt;  // [0x007178A8]
            } else {
                m_dipFalling = 0;
                m_dipRising = 1;
            }
        }
    }
    if (m_dipRising != 0) {
        if (m_dip < 0.0f) {
            m_dip += 1000.0f * f.dt;  // [0x007178AC]
        } else {
            m_dip = 0.0f;
            m_dipRising = 0;
            if (m_dipsLeft > 0) {
                m_dipFalling = 1;
                // [0x006FFB88]: 1 / 65534
                const float half = std::bit_cast<float>(0x37800100u);
                m_dipTarget = (static_cast<float>(random.next()) * half + 0.5f) * m_dipScale;
                m_dipsLeft -= 1;
            }
        }
    }
    m_rpm = m_dip + m_rpm;
    m_rpm = m_rpm < m_idle ? m_idle : (m_rpm > m_max ? m_max : m_rpm);
    // [0x0079E7E8] is never set: the pitch's curve is not reached.
    const float p = m_rpm;
    for (uint32_t i = 0; i < m_slots && i < 5; ++i) {
        const uint32_t hz = static_cast<uint32_t>(x87::chop(p * m_rev[0][i].c));
        if (m_on[i] != nullptr) m_on[i]->setFrequency(m_base + hz);
        if (m_off[i] != nullptr) m_off[i]->setFrequency(m_base + hz);
    }
}

void CarSound::turbo(CrtRandom& random) {
    const float b0 = m_veh.body->turboBoost();
    if (m_turboWhine == nullptr) {
        dump(false, random);
        return;
    }
    float g = 0.0f;
    if (m_focus != 0) {
        float v = b0 * b0;
        v = v < 0.0f ? 0.0f : (v > 0.8f ? 0.8f : v);  // [0x006FF968]
        m_turboWhine->setFrequency(static_cast<uint32_t>(x87::chop(v * 110000.0f) + 22000));  // [0x006FFC9C]
        g = m_veh.body->turboBoost() * 0.15f;  // [0x007178D4]
        if (g > 0.05f) g = 0.05f;              // [0x007178D8]
    }
    m_turboWhine->setVolume(g * m_master);
    dump(false, random);
}

void CarSound::dump(bool force, CrtRandom& random) {
    if (!m_veh.hasTurbo) return;
    const float b = m_veh.body->turboBoost();
    if (force || m_throttle == 0.0f) {
        if (b > 0.5f && m_dumpArmed != 0) {  // [0x0072551C]
            audio::Instance* v = b < 0.85f ? m_dumpMed : m_dump;  // [0x00725518]
            if (v != nullptr) {
                const int32_t hz = x87::chop(x87::unit(random.next()) * 2000.0f - 1000.0f) + 48000;
                v->setVolume(m_master * m_dumpVolume * 1.0f);  // [0x00725520]
                v->setRolloff(kEngineRolloff);
                v->setMinDistance(500.0f);  // [0x00717898]
                v->play();
                v->setFrequency(static_cast<uint32_t>(hz));
                m_dumpArmed = 0;
            }
        }
    }
    if (m_throttle > 0.2f) m_dumpArmed = 1;  // [0x006FF934]
}

void CarSound::splutter(const Frame& f, CrtRandom& random) {
    const float r1 = x87::unit(random.next());
    // [0x0079E7E4] is never set: the splutter always takes its chance.
    if (m_throttleMean > m_throttle && m_throttle == 0.0f && m_rpm > 3000.0f && r1 > m_splutterChance &&
        m_splutterTimer < 0.0f) {
        const int i = random.next() & 1;  // splutter3 never
        audio::Instance* v = m_splutters[static_cast<size_t>(i)];
        if (v != nullptr && !v->sounding()) {
            v->setRolloff(0.07f);
            v->setMinDistance(150.0f);
            v->setLooped(0);
            v->play();
            m_splutterTimer = m_splutterCooldown;
        }
    }
    m_splutterTimer -= f.dt;
}

void CarSound::nitro(const Frame& f) {
    if (m_nitroStart == nullptr) return;
    if (!m_veh.nitroAsked) {
        m_nitroRpm = m_nitroOff;
        if (m_nitroLoop != nullptr) m_nitroLoop->stop();
        m_nitroStart->stop();
        return;
    }
    m_nitroRpm = 20.0f;  // [0x00725510]
    if (m_veh.nitroFlash != 0) {
        m_nitroStart->setRolloff(kEngineRolloff);
        m_nitroStart->setVolume(1.0f * m_master);  // [0x007178C4]
        m_nitroStart->setMinDistance(200.0f);
        m_nitroStart->play();
        m_nitroTimer = 0.2f;  // [0x00725514]
    } else {
        m_nitroTimer -= f.dt;
        if (m_nitroTimer <= 0.0f && m_nitroLoop != nullptr) {
            m_nitroLoop->setRolloff(kEngineRolloff);
            m_nitroLoop->setVolume(1.0f * m_master);  // [0x007178C8]
            m_nitroLoop->setMinDistance(200.0f);
            m_nitroLoop->play();
        }
    }
}

void CarSound::gearUp(const Frame& f, CrtRandom& random) {
    (void)f;
    m_offFactor = 1.0f;  // [0x007178BC]
    dump(true, random);
    if (m_veh.body->gear > 2) m_downShifted = 0;
    if (m_gearUp != nullptr && m_focus != 0) m_gearUp->play();
}

void CarSound::gearDown() {
    m_downShifted = 1;
    m_blipTracking = 1;
    m_blipRpm = 0.0f;
    if (m_gearDown != nullptr && m_focus != 0) m_gearDown->play();
}

void CarSound::setRolloffScale(float v, audio::Instance* squeal) {
    for (audio::Instance* i : m_voices) i->setRolloffScale(v);
    if (squeal != nullptr) squeal->setRolloffScale(v);
}

void CarSound::setNode(const void* node) {
    for (audio::Instance* i : m_voices) i->setNode(node);
}

void CarSound::setOcclusion(float v) {
    for (audio::Instance* i : m_voices) i->setOcclusion(v);
}

void CarSound::minDistances(float d) {
    for (size_t k = 0; k < 5; ++k) {
        if (m_on[k] != nullptr) m_on[k]->setMinDistance(d);
        if (m_off[k] != nullptr) m_off[k]->setMinDistance(d);
    }
}

void CarSound::setFocus(bool on, audio::Instance* road, audio::Instance* horn,
                        std::array<audio::Instance*, 2> holders, audio::Instance* squeal) {
    // 0x00492820: the effects' sound holders by their priority and group.
    auto holdersTo = [&holders](float priority, int group) {
        for (audio::Instance* h : holders) {
            if (h == nullptr) continue;
            h->setPriority(priority);
            h->setGroup(group);
        }
    };
    m_focus = on ? 1 : 0;
    if (on) {
        setRolloffScale(1.0f, squeal);  // [0x00717908]
        for (audio::Instance* v : m_voices) {
            v->setPriority(200.0f);  // [0x00717888]
            v->setGroup(1);
            v->setOcclusionOn(1);
        }
        holdersTo(200.0f, 1);  // [0x00717888]
        if (road != nullptr) road->play();
        m_master = 1.5f;              // [0x007178FC]
        minDistances(1000.0f);        // [0x00717880]
        if (horn != nullptr) horn->setPriority(200.0f);
        m_clutchShare = 0.25f;        // [0x00717900]
    } else {
        setRolloffScale(2.0f, squeal);  // [0x00717904]
        for (audio::Instance* v : m_voices) {
            v->setPriority(1.0f);
            v->setGroup(2);
            v->setOcclusionOn(0);
        }
        minDistances(350.0f);  // [0x00717884]
        holdersTo(1.0f, 2);
        if (road != nullptr) road->stop();
        if (horn != nullptr) horn->setPriority(0.75f);
        m_master = 1.0f;
        m_clutchShare = 0.25f;  // [0x00717914]
    }
}

void CarSound::reset() {
    m_blipTracking = 0;
    m_downShifted = 0;
    m_slippingBefore = 0;
    m_dipRising = 0;
    m_dipFalling = 0;
    m_dip = 0.0f;
    m_throttle = 0.0f;
    m_throttleMean = 0.0f;
    m_loadTarget = 0.0f;
    m_onWeight = 0.0f;
    m_offWeight = 0.0f;
}

}  // namespace oj
