// Integration tests: the replay's controls (Game/Race/Hud/HudReplay.h) over the game's hud_replay.ui and texts.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/GameTexts.h"
#include "Core/Data/Language.h"
#include "Core/Input/Input.h"
#include "Core/Race/Replay.h"
#include "Game/Race/Hud/HudReplay.h"
#include "Game/UI/WindowManager.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The eject (BACK) of a replay not saved: "Quit Replay?" as the stock asks it, its NO the replay on; the fixed build's
// YES asks "Save replay?", whose YES asks for savereplay and quits, and whose NO quits.
OJ_INTEGRATION_TEST(HudReplay, EjectAsksQuitThenSave) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::test::NullRenderDevice device;
    const oj::Language language = *oj::Language::byName("english");
    oj::ui::WindowManager windows(device, language);
    oj::TextTable texts;
    if (!texts.load(std::filesystem::path("scripts") / "english.txt")) {
        check(false, "scripts\\english.txt read");
        return;
    }
    const auto eject = [&](int32_t quit, int32_t save) {
        std::string seen;
        oj::HudReplay hud(windows, texts);
        oj::Replay replay;
        replay.length = 10.0f;
        replay.time = 1.0f;
        const oj::HudRace race{};
        if (!hud.enter(race, replay)) return std::string("no window");
        oj::ReplayWindowFrame f = hud.frame(race, replay, 0.03f, oj::input::kBack, false);
        if (f.question != texts.at(oj::kTextQuitReplay) || f.answer == nullptr || !f.startNo) return seen + "no quit";
        seen += "quit?";
        *f.answer = quit;
        f = hud.frame(race, replay, 0.03f, oj::input::kNone, false);
        if (f.unpause) seen += " on";
        if (quit == 0) return seen;
        if (f.question != texts.at(oj::kTextSaveReplay) || f.answer == nullptr) return seen + " no save";
        seen += " save?";
        *f.answer = save;
        f = hud.frame(race, replay, 0.03f, oj::input::kNone, false);
        if (!f.window.empty()) seen += " " + f.window;
        f = hud.frame(race, replay, 0.03f, oj::input::kNone, false);
        if (!f.window.empty()) seen += " " + f.window;
        return seen;
    };
    const std::string no = eject(0, 0);
    const std::string saved = eject(1, 1);
    const std::string unsaved = eject(1, 0);
    std::printf("     eject: \"%s\", \"%s\", \"%s\"\n", no.c_str(), saved.c_str(), unsaved.c_str());
    check(no == "quit? on", "replay's eject: Quit Replay?, its NO the replay on");
    check(saved == "quit? save? savereplay closedown",
          "replay's eject (fixed): Quit, then Save replay? YES saves and quits");
    check(unsaved == "quit? save? closedown", "replay's eject (fixed): Save replay? NO quits");
}
