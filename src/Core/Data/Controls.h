#pragma once
// The controls file of the stock game, scripts\controls_dx9_<language letter>.txt (text):
//
//   [BeginActionMap]
//   Name UI|DRIVER1|DEBUG
//   Device ACTION_MAP|KEYBOARD_MOUSE|GAME_DEVICE
//   GUID ...
//   [BeginAction]
//   Name <action>
//   Type DIGITAL|ANALOGUE
//   OneShot
//   Control "<semantic> '<label>'"      e.g. DIKEYBOARD_RETURN, DIBUTTON_ANY(0), DIAXIS_ANY_X_1
//   Control2 "<semantic> '<label>'"
//   Deadzone <f>  Saturation <f>  Sign NEGATIVE
//   [EndAction]
//   [EndActionMap]
//
// Lines starting with ';' are comments. An action may be listed several times, once per control. The game uses the
// maps of Device ACTION_MAP.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

struct ControlAction {
    std::string name;
    bool analogue = false;
    bool oneShot = false;
    std::string control;   // the semantic, without the label
    std::string control2;
    // The control's text as written ("<semantic> '<label>' <flags>"), read again as the device makes its control
    // (0x00593D70).
    std::string controlText;
    std::string control2Text;
    // The action's defaults as the parser makes one (0x0058F165): Deadzone 0.2, Saturation 1, Sign 1, Attack and
    // Decay 1000.
    float deadzone = 0.2f;
    float saturation = 1.0f;
    bool negative = false;
    float attack = 1000.0f;  // Attack: the rise a second of an analogue action made of keys
    float decay = 1000.0f;   // Decay: its fall a second
};

struct ActionMap {
    std::string name;
    std::string device;
    std::string guid;  // the GUID line as written ("0x2E5615A0-0x8211-0x4bb4-0x81-..."), the map's guidActionMap
    std::vector<ControlAction> actions;
};

std::vector<ActionMap> parseControls(std::string_view text);

// A semantic of the stock table (0x0071E060: name, DirectInput semantic, kind): the axes of any controller
// (DIAXIS_ANY_*, kind 1), its buttons (DIBUTTON_ANY(n)), the racing genre's axes and buttons (DIAXIS_DRIVINGR_*,
// DIBUTTON_DRIVINGR_*) and the keyboard's keys (DIKEYBOARD_*, their key code the semantic's low byte).
struct Semantic {
    std::string_view name;
    uint32_t code = 0;
    int32_t kind = 0;  // 1 an axis, 0 a button or a key
};

// 0x00593D00: the semantic of the name (any case), nullopt for a name the table has not.
std::optional<Semantic> findSemantic(std::string_view name);

// 0x00593D70: a control's text split: the semantic up to the first space, the label between the two quotes (the
// DIACTION's name), the flags after them (any case: APPFIXED 0x10, APPMAPPED 2, APPNOMAP 4, FORCEFEEDBACK 1,
// NORANGE 8).
struct ControlText {
    std::string semantic;
    std::string label;
    uint32_t flags = 0;
};
ControlText splitControl(std::string_view text);

// 0x00595490 (with 0x005953D0 for each field): the GUID line's fields, each "0x" and hexadecimal digits up to the
// next character that is not one (skipped): Data1, Data2, Data3 and the eight bytes of Data4, in the GUID's byte
// order.
std::array<uint8_t, 16> parseGuid(std::string_view text);

// The DirectInput keyboard code of a DIKEYBOARD_<name> semantic, for the names the stock game knows.
std::optional<uint8_t> keyboardCode(std::string_view semantic);

}  // namespace oj
