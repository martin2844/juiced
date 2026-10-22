// The front end's life: the windows it keeps, the states it enters and leaves, the switch between windows and its
// frame.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"

namespace oj {

using namespace frontend;

namespace {

// The windows of the opening movies.
bool isMovieState(const std::string& state) {
    return state == "splash" || state == "juicesplash" || state == "skippable";
}

std::string fileText(const std::filesystem::path& relative) {
    const auto bytes = GameFiles(std::filesystem::current_path()).read(relative);
    return bytes ? std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()) : std::string();
}

// The name a window's state was registered under (0x005A8F20 on the window's record): careerload and garageload share
// one record and garageload (0x00446CEF) comes first; windows the stock code never registers have none.
static const char* registeredName(const std::string& state) {
    if (_stricmp(state.c_str(), "careerload") == 0) return "garageload";
    if (state.empty() || state.starts_with("unregistered:")) return nullptr;
    return state.c_str();
}

}  // namespace

Frontend::Frontend(render::IRenderDevice& device, const Language& language, TextTable& gameTexts,
                   GameData* data, std::filesystem::path saveFolder, scene::Engine* scene)
    : m_device(device),
      m_language(language),
      m_gameTexts(gameTexts),
      m_windows(device, m_language),
      m_data(data != nullptr && data->loaded() ? data : nullptr),
      m_saveFolder(std::move(saveFolder)),
      m_scene(scene) {
    if (m_data != nullptr) m_data->saveDevice.setFolder(m_saveFolder / L"SAVE");
    m_names.load(fileText(std::filesystem::path("scripts") / "profanities.txt"),
                 fileText(std::filesystem::path("scripts") / "nonprofanities.txt"));
    // 0x0046B5A0 -> 0x005A5F90: the interface's manager with the bank audio/ui/ui, ui_unlocked and ui_locked taken
    // into its textures at once (0x0046B676, 0x005A8B70; here each read as a window first draws it), then (0x0046BA28)
    // the effects' volume of the settings (0x005A89F0; the music's handle is none yet).
    m_ui.open(GameFiles(std::filesystem::current_path()), "ui");
    m_windows.prepare = [this](ui::Screen& screen) { prepareWindow(screen); };
    m_windows.fileOf = &Frontend::windowFile;
    // 0x005A5F90 (0x005A6438 ..): the message windows kept.
    for (const char* name : {"msgbox", "msgbox_critical", "msgbox_network"}) keepWindow(name);
    m_ui.setVolume(std::bit_cast<float>(settingsValue(save::Settings::kEffectsVolume)));
    netHook();
}

// 0x0046BF70 -> 0x005A6770: the manager's windows closed with their states' exits (the stack from the bottom, the
// window over the windows, the message windows), then freed.
Frontend::~Frontend() {
    closeWindows();
    m_garage.view.reset();  // the front end's own scene of the garage
    netUnhook();
    if (movie::System* m = movie::System::get()) m->closeCurrent();
}

void Frontend::start(const std::string& state) {
    enter(state);
}

void Frontend::showStartup() {
    m_screen = makeScreen("unregistered:startup");
    m_state = "unregistered:startup";
    m_screen->hideContaining("ACCLAIMLOGO");
    if (const int logo = m_screen->find("JUICELOGO"); logo >= 0) {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        m_screen->placeOf(logo, x, y, w, h);
        m_screen->setPlace(logo, 0.2f, y, w, h);
    }
}

void Frontend::keepGameWindows() {
    static constexpr const char* kWindows[] = {
        // 0x0045FBC0
        "instant_seltype", "instant_setup", "instant_selcar", "instant_manu",
        "menu", "start", "loading", "driver", "credits", "vid_tutorial", "loadreplay", "viewhiscores", "bootupcheck",
        "extras",
        // 0x00484CF0
        "opts_audio", "opts_bg", "opts_menu", "opts_game_pc", "opts_songlist", "opts_ctl_port", "opts_graphics",
        "opts_ctl_pc", "opts_wheel_dx9", "opts_voice",
        // 0x00446CE0
        "garageload", "careerload", "custmenu", "wheelsmenu", "decalmenu", "sponsordecalmenu", "tuningmenu",
        "stylingmenu", "modmenu", "neonmenu", "hudcolourmenu", "wheelcolourmenu", "icemenu", "hornmenu", "tintsmenu",
        "colourmenu", "colourchoose", "colourfocus", "schememenu", "suspensionmenu", "enginechip", "suppliers",
        "exhaustmenu", "performtype", "performmod", "performance", "gearboxmenu", "modshop", "buymod",
        // 0x004C5470
        "Car_Stats2", "Car_Stats2purple", "Car_StatsLogos",
        "character", "characterPostRace",
        // 0x004DC1E0
        "CMSRespect", "CMSRespectOverview", "CMSRespectMsg", "CMSRespectHelp",
        "CMSRaceResults", "CMSGridWalk", "CMSMeetingSummary", "CMSRespectChanges", "CMSBet", "CMSLoader", "DemoFlyby",
        // 0x00466650
        "saveprofile", "savereplay", "autosave", "loadhighscores", "savehighscores",
        // 0x00471640
        "link_search", "link_lobby", "link_selcar", "split_selcar",
        // 0x0042C530
        "Arcade_Race", "Arcade_Championship", "Arcade_Results", "Arcade_Loading",
        "CMSMainMenu", "CMSCalendar", "CMSCalendarHelp", "CMSCrew", "CMSAtMeeting", "CMSChallengeResults",
        "CMSHostMeetingSetup", "CMSHostMeetingSetupRace", "CMSHostMeetingSummary", "CMSMobilePhone",
        "CMSMobilePhoneMenu", "CMSMobilePhoneContacts", "CMSRaceSetup", "CMSCrewRaceSetup", "CMSCarCollection2",
        "CMSCarDealership",
        // 0x004C3780
        "CMSNewCont", "CMSChooseLogo", "CMSMobileChoose",
        "CMSCheats", "CMSAdvice",
        // 0x004E1610
        "CMSStats", "CMSStatsMenu", "CMSStatsCarCollection",
        "CMSOnlineRaceSetup", "CMSOnlineCrewRaceSetup", "CMSOnlineGridWalk"};
    for (const char* name : kWindows) keepWindow(name);
    log::info("frontend: {} windows kept", m_windows.keptCount());
}

