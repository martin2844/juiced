// What most windows do on entering and leaving (the base window of the stock code) and the settings they read.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"

#include <algorithm>

namespace oj {

using namespace frontend;

namespace {

// The windows whose enter begins with the windows' enter (0x005A5F20) itself and those whose leave keeps the Menu's
// item (0x004C7F70, or a leave of its own storing the menu's +0x13C in the state's +0x04), the garage's windows left
// out (their enters and leaves do it by name). A window of the first list without a leave of the second selects the
// item its state started with (0).
constexpr const char* kBaseEntered[] = {
    "Arcade_Championship", "Arcade_Race", "Arcade_Results", "Car_Stats2", "Car_Stats2purple", "carbuy", "carsel",
    "character", "characterPostRace", "CMSAtMeeting", "CMSBet", "CMSCalendar", "CMSCalendarHelp",
    "CMSCarCollection2", "CMSCarDealership", "CMSChallengeResults", "CMSCheats", "CMSCrew", "CMSCrewRaceSetup",
    "CMSGridWalk", "CMSHostMeetingSetup", "CMSHostMeetingSetupRace", "CMSHostMeetingSummary", "CMSLoader",
    "CMSMainMenu", "CMSMeetingSummary", "CMSMobileChoose", "CMSMobilePhone", "CMSMobilePhoneContacts",
    "CMSMobilePhoneMenu", "CMSOnlineCrewRaceSetup", "CMSOnlineGridWalk", "CMSOnlineRaceSetup", "CMSRaceResults",
    "CMSRaceSetup", "CMSRespect", "CMSRespectChanges", "CMSStats", "CMSStatsCarCollection", "CMSStatsMenu", "DemoFlyby",
    "driver", "instant_selcar", "instant_seltype", "instant_setup", "keyboard", "link_lobby", "link_search", "loadhighscores",
    "loadreplay", "menu", "multi", "newhighscore_cms", "online_arcade_create", "online_arcade_results",
    "online_arcade_search", "online_bg", "online_career_create", "online_career_results", "online_career_search",
    "online_connect_gs", "online_create_gs", "online_friend_options", "online_friends", "online_ingame_bg",
    "online_logon_gs", "online_main", "online_mode", "online_player_options", "online_players", "online_voice",
    "opts_audio", "opts_bg", "opts_game_pc", "opts_graphics", "opts_ingame_audio", "opts_ingame_bg",
    "opts_ingame_game", "opts_ingame_game_ply2", "opts_ingame_menu", "opts_menu", "opts_voice", "opts_wheel_dx9",
    "savehighscores", "split_selcar", "unregistered:CMSMainMenu", "unregistered:demomenu", "unregistered:repair",
    "unregistered:0x0072212C", "unregistered:0x00722138", "unregistered:0x007225D8"};
constexpr const char* kKeepsMenu[] = {
    "Arcade_Championship", "Arcade_Loading", "Arcade_Race", "Arcade_Results", "Car_Stats2", "Car_Stats2purple",
    "carbuy", "carsel", "character", "characterPostRace", "CMSAdvice", "CMSAtMeeting", "CMSBet", "CMSCalendar",
    "CMSCalendarHelp", "CMSCarCollection2", "CMSCarDealership", "CMSChallengeResults", "CMSCheats", "CMSChooseLogo",
    "CMSCrew", "CMSCrewRaceSetup", "CMSGridWalk", "CMSHostMeetingSetup", "CMSHostMeetingSetupRace",
    "CMSHostMeetingSummary", "CMSLoader", "CMSMainMenu", "CMSMeetingSummary", "CMSMobileChoose", "CMSMobilePhone",
    "CMSMobilePhoneContacts", "CMSMobilePhoneMenu", "CMSNewCont", "CMSOnlineCrewRaceSetup", "CMSOnlineGridWalk",
    "CMSOnlineRaceSetup",
    "CMSRaceResults", "CMSRaceSetup", "CMSRespect", "CMSRespectChanges", "CMSRespectHelp", "CMSRespectMsg",
    "CMSRespectOverview", "CMSStats", "CMSStatsCarCollection", "CMSStatsMenu", "DemoFlyby", "keyboard",
    "link_lobby", "link_search", "menu", "multi", "online_arcade_create", "online_arcade_results",
    "online_arcade_search", "online_bg", "online_career_create", "online_career_results", "online_career_search",
    "online_connect_gs", "online_create_gs", "online_friend_options", "online_friends", "online_ingame_bg",
    "online_logon_gs", "online_main", "online_mode", "online_player_options", "online_players", "online_voice",
    "opts_audio", "opts_bg", "opts_game_pc", "opts_graphics", "opts_ingame_audio", "opts_ingame_bg",
    "opts_ingame_game", "opts_ingame_game_ply2", "opts_ingame_menu", "opts_menu", "opts_voice", "opts_wheel_dx9",
    "split_selcar", "unregistered:CMSMainMenu", "unregistered:demomenu", "unregistered:repair", "unregistered:0x0072212C",
    "unregistered:0x00722138", "unregistered:0x007225D8"};

// The key of a state's record, in lower case: the names registered with one record share it (careerload and
// garageload 0x0071FC60, online_bg and online_ingame_bg 0x00721820, online_arcade_results and online_career_results
// 0x007219DC, opts_bg and opts_ingame_bg 0x00721A6C, opts_menu and opts_ingame_menu 0x00721A78, opts_audio and
// opts_ingame_audio 0x00721A90, opts_game_pc, opts_ingame_game and opts_ingame_game_ply2 0x00721B00).
static std::string recordKey(const std::string& state) {
    static constexpr std::pair<const char*, const char*> kShared[] = {
        {"careerload", "garageload"},       {"online_ingame_bg", "online_bg"},
        {"online_career_results", "online_arcade_results"}, {"opts_ingame_bg", "opts_bg"},
        {"opts_ingame_menu", "opts_menu"},    {"opts_ingame_audio", "opts_audio"},
        {"opts_ingame_game", "opts_game_pc"}, {"opts_ingame_game_ply2", "opts_game_pc"}};
    std::string key = state;
    for (const auto& [name, record] : kShared) {
        if (_stricmp(state.c_str(), name) == 0) key = record;
    }
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return key;
}

}  // namespace

void Frontend::statsCar(Car* car, int32_t garage) {
    g_statsCar = car;
    m_carStatsPurple.setGarage(garage);
}

uint32_t Frontend::settingsValue(save::Settings::Field field) {
    return driverSettings() ? driverSettings()->u32(field) : save::Settings::defaults(m_language.index).u32(field);
}

bool Frontend::baseEntered(const std::string& state) {
    return listed(state, kBaseEntered);
}

bool Frontend::keepsMenu(const std::string& state) {
    return listed(state, kKeepsMenu);
}

// 0x005A5F20, the windows' enter: a Menu element (a menu, type 6) back to its items (0x005AECF0); the record's first
// enter (its +0x08 set) clears +0x08 and leaves the highlight as it is (its help to the ticker, 0x005AF860), a later
// one highlights the item the leave kept (+0x04) at once (0x005AF8A0); every ticker started again (0x005B52C0).
void Frontend::baseEnter(ui::Screen& s, const std::string& state) {
    if (s.isMenu("Menu")) {
        s.restoreMenu("Menu");
        const std::string key = recordKey(state);
        if (s_entered.insert(key).second) {
            s.refreshMenuHelp("Menu");
        } else {
            const auto kept = s_keptMenu.find(key);
            s.selectMenuItem("Menu", kept != s_keptMenu.end() ? kept->second : 0);
        }
    }
    s.restartTickers();
}

// 0x004C7F70, the windows' leave: a Menu element's (a menu's) highlighted item kept by the record (its +0x04).
void Frontend::baseLeave(ui::Screen& s, const std::string& state) {
    if (s.isMenu("Menu")) s_keptMenu[recordKey(state)] = s.selectedMenuItem("Menu");
}

// The driver's settings the options change (the save system's +0x54); a new driver's when none is loaded.
save::Settings& Frontend::optionSettings() {
    if (!driverSettings()) driverSettings() = save::Settings::defaults(m_language.index);
    return *driverSettings();
}

void Frontend::setDetailFactor(float lodBias) {
    if (m_scene != nullptr) m_scene->renderer().lodBias = lodBias;
}

void Frontend::restorePlaylistChoice() {
    m_playlist.restoreChoice();
}

}  // namespace oj
