// The options' menu (opts_menu).

#include "Game/Frontend/Options/OptionsMenu.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace oj::frontend {

// 0x00480090 (opts_menu; from a race's pause opts_ingame_menu, [G+0x3C]): Menu and BackMenu (+0x10,
// +0x14) their +0x1A0 1 and back to their items (0x005AECF0); control's text "CONTROLS" (21); "STEERING WHEEL"
// (1677, wheel) put before the last item (shown only) and BackMenu given an orange bar (added at the end: one more
// in its items at each opening of the kept window, the highlighted one's the only picture drawn); control's text
// again; with [0x0071E718] & 1 audio taken out (and BackMenu's first item); without a network game of more players
// "VOICE" (1914, voice) before the last item and an orange bar (its +0x54 1); a highlight past the items back to
// the first; +0x0C 0.
void OptionsMenu::enter(OptionsHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    if (s.find("Menu") >= 0) {
        s.restoreMenu("Menu");
        s.restoreMenu("BackMenu");
        for (size_t i = 0; i < s.menuSize("Menu"); ++i) {
            if (_stricmp(s.menuItemCommand("Menu", i).c_str(), "control") == 0) {
                s.setMenuItemText("Menu", i, texts.at(kTextControls));
            }
        }
        if (host.racePause()) {
            // From a race's pause: a highlighted control moves the highlight to the first item; control taken out
            // with BackMenu's first item; a highlight past the items back to the first.
            if (s.menuSize("Menu") != 0 &&
                _stricmp(s.menuItemCommand("Menu", s.selectedMenuItem("Menu")).c_str(), "control") == 0) {
                s.selectMenuItem("Menu", 0);
                s.selectMenuItem("BackMenu", 0);
            }
            if (s.removeMenuItem("Menu", "control") && s.menuSize("BackMenu") != 0) {
                s.removeMenuItemAt("BackMenu", 0);
            }
            if (s.selectedMenuItem("Menu") >= s.menuSize("Menu")) {
                s.selectMenuItem("Menu", 0);
                s.selectMenuItem("BackMenu", 0);
            }
        }
        s.insertCommandItem("Menu", static_cast<int>(s.menuSize("Menu")) - 1, texts.at(kTextSteeringWheel), 0.0f,
                            "wheel");
        s.addMenuItem("BackMenu", L"", 0.0f, "", "ui_pan5_ora");
        // [0x0071E718] & 1 (audio taken out): 0 here. VOICE but for the second player's pause of a race of more
        // players (the race track's pausing player +0x10, its number 1).
        if (!host.secondPlayerPaused()) {
            s.insertCommandItem("Menu", static_cast<int>(s.menuSize("Menu")) - 1, texts.at(kTextVoice), 0.0f,
                                "voice");
            s.addMenuItem("BackMenu", L"", 0.0f, "", "ui_pan5_ora");
            s.setMenuItemAdditive("BackMenu", s.menuSize("BackMenu") - 1, true);
        }
        if (s.selectedMenuItem("Menu") >= s.menuSize("Menu")) {
            s.selectMenuItem("Menu", 0);
            s.selectMenuItem("BackMenu", 0);
        }
    }
    m_saving = 0;
}

// 0x00480310: BackMenu follows Menu. +0x0C set: the main menu (mode 3) with the window's transitions going out. The
// pausing or first player's controller's motors (0x0041DD60 with 0; with no race front end +0x10C and +0x110 0;
// read by nothing on the PC) not followed. SELECT on Menu's item (each player's event; one here): audio opts_audio,
// game opts_game_pc, graph opts_graphics, wheel opts_wheel_dx9, voice opts_voice (mode 7: in place of this window;
// from a race's pause their in-game windows), control the controls' set up (0x004080D0, 0x00407FB0: controlsSetUp),
// with CLICK; done (CLICK) when the save system saves the autosave over the window and +0x0C set, else the main menu
// (mode 3) with the transitions going out (a race's pause: hud_pause). BACK the same as done (CLICK only without the
// autosave).
void OptionsMenu::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    if (s.find("Menu") >= 0 && s.find("BackMenu") >= 0) {
        s.selectMenuItem("BackMenu", s.selectedMenuItem("Menu"));
    }
    if (m_saving != 0) {
        host.request("menu", SwitchMode::ReplaceAfterTransitions);
    }
    const GameData* data = host.gameData();
    const bool saving = data != nullptr && data->saving != 0;
    const auto leave = [&] {
        if (saving) {
            host.request("autosave", SwitchMode::Push);
            m_saving = 1;
        } else {
            host.request("menu", SwitchMode::ReplaceAfterTransitions);
        }
    };
    // Each player's event in turn (the window's +0x88, +0x8C; [G+0x6C]+0x58 players).
    const int32_t players = data != nullptr ? std::min(data->race.players, 2) : 1;
    for (int32_t player = 0; player < players; ++player) {
        const uint32_t e = player == 0 ? event : host.secondPlayerEvent();
        if (e == input::kSelect) {
            if (s.menuSize("Menu") == 0) continue;
            const std::string item = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
            const auto named = [&](const char* name) { return _stricmp(item.c_str(), name) == 0; };
            if (named("audio")) {
                host.request(host.racePause() ? "opts_ingame_audio" : "opts_audio", SwitchMode::Swap);
            } else if (named("game")) {
                // From a race's pause the first player's opts_ingame_game, the second's opts_ingame_game_ply2.
                const char* inRace = player == 0 ? "opts_ingame_game" : "opts_ingame_game_ply2";
                host.request(host.racePause() ? inRace : "opts_game_pc", SwitchMode::Swap);
            } else if (named("control")) {
                host.controlsSetUp();
            } else if (named("graph")) {
                host.request("opts_graphics", SwitchMode::Swap);
            } else if (named("wheel")) {
                host.request("opts_wheel_dx9", SwitchMode::Swap);
            } else if (named("voice")) {
                host.request("opts_voice", SwitchMode::Swap);
            } else if (named("done")) {
                host.sound("CLICK");
                if (host.racePause()) {
                    host.request("hud_pause", SwitchMode::Replace);
                } else {
                    leave();
                }
                continue;
            } else {
                continue;
            }
            host.sound("CLICK");
        } else if (e == input::kBack) {
            if (host.racePause()) {
                host.request("hud_pause", SwitchMode::ReplaceAfterTransitions);
                continue;
            }
            if (!saving) host.sound("CLICK");
            leave();
        }
    }
}

}  // namespace oj::frontend