void Frontend::keepLoaderWindows() {
    keepWindow("loader");
}

void Frontend::keepRaceWindows() {
    static constexpr const char* kWindows[] = {
        // 0x00484E20
        "opts_ingame_audio", "opts_ingame_bg", "opts_ingame_menu", "opts_ingame_game", "opts_ingame_game_ply2",
        "opts_voice", "opts_graphics", "opts_wheel_dx9",
        // 0x0047FB30
        "online_ingame_bg", "online_players", "online_player_options", "online_friends", "online_friend_options",
        "online_voice", "online_arcade_create", "online_career_create",
        // 0x004666E0
        "autosave", "savereplay", "savehighscores", "newhighscore_cms"};
    for (const char* name : kWindows) keepWindow(name);
    netUnhook();
}

void Frontend::keepWindow(const char* name) {
    m_windows.keep(name);
}

void Frontend::instantRace(bool automatic, bool wet) {
    m_profile = save::Profile::create(L"Player", m_language.index, gameRandom());
    m_profile->settings.setU32(save::Settings::kMusicVolume, 0);
    if (m_data == nullptr) return;
    RaceSetup& race = m_data->race;
    race.gearbox = automatic ? 1 : 0;
    race.players = 1;
    race.opponents = 0;
    race.weather = wet ? 2 : 1;
    race.heat = 0;
}

void Frontend::startRaceOptions(int32_t pausing) {
    netHook();
    m_racePause = true;
    m_racePausing = pausing;
    enter("opts_ingame_bg");
}

void Frontend::startRaceOnline() {
    netHook();
    m_racePause = true;
    enter("online_ingame_bg");
}

void Frontend::startLoader(const std::string& window, const std::string& animation) {
    m_loadingFrontEnd = true;
    m_loader.askTips();
    enter(window);
    std::shared_ptr<ui::Screen> screen = makeScreen(animation);
    // 0x004639BE: the arcade's animation's TEXT over several lines (the others' as their .ui has them).
    if (screen && is(animation, "arcade_loader_anim")) screen->setMultiline("TEXT", true);
    m_popups.push_back(Popup{animation, std::move(screen)});
}

std::shared_ptr<ui::Screen> Frontend::makeScreen(const std::string& name) {
    return m_windows.open(name);
}

// The windows' sounds go through the manager's (the race's in a race's pause); their 3d elements are views of the
// front end's engine.
void Frontend::prepareWindow(ui::Screen& screen) {
    screen.sound = [this](const char* name) { m_sounds->play(name); };
    if (m_scene != nullptr) {
        screen.setView3dFactory([this](const ui::Element& e) -> std::unique_ptr<ui::View3d> {
            return std::make_unique<scene::View3d>(*m_scene, e, gameRandom());
        });
    }
}

// The windows the stock code never registers take the .ui their elements are found in (none for the repair's nor for
// the classes named by their records' addresses).
std::string Frontend::windowFile(const std::string& name) {
    if (is(name, "unregistered:CMSMainMenu")) return "CMSMainMenu";
    if (is(name, "unregistered:demomenu")) return "demomenu";
    if (is(name, "unregistered:byebye")) return "byebye";
    if (is(name, "unregistered:startup")) return "startup";
    if (is(name, "unregistered:repair") || name.starts_with("unregistered:0x")) return {};
    return name;
}

void Frontend::enter(std::string state) {
    leaveState(state);
    m_state = state;
    m_next.clear();
    m_nextMode = SwitchMode::None;
    m_elapsed = 0.0f;
    m_timer = 0.0f;
    for (Popup& popup : m_popups) leavePopup(popup);
    m_popups.clear();
    // The message window stays (0x005A8580 closes it only with its close flag, +0x48).
    m_screen = makeScreen(state);
    enterState(state);
}

void Frontend::closeWindows() {
    leaveState("");
    m_state.clear();
    m_next.clear();
    m_nextMode = SwitchMode::None;
    for (Popup& popup : m_popups) leavePopup(popup);
    m_popups.clear();
    closeOverlay();
    m_screen.reset();
    m_racePause = false;
    netUnhook();
}

void Frontend::releaseRecords() {
    m_raceSetup = frontend::RaceSetupWindow{};
    m_dealership = frontend::Dealership{};
    m_stats = frontend::Stats{};
    m_gridWalk = frontend::GridWalk{};
    m_linkSelCar = frontend::LinkSelCar{};
    m_onlineRaceSetup = frontend::OnlineRaceSetup{};
    m_onlineGridWalk = frontend::OnlineGridWalk{};
    m_onlineStats = frontend::OnlineStats{};
    m_instantSelCar = frontend::InstantSelCar{};
    m_splitSelCar = frontend::SplitSelCar{};
    m_arcadeRace = frontend::ArcadeRace{};
}

