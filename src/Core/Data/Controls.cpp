#include "Core/Data/Controls.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace oj {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

// "DIKEYBOARD_RETURN 'Select'" -> DIKEYBOARD_RETURN; also a bare word.
std::string semanticOf(std::string_view value) {
    value = trim(value);
    if (!value.empty() && value.front() == '"') value.remove_prefix(1);
    const size_t end = value.find_first_of(" \t'\"");
    return std::string(value.substr(0, end));
}

// The value without its outer double quotes, as the reader gives a quoted string.
std::string unquoted(std::string_view value) {
    value = trim(value);
    if (!value.empty() && value.front() == '"') value.remove_prefix(1);
    if (!value.empty() && value.back() == '"') value.remove_suffix(1);
    return std::string(value);
}

// The semantics of the stock table (0x0071E060), in its order. The DirectInput semantics are those of dinput.h: an
// axis of the racing genre (DIVIRTUAL_DRIVING_RACE 0x01000000), the "any" axes and buttons (0xFF......), the keyboard's
// keys (0x81000400 + the key's code).
constexpr Semantic kSemantics[] = {
    {"DIAXIS_ANY_X_1", 0xFF00C201u, 1},
    {"DIAXIS_ANY_Y_1", 0xFF014201u, 1},
    {"DIAXIS_ANY_A_1", 0xFF03C201u, 1},
    {"DIAXIS_ANY_B_1", 0xFF044201u, 1},
    {"DIAXIS_ANY_X_2", 0xFF00C202u, 1},
    {"DIAXIS_ANY_Y_2", 0xFF014202u, 1},
    {"DIBUTTON_ANY(0)", 0xFF004400u, 0},
    {"DIBUTTON_ANY(1)", 0xFF004401u, 0},
    {"DIBUTTON_ANY(2)", 0xFF004402u, 0},
    {"DIBUTTON_ANY(3)", 0xFF004403u, 0},
    {"DIBUTTON_ANY(4)", 0xFF004404u, 0},
    {"DIBUTTON_ANY(5)", 0xFF004405u, 0},
    {"DIBUTTON_ANY(6)", 0xFF004406u, 0},
    {"DIBUTTON_ANY(7)", 0xFF004407u, 0},
    {"DIBUTTON_ANY(8)", 0xFF004408u, 0},
    {"DIBUTTON_ANY(9)", 0xFF004409u, 0},
    {"DIBUTTON_ANY(10)", 0xFF00440Au, 0},
    {"DIBUTTON_ANY(11)", 0xFF00440Bu, 0},
    {"DIAXIS_DRIVINGR_ACCELERATE", 0x01039202u, 1},
    {"DIAXIS_DRIVINGR_BRAKE", 0x01041203u, 1},
    {"DIAXIS_DRIVINGR_STEER", 0x01008A01u, 1},
    {"DIBUTTON_DRIVINGR_MENU", 0x010004FDu, 0},
    {"DIBUTTON_DRIVINGR_SHIFTDOWN", 0x01000C02u, 0},
    {"DIBUTTON_DRIVINGR_SHIFTUP", 0x01000C01u, 0},
    {"DIBUTTON_DRIVINGR_VIEW", 0x01001C03u, 0},
    {"DIAXIS_DRIVINGR_ACCEL_AND_BRAKE", 0x01014A04u, 1},
    {"DIBUTTON_DRIVINGR_ACCELERATE_LINK", 0x0103D4E0u, 0},
    {"DIBUTTON_DRIVINGR_AIDS", 0x01004406u, 0},
    {"DIBUTTON_DRIVINGR_BOOST", 0x01004408u, 0},
    {"DIBUTTON_DRIVINGR_BRAKE", 0x01004C04u, 0},
    {"DIBUTTON_DRIVINGR_DASHBOARD", 0x01004405u, 0},
    {"DIBUTTON_DRIVINGR_DEVICE", 0x010044FEu, 0},
    {"DIBUTTON_DRIVINGR_GLANCE_LEFT_LINK", 0x0107C4E4u, 0},
    {"DIBUTTON_DRIVINGR_GLANCE_RIGHT_LINK", 0x0107C4ECu, 0},
    {"DIBUTTON_DRIVINGR_MAP", 0x01004407u, 0},
    {"DIBUTTON_DRIVINGR_PIT", 0x01004409u, 0},
    {"DIBUTTON_DRIVINGR_PAUSE", 0x010044FCu, 0},
    {"DIBUTTON_DRIVINGR_STEER_LEFT_LINK", 0x0100CCE4u, 0},
    {"DIBUTTON_DRIVINGR_STEER_RIGHT_LINK", 0x0100CCECu, 0},
#ifdef OJ_MACOS
    // Additional keys offered by the native controls editor.
    {"DIKEYBOARD_1", 0x81000402u, 0},
    {"DIKEYBOARD_2", 0x81000403u, 0},
    {"DIKEYBOARD_3", 0x81000404u, 0},
    {"DIKEYBOARD_4", 0x81000405u, 0},
    {"DIKEYBOARD_5", 0x81000406u, 0},
    {"DIKEYBOARD_6", 0x81000407u, 0},
    {"DIKEYBOARD_7", 0x81000408u, 0},
    {"DIKEYBOARD_8", 0x81000409u, 0},
    {"DIKEYBOARD_9", 0x8100040Au, 0},
    {"DIKEYBOARD_0", 0x8100040Bu, 0},
    {"DIKEYBOARD_LCONTROL", 0x8100041Du, 0},
    {"DIKEYBOARD_BACK", 0x8100040Eu, 0},
    {"DIKEYBOARD_HOME", 0x810004C7u, 0},
#endif
    {"DIKEYBOARD_UP", 0x810004C8u, 0},
    {"DIKEYBOARD_DOWN", 0x810004D0u, 0},
    {"DIKEYBOARD_LEFT", 0x810004CBu, 0},
    {"DIKEYBOARD_RIGHT", 0x810004CDu, 0},
    {"DIKEYBOARD_A", 0x8100041Eu, 0},
    {"DIKEYBOARD_B", 0x81000430u, 0},
    {"DIKEYBOARD_C", 0x8100042Eu, 0},
    {"DIKEYBOARD_D", 0x81000420u, 0},
    {"DIKEYBOARD_E", 0x81000412u, 0},
    {"DIKEYBOARD_F", 0x81000421u, 0},
    {"DIKEYBOARD_G", 0x81000422u, 0},
    {"DIKEYBOARD_H", 0x81000423u, 0},
    {"DIKEYBOARD_I", 0x81000417u, 0},
    {"DIKEYBOARD_J", 0x81000424u, 0},
    {"DIKEYBOARD_K", 0x81000425u, 0},
    {"DIKEYBOARD_L", 0x81000426u, 0},
    {"DIKEYBOARD_M", 0x81000432u, 0},
    {"DIKEYBOARD_N", 0x81000431u, 0},
    {"DIKEYBOARD_O", 0x81000418u, 0},
    {"DIKEYBOARD_P", 0x81000419u, 0},
    {"DIKEYBOARD_Q", 0x81000410u, 0},
    {"DIKEYBOARD_R", 0x81000413u, 0},
    {"DIKEYBOARD_S", 0x8100041Fu, 0},
    {"DIKEYBOARD_T", 0x81000414u, 0},
    {"DIKEYBOARD_U", 0x81000416u, 0},
    {"DIKEYBOARD_V", 0x8100042Fu, 0},
    {"DIKEYBOARD_W", 0x81000411u, 0},
    {"DIKEYBOARD_X", 0x8100042Du, 0},
    {"DIKEYBOARD_Y", 0x81000415u, 0},
    {"DIKEYBOARD_Z", 0x8100042Cu, 0},
    {"DIKEYBOARD_RETURN", 0x8100041Cu, 0},
    {"DIKEYBOARD_SPACE", 0x81000439u, 0},
    {"DIKEYBOARD_RCONTROL", 0x8100049Du, 0},
    {"DIKEYBOARD_NEXT", 0x810004D1u, 0},
    {"DIKEYBOARD_PRIOR", 0x810004C9u, 0},
    {"DIKEYBOARD_ESCAPE", 0x81000401u, 0},
    {"DIKEYBOARD_END", 0x810004CFu, 0},
    {"DIKEYBOARD_MINUS", 0x8100040Cu, 0},
    {"DIKEYBOARD_ADD", 0x8100044Eu, 0},
    {"DIKEYBOARD_EQUALS", 0x8100040Du, 0},
    {"DIKEYBOARD_F1", 0x8100043Bu, 0},
    {"DIKEYBOARD_F2", 0x8100043Cu, 0},
    {"DIKEYBOARD_F3", 0x8100043Du, 0},
    {"DIKEYBOARD_F4", 0x8100043Eu, 0},
    {"DIKEYBOARD_F5", 0x8100043Fu, 0},
    {"DIKEYBOARD_F6", 0x81000440u, 0},
    {"DIKEYBOARD_F7", 0x81000441u, 0},
    {"DIKEYBOARD_F8", 0x81000442u, 0},
    {"DIKEYBOARD_F9", 0x81000443u, 0},
    {"DIKEYBOARD_F10", 0x81000444u, 0},
    {"DIKEYBOARD_F11", 0x81000457u, 0},
    {"DIKEYBOARD_F12", 0x81000458u, 0},
    {"DIKEYBOARD_NUMPAD0", 0x81000452u, 0},
    {"DIKEYBOARD_NUMPAD1", 0x8100044Fu, 0},
    {"DIKEYBOARD_NUMPAD2", 0x81000450u, 0},
    {"DIKEYBOARD_NUMPAD3", 0x81000451u, 0},
    {"DIKEYBOARD_NUMPAD4", 0x8100044Bu, 0},
    {"DIKEYBOARD_NUMPAD5", 0x8100044Cu, 0},
    {"DIKEYBOARD_NUMPAD6", 0x8100044Du, 0},
    {"DIKEYBOARD_NUMPAD7", 0x81000447u, 0},
    {"DIKEYBOARD_NUMPAD8", 0x81000448u, 0},
    {"DIKEYBOARD_NUMPAD9", 0x81000449u, 0},
    {"DIKEYBOARD_INSERT", 0x810004D2u, 0},
    {"DIKEYBOARD_DELETE", 0x810004D3u, 0},
    {"DIKEYBOARD_TAB", 0x8100040Fu, 0},
    {"DIKEYBOARD_RSHIFT", 0x81000436u, 0},
    {"DIKEYBOARD_LSHIFT", 0x8100042Au, 0},
    {"DIKEYBOARD_RBRACKET", 0x8100041Bu, 0},
    {"DIKEYBOARD_LBRACKET", 0x8100041Au, 0},
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

// 0x005953D0: "0x" skipped, then hexadecimal digits up to the first other character, which is skipped unless it ends
// the text.
uint32_t guidField(std::string_view& s) {
    s.remove_prefix(std::min<size_t>(2, s.size()));
    uint32_t v = 0;
    while (!s.empty()) {
        const char c = s.front();
        uint32_t d = 0;
        if (c >= '0' && c <= '9') {
            d = static_cast<uint32_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = static_cast<uint32_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = static_cast<uint32_t>(c - 'A' + 10);
        } else {
            break;
        }
        v = (v << 4) + d;
        s.remove_prefix(1);
    }
    if (!s.empty()) s.remove_prefix(1);
    return v;
}

}  // namespace

