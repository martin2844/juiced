// The pop-up windows over a window.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

namespace oj {

using namespace frontend;

bool Frontend::onlinePopup(const std::string& state) const {
    for (const char* name : {"online_arcade_create", "online_career_create", "online_players", "online_player_options",
                             "online_friends", "online_friend_options", "online_voice"}) {
        if (is(state, name)) return true;
    }
    return false;
}

void Frontend::onPopupScreen(Popup& popup, const std::function<void()>& run) {
    std::swap(m_screen, popup.screen);
    run();
    std::swap(m_screen, popup.screen);
}

void Frontend::enterPopup(Popup& popup) {
    if (baseEntered(popup.state)) baseEnter(*popup.screen, popup.state);
    if (onlinePopup(popup.state)) {
        onPopupScreen(popup, [&] {
            if (is(popup.state, "online_arcade_create")) {
                m_onlineArcadeCreate.enter(*this, *m_screen);
            } else if (is(popup.state, "online_career_create")) {
                m_onlineCareerCreate.enter(*this, *m_screen);
            } else if (is(popup.state, "online_players")) {
                m_onlinePlayers.enter(*this, *m_screen);
            } else if (is(popup.state, "online_player_options")) {
                m_onlinePlayerOptions.enter(*this, *m_screen);
            } else if (is(popup.state, "online_friends")) {
                m_onlineFriends.enter(*this, *m_screen);
            } else if (is(popup.state, "online_friend_options")) {
                m_onlineFriendOptions.enter(*this, *m_screen);
            } else if (is(popup.state, "online_voice")) {
                m_onlineVoice.enter(*this, *m_screen);
            }
        });
        return;
    }
    if (is(popup.state, "autosave")) {
        m_autosave.enter(*this, *popup.screen);
    } else if (is(popup.state, "loadhighscores")) {
        m_loadHighScores.enter(*this);
    } else if (is(popup.state, "CMSRespectMsg")) {
        m_framesUnderMessage = false;  // 0x004DC9FE
        m_respect.enter(*this, *popup.screen);
    } else if (is(popup.state, "CMSAdvice")) {
        m_advice.enter(*this, *popup.screen);
    } else if (is(popup.state, "CMSBet")) {
        m_gridWalk.enterBet(*this, *popup.screen);
    } else if (is(popup.state, "CMSCalendarHelp")) {
        m_calendarHelp.enter(*this, *popup.screen);
    } else if (is(popup.state, "car_stats2purple")) {
        m_carStatsPurple.enter(*this, *popup.screen);
    } else if (is(popup.state, "car_stats2")) {
        m_carStats.enter(*this, *popup.screen);
    } else if (is(popup.state, "opts_menu") || is(popup.state, "opts_ingame_menu")) {
        m_optionsMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "opts_game_pc") || is(popup.state, "opts_ingame_game") ||
               is(popup.state, "opts_ingame_game_ply2")) {
        m_gameOptions.enter(*this, *popup.screen);
    } else if (is(popup.state, "opts_audio") || is(popup.state, "opts_ingame_audio")) {
        m_audioOptions.enter(*this, *popup.screen);
    } else if (is(popup.state, "opts_songlist")) {
        m_songList.enter(*this, *popup.screen);
    } else if (is(popup.state, "opts_graphics")) {
        m_graphicsOptions.enter(*this, *popup.screen);
    } else if (is(popup.state, "CMSMobilePhone")) {
        m_mobilePhone.enter(*this, *popup.screen);
    } else if (is(popup.state, "CMSMobilePhoneMenu")) {
        m_mobilePhone.enterMenu(*this, *popup.screen);
    } else if (is(popup.state, "CMSMobilePhoneContacts")) {
        m_mobilePhone.enterContacts(*this, *popup.screen);
    } else if (is(popup.state, "CMSMobileChoose")) {
        m_mobilePhone.enterChoose();
    } else if (is(popup.state, "opts_wheel_dx9")) {
        m_wheelOptions.enter(*this, *popup.screen);
    } else if (is(popup.state, "modshop")) {
        m_modShop.enter(*this, *popup.screen);
    } else if (is(popup.state, "custmenu")) {
        m_custMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "tuningmenu")) {
        m_tuningMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "performtype")) {
        m_performType.enter(*this, *popup.screen);
    } else if (is(popup.state, "performmod")) {
        m_performMod.enter(*this, *popup.screen);
    } else if (is(popup.state, "buymod")) {
        m_buyMod.enter(*this, *popup.screen);
    } else if (is(popup.state, "suppliers")) {
        m_suppliers.enter(*this, *popup.screen);
    } else if (is(popup.state, "exhaustmenu")) {
        m_exhaustMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "suspensionmenu")) {
        m_suspensionMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "enginechip")) {
        m_engineChip.enter(*this, *popup.screen);
    } else if (is(popup.state, "gearboxmenu")) {
        m_gearboxMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "colourmenu")) {
        m_colourMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "colourchoose")) {
        m_colourChoose.enter(*this, *popup.screen);
    } else if (is(popup.state, "colourfocus")) {
        m_colourFocus.enter(*this, *popup.screen);
    } else if (is(popup.state, "wheelcolourmenu")) {
        m_wheelColourMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "stylingmenu")) {
        m_stylingMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "modmenu")) {
        m_modMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "wheelsmenu")) {
        m_wheelsMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "tintsmenu")) {
        m_tintsMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "neonmenu")) {
        m_neonMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "hudcolourmenu")) {
        m_hudColourMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "icemenu")) {
        m_iceMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "hornmenu")) {
        m_hornMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "decalmenu")) {
        m_decalMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "sponsordecalmenu")) {
        m_sponsorDecalMenu.enter(*this, *popup.screen);
    } else if (is(popup.state, "schememenu")) {
        m_schemeMenu.enter(*this, *popup.screen);
    } else {
        enterSharedWindow(*popup.screen, popup.state);
    }
}