void Frontend::leaveState(const std::string& state) {
    if (m_screen && !is(state, m_state.c_str()) && keepsMenu(m_state)) baseLeave(*m_screen, m_state);
    if (m_state == "keyboard" && state != "keyboard") {
        m_keyboard.clear();  // 0x00460840 when the window closes
    }
    if (is(m_state, "CMSRespectChanges") && !is(state, "CMSRespectChanges")) {
        m_respectChanges.leave(*this);
    }
    if (m_state == "menu" && state != "menu") {
        m_mainMenu.leave(*this);
    }
    if (is(m_state, "CMSAtMeeting") && !is(state, "CMSAtMeeting")) {
        m_atMeeting.leave();
    }
    if (is(m_state, "CMSCutScene") && !is(state, "CMSCutScene")) {
        m_cutScene.leave(*m_screen);
    }
    if (is(m_state, "DemoFlyby") && !is(state, "DemoFlyby")) {
        m_demoFlyby.leave(*this);
    }
    if (is(m_state, "CMSStats") && !is(state, "CMSStats")) {
        m_stats.leave();
    }
    if (is(m_state, "car_rolling") && !is(state, "car_rolling")) {
        m_rolling.leave(*this, *m_screen);
    }
    if (is(m_state, "CMSRaceSetup") && !is(state, "CMSRaceSetup")) {
        m_raceSetup.leave();
    }
    if (is(m_state, "CMSNewCont") && !is(state, "CMSNewCont")) {
        m_newCont.leave(*this);
    }
    if ((is(m_state, "CMSCarDealership") || is(m_state, "CMSCarCollection2")) && !is(state, m_state.c_str())) {
        m_dealership.leave(*this);
    }
    if (is(m_state, "CMSHostMeetingSetup") && !is(state, "CMSHostMeetingSetup")) {
        m_hostMeeting.leaveSetup();
    }
    if (is(m_state, "CMSHostMeetingSetupRace") && !is(state, "CMSHostMeetingSetupRace")) {
        m_hostMeeting.leaveRace();
    }
    if (is(m_state, "CMSHostMeetingSummary") && !is(state, "CMSHostMeetingSummary")) {
        m_hostMeeting.leaveSummary();
    }
    if (is(m_state, "CMSGridWalk") && !is(state, "CMSGridWalk")) {
        m_gridWalk.leave();
    }
    if (is(m_state, "CMSMeetingSummary") && !is(state, "CMSMeetingSummary")) {
        m_meeting.leave(*this);
    }
    if (is(m_state, "CMSRespect") && !is(state, "CMSRespect")) m_respectWindow.leave(*this);
    if (is(m_state, "character") && !is(state, "character")) m_character.leave(*this);
    if (is(m_state, "carbuy") && !is(state, "carbuy")) m_carBuy.leave(*this);
    if (is(m_state, "instant_selcar") && !is(state, "instant_selcar")) m_instantSelCar.leave();
    if (is(m_state, "split_selcar") && !is(state, "split_selcar")) m_splitSelCar.leave();
    if (is(m_state, "link_selcar") && !is(state, "link_selcar")) m_linkSelCar.leave(*this);
    if ((is(m_state, "online_create_gs") || is(m_state, "online_connect_gs") || is(m_state, "online_main")) &&
        !is(state, m_state.c_str())) {
        frontend::leaveOnlineForm(*this);
    }
    if (is(m_state, "CMSOnlineRaceSetup") && !is(state, "CMSOnlineRaceSetup")) m_onlineRaceSetup.leave();
    // 0x00466AB0, skippable's leave: the front end's music (0x0046BDD0; held back from the disclaimer on).
    if (is(m_state, "skippable") && !is(state, "skippable")) startMusic();
    if (is(m_state, "online_stats") && !is(state, "online_stats")) m_onlineStats.leave();
    if (is(m_state, "CMSOnlineGridWalk") && !is(state, "CMSOnlineGridWalk")) m_onlineGridWalk.leave(*this);
    if ((is(m_state, "online_arcade_create") || is(m_state, "online_career_create")) && !is(state, m_state.c_str())) {
        frontend::leaveOnlineCreate(*this);
    }
    if ((is(m_state, "online_arcade_results") || is(m_state, "online_career_results")) &&
        !is(state, m_state.c_str())) {
        m_onlineResults.leave(*m_screen);
    }
    if (is(m_state, "Arcade_Race") && !is(state, "Arcade_Race")) m_arcadeRace.leave(*this, *m_screen);
    if (is(m_state, "instant_setup") && !is(state, "instant_setup")) m_instantSetup.leave();
    if (is(m_state, "carsel") && !is(state, "carsel")) m_carSel.leave(*this);
    if (is(m_state, "saveprofile") && !is(state, "saveprofile")) m_saveProfile.leave(*this);
    if (is(m_state, "bootupcheck") && !is(state, "bootupcheck") && m_data) m_data->saveDevice.reset();
    if (m_screen && !is(state, m_state.c_str())) leaveSharedWindow(*m_screen, m_state);
    // 0x00469D60: loadreplay's leave starts the replays' device again.
    if (is(m_state, "loadreplay") && !is(state, "loadreplay") && m_data) m_data->saveDevice.reset();
    if (is(m_state, "characterPostRace") && !is(state, "characterPostRace") && m_data != nullptr) {
        m_postRace.leave(*this);
    }
    if ((is(m_state, "careerload") || is(m_state, "garageload")) && !is(state, m_state.c_str())) {
        // 0x00435930, the leave of the record careerload and garageload share (0x0071FC60): the next window (the
        // garage's too) comes in under black that clears over a second; the front end's +0x38 1 (its controllers
        // watched).
        m_screenFade = 1.0f;
        m_watchControllers = true;
    }
}

