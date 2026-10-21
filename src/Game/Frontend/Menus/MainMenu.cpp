// The main menu (menu).

#include "Game/Frontend/Menus/MainMenu.h"

#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// menu's enter (0x00468490): the race's players 1, the profile's race cars chosen (+0x614) 1, +0x0C 0, Menu and
// BackMenu back to their items (0x005AECF0), the driver's name into +0x10 and PLAYERNAME (menu.ui has none), every
// element named GAMESPY hidden, the online status (0x00473CD0), VERSION emptied (none either), +0x28, +0x2C, +0x30
// -1; the command line's -host ([0x0075ED50]): its account into the edit buffers to log in at once (0x004729D0),
// online_main to go on to online_mode ([0x0075EE9C]), online_connect_gs; its -connect a.b.c.d:port ([0x0075ED54],
// five numbers read): the same with the address to join ([0x00715D28], [0x0075EEA0]; 0x00473E60); else +0x34 9999999.
void MainMenu::enter(MainMenuHost& host, ui::Screen& s) {
    if (GameData* data = host.gameData(); data != nullptr) {
        data->race.players = 1;
        if (data->career) data->career->profile.raceCarsChosen = 1;
    }
    s.restoreMenu("Menu");
    s.restoreMenu("BackMenu");
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), "GAMESPY") == 0) s.setHidden(e, true);
    }
    showOnlineStatus(s, host.gameTexts());
    m_career = 0;
    m_backAnswer = -1;
    OnlineCommandLine& c = host.shared().onlineCommandLine;
    OnlineForms& forms = host.shared().onlineForms;
    // 0x004729D0: the name (30 characters), the email (50) and the password (30) into the edit buffers.
    auto account = [&c, &forms] {
        forms.commandLine = 1;
        forms.nick = c.name.substr(0, 30);
        forms.email = c.email.substr(0, 50);
        forms.password = c.password.substr(0, 30);
    };
    if (c.host) {
        c.host = false;
        account();
        forms.host = 1;
        host.request("online_connect_gs", SwitchMode::Replace);
        return;
    }
    if (c.connect) {
        c.connect = false;
        int a = 0, b = 0, d = 0, e = 0, port = 0;
        if (sscanf_s(c.address.c_str(), "%d.%d.%d.%d:%d", &a, &b, &d, &e, &port) == 5) {
            account();
            net::Address address;
            address.ip = static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
                         (static_cast<uint32_t>(d) << 16) | (static_cast<uint32_t>(e) << 24);
            address.port = static_cast<uint16_t>(((port & 0xFF) << 8) | ((port >> 8) & 0xFF));
            forms.connectAddress = address;
            forms.connect = 1;
            host.request("online_connect_gs", SwitchMode::Replace);
        }
    }
}

// menu's leave (0x00468790): Menu's highlighted item kept (a menu element, +0x04); with +0x0C (CMSHub went to
// DemoFlyby) the career is made again and its first race set up as DemoFlyby's end does it (0x004A2300, the bets
// with crew 1 0x004B1870, the profile's racers, 0x004B8570), so a new career is made twice on that way.
void MainMenu::leave(MainMenuHost& host) {
    GameData* data = host.gameData();
    if (m_career == 0 || data == nullptr || !data->career) return;
    const CareerScreen screen = data->careerScreen;
    data->enterCareer(gameRandom(), crewName8(data->profileName));
    data->careerScreen = screen;
}

