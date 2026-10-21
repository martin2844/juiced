#pragma once
// The race front end (docs/spec/22-race-hud.md; the game object's +0x3C, made by 0x004563E0): its window manager, the
// windows it shows over the race (the HUD, the race's end and results, the pause, the options, the replay's, closedown
// and the restart's), its messages and notes, the save windows, its sounds and its black. Each frame the window asked
// for comes in and the window shown runs; what the windows ask of the race goes through RaceFrontEndHost. The race
// watches the controllers and the network around its frame (TrackView).

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Data/GameTexts.h"
#include "Core/Data/Language.h"
#include "Core/Input/ForceFeedback.h"
#include "Core/Input/Input.h"
#include "Core/Race/RaceKinds.h"
#include "Core/Race/Replay.h"
#include "Game/Frontend/Frontend.h"
#include "Game/Race/Hud/RaceSaves.h"
#include "Game/Race/Hud/RaceWindowId.h"
#include "Game/Race/Hud/RaceWindows.h"
#include "Game/Scene/Scene3d.h"
#include "Game/UI/UiSound.h"
#include "Game/UI/WindowManager.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace oj {

class RaceState;
struct RaceRules;
namespace net {
class NetSession;
}

// What the race front end's windows ask of the race.
class RaceFrontEndHost {
public:
    virtual ~RaceFrontEndHost() = default;
    // What the HUD reads of the race; the race's state (none before it is made).
    virtual HudRace hudRace() = 0;
    virtual RaceState* raceState() = 0;
    // The records (none in a view without them) and the folder of the saves (the options', the save windows').
    virtual GameData* gameData() = 0;
    virtual const std::filesystem::path& saveFolder() const = 0;
    // The race's players (the options' second player), the player who paused the race (the race track's +0x10; -1
    // none) and the network game's session (none off it).
    virtual int32_t players() const = 0;
    virtual int32_t pausingPlayer() const = 0;
    virtual const net::NetSession* netSession() const = 0;
    // The UI map registered (0x00407FB0).
    virtual void useUi() = 0;
    // What a window's frame asks of the race; the replay window's.
    virtual void followWindow(const RaceWindowFrame& f) = 0;
    virtual void followReplayWindow(const ReplayWindowFrame& f) = 0;
    // restart_race at its time; the play of start_replay (true) or restart_replay.
    virtual void restartRace() = 0;
    virtual void startReplay(bool fromStart) = 0;
    // The game object's replay (+0x9C) and the race's flags of it followed.
    virtual Replay& replay() = 0;
    virtual void syncReplayFlags() = 0;
    // The race state's +0x20 (0x004263E0); the sounds held by restart_race's enter, the pause as the sounds last
    // followed it.
    virtual bool paused() const = 0;
    virtual void setPaused(bool paused) = 0;
    virtual void setSoundsHeld(bool held) = 0;
    virtual void setSoundsPaused(bool paused) = 0;
    // start_replay's enter: the 3D world at once at 1 x the effects' volume (down 20, up 50, its player's master).
    virtual void worldSoundsAtOnce() = 0;
    // hud_sprint_results' leave: the 3D world's voices paused (its manager's +0x20 with 1).
    virtual void pauseWorldVoices() = 0;
    // 0x004B0A50 from hud_cruise_results' enter.
    virtual void netCruiseBoard() = 0;
};

class RaceFrontEnd final : RaceSaveHost {
public:
    explicit RaceFrontEnd(RaceFrontEndHost& host) : m_host(host) {}
    RaceFrontEnd(const RaceFrontEnd&) = delete;
    RaceFrontEnd& operator=(const RaceFrontEnd&) = delete;

    // 0x004563E0: the race front end's sounds and window manager made, the windows it keeps, the HUD's window of the
    // race's type (the table 0x00456A24): hud_split with more than one player (or none), hud_solo for 0 .. 3 and 9,
    // hud_cruise for 4, hud_race for 5, 7 and 8, hud_sprint for 6; the map's models for all of one player but
    // hud_cruise and type 8 (without a map made here).
    void load(const RaceRules& rules, scene::Engine* engine, ff::Wheels* wheels, float effectsVolume,
              CrtRandom& loading);
    // The race's end: its windows and messages closed, in the order the race's end closes them.
    void closeWindows();