void Frontend::enterState(const std::string& state) {
    log::info("frontend: {}", state);
    if (baseEntered(state)) baseEnter(*m_screen, state);
    if (state == "disclaimer") {
        m_disclaimer.enter(*this, *m_screen);
    } else if (state == "legals") {
        m_legals.enter(*this, *m_screen);
    } else if (state == "splash" && !skipIntroMovies) {
        m_splash.enter(*m_screen);
    } else if (state == "juicesplash" && !skipIntroMovies) {
        m_juiceSplash.enter(*m_screen);
    } else if (state == "skippable" && !skipIntroMovies) {
        m_skippable.enter(*this, *m_screen);
    } else if (state == "start") {
        m_title.enter(*this, *m_screen);
    } else if (state == "driver") {
        m_driverWindow.enter(*this, *m_screen);
    } else if (state == "saveprofile") {
        m_saveProfile.enter(*this);
    } else if (state == "keyboard") {
        m_keyboard.enter(*this, *m_screen);
    } else if (_stricmp(state.c_str(), "DemoFlyby") == 0) {
        m_demoFlyby.enter(*this, *m_screen);
    } else if (state == "menu") {
        m_mainMenu.enter(*this, *m_screen);
    } else if (is(state, "careerload") || is(state, "garageload")) {
        m_careerLoad.enter(*this, *m_screen);
    } else if (is(state, "CMSRaceResults")) {
        m_raceResults.enter(*this, *m_screen);
    } else if (is(state, "CMSRespectChanges")) {
        m_respectChanges.enter(*this, *m_screen);
    } else if (is(state, "CMSMeetingSummary")) {
        m_meeting.enter(*this, *m_screen);
    } else if (is(state, "CMSMainMenu")) {
        m_careerMenu.enter(*this, *m_screen);
    } else if (is(state, "unregistered:demomenu")) {
        m_demoMenu.enter();
    } else if (is(state, "unregistered:byebye")) {
        m_byeBye.enter(*m_screen);
    } else if (is(state, "unregistered:CMSMainMenu")) {
        m_oldCareerMenu.enter(*this, *m_screen);
    } else if (is(state, "unregistered:repair")) {
        m_repair.enter(*this, *m_screen);
    } else if (enterSharedWindow(*m_screen, state)) {
    } else if (is(state, "CMSCalendar")) {
        m_calendar.enter(*this, *m_screen);
    } else if (is(state, "CMSAtMeeting")) {
        m_atMeeting.enter(*this, *m_screen);
    } else if (is(state, "bootupcheck")) {
        m_bootUpCheck.enter(*this, *m_screen);
    } else if (is(state, "loadreplay")) {
        m_loadReplay.enter(*this, *m_screen);
    } else if (is(state, "CMSCutScene")) {
        m_cutScene.enter(*this, *m_screen);
    } else if (is(state, "car_rolling")) {
        m_rolling.enter(*this, *m_screen);
    } else if (is(state, "CMSRaceSetup")) {
        m_raceSetup.enter(*this, *m_screen);
    } else if (is(state, "CMSNewCont")) {
        m_newCont.enter(*this, *m_screen);
    } else if (is(state, "CMSCarDealership") || is(state, "CMSCarCollection2")) {
        m_dealership.enter(*this, *m_screen);
    } else if (is(state, "extras")) {
        m_extras.enter(*this, *m_screen);
    } else if (is(state, "vid_tutorial")) {
        m_vidTutorial.enter(*this, *m_screen);
    } else if (is(state, "CMSCheats")) {
        m_cheats.enter(*this, *m_screen);
    } else if (is(state, "CMSHostMeetingSetup")) {
        m_hostMeeting.enterSetup(*this, *m_screen);
    } else if (is(state, "CMSHostMeetingSetupRace")) {
        m_hostMeeting.enterRace(*this, *m_screen);
    } else if (is(state, "CMSHostMeetingSummary")) {
        m_hostMeeting.enterSummary(*this, *m_screen);
    } else if (is(state, "viewhiscores")) {
        m_hiScores.enter(*this, *m_screen);
    } else if (is(state, "credits")) {
        m_credits.enter();
    } else if (is(state, "opts_bg") || is(state, "opts_ingame_bg")) {
        // 0x0047FF60: the options' menu over this window (opts_ingame_menu from a race's pause, [G+0x3C]).
        request(optionsMenu(), SwitchMode::Push);
    } else if (is(state, "CMSStatsCarCollection")) {
        m_carCollection.enter(*this, *m_screen);
    } else if (is(state, "CMSRespectHelp")) {
        m_respectHelp.enter(*this, *m_screen);
    } else if (is(state, "CMSRespectOverview")) {
        m_respectOverview.enter(*this, *m_screen);
    } else if (is(state, "CMSStatsMenu")) {
        m_statsMenu.enter(*this, *m_screen);
    } else if (is(state, "CMSStats")) {
        m_stats.enter(*this, *m_screen);
    } else if (is(state, "CMSRespect")) {
        m_respectWindow.enter(*this, *m_screen);
    } else if (is(state, "CMSCrew")) {
        m_crew.enter(*this, *m_screen);
    } else if (is(state, "CMSChallengeResults")) {
        m_challengeResults.enter(*this, *m_screen);
    } else if (is(state, "CMSCrewRaceSetup")) {
        m_crewRaceSetup.enter(*this, *m_screen);
    } else if (is(state, "CMSGridWalk")) {
        m_gridWalk.enter(*this, *m_screen);
    } else if (is(state, "character")) {
        if (m_data != nullptr) m_character.enter(*this, *m_screen);
    } else if (is(state, "loading")) {
        m_raceLoadAsked = true;
    } else if (is(state, "Arcade_Loading") && !m_loadingFrontEnd) {
        m_arcadeLoading.enter(*this, *m_screen);
    } else if (is(state, "arcade_loading") || is(state, "loader") || is(state, "custom_loader")) {
        m_loader.enter(*this, *m_screen);
    } else if (is(state, "characterPostRace")) {
        if (m_data != nullptr && m_data->career) {
            m_postRace.enter(*this, *m_screen);
        }
    } else if (is(state, "carbuy")) {
        m_carBuy.enter(*this, *m_screen);
    } else if (is(state, "Arcade_Championship")) {
        m_arcadeChampionship.enter(*this, *m_screen);
    } else if (is(state, "Arcade_Race")) {
        m_arcadeRace.enter(*this, *m_screen);
    } else if (is(state, "Arcade_Results")) {
        m_arcadeResults.enter(*this, *m_screen);
    } else if (is(state, "link_search")) {
        m_linkSearch.enter(*this, *m_screen);
    } else if (is(state, "link_lobby")) {
        m_linkLobby.enter(*this, *m_screen);
    } else if (is(state, "link_selcar")) {
        m_linkSelCar.enter(*this, *m_screen);
    } else if (is(state, "multi")) {
        m_multi.enter(*this, *m_screen);
    } else if (is(state, "online_logon_gs")) {
        m_onlineLogon.enter(*this, *m_screen);
    } else if (is(state, "online_create_gs")) {
        m_onlineCreate.enter(*this, *m_screen);
    } else if (is(state, "online_connect_gs")) {
        m_onlineConnect.enter(*this, *m_screen);
    } else if (is(state, "online_main")) {
        m_onlineMain.enter(*this, *m_screen);
    } else if (is(state, "online_stats")) {
        m_onlineStats.enter(*this, *m_screen);
    } else if (is(state, "CMSOnlineCrewRaceSetup")) {
        m_onlineCrewRaceSetup.enter(*this, *m_screen);
    } else if (is(state, "CMSOnlineRaceSetup")) {
        m_onlineRaceSetup.enter(*this, *m_screen);
    } else if (is(state, "CMSOnlineGridWalk")) {
        m_onlineGridWalk.enter(*this, *m_screen);
    } else if (is(state, "online_bg") || is(state, "online_ingame_bg")) {
        m_onlineBg.enter(*this);
    } else if (is(state, "online_mode")) {
        m_onlineMode.enter();
    } else if (is(state, "online_arcade_create")) {
        m_onlineArcadeCreate.enter(*this, *m_screen);
    } else if (is(state, "online_arcade_search")) {
        m_onlineArcadeSearch.enter(*this, *m_screen);
    } else if (is(state, "online_career_create")) {
        m_onlineCareerCreate.enter(*this, *m_screen);
    } else if (is(state, "online_career_search")) {
        m_onlineCareerSearch.enter(*this, *m_screen);
    } else if (is(state, "online_arcade_results") || is(state, "online_career_results")) {
        m_onlineResults.enter(*this, *m_screen);
    } else if (is(state, "instant_manu")) {
        m_instantManu.enter(*this, *m_screen);
    } else if (is(state, "instant_selcar")) {
        m_instantSelCar.enter(*this, *m_screen);
    } else if (is(state, "split_selcar")) {
        m_splitSelCar.enter(*this, *m_screen);
    } else if (is(state, "instant_seltype")) {
        m_instantSelType.enter(*this, *m_screen);
    } else if (is(state, "instant_setup")) {
        m_instantSetup.enter(*this, *m_screen);
    } else if (is(state, "carsel")) {
        m_carSel.enter(*this, *m_screen);
    } else if (state == "cmschooselogo") {
        m_chooseLogo.enter(*this, *m_screen);
    }
}

