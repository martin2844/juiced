#pragma once
// The input library of the stock game (docs/spec/02-frontend.md, "Input"): the input manager (0x00591050, vtable
// 0x006FC4E0) with one device per game controller DirectInput found (none: one device without a controller), each
// device also reading the keyboard; the actions a map of the controls file makes on a device (0x0058F890); the
// players' devices; the UI and DRIVER1 controllers' action slots (0x00407FB0, 0x004080D0) and the window event made of
// the UI actions (0x00408260).
//
// The platform's side is outside: the keyboard comes as the window's key events (the scan codes are DirectInput's
// keyboard codes); a device's game controller is a ControllerPort (Game/Platform/InputDevices: an IDirectInputDevice8).

#include "Core/Data/Controls.h"
#include "Core/Input/ForceFeedback.h"
#include "Core/Saves/SaveEntries.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oj::input {

// A key going down or up, by its DirectInput keyboard code (the scan code, + 0x80 for the extended keys).
struct KeyEvent {
    uint8_t code = 0;
    bool down = false;
};

// The window events (0x00408260).
enum UiEvent : uint32_t {
    kNone = 0,
    kUp = 1,
    kDown = 2,
    kLeft = 3,
    kRight = 4,
    kSelect = 5,   // SELECT or START
    kSelect2 = 6,
    kSelect3 = 7,
    kSelect4 = 8,
    kSelect5 = 9,
    kSelect6 = 10,
    kSelect7 = 11,
    kBack = 12,    // BACK or BACK2
};

// The window event of a controller (0x00408260, 7 dwords).
struct UiFrame {
    uint32_t event = kNone;   // +0x00
    uint32_t scroll = kNone;  // +0x04: kUp .. kRight while a SCROLL_ action is on
    bool start = false;       // START this frame
    // The input manager's +0x08 (0x00591820): a device whose poll typed a character (its +0x39C, 0x00595A80) or
    // which has input (0x00594E20).
    bool anyInput = false;
    std::array<float, 4> axes{};  // +0x08 .. +0x14: AXIS1_X, AXIS1_Y, AXIS2_X, AXIS2_Y (0x00711600)
    bool connected = true;        // +0x18: the device's +0x34 (its last poll read it)
};

// A DIACTION of a device's map (0x28 bytes, the device's +0x48 list).
struct MapAction {
    uint32_t appData = 0;   // uAppData: the control's value (+0x58) index; a character key's code | 0x80000000
    uint32_t semantic = 0;  // dwSemantic
    uint32_t flags = 0;     // dwFlags: APPFIXED 0x10, APPMAPPED 2, APPNOMAP 4, FORCEFEEDBACK 1, NORANGE 8
    std::string label;      // lptszActionName
};

// The DIACTIONFORMAT a device registers its map with (0x005954D0): the device's actions, the map's GUID and name, the
// racing genre, a buffer of 32, axes -100 .. 100.
struct MapFormat {
    std::string name;                // tszActionMap
    std::array<uint8_t, 16> guid{};  // guidActionMap (the map's GUID line, 0x00595490)
    uint32_t genre = 0x01000000;     // DIVIRTUAL_DRIVING_RACE
    uint32_t bufferSize = 32;
    int32_t axisMin = -100;
    int32_t axisMax = 100;
    std::vector<MapAction> actions;
};

// What a device's game controller gave at a poll (0x00594C20).
struct ControllerRead {
    bool ok = true;  // Poll (else Acquire) and the read succeeded
    // With a map (GetDeviceData, room for 32): each event's uAppData and dwData.
    std::vector<std::pair<uint32_t, uint32_t>> events;
    // Raw (GetDeviceState, c_dfDIJoystick2): the 128 buttons of the state (+0xAC of the device).
    std::array<uint8_t, 128> buttons{};
};

