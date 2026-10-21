#pragma once
// The force feedback of the stock game (docs/spec/02-frontend.md, "Force feedback"): the library it links ("EZ Wheel
// Wrapper v4.30.001"), the input manager's FF manager at [0x0075F5F8] (0x4E70 bytes). It opens the first two game
// controllers with a DirectInput of its own (two channels), reads them each frame and plays DirectInput effects on
// them: a spring, a constant force, a damper, the collisions' jolts, periodic rumbles and a negative damper, each in a
// slot of its group, made once and then changed in place. The platform's side (the devices and their effects) is a
// WheelDevice (Game/Platform/InputDevices). The player's control (0x0041E200) gives the forces of a car (playerForces).

#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace oj::ff {

// The effects' types, by the GUIDs the wrapper passes (0x006E94C4 .. 0x006E9534); None: a surface's type past 4
// (Stock problem: CreateEffect then gets no GUID).
enum class EffectType : uint8_t { None, Constant, Square, Sine, Triangle, SawtoothUp, SawtoothDown, Spring, Damper };

// A DICONDITION.
struct Condition {
    int32_t offset = 0;
    int32_t positiveCoefficient = 0;
    int32_t negativeCoefficient = 0;
    uint32_t positiveSaturation = 0;
    uint32_t negativeSaturation = 0;
    int32_t deadBand = 0;
};

// A DIEFFECT as the wrapper fills it (constant 0x005973D0 / 0x00597550, periodic 0x00596D60 / 0x00596F10, condition
// 0x00597080 / 0x00597250): DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS (0x12), the axes the X and Y offsets (0, 4) of
// which the channel's force feedback axes count, the type's own parameters; an envelope (DIENVELOPE) for the
// constant and periodic ones, none for a condition. A condition's second axis always takes 0, 8000, 8000, 8000, 8000
// and the dead band 500.
struct EffectParams {
    EffectType type = EffectType::None;
    uint32_t flags = 0x12;
    uint32_t duration = 0xFFFFFFFF;  // INFINITE, or the collisions' 150000 us
    uint32_t samplePeriod = 0;
    uint32_t gain = 10000;
    uint32_t triggerButton = 0xFFFFFFFF;  // DIEB_NOTRIGGER
    uint32_t triggerRepeat = 0;
    uint32_t axisCount = 0;
    std::array<uint32_t, 2> axes{0, 4};
    std::array<int32_t, 2> directions{1, 0};
    uint32_t startDelay = 0;
    bool envelope = false;
    uint32_t attackLevel = 0;
    uint32_t attackTime = 0;
    uint32_t fadeLevel = 0;
    uint32_t fadeTime = 0;
    int32_t magnitude = 0;  // DICONSTANTFORCE / DIPERIODIC
    int32_t offset = 0;     // DIPERIODIC
    uint32_t phase = 0;
    uint32_t period = 0;
    std::array<Condition, 2> conditions{};

    bool operator==(const EffectParams&) const = default;
};

// A channel's device (an IDirectInputDevice8 of the wrapper's DirectInput, c_dfDIJoystick2, exclusive and foreground)
// and its effects. The results are HRESULTs (negative a failure).
class WheelDevice {
public:
    virtual ~WheelDevice() = default;
    virtual int32_t acquire() = 0;
    virtual int32_t unacquire() = 0;
    virtual int32_t poll() = 0;
    virtual int32_t actuatorsOn() = 0;                           // SendForceFeedbackCommand(DISFFC_SETACTUATORSON)
    virtual int32_t state(std::array<uint8_t, 0x110>& out) = 0;  // GetDeviceState(DIJOYSTATE2)
    virtual int32_t gain(uint32_t& out) = 0;                     // GetProperty(DIPROP_FFGAIN)
    virtual int32_t setGain(uint32_t value) = 0;                 // SetProperty(DIPROP_FFGAIN)
    // CreateEffect: handle not 0 when made.
    virtual int32_t createEffect(const EffectParams& p, uintptr_t& handle) = 0;
    // SetParameters with DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION (0x140).
    virtual int32_t setParameters(uintptr_t handle, const EffectParams& p) = 0;
    virtual int32_t start(uintptr_t handle) = 0;  // Start(1, 0)
    virtual int32_t stop(uintptr_t handle) = 0;
    virtual int32_t status(uintptr_t handle, uint32_t& out) = 0;  // GetEffectStatus
    virtual int32_t unload(uintptr_t handle) = 0;
    virtual void releaseEffect(uintptr_t handle) = 0;
    // 0x00596CA0: Unacquire, Release.
    virtual void release() = 0;
};

// __ftime as the wrapper keeps it: seconds (the low 32 bits) and milliseconds.
struct Time {
    int32_t seconds = 0;
    int32_t millis = 0;
};

// The sine table (0x00591FF0, +0x4828): 360 entries trunc(sin(i x 3.141592654 / 180) x 1000).
std::array<int32_t, 360> sineTable();

class Wheels {
public:
    static constexpr int kChannels = 2;
    static constexpr uint32_t kDeviceUnplugged = 0x8007001E;  // DIERR_UNPLUGGED

