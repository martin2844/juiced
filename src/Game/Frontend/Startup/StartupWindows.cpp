// The startup windows (docs/spec/02-frontend.md, "Startup"): disclaimer, legals, splash, juicesplash, skippable
// and start (the title).

#include "Game/Frontend/Startup/StartupWindows.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x00466D00: the window's transitions started again (0x005AA0A0; the record's +0x10 1 and +0x14 120, read
// by nothing), the time 5 s, the boxes over several lines.
void Disclaimer::enter(StartupHost& host, ui::Screen& s) {
    s.restartTransitions();
    for (const char* text : {"BOX01", "BOX02", "BOX03"}) {
        s.setMultiline(text, true);
    }
    host.setWindowTimer(5.0f);
}

// 0x00466DF0: the transitions started again (0x005AA0A0) before the console and licence notes are hidden.
void Legals::enter(StartupHost& host, ui::Screen& s) {
    s.restartTransitions();
    for (const char* text : {"OWNERS", "NOKIA", "BINK", "DEVELOPED", "JUICEDTHQ", "ONLINE", "DOLBY", "LOGITECH"}) {
        s.setMultiline(text, true);
    }
    // The PC legals leave out the console and licence notes.
    for (const char* part : {"DOLBY", "LOGITECH", "ESRB", "ACCLAIMLOGO", "JUICEDACCLAIM"}) {
        s.hideContaining(part);
    }
    host.setWindowTimer(9.0f);
}

// 0x00466840: SPLASH's volume 0.3; the window's transitions started again (0x005AA0A0); +0x0C and +0x10 0;
// FADEOUT3's transition waits 12 s (0x0046C970) and starts again.
void Splash::enter(ui::Screen& s) {
    s.setMovieVolume(s.find("SPLASH"), 0.3f);
    s.restartTransitions();
    m_time = 0.0f;
    m_pressed = false;
    s.setTransitionDelay("FADEOUT3", 12.0f);
    s.restartTransition("FADEOUT3");
}

void JuiceSplash::enter(ui::Screen& s) {
    s.setMovieVolume(s.find("SPLASH"), 0.7f);  // 0x00466C00
}

// 0x00466A40: SPLASH plays JuicedCGI_PC once (0x005AA300) at the volume 1, on a wide screen ([0x0075F0FC]) at
// (0, 0.05) and 1 x 0.9 of the screen; +0x08 1.
void Skippable::enter(StartupHost& host, ui::Screen& s) {
    const int splash = s.find("SPLASH");
    if (splash >= 0) {
        s.playMovie(splash, "JuicedCGI_PC", false);
        s.setMovieVolume(splash, 1.0f);
        if (host.engine() != nullptr && host.engine()->wideScreen()) s.setPlace(splash, 0.0f, 0.05f, 1.0f, 0.9f);
    }
    m_skipHeld = true;
}

// 0x004671C2: the keyboard's first player let go (0x0058FEA0).
void Title::enter(StartupHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    host.releaseInput();
    // 0x00467060: the save system's settings go back to a new driver's (0x004097F0: the volumes read below are the new
    // driver's, not the ones of the save loaded before) and the music is shuffled again.
    std::optional<save::Settings>& settings = host.driverSettings();
    if (settings) settings->reset(host.language().index);
    if (data) {
        data->shuffleMusic(settings ? *settings : save::Settings::defaults(host.language().index), gameRandom());
    }
    // The game's sounds resumed as 0x0040D8E0 with 0 does it; the renderer's +0x38 with 2 (empty on the PC); the music's
    // and the effects' volumes of the settings (the speech's not); the front end's +0x18 .. +0x24 0 (the players'
    // UI maps to be registered, their notes counted as up).
    host.forgetPlayersUi();
    pauseGameSounds(false, &host.uiSounds());
    host.setMusicVolume(std::bit_cast<float>(host.settingsValue(save::Settings::kMusicVolume)), false);
    host.uiSounds().setVolume(std::bit_cast<float>(host.settingsValue(save::Settings::kEffectsVolume)));
    // PRESS takes line 24 of the game's text table ([0x0075FA6C], "PRESS ENTER"; 0x004671E0), its FontModifier
    // record started again; [0x0075EC98] 0 (the only write of it), the window's +0x14 1 and +0x18 5.
    s.setText("PRESS", texts.at(kTextPressEnter));
    // Its +0x0C and +0x10 cleared; no race mode ([G+0x6C]+0x00 0).
    m_attractTime = 0.0f;
    if (data) data->race.mode = RaceMode::Arcade;
}

