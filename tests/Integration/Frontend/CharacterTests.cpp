// Integration tests: the betting window "character" on a career of the game's data, the game's window and texts
// through a front end of the tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Game/Frontend/Career/CharacterScreen.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The character test mode (cheat 5) stays at the talk's step 0, whose clip is the one its count names (0x0042F140:
// 0x00715AE0 by +0x0C, SELECT up, BACK down through 81), the clip's name shown in BOSSCAR. Stock problem: the count's
// last six name no clip (the game stops); the leader's folder alone here.
OJ_INTEGRATION_TEST(Character, TestModeClips) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "game data and a first career loaded: " + error);
        return;
    }
    oj::Career& career = *data.career;
    career.cheatFlags[oj::kCheatCharacterTest] = 1;
    career.bets.crew = 1;
    career.bets.kind = oj::BetKind::Money;
    career.bets.firstStage = 3;
    oj::test::FakeCareerHost host;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("character");
    if (screen == nullptr) return;
    oj::frontend::CharacterScreen character;
    character.enter(host, *screen);
    const int movie = screen->find("MOVIE");
    const bool hiddenFirst = screen->hidden("BOSSCAR");
    auto clipAfter = [&](uint32_t event) {
        character.update(host, *screen, event);
        character.update(host, *screen, 0);
        return screen->movieName(movie);
    };
    character.update(host, *screen, 0);
    const std::string first = screen->movieName(movie);
    check(first == "TK\\TK_GREETING1" && hiddenFirst && !screen->hidden("BOSSCAR") &&
              screen->textOf("BOSSCAR") == L"TK\\TK_GREETING1",
          "the step 0's clip the count's first, its name in BOSSCAR shown (" + first + ")");
    const std::string next = clipAfter(oj::input::kSelect);
    check(next == "TK\\TK_GREETING_WON_LAST1" && !host.sounds.empty() && host.sounds.back() == "CLICK",
          "SELECT: the next clip, the one playing cut with CLICK (" + next + ")");
    clipAfter(oj::input::kBack);
    const std::string last = clipAfter(oj::input::kBack);
    check(last == "TK\\TK" && screen->textOf("BOSSCAR") == L"TK\\TK",
          "BACK from the first: the count's last, no clip (" + last + ")");
}