// A frame of the front end (0x005A6AB0): the message window takes the events first; a note or a question (types 1
// and 3) keeps the windows under it waiting. Of the windows the top one runs its state; the ones under it only turn
// their 3d views. The switch asked for this frame comes at its end.
void Frontend::update(float dt, const FrontendInput& in) {
    // The front end object's frame (0x0046BFD0) first fades its music (0x0046BED0) by the game's frame time.
    if (!m_racePause) musicFade(dt);
    m_ui.frame(dt);
    // The front end's frame time (+0x104) is the game's held to 0.1 s at most; the scrolling textures go by the game's
    // clock.
    m_clock += dt;
    m_gameFrameTime = dt;
    if (0.1f <= dt) dt = 0.1f;
    m_elapsed += dt;
    m_timer -= dt;
    m_frameTime = dt;
    // 0x005A8805 .. 0x005A8837: the message (+0x44), the note (+0x34) and the network's message (+0x3C) asked to close
    // go.
    if (m_message && m_message->close) {
        m_message.reset();
    }
    if (m_criticalClose) {
        m_critical.reset();
        m_criticalClose = false;
    }
    if (m_netMessage && m_netMessage->close) {
        m_netMessage.reset();
    }
    m_scroll = in.ui.scroll;
    // 0x005A6AB0: the first player's event kept by the manager (+0x114 ..): the 3d views' stick and pad.
    scene::View3d::stick[0] = in.ui.axes[2];
    scene::View3d::stick[1] = in.ui.axes[3];
    scene::View3d::pad = in.ui.scroll;
    m_playerEvents = {in.ui.event, in.second ? in.ui2.event : input::kNone};
    m_secondPlayer = in.second;
    m_inputDevices = in.devices;
    // 0x005A6B3C: a note of a lost controller up (+0x34) alone takes the events, else the network's message (+0x3C);
    // else the message (+0x44), then unless it waits (+0x4C) or a switch waits its frame (ReplaceNow) the windows: the
    // top one fully, the ones under it their 3d views, then the window over the windows (+0x30).
    const bool switchWaits = m_nextMode == SwitchMode::ReplaceNow;
    if (m_critical) {
        updateWindow(*m_critical->screen, in);
    } else if (m_netMessage) {
        updateWindow(*m_netMessage->screen, in);
        updateNetMessage(in.ui.event);
    } else {
        if (m_message) {
            updateWindow(*m_message->screen, in);
            updateMessage(in.ui.event);
        }
        if ((!m_message || !m_message->modal) && !switchWaits) {
            if (!m_popups.empty()) {
                updateWindow(*m_popups.back().screen, in);
                uint32_t popupEvent = in.ui.event;
#ifdef OJ_MACOS
                // The paint purchase shortcut must not affect text entry or other menus.
                if (is(m_popups.back().state, "colourmenu") && in.typed == '0')
                    popupEvent = input::kSelect2;
#endif
                updatePopup(m_popups.back(), popupEvent);
            } else {
                if (m_screen) updateWindow(*m_screen, in);
                updateState(in);
            }
            if (m_overlay) {
                updateWindow(*m_overlay->screen, in);
                if (is(m_overlay->state, "performance")) m_performance.update(*this, *m_overlay->screen);
            }
        }
    }
    // After the window manager's frame the front end object's watch of the network (0x0046C3FA); its way back from the
    // network ends the frame there.
    m_presenceSkipped = networkWatch();
    finishSwitch();
    if (m_presenceSkipped) return;
    // 0x0046C81E: and ends with the game's music's frame (0x004090D0), the next song unless a preview runs.
    if (!m_racePause && m_data != nullptr) jukebox().update(m_data->playlist, m_gameFrameTime, !jukebox().previewing);
}

