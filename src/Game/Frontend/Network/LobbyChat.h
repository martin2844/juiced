#pragma once
// The fixed build's chat of an online session's lobby on the game's keyboard window (the user, 2026-10-24: as
// FlatOut 2's, in the lobby only; the chat above, the game's keyboard below, the physical keys too, so a pad or a
// keyboard): link_lobby's SELECT2 (the pad's second button, PAGE DOWN) opens it in an online session, its hint
// CHAT_BUT / CHAT_TEXT (WindowFixes) with the lines unread; the keyboard window's top frame gives way to the chat's
// lines CHAT_LINE1 .. 6; DONE or ENTER says the text; BACK, or the session leaving the lobby, goes back to link_lobby.
// The chat itself: Core/Network/GameSpy/Peerchat.

#include <string>

namespace oj::ui {
class Screen;
}

namespace oj::frontend {

class OnlineHost;

// The chat of an online session there (its hint shown, SELECT2 taken).
bool lobbyChatOn();
// link_lobby's hint: CHAT_BUT and CHAT_TEXT ("CHAT", with the lines unread "CHAT (n)") shown with the chat, else
// hidden.
void lobbyChatHint(ui::Screen& s);
// The keyboard set up for the chat (its mode, every printable character, 100 at most, back to link_lobby) and the
// keyboard window.
void openLobbyChat(OnlineHost& host);
// The keyboard window in the chat's mode (its top frame hidden, the chat's lines shown) or not (the other way round).
void lobbyChatLayout(ui::Screen& s, bool chat);
// A frame of the chat's keyboard: the last lines wrapped into CHAT_LINE1 .. 6 (made again when the chat changed), the
// unread cleared; false when the chat or the session's lobby is gone.
bool lobbyChatFrame(ui::Screen& s);
// The text said to the chat; false when it is empty or the chat not joined.
bool lobbyChatSay(const std::wstring& text);

}  // namespace oj::frontend
