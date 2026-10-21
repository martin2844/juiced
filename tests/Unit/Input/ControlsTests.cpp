// Unit tests: Controls.

#include "Framework/Test.h"

#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"

#include <array>
#include <cstdio>

namespace {

using oj::test::check;

}  // namespace

// The controls' texts (0x00593D70), the semantics' table (0x0071E060) and a map's GUID line (0x00595490).
OJ_UNIT_TEST(Controls, Texts) {
    const oj::ControlText t = oj::splitControl("DIKEYBOARD_ESCAPE 'Pause'  APPFIXED");
    const oj::ControlText f = oj::splitControl("DIAXIS_DRIVINGR_STEER 'Steering' forcefeedback norange");
    const auto steer = oj::findSemantic("diaxis_drivingr_steer");
    const auto button = oj::findSemantic("DIBUTTON_ANY(11)");
    const std::array<uint8_t, 16> guid = oj::parseGuid("0x2E5615A0-0x8211-0x4bb4-0x81-0xBA-0x7C-0xB6-0x21-0xBF-0x90-0x1F");
    const std::array<uint8_t, 16> want = {0xA0, 0x15, 0x56, 0x2E, 0x11, 0x82, 0xB4, 0x4B,
                                          0x81, 0xBA, 0x7C, 0xB6, 0x21, 0xBF, 0x90, 0x1F};
    const std::array<uint8_t, 16> shortField = oj::parseGuid("0xba3f36fe-0x24b9-0x4ef3-0xaa-0x7a-0x7-0xa6-0x9b-0xa0-0x44-0xbe");
    check(t.semantic == "DIKEYBOARD_ESCAPE" && t.label == "Pause" && t.flags == 0x10 && f.flags == 9 && steer &&
              steer->code == 0x01008A01u && steer->kind == 1 && button && button->code == 0xFF00440Bu &&
              button->kind == 0 && !oj::findSemantic("DIAXIS_ANY_Z_1") && oj::keyboardCode("DIKEYBOARD_V") == 0x2F &&
              guid == want && shortField[10] == 0x07,
          "controls: a control's semantic, label and flags; the semantics' table; a map's GUID");
}
// The axis response table (0x00596C00): ((1 - k) x + k x^3) x 10000 cut, x = (i - 511) / 511, the last entry
// the one before.
OJ_UNIT_TEST(Controls, AxisResponse) {
    const auto t20 = oj::input::responseTable(20);
    const auto tm20 = oj::input::responseTable(-20);
    std::printf("     response table: %d %d %d %d | %d %d\n", t20[0], t20[511], t20[767], t20[1023], tm20[0],
                tm20[767]);
    check(t20[0] == -10000 && t20[511] == 0 && t20[1022] == 10000 && t20[1023] == t20[1022] && tm20[0] == -10000 &&
              t20[767] < 256 * 10000 / 511 && tm20[767] > 256 * 10000 / 511 && t20[767] == -t20[255],
          "axis response table: its ends, its middle, the curve each way by the nonlinearity");
}
