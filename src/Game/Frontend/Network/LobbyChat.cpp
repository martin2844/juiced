#include "Game/Frontend/Network/LobbyChat.h"

#include "Core/Network/GameSpy/Peerchat.h"
#include "Core/Network/NetSession.h"
#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowFixes.h"

#include <array>
#include <format>
#include <string>
#include <vector>

namespace oj::frontend {

namespace {

// The keyboard window's top frame, where the chat's lines go.
constexpr std::array<const char*, 6> kTopFrame = {"INSTRUCTION",      "FRAME_TRIBAL01",       "FRAME_LINESTOP",
                                                  "FRAME_METALLINE1", "FRAME_CORNER_BLUETOP", "FRAME_CORNER_REDTOP"};

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
    return net::peerchat::lobbyChat() != nullptr && game != nullptr &&
           game->session.stage() == net::SessionStage::Lobby;
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

void openLobbyChat(OnlineHost& host) {
    KeyboardSetup& k = host.keyboardSetup();
    k = KeyboardSetup{};
    k.chat = true;
    for (char c = 0x20; c < 0x7F; ++c) k.allowed.push_back(c);
    k.minLength = 1;
    k.maxLength = 100;
    k.small = true;
    k.checkName = false;
    k.symbols = true;
    k.back = "link_lobby";
    k.next = "link_lobby";
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
}

bool lobbyChatFrame(ui::Screen& s) {
    net::peerchat::LobbyChat* chat = net::peerchat::lobbyChat();
    if (chat == nullptr || !lobbyChatOn()) return false;
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
