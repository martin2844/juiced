#include "Game/Frontend/Network/LobbyChat.h"

#include "Core/Input/Input.h"
#include "Core/Network/GameSpy/Peerchat.h"
#include "Core/Network/NetSession.h"
#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowFixes.h"

#include <array>
#include <cmath>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace oj::frontend {

namespace {

// The keyboard window's top frame, where the chat's lines go.
constexpr std::array<const char*, 6> kTopFrame = {"INSTRUCTION",      "FRAME_TRIBAL01",       "FRAME_LINESTOP",
                                                  "FRAME_METALLINE1", "FRAME_CORNER_BLUETOP", "FRAME_CORNER_REDTOP"};

// The chat's keyboard: the keys' rows from keyboard.ui's to the lower half, their height, the text above them.
constexpr std::pair<float, float> kChatKeyRows[] = {
    {0.295f, 0.530f}, {0.368f, 0.585f}, {0.441f, 0.640f}, {0.516f, 0.695f}, {0.591f, 0.750f}};
constexpr float kChatKeyHeight = 0.050f;
constexpr float kChatTextTop = 0.455f;
constexpr float kChatTextHeight = 0.060f;

// The chat's own keys: an empty key of keyboard.ui, its character and its neighbours.
struct ChatKey {
    const char* key;
    wchar_t symbol;
    const char* up;
    const char* down;
    const char* left;
    const char* right;
};
constexpr ChatKey kChatKeys[] = {
    {"BLANK1", L'!', "SHIFT", "BQ", "BACKSPACE", "B1"},
    {"BLANK4", L'?', "BACKSPACE", "DONE", "BP", "BQ"},
    {"BLANK5", L',', "B@", "SPACE", "B.", "SHIFT"},
};

std::wstring wide(std::u16string_view text) {
    return std::wstring(text.begin(), text.end());
}

// What the lines were wrapped from (the chat's last line and their count): made again when it changes.
struct Wrapped {
    size_t count = 0;
    std::u16string last;
    net::peerchat::LobbyChat::State state = net::peerchat::LobbyChat::State::Idle;
    std::array<std::wstring, ui::kChatLines> rows{};
};
Wrapped g_wrapped;
// The session's stage the chat was opened at (its window's).
net::SessionStage g_chatStage = net::SessionStage::Lobby;

// The rows of a line within the width: the longest beginning that fits each time (by halves), then the rest.
std::vector<std::wstring> wrap(ui::Screen& s, const std::wstring& line, int most) {
    std::vector<std::wstring> rows;
    std::wstring rest = line;
    const std::string element = ui::chatLineName(0);
    auto fits = [&](const std::wstring& t) { return s.fitTextWithin(element, t, most) == t; };
    while (!rest.empty()) {
        if (fits(rest)) {
            rows.push_back(rest);
            break;
        }
        size_t low = 1;
        size_t high = rest.size();
        while (low + 1 < high) {
            const size_t mid = (low + high) / 2;
            (fits(rest.substr(0, mid)) ? low : high) = mid;
        }
        // At a space when there is one in the row's second half.
        size_t cut = low;
        const size_t space = rest.rfind(L' ', cut);
        if (space != std::wstring::npos && space > cut / 2) cut = space + 1;
        rows.push_back(rest.substr(0, cut));
        rest = rest.substr(cut);
    }
    return rows;
}

std::wstring stateLine(net::peerchat::LobbyChat::State state) {
    using State = net::peerchat::LobbyChat::State;
    switch (state) {
    case State::Joined: return L"";
    case State::Failed: return L"* The chat server does not answer";
    default: return L"* Joining the chat...";
    }
}

}  // namespace

bool lobbyChatOn() {
    const net::NetGame* game = net::netGame();
    if (net::peerchat::lobbyChat() == nullptr || game == nullptr) return false;
    const net::SessionStage stage = game->session.stage();
    return stage == net::SessionStage::Lobby || stage == net::SessionStage::Cars;
}

void lobbyChatHint(ui::Screen& s) {
    const bool on = lobbyChatOn();
    for (const char* name : {ui::kChatButton, ui::kChatText}) {
        if (s.find(name) >= 0) s.setHidden(name, !on);
    }
    if (!on || s.find(ui::kChatText) < 0) return;
    const uint32_t unread = net::peerchat::lobbyChat()->unread();
    s.setText(ui::kChatText, unread != 0 ? std::format(L"CHAT ({})", unread) : std::wstring(L"CHAT"));
}

