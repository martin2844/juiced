#pragma once
// The front end of the stock game (docs/spec/02-frontend.md): each state opens its interface window, counts its
// time, reads the window events and names the next state. Windows can lie over the current one (switch mode 5) and
// the message window over all of them.

#include "Core/Data/GameData.h"
#include "Core/Data/Language.h"
#include "Core/Data/NameFilter.h"
#include "Core/Input/Input.h"
#include "Core/Network/NetTransport.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Physics/PhysicsWorld.h"
#include "Core/Race/Racer.h"
#include "Core/Saves/Profile.h"
#include "Core/Saves/SaveEntries.h"
#include "Game/Cars/CarSound.h"
#include "Game/Cars/CarView.h"
#include "Game/Cars/CarWheels.h"
#include "Game/Frontend/Arcade/ArcadeWindows.h"
#include "Game/Frontend/Career/AtMeeting.h"
#include "Game/Frontend/Career/Calendar.h"
#include "Game/Frontend/Career/Advice.h"
#include "Game/Frontend/Career/CalendarHelp.h"
#include "Game/Frontend/Career/CarCollection.h"
#include "Game/Frontend/Career/CareerMenu.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/Frontend/Career/CareerLoad.h"
#include "Game/Frontend/Career/CareerChallengeResults.h"
#include "Game/Frontend/Career/CharacterScreen.h"
#include "Game/Frontend/Career/CityMap.h"
#include "Game/Frontend/Career/CrewWindow.h"
#include "Game/Frontend/Career/CrewRaceSetup.h"
#include "Game/Frontend/Career/CutScene.h"
#include "Game/Frontend/Career/GridWalk.h"
#include "Game/Frontend/Career/HostMeeting.h"
#include "Game/Frontend/Career/MeetingSummary.h"
#include "Game/Frontend/Career/NewCont.h"
#include "Game/Frontend/Career/OldCareerMenu.h"
#include "Game/Frontend/Career/PhoneWindows.h"
#include "Game/Frontend/Career/DemoFlyby.h"
#include "Game/Frontend/Career/CareerRaceResults.h"
#include "Game/Frontend/Career/RaceSetup.h"
#include "Game/Frontend/Career/Respect.h"
#include "Game/Frontend/Career/RespectChanges.h"
#include "Game/Frontend/Career/RespectMessage.h"
#include "Game/Frontend/Career/RespectOverview.h"
#include "Game/Frontend/Career/Stats.h"
#include "Game/Frontend/Career/StatsMenu.h"
#include "Game/Frontend/CustomRace/CustomRace.h"
#include "Game/Frontend/Garage/CarBuy.h"
#include "Game/Frontend/Garage/CarSel.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/Garage/Dealership.h"
#include "Game/Frontend/Garage/Customise/Accessories.h"
#include "Game/Frontend/Garage/Customise/CustMenu.h"
#include "Game/Frontend/Garage/Customise/Paint.h"
#include "Game/Frontend/Garage/Customise/PartMenus.h"
#include "Game/Frontend/Garage/Customise/SchemeMenu.h"
#include "Game/Frontend/Garage/Customise/Styling.h"
#include "Game/Frontend/Garage/Customise/Tuning.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/Frontend/Garage/Graph.h"
#include "Game/Frontend/Garage/ModShop.h"
#include "Game/Frontend/Garage/Repair.h"
#include "Game/Frontend/Garage/RollingRoad.h"
#include "Game/Frontend/Menus/Cheats.h"
#include "Game/Frontend/Menus/DemoMenu.h"
#include "Game/Frontend/Menus/Extras.h"
#include "Game/Frontend/Menus/HiScores.h"
#include "Game/Frontend/Menus/MainMenu.h"
#include "Game/Frontend/Menus/VidTutorial.h"
#include "Game/Frontend/Messages.h"
#include "Game/Frontend/Network/LinkWindows.h"
#include "Game/Frontend/Network/OnlineCareerWindows.h"
#include "Game/Frontend/Network/OnlineCrewRaceSetup.h"
#include "Game/Frontend/Network/OnlineForms.h"
#include "Game/Frontend/Network/OnlineFriendWindows.h"
#include "Game/Frontend/Network/OnlineRaceSetup.h"
#include "Game/Frontend/Network/OnlineSessionWindows.h"
#include "Game/Frontend/Network/OnlineStatsWindow.h"
#include "Game/Frontend/Network/OnlineWindows.h"
#include "Game/Frontend/Network/VoiceOptions.h"
#include "Game/Frontend/Options/AudioOptions.h"
#include "Game/Frontend/Options/GameOptions.h"
#include "Game/Frontend/Options/GraphicsOptions.h"
#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/Frontend/Options/OptionsMenu.h"
#include "Game/Frontend/Options/SongList.h"
#include "Game/Frontend/Options/WheelOptions.h"
#include "Game/Frontend/Popups.h"
#include "Game/Frontend/Profile/ChooseWindows.h"
#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/Frontend/Saves/Autosave.h"
#include "Game/Frontend/Saves/Driver.h"
#include "Game/Frontend/Saves/LoadHighScores.h"
#include "Game/Frontend/Saves/LoadReplay.h"
#include "Game/Frontend/Saves/SaveHost.h"
#include "Game/Frontend/Saves/SaveProfile.h"
#include "Game/Frontend/Startup/BootUpCheck.h"
#include "Game/Frontend/Loading/Loader.h"
#include "Game/Frontend/Startup/StartupWindows.h"
#include "Game/Frontend/Unused/CutWindows.h"
#include "Game/Frontend/WindowHost.h"
#include "Game/Scene/Scene3d.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/UiSound.h"
#include "Game/UI/WindowManager.h"
#include "Render/RenderDevice.h"

