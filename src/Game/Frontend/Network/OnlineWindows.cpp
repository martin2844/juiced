// The online windows of GameSpy Presence (docs/spec/28-network.md, "GameSpy Presence"): online_logon_gs (the account
// kept or a new one), online_create_gs (a new profile), online_connect_gs (the login), online_main (the online menu)
// and the online services brought up for them (0x00471920). The matchmaking's windows (online_mode and on) are
// another part of the port; online_main reaches them through its hooks.

#include "Game/Frontend/Network/OnlineWindows.h"

#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/VoiceChat.h"
#include "Core/Data/JetTexts.h"
#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <windows.h>

namespace oj::frontend {

namespace {


// [0x00715C60]: the characters of a nick (a space among them); [0x00715CC0]: of an email or a password.
constexpr char kNickCharacters[] =
    " \"#$%&'()*+-./:;<=>?@[]^_`{|}~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
constexpr char kAccountCharacters[] =
    "\"#$%&'()*+-./:;<=>?@[]^_`{|}~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
constexpr char kNotFirst[] = "@+:#";  // [0x00715D1C]

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// The first element of that name (case aside, the list in order) given the text (0x005B27B0, its FontModifier
// record started again).
void setFirst(ui::Screen& s, const char* name, const std::wstring& text) {
    for (int e = 0; e < s.size(); ++e) {
        if (same(s.nameOf(e), name)) {
            s.setText(e, text);
            return;
        }
    }
}

std::u16string narrowWide(const std::wstring& text) {
    return std::u16string(text.begin(), text.end());
}

// The note of a failed login, both windows: the states 9 .. 13 and 15 their own, any other but none "Failed to
// connect". The stock's create window (0x0047273C) had none for 1 .. 8, its connect window (0x00473160 by 0x00473180)
// none for MemoryError, ServerError, Unavailable, ForcedDisconnect and ConnectionClosed: the login waited for ever.
size_t loginText(net::UserState state) {
    switch (state) {
    case net::UserState::None: return 0;
    case net::UserState::BadNick: return kTextBadNick;
    case net::UserState::BadEmail: return kTextBadEmail;
    case net::UserState::BadPassword: return kTextBadPassword;
    case net::UserState::BadProfile: return kTextUnknownProfile;
    case net::UserState::ProfileDeleted: return kTextProfileDeleted;
    case net::UserState::AuthFailed: return kTextNotAuthenticated;
    default: return kTextFailedToConnect;
    }
}

// The fixed build's guest's item of online_logon_gs (its own text, in every language) and the online_main items a
// guest goes without (the friends' windows and the scoreboards need a GameSpy profile).
constexpr char kGuest[] = "guest";
constexpr wchar_t kGuestText[] = L"PLAY AS GUEST";
constexpr std::array<const char*, 2> kAccountOnly = {"friends", "rankings"};

bool hasItem(ui::Screen& s, const char* menu, const char* command) {
    for (size_t i = 0; i < s.menuSize(menu); ++i) {
        if (same(s.menuItemCommand(menu, i), command)) return true;
    }
    return false;
}

// The fixed build's time for a login: then "Failed to connect" (the stock waited for ever on a server that did not
// answer; the available check gives up after 18 s, a connect that is not answered fails after about 21).
constexpr float kLoginTime = 60.0f;

// 0x004EE990, the user object's logged-on hook: each local player online as the settings' +0x44 says (its presence
// within a second); the settings' +0x48 0 when the first player's device has a headset (the input manager's methods
// +0x30 and +0x34: none of the PC's devices has one); the voice made when there is none (0x004EECA0, 0x00565360, its
// +0x1C 0x004EEB50); the friends made again (0x004E17C0).
void onlineLoggedOn(GameData* data) {
    if (net::OnlineUser* u = net::onlineUser()) {
        const uint32_t online = data != nullptr && data->settings ? data->settings->u32(save::Settings::kOnline) : 1;
        for (size_t i = 0; i < u->localPlayers.size(); ++i) {
            u->setOnline(u->localPlayers[i], static_cast<int32_t>(online));
        }
    }
    if (net::gameVoice() == nullptr) net::makeGameVoice(voiceChatHook(data));
    net::remakeOnlineFriends();
}

// online_main's enter: the nick of the online setups' settings (+0x21C, +0x268, at their +4): 16 characters at most
// and the 0 after them, then the 0 at +0x22 (Stock problem: a nick of 16 characters or more puts its 0 at +0x24, the
// playing flag's low half; the fixed build leaves it: the 0 at +0x22 ends the nick).
void setupNick(save::Settings& set, uint32_t offset, const std::u16string& nick) {
    size_t i = 0;
    for (; i < nick.size() && i < 16; ++i) {
        set.object[offset + 4 + i * 2] = static_cast<uint8_t>(nick[i] & 0xFF);
        set.object[offset + 4 + i * 2 + 1] = static_cast<uint8_t>(nick[i] >> 8);
    }
    if (i < 16) {
        set.object[offset + 4 + i * 2] = 0;
        set.object[offset + 4 + i * 2 + 1] = 0;
    }
    set.object[offset + 0x22] = 0;
    set.object[offset + 0x23] = 0;
}

}  // namespace

std::function<void(int32_t on)> voiceChatHook(GameData* data) {
    return [data](int32_t on) {
        if (data != nullptr && data->settings) data->settings->setU32(save::Settings::kVoiceChat, on == 0 ? 1u : 0u);
    };
}

// Not up yet: up, Winsock started (0x00597690) and the game's name "juicedpc", its secret key and its presence
// product 10362 set (0x0055C600; built a byte at a time on the stack). No user object of the game: both references
// let go (0x004EEA10), a new one made (0x0055E060) and referenced, its logged-on hook 0x004EE990.
void onlineBringUp(WindowHost& host) {
    GameData* gameData = host.gameData();
    if (!net::onlineUp()) {
        net::onlineUp() = true;
        net::startWinsock();
    }
    if (net::onlineUser() != nullptr) return;
    net::makeOnlineUser();
    if (net::OnlineUser* u = net::onlineUser()) {
        GameData* data = gameData;
        u->loggedOnHook = [data] { onlineLoggedOn(data); };
    }
}

// 0x00472940: the message up closed, the note of the text, the user signed out, the login's loop ended.
void onlineFailed(WindowHost& host, size_t text, int32_t& connecting) {
    const TextTable& texts = host.gameTexts();
    host.closeMessage();
    host.message(MessageType::Continue, texts.at(text), false, nullptr);
    if (net::OnlineUser* u = net::onlineUser()) u->signOut();
    connecting = 0;
}

// The keyboard set up for an account's field: its characters (0x00460140), its title (0x00460110), the buffer's text
// at most `most` and at least 1, DONE writing it back into the buffer, the name filter for a nick (0x004612B0), the
// stars of a password ([0x00720FA4]), none of "@+:#" first (0x00460180), the symbols' page ([0x007211A8]), BACK and
// DONE to the window again (0x00461320, 0x00461340); keyboard.
void onlineKeyboard(OnlineHost& host, size_t title, std::wstring& buffer, size_t most, bool checkName, const char* allowed, bool password, const char* window) {
    const TextTable& texts = host.gameTexts();
    frontend::KeyboardSetup& k = host.keyboardSetup();
    k.allowed = allowed;
    k.title = texts.at(title);
    k.text = buffer;
    k.minLength = 1;
    k.maxLength = most;
    k.small = most > 26;
    k.checkName = checkName;
    std::wstring* target = &buffer;
    k.accept = [target](const std::wstring& text) {
        *target = text;
        return true;
    };
    if (password) k.password = true;
    k.notFirst = kNotFirst;
    k.symbols = true;
    k.back = window;
    k.next = window;
    host.request("keyboard", SwitchMode::Replace);
}

// "Logging into account..." (type 0). The stock kept the edit buffers as the driver's account here, before the login
// was known to be good (a failed one replaced the account kept, saved with the driver later); the fixed build keeps
// them once logged on (onlineKeepAccount).
void onlineStore(WindowHost& host) {
    const TextTable& texts = host.gameTexts();
    host.message(MessageType::Note, texts.at(kTextLoggingIn), false, nullptr);
}

// The edit buffers kept as the driver's account (0x00409FC0, 0x0040A020, 0x0040A080: the settings' +0xE4, +0x122,
// +0x188; the fixed build's driver save keeps them protected, Core/Saves/OnlineAccount).
void onlineKeepAccount(WindowHost& host) {
    save::Settings& set = host.optionSettings();
    const OnlineForms& f = host.shared().onlineForms;
    set.setWideText(save::Settings::kOnlineNick, f.nick, 31);
    set.setWideText(save::Settings::kOnlineEmail, f.email, 51);
    set.setWideText(save::Settings::kOnlinePassword, f.password, 31);
}

// The login's frame, the fixed build's in place of the stock's loop inside the frame (the screen held, Sleep(50) and
// the user object's frame of 50 s each time): the game's loop runs the user object's frame; a failed state its note
// (0x00472940: signed out), no login within its time "Failed to connect"; logged on, the message closed and the
// account kept.
LoginStep onlineLoginFrame(WindowHost& host, int32_t& connecting, float& time, bool keepAccount) {
    net::OnlineUser* u = net::onlineUser();
    if (u == nullptr) {
        connecting = 0;
        return LoginStep::NoUser;
    }
    time += host.frameTime();
    size_t text = loginText(u->state);
    if (text == 0 && u->loggedOn == 0 && time > kLoginTime) text = kTextFailedToConnect;
    if (text != 0) {
        onlineFailed(host, text, connecting);
        return LoginStep::Failed;
    }
    if (u->loggedOn == 0) return LoginStep::Waiting;
    connecting = 0;
    host.closeMessage();
    if (keepAccount) onlineKeepAccount(host);
    return LoginStep::LoggedOn;
}

// 0x004719F0: the services brought up (0x00471920); every element whose name holds "EULA_" hidden (its agreement:
// the console's); BACK not pressed, the message number 9999, 1 s, not leaving.
void OnlineLogon::enter(WindowHost& host, ui::Screen& s) {
    onlineBringUp(host);
    s.hideContaining("EULA_");
    *this = OnlineLogon{};
    // The fixed build: a third item, the guest's, with BACKMENU's bar under it (the .ui's two items alternate
    // ui_pan5_yel and ui_pan5_ora, drawn adding: the third yellow).
    if (s.find("MENU") >= 0 && !hasItem(s, "MENU", kGuest)) {
        s.addCommandItem("MENU", kGuestText, 0.0f, kGuest);
        if (s.find("BACKMENU") >= 0) {
            s.addMenuItem("BACKMENU", L"", 0.0f, "ui_pan5_yel");
            const size_t bar = s.menuSize("BACKMENU") - 1;
            s.setMenuItemCommand("BACKMENU", bar, "none");
            s.setMenuItemAdditive("BACKMENU", bar, true);
        }
    }
}

// 0x00471A60. Leaving: nothing. BACKMENU follows MENU. (A message numbered 9999, never given, would close after the
// wait and go back to menu.) BACK pressed the frame before: the user object let go (0x004EEA10, its sign-out),
// Winsock let go with the services, CLICK, menu, leaving. SELECT: "connect" fills the edit buffers with the driver's
// account (CLICK, online_connect_gs); "create" empties them (CLICK, online_create_gs). BACK: noted for the next frame.
void OnlineLogon::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    OnlineForms& f = host.shared().onlineForms;
    if (m_leaving != 0) return;
    if (s.find("MENU") >= 0 && s.find("BACKMENU") >= 0) s.selectMenuItem("BACKMENU", s.selectedMenuItem("MENU"));
    // The fixed build's guest: its logon (the available check) a frame at a time as a login, then online_main.
    if (m_guest != 0) {
        const LoginStep step = onlineLoginFrame(host, m_guest, m_guestTime, false);
        if (step == LoginStep::LoggedOn) {
            host.request("online_main", SwitchMode::Replace);
            m_leaving = 1;
        }
        if (step != LoginStep::NoUser) return;
    }
    if (host.messageNumber() == m_box) {
        if (host.onlineTopMoving()) return;
        if (m_wait > 0.0f) {
            m_wait -= host.frameTime();
            return;
        }
        host.closeMessage();
        host.request("menu", SwitchMode::Replace);
        m_leaving = 1;
        return;
    }
    if (m_back == 1) {
        net::endOnlineUser();
        if (net::onlineUp()) {
            net::stopWinsock();
            net::onlineUp() = false;
        }
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
        m_leaving = 1;
        return;
    }
    if (event == input::kSelect) {
        if (s.find("MENU") < 0) return;
        const std::string item = s.menuItemCommand("MENU", s.selectedMenuItem("MENU"));
        if (same(item, "connect")) {
            const save::Settings& set = host.optionSettings();
            f.nick = set.wideText(save::Settings::kOnlineNick, 31);
            f.email = set.wideText(save::Settings::kOnlineEmail, 51);
            f.password = set.wideText(save::Settings::kOnlinePassword, 31);
            host.sound("CLICK");
            host.request("online_connect_gs", SwitchMode::Replace);
        } else if (same(item, "create")) {
            f.nick.clear();
            f.email.clear();
            f.password.clear();
            f.verify.clear();
            host.sound("CLICK");
            host.request("online_create_gs", SwitchMode::Replace);
        } else if (same(item, kGuest)) {
            // The fixed build's guest under the driver's name ("Guest" without one): "Logging into account..." while
            // the available check answers, then online_main.
            GameData* data = host.gameData();
            const std::wstring name = data != nullptr && !data->profileName.empty() ? data->profileName : L"Guest";
            if (net::OnlineUser* u = net::onlineUser()) {
                host.sound("CLICK");
                host.message(MessageType::Note, host.gameTexts().at(kTextLoggingIn), false, nullptr);
                u->setGuest(narrowWide(name.substr(0, 30)));
                m_guest = 1;
                m_guestTime = 0.0f;
            }
        }
    } else if (event == input::kBack) {
        m_back = 1;
    }
}

