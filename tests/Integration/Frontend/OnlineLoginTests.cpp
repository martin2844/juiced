// Integration tests: the fixed build's login of online_connect_gs (a frame at a time, its notes, the account kept
// once the login is good) against a stand-in of the connection manager, the game's window and texts through a front
// end of the tests (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeGpcm.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Network/NetTransport.h"
#include "Core/Saves/OnlineAccount.h"
#include "Game/Frontend/Network/LinkWindows.h"
#include "Game/Frontend/Network/LobbyChat.h"
#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/Frontend/Network/OnlineVoice.h"
#include "Game/Frontend/Network/OnlineWindows.h"
#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/UI/WindowFixes.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace {

using oj::test::check;
using oj::test::FakeGpcm;

// The online windows' front end with no session.
class FakeOnlineHost : public oj::test::FakeWindowHost, public oj::frontend::OnlineHost {
public:
    bool netWait(oj::net::SessionStage, bool) override { return false; }
    bool netKeys(uint32_t, oj::net::SessionStage, bool) override { return false; }
    void netCountdown() override {}
    bool onlineCareer() const override { return false; }
    void onlineCareerRaceSetup() override {}
    void netHook() override {}
    bool netDropped() override { return false; }
    bool onlineTopMoving() const override { return false; }
    bool onlineJoinSession(int32_t) override { return false; }
    void delayGridWalkTimeout() override {}
    bool racePause() const override { return false; }
    void netFollow() override {}
    void onlineHostIntoLobby() override {}
    bool nameRefused(const std::wstring&) const override { return false; }
    std::optional<oj::save::Profile>& profile() override { return m_profile; }
    void setUpDriverName(const std::wstring&) override {}
    oj::frontend::KeyboardSetup& keyboardSetup() override { return m_keyboard; }
    oj::frontend::OnlineVoice& onlineVoice() override { return m_voice; }

private:
    std::optional<oj::save::Profile> m_profile;
    oj::frontend::KeyboardSetup m_keyboard;
    oj::frontend::OnlineVoice m_voice;
};

struct Login {
    bool loggedOn = false;      // online_main asked
    bool noted = false;         // a note after "Logging into account..."
    std::wstring note;
    double longestFrame = 0.0;  // ms
};

// The window's frames with the game's loop's user frame after each (as 0x0040CF48), the account of the command line
// (stored at once), until online_main or a note; at most ~5 s.
Login runLogin(FakeOnlineHost& host, oj::ui::Screen& s, oj::frontend::OnlineConnect& window, FakeGpcm& server,
               const std::filesystem::path& info) {
    Login out;
    window.enter(host, s);
    oj::net::OnlineUser* user = oj::net::onlineUser();
    if (user == nullptr) return out;
    user->server = {"127.0.0.1", server.port(), info.string()};
    oj::frontend::OnlineForms& f = host.shared().onlineForms;
    f.commandLine = 1;
    const size_t notesBefore = host.messages.size();
    for (int i = 0; i < 5000 && !out.loggedOn && !out.noted; ++i) {
        const auto start = std::chrono::steady_clock::now();
        window.update(host, s, 0);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        out.longestFrame = std::max(out.longestFrame, ms);
        for (const auto& r : host.requests) out.loggedOn = out.loggedOn || r.first == "online_main";
        if (host.messages.size() > notesBefore + 1) {
            out.noted = true;
            out.note = host.messages.back().text;
        }
        if (oj::net::OnlineUser* u = oj::net::onlineUser()) u->update(0.001f);
        Sleep(1);
    }
    return out;
}

}  // namespace