    // The groups of effects (+0x4680 unused, +0x46D0 conditions, +0x4720 constant forces, +0x4770 periodic ones,
    // +0x47C0 unused), each 8 slots of each channel: a playing flag and an effect.
    enum Group { kUnused, kConditions, kConstants, kPeriodics, kSpare, kGroups };

    // The wrapper's clock (__ftime); messages go to the debugger.
    std::function<Time()> clock;

    // 0x00591DF0 with the devices the wrapper opened (0x00596790: the first two game controllers, each made with the
    // autocenter off; null for none) and their force feedback axes (0x005969A0, at most 2): the groups made empty
    // (0x005963E0, 0x00591EA0) with the clock's time, the sine table (0x00591FF0), "EZ Wheel Wrapper v4.30.001".
    void open(const std::array<WheelDevice*, 2>& devices, const std::array<uint32_t, 2>& ffAxes);
    // 0x00593C80: each channel's gain back to 100 when it is not (0x00596640, 0x005965B0), the effects of the groups
    // +0x4720, +0x4770, +0x47C0 and +0x46D0 released (0x005966C0; the playing flags kept), the devices released
    // (0x00596CA0). Then the input manager's teardown releases every group's effects again.
    void shutdown();
    // 0x005966C0 for every group (the input manager's teardown and the FF manager made anew): the effects released.
    void releaseAll();
    // 0x00592050: each channel with a device read (0x00596AE0).
    void read();

    bool hasDevice(int ch) const { return m_channels[static_cast<size_t>(ch)].device != nullptr; }  // +0x444C
    uint32_t ffAxes(int ch) const { return m_channels[static_cast<size_t>(ch)].axes; }                 // +0x4454
    bool airborne(int ch) const { return m_channels[static_cast<size_t>(ch)].airborne; }              // +0x2220
    bool playing(Group g, int slot, int ch) const;   // 0x00596570: GetEffectStatus DIEGES_PLAYING
    bool flag(Group g, int slot, int ch) const;       // the slot's playing flag
    const std::array<uint8_t, 0x110>& wheelState(int ch) const { return m_channels[static_cast<size_t>(ch)].state; }

    // The forces (each with no device: the two messages; airborne or without force feedback axes: nothing).
    void spring(int ch, int32_t offset, int32_t saturation, int32_t coefficient);  // 0x005923B0
    void constant(int ch, int32_t magnitude);                                       // 0x00592620
    void constantAt(int ch, int32_t angle, int32_t magnitude);                      // 0x00592790
    void stopConstant(int ch);                                                      // 0x005927D0
    void damper(int ch, int32_t coefficient);                                       // 0x00592810
    void sideCollision(int ch, int32_t magnitude);                                  // 0x005929F0
    void sideCollisionAt(int ch, int32_t angle, int32_t magnitude);                 // 0x00592BD0
    void frontalCollision(int ch, int32_t magnitude);                               // 0x00592C10
    void sine(int ch, int32_t magnitude);                                           // 0x00592E50
    void stopSine(int ch);                                                          // 0x00593000
    void square(int ch, int32_t magnitude);                                         // 0x00593040
    void stopSquare(int ch);                                                        // 0x005931F0
    void slippery(int ch, int32_t magnitude);                                       // 0x00593230
    void stopSlippery(int ch);                                                      // 0x00593460
    void surface(int ch, int32_t type, int32_t magnitude, int32_t period);          // 0x00593520
    bool sameSurface(int ch, int32_t type, int32_t magnitude, int32_t period) const;  // 0x00593940
    void stopSurface(int ch);                                                       // 0x00593900
    void airborneStart(int ch);                                                     // 0x00593980
    void airborneEnd(int ch);                                                       // 0x00593B30
    void stopForce(int ch, int kind);                                               // 0x00592090
    // 0x0058F650 (0x00407FB0, the UI map registered): with a device, the car landed (0x00593B30), the constant force
    // and the damper stopped, the sine, the negative damper (its spring and damper given back) and the surface
    // stopped, and the spring 0, 5000, 5000.
    void letGo(int ch);
    int32_t gainPercent(int ch);                    // 0x00596640: the gain x 100 / 10000, -1 when not read
    void setGainPercent(int ch, uint32_t percent);  // 0x005965B0: at most 100, x 10000 / 100

private:
    struct Slot {
        bool playing = false;  // the group's byte
        uintptr_t effect = 0;  // the group's +0x10 ..
    };
    struct Channel {
        WheelDevice* device = nullptr;      // +0x444C (the reader's +0x2228)
        uint32_t axes = 0;                  // +0x4454 (+0x2230): the force feedback axes
        bool airborne = false;              // +0x2220
        bool actuators = false;             // the reader's +0x2458
        std::array<uint8_t, 0x110> state{};     // the reader's +0x00 (copied to the wrapper's)
        std::array<uint8_t, 0x110> previous{};  // the reader's +0x2238
        bool damperDisplaced = false;       // +0x4810: the negative damper stopped the damper
        bool springDisplaced = false;       // +0x4812: and the spring
        // +0x4814 (10 bytes): what the car's flight stopped, given back as it lands: [0] the spring, [1] the constant
        // force, [2] the damper, [5] the sine, [6] the square, [7] the negative damper, [8] the surface.
        std::array<bool, 10> stopped{};
        Time frontalTime;                   // +0x4DC8, +0x4DD0
        Time sideTime;                      // +0x4DD8, +0x4DE0
        int32_t frontalMagnitude = 0;       // +0x4DE8: the frontal jolts of the last 150 ms
        int32_t sideMagnitude = 0;          // +0x4DF0
        int32_t springOffset = 0;           // +0x4DF8
        int32_t springSaturation = 0;       // +0x4DFC
        int32_t springCoefficient = 0;      // +0x4E00
        int32_t constantMagnitude = 0;      // +0x4E10
        int32_t constantAngle = 0;          // +0x4E14: read by the landing, written by nothing
        int32_t damperCoefficient = 0;      // +0x4E20
        int32_t sideMade = 0;               // +0x4E28
        int32_t frontalMade = 0;            // +0x4E38
        int32_t sineMagnitude = 0;          // +0x4E40
        int32_t squareMagnitude = 0;        // +0x4E48
        int32_t slipperyMagnitude = 0;      // +0x4E50
        int32_t surfaceType = 0;            // +0x4E58
        int32_t surfaceMagnitude = 0;       // +0x4E5C
        int32_t surfacePeriod = 0;          // +0x4E60
    };

