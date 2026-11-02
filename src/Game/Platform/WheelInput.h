#pragma once
// The fixed build's steering wheels (docs/fixes.md Added), JuicedConfigExt's way with its core
// (external/JuicedConfigExt/src/Shared): the devices of controller.ini's [WheelMap] read by its WheelReader (a thread
// of its own, DirectInput's c_dfDIJoystick2 not exclusive and in the background; a device the FF manager opened is
// read through its channel, not opened a second time), their values the driving actions' and the menus' (PadControls'
// wheel); with no driving action in [WheelMap] the first steering wheel steers with its X axis and its two axes resting
// at an end are the throttle and the brake. The FF manager (InputDevices::startWheels) is given the steering wheels and
// the other devices with force feedback (none with ffb = off: a button box or a shifter listed first took a channel),
// its effects scaled with ffb = scaled. controller.ini's [Wheel] as the plugin's: deadzone, rotation, lock, exponent,
// linearInput, ffb (off, game, scaled), ffbStrength, ffbCentering, ffbEffects, ffbInvert, read at the start.

#include <ForceFeedbackScale.h>

#include <cstdint>
#include <memory>
#include <string>

struct DIDEVICEINSTANCEA;

namespace WheelReader {
class Reader;
}

namespace oj {

struct WheelSettings {
    enum class Ffb : uint8_t { Off, Game, Scaled };

    float deadzone = 0.0f;     // the steering's dead zone, 0 .. 0.5
    float rotation = 0.0f;     // the wheel's rotation in degrees, 0 .. 3600; 0 none
    float lock = 240.0f;       // the car's lock in degrees, 30 .. 3600
    float exponent = 0.0f;     // the steering's power in place of the game's (0.3 .. 4); 0 the game's
    bool linearInput = false;  // the steering past the response table (its flag [0x0079E89C] set)
    Ffb ffb = Ffb::Game;
    ForceFeedbackScale::Settings scale;  // ffbStrength 10 .. 100, ffbCentering and ffbEffects 0 .. 200, ffbInvert

    // The steering's factor: the wheel's rotation / the lock (1 without a rotation).
    float gain() const { return rotation > 0.0f ? rotation / lock : 1.0f; }
};

// controller.ini's [Wheel] (a value missing, not a number or out of its range: the default).
WheelSettings readWheelSettings(const std::wstring& path);

class WheelInput {
public:
    WheelInput();
    ~WheelInput();
    WheelInput(const WheelInput&) = delete;
    WheelInput& operator=(const WheelInput&) = delete;

    // The settings of controller.ini and its map for the reader (its default map on).
    void load(const std::wstring& path);
    const WheelSettings& settings() const { return m_settings; }
    WheelReader::Reader& reader() { return *m_reader; }

    // The FF manager's listing: whether a game controller listed is given to it (JuicedConfigExt's SkipDevice: none
    // with ffb = off, a steering wheel by WheelDetect's marks, else one with a force feedback driver), then what it
    // made of it (opened: its channel; the reader's slot, -1 for none) and the states the channel read.
    bool forceFeedbackTakes(const DIDEVICEINSTANCEA& instance) const;
    int channel(const DIDEVICEINSTANCEA& instance, bool opened);
    void channelState(int slot, const void* state);

    // The reader's thread (before the FF manager's listing, which it waits for to open a device the manager may take;
    // a test run starts none), the listing over, the thread's end.
    void start();
    void listed();
    void stop();

private:
    WheelSettings m_settings;
    std::wstring m_path;
    std::unique_ptr<WheelReader::Reader> m_reader;
};

}  // namespace oj