// The platform's game controller of a device (the device's +0x3C, an IDirectInputDevice8 of the input manager's
// DirectInput).
class ControllerPort {
public:
    virtual ~ControllerPort() = default;
    virtual void unacquire() = 0;
    // 0x005954D0: BuildActionMap, then SetActionMap, with the device's format.
    virtual void setActionMap(const MapFormat& format) = 0;
    // 0x00595890: SetDataFormat(c_dfDIJoystick2), raw again.
    virtual void setRawFormat() = 0;
    // 0x00594C20: Poll, on failure Acquire (and Unacquire when that fails too, ok false); then with a map
    // GetDeviceData into events, else GetDeviceState into buttons.
    virtual ControllerRead read(bool mapped) = 0;
    // 0x00596210 (after the controls' mapping window): released and made again from its instance GUID, c_dfDIJoystick2,
    // SetCooperativeLevel 0x16.
    virtual void recreate() = 0;
};

// The kinds of a device's controls: a key or a button (vtable 0x006FC5F0, 0x00594030: 1 while its value is not 0), an
// axis (0x006FC5B8, 0x00593F70: its value x 0.01) and an axis through the response table (0x006FC5F4, 0x00593F90).
enum class ControlKind : uint8_t { Key, Axis, AxisTable };

struct Control {
    ControlKind kind = ControlKind::Key;
    int32_t index = 0;  // the value's (+0x58) index
};

// A binding of an action to its controls (the classes made by 0x0058F890).
struct Binding {
    enum class Type : uint8_t {
        DigitalKey,    // 0x006FC478 (0x005900F0): a key or button's value; OneShot only as it comes on
        DigitalAxis,   // 0x006FC470 (0x00590130): on while the axis x the sign passes the dead zone
        AnalogueKey,   // 0x006FC4C0 (0x00590180): up by Attack a second while on, down by Decay, 0 .. 1
        AnalogueKeys,  // 0x006FC4B0 (0x00590360): two keys, -1 .. 1
        AnalogueAxis,  // 0x006FC4B8 (0x00590260): the axis past the dead zone, scaled, followed by Attack and Decay
        AnalogueAxes,  // 0x006FC340 (0x0058EA50): the second axis less the first
    };
    Type type = Type::DigitalKey;
    Control control;
    Control control2;
    bool oneShot = false;     // +0x04 (DIGITAL)
    int32_t value = 0;        // +0x08: a DIGITAL binding's value
    int32_t previous = 0;     // +0x0C: its raw value at the poll before (OneShot)
    int32_t raw = 0;          // +0x10
    float level = 0.0f;       // +0x04: an ANALOGUE binding's value
    float deadzone = 0.2f;
    float scale = 0.0f;       // AnalogueAxis +0x10: 1 / (saturation - dead zone), 0 when not above it
    float sign = 1.0f;
    float attack = 1000.0f;
    float decay = 1000.0f;
};

// A named action of a device (0x00590460): its bindings and its value (+0x14: a DIGITAL action's 0 or 1, an ANALOGUE
// action's float).
struct Action {
    std::string name;
    std::vector<Binding> bindings;
    int32_t on = 0;
    float value = 0.0f;
};

// The input manager's devices (vtable 0x006FC5BC, 0x3B8 bytes; its base, the action set, 0x00590990).
struct Device {
    int32_t index = 0;                   // +0x0C: the player slot's device number (0x005918E0)
    std::vector<Action> digital;         // +0x10
    std::vector<Action> analogue;        // +0x20
    int32_t connected = 0;               // +0x34: its last poll read it
    ControllerPort* controller = nullptr;  // +0x3C (none: a device of the keyboard alone)
    std::vector<MapAction> format;       // +0x48
    std::vector<int32_t> values;         // +0x58
    std::vector<int32_t> valueFlags;     // +0x68: 1 a key or a button, 0 an axis
    bool mapped = false;                 // +0x78
    std::array<uint8_t, 128> buttons{};  // +0xAC: the controller's raw buttons
    std::array<uint8_t, 256> state{};    // +0x19C: the keyboard's state as the device read it
    std::array<bool, 256> charDown{};    // +0x29C
    bool typedAny = false;               // +0x39C
    char typed = 0;                      // +0x3A0
    std::string mapName;                 // the map registered last