void Frontend::updatePopup(Popup& popup, uint32_t event) {
    if (onlinePopup(popup.state)) {
        onPopupScreen(popup, [&] {
            if (is(popup.state, "online_arcade_create")) {
                m_onlineArcadeCreate.update(*this, *m_screen, event);
            } else if (is(popup.state, "online_career_create")) {
                m_onlineCareerCreate.update(*this, *m_screen, event);
            } else if (is(popup.state, "online_players")) {
                m_onlinePlayers.update(*this, *m_screen, event);
            } else if (is(popup.state, "online_player_options")) {
                m_onlinePlayerOptions.update(*this, *m_screen, event);
            } else if (is(popup.state, "online_friends")) {
                m_onlineFriends.update(*this, *m_screen, event);
            } else if (is(popup.state, "online_friend_options")) {
                m_onlineFriendOptions.update(*this, *m_screen, event);
            } else if (is(popup.state, "online_voice")) {
                m_onlineVoice.update(*this, *m_screen, event);
            }
        });
        return;
    }
    if (is(popup.state, "autosave")) {
        m_autosave.update(*this, *popup.screen);
    } else if (is(popup.state, "loadhighscores")) {
        m_loadHighScores.update(*this);
    } else if (is(popup.state, "CMSRespectMsg")) {
        m_respect.update(*this, *popup.screen, event);
    } else if (is(popup.state, "CMSAdvice")) {
        m_advice.update(*this, *popup.screen, event);
    } else if (is(popup.state, "CMSBet")) {
        m_gridWalk.updateBet(*this, *popup.screen, event);
    } else if (is(popup.state, "CMSCalendarHelp")) {
        m_calendarHelp.update(*this, event);
    } else if (is(popup.state, "car_stats2purple")) {
        m_carStatsPurple.update(*this, *popup.screen, event);
    } else if (is(popup.state, "car_stats2")) {
        m_carStats.update(*this, *popup.screen, event);
    } else if (is(popup.state, "opts_menu") || is(popup.state, "opts_ingame_menu")) {
        m_optionsMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "opts_game_pc") || is(popup.state, "opts_ingame_game") ||
               is(popup.state, "opts_ingame_game_ply2")) {
        m_gameOptions.update(*this, *popup.screen, event);
    } else if (is(popup.state, "opts_audio") || is(popup.state, "opts_ingame_audio")) {
        m_audioOptions.update(*this, *popup.screen, event);
    } else if (is(popup.state, "opts_songlist")) {
        m_songList.update(*this, *popup.screen, event);
    } else if (is(popup.state, "opts_graphics")) {
        m_graphicsOptions.update(*this, *popup.screen, event);
    } else if (is(popup.state, "CMSMobilePhone")) {
        m_mobilePhone.update(*this, *popup.screen, event);
    } else if (is(popup.state, "CMSMobilePhoneMenu")) {
        m_mobilePhone.updateMenu(*this, *popup.screen, event);
    } else if (is(popup.state, "CMSMobilePhoneContacts")) {
        m_mobilePhone.updateContacts(*this, *popup.screen, event);
    } else if (is(popup.state, "CMSMobileChoose")) {
        m_mobilePhone.updateChoose(*this, *popup.screen, event);
    } else if (is(popup.state, "opts_wheel_dx9")) {
        m_wheelOptions.update(*this, *popup.screen, event);
    } else if (is(popup.state, "modshop")) {
        m_modShop.update(*this, *popup.screen, event);
    } else if (is(popup.state, "custmenu")) {
        m_custMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "tuningmenu")) {
        m_tuningMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "performtype")) {
        m_performType.update(*this, *popup.screen, event);
    } else if (is(popup.state, "performmod")) {
        m_performMod.update(*this, *popup.screen, event);
    } else if (is(popup.state, "buymod")) {
        m_buyMod.update(*this);
    } else if (is(popup.state, "suppliers")) {
        m_suppliers.update(*this, *popup.screen, event);
    } else if (is(popup.state, "exhaustmenu")) {
        m_exhaustMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "suspensionmenu")) {
        m_suspensionMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "enginechip")) {
        m_engineChip.update(*this, *popup.screen, event);
    } else if (is(popup.state, "gearboxmenu")) {
        m_gearboxMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "colourmenu")) {
        m_colourMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "colourchoose")) {
        m_colourChoose.update(*this, *popup.screen, event);
    } else if (is(popup.state, "colourfocus")) {
        m_colourFocus.update(*this, *popup.screen, event);
    } else if (is(popup.state, "wheelcolourmenu")) {
        m_wheelColourMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "stylingmenu")) {
        m_stylingMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "modmenu")) {
        m_modMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "wheelsmenu")) {
        m_wheelsMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "tintsmenu")) {
        m_tintsMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "neonmenu")) {
        m_neonMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "hudcolourmenu")) {
        m_hudColourMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "icemenu")) {
        m_iceMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "hornmenu")) {
        m_hornMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "decalmenu")) {
        m_decalMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "sponsordecalmenu")) {
        m_sponsorDecalMenu.update(*this, *popup.screen, event);
    } else if (is(popup.state, "schememenu")) {
        m_schemeMenu.update(*this, *popup.screen, event);
    } else {
        updateSharedWindow(*popup.screen, popup.state, event);
    }
}

