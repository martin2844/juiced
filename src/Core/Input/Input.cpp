#include "Core/Input/Input.h"

#include "Core/Base/Log.h"
#include "Core/Base/X87.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <utility>

namespace oj::input {

namespace {

// The UI controller's action names (0x00711540, 0x00711600) and DRIVER1's (0x00711588, 0x00711610).
constexpr const char* kUiDigital[18] = {
    "LEFT",    "RIGHT",   "UP",      "DOWN",    "SCROLL_LEFT", "SCROLL_RIGHT", "SCROLL_UP", "SCROLL_DOWN", "SELECT",
    "SELECT2", "SELECT3", "SELECT4", "SELECT5", "SELECT6",     "SELECT7",      "BACK",      "BACK2",       "START",
};
constexpr const char* kUiAnalogue[4] = {"AXIS1_X", "AXIS1_Y", "AXIS2_X", "AXIS2_Y"};
constexpr const char* kDriverDigitalNames[16] = {
    "PAUSE",       "SELECT",  "REAR_VIEW",         "GEAR_DOWN",            "GEAR_UP",
    "CHANGE_CAMERA", "RESET", "NITRO",             "DRIVER_HORN",          "NEXTCAR",
    "DRIVER_TACTICS_UP", "DRIVER_TACTICS_RIGHT", "DRIVER_TACTICS_DOWN", "DRIVER_TACTICS_LEFT",
    "DRIVER_TACTICS3_UP", "DRIVER_TACTICS3_DOWN",
};
constexpr const char* kDriverAnalogueNames[6] = {"STEERING", "THROTTLE", "BRAKE",
                                                 "HANDBRAKE", "REVERSE",  "DRIVER_CAMERA_PAN"};
// The driver's maps by the settings' +0xCC of the device (0x0071152C).
constexpr const char* kDriverMaps[4] = {"DRIVER1", "DRIVER2", "DRIVER3", "DRIVER4"};

enum UiSlot {
    kActionLeft, kActionRight, kActionUp, kActionDown, kScrollLeft, kScrollRight, kScrollUp, kScrollDown,
    kActionSelect, kActionSelect2, kActionSelect3, kActionSelect4, kActionSelect5, kActionSelect6, kActionSelect7,
    kActionBack, kActionBack2, kActionStart,
};

// Keys that type a character, in the order the keyboard device looks at them (0x00595A80).
struct KeyCharacter {
    int code;
    char character;
};

constexpr KeyCharacter kCharacters[] = {
    {0x01, 0x1B}, {0x02, '1'}, {0x03, '2'}, {0x04, '3'},  {0x05, '4'},  {0x06, '5'}, {0x07, '6'}, {0x08, '7'},
    {0x09, '8'},  {0x0A, '9'}, {0x0B, '0'}, {0x0E, 0x08}, {0x0C, '-'},  {0x10, 'Q'}, {0x11, 'W'}, {0x12, 'E'},
    {0x13, 'R'},  {0x14, 'T'}, {0x15, 'Y'}, {0x16, 'U'},  {0x17, 'I'},  {0x18, 'O'}, {0x19, 'P'}, {0x1C, 0x0A},
    {0x1E, 'A'},  {0x1F, 'S'}, {0x20, 'D'}, {0x21, 'F'},  {0x22, 'G'},  {0x23, 'H'}, {0x24, 'J'}, {0x25, 'K'},
    {0x26, 'L'},  {0x91, '@'}, {0x2C, 'Z'}, {0x2D, 'X'},  {0x2E, 'C'},  {0x2F, 'V'}, {0x30, 'B'}, {0x31, 'N'},
    {0x32, 'M'},  {0x34, '.'}, {0x39, ' '}, {0x3A, 0x0E},
};
constexpr uint8_t kLeftShift = 0x2A;
constexpr uint8_t kRightShift = 0x36;
constexpr uint8_t kLeftAlt = 0x38;
constexpr uint8_t kRightAlt = 0xB8;
constexpr uint8_t kReturn = 0x1C;
constexpr char kShift = 0x0F;

// The keys of characters the UI map's making adds to the device (0x00595030), with their DIACTIONs' names.
struct CharacterKey {
    uint8_t code;
    const char* label;
};
constexpr CharacterKey kCharacterKeys[] = {
    {0x02, "KEY_1"},      {0x03, "KEY_2"},      {0x04, "KEY_3"},     {0x05, "KEY_4"},      {0x06, "KEY_5"},
    {0x07, "KEY_6"},      {0x08, "KEY_7"},      {0x09, "KEY_8"},     {0x0A, "KEY_9"},      {0x0B, "KEY_0"},
    {0x0E, "KEY_BACK"},   {0x0C, "KEY_MINUS"},  {0x10, "KEY_Q"},     {0x11, "KEY_W"},      {0x12, "KEY_E"},
    {0x13, "KEY_R"},      {0x14, "KEY_T"},      {0x15, "KEY_Y"},     {0x16, "KEY_U"},      {0x17, "KEY_I"},
    {0x18, "KEY_O"},      {0x19, "KEY_P"},      {0x1E, "KEY_A"},     {0x1F, "KEY_S"},      {0x20, "KEY_D"},
    {0x21, "KEY_F"},      {0x22, "KEY_G"},      {0x23, "KEY_H"},     {0x24, "KEY_J"},      {0x25, "KEY_K"},
    {0x26, "KEY_L"},      {0x91, "KEY_AT"},     {0x2C, "KEY_Z"},     {0x2D, "KEY_X"},      {0x2E, "KEY_C"},
    {0x2F, "KEY_V"},      {0x30, "KEY_B"},      {0x31, "KEY_N"},     {0x32, "KEY_M"},      {0x34, "KEY_PERIOD"},
    {0x39, "KEY_SPACE"},  {0x3A, "KEY_CAPITAL"}, {0x2A, "KEY_LSHIFT"}, {0x36, "KEY_RSHIFT"}, {0x38, "KEY_LMENU"},
    {0xB8, "KEY_RMENU"},
};

bool sameText(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) != std::toupper(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// 0x00590C20 / 0x00590CA0: the action's index by its name (any case), -1 for none.
int32_t slotOf(const std::vector<Action>& actions, std::string_view name) {
    for (size_t i = 0; i < actions.size(); ++i) {
        if (sameText(actions[i].name, name)) return static_cast<int32_t>(i);
    }
    return -1;
}

// 0x00590650 / 0x005907F0: the binding added to the action of the name (as written), the action made when none is.
void addBinding(std::vector<Action>& actions, const std::string& name, const Binding& b) {
    for (Action& a : actions) {
        if (a.name == name) {
            a.bindings.push_back(b);
            return;
        }
    }
    Action a;
    a.name = name;
    a.bindings.push_back(b);
    actions.push_back(std::move(a));
}

// A DIGITAL binding's value from its raw one (0x005900F0): OneShot only as it comes on.
void digitalStep(Binding& b, int32_t raw) {
    b.raw = raw;
    if (!b.oneShot) {
        b.value = raw;
        return;
    }
    if (raw != 0 && b.previous == 0) {
        b.value = 1;
    } else {
        b.value = 0;
    }
    b.previous = raw;
}

}  // namespace

Control Device::control(const std::string& text, ControlKind kind) {
    const ControlText t = splitControl(text);
    const auto s = findSemantic(t.semantic);
    const uint32_t semantic = s ? s->code : 0;
    for (const MapAction& a : format) {
        if (a.semantic == semantic) return {kind, static_cast<int32_t>(a.appData)};
    }
    MapAction a;
    a.appData = static_cast<uint32_t>(values.size());
    a.semantic = semantic;
    a.flags = t.flags;
    a.label = t.label;
    format.push_back(a);
    values.push_back(0);
    valueFlags.push_back(kind == ControlKind::Key ? 1 : 0);
    return {kind, static_cast<int32_t>(a.appData)};
}

void Device::addCharacterKeys(const std::string& name) {
    if (name.find("UI") == std::string::npos) return;
    for (const CharacterKey& k : kCharacterKeys) {
        MapAction a;
        a.appData = 0x80000000u | k.code;
        a.semantic = 0x81000400u | k.code;
        a.label = k.label;
        format.push_back(a);
    }
}

bool Device::hasInput() const {
    if (mapped) {
        for (size_t i = 0; i < values.size(); ++i) {
            if (values[i] != 0 && valueFlags[i] != 0) return true;
        }
    }
    if (controller != nullptr) {
        for (const uint8_t b : buttons) {
            if (b != 0) return true;
        }
    }
    return state[kReturn] != 0;
}

float Device::read(const Control& c, const std::array<int32_t, 2048>& table) const {
    const auto at = static_cast<uint32_t>(c.index);
    switch (c.kind) {
    case ControlKind::Key:
        // 0x00594030: an index past 0x8000 (a character key's) reads 0.
        return at < 0x8000u && at < values.size() && values[at] != 0 ? 1.0f : 0.0f;
    case ControlKind::Axis:
        if (at >= values.size()) return 0.0f;
        return static_cast<float>(values[at]) * 0.01f;  // [0x006FF780]
    case ControlKind::AxisTable: {
        // 0x00593F90 ([0x0079E89C] is never set): (the value + 100) x 5.12 made whole (cvtss2si with the SSE path,
        // [0x0079E5F8] & 2, to the nearest; x87 cut otherwise; here the nearest), at most 0x3FF, the table's entry x
        // 0.0001.
        if (at >= values.size()) return 0.0f;
        const float f = static_cast<float>(values[at] + 100) * 5.12f;  // [0x006FF9D4]
        auto i = static_cast<uint32_t>(x87::nearest(f));
        if (i > 0x3FF) i = 0x3FF;
        return static_cast<float>(table[i]) * 0.0001f;  // [0x006FF9D0]
    }
    }
    return 0.0f;
}

std::array<int32_t, 1024> responseTable(int32_t nonlinearity) {
    std::array<int32_t, 1024> t{};
    const float k = static_cast<float>(nonlinearity) * 0.01f;  // [0x006FF780]
    const float linear = 1.0f - k;
    for (int i = 0; i < 0x3FF; ++i) {
        const float x = (static_cast<float>(i) - 511.0f) * (1.0f / 511.0f);  // [0x006FF77C], [0x006FF778]
        const float cube = x * x * x * k;
        const float v = (linear * x + cube) * 10000.0f;  // [0x006FF784]
        t[static_cast<size_t>(i)] = x87::chop(v);
    }
    t[0x3FF] = t[0x3FE];
    return t;
}

Input::Input() {
    m_keyboard.appData.fill(-1);
    setControllers({});
}

void Input::load(const std::vector<ActionMap>& maps) {
    m_maps = maps;
    size_t count = 0;
    for (const ActionMap& m : maps) count += m.actions.size();
    log::info("input: {} action map(s), {} action(s)", maps.size(), count);
}

void Input::setControllers(const std::vector<ControllerPort*>& controllers) {
    m_devices.clear();
    m_controllers.clear();
    m_formats.clear();
    for (ControllerPort* c : controllers) {
        Device d;
        d.controller = c;
        m_devices.push_back(std::move(d));
    }
    // 0x00591335: no controller, one device of the keyboard alone.
    if (m_devices.empty()) m_devices.emplace_back();
    for (size_t i = 0; i < m_devices.size(); ++i) {
        Controller c;
        c.device = static_cast<int32_t>(i);
        c.ui.fill(-1);
        c.driver.fill(-1);
        c.uiAxes.fill(-1);
        c.driverAxes.fill(-1);
        m_controllers.push_back(c);
    }
    m_formats.resize(m_devices.size());
    m_players = {-1, -1};
    // 0x005914D0: the FF manager's table of channel 0 with 20, copied to its +0x220.
    m_table.fill(0);
    const auto t = responseTable(20);
    std::copy(t.begin(), t.end(), m_table.begin());
}

uint32_t Input::setting(save::Settings::Field field) const {
    if (m_settings == nullptr || !m_settings->has_value()) return 0;
    return (*m_settings)->u32(field);
}

Device* Input::firstDevice() {
    if (m_devices.empty()) return nullptr;
    const int32_t p = m_players[0];
    if (p >= 0 && static_cast<size_t>(p) < m_devices.size()) return &m_devices[static_cast<size_t>(p)];
    return &m_devices.back();
}

const Device* Input::firstDevice() const {
    return const_cast<Input*>(this)->firstDevice();
}

Input::Controller* Input::firstController() {
    if (m_controllers.empty()) return nullptr;
    const int32_t p = m_players[0];
    if (p >= 0 && static_cast<size_t>(p) < m_controllers.size()) return &m_controllers[static_cast<size_t>(p)];
    return &m_controllers.front();
}

Input::Controller* Input::playerController(int player) {
    if (player == 0) return firstController();
    const int32_t p = m_players[static_cast<size_t>(player)];
    if (p < 0 || static_cast<size_t>(p) >= m_controllers.size()) return nullptr;
    return &m_controllers[static_cast<size_t>(p)];
}

// 0x00595890: a device with a map loses its actions, its DIACTIONs and values, and goes raw: its controller, the
// keyboard and the mouse unacquired and set to their plain formats (c_dfDIJoystick2, c_dfDIKeyboard, c_dfDIMouse).
// A device without a map is left as it is.
void Input::empty(Device& d) {
    if (!d.mapped) return;
    d.digital.clear();
    d.analogue.clear();
    d.format.clear();
    d.values.clear();
    d.valueFlags.clear();
    d.mapped = false;
    d.mapName.clear();
    if (d.controller != nullptr) {
        d.controller->unacquire();
        d.controller->setRawFormat();
    }
    m_keyboard.mapped = false;
    m_keyboard.appData.fill(-1);
    m_keyboard.buffer.clear();
}

// 0x0058F890: the first map of the name whose Device is the device's name ("ACTION_MAP") or none; without one nothing
// is made. Each action's control by its semantic's kind: a key or a button (0) or an axis (1, through the response
// table for STEERING with the steering wheel on, the manager's +0x38); another semantic passes the action over. Then
// the device registers the map (0x005954D0).
void Input::registerMap(const std::string& name, Device& d) {
    const ActionMap* map = nullptr;
    for (const ActionMap& m : m_maps) {
        if (sameText(m.name, name) && (m.device.empty() || sameText(m.device, "ACTION_MAP"))) {
            map = &m;
            break;
        }
    }
    if (map == nullptr) return;
    d.addCharacterKeys(name);
    for (const ControlAction& a : map->actions) {
        const auto s = findSemantic(splitControl(a.controlText).semantic);
        if (!s) continue;
        const float sign = a.negative ? -1.0f : 1.0f;
        if (s->kind == 0) {
            const Control c1 = d.control(a.controlText, ControlKind::Key);
            const bool two = !a.control2Text.empty();
            const Control c2 = two ? d.control(a.control2Text, ControlKind::Key) : Control{};
            Binding b;
            if (!a.analogue) {
                // 0x00590050: made with the key's value then.
                b.type = Binding::Type::DigitalKey;
                b.control = c1;
                b.oneShot = a.oneShot;
                b.previous = a.oneShot ? 1 : 0;
                digitalStep(b, static_cast<int32_t>(d.read(c1, m_table)));
                addBinding(d.digital, a.name, b);
            } else {
                b.type = two ? Binding::Type::AnalogueKeys : Binding::Type::AnalogueKey;
                b.control = c1;
                b.control2 = c2;
                b.attack = a.attack;
                b.decay = a.decay;
                addBinding(d.analogue, a.name, b);
            }
        } else if (s->kind == 1) {
            const ControlKind kind =
                m_wheel && sameText("STEERING", a.name) ? ControlKind::AxisTable : ControlKind::Axis;
            const Control c1 = d.control(a.controlText, kind);
            const bool two = !a.control2Text.empty();
            const Control c2 = two ? d.control(a.control2Text, kind) : Control{};
            Binding b;
            b.control = c1;
            b.control2 = c2;
            if (!a.analogue) {
                b.type = Binding::Type::DigitalAxis;
                b.oneShot = a.oneShot;
                b.previous = a.oneShot ? 1 : 0;
                b.deadzone = a.deadzone;
                b.sign = sign;
                addBinding(d.digital, a.name, b);
            } else if (!two) {
                // 0x00590200; 1.01 (0x005920F0): a deadzone of 1% (0.009 .. 0.011) taken as 20% before the rest,
                // any other deadzone of the map as it is.
                b.type = Binding::Type::AnalogueAxis;
                const float deadzone = a.deadzone >= 0.009f && a.deadzone <= 0.011f ? 0.2f : a.deadzone;
                b.deadzone = deadzone;
                b.scale = a.saturation > deadzone ? 1.0f / (a.saturation - deadzone) : 0.0f;
                b.attack = a.attack;
                b.decay = a.decay;
                b.sign = sign;
                addBinding(d.analogue, a.name, b);
            } else {
                b.type = Binding::Type::AnalogueAxes;
                addBinding(d.analogue, a.name, b);
            }
        }
    }
    // 0x005954D0: the controller, the keyboard and the mouse unacquired; BuildActionMap and SetActionMap on the
    // controller and the keyboard (not the mouse); the device mapped, its controller's and keyboard's states cleared.
    MapFormat f;
    f.name = name;
    f.guid = parseGuid(map->guid);
    f.actions = d.format;
    if (d.controller != nullptr) {
        d.controller->unacquire();
        d.controller->setActionMap(f);
    }
    m_keyboard.mapped = true;
    m_keyboard.buffer.clear();
    mapKeyboard(f);
    const size_t at = static_cast<size_t>(&d - m_devices.data());
    if (at < m_formats.size()) m_formats[at] = f;
    d.mapped = true;
    d.mapName = name;
    d.buttons.fill(0);
    d.state.fill(0);
}

// 0x00590B50: nothing while the device is not connected; each DIGITAL action (0x00590590: on while a binding is),
// then each ANALOGUE action by dt (0x005905E0: its binding's value of the greatest size, the first of equal ones).
void Input::step(Device& d, float dt) {
    if (d.connected == 0) return;
    for (Action& a : d.digital) {
        a.on = 0;
        for (Binding& b : a.bindings) {
            if (b.type == Binding::Type::DigitalAxis) {
                // 0x00590130.
                const float v = d.read(b.control, m_table) * b.sign;
                digitalStep(b, v > b.deadzone ? 1 : 0);
            } else {
                digitalStep(b, static_cast<int32_t>(d.read(b.control, m_table)));
            }
            if (b.value != 0) a.on = 1;
        }
    }
    for (Action& a : d.analogue) {
        a.value = 0.0f;
        for (Binding& b : a.bindings) {
            switch (b.type) {
            case Binding::Type::AnalogueKey: {
                // 0x00590180.
                const bool on = d.read(b.control, m_table) != 0.0f;
                float v = on ? dt * b.attack : -(dt * b.decay);
                v = v + b.level;
                b.level = v > 1.0f ? 1.0f : (v < 0.0f ? 0.0f : v);
                break;
            }
            case Binding::Type::AnalogueKeys: {
                // 0x00590360.
                const bool first = d.read(b.control, m_table) != 0.0f;
                const bool second = d.read(b.control2, m_table) != 0.0f;
                if (first) {
                    if (0.0f < b.level) b.level = 0.0f;
                    b.level = b.level - dt * b.attack;
                }
                if (second) {
                    if (b.level < 0.0f) b.level = 0.0f;
                    b.level = dt * b.attack + b.level;
                }
                if (!first && !second) {
                    float s = std::fabs(dt * b.decay);
                    if (!(s <= std::fabs(b.level))) s = std::fabs(b.level);
                    if (0.0f < b.level) s = -s;
                    b.level = s + b.level;
                }
                if (b.level <= 1.0f && b.level < -1.0f) {
                    b.level = -1.0f;
                } else if (1.0f < b.level) {
                    b.level = 1.0f;
                }
                break;
            }
            case Binding::Type::AnalogueAxis: {
                // 0x00590260: past the dead zone (either way) less it, x the scale, within -1 .. 1; a target of the
                // other side clears the value; it goes by Attack a second toward a target further on its side, else
                // by Decay.
                const float v = d.read(b.control, m_table) * b.sign;
                float t = 0.0f;
                if (v > b.deadzone) {
                    t = v - b.deadzone;
                } else if (v < -b.deadzone) {
                    t = v + b.deadzone;
                }
                t = t * b.scale;
                if (1.0f < t) {
                    t = 1.0f;
                } else if (-1.0f > t) {
                    t = -1.0f;
                }
                if (t * b.level < 0.0f) b.level = 0.0f;
                const float diff = t - b.level;
                const float rate = diff * t > 0.0f ? dt * b.attack : dt * b.decay;
                float change = diff;
                if (std::fabs(diff) > rate) change = diff > 0.0f ? rate : -rate;
                b.level = change + b.level;
                break;
            }
            case Binding::Type::AnalogueAxes:
                // 0x0058EA50.
                b.level = d.read(b.control2, m_table) - d.read(b.control, m_table);
                break;
            default:
                break;
            }
            if (std::fabs(a.value) < std::fabs(b.level)) a.value = b.level;
        }
    }
}

// 0x00595A80.
void Input::scanCharacters(Device& d) {
    for (const auto& [key, c] : kCharacters) {
        const auto code = static_cast<uint8_t>(key);
        const auto i = static_cast<uint8_t>(c);
        if (d.state[code] != 0) {
            if (!d.charDown[i]) {
                d.typed = c;
                d.typedAny = true;
            }
            d.charDown[i] = true;
        } else {
            d.charDown[i] = false;
        }
    }
    const auto shift = static_cast<uint8_t>(kShift);
    if (d.state[kLeftShift] != 0 || d.state[kRightShift] != 0) {
        if (!d.charDown[shift]) {
            d.typed = kShift;
            d.typedAny = true;
        }
        d.charDown[shift] = true;
    } else {
        d.charDown[shift] = false;
    }
}

// 0x00594C20: with the window inactive ([0x0075F780] 0; or the 3D device lost, [0x0075F0F8], whose frames the port
// does not run) nothing is read and the device is not connected. Else its controller (Poll, Acquire on failure) gives
// with a map up to 32 buffered events, each its value, or raw its state; the device is connected while that succeeds
// (a device without a controller always). The keyboard gives with a map one buffered event: a character key's state,
// or a key's value, kept 0 while an Alt key's state is down; raw its whole state. Then the actions step.
void Input::pollDevice(Device& d, bool active, float dt) {
    if (!active) {
        d.connected = 0;
        return;
    }
    if (d.controller != nullptr) {
        const ControllerRead r = d.controller->read(d.mapped);
        if (r.ok) {
            if (d.mapped) {
                for (const auto& [appData, data] : r.events) {
                    if (appData < d.values.size()) d.values[appData] = static_cast<int32_t>(data);
                }
            } else {
                d.buttons = r.buttons;
            }
        }
        d.connected = r.ok ? 1 : 0;
    } else {
        d.connected = 1;
    }
    d.typedAny = false;
    d.typed = 0;
    if (d.mapped) {
        if (!m_keyboard.buffer.empty()) {
            const KeyEvent e = m_keyboard.buffer.front();
            m_keyboard.buffer.pop_front();
            const auto appData = static_cast<uint32_t>(m_keyboard.appData[e.code]);
            if ((appData & 0x80000000u) != 0) {
                d.state[appData & 0xFF] = e.down ? 0x80 : 0;
                scanCharacters(d);
            } else if (appData < d.values.size()) {
                int32_t& v = d.values[appData];
                v = e.down ? 0x80 : 0;
                if (v != 0 && (d.state[kLeftAlt] != 0 || d.state[kRightAlt] != 0)) v = 0;
            }
        }
    } else {
        for (size_t i = 0; i < 256; ++i) d.state[i] = m_keyboard.physical[i] ? 0x80 : 0;
        scanCharacters(d);
    }
    step(d, dt);
}

void Input::poll(const std::vector<KeyEvent>& events, bool active, float dt) {
    // The keyboard: DirectInput buffers the keys of its action map while the window is the active one (inactive, it
    // is unacquired and its buffer is lost). Acquired again with the focus it reports the release of each key it last
    // gave down that went up meanwhile: Alt after Alt+Tab, whose press the device may have taken before the focus went
    // (held, it zeroes every mapped key, 0x00594D95: the keyboard dead until Alt alone is pressed again).
    if (active && !m_keyboard.acquired) {
        for (size_t code = 0; code < m_keyboard.buffered.size(); ++code) {
            if (!m_keyboard.buffered[code] || m_keyboard.physical[code]) continue;
            m_keyboard.buffered[code] = false;
            if (m_keyboard.mapped && m_keyboard.appData[code] != -1) {
                m_keyboard.buffer.push_back({static_cast<uint8_t>(code), false});
            }
        }
    }
    m_keyboard.acquired = active;
    for (const KeyEvent& e : events) {
        m_keyboard.physical[e.code] = e.down;
        if (active && m_keyboard.mapped && m_keyboard.appData[e.code] != -1) {
            m_keyboard.buffer.push_back(e);
            m_keyboard.buffered[e.code] = e.down;
        }
    }
    if (!active) m_keyboard.buffer.clear();
    // 0x0058FE50: the first player's device (0x005918E0) by the polls before.
    takePlayer(0);
    for (Device& d : m_devices) pollDevice(d, active, dt);
    // 0x005918C0: then the FF manager's channels read (0x00592050).
    if (m_wheels != nullptr) m_wheels->read();
}

void Input::takePlayer(int player) {
    if (m_players[static_cast<size_t>(player)] != -1) return;
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (!m_devices[i].hasInput()) continue;
        const auto at = static_cast<int32_t>(i);
        if (m_players[1] != at && m_players[0] != at) {
            m_devices[i].index = at;
            m_players[static_cast<size_t>(player)] = at;
        }
    }
}

int32_t Input::slotValue(const Device& d, int32_t slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= d.digital.size()) return 0;
    return d.digital[static_cast<size_t>(slot)].on;
}