// A wrong password: its note, signed out, the account kept before untouched; then the right one: online_main, the
// account kept. No frame of the window waits for the server (the stock's loop held the frame with Sleep(50)).
OJ_INTEGRATION_TEST(OnlineLogin, FrameAtATimeAccountKeptWhenGood) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeOnlineHost host;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("online_connect_gs");
    if (screen == nullptr) return;
    // The services up first (Winsock started anew: the sockets made before would be gone), then the stand-ins.
    oj::frontend::onlineBringUp(host);
    const std::filesystem::path info = std::filesystem::temp_directory_path() / "OpenJuice.Tests.login.gp.info";
    std::filesystem::remove(info);
    FakeGpcm wrong;
    wrong.password = "wrong";
    wrong.nicks[1234] = "Tester";
    FakeGpcm right;
    right.nicks[1234] = "Tester";
    if (!wrong.start() || !right.start()) {
        check(false, "the stand-ins started");
        oj::net::endOnline();
        return;
    }
    oj::save::Settings& set = host.optionSettings();
    oj::save::OnlineAccount{L"Kept", L"kept@example.com", L"kept"}.into(set);
    oj::frontend::OnlineForms& f = host.shared().onlineForms;
    f.nick = L"Tester";
    f.email = L"tester@example.com";
    f.password = L"secret";

    oj::frontend::OnlineConnect first;
    const Login failed = runLogin(host, *screen, first, wrong, info);
    const oj::save::OnlineAccount afterFail = oj::save::OnlineAccount::of(set);
    const bool failOk = !failed.loggedOn && failed.noted && failed.note == host.gameTexts().at(oj::kTextBadPassword) &&
                        afterFail.nick == L"Kept" && afterFail.password == L"kept" &&
                        !oj::net::gpConnection();
    host.closeMessage();
    host.requests.clear();

    oj::frontend::OnlineConnect second;
    const Login good = runLogin(host, *screen, second, right, info);
    const oj::save::OnlineAccount afterGood = oj::save::OnlineAccount::of(set);
    const bool goodOk = good.loggedOn && !good.noted && afterGood.nick == L"Tester" &&
                        afterGood.email == L"tester@example.com" && afterGood.password == L"secret";

    oj::net::endOnline();
    wrong.stop();
    right.stop();
    std::filesystem::remove(info);
    check(failOk, "online login: a wrong password's note, the account kept before untouched");
    check(goodOk, "online login: logged on, the account kept");
    // The frames hold no loop (the stock's held the frame through the login: 570 .. 590 ms here); the available
    // check's name resolved in the frame of the login's start (as the stock's) may take tens of ms.
    check(failed.longestFrame < 300.0 && good.longestFrame < 300.0,
          "online login: no frame waits for the server (" + std::to_string(failed.longestFrame) + ", " +
              std::to_string(good.longestFrame) + " ms)");
}

// The fixed build's guest: online_logon_gs's third item (once, the window kept), logged on under the driver's name
// without a connection (no presence: the state stays none past the 60 s; no statistics), online_main without the
// friends and the scoreboards; signed out, an account's menu whole again.
OJ_INTEGRATION_TEST(OnlineLogin, Guest) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeOnlineHost host;
    const std::shared_ptr<oj::ui::Screen> logon = host.open("online_logon_gs");
    const std::shared_ptr<oj::ui::Screen> main = host.open("online_main");
    if (logon == nullptr || main == nullptr) return;
    oj::frontend::OnlineLogon window;
    window.enter(host, *logon);
    window.enter(host, *logon);
    const bool item = logon->menuSize("MENU") == 3 && logon->menuItemCommand("MENU", 2) == "guest" &&
                      logon->menuSize("BACKMENU") == 3;
    logon->selectMenuItem("MENU", 2);
    oj::net::availableResult() = oj::net::AvailableCheck::Waiting;
    window.update(host, *logon, oj::input::kSelect);
    // "Logging into account..." while the available check answers (the game's loop runs the user object's frame),
    // then online_main.
    const bool noted = !host.messages.empty() && host.messages.back().text == host.gameTexts().at(oj::kTextLoggingIn);
    bool asked = false;
    for (int i = 0; i < 20000 && !asked; ++i) {
        window.update(host, *logon, 0);
        if (oj::net::OnlineUser* waiting = oj::net::onlineUser()) waiting->update(0.001f);
        for (const auto& r : host.requests) asked = asked || r.first == "online_main";
        Sleep(1);
    }
    // The available check ran (the server browser is made only after its "available").
    asked = asked && noted && oj::net::availableResult() == oj::net::AvailableCheck::Available;
    oj::net::OnlineUser* u = oj::net::onlineUser();
    bool guest = u != nullptr && u->guest() && u->loggedOn == 1 && u->localPlayers.size() == 1 &&
                 u->playerNick(u->localPlayers[0]) == u"Guest" && !oj::net::gpConnection();
    if (guest) {
        const int32_t own = u->playerUser(u->localPlayers[0]);
        guest = u->record(own).profile < -1;
        u->userRelease(own);
        u->update(61.0f);
        oj::net::stats::frame(0.1f, true);
        guest = guest && u->state == oj::net::UserState::None && oj::net::stats::gameStatistics() == nullptr;
    }
    oj::frontend::OnlineMain online;
    online.enter(host, *main);
    bool menu = main->menuSize("MENU") == 4 && main->menuSize("BACKMENU") == 4;
    for (size_t i = 0; i < main->menuSize("MENU"); ++i) {
        menu = menu && main->menuItemCommand("MENU", i) != "friends" && main->menuItemCommand("MENU", i) != "rankings";
    }
    // The lobby of a guest (the matchmaking there): FRIENDS and PLAYERS' hints hidden (F1, F2 do nothing).
    bool lobbyHints = false;
    if (const std::shared_ptr<oj::ui::Screen> lobby = host.open("link_lobby")) {
        oj::frontend::LinkLobby lobbyWindow;
        lobbyWindow.enter(host, *lobby);
        lobbyHints = oj::net::matchmaking() != nullptr && lobby->hidden("FRIENDS_BUT") && lobby->hidden("FRIENDS_TEXT") &&
                     lobby->hidden("PLAYERS_BUT") && lobby->hidden("PLAYERS_TEXT");
    }
    menu = menu && lobbyHints;
    // Signed out: an account's online_main whole again.
    if (u != nullptr) u->signOut();
    online.enter(host, *main);
    const bool whole = main->menuSize("MENU") == 6 && main->menuSize("BACKMENU") == 6;
    oj::net::endOnline();
    oj::net::endNetGame();
    oj::net::releaseMatchmaking();
    check(item, "online guest: online_logon_gs's third item, once");
    check(asked && guest, "online guest: logged on under the driver's name, no connection, presence or statistics");
    check(menu && whole, "online guest: online_main without the friends and the scoreboards, the lobby without their hints; an account's whole");
}