// An event while a window still moves ends the transitions of the windows open and its own (0x005A9E80: a window
// under it going out is hidden then, so that it is shown again when the window over it closes); menus take their
// directions (0x005B11A0); buttons move the highlight to the neighbour they name (0x005A9F70).
void Frontend::updateWindow(ui::Screen& screen, const FrontendInput& in) {
    if (in.ui.event != input::kNone) {
        std::vector<ui::Screen*> open{m_screen.get()};
        for (const Popup& popup : m_popups) open.push_back(popup.screen.get());
        ui::endTransitions(screen, open);
    }
    // Both play SELECT through the window's sound when the highlight moves. A message's menu takes the second player's
    // event too (the window's +0x8C).
    screen.menuEvent(in.ui.event, m_playerEvents[1]);
    if (in.ui.event >= input::kUp && in.ui.event <= input::kRight) screen.moveHighlight(in.ui.event);
}

// The switch asked for (0x005A8580): Replace waits a frame (it becomes ReplaceNow, the wait +0x100 0 run down by the
// frame time; the black over the screen, +0x10C, is cleared each frame of it), ReplaceNow and ReplaceAfterTransitions
// (once the transitions of the top window have ended) replace the windows unless the bottom one's state is registered
// under the name asked for (then nothing happens), ReplaceAlways replaces them always, Push opens a window over the
// current one, Pop takes the top one off, Swap does both, Overlay puts a window over the windows.
void Frontend::finishSwitch() {
    if (m_nextMode == SwitchMode::None) {
        return;
    }
    if (m_nextMode == SwitchMode::Replace) {
        m_switchTime = 0.0f;
        m_nextMode = SwitchMode::ReplaceNow;
        return;
    }
    if (m_nextMode == SwitchMode::ReplaceNow) {
        m_screenFade = 0.0f;
        m_switchTime = m_switchTime - m_frameTime;
        if (!(m_switchTime < 0.0f)) return;
    }
    if (m_nextMode == SwitchMode::Push) {
        Popup popup{m_next, makeScreen(m_next)};
        m_next.clear();
        m_nextMode = SwitchMode::None;
        log::info("frontend: {} over {}", popup.state, m_state);
        m_popups.push_back(std::move(popup));
        enterPopup(m_popups.back());
        return;
    }
    if (m_nextMode == SwitchMode::Pop) {
        // 0x005A83A0: the top window closed, then the window over the windows (its leave) taken away.
        m_next.clear();
        m_nextMode = SwitchMode::None;
        if (!m_popups.empty()) {
            log::info("frontend: {} closed", m_popups.back().state);
            leavePopup(m_popups.back());
            m_popups.pop_back();
        }
        closeOverlay();
        return;
    }
    if (m_nextMode == SwitchMode::Overlay) {
        // 0x005A7040: one there is freed without its leave; the window loaded and its state entered.
        m_overlay.reset();
        m_overlay = Popup{m_next, makeScreen(m_next)};
        m_next.clear();
        m_nextMode = SwitchMode::None;
        log::info("frontend: {} over the windows", m_overlay->state);
        enterOverlay();
        return;
    }
    if (m_nextMode == SwitchMode::Swap && m_popups.empty()) {
        // 0x005A87B5 with no window over the current one: the current one is the top window, closed (its leave), and
        // the new one opened alone.
        closeOverlay();
        enter(m_next);
        return;
    }
    if (m_nextMode == SwitchMode::Swap) {
        // 0x005A87B5: the top window closed (as Pop, 0x005A83A0), then the new one over the rest (as Push).
        if (!m_popups.empty()) {
            log::info("frontend: {} closed", m_popups.back().state);
            leavePopup(m_popups.back());
            m_popups.pop_back();
        }
        closeOverlay();
        m_nextMode = SwitchMode::Push;
        finishSwitch();
        return;
    }
    // ReplaceAfterTransitions waits for the transitions of the top window (0x005AA120 on the last of the stack).
    ui::Screen* top = !m_popups.empty() ? m_popups.back().screen.get() : m_screen.get();
    if (m_nextMode == SwitchMode::ReplaceAfterTransitions && top != nullptr && !top->transitionsDone()) {
        return;
    }
    const std::string next = m_next;
    const bool same = m_nextMode != SwitchMode::ReplaceAlways && m_screen && registeredName(m_state) != nullptr &&
                      _stricmp(registeredName(m_state), next.c_str()) == 0;
    m_next.clear();
    m_nextMode = SwitchMode::None;
    m_switchTime = 0.0f;
    if (same) {
        log::info("frontend: {} asked for again, nothing changes", next);
        return;
    }
    // ReplaceNow, ReplaceAfterTransitions and ReplaceAlways (0x005A85D7, 0x005A86B0, 0x005A6F90) also take the window
    // over the windows away, its state left.
    closeOverlay();
    enter(next);
}