    // 0x005940B0 / 0x00594390 / 0x00594670: a control of the semantic's text; a semantic already in the format keeps
    // its value, else a DIACTION and a value (flag 1 for a key or button, 0 for an axis) are added.
    Control control(const std::string& text, ControlKind kind);
    // 0x00595030: with "UI" in the map's name the character keys (KEY_1 .. KEY_RMENU), DIACTIONs without values.
    void addCharacterKeys(const std::string& mapName);
    // 0x00594E20.
    bool hasInput() const;
    // A control's value as its class reads it; table: the response table (channel 0, 1024 entries).
    float read(const Control& c, const std::array<int32_t, 2048>& table) const;
};

// The response table of an axis (0x00596C00): 1023 entries ((1 - k) x + k x³) x 10000 cut toward zero for x = (i - 511)
// / 511 and k = nonlinearity / 100, the last entry the one before (the main thread's x87 in single precision).
std::array<int32_t, 1024> responseTable(int32_t nonlinearity);

// The input manager ([game + 0x18]) and the players' controllers ([game + 0xAC], one per device).
class Input {
public:
    enum class Map { None, Ui, Driver };

    Input();

    // The controls file's maps (the parser's object, [0x0075EFAC]).
    void load(const std::vector<ActionMap>& maps);
    // 0x00591050: a device for each controller found, in the order DirectInput listed them; none: one device without.
    void setControllers(const std::vector<ControllerPort*>& controllers);

    size_t deviceCount() const { return m_devices.size(); }
    const Device& device(size_t i) const { return m_devices[i]; }
    // +0x28, +0x2C: the device of the first and the second player, -1 for none.
    int32_t playerDevice(int player) const { return m_players[static_cast<size_t>(player)]; }
    // The player's device let go (multi's and instant_manu's enter: +0x2C -1).
    void releasePlayer(int player) { m_players[static_cast<size_t>(player)] = -1; }
    // The player's device set as it is (-i's first player: +0x28 0).
    void setPlayerDevice(int player, int32_t device) { m_players[static_cast<size_t>(player)] = device; }
    // 0x005918E0 (vt+0x3C): the player's device taken when it has none: each device with input that is no player's
    // (the last of them), its number (+0x0C) its place.
    void takePlayer(int player);

    // 0x005918C0 (0x0058FE50): the first player's device taken when none (0x005918E0, by the polls before), then every
    // device polled (0x00594C20) and its actions stepped (0x00590B50) by dt. active: the game's window is the active
    // one ([0x0075F780]); else nothing is read and the devices are not connected (+0x34 0).
    void poll(const std::vector<KeyEvent>& events, bool active = true, float dt = 0.0f);
    // 0x00408260: the player's window event from its controller's UI slots (none when another map is its device's);
    // dt in seconds. The first player's falls back on the first controller without a device; the second player's is
    // empty without one.
    UiFrame uiFrame(float dt, int player = 0);

    void useUi(int player = 0);      // 0x00407FB0 for the player's controller
    void useDriver(int player = 0);  // 0x004080D0 for the player's controller
    void release();    // 0x0058FEA0: no player devices (+0x28, +0x2C -1), every device emptied to raw (0x00595890)
    // The map the first player's device has registered last (that of the last device without a first player).
    Map map() const;
    bool playerTaken() const { return m_players[0] != -1; }
    // 0x00594E20 of the first player's device (the last device without one).
    bool hasInput() const;
    // 0x00591820: a device typed a character at its poll or has input.
    bool anyInput() const;
    // A key's value (+0x58) of the first player's device as the DRIVER1 bindings read it (0x00594030).
    bool value(uint8_t code) const;

    // 0x00591870: the character typed at the first player's device's poll (the last device's without one), 0 for
    // none: upper-case letters, digits, '-', '@', '.', ' ', 0x08 backspace, 0x0A return, 0x0E caps lock, 0x0F shift,
    // 0x1B escape.
    char typed() const;
    // 0x005917E0: the key of the character is down (the game asks for 0x0F, either shift key).
    bool held(char c) const;