float Input::slotAnalogue(const Device& d, int32_t slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= d.analogue.size()) return 0.0f;
    return d.analogue[static_cast<size_t>(slot)].value;
}

UiFrame Input::uiFrame(float dt, int player) {
    UiFrame f;
    f.anyInput = anyInput();
    Controller* c = playerController(player);
    if (c == nullptr) return f;
    const Device& d = m_devices[static_cast<size_t>(c->device)];
    const auto on = [&](int slot) { return slotValue(d, c->ui[static_cast<size_t>(slot)]) != 0; };

    // Directions: one event, then again after 0.25 s and every 0.1 s while held.
    if (!on(kActionLeft) && !on(kActionRight) && !on(kActionUp) && !on(kActionDown)) {
        c->repeat = 0.0f;
        c->lastDirection = kNone;
    }
    if (c->repeat != 0.0f) {
        c->repeat = std::clamp(c->repeat - dt, 0.0f, 1.0f);
    } else {
        uint32_t direction = kNone;
        if (on(kActionLeft)) direction = kLeft;
        else if (on(kActionRight)) direction = kRight;
        else if (on(kActionUp)) direction = kUp;
        else if (on(kActionDown)) direction = kDown;
        if (direction != kNone) {
            f.event = direction;
            c->repeat = c->lastDirection == direction ? 0.1f : 0.25f;
        }
        c->lastDirection = direction;
    }

    f.start = on(kActionStart);
    if (on(kActionSelect) || on(kActionStart)) f.event = kSelect;
    else if (on(kActionBack) || on(kActionBack2)) f.event = kBack;
    else if (on(kActionSelect2)) f.event = kSelect2;
    else if (on(kActionSelect3)) f.event = kSelect3;
    else if (on(kActionSelect4)) f.event = kSelect4;
    else if (on(kActionSelect5)) f.event = kSelect5;
    else if (on(kActionSelect6)) f.event = kSelect6;
    else if (on(kActionSelect7)) f.event = kSelect7;

    if (on(kScrollLeft)) f.scroll = kLeft;
    else if (on(kScrollRight)) f.scroll = kRight;
    else if (on(kScrollUp)) f.scroll = kUp;
    else if (on(kScrollDown)) f.scroll = kDown;
    for (size_t i = 0; i < 4; ++i) f.axes[i] = slotAnalogue(d, c->uiAxes[i]);
    f.connected = d.connected != 0;
    return f;
}