std::optional<Semantic> findSemantic(std::string_view name) {
    for (const Semantic& s : kSemantics) {
        if (sameText(s.name, name)) return s;
    }
    return std::nullopt;
}

ControlText splitControl(std::string_view text) {
    ControlText t;
    const size_t space = text.find(' ');
    t.semantic = std::string(text.substr(0, space));
    if (space == std::string_view::npos) return t;
    std::string_view rest = text.substr(space + 1);
    const size_t open = rest.find('\'');
    if (open == std::string_view::npos) return t;
    rest.remove_prefix(open + 1);
    const size_t close = rest.find('\'');
    t.label = std::string(rest.substr(0, close));
    if (close == std::string_view::npos) return t;
    std::string flags(rest.substr(close + 1));
    for (char& c : flags) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (flags.find("APPFIXED") != std::string::npos) t.flags |= 0x10;
    if (flags.find("APPMAPPED") != std::string::npos) t.flags |= 2;
    if (flags.find("APPNOMAP") != std::string::npos) t.flags |= 4;
    if (flags.find("FORCEFEEDBACK") != std::string::npos) t.flags |= 1;
    if (flags.find("NORANGE") != std::string::npos) t.flags |= 8;
    return t;
}

std::array<uint8_t, 16> parseGuid(std::string_view text) {
    std::array<uint8_t, 16> g{};
    const uint32_t data1 = guidField(text);
    const uint32_t data2 = guidField(text);
    const uint32_t data3 = guidField(text);
    for (int i = 0; i < 4; ++i) g[static_cast<size_t>(i)] = static_cast<uint8_t>(data1 >> (8 * i));
    g[4] = static_cast<uint8_t>(data2);
    g[5] = static_cast<uint8_t>(data2 >> 8);
    g[6] = static_cast<uint8_t>(data3);
    g[7] = static_cast<uint8_t>(data3 >> 8);
    for (size_t i = 0; i < 8; ++i) g[8 + i] = static_cast<uint8_t>(guidField(text));
    return g;
}