    Slot& slot(Group g, int s, int ch) { return m_groups[g][static_cast<size_t>(ch * 8 + s)]; }
    const Slot& slot(Group g, int s, int ch) const { return m_groups[g][static_cast<size_t>(ch * 8 + s)]; }
    bool ready(int ch);  // a device (else the two messages), not airborne, force feedback axes
    int32_t start(Group g, int s, int ch);   // 0x00596420
    int32_t stop(Group g, int s, int ch);    // 0x00596490
    int32_t unload(Group g, int s, int ch);  // 0x00596500
    void releaseGroup(Group g);              // 0x005966C0
    int32_t create(Group g, int s, int ch, const EffectParams& p);  // 0x00596D60 / 0x00597080 / 0x005973D0
    int32_t change(Group g, int s, int ch, const EffectParams& p);  // 0x00596F10 / 0x00597250 / 0x00597550
    int32_t readChannel(int ch);             // 0x00596AE0
    EffectParams constantParams(int ch, uint32_t duration, int32_t magnitude) const;
    EffectParams periodicParams(int ch, EffectType type, uint32_t duration, int32_t magnitude, uint32_t period,
                                uint32_t fadeTime) const;
    EffectParams conditionParams(int ch, EffectType type, std::array<int32_t, 2> directions, int32_t offset,
                                 int32_t coefficient, int32_t saturation) const;
    void play(Group g, int s, int ch, const EffectParams& p, bool same, bool restartWhenSame);
    Time now() const;

    std::array<Channel, kChannels> m_channels{};
    std::array<std::array<Slot, 16>, kGroups> m_groups{};
    std::array<int32_t, 360> m_sine{};
};

// The car's values the player's forces take (0x0041E200), as its fields are.
struct FeedbackCar {
    std::array<float, 4> travel{};  // the wheels' suspension travel (the poses' +0x1DC, +0x278, +0x314, +0x3B0; cm)
    std::array<float, 2> skid{};    // the front wheels' skid (+0x220, +0x2BC)
    std::array<float, 2> smoke{};   // and smoke (+0x224, +0x2C0)
    std::array<bool, 2> down{};     // touching the route (+0x200, +0x29C)
    std::array<int32_t, 2> surface{};  // their zone's surface (+0x230, +0x2CC: 0 tarmac)
    float speed = 0.0f;             // the body's local velocity's length (0x00407E60 / 0x00407C50)
    float impact = 0.0f;            // +0x174: what is left of the last hit's damage
    std::array<float, 3> hitPoint{};  // +0x46C: the last hit's direction from the centre
    float engineSpeed = 0.0f;       // the body's +0x358
    bool nitroOn = false;           // the body's +0x378
    float part0 = 0.0f;             // 0x0048E340: DATAVAL of the car's catalogue part of kind 0 (none: 0)
    float part2 = 0.0f;             // 0x0048E300: DATAVAL of its part of kind 2
};

// 0x0041E6F5 .. 0x0041F244, the player's forces with the steering wheel and its force feedback on (strength: the
// settings' +0xA4 of the player); maxLoad the steering load's highest ([0x0079E790], read by nothing else).
void playerForces(Wheels& wheels, int channel, float strength, const FeedbackCar& car, float& maxLoad);

// The controller's motors (+0x10C, +0x110; 0x0041DD40 keeps the larger, 0x0041DD60 the larger within 0 .. 1), which
// nothing on the PC reads.
struct Rumble {
    float large = 0.0f;
    float small = 0.0f;
    void largeAtLeast(float v) {
        if (large < v) large = v;
    }
    void smallAtLeast(float v) {
        if (small < v) small = v;
        if (small < 0.0f) small = 0.0f;
        else if (1.0f < small) small = 1.0f;
    }
};

}  // namespace oj::ff