// The main menu (0x00468850): first the pink slip's note (0x004A4EE0). BACK's question answered yes (+0x28 1):
// online the network game ended (0x004E9DD0) and the online objects let go, the music (0x0040D200, made once:
// already made), start (mode 1) and the records asked for again ([0x0075EE68] 0). Else the online status (0x00473CD0); "Quit to
// Windows" asked (+0x20) and answered yes (+0x24 1) ends the game ([game + 4]'s method +0x40), both cleared. SELECT
// (CLICK) goes to the state of the item (its name): CMSHub the career (DemoFlyby while the career has not started, else
// CMSNewCont), instant the arcade championships, custom the custom race, multi the link search, online the online log
// on, options, Extras; quit asks "Quit to Windows" / "ARE YOU SURE?" (+0x24, starting on no) and sets +0x20. BACK
// (CLICK) with no message up asks line 1528 (+0x28, starting on no), else closes the message (the front end's +0x48).
// BACKMENU follows MENU.
void MainMenu::update(MainMenuHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    pinkSlipNote(host);
    if (m_backAnswer == 1) {
        // 0x00468889: online, the network game ended (with the matchmaking only) and the online objects let go.
        if (net::onlineUser() != nullptr) {
            if (net::matchmaking() != nullptr) net::endNetGame();
            net::endOnline();
        }
        host.request("start", SwitchMode::Replace);
        if (data) data->recordsAsked = 0;
        return;
    }
    showOnlineStatus(s, texts);
    if (m_quitAsked != 0) {
        if (m_quitAnswer == 1) host.quitGame();
        m_quitAsked = 0;
        m_quitAnswer = 0;
    }
    // 0x004689C0: the records not read: loadhighscores over the menu (mode 5), asked once ([0x0075EE68]).
    if (data != nullptr && data->recordsAsked == 0 && data->highScores.loaded == 0) {
        host.request("loadhighscores", SwitchMode::Push);
        data->recordsAsked = 1;
    }
    const size_t selected = s.selectedMenuItem("MENU");
    s.selectMenuItem("BACKMENU", selected);
    if (event == input::kBack) {
        host.sound("CLICK");
        if (!host.messageUp()) {
            host.message(MessageType::YesNo, texts.at(kTextQuitToTitle), false, &m_backAnswer, true);
        } else {
            host.closeMessage();
        }
        return;
    }
    if (event != input::kSelect) {
        return;
    }
    host.sound("CLICK");
    const std::string item = s.menuItemCommand("MENU", selected);
    static const std::pair<const char*, const char*> kNext[] = {
        {"career", "careerload"},  {"instant", "Arcade_Championship"}, {"custom", "instant_manu"},
        {"multi", "link_search"},  {"Extras", "Extras"},
        {"options", "opts_bg"},
    };
    for (const auto& [name, next] : kNext) {
        if (_stricmp(item.c_str(), name) == 0) {
            // 0x00468AD4, 0x00468B39: the arcade championships' race mode, the custom race's, one player.
            if (data != nullptr && (_stricmp(name, "instant") == 0 || _stricmp(name, "custom") == 0)) {
                data->race.mode = _stricmp(name, "custom") == 0 ? RaceMode::Custom : RaceMode::Arcade;
                data->race.players = 1;
            }
            // 0x00468B9D: multi starts Winsock (0x00597690; off line, [0x0075F040] 0) or says "An error occurred
            // starting the network adaptor. .." (513) and stays; else the race a custom one of type 5 and CLICK a
            // second time.
            if (_stricmp(name, "multi") == 0) {
                if (!net::startWinsock()) {
                    host.message(MessageType::Continue, texts.at(kTextNoNetwork), false, nullptr);
                    return;
                }
                if (data != nullptr) {
                    data->race.mode = RaceMode::Custom;
                    data->race.type = RaceType::Circuit;
                }
                host.sound("CLICK");
            }
            host.request(next, SwitchMode::Replace);
            return;
        }
    }
    if (_stricmp(item.c_str(), "online") == 0) {
        // 0x00468C5B: logged on, online_main; else online_logon_gs.
        const net::OnlineUser* u = net::onlineUser();
        host.request(u != nullptr && u->loggedOn != 0 ? "online_main" : "online_logon_gs", SwitchMode::Replace);
        return;
    }
    if (_stricmp(item.c_str(), "CMSHub") == 0) {
        // 0x00468E77: the race's mode 2 (the career); a career that has not started yet ([[game + 0x7C]] 0) shows its
        // flyby first.
        if (data != nullptr) data->race.mode = RaceMode::Career;
        host.sound("CLICK");  // 0x00468E9E: a second time
        const bool started = data != nullptr && data->career->head != 0;
        host.request(started ? "CMSNewCont" : "DemoFlyby", SwitchMode::Replace);
        if (!started) m_career = 1;  // 0x00468EE9
    } else if (_stricmp(item.c_str(), "quit") == 0) {
        host.message(MessageType::YesNo, texts.at(kTextQuitToWindows) + L"\n" + texts.at(kTextAreYouSure), false,
                     &m_quitAnswer, true);
        m_quitAsked = 1;
    }
}

}  // namespace oj::frontend