#include <bit>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace oj::movie {
class Player;
}

namespace oj {

namespace scene {
class Engine;
}

struct FrontendInput {
    input::UiFrame ui;
    char typed = 0;          // the character typed this frame (input::Input::typed)
    bool shiftHeld = false;  // a shift key is down
    bool playerTaken = false;  // the first player's device taken (the input manager's +0x28 not -1)
    // The second player's frame (0x00408260 on the controller of the input manager's +0x2C), none without its device.
    input::UiFrame ui2;
    bool second = false;
    size_t devices = 1;  // the input manager's devices ([G+0x18]'s list, each counted by its +0x20: 1)
};

class Frontend : private frontend::CareerHost,
                 private save::SaveGameSide,
                 private frontend::OptionsHost,
                 private frontend::MainMenuHost,
                 private frontend::SaveHost,
                 private frontend::GarageHost,
                 private frontend::CustomRaceHost,
                 private frontend::OnlineHost,
                 private frontend::StartupHost {
public:
    // data: the game's data (none: no driver can be made or loaded into a career); saveFolder holds SAVE.
    // scene: the 3D engine for the 3d elements of the windows (none: they stay empty).
    Frontend(render::IRenderDevice& device, const Language& language, TextTable& gameTexts, GameData* data,
             std::filesystem::path saveFolder, scene::Engine* scene = nullptr);
    // 0x0046BF70: the front end's phase ends with the interface's movie given back (0x005AA340).
    ~Frontend() override;

    // OpenJuice.ini's [Game] SkipIntroMovies: splash, juicesplash and skippable go on at once, their movies unplayed.
    static inline bool skipIntroMovies = false;

    using OnlineCommandLine = frontend::OnlineCommandLine;
    using OnlineForms = frontend::OnlineForms;
    // The stock's globals the windows share (they outlive the front end).
    static frontend::SharedState& sharedState() { return s_shared; }

    // The first state ("disclaimer" for the stock start).
    void start(const std::string& state);
    // 0x0046B5A0 for the game's front end outside a replay's race ([G+0x6C] mode 4): the window startup (registered
    // by no record) with its ACCLAIMLOGO elements hidden (strstr; it has none) and JUICELOGO at x 0.2; the stock
    // code then draws three frames of it (the window manager's 0x005A6AB0 and the renderer's 0x00514A40) before the
    // windows are made.
    void showStartup();
    // The windows the front end keeps (0x0046AF80 on the loading thread, the startup window drawn meanwhile; the
    // manager's 0x005A8430 into its +0x10 list), made in its order: instant's (0x0045FBC0); menu .. extras; the
    // options' (0x00484CF0); the garage's (0x00446CE0, with 0x004C5470's); character, characterPostRace; the respect
    // windows (0x004DC1E0); CMSRaceResults .. DemoFlyby; the save windows (0x00466650); the link windows
    // (0x00471640); the arcade's (0x0042C530); CMSMainMenu .. CMSCarDealership; 0x004C3780's; CMSCheats, CMSAdvice;
    // the stats windows (0x004E1610); CMSOnlineRaceSetup .. CMSOnlineGridWalk.
    void keepGameWindows();
    // The loading front end's (0x004634B0): loader.
    void keepLoaderWindows();
    // The race front end's (0x004563E0) that the options' front end opens: the in-game options' (0x00484E20), the
    // online windows' (0x0047FB30) and the save windows' (0x004666E0). Its network hooks are on only while its
    // windows are open.
    void keepRaceWindows();
    // The race front end's switch away from the options (0x005A6F90): each window's state left, from the bottom.
    void closeWindows();
    // The windows' records are the game's (static here): the views and objects they may still hold freed as the game
    // ends, before the engine.
    static void releaseRecords();
    // 0x0046BAC0 (the main function, with the sound on; and 0x0046BA90 after a race): unless the music is off one
    // rand() value drawn for nothing and the bank "music1" (always 1) added to the interface's player.
    void loadMusic();
    // 0x0046BDD0: unless the music is off, "music1" played looping at the settings' music volume (+0x10, +0x14)
    // when its handle (+0x0C) is none, stopped, ended or fading, and held.
    void startMusic() override;
    // The options run from a race's pause: their sounds and the effects' volume are the race front end's manager's
    // ([G+0x3C], 0x004563E0: audio/ui/race), the one 0x005A8860 and 0x005A89F0 find while the race runs.
    void useSounds(UiSound& sounds) { m_sounds = &sounds; }
    // The main function with -i (0x004184D2), before the first window: the input manager's +0x28 0 (Main), a
    // new driver "Player" (0x0040AB80) with its music volume (the settings' +0x00) 0, the race's gearbox 1 (0 with -m),
    // one player, no opponents, +0x64 1, not a replay's (0 already here), dry (wet, 2, with -wet) and heat 0; the race
    // keeps the rest of the game object's start (0x0040D0C0). The first window is then loading.
    void instantRace(bool automatic, bool wet);
    // The loading phase's own front end (0x00462D90): the loading window (its record's +0x0C set: the tips shown) and
    // over it the animation window.
    void startLoader(const std::string& window, const std::string& animation);
    // The state "loading" came: the game starts loading the race (0x0046AE50).
    bool raceLoadAsked() const { return m_raceLoadAsked; }
    // The main menu's "Quit to Windows" answered yes: the application's method +0x40 (0x00513C50) clears its +0x14
    // bit 0 and the game's loop (0x0040CCA0) ends.
    bool quitAsked() const { return m_quitGame; }
    // What the frame asked of the keyboard: 0x0058FEA0 (its first player let go, the device raw) or 0x00407FB0
    // (the UI map made again).
    bool takeInputRelease() { return std::exchange(m_inputRelease, false); }
    bool takeInputUi() { return std::exchange(m_inputUi, false); }
    // The options' controls set up (0x00480612): DRIVER1 registered with the mapping window, the controllers made
    // again (0x00590EE0).
    bool takeInputConfigure() { return std::exchange(m_inputConfigure, false); }
    // The second player's device let go (the input manager's +0x2C -1: 0x0046CAA0, 0x0045E490).
    bool takeSecondPlayerRelease() { return std::exchange(m_secondPlayerRelease, false); }
    // The second player's device asked for (the input manager's vt+0x3C with 1: split_selcar, 0x0046DA60).
    bool takeSecondPlayerTake() { return std::exchange(m_secondPlayerTake, false); }
    // The front end's frame (0x0046BFD0) watches the players' controllers while its +0x38 is set: from start going
    // on (0x00467350) until the way into the career (0x0043542B); CMSLoader's enter clears it (0x004CA910), its leave
    // sets it again (0x004CA950).
    bool watchingControllers() const { return m_watchControllers; }
    // The window manager's switch wait (+0x100): the players' events are made only while it is 0.
    float switchWait() const { return m_switchTime; }
    // 0x0046BFD0's watch of a player's controller (0 or 1) by the event's +0x18. Connected while
    // a note of it is up (+0x1C / +0x24 0): true (the UI map registered again, 0x00407FB0), the note closed
    // (0x005A7F70 with 1), the game's sounds resumed (0x0040D8E0 with 0), the interface's volume the settings'
    // effects' (+0x58, 0x005A89F0), no note up. Lost while none is (also while the game's window is inactive: the
    // device is not read then): the note (msgbox_critical, priority 1, 0x005A8030) with no text, the screen fading to
    // black (the text "To continue, please reconnect the DUALSHOCK 2 analog controller into controller port " (1632)
    // and "<device + 1>." is made and emptied before it opens); opened, a note up; on the network before the race
    // (the stage under 9) our player not ready (0x004EA370 with 0, 0); opened, the game's sounds paused (0x0040D8E0
    // with 1) and the interface's volume 0.
    bool watchController(int player, bool connected);
    // 0x0046C393: the first player's device none, or the switch waiting: the note of priority 1 or more closing.
    void controllerUnwatched();
    // 0x0046C060: the player's UI map registered (+0x18 / +0x20), no note of its controller up (+0x1C / +0x24).
    void playerUi(int player) {
        m_noNote[static_cast<size_t>(player)] = true;
        if (player == 1) m_secondUi = true;
    }
    // The front end's +0x20: the second player's UI map registered (cleared by split_selcar's BACK and a new driver,
    // not as its device goes).
    bool secondPlayerUi() const { return m_secondUi; }
    // 0x0046C208, the second player's controller whatever the front end's +0x38: connected with its note up (+0x24 0),
    // the note of priority 1 or more closing (0x005A7F70 inlined) and both players' notes counted as closed (+0x24
    // and +0x1C 1; nothing more of the first player's part). Lost with no note up, unless the first player's closed
    // one this frame (`opens`): the note "To continue, please reconnect .. controller port " (1632) and
    // "<device + 1>." (0x005A8030, priority 1; Stock problem: the console's text, not emptied as the first player's),
    // +0x24 0 when it opened.
    void watchSecondController(bool connected, int32_t device, bool opens);
    // The options from a race's pause (the game object's +0x3C set): opts_ingame_bg, its windows asking for the
    // race's windows (hud_pause) in place of the front end's.
    // pausing: the number of the race's player who paused (the race track's +0x10), -1 none.
    void startRaceOptions(int32_t pausing = -1);
    // The online windows from a race's pause (0x0047FB30's registrations): online_ingame_bg with the window of
    // [0x0072182C] over it; hud_pause and closedown go to the race's windows.
    void startRaceOnline();
    // The race's window asked for by the options (hud_pause), once; empty when none.
    std::string takeRaceWindow() { return std::exchange(m_raceWindow, {}); }

    // dt: seconds since the last frame.
    void update(float dt, const FrontendInput& in);
    void draw();

    const std::string& state() const { return m_state; }
    // The driver created or loaded, if any.
    const std::optional<save::Profile>& profile() const { return m_profile; }

private:
    using Popup = frontend::Popup;

    // The window over the windows (mode 8, the front end's +0x30): CMSLoader, car_StatsLogos.
    std::optional<Popup> m_overlay;

    using Message = frontend::Message;


    // 0x005A6DB0 / 0x005A7040: the manager's window of the name (ui::WindowManager).
    std::shared_ptr<ui::Screen> makeScreen(const std::string& name);
    void prepareWindow(ui::Screen& screen);
    static std::string windowFile(const std::string& name);
    // 0x005A8430: a window made and kept.
    void keepWindow(const char* name);
    void enter(std::string state);
    // A state's leave before `state` (none when it is the same) and a state's enter.
    void leaveState(const std::string& state);
    void enterState(const std::string& state);
    void enterPopup(Popup& popup);
    void updateWindow(ui::Screen& screen, const FrontendInput& in);
    void updateState(const FrontendInput& in);
    void updatePopup(Popup& popup, uint32_t event);
    void updateMessage(uint32_t event);
    // 0x005A7F90: unless the save device is busy (the manager's +0x110), msgbox_network (+0x3C) opens with the type,
    // the text and the answer (a note's +0x20, a question's +0x18; +0x28 0), its close (+0x40) cleared; the number of
    // the last message (+0x108) returned, 0 when it does not open. Its state is not entered (no 0x005A9180).
    int32_t netMessage(ui::MessageType type, const std::wstring& text, int32_t* answer);
    void updateNetMessage(uint32_t event);
    // The front end object's frame after the window manager's (0x0046C3FA .. 0x0046C80D, FrontendNetwork.cpp): the
    // network watched; true when the frame ends there (0x0046C64E: no invitation's note, no music frame).
    bool networkWatch();
    // The name of the window on top of the manager's stack (its +0x04's last; the windows over the windows and the
    // messages aside); empty with none.
    const std::string& topWindowName() const { return m_popups.empty() ? m_state : m_popups.back().state; }
    void finishSwitch();
    void helpPage(const std::string& window, bool force) override;
    // The autosave over the window (0x004648C0, 0x00464A10): the driver written.
    // The options' windows (Game/Frontend/Options), each with its state kept from visit to visit (the stock's
    // records); the front end is their OptionsHost.
    static inline frontend::OptionsMenu m_optionsMenu;
    static inline frontend::GameOptions m_gameOptions;
    static inline frontend::AudioOptions m_audioOptions;
    static inline frontend::SongList m_songList;
    static inline frontend::GraphicsOptions m_graphicsOptions;
    static inline frontend::WheelOptions m_wheelOptions;
    // OptionsHost
    void controlsSetUp() override;  // 0x004080D0 with 1
    const char* optionsMenu() const override { return m_racePause ? "opts_ingame_menu" : "opts_menu"; }
    bool racePause() const override { return m_racePause; }
    uint32_t secondPlayerEvent() const override { return m_playerEvents[1]; }
    void setEffectsVolume(float volume) override { m_sounds->setVolume(volume); }
    void setDetailFactor(float lodBias) override;
    void restorePlaylistChoice() override;
    // The network's windows (Game/Frontend/Network/LinkWindows.cpp; docs/spec/28-network.md, "The windows").
    void netHook() override;
    void netUnhook();
    void netSetupCame(uint8_t id);
    void netFollow() override;
    bool netDropped() override;
    bool netWait(net::SessionStage stage, bool loading) override;
    bool netKeys(uint32_t event, net::SessionStage stage, bool loading) override;
    void netCountdown() override;
    bool onlineTopMoving() const override;
    // The online sessions' side the front end keeps (Game/Frontend/FrontendOnline.cpp).
    // 0x004E1AC0: the matchmaking there and the online career ([0x0075EF88] 1).
    bool onlineCareer() const override;
    // 0x004A44E0: the online career's race setup.
    void onlineCareerRaceSetup() override;
    // 0x004D4090, the network's delayed call (a front end's member, as NetCall calls): the grid walk's timeout.
    void onlineGridWalkTimeout();
    void delayGridWalkTimeout() override { m_netCall = NetCall{1, 50.0f, &Frontend::onlineGridWalkTimeout}; }
    // 0x004EBBF0: the online career's pink slip ended by the race as the session is reset.
    void onlineRaceSettle();
    void onlineHostIntoLobby() override;
    void onlineHostArcade();
    void onlineHostCareer();
    bool onlineJoinSession(int32_t position) override;
    void onlineJoinArcade(int32_t position);
    void onlineJoinCareer(int32_t position);
    void enterOverlay();
    void closeOverlay() override;
    bool overlayUp() const override { return m_overlay.has_value(); }
    void leavePopup(Popup& popup);
    // The online windows over online_bg (0x004739C0 opens them in mode 5, they open each other in mode 7): their code
    // reads the window's screen as the current one (m_screen) while it runs.
    bool onlinePopup(const std::string& state) const;
    void onPopupScreen(Popup& popup, const std::function<void()>& run);
    void phoneFrame(ui::Screen& screen) override;
    ui::Screen* phoneScreen() override;
    void baseEnter(ui::Screen& s, const std::string& state) override;
    static bool baseEntered(const std::string& state);
    static bool keepsMenu(const std::string& state);
    void baseLeave(ui::Screen& s, const std::string& state) override;
    void goToMeeting() override;
    // GarageHost
    frontend::Garage& garage() override { return m_garage; }
    frontend::CarStats& carStats() override { return m_carStats; }
    frontend::CarStats& carStatsPurple() override { return m_carStatsPurple; }

    // ProfileHost
    bool nameRefused(const std::wstring& name) const override { return m_names.refuses(name); }
    frontend::KeyboardSetup& keyboardSetup() override { return m_keyboard.setup(); }

    // SaveHost
    void setSaveBusy(int32_t busy) override { m_saveBusy = busy; }
    TextTable& jetTexts() override { return m_jetTexts; }
    save::SaveGameSide& saveSide() override { return *this; }
    void setUpDriverName(const std::wstring& text) override;
    std::optional<save::Profile>& profile() override { return m_profile; }
    std::optional<save::Settings>& driverSettings() override {
        return m_data != nullptr ? m_data->settings : m_ownSettings;
    }
    void netCallFrame() override;
    void cancelNetCall() override { m_netCall.on = 0; }

    // save::SaveGameSide
    void buildProfile(save::SaveSource& source) override;
    bool applyProfile(const save::SaveSlot& slot, const save::SaveSource& source) override;
    void buildHighScores(save::SaveSource& source) override;
    bool applyHighScores(const save::SaveSource& source) override;
    void writeRegCf1() override;
    void readRegCf1() override;

    // WindowHost
    bool request(const std::string& state, frontend::SwitchMode mode) override;
    bool switchAsked() const override { return !m_next.empty(); }
    ui::Screen& baseScreen() override { return *m_screen; }
    UiSound& uiSounds() override { return *m_sounds; }
    render::IRenderDevice& device() override { return m_device; }
    void message(ui::MessageType type, const std::wstring& text, bool small, int* answer,
                 bool startNo = false) override;
    void messageTitle(const std::wstring& text) override;  // 0x005A8330
    bool messageUp() const override { return m_message.has_value(); }
    void closeMessage() override {
        if (m_message) m_message->close = true;
    }
    int32_t messageNumber() const override { return m_message ? m_messageNumber : 0; }
    int32_t lastMessageNumber() const override { return m_messageNumber; }
    bool messageTransitionsDone() const override { return m_message && m_message->screen->transitionsDone(); }
    // A line of scripts\jettext<language>.txt (0x004FAC80), the save device's texts: k + 63 the flag k's.
    const std::wstring& jetText(size_t line) const override;
    void setCrewNameTexts(const std::wstring& name) override;
    void sound(const char* name) override;
    // 0x005A8860 with a volume and the masks of its slot and voice (the options' Warning and SPEECH: 1).
    void sound(const char* name, float volume, uint32_t mask) override;
    GameData* gameData() override { return m_data; }
    const TextTable& gameTexts() const override { return m_gameTexts; }
    const Language& language() const override { return m_language; }
    scene::Engine* engine() override { return m_scene; }
    CrtRandom& viewLoading() override { return m_viewLoading; }
    float frameTime() override { return m_frameTime; }
    uint32_t scrollHeld() const override { return m_scroll; }
    float gameFrameTime() const override { return m_gameFrameTime; }
    uint32_t settingsValue(save::Settings::Field field) override;
    save::Settings& optionSettings() override;
    // The settings' music volume (+0x54 of the profile) and that x 0.25 ([0x006FFBEC]) for the windows that take the
    // front end's music down to a quarter.
    float settingsMusic() override { return std::bit_cast<float>(settingsValue(save::Settings::kMusicVolume)); }
    void musicQuarter() { setMusicVolume(settingsMusic() * 0.25f, false); }
    frontend::SharedState& shared() override { return s_shared; }
    static inline frontend::SharedState s_shared;

    // MainMenuHost
    void quitGame() override { m_quitGame = true; }

    // StartupHost
    float windowTimer() const override { return m_timer; }
    void setWindowTimer(float seconds) override { m_timer = seconds; }
    void releaseInput() override { m_inputRelease = true; }
    void forgetPlayersUi() override {
        m_noNote = {false, false};
        m_secondUi = false;
    }
    // OnlineHost
    frontend::OnlineVoice& onlineVoice() override { return m_onlineVoice; }
    // CustomRaceHost
    uint32_t playerEvent(size_t player) const override { return m_playerEvents[player]; }
    bool secondPlayer() const override { return m_secondPlayer; }
    void askSecondPlayerRelease() override { m_secondPlayerRelease = true; }
    void askSecondPlayerTake() override { m_secondPlayerTake = true; }
    size_t inputDevices() const override { return m_inputDevices; }
    void forgetSecondPlayerUi() override {
        m_secondUi = false;
        m_noNote[1] = false;
    }
    // CareerHost
    void setScreenFade(float fade) override { m_screenFade = fade; }
    frontend::HostMeeting& hostMeeting() override { return m_hostMeeting; }
    void watchControllers(bool on) override { m_watchControllers = on; }
    void respectMessage(int crew, float respect, float change, const std::wstring& comment) override;
    // 0x0040B600 on the speech object when there is one: its line stopped (the system stepped), nothing waiting.
    void stopSpeech() override;
    // 0x004B69B0 from the windows: the phone's state; idle also stops the speech.
    // The ring's or the dial's sound of the phone ([0x0075EF74]) stopped and forgotten.
    void statsCar(Car* car, int32_t garage) override;
    void afterRaceResults() override;
    void firstEventDealer() override;

    render::IRenderDevice& m_device;
    Language m_language;
    TextTable& m_gameTexts;
    NameFilter m_names;
    std::shared_ptr<ui::Screen> m_screen;
    ui::WindowManager m_windows;  // the interface manager's windows (0x005A5F90; +0x10 those it keeps)
    UiSound m_ui;  // the interface manager's sounds (+0xA8 .. +0x104)
    UiSound* m_sounds = &m_ui;
    static inline float m_loaderEffects = 0.0f;  // CMSLoader's +0x0C: the effects' volume while it mutes the interface  // the manager the sounds go to (the race's in a race's pause)
    // 0x0046BD80: with the manager and the music's handle, the music's volume (unless only the target is asked: +0x10
    // and the player's) and its target (+0x14).
    void setMusicVolume(float volume, bool targetOnly) override;
    // A driver loaded or saved (0x004679B0 flag 11, 0x00463F30 flag 12) and "start" (0x00467060): the music at the
    // settings' music volume (0x0046BD80), the effects' (0x005A89F0) and the speech's (0x00463DB0: the speech's
    // player's master volume, with the manager).
    void settingsVolumes() override;
    // 0x0046BED0: the music's volume (+0x10) to its target (+0x14) by (target - volume) / (1000 x the frame's time) in
    // [0.001, 1]; reaching 0.001 it becomes the target and is not given (the next frame gives it); then given to the
    // handle (+0x0C, whichever it is).
    void musicFade(float frameTime);
    uint32_t m_musicHandle = 0;   // +0x0C
    float m_musicVolume = 0.0f;   // +0x10
    float m_musicTarget = 0.0f;   // +0x14
    std::vector<Popup> m_popups;
    std::optional<Message> m_message;
    int32_t m_messageNumber = 0;  // the front end's +0x108
    int32_t m_saveBusy = 0;       // the front end's +0x110: no network message window while set (0x005A7F90)
    TextTable m_jetTexts;         // scripts\jettext<language>.txt
    // The save device's windows (Game/Frontend/Saves, Startup/BootUpCheck).
    static inline frontend::Driver m_driverWindow;
    static inline frontend::LoadReplay m_loadReplay;
    static inline frontend::SaveProfile m_saveProfile;
    static inline frontend::Autosave m_autosave;
    static inline frontend::LoadHighScores m_loadHighScores;
    static inline frontend::BootUpCheck m_bootUpCheck;
    // The garage's windows (Game/Frontend/Garage) and what they share; the garage's scene (the shared object's view)
    // is the front end's own, freed with it.
    static inline frontend::Garage m_garage;
    static inline frontend::CarBuy m_carBuy;
    static inline frontend::CarSel m_carSel;
    static inline frontend::CarStats m_carStats;        // Car_Stats2
    static inline frontend::CarStats m_carStatsPurple;  // Car_Stats2purple
    static inline frontend::Dealership m_dealership;
    static inline frontend::Repair m_repair;
    static inline frontend::ModShop m_modShop;
    static inline frontend::CustMenu m_custMenu;
    static inline frontend::TuningMenu m_tuningMenu;
    static inline frontend::PerformType m_performType;
    static inline frontend::PerformMod m_performMod;
    static inline frontend::BuyMod m_buyMod;
    static inline frontend::Performance m_performance;
    static inline frontend::Suppliers m_suppliers;
    static inline frontend::ExhaustMenu m_exhaustMenu;
    static inline frontend::SuspensionMenu m_suspensionMenu;
    static inline frontend::EngineChip m_engineChip;
    static inline frontend::GearboxMenu m_gearboxMenu;
    static inline frontend::ColourMenu m_colourMenu;
    static inline frontend::ColourChoose m_colourChoose;
    static inline frontend::ColourFocus m_colourFocus;
    static inline frontend::WheelColourMenu m_wheelColourMenu;
    static inline frontend::StylingMenu m_stylingMenu;
    static inline frontend::ModMenu m_modMenu;
    static inline frontend::WheelsMenu m_wheelsMenu;
    static inline frontend::DecalMenu m_decalMenu;
    static inline frontend::SponsorDecalMenu m_sponsorDecalMenu;
    static inline frontend::TintsMenu m_tintsMenu;
    static inline frontend::NeonMenu m_neonMenu;
    static inline frontend::HudColourMenu m_hudColourMenu;
    static inline frontend::IceMenu m_iceMenu;
    static inline frontend::HornMenu m_hornMenu;
    static inline frontend::SchemeMenu m_schemeMenu;
    // The career's windows (Game/Frontend/Career).
    static inline frontend::Advice m_advice;
    static inline frontend::CalendarHelp m_calendarHelp;
    static inline frontend::CareerLoad m_careerLoad;
    static inline frontend::CareerMenu m_careerMenu;
    static inline frontend::OldCareerMenu m_oldCareerMenu;
    static inline frontend::NewCont m_newCont;
    static inline frontend::DemoFlyby m_demoFlyby;
    static inline frontend::CareerRaceResults m_raceResults;
    static inline frontend::CareerChallengeResults m_challengeResults;
    static inline frontend::MeetingSummary m_meeting;
    static inline frontend::RespectChanges m_respectChanges;
    static inline frontend::RespectMessage m_respect;
    static inline frontend::Respect m_respectWindow;
    static inline frontend::RespectOverview m_respectOverview;
    static inline frontend::RespectHelp m_respectHelp;
    static inline frontend::CrewWindow m_crew;
    static inline frontend::StatsMenu m_statsMenu;
    static inline frontend::Stats m_stats;
    static inline frontend::CarCollection m_carCollection;
    static inline frontend::CalendarWindow m_calendar;
    static inline frontend::AtMeeting m_atMeeting;
    static inline frontend::HostMeeting m_hostMeeting;
    static inline frontend::CrewRaceSetup m_crewRaceSetup;
    static inline frontend::RaceSetupWindow m_raceSetup;
    static inline frontend::GridWalk m_gridWalk;
    static inline frontend::MobilePhone m_mobilePhone;
    // The arcade championships' windows (Game/Frontend/Arcade).
    static inline frontend::ArcadeChampionship m_arcadeChampionship;
    static inline frontend::ArcadeRace m_arcadeRace;
    static inline frontend::ArcadeLoading m_arcadeLoading;
    static inline frontend::ArcadeResults m_arcadeResults;
    // The custom race's windows (Game/Frontend/CustomRace).
    static inline frontend::InstantManu m_instantManu;
    static inline frontend::InstantSelCar m_instantSelCar;
    static inline frontend::InstantSelType m_instantSelType;
    static inline frontend::InstantSetup m_instantSetup;
    static inline frontend::SplitSelCar m_splitSelCar;
    // The windows the PC game never opens (Game/Frontend/Unused).
    static inline frontend::Playlist m_playlist;
    static inline frontend::ControllerPorts m_controllerPorts;
    static inline frontend::PcControls m_pcControls;
    static inline frontend::BackToCareerMenu m_backToCareerMenu;
    // The network game's windows (Game/Frontend/Network).
    static inline frontend::VoiceOptions m_voiceOptions;
    static inline frontend::OnlineStats m_onlineStats;
    static inline frontend::OnlineCrewRaceSetup m_onlineCrewRaceSetup;
    static inline frontend::OnlineRaceSetup m_onlineRaceSetup;
    static inline frontend::OnlineLogon m_onlineLogon;
    static inline frontend::OnlineCreate m_onlineCreate;
    static inline frontend::OnlineConnect m_onlineConnect;
    static inline frontend::OnlineMain m_onlineMain;
    static inline frontend::OnlineGridWalk m_onlineGridWalk;
    static inline frontend::OnlinePlayers m_onlinePlayers;
    static inline frontend::OnlinePlayerOptions m_onlinePlayerOptions;
    static inline frontend::OnlineFriends m_onlineFriends;
    static inline frontend::OnlineFriendOptions m_onlineFriendOptions;
    static inline frontend::OnlineVoice m_onlineVoice;
    static inline frontend::OnlineBg m_onlineBg;
    static inline frontend::OnlineMode m_onlineMode;
    static inline frontend::OnlineArcadeCreate m_onlineArcadeCreate;
    static inline frontend::OnlineArcadeSearch m_onlineArcadeSearch;
    static inline frontend::OnlineCareerCreate m_onlineCareerCreate;
    static inline frontend::OnlineCareerSearch m_onlineCareerSearch;
    static inline frontend::OnlineResults m_onlineResults;
    static inline frontend::LinkSearch m_linkSearch;
    static inline frontend::LinkLobby m_linkLobby;
    static inline frontend::Multi m_multi;
    static inline frontend::LinkSelCar m_linkSelCar;
    // The startup windows (Game/Frontend/Startup).
    static inline frontend::Disclaimer m_disclaimer;
    static inline frontend::Legals m_legals;
    static inline frontend::JuiceSplash m_juiceSplash;
    static inline frontend::Title m_title;
    static inline frontend::Loader m_loader;
    // [0x0075ED3C] .. [0x0075ED44]: the network's delayed call autosave makes (0x00464A10): on, the time left (run
    // down by the frame time) and the call; autosave's leave turns it off. Only CMSOnlineGridWalk's pink slip sets it
    // (0x004D40B0: 50, 0x004D4090).
    struct NetCall {
        int32_t on = 0;
        float left = 0.0f;
        void (Frontend::*call)() = nullptr;
    };
    NetCall m_netCall;
    scene::Engine* m_scene = nullptr;
    // The loading phase's own front end (0x00462D90), whose windows are arcade_loading, loader and custom_loader (the
    // game's front end has Arcade_Loading under the same name, case aside).
    bool m_loadingFrontEnd = false;
    bool m_raceLoadAsked = false;
    bool m_quitGame = false;
    bool m_inputRelease = false;
    // The startup windows with a front end's own state (Game/Frontend/Startup).
    frontend::Splash m_splash;
    frontend::Skippable m_skippable;
    // The windows of the opening movies: splash's +0x0C (the time since a press) and +0x10 (pressed), skippable's
    // +0x08 (the press that opened it still held).
    bool m_inputUi = false;
    bool m_inputConfigure = false;
    bool m_secondPlayerRelease = false;
    bool m_secondPlayerTake = false;
    // The windows' events of the two players (the window's +0x88 and +0x8C from the manager's frames +0x114 and
    // +0x134, 0x005A9E80) and the input manager's devices.
    std::array<uint32_t, 2> m_playerEvents{};
    bool m_secondPlayer = false;  // the input manager's +0x2C not -1
    size_t m_inputDevices = 1;
    bool m_watchControllers = false;         // the front end's +0x38
    std::array<bool, 2> m_noNote{};         // +0x1C, +0x24: no note of the player's controller up
    bool m_secondUi = false;                // +0x20: the second player's UI map registered
    // The window manager's note of a lost controller (+0x34: msgbox_critical, type 0, the state 0x00722FD8 with
    // its priority [0x00722FEC]); +0x38 set closes it at the next frame.
    std::optional<Message> m_critical;
    uint32_t m_criticalPriority = 0;
    bool m_criticalClose = false;
    bool openCritical(uint32_t priority, const std::wstring& text);  // 0x005A8030
    void closeCritical(uint32_t priority);                           // 0x005A7F70
    // The window manager's msgbox_network (+0x3C; its close +0x40): the network's notes (netMessage).
    std::optional<Message> m_netMessage;
    // The front end object's watch of the network (networkWatch): +0x2C the answer its notes write (their +0x20), set
    // going back from the network the next frame; +0x34 read before the cable's note, set by nothing but to 0; +0x3C
    // the frames the network was seen down (the note past 10); +0x50, +0x54 -1 as it is made and as the network comes
    // back, read by nothing.
    int32_t m_netNoteAnswer = 0;
    int32_t m_cableNoteHeld = 0;
    uint32_t m_cableFrames = 0;
    std::array<int32_t, 2> m_networkBack = {-1, -1};
    // This frame ended at 0x0046C64E: the invitation's note (0x0046C80D) not drawn.
    bool m_presenceSkipped = false;
    static inline bool m_framesUnderMessage = true;  // [0x0071E710]: the FRAME_ elements under a message drawn
    float m_screenFade = 0.0f;        // the front end's +0x10C: black over the whole screen, fading out
    std::string m_state;
    std::string m_next;
    frontend::SwitchMode m_nextMode = frontend::SwitchMode::None;  // the front end's +0xBC
    float m_switchTime = 0.0f;  // the front end's +0x100: ReplaceNow's wait, down by the frame time
    float m_timer = 0.0f;    // the state's countdown
    float m_elapsed = 0.0f;  // seconds since the window opened
    float m_clock = 0.0f;    // seconds since the front end started (scrolling textures)
    float m_frameTime = 0.0f;
    float m_gameFrameTime = 0.0f;  // the game's frame time ([G+4]+0x1C), not held
    // The new driver's windows (Game/Frontend/Profile).
    static inline frontend::Keyboard m_keyboard;
    static inline frontend::ChooseLogo m_chooseLogo;
    static inline frontend::ChoosePhone m_choosePhone;
    std::optional<save::Profile> m_profile;
    // The driver's settings: the game's (the save system's +0x54), one object as the stock's, so that what the windows
    // change is the game's at once and outlives the front end; a front end without the game's data its own.
    std::optional<save::Settings> m_ownSettings;
    GameData* m_data = nullptr;
    std::filesystem::path m_saveFolder;
    frontend::CharacterScreen m_character;
    frontend::CharacterPostRace m_postRace;
    // The garage's scenes (each front end's own records, as before).
    frontend::CutScene m_cutScene;
    frontend::RollingRoad m_rolling;
    // The menus' windows (Game/Frontend/Menus).
    static inline frontend::MainMenu m_mainMenu;
    static inline frontend::Extras m_extras;
    static inline frontend::Credits m_credits;
    static inline frontend::HiScores m_hiScores;
    static inline frontend::DemoMenu m_demoMenu;
    static inline frontend::ByeBye m_byeBye;
    static inline frontend::Cheats m_cheats;
    static inline frontend::VidTutorial m_vidTutorial;
    // The windows run the same as the base window or as a window over it (both dispatchers; Game/Frontend/Unused/CutWindows.cpp):
    // true when the state is one of them.
    bool enterSharedWindow(ui::Screen& s, const std::string& state);
    bool updateSharedWindow(ui::Screen& s, const std::string& state, uint32_t event);
    bool leaveSharedWindow(ui::Screen& s, const std::string& state);
    // The loading thread's generator (the car views' models; each thread's rand() starts at 1).
    CrtRandom m_viewLoading;
    // The window records' +0x04 (a Menu's highlighted item kept by the state's leave) and +0x08 (1 in the records'
    // data, cleared by their first enter), by the record's name: the records are the game's and outlive the front
    // ends.
    static inline std::map<std::string, size_t> s_keptMenu;
    static inline std::set<std::string> s_entered;
    // The window the phone's frame saw last ([0x0079E824]; each front end's own: a new one waits again).
    const ui::Screen* m_phoneScreen = nullptr;
    uint32_t m_scroll = 0;   // the input's +0x90: the direction held (input::kUp .. kRight)
    static inline int m_netAnswer = 0;  // [0x0079E7D8]: BACK's question in a network game's windows
    bool m_racePause = false;      // [game + 0x3C]: the options run from a race's pause
    // The driver's settings the network's players take their voice from ([G+0x64]+0x54), none before a driver.
    const save::Settings* voiceSettings() const { return m_data != nullptr && m_data->settings ? &*m_data->settings : nullptr; }
    int32_t m_racePausing = -1;    // the race track's pausing player's number (+0x10) as the options were made
    // A race of more players paused by its second player ([G+0x6C]+0x58 over 1, the pausing player's +0x00 1).
    bool secondPlayerPaused() const override {
        return m_racePause && m_data != nullptr && m_data->race.players > 1 && m_racePausing == 1;
    }
    std::string m_raceWindow;      // the race's window the options asked for
};

}  // namespace oj