void Frontend::enterOverlay() {
    if (is(m_overlay->state, "car_StatsLogos")) {
        frontend::CarStats::enterLogos(*this, *m_overlay->screen);
    } else if (is(m_overlay->state, "performance")) {
        m_performance.enter(*this, *m_overlay->screen);
    }
    // CMSLoader (0x004CA910): the effects' volume ([G+0x64]+0x58) kept in +0x0C and the interface's sounds at 0
    // (0x005A89F0); the front end's +0x38 0 (its controllers not watched). Its leave (0x004CA950) gives
    // them back.
    if (is(m_overlay->state, "CMSLoader")) {
        m_loaderEffects = std::bit_cast<float>(settingsValue(save::Settings::kEffectsVolume));
        m_sounds->setVolume(0.0f);
        m_watchControllers = false;
    }
}

// The window over the windows left (its state's leave) and freed.
void Frontend::closeOverlay() {
    if (!m_overlay) return;
    log::info("frontend: {} taken away", m_overlay->state);
    if (is(m_overlay->state, "CMSLoader")) {
        m_sounds->setVolume(m_loaderEffects);
        m_watchControllers = true;
    }
    m_overlay.reset();
}

bool Frontend::request(const std::string& state, SwitchMode mode) {
    if (!m_next.empty() || m_nextMode != SwitchMode::None) {
        return false;
    }
    if (m_racePause && (is(state, "hud_pause") || is(state, "closedown"))) {
        // The race's window manager takes it (the windows of the options all go).
        if (m_raceWindow.empty()) m_raceWindow = state;
        return true;
    }
    // Names the stock asks for that no record takes (car_maintain from carsel, opts_ctl_ps2 and opts_wheel from
    // opts_ctl_port): 0x005A6DB0 finds no state and the window of the name's .ui, if there is one, comes up with
    // nothing to run it (0x005A9490 gives an empty window when the file is missing).
    m_next = state;
    m_nextMode = mode;
    // 0x005A8520: ReplaceAfterTransitions sends the top window's elements out (0x005AA0E0).
    if (mode == SwitchMode::ReplaceAfterTransitions) {
        ui::Screen* top = !m_popups.empty() ? m_popups.back().screen.get() : m_screen.get();
        if (top != nullptr) top->transitionsOut();
    }
    return true;
}