void Input::useUi(int player) {
    // 0x00407FB0: the FF manager's channels with a device let go first (0x0058F650).
    if (m_wheels != nullptr) {
        for (int ch = 0; ch < ff::Wheels::kChannels; ++ch) {
            if (m_wheels->hasDevice(ch)) m_wheels->letGo(ch);
        }
    }
    Controller* c = playerController(player);
    if (c == nullptr) return;
    Device& d = m_devices[static_cast<size_t>(c->device)];
    empty(d);
    c->ui.fill(-1);
    c->driver.fill(-1);
    c->uiAxes.fill(-1);
    c->driverAxes.fill(-1);
    m_wheel = setting(save::Settings::kSteeringWheel) != 0;
    registerMap("UI", d);
    for (size_t i = 0; i < c->ui.size(); ++i) c->ui[i] = slotOf(d.digital, kUiDigital[i]);
    for (size_t i = 0; i < c->uiAxes.size(); ++i) c->uiAxes[i] = slotOf(d.analogue, kUiAnalogue[i]);
    c->map = Map::Ui;
}

void Input::useDriver(int player) {
    Controller* c = playerController(player);
    if (c == nullptr) return;
    Device& d = m_devices[static_cast<size_t>(c->device)];
    // The map of the device's settings (the save system's +0xCC by the device's number; DRIVER1 .. DRIVER4; a value
    // past them would read past the table 0x0071152C, here DRIVER1).
    const auto chosen = static_cast<int32_t>(setting(static_cast<save::Settings::Field>(
        save::Settings::kDriverMap + 4 * static_cast<uint32_t>(std::max(d.index, 0)))));
    const int32_t which = chosen >= 0 && chosen < 4 ? chosen : 0;
    empty(d);
    c->ui.fill(-1);
    c->driver.fill(-1);
    c->uiAxes.fill(-1);
    c->driverAxes.fill(-1);
    m_wheel = setting(save::Settings::kSteeringWheel) != 0;
    // The FF manager's table of channel 0 again, by the settings' +0xAC (the first player's): not speed sensitive 20,
    // else -20; copied to its +0x220.
    const auto t = responseTable(setting(save::Settings::kWheelSpeed) != 0 ? 20 : -20);
    std::copy(t.begin(), t.end(), m_table.begin());
    registerMap(kDriverMaps[which], d);
    for (size_t i = 0; i < c->driver.size(); ++i) c->driver[i] = slotOf(d.digital, kDriverDigitalNames[i]);
    for (size_t i = 0; i < c->driverAxes.size(); ++i) c->driverAxes[i] = slotOf(d.analogue, kDriverAnalogueNames[i]);
    c->map = Map::Driver;
}