std::vector<ActionMap> parseControls(std::string_view text) {
    std::vector<ActionMap> maps;
    ActionMap* map = nullptr;
    ControlAction* action = nullptr;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line.front() == ';') {
            continue;
        }
        if (line == "[BeginActionMap]") {
            map = &maps.emplace_back();
            action = nullptr;
            continue;
        }
        if (line == "[EndActionMap]") {
            map = nullptr;
            action = nullptr;
            continue;
        }
        if (map == nullptr) {
            continue;
        }
        if (line == "[BeginAction]") {
            action = &map->actions.emplace_back();
            continue;
        }
        if (line == "[EndAction]") {
            action = nullptr;
            continue;
        }
        const size_t space = line.find_first_of(" \t");
        const std::string_view key = line.substr(0, space);
        const std::string_view value = space == std::string_view::npos ? std::string_view() : trim(line.substr(space));
        if (action == nullptr) {
            if (key == "Name") map->name = std::string(value);
            if (key == "Device") map->device = std::string(value);
            if (key == "GUID") map->guid = std::string(value);
            continue;
        }
        if (key == "Name") action->name = std::string(value);
        else if (key == "Type") action->analogue = value == "ANALOGUE";
        else if (key == "OneShot") action->oneShot = true;
        else if (key == "Control") {
            action->control = semanticOf(value);
            action->controlText = unquoted(value);
        } else if (key == "Control2") {
            action->control2 = semanticOf(value);
            action->control2Text = unquoted(value);
        }
        else if (key == "Deadzone") action->deadzone = std::strtof(std::string(value).c_str(), nullptr);
        else if (key == "Saturation") action->saturation = std::strtof(std::string(value).c_str(), nullptr);
        else if (key == "Sign") action->negative = value == "NEGATIVE";
        else if (key == "Attack") action->attack = std::strtof(std::string(value).c_str(), nullptr);
        else if (key == "Decay") action->decay = std::strtof(std::string(value).c_str(), nullptr);
    }
    return maps;
}

std::optional<uint8_t> keyboardCode(std::string_view semantic) {
    const auto s = findSemantic(semantic);
    if (!s || (s->code & 0xFFFFFF00u) != 0x81000400u) return std::nullopt;
    return static_cast<uint8_t>(s->code & 0xFF);
}

}  // namespace oj