void openLobbyChat(NetworkHost& host, const char* window) {
    KeyboardSetup* setup = host.chatKeyboard();
    const net::NetGame* game = net::netGame();
    if (setup == nullptr || game == nullptr) return;
    g_chatStage = game->session.stage();
    KeyboardSetup& k = *setup;
    k = KeyboardSetup{};
    k.chat = true;
    for (char c = 0x20; c < 0x7F; ++c) k.allowed.push_back(c);
    k.minLength = 1;
    k.maxLength = 100;
    k.small = true;
    k.checkName = false;
    k.symbols = true;
    k.back = window;
    k.next = window;
    g_wrapped = Wrapped{};
    host.sound("CLICK");
    host.request("keyboard", SwitchMode::Replace);
}

void lobbyChatLayout(ui::Screen& s, bool chat) {
    for (const char* name : kTopFrame) {
        if (s.find(name) >= 0) s.setHidden(name, chat);
    }
    for (int i = 0; i < ui::kChatLines; ++i) {
        const std::string name = ui::chatLineName(i);
        if (s.find(name) < 0) continue;
        s.setHidden(name, !chat);
        s.setText(name, L"");
    }
    if (!chat) return;
    // The keys lower and shorter, the text above them, so the chat's lines fill the upper half (keyboard.ui's rows at
    // 0.295, 0.368, 0.441, 0.516 and 0.591, 0.067 high; the text at 0.687).
    for (const std::string& b : s.buttons()) {
        const int e = s.find(b);
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        s.placeOf(e, x, y, w, h);
        for (const auto& [from, to] : kChatKeyRows) {
            if (std::fabs(y - from) < 0.01f) {
                s.setPlace(e, x, to, w, kChatKeyHeight);
                break;
            }
        }
    }
    for (const char* name : {"STRING", "STRING_SMALL", "DRIVERNAMEBOX"}) {
        const int e = s.find(name);
        if (e < 0) continue;
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        s.placeOf(e, x, y, w, h);
        s.setPlace(e, x, kChatTextTop, w, kChatTextHeight);
    }
    // keyboard.ui's empty keys BLANK1 (before 1), BLANK4 (after P) and BLANK5 (after .) type ! ? and , (the game's
    // keys have none of them), linked into the rows: the pad reaches them.
    for (const ChatKey& k : kChatKeys) {
        if (s.find(k.key) < 0) continue;
        s.setText(k.key, std::wstring(1, k.symbol));
        s.setNeighbour(k.key, input::kUp, k.up);
        s.setNeighbour(k.key, input::kDown, k.down);
        s.setNeighbour(k.key, input::kLeft, k.left);
        s.setNeighbour(k.key, input::kRight, k.right);
        s.setNeighbour(k.left, input::kRight, k.key);
        s.setNeighbour(k.right, input::kLeft, k.key);
    }
}

bool lobbyChatFrame(ui::Screen& s) {
    net::peerchat::LobbyChat* chat = net::peerchat::lobbyChat();
    const net::NetGame* game = net::netGame();
    if (chat == nullptr || !lobbyChatOn() || game == nullptr || game->session.stage() != g_chatStage) return false;
    const auto& lines = chat->lines();
    const std::u16string last = lines.empty() ? std::u16string() : lines.back().nick + u":" + lines.back().text;
    if (lines.size() != g_wrapped.count || last != g_wrapped.last || chat->state() != g_wrapped.state) {
        g_wrapped.count = lines.size();
        g_wrapped.last = last;
        g_wrapped.state = chat->state();
        const std::string first = ui::chatLineName(0);
        const int most = s.find(first) >= 0 ? s.pixelWidth(first) - 4 : 600;
        std::vector<std::wstring> rows;
        // From the last line up until the rows are full.
        std::vector<std::wstring> texts;
        const std::wstring note = stateLine(chat->state());
        if (!note.empty()) texts.push_back(note);
        for (auto it = lines.rbegin(); it != lines.rend() && texts.size() < ui::kChatLines; ++it) {
            texts.push_back(it->nick.empty() ? L"* " + wide(it->text) : wide(it->nick) + L": " + wide(it->text));
        }
        for (const std::wstring& t : texts) {
            std::vector<std::wstring> w = wrap(s, t, most);
            rows.insert(rows.begin(), w.begin(), w.end());
            if (rows.size() >= ui::kChatLines) break;
        }
        if (rows.size() > ui::kChatLines) rows.erase(rows.begin(), rows.end() - ui::kChatLines);
        g_wrapped.rows = {};
        for (size_t i = 0; i < rows.size(); ++i) g_wrapped.rows[i] = rows[i];
        for (int i = 0; i < ui::kChatLines; ++i) {
            const std::string name = ui::chatLineName(i);
            if (s.find(name) >= 0) s.setText(name, g_wrapped.rows[static_cast<size_t>(i)]);
        }
    }
    chat->clearUnread();
    return true;
}

bool lobbyChatSay(const std::wstring& text) {
    net::peerchat::LobbyChat* chat = net::peerchat::lobbyChat();
    if (chat == nullptr || text.empty()) return false;
    return chat->say(std::u16string(text.begin(), text.end()));
}

}  // namespace oj::frontend
