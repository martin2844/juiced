#pragma once
// The fixed build's changes to the game's windows as they are read (its own; the files stay as they are).
//
//   opts_graphics: the glow's row a meter, GLOW_METER with its GLOW_MINUS and GLOW_PLUS boxes, made as the detail's
//   (DETAIL and the MINUSSIGN1 and PLUSSIGN1 boxes of its row) moved down by GLOW_BUT's place below DETAIL_BUT, in
//   place of the ARROW boxes of the GLOW text's row; the text stays for "-" without shaders. Nothing without those
//   elements, or with a GLOW_METER already.
//   keyboard: the lobby chat's lines CHAT_LINE1 .. 6 (texts in goodtime11, left, 0.04 of the screen apart from 0.05
//   down, across the window), in the top frame's place above the keys (Frontend/Network/LobbyChat).
//   link_lobby: the chat's hint in the bottom bar, CHAT_BUT (the second button's picture, ui_selectb) and CHAT_TEXT,
//   between PLAYERS and OK, the FRIENDS and PLAYERS hints moved left for it (FRIENDS_BUT 0.021, its text 0.068,
//   PLAYERS_BUT 0.190, its text 0.240).

#include "Game/UI/UiFile.h"

#include <string>

namespace oj::ui {

inline constexpr const char* kGlowMeter = "GLOW_METER";
inline constexpr const char* kGlowMinus = "GLOW_MINUS";
inline constexpr const char* kGlowPlus = "GLOW_PLUS";
inline constexpr int kChatLines = 6;
inline constexpr const char* kChatButton = "CHAT_BUT";
inline constexpr const char* kChatText = "CHAT_TEXT";
// "CHAT_LINE1" .. "CHAT_LINE6" for 0 .. 5.
std::string chatLineName(int index);

// The window of the name (opts_graphics, keyboard, link_lobby; case aside) changed as above.
void fixWindow(const std::string& name, Window& window);

}  // namespace oj::ui
