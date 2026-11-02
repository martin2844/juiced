#pragma once
// The fixed build's chat of an online session's lobby on the game's keyboard window (the user, 2026-10-24: as
// FlatOut 2's, in the lobby only; the chat above, the game's keyboard below, the physical keys too, so a pad or a
// keyboard): in an online session's lobby (link_lobby) and its cars' choice (link_selcar) SELECT3 (TAB, the pad's
// third button; the user, 2026-10-24: the same key in both) opens it, its hint CHAT_BUT / CHAT_TEXT (WindowFixes) with
// the lines unread; the keyboard window's top frame gives way to the chat's
// lines CHAT_LINE1 .. 12, the text and the keys moved down below them (the keys shorter), its empty keys typing ! ?
// and ,; the physical keys type the window's characters (any the keyboard's layout gives); DONE or ENTER says the
// text; BACK, or the session leaving the stage it was opened at, goes back to the window it was opened from.
// The chat itself: Core/Network/GameSpy/Peerchat.

#include <string>

namespace oj::ui {
class Screen;
}

namespace oj::frontend {

class NetworkHost;

// The chat of an online session there, at its lobby or its cars' choice (its hint shown, SELECT3 taken).
bool lobbyChatOn();
// link_lobby's hint: CHAT_BUT and CHAT_TEXT ("CHAT", with the lines unread "CHAT (n)") shown with the chat, else
// hidden.
void lobbyChatHint(ui::Screen& s);
// The keyboard set up for the chat (its mode, every printable character, 100 at most, back to the window) and the
// keyboard window; nothing without the front end's keyboard.
void openLobbyChat(NetworkHost& host, const char* window);
// The keyboard window in the chat's mode (its top frame hidden, the chat's lines shown, the text and the keys moved
// down, the empty keys BLANK1, BLANK4 and BLANK5 typing ! ? and ,) or not (the top frame shown, the lines hidden).
void lobbyChatLayout(ui::Screen& s, bool chat);
// A frame of the chat's keyboard: the last lines wrapped into CHAT_LINE1 .. 12 (made again when the chat changed), the
// unread cleared; false when the chat is gone or the session has left the stage the chat was opened at.
bool lobbyChatFrame(ui::Screen& s);
// The text said to the chat; false when it is empty or the chat not joined.
bool lobbyChatSay(const std::wstring& text);

}  // namespace oj::frontend