// 0x00471D70: NICK and EMAIL the buffers, PASSWORD and VERIFY a star for each character; not connecting, the message
// number 999999, no answer, SUBMIT not accepted; the button the keyboard went back for highlighted.
void OnlineCreate::enter(WindowHost& host, ui::Screen& s) {
    const OnlineForms& f = host.shared().onlineForms;
    if (s.find("NICK") >= 0) s.setText("NICK", f.nick);
    if (s.find("EMAIL") >= 0) s.setText("EMAIL", f.email);
    setFirst(s, "PASSWORD", std::wstring(f.password.size(), L'*'));
    setFirst(s, "VERIFY", std::wstring(f.verify.size(), L'*'));
    *this = OnlineCreate{};
    if (!f.createFocus.empty() && s.find(f.createFocus) >= 0) s.setCurrentButton(s.nameOf(s.find(f.createFocus)));
}

// 0x00472070. "Logging into account..." up and the login not begun: once it has moved in, the account given to the user
// object as a new profile (0x0055F090) and the login, a frame at a time in the fixed build (onlineLoginFrame; the
// stock's loop inside this one frame held the screen, its states 1 .. 8 and a server not answering for ever): a failed
// state its note, signed out (0x00472940); logged on, the message closed, the account kept and to be saved,
// online_main. SUBMIT accepted: with an account kept, "If you continue, existing Gamespy account information will be
// lost! .." (starting on no), else stored at once; stored: "Logging into account...". SELECT with no message up on the
// highlighted button: B_SUBMIT (CLICK) checks the nick, the email (an '@' neither first nor last), the password and the
// verification; B_NICK, B_EMAIL, B_PASSWORD, B_VERIFY the keyboard, the next button kept for the way back. BACK with no
// message up: CLICK, online_logon_gs.
void OnlineCreate::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    OnlineForms& f = host.shared().onlineForms;
    const int32_t box = host.messageNumber();
    if (box == m_box && m_connecting == 0) {
        if (host.onlineTopMoving()) return;
        if (net::OnlineUser* u = net::onlineUser()) {
            u->setCredentials(narrowWide(f.nick), narrowWide(f.email), narrowWide(f.password), true);
        }
        m_connecting = 1;
        m_loginTime = 0.0f;
    }
    if (m_connecting != 0) {
        const LoginStep step = onlineLoginFrame(host, m_connecting, m_loginTime);
        if (step == LoginStep::LoggedOn) {
            f.accountChanged = 1;
            host.request("online_main", SwitchMode::Replace);
            return;
        }
        if (step != LoginStep::NoUser) return;
    }
    if (m_submit == 1) {
        m_submit = -1;
        if (!host.optionSettings().wideText(save::Settings::kOnlineNick, 31).empty()) {
            host.message(MessageType::YesNo, texts.at(kTextLoseAccount), false, &m_store, true);
            m_store = -1;
        } else {
            m_store = 1;
        }
        return;
    }
    if (m_submit == 0) m_submit = -1;
    if (m_store == 1) {
        onlineStore(host);
        m_box = host.messageNumber();
        m_store = -1;
        return;
    }
    if (event == input::kSelect) {
        if (box != 0) return;
        const std::string& button = s.currentButton();
        if (button.empty()) return;
        if (same(button, "B_SUBMIT")) {
            host.sound("CLICK");
            size_t text = 0;
            const size_t at = f.email.find(L'@');
            if (f.nick.empty()) {
                text = kTextNoNick;
            } else if (f.email.empty() || at == std::wstring::npos || at == 0 || at == f.email.size() - 1) {
                text = kTextNoEmail;
            } else if (f.password.empty()) {
                text = kTextNoPassword;
            } else if (f.password != f.verify) {
                text = kTextNoMatch;
            } else {
                m_submit = 1;
                return;
            }
            host.message(MessageType::Continue, texts.at(text), false, nullptr);
        } else if (same(button, "B_NICK")) {
            f.createFocus = "B_EMAIL";
            onlineKeyboard(host, kTextChooseNick, f.nick, 30, true, kNickCharacters, false, "online_create_gs");
        } else if (same(button, "B_EMAIL")) {
            f.createFocus = "B_PASSWORD";
            onlineKeyboard(host, kTextEnterEmail, f.email, 50, false, kAccountCharacters, false, "online_create_gs");
        } else if (same(button, "B_PASSWORD")) {
            f.createFocus = "B_VERIFY";
            onlineKeyboard(host, kTextChoosePassword, f.password, 30, false, kAccountCharacters, true, "online_create_gs");
        } else if (same(button, "B_VERIFY")) {
            f.createFocus = "B_SUBMIT";
            onlineKeyboard(host, kTextVerifyPassword, f.verify, 30, false, kAccountCharacters, true, "online_create_gs");
        }
    } else if (event == input::kBack) {
        if (box != 0) return;
        host.sound("CLICK");
        host.request("online_logon_gs", SwitchMode::Replace);
    }
}

