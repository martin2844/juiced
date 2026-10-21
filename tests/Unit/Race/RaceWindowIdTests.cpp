// Unit tests: RaceWindowId.

#include "Framework/Test.h"

#include "Game/Race/Hud/RaceWindowId.h"

#include <string>

namespace {

using oj::test::check;
using oj::RaceWindowId;

}  // namespace

// The race front end's windows by name: case ignored as the window manager does, "" none, an unknown name other;
// each window's name gives it back.
OJ_UNIT_TEST(RaceWindowId, Names) {
    check(oj::raceWindowId("") == RaceWindowId::None && oj::raceWindowId("hud_pause") == RaceWindowId::Pause &&
              oj::raceWindowId("HUD_Race_Results") == RaceWindowId::RaceResults &&
              oj::raceWindowId("closedown") == RaceWindowId::Closedown &&
              oj::raceWindowId("savereplay") == RaceWindowId::Other &&
              std::string(oj::raceWindowName(RaceWindowId::None)).empty() &&
              std::string(oj::raceWindowName(RaceWindowId::Other)).empty(),
          "the names, case ignored");
    bool back = true;
    for (int i = static_cast<int>(RaceWindowId::Blank); i <= static_cast<int>(RaceWindowId::Autosave); ++i) {
        const auto id = static_cast<RaceWindowId>(i);
        back = back && oj::raceWindowId(oj::raceWindowName(id)) == id;
    }
    check(back, "every window's name gives it back");
    const oj::RaceWindowShown shown("Hud_Spectator");
    check(shown.id == RaceWindowId::Spectator && shown.name == "Hud_Spectator", "the window shown keeps its name");
}