    // The settings the controllers' making reads ([G+0x64], none before a driver: the defaults' 0): the steering
    // wheel (+0x78, the manager's +0x38: STEERING's axes through the response table), not speed sensitive (+0xAC, the
    // table's nonlinearity 20, else -20), the driver's map of each device number (+0xCC + 4 x the device's +0x0C: 0 ..
    // 3 for DRIVER1 .. DRIVER4).
    void setSettings(const std::optional<save::Settings>* settings) { m_settings = settings; }
    bool wheel() const { return m_wheel; }
    // The FF manager ([0x0075F5F8]): the UI map's registering lets its channels go (0x00407FB0 -> 0x0058F650), the
    // update reads them after the devices' polls (0x005918C0 -> 0x00592050).
    void setWheels(ff::Wheels* wheels) { m_wheels = wheels; }
    // The first player's controller's motors (+0x10C, +0x110; 0x0041DD40, 0x0041DD60), read by nothing on the PC.
    ff::Rumble& rumble(int player = 0);  // the player's controller's motors
    // The FF manager's response table ([0x0075F5F8] +0x220, two channels of 1024; 0x005914D0 makes it with 20).
    std::array<int32_t, 2048>& table() { return m_table; }
    const std::array<int32_t, 2048>& table() const { return m_table; }

    // The DRIVER1 slots of a player's controller (0x004015D0 for a digital one, 0x00401640 for an analogue one; the
    // first player's or the second's, playerController): -1 or past the device's actions 0.
    int32_t digital(size_t slot, int player = 0) const;
    float analogue(size_t slot, int player = 0) const;
    // The DRIVER1 action names of the slots (0x00711588 .., 0x00711610 ..).
    static constexpr int kDriverDigital = 16;
    static constexpr int kDriverAnalogue = 6;

    // 0x00590EE0 (after the controls' mapping window, 0x005957D1): every device's controller made again (0x00596210)
    // and its format registered again.
    void recreateControllers();

private:
    struct Controller {
        int32_t device = 0;               // +0x00
        std::array<int32_t, 18> ui{};     // +0x04 .. +0x48 (0x00711540)
        std::array<int32_t, 16> driver{}; // +0x4C .. +0x88 (0x00711588)
        std::array<int32_t, 4> uiAxes{};  // +0xC4 .. +0xD0 (0x00711600)
        std::array<int32_t, 6> driverAxes{};  // +0xD4 .. +0xE8 (0x00711610)
        ff::Rumble rumble;                // +0x10C, +0x110
        float repeat = 0.0f;              // +0x114: the direction repeat timer
        uint32_t lastDirection = kNone;   // +0x118
        Map map = Map::None;
    };

    // The keyboard all the devices share (+0x40, one IDirectInputDevice8): its action map the device that registered
    // last set (key codes to uAppData), the events buffered while it has one and the window is active.
    struct Keyboard {
        std::array<bool, 256> physical{};
        std::deque<KeyEvent> buffer;
        bool mapped = false;
        std::array<int64_t, 256> appData{};  // -1 a key of no action
        std::array<bool, 256> buffered{};    // each key as its last buffered event left it
        bool acquired = false;               // the window active at the last poll
    };

    Device* firstDevice();
    const Device* firstDevice() const;
    Controller* firstController();
    // The player's controller: the first player's (firstController), the second's of its device (none without).
    Controller* playerController(int player);
    uint32_t setting(save::Settings::Field field) const;  // the driver's settings' value
    void registerMap(const std::string& name, Device& device);  // 0x0058F890 with the device's +0x18 (0x005954D0)
    void empty(Device& device);                                  // 0x00595890
    void pollDevice(Device& device, bool active, float dt);     // 0x00594C20
    void step(Device& device, float dt);                         // 0x00590B50
    void scanCharacters(Device& device);                         // 0x00595A80
    int32_t slotValue(const Device& d, int32_t slot) const;
    float slotAnalogue(const Device& d, int32_t slot) const;

    std::vector<ActionMap> m_maps;
    std::vector<Device> m_devices;          // +0x18
    std::vector<Controller> m_controllers;  // [game + 0xAC]
    std::array<int32_t, 2> m_players{-1, -1};  // +0x28, +0x2C
    bool m_wheel = false;                   // +0x38
    const std::optional<save::Settings>* m_settings = nullptr;
    Keyboard m_keyboard;
    std::array<int32_t, 2048> m_table{};
    std::vector<MapFormat> m_formats;       // each device's format registered last (for 0x005957D6)
    ff::Wheels* m_wheels = nullptr;
};

}  // namespace oj::input
