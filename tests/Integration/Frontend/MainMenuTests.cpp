// Integration tests: the main menu (menu), the game's menu.ui and texts through a front end of the tests
// (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"
#include "Framework/Fakes/FakeCustomRaceHost.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/NetTransport.h"
#include "Game/Frontend/Menus/MainMenu.h"
#include "Game/Frontend/Network/LinkWindows.h"

#include <memory>
#include <string>
#include <utility>

namespace {

using oj::test::check;

class FakeMainMenuHost : public oj::test::FakeWindowHost, public oj::frontend::MainMenuHost {
public:
    int quits = 0;
    void quitGame() override { ++quits; }
};

// The main menu over the game's menu.ui with the English texts. Not ok() without the game.
struct MainMenuWindow {
    oj::test::InGameFolder folder;
    FakeMainMenuHost host;
    std::shared_ptr<oj::ui::Screen> screen;
    oj::frontend::MainMenu menu;

    MainMenuWindow() {
        if (folder.valid()) screen = host.open("menu");
    }
    bool ok() const { return screen != nullptr; }
    void enter() { menu.enter(host, *screen); }
    void update(uint32_t event) { menu.update(host, *screen, event); }

    // MENU's item of that name highlighted; false when the menu has none.
    bool highlight(const char* item) {
        for (size_t i = 0; i < screen->menuSize("MENU"); ++i) {
            if (_stricmp(screen->menuItemCommand("MENU", i).c_str(), item) == 0) {
                screen->selectMenuItem("MENU", i);
                return true;
            }
        }
        return false;
    }
    // The switches and the sounds asked so far forgotten, the switch made.
    void clear() {
        host.requests.clear();
        host.sounds.clear();
        host.switchPending = false;
    }
};

bool asked(const FakeMainMenuHost& host, const char* state, oj::frontend::SwitchMode mode) {
    return host.requests.size() == 1 && host.requests[0].first == state && host.requests[0].second == mode;
}

}  // namespace

#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
OJ_INTEGRATION_TEST(MainMenu, MultiplayDoesNotRequireNetworkStartup) {
    MainMenuWindow w;
    if (!w.ok()) return;
    oj::net::stopWinsock();
    w.enter();
    const bool found = w.highlight("multi");
    w.update(oj::input::kSelect);
    check(found && asked(w.host, "multi", oj::frontend::SwitchMode::Replace) &&
              !oj::net::winsockStarted(),
          "menu: local/LAN selector opens without starting the network");
}
#endif

OJ_INTEGRATION_TEST(Multi, LocalGateLanAndBack) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    struct Host : oj::test::FakeCustomRaceHost {
        size_t devices = 1;
        int releases = 0;
        size_t inputDevices() const override { return devices; }
        void askSecondPlayerRelease() override { ++releases; }
    } host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    auto s = host.open("multi");
    if (!s) return;
    oj::frontend::Multi window;
    oj::net::stopWinsock();
    window.enter(host, *s);
    window.update(host, *s, 0);
    auto select = [&](const char* command) {
        for (size_t i = 0; i < s->menuSize("MENU"); ++i) {
            if (s->menuItemCommand("MENU", i) == command) {
                s->selectMenuItem("MENU", i);
                return true;
            }
        }
        return false;
    };
    select("split");
    const bool disabled = !s->menuItemEnabled("MENU", s->selectedMenuItem("MENU"));
    check(disabled && host.releases == 1,
          "multiplay: split screen requires a second input device; stale player-two binding released");
    host.devices = 2;
    window.update(host, *s, 0);
    const bool split = select("split");
    window.update(host, *s, oj::input::kSelect);
    check(split && host.requests.size() == 1 && host.requests.back().first == "instant_manu" &&
              data.race.mode == oj::RaceMode::SplitScreen && data.race.players == 2 &&
              data.race.type == oj::RaceType::Circuit && !oj::net::winsockStarted(),
          "multiplay: two-device local choice opens split setup without networking");
    host.requests.clear();
    const bool lan = select("link");
    window.update(host, *s, oj::input::kSelect);
    check(lan && host.requests.size() == 1 && host.requests.back().first == "link_search" &&
              data.race.mode == oj::RaceMode::Custom && oj::net::winsockStarted(),
          "multiplay: LAN still starts networking and opens LAN search");
    host.requests.clear();
    window.update(host, *s, oj::input::kBack);
    check(host.requests.size() == 1 && host.requests.back().first == "menu",
          "multiplay: back returns to main menu");
    oj::net::stopWinsock();
}