// The fixed build's lobby chat's windows: keyboard with CHAT_LINE1 .. 6, link_lobby with CHAT_BUT and CHAT_TEXT; the
// keyboard in the chat's mode its top frame hidden and the lines shown, out of it the other way round; link_lobby's
// hint hidden without the chat.
OJ_INTEGRATION_TEST(LobbyChat, Windows) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeOnlineHost host;
    const std::shared_ptr<oj::ui::Screen> keys = host.open("keyboard");
    const std::shared_ptr<oj::ui::Screen> lobby = host.open("link_lobby");
    if (keys == nullptr || lobby == nullptr) return;
    bool made = lobby->find(oj::ui::kChatButton) >= 0 && lobby->find(oj::ui::kChatText) >= 0;
    // The hint between PLAYERS and OK, past PLAYERS' text (the bar's hints moved left for it).
    float px = 0, py = 0, pw = 0, ph = 0, cx = 0, cy = 0, cw = 0, ch = 0, ox = 0, oy = 0, ow = 0, oh = 0;
    lobby->placeOf(lobby->find("PLAYERS_TEXT"), px, py, pw, ph);
    lobby->placeOf(lobby->find(oj::ui::kChatButton), cx, cy, cw, ch);
    lobby->placeOf(lobby->find("SELECTA"), ox, oy, ow, oh);
    made = made && px + 0.12f < cx && cx + 0.05f + 0.115f <= ox;
    for (int i = 0; i < oj::ui::kChatLines; ++i) made = made && keys->find(oj::ui::chatLineName(i)) >= 0;
    oj::frontend::lobbyChatLayout(*keys, true);
    const bool chatMode = keys->hidden("INSTRUCTION") && keys->hidden("FRAME_TRIBAL01") &&
                          !keys->hidden(oj::ui::chatLineName(0)) && !keys->hidden(oj::ui::chatLineName(5));
    oj::frontend::lobbyChatLayout(*keys, false);
    const bool plain = !keys->hidden("INSTRUCTION") && !keys->hidden("FRAME_TRIBAL01") &&
                       keys->hidden(oj::ui::chatLineName(0));
    oj::frontend::lobbyChatHint(*lobby);
    const bool noHint = lobby->hidden(oj::ui::kChatButton) && lobby->hidden(oj::ui::kChatText);
    check(made && chatMode && plain && noHint,
          "lobby chat: the windows' elements, the keyboard's chat mode and back, no hint without the chat");
}
