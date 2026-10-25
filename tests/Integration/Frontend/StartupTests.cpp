// Integration tests: the startup windows (disclaimer, legals, start), the game's windows and texts through a front end
// of the tests (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Data/GameData.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Startup/StartupHost.h"
#include "Game/Frontend/Startup/StartupWindows.h"

#include <bit>
#include <memory>
#include <string>

namespace {

using oj::test::check;

class FakeStartupHost : public oj::test::FakeWindowHost, public oj::frontend::StartupHost {
public:
    float timer = 0.0f;
    int released = 0;
    int forgotten = 0;
    bool watching = false;

    float windowTimer() const override { return timer; }
    void setWindowTimer(float seconds) override { timer = seconds; }
    void releaseInput() override { ++released; }
    void forgetPlayersUi() override { ++forgotten; }
    void watchControllers(bool on) override { watching = on; }
};

bool asked(const FakeStartupHost& host, const char* state) {
    return host.requests.size() == 1 && host.requests[0].first == state &&
           host.requests[0].second == oj::frontend::SwitchMode::Replace;
}

}  // namespace

// The disclaimer: 5 s, then legals.
OJ_INTEGRATION_TEST(Startup, DisclaimerWaits) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeStartupHost host;
    const auto s = host.open("disclaimer");
    if (!s) return;
    oj::frontend::Disclaimer window;
    window.enter(host, *s);
    const bool timed = host.timer == 5.0f;
    window.update(host);
    const bool waits = host.requests.empty();
    host.timer = -0.01f;
    window.update(host);
    check(timed && waits && asked(host, "legals"), "startup: the disclaimer 5 s, then legals");
}

// The legals: 9 s; a press goes on once 5 s have passed (the countdown under 4); the PC leaves out the consoles' and
// the licences' notes.
OJ_INTEGRATION_TEST(Startup, LegalsPressAfterFiveSeconds) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeStartupHost host;
    const auto s = host.open("legals");
    if (!s) return;
    oj::frontend::Legals window;
    window.enter(host, *s);
    const bool timed = host.timer == 9.0f;
    bool hidden = true;
    for (int e = 0; e < s->size(); ++e) {
        const std::string& name = s->nameOf(e);
        for (const char* part : {"DOLBY", "LOGITECH", "ESRB", "ACCLAIMLOGO", "JUICEDACCLAIM"}) {
            if (name.find(part) != std::string::npos && !s->hidden(name)) hidden = false;
        }
    }
    host.timer = 5.0f;
    window.update(host, true);
    const bool early = host.requests.empty();
    host.timer = 3.9f;
    window.update(host, true);
    check(timed && hidden && early && asked(host, "splash"), "startup: the legals 9 s, a press after 5 s, then splash");
}

// The title: PRESS shows line 24 ("PRESS ENTER"), the keyboard's player let go; the player's key goes to driver with
// CLICK and the controllers watched.
OJ_INTEGRATION_TEST(Startup, TitlePressGoesToDriver) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeStartupHost host;
    const auto s = host.open("start");
    if (!s) return;
    oj::frontend::Title window;
    window.enter(host, *s);
    const bool entered = s->textOf("PRESS") == host.texts.at(oj::kTextPressEnter) && !s->textOf("PRESS").empty() &&
                         host.released == 1 && host.forgotten == 1;
    window.update(host, false);
    const bool waits = host.requests.empty();
    window.update(host, true);
    check(entered && waits && asked(host, "driver") && host.watching && host.sounds.size() == 1 &&
              host.sounds[0] == "CLICK",
          "startup: the title's PRESS ENTER, then driver");
}

// The title past 100 s: the attract replays as a race of mode 4, one player, attract1, attract2, attract3, then
// attract1 again (Stock problem: the count went back at 2, attract3 never played; the fixed build plays it).
OJ_INTEGRATION_TEST(Startup, TitleAttractReplays) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeStartupHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    std::string played;
    bool raced = true;
    for (int i = 0; i < 4; ++i) {
        oj::frontend::Title window;
        host.requests.clear();
        host.dt = 60.0f;
        window.update(host, false);
        const bool waits = host.requests.empty();
        window.update(host, false);
        raced = raced && waits && asked(host, "loading") && data.race.mode == oj::RaceMode::Attract &&
                data.race.players == 1;
        played += data.replayName + " ";
    }
    check(raced && played == "attracts\\attract1 attracts\\attract2 attracts\\attract3 attracts\\attract1 " &&
              data.attractsStarted == 4,
          "startup: the title's attract replays 1, 2, 3, 1 (" + played + ")");
}

// Back to the title the save system's settings go back to a new driver's (0x00467060 with 0x004097F0): a save's music
// at 0 is forgotten and the music takes 0.9; the paint scheme's first values (the constructor's) stay.
OJ_INTEGRATION_TEST(Startup, TitleForgetsTheSavesSettings) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeStartupHost host;
    const auto s = host.open("start");
    if (!s) return;
    oj::save::Settings saved = oj::save::Settings::defaults(host.lang.index);
    saved.setU32(oj::save::Settings::kMusicVolume, 0);
    saved.setU32(oj::save::Settings::kEffectsVolume, 0);
    saved.setU32(oj::save::Settings::kScheme, 0x12345678);
    host.settings = saved;
    oj::frontend::Title window;
    window.enter(host, *s);
    check(host.musicVolume == 0.9f && host.settings &&
              host.settings->u32(oj::save::Settings::kEffectsVolume) == std::bit_cast<uint32_t>(0.75f),
          "startup: the title's music at a new driver's 0.9, the effects at 0.75");
    check(host.settings && host.settings->u32(oj::save::Settings::kScheme) == 0x12345678,
          "startup: the scheme's base (the constructor's) kept");
}

// Back to the title the save system's settings go back to a new driver's (0x00467060 with 0x004097F0): a save's music
// at 0 is forgotten and the music takes 0.9; the paint scheme's first values (the constructor's) stay.
OJ_INTEGRATION_TEST(Startup, TitleForgetsTheSavesSettings) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeStartupHost host;
    const auto s = host.open("start");
    if (!s) return;
    oj::save::Settings saved = oj::save::Settings::defaults(host.lang.index);
    saved.setU32(oj::save::Settings::kMusicVolume, 0);
    saved.setU32(oj::save::Settings::kEffectsVolume, 0);
    saved.setU32(oj::save::Settings::kScheme, 0x12345678);
    host.settings = saved;
    oj::frontend::Title window;
    window.enter(host, *s);
    check(host.musicVolume == 0.9f && host.settings &&
              host.settings->u32(oj::save::Settings::kEffectsVolume) == std::bit_cast<uint32_t>(0.75f),
          "startup: the title's music at a new driver's 0.9, the effects at 0.75");
    check(host.settings && host.settings->u32(oj::save::Settings::kScheme) == 0x12345678,
          "startup: the scheme's base (the constructor's) kept");
}
