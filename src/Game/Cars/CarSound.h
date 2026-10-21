#pragma once
// A race car's engine sounds (0x00492D20 → 0x00492DC0, 0x268 bytes, vtable 0x006F3E70; the vehicle's +0x498;
// docs/spec/26-sound.md "The engine"): a master instance on the car's node 90 up, the bank cars\<engine>[b]'s idle and
// its _on and _off loops of min, low, mid and high, their volumes by the rpm (.rev ranges, .vol curves) and the load,
// their frequencies by the rpm; the transmission's whine, the turbo's whine and dump, the gears, the nitro and the
// exhaust's splutters.

#include "Audio/Voices.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace oj {

class CrtRandom;
class GameFiles;
struct Car;
struct CarDef;
struct PartCatalogue;
struct RaceCar;

class CarSound {
public:
    // What the frame reads of the race (the stock's globals).
    struct Frame {
        float dt = 0.0f;           // [[G+4]+0x1C]
        bool replayRace = false;   // [G+0x6C]+0x68: the load is the replay frame's
        float replayLoad = 0.5f;   // the vehicle's +0x508
        bool raceState = false;    // [G+0x34]: a race runs
        float count = 0.0f;        // [[[G+0x34]+0x10]+0x1C]+0x38: the start's count
    };

    // 0x00492DC0 with the sound on (the caller checks): node the car's (the vehicle's +0x0C), nearest the vehicle's
    // +0x510; automod [0x0075EF40] and its N [0x0075EF44].
    CarSound(audio::World& world, const GameFiles& files, RaceCar& veh, const Car& rec, const PartCatalogue& catalogue,
             const void* node, int nearest, bool automod, long automodValue);
    // 0x004957F0: every instance released (the third bank's none).
    ~CarSound();
    CarSound(const CarSound&) = delete;
    CarSound& operator=(const CarSound&) = delete;

    void frame(const Frame& f, CrtRandom& random);  // vt[1] 0x00494620
    void gearUp(const Frame& f, CrtRandom& random);  // vt[2] 0x00495240
    void gearDown();                                 // vt[3] 0x004952A0
    // vt[4] 0x00494520: the engine's sounds' rolloff scale and the sound of the effects' squeal of the surface
    // (+0x90's [7]) when it has one.
    void setRolloffScale(float v, audio::Instance* squeal = nullptr);
    void setNode(const void* node);                  // vt[5] 0x004945A0 (called by nothing)
    void setOcclusion(float v);                      // vt[8] 0x004945E0
    void setMaster(float v) { m_master = v; }        // vt[9] 0x00495BD0: +0x144
    // vt[10] 0x00495BE0: the camera's car (on) or not: rolloff scales, priorities, groups, the occlusion's use, the
    // master volume and the engine's minimum distances; the vehicle's road sound played or stopped and its horn's
    // priority (given by the caller: road, horn).
    // `holders`: the sounds of the car's effects' scrape sparks and flakes (+0xD4, +0xE0; 0x00492820), either null.
    // `squeal`: the sound of the effects' squeal of the surface (+0x90), given to vt[4].
    void setFocus(bool on, audio::Instance* road, audio::Instance* horn,
                  std::array<audio::Instance*, 2> holders = {}, audio::Instance* squeal = nullptr);
    bool focus() const { return m_focus != 0; }      // vt[11] 0x00492D10: +0x188
    float load() const { return m_load; }            // vt[12] 0x00495DD0: +0x130
    void reset();                                    // vt[13] 0x00494080

private:
    struct Rev {
        float a = 0.0f, b = 0.0f, c = 0.0f;  // start rpm, end rpm, 48000 / the recorded rpm
    };
    struct Vol {
        float y[4]{};
        float x[4]{};
    };
    using VolTables = std::array<std::array<Vol, 5>, 3>;