// 0x00472030, the leave of online_create_gs, online_connect_gs and online_main: a message up closed (the Menu's
// item kept as every window's leave keeps it).
void leaveOnlineForm(WindowHost& host) {
    host.closeMessage();
}

// 0x00472760: the services brought up (0x00471920); nick and email the buffers, password a star for each character
// (names matched case aside); not connecting, the account not changed, the message number 999999, no answer, SUBMIT
// not accepted; the button the keyboard went back for highlighted (0x005A99F0).
void OnlineConnect::enter(WindowHost& host, ui::Screen& s) {
    const OnlineForms& f = host.shared().onlineForms;
    onlineBringUp(host);
    if (s.find("nick") >= 0) s.setText("nick", f.nick);
    if (s.find("email") >= 0) s.setText("email", f.email);
    setFirst(s, "password", std::wstring(f.password.size(), L'*'));
    *this = OnlineConnect{};
    if (!f.connectFocus.empty() && s.find(f.connectFocus) >= 0) s.setCurrentButton(s.nameOf(s.find(f.connectFocus)));
}

// 0x00472A90. The command line's account ([0x0075EE84]): stored at once (no check, no question). "Logging into
// account..." up and the login not begun: once it has moved in, the account given to the user object (0x0055F090)
// and the login as online_create_gs's (the stock's loop: the states 1, 3, 8, 14 "Failed to connect", 9 .. 13 and 15
// their notes, 2, 4, 5, 6 and 7 for ever); logged on: the message closed, the account kept and to be saved when it
// changed, CLICK, online_main. SUBMIT accepted: the account against the driver's (no user object: nothing more); a
// changed one asks "If you continue, existing Gamespy account information will be lost! .." (starting on no), else
// stored at once. SELECT with no message up: B_SUBMIT (no CLICK) checks the nick, the email and the password;
// NICKTITLE, EMAILTITLE, PASSTITLE the keyboard. BACK with no message up: CLICK, online_logon_gs.
void OnlineConnect::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    OnlineForms& f = host.shared().onlineForms;
    if (f.commandLine != 0) {
        f.commandLine = 0;
        m_store = 1;
    }
    const int32_t box = host.messageNumber();
    if (box == m_box && m_connecting == 0) {
        if (host.onlineTopMoving()) return;
        if (net::OnlineUser* u = net::onlineUser()) {
            u->setCredentials(narrowWide(f.nick), narrowWide(f.email), narrowWide(f.password), false);
        }
        m_connecting = 1;
        m_loginTime = 0.0f;
    }
    if (m_connecting != 0) {
        const LoginStep step = onlineLoginFrame(host, m_connecting, m_loginTime);
        if (step == LoginStep::LoggedOn) {
            if (m_changed != 0) f.accountChanged = 1;
            host.sound("CLICK");
            host.request("online_main", SwitchMode::Replace);
            return;
        }
        if (step != LoginStep::NoUser) return;
    }
    if (m_submit == 1) {
        m_submit = -1;
        const save::Settings& set = host.optionSettings();
        m_changed = f.password != set.wideText(save::Settings::kOnlinePassword, 31) ||
                            f.email != set.wideText(save::Settings::kOnlineEmail, 51) ||
                            f.nick != set.wideText(save::Settings::kOnlineNick, 31)
                        ? 1
                        : 0;
        if (net::onlineUser() == nullptr) return;
        if (m_changed != 0) {
            host.message(MessageType::YesNo, texts.at(kTextLoseAccount), false, &m_store, true);
            m_store = -1;
        } else {
            m_store = 1;
        }
        return;
    }
    if (m_submit == 0) m_submit = -1;
    if (m_store == 1) {
        onlineStore(host);
        m_box = host.messageNumber();
        m_store = -1;
        return;
    }
    if (event == input::kSelect) {
        if (box != 0) return;
        const std::string& button = s.currentButton();
        if (button.empty()) return;
        if (same(button, "B_SUBMIT")) {
            size_t text = 0;
            const size_t at = f.email.find(L'@');
            if (f.nick.empty()) {
                text = kTextNoNick;
            } else if (f.email.empty() || at == std::wstring::npos || at == 0 || at == f.email.size() - 1) {
                text = kTextNoEmail;
            } else if (f.password.empty()) {
                text = kTextNoPassword;
            } else {
                m_submit = 1;
                return;
            }
            host.message(MessageType::Continue, texts.at(text), false, nullptr);
        } else if (same(button, "NICKTITLE")) {
            f.connectFocus = "EMAILTITLE";
            onlineKeyboard(host, kTextEnterNick, f.nick, 30, true, kNickCharacters, false, "online_connect_gs");
        } else if (same(button, "EMAILTITLE")) {
            f.connectFocus = "PASSTITLE";
            onlineKeyboard(host, kTextEnterEmail, f.email, 50, false, kAccountCharacters, false, "online_connect_gs");
        } else if (same(button, "PASSTITLE")) {
            f.connectFocus = "B_SUBMIT";
            onlineKeyboard(host, kTextEnterPassword, f.password, 30, false, kAccountCharacters, true, "online_connect_gs");
        }
    } else if (event == input::kBack) {
        if (box != 0) return;
        host.sound("CLICK");
        host.request("online_logon_gs", SwitchMode::Replace);
    }
}

