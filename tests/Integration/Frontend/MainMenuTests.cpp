// Integration tests: the main menu (menu), the game's menu.ui and texts through a front end of the tests
// (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Game/Frontend/Menus/MainMenu.h"

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

// SELECT on an item goes to its window (mode 1) after CLICK: the arcade's championships, the custom race, the
// options and the extras.
OJ_INTEGRATION_TEST(MainMenu, SelectGoesToItsWindow) {
    MainMenuWindow w;
    if (!w.ok()) return;
    w.enter();
    const std::pair<const char*, const char*> kItems[] = {
        {"instant", "Arcade_Championship"}, {"custom", "instant_manu"}, {"options", "opts_bg"}, {"Extras", "Extras"}};
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