// A popup's leave: CMSMobilePhone (0x004D1AD0); CMSMobilePhoneMenu and CMSMobilePhoneContacts (0x004D2740) keep the
// Menu's highlighted item and stop the speech's line (0x0040B600 inlined).
void Frontend::leavePopup(Popup& popup) {
    if (onlinePopup(popup.state)) {
        onPopupScreen(popup, [&] {
            if (is(popup.state, "online_arcade_create") || is(popup.state, "online_career_create")) {
                frontend::leaveOnlineCreate(*this);
            } else if (is(popup.state, "online_players")) {
                m_onlinePlayers.leave(*m_screen);
            } else if (is(popup.state, "online_friends")) {
                m_onlineFriends.leave(*this, *m_screen);
            } else if (is(popup.state, "online_friend_options")) {
                m_onlineFriendOptions.leave(*this);
            } else if (is(popup.state, "online_voice")) {
                m_onlineVoice.leave(*this);
            }
        });
    }
    if (is(popup.state, "autosave")) m_autosave.leave(*this);
    if (is(popup.state, "loadhighscores")) m_loadHighScores.leave(*this);
    if (keepsMenu(popup.state)) baseLeave(*popup.screen, popup.state);
    if (is(popup.state, "car_stats2purple")) {
        m_carStatsPurple.leave(*popup.screen);
    } else if (is(popup.state, "car_stats2")) {
        m_carStats.leave(*popup.screen);
    }
    if (is(popup.state, "CMSMobilePhoneMenu") || is(popup.state, "CMSMobilePhoneContacts")) stopSpeech();
    if (is(popup.state, "CMSRespectMsg")) m_framesUnderMessage = true;  // 0x004DCB90
    if (is(popup.state, "CMSMobilePhone")) m_mobilePhone.leave(*this);
    if (is(popup.state, "custmenu") || is(popup.state, "performtype") || is(popup.state, "suppliers") ||
        is(popup.state, "suspensionmenu") || is(popup.state, "enginechip") || is(popup.state, "colourmenu") ||
        is(popup.state, "stylingmenu")) {
        baseLeave(*popup.screen, popup.state);
    }
    if (is(popup.state, "exhaustmenu") || is(popup.state, "gearboxmenu") || is(popup.state, "colourchoose") ||
        is(popup.state, "colourfocus") || is(popup.state, "wheelcolourmenu") || is(popup.state, "modmenu") ||
        is(popup.state, "tintsmenu") || is(popup.state, "neonmenu") || is(popup.state, "hudcolourmenu") ||
        is(popup.state, "icemenu") || is(popup.state, "hornmenu") || is(popup.state, "decalmenu")) {
        m_garage.leavePriced(*this, *popup.screen, popup.state);
    }
    // 0x0043FF90: hornmenu's leave is 0x00440320's (above) and stops the horn playing.
    if (is(popup.state, "hornmenu")) m_hornMenu.stopHorn(*this);
    if (is(popup.state, "wheelsmenu")) m_wheelsMenu.leave(*this, *popup.screen);
    if (is(popup.state, "opts_songlist")) m_songList.leave(*this);
    leaveSharedWindow(*popup.screen, popup.state);
    if (is(popup.state, "tuningmenu")) m_tuningMenu.leave(*this, *popup.screen);
    if (is(popup.state, "performmod")) m_performMod.leave(*this, *popup.screen);
}

}  // namespace oj