// 0x00473EA0: every GAMESPY hidden, the online status (0x00473CD0); the network game made when there is none
// (0x004E9C40), the matchmaking made when there is none (its hook); 5 s; with a user object the online setups'
// settings (+0x21C, +0x268) take our first player's nick at their +4; the account changed and the saving on: the
// autosave over this window (mode 5: the account kept once the login is good, protected in the fixed build's driver
// save; the stock's was kept before, in plain text) and the flag cleared; not joining, the sign-out not asked.
void OnlineMain::enter(OnlineHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    hideAll(s, "GAMESPY");
    frontend::showOnlineStatus(s, texts);
    // The fixed build: a guest's menu without the friends and the scoreboards (BACKMENU's highlights with it); an
    // account's whole again.
    const net::OnlineUser* signedIn = net::onlineUser();
    if (signedIn != nullptr && signedIn->guest()) {
        for (const char* item : kAccountOnly) {
            if (s.removeMenuItem("MENU", item) && s.find("BACKMENU") >= 0 && s.menuSize("BACKMENU") != 0) {
                s.removeMenuItemAt("BACKMENU", s.menuSize("BACKMENU") - 1);
            }
        }
    } else if (s.find("MENU") >= 0 && !hasItem(s, "MENU", kAccountOnly[0])) {
        s.restoreMenu("MENU");
        if (s.find("BACKMENU") >= 0) s.restoreMenu("BACKMENU");
    }
    if (net::netGame() == nullptr) {
        net::startNetGame(gameRandom());
        host.netHook();
    }
    if (net::matchmaking() == nullptr) net::makeMatchmaking();
    m_unusedTimer = 5.0f;
    if (net::OnlineUser* u = net::onlineUser()) {
        save::Settings& set = host.optionSettings();
        const int32_t p = u->localPlayers.empty() ? -1 : u->localPlayers[0];
        setupNick(set, 0x21C, u->playerNick(p));
        setupNick(set, 0x268, u->playerNick(p));
    }
    if (host.shared().onlineForms.accountChanged != 0 && data != nullptr && data->saving != 0) {
        host.request("autosave", SwitchMode::Push);
        host.shared().onlineForms.accountChanged = 0;
    }
    m_joining = 0;
    m_joined = 0;
    m_signOutAnswer = -1;
    m_unusedMessage = 99999;
}