    // The window manager's frame (0x005A6AB0 ..): the front end's sounds, the note closing, the window asked for
    // comes in; the note of a lost controller, the network's message, the message, the save windows or the window
    // shown take the frame's events. The frame's event reaches a window from the frame after it came in.
    void windowsFrame(float dt);
    // 0x00458480: the players' messages (+0x0C, +0x10) run down whatever the window (0x00456300).
    void playerMessages(float dt, size_t players);
    // 0x005A9D20 ..: the window shown (with a message up only its FRAME_ elements, unless the note of a lost
    // controller or the network's message is up), the message, the front end's black clearing by the frame time,
    // the network's message, the note; then (0x0045846D, presence) the note of an invitation (0x004E1860).
    void draw(render::IRenderDevice& device, float seconds, float dt, bool presence);

    // The window manager's request (+0xBC = 2, the name at +0xC0), kept while one waits.
    void askWindow(const std::string& name) {
        if (windowAsked.empty()) windowAsked = name;
    }
    void askWindow(RaceWindowId id) { askWindow(std::string(raceWindowName(id))); }
    // The HUD shown: the window shown is the HUD's.
    bool hudShown() const { return hud != nullptr && window.id == hudWindow; }
    // The race front end's +0x38: cleared by restart_race's, start_replay's and restart_replay's enter, set by their
    // leave (0x00455F50).
    bool watchesControllers() const {
        switch (window.id) {
        case RaceWindowId::RestartRace:
        case RaceWindowId::StartReplay:
        case RaceWindowId::RestartReplay: return false;
        default: return true;
        }
    }

    // The race front end's window manager (0x004563E0): the windows it keeps (made as the race loads) and makes; the
    // race's windows open theirs through it. The stock's also takes the font concielian12 (+0x50, 0x004569A3: found
    // among the fonts or made), which nothing reads; not taken here.
    ui::WindowManager& windows() {
        if (!raceWindows) raceWindows = std::make_unique<ui::WindowManager>(*hudDevice, *hudLanguage);
        return *raceWindows;
    }
    // The options from the pause (opts_ingame_bg and the windows over it): the race front end's windows of the front
    // end's states (0x0047FF60 ..) with the game object's +0x3C set, kept for the race.
    Frontend& raceOptions();

    // A message of the race front end (0x005A80E0): its number counted.
    void raceMessage(ui::MessageType type, const std::wstring& text, int32_t* answer, bool startNo) override;
    // 0x005A7F90: unless the save device is busy (+0x110), msgbox_network (the window manager's +0x3C) opens with the
    // type, the text and the answer; the number of the last message (+0x108) returned (Stock problem: 0 in a race
    // with no message yet, as when it does not open), else 0.
    int32_t netNote(ui::MessageType type, const std::wstring& text, int32_t* answer);
    // 0x005A8030: with no note up, a note of a lower priority up or the one up closing, msgbox_critical (type 0) opens
    // with the text and its priority kept.
    bool openCritical(uint32_t priority, const std::wstring& text);
    // 0x005A7F70: the note up closes when its priority is not above the one given.
    void closeCritical(uint32_t priority) {
        if (critical != nullptr && priority <= criticalPriority) criticalClose = true;
    }
    // newhighscore_cms over the window (0x00466420: with fewer than two players the race type's text made and dropped;
    // 0x004664C0): its first frame, when the save system saves (its method +0x24) while its +0x3C is 0 (nothing sets
    // it) or the records were loaded, asks for savehighscores over it (CLICK); it closes itself once savehighscores
    // is gone.
    void newHighScore();

    // A line of the game's texts; none without them (a view without the HUD).
    const std::wstring& hudLine(size_t index) const {
        static const std::wstring none;
        return hudTexts != nullptr ? hudTexts->at(index) : none;
    }
    // A line of scripts\jettext<language>.txt.
    const std::wstring& jetText(size_t line) override;