// SELECT on an item goes to its window (mode 1) after CLICK: the arcade's championships, the custom race, the
// options and the extras.
OJ_INTEGRATION_TEST(MainMenu, SelectGoesToItsWindow) {
    MainMenuWindow w;
    if (!w.ok()) return;
    w.enter();
    const std::pair<const char*, const char*> kItems[] = {
        {"instant", "Arcade_Championship"}, {"custom", "instant_manu"}, {"options", "opts_bg"}, {"Extras", "Extras"},
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
        {"multi", "multi"},
#else
        {"multi", "link_search"},
#endif
    };
    for (const auto& [item, next] : kItems) {
        w.clear();
        const bool found = w.highlight(item);
        w.update(oj::input::kSelect);
        check(found && asked(w.host, next, oj::frontend::SwitchMode::Replace) && !w.host.sounds.empty() &&
                  w.host.sounds[0] == "CLICK",
              std::string("menu: ") + item + " -> " + next);
    }
}

// BACK asks line 1528 (yes / no, starting on no); a yes goes back to start. BACK with the message up closes it.
OJ_INTEGRATION_TEST(MainMenu, BackAsksThenGoesToStart) {
    MainMenuWindow w;
    if (!w.ok()) return;
    w.enter();
    w.update(oj::input::kBack);
    const bool question = w.host.messages.size() == 1 && w.host.messages[0].type == oj::ui::MessageType::YesNo &&
                          w.host.messages[0].startNo && w.host.messages[0].answer != nullptr &&
                          w.host.messages[0].text == w.host.texts.at(oj::kTextQuitToTitle) &&
                          !w.host.messages[0].text.empty() && w.host.requests.empty();
    check(question, "menu: BACK asks to quit to the title");

    w.update(oj::input::kBack);
    check(w.host.closedMessages == 1 && w.host.messages.size() == 1, "menu: BACK with the question up closes it");

    w.host.messageOpen = false;
    *w.host.messages[0].answer = 1;
    w.update(0);
    check(asked(w.host, "start", oj::frontend::SwitchMode::Replace), "menu: yes goes to start");
}

// quit asks "Quit to Windows" / "ARE YOU SURE?"; no leaves the game running, yes ends it.
OJ_INTEGRATION_TEST(MainMenu, QuitAsksThenQuits) {
    MainMenuWindow w;
    if (!w.ok()) return;
    w.enter();
    const bool found = w.highlight("quit");
    w.update(oj::input::kSelect);
    const std::wstring text =
        w.host.texts.at(oj::kTextQuitToWindows) + L"\n" + w.host.texts.at(oj::kTextAreYouSure);
    check(found && w.host.messages.size() == 1 && w.host.messages[0].type == oj::ui::MessageType::YesNo &&
              w.host.messages[0].startNo && w.host.messages[0].text == text,
          "menu: quit asks");

    w.host.messageOpen = false;
    w.update(0);
    check(w.host.quits == 0, "menu: no keeps the game");

    w.update(oj::input::kSelect);
    w.host.messageOpen = false;
    *w.host.messages.back().answer = 1;
    w.update(0);
    check(w.host.quits == 1, "menu: yes quits");
}

// -host: the account into the online forms (the name cut to 30 characters) and online_connect_gs at once.
OJ_INTEGRATION_TEST(MainMenu, CommandLineHost) {
    MainMenuWindow w;
    if (!w.ok()) return;
    oj::frontend::OnlineCommandLine& c = w.host.sharedState.onlineCommandLine;
    c.host = true;
    c.name = std::wstring(40, L'n');
    c.email = L"a@b.c";
    c.password = L"secret";
    w.enter();
    const oj::frontend::OnlineForms& forms = w.host.sharedState.onlineForms;
    check(asked(w.host, "online_connect_gs", oj::frontend::SwitchMode::Replace) && !c.host && forms.host == 1 &&
              forms.commandLine == 1 && forms.nick == std::wstring(30, L'n') && forms.email == L"a@b.c" &&
              forms.password == L"secret",
          "menu: -host logs on at once");
}

// -connect a.b.c.d:port: the address to join (the port in network order) and online_connect_gs; a malformed
// address stays in the menu.
OJ_INTEGRATION_TEST(MainMenu, CommandLineConnect) {
    {
        MainMenuWindow w;
        if (!w.ok()) return;
        oj::frontend::OnlineCommandLine& c = w.host.sharedState.onlineCommandLine;
        c.connect = true;
        c.address = "10.1.2.3:3658";
        w.enter();
        const oj::frontend::OnlineForms& forms = w.host.sharedState.onlineForms;
        const bool address = forms.connectAddress && forms.connectAddress->ip == 0x0302010Au &&
                             forms.connectAddress->port == 0x4A0E;
        check(asked(w.host, "online_connect_gs", oj::frontend::SwitchMode::Replace) && forms.connect == 1 && address &&
                  !c.connect,
              "menu: -connect joins the address");
    }
    MainMenuWindow w;
    if (!w.ok()) return;
    oj::frontend::OnlineCommandLine& c = w.host.sharedState.onlineCommandLine;
    c.connect = true;
    c.address = "10.1.2:3658";
    w.enter();
    check(w.host.requests.empty() && w.host.sharedState.onlineForms.connect == 0 && !c.connect,
          "menu: a malformed -connect stays");
}