// online_main's join of a session at an address (-connect, an invitation): everything reset (0x004EA4E0 with 1), the
// matchmaking's join (0x004E58C0), joining, "JOINING GAME..." (type 0).
void OnlineMain::joinAt(OnlineHost& host, const net::Address& address) {
    const TextTable& texts = host.gameTexts();
    if (net::NetGame* net = net::netGame()) net->session.reset(true);
    join(address);
    m_joining = 1;
    host.message(MessageType::Note, texts.at(kTextJoining), false, nullptr);
}

// 0x004740D0: [0x0075EE70] 0; BACKMENU follows MENU; the pink slip's note (0x004A4EE0). -host
// ([0x0075EE9C]): online_mode to create (2). -connect ([0x0075EEA0]) or an invitation ([0x00715D24]): its join. Joining:
// the matchmaking's. Joined: once no message moves in, the session lost (0x00471860) stops; in the session the message
// closed and link_lobby. Else the online status; the sign-out answered no forgotten, yes: the network game ended
// (0x004E9DD0) and the online objects let go (friends, matchmaking, voice, statistics, the user object, Winsock), CLICK,
// menu. SELECT: quick, search, create online_mode with its choice 0, 1, 2 (CLICK); friends online_bg with
// online_friends (CLICK, [0x0075EE70] 1); rankings online_stats (CLICK); sign_out (CLICK) "Are you sure you want to
// sign out?" (starting on no). BACK: the network game ended, the matchmaking let go, CLICK, menu (still logged in: the
// main menu's online goes straight back here).
void OnlineMain::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    OnlineForms& f = host.shared().onlineForms;
    f.friendsFromMain = 0;
    if (s.find("MENU") >= 0 && s.find("BACKMENU") >= 0) s.selectMenuItem("BACKMENU", s.selectedMenuItem("MENU"));
    pinkSlipNote(host);
    if (f.host != 0) {
        f.mode = OnlineChoice::Create;
        f.host = 0;
        host.request("online_mode", SwitchMode::Replace);
        return;
    }
    if (f.connect != 0) {
        f.connect = 0;
        if (f.connectAddress) joinAt(host, *f.connectAddress);
        f.connectAddress.reset();
        return;
    }
    if (f.invitation) {
        joinAt(host, *f.invitation);
        f.invitation.reset();
        return;
    }
    if (m_joining != 0) {
        joining(host);
        return;
    }
    if (m_joined != 0) {
        if (host.onlineTopMoving()) return;
        if (host.netDropped()) {
            m_joined = 0;
            return;
        }
        net::NetGame* net = net::netGame();
        if (net != nullptr && net->transport.inSession()) {
            m_joined = 0;
            host.closeMessage();
            host.request("link_lobby", SwitchMode::Replace);
        }
        return;
    }
    frontend::showOnlineStatus(s, texts);
    if (m_signOutAnswer == 0) {
        m_signOutAnswer = -1;
    } else if (m_signOutAnswer == 1) {
        net::endNetGame();
        net::endOnline();
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
    }
    if (event == input::kSelect) {
        if (s.find("MENU") < 0 || s.menuSize("MENU") == 0) return;
        const std::string item = s.menuItemCommand("MENU", s.selectedMenuItem("MENU"));
        if (same(item, "quick") || same(item, "search") || same(item, "create")) {
            f.mode = same(item, "quick")    ? OnlineChoice::Quick
                     : same(item, "search") ? OnlineChoice::Search
                                            : OnlineChoice::Create;
            host.sound("CLICK");
            host.request("online_mode", SwitchMode::Replace);
        } else if (same(item, "friends")) {
            host.sound("CLICK");
            f.background = "online_friends";
            host.request("online_bg", SwitchMode::Replace);
            f.friendsFromMain = 1;
        } else if (same(item, "rankings")) {
            host.sound("CLICK");
            host.request("online_stats", SwitchMode::Replace);
        } else if (same(item, "sign_out")) {
            host.sound("CLICK");
            host.message(MessageType::YesNo, texts.at(kTextSignOut), false, &m_signOutAnswer, true);
        }
    } else if (event == input::kBack) {
        net::endNetGame();
        net::releaseMatchmaking();
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
    }
}

