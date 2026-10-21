// Integration tests: the race front end (Game/Race/FrontEnd/RaceFrontEnd.h) over the game's closedown.ui,
// restart_race.ui and msgbox.ui, with a race of its own (a fake host).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Game/Race/FrontEnd/RaceFrontEnd.h"

#include <filesystem>
#include <string>

namespace {

using oj::test::check;
using oj::RaceWindowId;

// What the front end's windows ask of the race, kept to be read.
class FakeRaceHost final : public oj::RaceFrontEndHost {
public:
    oj::Replay replayed;
    bool pausedNow = false;
    bool soundsHeld = false;
    bool soundsPaused = false;
    int syncs = 0;
    int restarts = 0;
    int uiTaken = 0;
    std::filesystem::path folder;

    oj::HudRace hudRace() override { return {}; }
    oj::RaceState* raceState() override { return nullptr; }
    oj::GameData* gameData() override { return nullptr; }
    const std::filesystem::path& saveFolder() const override { return folder; }
    int32_t players() const override { return 1; }
    int32_t pausingPlayer() const override { return -1; }
    const oj::net::NetSession* netSession() const override { return nullptr; }
    void useUi() override { ++uiTaken; }
    void followWindow(const oj::RaceWindowFrame&) override {}
    void followReplayWindow(const oj::ReplayWindowFrame&) override {}
    void restartRace() override { ++restarts; }
    void startReplay(bool) override {}
    oj::Replay& replay() override { return replayed; }
    void syncReplayFlags() override { ++syncs; }
    bool paused() const override { return pausedNow; }
    void setPaused(bool paused) override { pausedNow = paused; }
    void setSoundsHeld(bool held) override { soundsHeld = held; }
    void setSoundsPaused(bool paused) override { soundsPaused = paused; }
    void worldSoundsAtOnce() override {}
    void pauseWorldVoices() override {}
    void netCruiseBoard() override {}
};

struct FrontEnd {
    oj::test::NullRenderDevice device;
    oj::Language language = *oj::Language::byName("english");
    oj::TextTable texts;
    FakeRaceHost host;
    oj::RaceFrontEnd fe{host};

    FrontEnd() {
        fe.hudDevice = &device;
        fe.hudLanguage = &language;
        fe.hudTexts = &texts;
    }
};

}  // namespace

// closedown (0x0044F9C0, 0x0044FAC0): its enter stops the replay and pauses the race; three frames later, with no
// message up, the race unloads. Asked again while it is shown it does not enter again.
OJ_INTEGRATION_TEST(RaceFrontEnd, ClosedownUnloadsAfterThreeFrames) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FrontEnd f;
    f.host.replayed.playing = 1;
    f.fe.askWindow(RaceWindowId::Closedown);
    f.fe.windowsFrame(0.03f);
    const bool entered = f.fe.window.id == RaceWindowId::Closedown && f.fe.window.name == "closedown" &&
                         f.host.pausedNow && f.host.replayed.playing == 0 && f.host.syncs == 1 &&
                         f.fe.screenWindow != nullptr && f.fe.closedownFrames == 2;
    f.fe.askWindow("CLOSEDOWN");
    f.fe.windowsFrame(0.03f);
    f.fe.windowsFrame(0.03f);
    const bool notAgain = f.host.syncs == 1 && f.fe.closedownFrames == 0 && !f.fe.unloadAsked;
    f.fe.windowsFrame(0.03f);
    check(entered && notAgain && f.fe.unloadAsked, "closedown: entered once, the race unloads on its fourth frame");
}

// A message up takes the frame: the window under it waits (closedown's frames do not run down); its number counts.
OJ_INTEGRATION_TEST(RaceFrontEnd, MessageHoldsTheWindow) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FrontEnd f;
    f.fe.askWindow(RaceWindowId::Closedown);
    f.fe.windowsFrame(0.03f);
    f.fe.raceMessage(oj::ui::MessageType::Continue, L"note", nullptr, false);
    const bool up = f.fe.message != nullptr && f.fe.messageNumber == 1;
    f.fe.windowsFrame(0.03f);
    f.fe.windowsFrame(0.03f);
    f.fe.windowsFrame(0.03f);
    check(up && f.fe.closedownFrames == 2 && !f.fe.unloadAsked, "a message up: closedown waits under it");
    // The save device busy: no network message, its number 0.
    f.fe.saveBusy = 1;
    const int32_t number = f.fe.netNote(oj::ui::MessageType::Continue, L"net", nullptr);
    check(number == 0 && f.fe.netMessage == nullptr, "the save device busy: no network message");
}

// restart_race (0x00455940, 0x00455A90, 0x00455F50): its enter holds the sounds and stops the watch of the
// controllers; without a race its time runs out and nothing restarts; its leave gives the sounds back as the race's
// pause stands.
OJ_INTEGRATION_TEST(RaceFrontEnd, RestartRaceHoldsTheSounds) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FrontEnd f;
    f.fe.askWindow(RaceWindowId::RestartRace);
    f.fe.windowsFrame(0.03f);
    const bool held = f.host.soundsHeld && !f.fe.watchesControllers() && f.fe.restartTime == 2.0f;
    for (int i = 0; i < 5; ++i) f.fe.windowsFrame(0.03f);
    const bool waits = f.host.restarts == 0 && f.fe.restartTime < 0.0f;
    f.host.pausedNow = true;
    f.fe.askWindow(RaceWindowId::Blank);
    f.fe.windowsFrame(0.03f);
    check(held && waits && !f.host.soundsHeld && f.host.soundsPaused && f.fe.watchesControllers() &&
              f.fe.window.id == RaceWindowId::Blank,
          "restart_race: the sounds held while it is shown, given back as it leaves");
}

// A name the front end has no window for comes in as Other: shown, nothing made.
OJ_INTEGRATION_TEST(RaceFrontEnd, UnknownWindowIsShownAsOther) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FrontEnd f;
    f.fe.askWindow("no_such_window");
    f.fe.windowsFrame(0.03f);
    check(f.fe.window.id == RaceWindowId::Other && f.fe.window.name == "no_such_window" && f.fe.screenWindow == nullptr,
          "an unknown window: shown by its name, nothing made");
}
