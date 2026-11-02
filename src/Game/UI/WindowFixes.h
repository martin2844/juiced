#pragma once
// The fixed build's changes to the game's windows as they are read (its own; the files stay as they are).
//
//   keyboard: the lobby chat's lines CHAT_LINE1 .. 12 (texts in goodtime11, left, 0.033 of the screen apart from 0.05
//   down, across the window), in the upper half the chat's keyboard leaves (Frontend/Network/LobbyChat).
//   link_lobby: the chat's hint in the bottom bar, CHAT_BUT (the third button's picture, ui_selectc) and CHAT_TEXT,
//   between PLAYERS and OK, the FRIENDS and PLAYERS hints moved left for it (FRIENDS_BUT 0.021, its text 0.068,
//   PLAYERS_BUT 0.190, its text 0.240).
//   link_selcar: the chat's hint the same, at 0.270, left of READY (SELECTA 0.439).
//   loadreplay: the replay's picture INFO_TRACK_PIC (the Acclaim builds' element, which the PC's file leaves out; the
//   picture set by the window's code) right of its details, and over it INFO_TRACK_PIC_FRAME, the white frame
//   (ui_replayborder) of a replay window the Xbox builds have in their files and never use, its line on the picture's
//   edge.
//   opts_game_pc, opts_ingame_game: the field of view's row (the fixed build's): the rows below the hidden VIB row
//   (the PC's steering wheel, every VIB element hidden by the window) up a place each, SENSITIVITY's row copied into
//   the last place as FOV_BUT, FOV_METER, FOV_MINUS, FOV_PLUS and the degrees FOV_VALUE right of the plus (a copy of
//   the DIFF text). Nothing without those elements, or with a FOV_BUT already.
//   opts_graphics: the depth of field's row out (DOF_BUT, DOF and the arrows on its line; the fixed build draws none);
//   the fixed build's ADVANCED page of the effects (Frontend/Options/GraphicsOptions): MORE_BUT (a copy of RESET_BUT)
//   in DOF's place at 0.715 between GLOW_BUT and RESET_BUT (RESET_BUT and DONE_BUT where they are); the page's six rows
//   from BRIGHT's place 0.0625 apart, each on one line: FX1_BUT .. FX6_BUT (copies of SHADOWS_BUT), their value FX1 ..
//   FX6 (0.22 wide) between the arrows FXLEFT and FXRIGHT (SHADOWS' and VIB_ARROW's), right of the labels. Nothing
//   without those elements, or with a MORE_BUT already.

#include "Game/UI/UiFile.h"

#include <string>

namespace oj::ui {

inline constexpr const char* kFovButton = "FOV_BUT";
inline constexpr const char* kFovMeter = "FOV_METER";
inline constexpr const char* kFovMinus = "FOV_MINUS";
inline constexpr const char* kFovPlus = "FOV_PLUS";
inline constexpr const char* kFovValue = "FOV_VALUE";
inline constexpr int kChatLines = 12;
inline constexpr const char* kChatButton = "CHAT_BUT";
inline constexpr const char* kChatText = "CHAT_TEXT";
inline constexpr const char* kReplayPicture = "INFO_TRACK_PIC";
inline constexpr const char* kReplayPictureFrame = "INFO_TRACK_PIC_FRAME";
inline constexpr const char* kMoreButton = "MORE_BUT";
inline constexpr const char* kEffectsPart = "FX";  // every element of the ADVANCED page has it, no other
inline constexpr int kEffectsRows = 9;
// The row shown as a meter of ten steps between a minus and a plus (HEAT HAZE; the user, 2026-10-29: the game's own
// selector), in place of its value and arrows.
inline constexpr int kEffectsMeterRow = 8;
inline constexpr const char* kEffectsMeter = "FXMETER";
inline constexpr const char* kEffectsMinus = "FXMINUS";
inline constexpr const char* kEffectsPlus = "FXPLUS";
// The front end's audio options' EAX row (opts_audio; the user, 2026-10-29: EAX in the game, OpenJuiceConfig's Audio
// page gone; not in a race's pause, whose banks were made with or without it): its button, its value between arrows.
inline constexpr const char* kEaxButton = "EAX_BUT";
inline constexpr const char* kEaxValue = "EAX_VALUE";
inline constexpr const char* kEaxLeft = "EAX_ARROW1";
inline constexpr const char* kEaxRight = "EAX_ARROW2";
// "FX1_BUT" .., "FX1" .. (the value) for the rows 0 .. 6 (the meter's row has no value).
std::string effectsButton(int row);
std::string effectsValue(int row);
// "CHAT_LINE1" .. "CHAT_LINE12" for 0 .. 11.
std::string chatLineName(int index);

// The window of the name (opts_game_pc, opts_ingame_game, opts_graphics, keyboard, link_lobby, link_selcar,
// loadreplay; case aside)
// changed as above.
void fixWindow(const std::string& name, Window& window);

}  // namespace oj::ui