void Frontend::updateState(const FrontendInput& in) {
    const bool pressed = in.ui.anyInput;
    // 0x00466D91, 0x00466FE1, 0x004668CE, 0x00466C3A, 0x00466ADD: these windows let the keyboard's first player go
    // (0x0058FEA0) each frame.
    if (m_state == "disclaimer" || m_state == "legals" || m_state == "splash" || m_state == "juicesplash" ||
        m_state == "skippable") {
        m_inputRelease = true;
    }
    if (m_state == "disclaimer") {
        m_disclaimer.update(*this);
    } else if (m_state == "legals") {
        m_legals.update(*this, pressed);
    } else if (isMovieState(m_state) && skipIntroMovies) {
        request(m_state == "splash"        ? "juicesplash"
                : m_state == "juicesplash" ? "skippable"
                                           : "start",
                SwitchMode::Replace);
    } else if (m_state == "splash") {
        m_splash.update(*this, *m_screen, pressed);
    } else if (m_state == "juicesplash") {
        m_juiceSplash.update(*this, *m_screen, pressed);
    } else if (m_state == "skippable") {
        m_skippable.update(*this, *m_screen, pressed);
    } else if (m_state == "start") {
        m_title.update(*this, in.playerTaken);
    } else if (m_state == "driver") {
        m_driverWindow.update(*this, *m_screen, in.ui.event);
    } else if (m_state == "keyboard") {
        m_keyboard.update(*this, *m_screen, in.ui.event, in.typed, in.shiftHeld);
    } else if (m_state == "cmschooselogo") {
        m_chooseLogo.update(*this, *m_screen, in.ui.event);
    } else if (m_state == "cmsmobilechoose") {
        m_choosePhone.update(*this, *m_screen, in.ui.event);
    } else if (m_state == "saveprofile") {
        m_saveProfile.update(*this);
    } else if (is(m_state, "bootupcheck")) {
        m_bootUpCheck.update(*this);
    } else if (m_state == "menu") {
        m_mainMenu.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "DemoFlyby")) {
        m_demoFlyby.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "careerload") || is(m_state, "garageload")) {
        m_careerLoad.update(*this, *m_screen);
    } else if (is(m_state, "CMSRaceResults")) {
        m_raceResults.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSRespectChanges")) {
        m_respectChanges.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSMeetingSummary")) {
        m_meeting.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSMainMenu")) {
        m_careerMenu.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "unregistered:demomenu")) {
        m_demoMenu.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "unregistered:byebye")) {
        m_byeBye.update(*this, in.ui.anyInput);
    } else if (is(m_state, "unregistered:CMSMainMenu")) {
        m_oldCareerMenu.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "unregistered:repair")) {
        m_repair.update(*this, *m_screen, in.ui.event);
    } else if (updateSharedWindow(*m_screen, m_state, in.ui.event)) {
    } else if (is(m_state, "CMSCalendar")) {
        m_calendar.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSAtMeeting")) {
        m_atMeeting.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "loadreplay")) {
        m_loadReplay.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSCutScene")) {
        m_cutScene.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "car_rolling")) {
        m_rolling.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSRaceSetup")) {
        m_raceSetup.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "Arcade_Championship")) {
        m_arcadeChampionship.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "Arcade_Race")) {
        m_arcadeRace.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "Arcade_Results")) {
        m_arcadeResults.update(*this, in.ui.event);
    } else if (is(m_state, "link_search")) {
        m_linkSearch.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "link_lobby")) {
        m_linkLobby.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "link_selcar")) {
        m_linkSelCar.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "multi")) {
        m_multi.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_logon_gs")) {
        m_onlineLogon.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_create_gs")) {
        m_onlineCreate.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_connect_gs")) {
        m_onlineConnect.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_main")) {
        m_onlineMain.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_stats")) {
        m_onlineStats.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSOnlineCrewRaceSetup")) {
        m_onlineCrewRaceSetup.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSOnlineRaceSetup")) {
        m_onlineRaceSetup.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSOnlineGridWalk")) {
        m_onlineGridWalk.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_mode")) {
        m_onlineMode.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_arcade_create")) {
        m_onlineArcadeCreate.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_arcade_search")) {
        m_onlineArcadeSearch.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_career_create")) {
        m_onlineCareerCreate.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_career_search")) {
        m_onlineCareerSearch.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "online_arcade_results") || is(m_state, "online_career_results")) {
        m_onlineResults.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "instant_manu")) {
        m_instantManu.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "instant_selcar")) {
        m_instantSelCar.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "split_selcar")) {
        m_splitSelCar.update(*this, *m_screen);
    } else if (is(m_state, "instant_seltype")) {
        m_instantSelType.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "instant_setup")) {
        m_instantSetup.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSNewCont")) {
        m_newCont.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSCarDealership") || is(m_state, "CMSCarCollection2")) {
        m_dealership.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "extras")) {
        m_extras.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "vid_tutorial")) {
        m_vidTutorial.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSCheats")) {
        m_cheats.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSHostMeetingSetup")) {
        m_hostMeeting.updateSetup(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSHostMeetingSetupRace")) {
        m_hostMeeting.updateRace(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSHostMeetingSummary")) {
        m_hostMeeting.updateSummary(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "viewhiscores")) {
        m_hiScores.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "credits")) {
        m_credits.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "opts_bg") || is(m_state, "opts_ingame_bg")) {
        // 0x0047FFF0: with no window over this one the options' menu again.
        if (m_popups.empty()) request(optionsMenu(), SwitchMode::Push);
    } else if (is(m_state, "CMSStatsCarCollection")) {
        m_carCollection.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSRespectHelp")) {
        m_respectHelp.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSRespectOverview")) {
        m_respectOverview.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSStatsMenu")) {
        m_statsMenu.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSStats")) {
        m_stats.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "carbuy")) {
        m_carBuy.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "carsel")) {
        m_carSel.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSRespect")) {
        m_respectWindow.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSCrew")) {
        m_crew.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSChallengeResults")) {
        m_challengeResults.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSCrewRaceSetup")) {
        m_crewRaceSetup.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "CMSGridWalk")) {
        m_gridWalk.update(*this, *m_screen, in.ui.event);
    } else if (is(m_state, "character")) {
        if (m_data != nullptr) {
            m_character.update(*this, *m_screen, in.ui.event);
        }
    } else if (is(m_state, "characterPostRace")) {
        if (m_data != nullptr && m_data->career) {
            m_postRace.update(*this, *m_screen, in.ui.event);
        }
    }
}

}  // namespace oj