    // The device, the language and the game's texts of the windows (none in a view without the HUD).
    render::IRenderDevice* hudDevice = nullptr;
    const Language* hudLanguage = nullptr;
    TextTable* hudTexts = nullptr;
    UiSound raceUi;          // the race front end's manager's sounds (0x004563E0: audio/ui/race)
    float raceFade = 0.0f;   // the race front end's black over the screen ([[G+0x3C]+8]+0x10C)
    // The race front end's +0x1C, +0x20: the FF manager's channels 0 and 1 had a device as it was made (0x004568C5;
    // read by nothing).
    std::array<bool, 2> wheelsOpened{};
    std::unique_ptr<ui::WindowManager> raceWindows;
    std::unique_ptr<Frontend> options;
    input::UiFrame uiFrame;   // the UI map's frame, for the options' sliders
    input::UiFrame uiFrame2;  // the second player's
    uint32_t uiEvent = 0;     // the UI map's event of the frame, for the race front end's window
    uint32_t uiEvent2 = 0;    // the second player's (0x00457BEF: with more than one player)
    std::unique_ptr<RaceHud> hud;
    std::unique_ptr<RaceComplete> complete;
    std::unique_ptr<RaceResults> results;
    std::unique_ptr<ChallengeResults> challengeResults;
    std::unique_ptr<SprintResults> sprintResults;
    std::unique_ptr<CruiseResults> cruiseResults;
    std::unique_ptr<RacePause> pauseWindow;
    std::unique_ptr<RaceMessage> message;     // the window manager's +0x44 (a message window open: +0x108)
    std::unique_ptr<RaceMessage> netMessage;  // the window manager's +0x3C: msgbox_network (0x005A7F90)
    // The window manager's +0x34: msgbox_critical (0x005A8030), the note of a lost controller, its priority
    // ([0x00722FEC]) and its closing asked (+0x38, 0x005A7F70).
    std::unique_ptr<RaceMessage> critical;
    uint32_t criticalPriority = 0;
    bool criticalClose = false;
    bool noControllerNote = true;   // the race front end's +0x14: no note of the first player's controller up
    bool noControllerNote2 = true;  // +0x18: the second player's
    int32_t messageNumber = 0;      // the window manager's +0x108: the messages made, 1 .. 9998
    // The window manager's +0x110: busy with the save device (savehighscores, savereplay): no network message.
    int32_t saveBusy = 0;
    SaveHighScoresWindow saveHighScores;
    SaveReplayWindow saveReplay;
    TextTable jetTexts;  // scripts\jettext<language>.txt
    std::unique_ptr<RaceScreenWindow> screenWindow;  // closedown's or restart_race's window
    float restartTime = 0.0f;  // restart_race's +0x0C
    std::unique_ptr<HudReplay> hudReplay;
    int replayFrames = 0;       // start_replay's / restart_replay's +0x0C
    int closedownFrames = 0;    // closedown's +0x0C: frames left before the race unloads
    bool unloadAsked = false;   // closedown made the game phase "load" with 1 (0x00462C30)
    bool windowOpened = false;  // a window came in this frame (it takes the frame's event from the next one)
    // The window manager's +0x50: cleared as its frame starts (0x005A6B41), set by the HUD's frame (RaceHud::
    // hidesWindows); while set the windows are not drawn that frame (0x005A6BB5), the messages and notes are.
    bool windowsHidden = false;
    RaceWindowShown window;                       // the window shown (None before any)
    RaceWindowId hudWindow = RaceWindowId::None;  // the HUD's window (0x004563E0)
    std::string windowAsked;                      // the window manager's +0xC0 while +0xBC is 2

private:
    // 0x005A8580 with +0xBC 2: the window asked for, unless it is the one shown, takes the place of the windows open
    // (each leaves: the HUD state 0x0044A450) and enters.
    void openAskedWindow();
    // The leave of the window shown; the enter of the window come in (false: none made for it).
    void leaveWindow();
    bool enterWindow();
    // The frame of the window shown, with the frame's events (none in the frame it came in).
    void windowFrame(float dt, uint32_t event, uint32_t event2);
    // The window shown drawn.
    void drawWindow(float seconds, float dt);

    // The save windows' asks (RaceSaveHost).
    bool messageUp() const override { return message != nullptr; }
    int32_t lastMessageNumber() const override { return messageNumber; }
    void closeMessage() override { message.reset(); }
    void setSaveBusy(int32_t busy) override { saveBusy = busy; }
    const TextTable* gameTexts() const override { return hudTexts; }

    RaceFrontEndHost& m_host;
};

}  // namespace oj