// 0x004E58C0: the re-query's attributes (12, one result) and the address asked through the master (the matchmaking's
// +0x24 with 1). Nothing without the matchmaking.
void OnlineMain::join(const net::Address& address) {
    net::Matchmaking* mm = net::matchmaking();
    if (mm == nullptr) return;
    const std::vector<net::SearchAttribute> attributes = net::requeryAttributes();
    mm->setSearchAttributes(1, static_cast<uint32_t>(attributes.size()), attributes.data());
    mm->searchAddress(1, address);
}

// online_main's +0x0C: once no message moves in (0x005A7150), with the matchmaking's answers (0x004E6300) or its list in
// (+0x0C) the join no longer waited for, the search stopped (0x004E62F0) and either the session at position 0 joined
// (0x004734E0; joined) or the message closed and "This game session is no longer available." (type 1). Without the
// matchmaking it waits.
void OnlineMain::joining(OnlineHost& host) {
    net::Matchmaking* mm = net::matchmaking();
    if (mm == nullptr || host.onlineTopMoving()) return;
    if (net::countValidResults() == 0 && mm->listIn == 0) return;
    m_joining = 0;
    mm->stopSearch();
    if (net::countValidResults() != 0) {
        if (host.onlineJoinSession(0)) m_joined = 1;
    } else {
        host.closeMessage();
        host.message(MessageType::Continue, host.jetText(kJetNoLongerAvailable), false, nullptr);
    }
}

}  // namespace oj::frontend