ff::Rumble& Input::rumble(int player) {
    static ff::Rumble none;
    Controller* c = playerController(player);
    return c != nullptr ? c->rumble : none;
}

void Input::release() {
    m_players = {-1, -1};
    for (Device& d : m_devices) empty(d);
    for (Controller& c : m_controllers) c.map = Map::None;
}

Input::Map Input::map() const {
    const Device* d = firstDevice();
    if (d == nullptr || !d->mapped) return Map::None;
    return sameText(d->mapName, "UI") ? Map::Ui : Map::Driver;
}

bool Input::hasInput() const {
    const Device* d = firstDevice();
    return d != nullptr && d->hasInput();
}

bool Input::anyInput() const {
    for (const Device& d : m_devices) {
        if (d.typedAny || d.hasInput()) return true;
    }
    return false;
}

bool Input::value(uint8_t code) const {
    const Device* d = firstDevice();
    if (d == nullptr) return false;
    const int64_t at = m_keyboard.appData[code];
    return at >= 0 && static_cast<size_t>(at) < d->values.size() && d->values[static_cast<size_t>(at)] != 0;
}

char Input::typed() const {
    const Device* d = firstDevice();
    return d != nullptr && d->typedAny ? d->typed : 0;
}

bool Input::held(char c) const {
    const Device* d = firstDevice();
    return d != nullptr && d->charDown[static_cast<uint8_t>(c)];
}

