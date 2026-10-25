// Unit tests: the fixed build's bounds of ArcadeRaces.txt (Core/Race/Arcade.cpp).

#include "Framework/Test.h"

#include "Core/Data/ExeTables.h"
#include "Core/Race/Arcade.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The stock writes a seventh Car of a race into the race's paint and a 21st Track into the next championship's list;
// the fixed build keeps six cars and 20 tracks.
OJ_UNIT_TEST(ArcadeRacesBounds, SixCarsTwentyTracks) {
    std::string text = "[UnlockTracks] 1\n";
    for (int i = 0; i < 21; ++i) text += "Track Angel_North_R1_rev\n";
    text += "[EndUnlock]\n[Race] 1\n";
    for (int i = 0; i < 7; ++i) text += "Car " + std::to_string(i + 1) + " fto 1\n";
    text += "[EndRace]\n";
    auto arcade = std::make_unique<oj::Arcade>();
    arcade->load(text, oj::ExeTables::stock());
    const size_t race = oj::Arcade::race(0, 0);
    check(arcade->i32(race + oj::Arcade::kRaceCarCount) == 6, "arcade (fixed): six cars kept");
    check(arcade->i32(race + oj::Arcade::kRaceBase) == -1, "arcade (fixed): the race's paint kept");
    check(arcade->i32(oj::Arcade::kTracks + 19 * 4) != 0 && arcade->i32(oj::Arcade::kTracks + 20 * 4) == 0,
          "arcade (fixed): 20 tracks kept, a 21st passed over");
}