    static VolTables& tablesOf(const CarDef* def);  // the CarDef's +0x200, shared by the model's cars
    void loadTables(const GameFiles& files);        // 0x004940D0
    void rebuildKnots();                            // 0x00494360
    void volumes(const Frame& f);                   // 0x00495750
    void weights();                                 // 0x00495670
    void loadMix(const Frame& f);                   // 0x00495400
    void transmission();                            // 0x00494D00
    void pitch(const Frame& f, CrtRandom& random);  // 0x004949B0
    void turbo(CrtRandom& random);                  // 0x00494F00
    void dump(bool force, CrtRandom& random);       // 0x00495010
    void splutter(const Frame& f, CrtRandom& random);  // 0x004952E0
    void nitro(const Frame& f);                     // 0x00495130
    void minDistances(float d);                     // 0x00495D90
    audio::Instance* make(const char* name);        // the world's create, attached to the master

    audio::World& m_world;
    RaceCar& m_veh;                                  // +0x04
    const Car& m_rec;
    VolTables* m_vol = nullptr;                      // +0xF4
    uint32_t m_base = 0;                             // +0x0C: added to every engine frequency (0)
    audio::Instance* m_masterInstance = nullptr;     // +0x38
    std::array<audio::Instance*, 5> m_on{};          // +0x40
    std::array<audio::Instance*, 5> m_off{};         // +0x54
    uint32_t m_slots = 0;                            // +0x7C
    float m_onWeight = 0.0f;                         // +0x80
    float m_offWeight = 0.0f;                        // +0x84
    float m_splutterTimer = 0.0f;                    // +0x94
    float m_splutterCooldown = 0.001f;               // +0x98
    audio::Instance* m_gearUp = nullptr;             // +0xA4
    audio::Instance* m_gearDown = nullptr;           // +0xA8
    audio::Instance* m_turboWhine = nullptr;         // +0xAC
    audio::Instance* m_dump = nullptr;               // +0xB0
    audio::Instance* m_dumpMed = nullptr;            // +0xB4
    audio::Instance* m_nitroStart = nullptr;         // +0xB8
    audio::Instance* m_nitroLoop = nullptr;          // +0xBC
    audio::Instance* m_transOn = nullptr;            // +0xC0
    audio::Instance* m_transOff = nullptr;           // +0xC4
    std::array<audio::Instance*, 3> m_splutters{};   // +0xC8
    float m_splutterChance = 0.8f;                   // +0xD4
    int m_splutterOn = 0;                            // +0xD8
    int m_dumpArmed = 0;                             // +0xDC
    float m_nitroTimer = 0.0f;                       // +0xE4
    float m_throttle = 0.0f;                         // +0x10C
    float m_throttleMean = 0.0f;                     // +0x110
    float m_rpm = 0.0f;                              // +0x114
    float m_rpmNitro = 0.0f;                         // +0x11C
    float m_rpmBefore = 0.0f;                        // +0x120
    float m_idle = 0.0f;                             // +0x124
    float m_max = 0.0f;                              // +0x128
    float m_loadTarget = 0.0f;                       // +0x12C
    float m_load = 0.0f;                             // +0x130
    float m_dumpVolume = 0.0f;                       // +0x134
    uint32_t m_dumpLevel = 0;                        // +0x138 (left as allocated with -automod 0: 0 here)
    float m_nitroOff = 0.0f;                         // +0x13C
    float m_nitroRpm = 0.0f;                         // +0x140
    float m_master = 1.0f;                           // +0x144
    float m_offFactor = 1.0f;                        // +0x148
    float m_clutchShare = 0.25f;                     // +0x14C
    float m_dip = 0.0f;                              // +0x150
    float m_dipTarget = 0.0f;                        // +0x154
    float m_dipScale = 0.0f;                         // +0x158
    float m_dipDelay = 0.0f;                         // +0x15C
    int m_dipFalling = 0;                            // +0x160
    int m_dipRising = 0;                             // +0x164
    int m_dipsLeft = 0;                              // +0x168
    uint32_t m_forwardGears = 0;                     // +0x16C
    int m_downShifted = 0;                           // +0x170
    int m_slipping = 0;                              // +0x174
    int m_slippingBefore = 0;                        // +0x178
    int m_blipTracking = 0;                          // +0x17C
    float m_blipRpm = 0.0f;                          // +0x180
    float m_blip = 0.0f;                             // +0x184
    int m_focus = 0;                                 // +0x188
    std::vector<audio::Instance*> m_voices;          // +0x18C
    std::array<float, 5> m_weights{};                // +0x19C
    std::array<std::array<Rev, 5>, 3> m_rev{};       // +0x1B4
};

}  // namespace oj