void Disclaimer::update(StartupHost& host) {
    if (host.windowTimer() < 0.0f) host.request("legals", SwitchMode::Replace);
}

void Legals::update(StartupHost& host, bool pressed) {
    if ((pressed && host.windowTimer() < 4.0f) || host.windowTimer() < 0.0f)
        host.request("splash", SwitchMode::Replace);
}

// 0x00466C30: a press, or the end of its movie.
void JuiceSplash::update(StartupHost& host, ui::Screen& s, bool pressed) {
    if (pressed && !host.switchAsked()) host.request("skippable", SwitchMode::Replace);
    const movie::Player* p = splashMovie(s);
    if (p != nullptr && p->finished() && !host.switchAsked()) host.request("skippable", SwitchMode::Replace);
}

// 0x00466AD0: the press that opened it let go first; then a press, the end of the movie or its frame 2901.
void Skippable::update(StartupHost& host, ui::Screen& s, bool pressed) {
    if (m_skipHeld && !pressed) m_skipHeld = false;
    if (pressed && !m_skipHeld && !host.switchAsked()) host.request("start", SwitchMode::Replace);
    const movie::Player* p = splashMovie(s);
    if (p != nullptr && (p->finished() || p->frameNum() > 0xB54) && !host.switchAsked())
        host.request("start", SwitchMode::Replace);
}

// 0x00467260 (the time cleared while the device is lost, which it is not here): past 100 s on the title the
// attract replay next (attracts\attract1, then 2; Stock problem: the count goes back to the first at 2, so
// attract3 is never played) is the race (mode 4, one player) and loading is asked; a name not taken
// (0x004296F0 always takes it) would start the time again.
void Title::update(StartupHost& host, bool playerTaken) {
    GameData* data = host.gameData();
    m_attractTime = host.frameTime() + m_attractTime;
    if (m_attractTime > 100.0f && data != nullptr) {
        if (data->attractNext >= 2) data->attractNext = 0;
        data->race.mode = RaceMode::Attract;
        data->race.players = 1;
        static const char* const kAttracts[] = {"attracts\\attract1", "attracts\\attract2", "attracts\\attract3"};
        data->replayName = kAttracts[data->attractNext];
        ++data->attractNext;
        host.request("loading", SwitchMode::Replace);
        // The input manager's +0x38 with 1 (0x0051F960, empty) and [0x0075ED4C] counted (0x00467347).
        ++data->attractsStarted;
        return;
    }
    // 0x00467365: a first player's device taken (the manager's +0x28, cleared by the window's enter through
    // 0x0058FEA0 and set by its update once the raw keyboard's RETURN is held): any other key does nothing.
    if (playerTaken) {
        host.sound("CLICK");
        // 0x00467350: the front end's +0x38 set, its players' controllers watched.
        host.watchControllers(true);
        host.request("driver", SwitchMode::Replace);
    }
}

// The interface's movie when it is SPLASH's (its name that of the movie playing, case aside); nullptr otherwise.
const movie::Player* splashMovie(ui::Screen& s) {
    const movie::System* m = movie::System::get();
    const int splash = s.find("SPLASH");
    if (m == nullptr || m->current == nullptr || splash < 0) return nullptr;
    return _stricmp(s.movieName(splash).c_str(), m->currentName.c_str()) == 0 ? m->current : nullptr;
}

// 0x004668C0: a press starts +0x0C counting the frame time; the window's transitions over (FADEOUT3's after 12 s),
// the THQ movie's end, its frame 182, or 0.4 s after a press: juicesplash.
void Splash::update(StartupHost& host, ui::Screen& s, bool pressed) {
    if (pressed) m_pressed = true;
    if (s.transitionsDone() && !host.switchAsked()) host.request("juicesplash", SwitchMode::Replace);
    if (m_pressed) m_time = host.frameTime() + m_time;
    const movie::Player* p = splashMovie(s);
    const uint32_t frame = p != nullptr ? p->frameNum() : 0;
    if ((p != nullptr && p->finished()) || frame > 0xB5 || m_time > 0.4f) {
        if (!host.switchAsked()) host.request("juicesplash", SwitchMode::Replace);
    }
}

}  // namespace oj::frontend