int32_t Input::digital(size_t slot, int player) const {
    const Controller* c = const_cast<Input*>(this)->playerController(player);
    if (c == nullptr || slot >= c->driver.size()) return 0;
    return slotValue(m_devices[static_cast<size_t>(c->device)], c->driver[slot]);
}

float Input::analogue(size_t slot, int player) const {
    const Controller* c = const_cast<Input*>(this)->playerController(player);
    if (c == nullptr || slot >= c->driverAxes.size()) return 0.0f;
    return slotAnalogue(m_devices[static_cast<size_t>(c->device)], c->driverAxes[slot]);
}

void Input::recreateControllers() {
    // 0x00590F40: each device's +0x30 (0x00596210); then 0x005957D6: the controller unacquired, BuildActionMap and
    // SetActionMap with the format of the configuring device, the keyboard likewise.
    for (Device& d : m_devices) {
        if (d.controller != nullptr) d.controller->recreate();
    }
    for (size_t i = 0; i < m_devices.size(); ++i) {
        Device& d = m_devices[i];
        if (d.controller != nullptr && d.mapped) {
            d.controller->unacquire();
            d.controller->setActionMap(m_formats[i]);
        }
    }
    if (const MapFormat* f = playerFormat(0); f != nullptr && m_keyboard.mapped) mapKeyboard(*f);
}

// 0x005954D0's keyboard: the keys its map gives the actions (the first action of a key keeps it). Without the
// platform's keyboard each DIKEYBOARD_ action (0x81000400 | the key) on its key, as DirectInput's default map gives them.
void Input::mapKeyboard(const MapFormat& f) {
    m_keyboard.appData.fill(-1);
    if (m_keyboardPort != nullptr) {
        for (const auto& [key, appData] : m_keyboardPort->setActionMap(f)) {
            int64_t& slot = m_keyboard.appData[key];
            if (slot == -1) slot = appData;
        }
        return;
    }
    for (const MapAction& a : f.actions) {
        if ((a.semantic & 0xFFFFFF00u) == 0x81000400u) {
            int64_t& slot = m_keyboard.appData[a.semantic & 0xFF];
            if (slot == -1) slot = a.appData;
        }
    }
}

const MapFormat* Input::playerFormat(int player) const {
    const Controller* c = const_cast<Input*>(this)->playerController(player);
    if (c == nullptr || c->device < 0 || static_cast<size_t>(c->device) >= m_formats.size()) return nullptr;
    return &m_formats[static_cast<size_t>(c->device)];
}

}  // namespace oj::input
