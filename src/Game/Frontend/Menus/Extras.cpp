// The extras window and the credits.

#include "Game/Frontend/Menus/Extras.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x0046A920: menu and backmenu back to their items (0x005AECF0), replay taken out of menu and backmenu's first item
// with it (its pointer only), every ONLINEICON hidden; +0x0C 4, +0x10 -1.
void Extras::enter(WindowHost&, ui::Screen& s) {
    s.restoreMenu("menu");
    s.restoreMenu("backmenu");
    if (s.find("menu") >= 0) s.removeMenuItem("menu", "replay");
    if (s.find("backmenu") >= 0) s.removeMenuItem("backmenu", "none");
    hideNamed(s, "ONLINEICON", true);
    m_refresh = 4;
    m_answer = -1;
}

// 0x0046AA00: SELECT on menu's highlighted item (CLICK): cheats CMSCheats, credits credits, videos vid_tutorial (back
// to extras, [0x0072158C]), hiscores viewhiscores, replay loadreplay (the race's mode, [G+0x6C]+0x00, 0 first; the
// enter took the item out), save the question "Are you sure you want to save?" (1504) when the save system saves,
// else "Do you want to enable Autosave and save your driver?" (1503) into +0x10, done menu. BACK (CLICK) menu. +0x0C
// counts down; at 1 the save item's text "SAVE" (1582) or "ENABLE AUTOSAVE" (1550) and +0x0C 0. A "yes": [0x0075ED38]
// 1, autosave over the window and +0x0C 4; +0x10 back to -1.
void Extras::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const bool saving = data != nullptr && data->saving != 0;
    if (event == input::kSelect) {
        if (s.menuSize("menu") != 0) {
            host.sound("CLICK");
            const std::string item = s.menuItemCommand("menu", s.selectedMenuItem("menu"));
            const auto named = [&](const char* name) { return _stricmp(item.c_str(), name) == 0; };
            if (named("Cheats")) {
                host.request("CMSCheats", SwitchMode::Replace);
            } else if (named("credits")) {
                host.request("credits", SwitchMode::Replace);
            } else if (named("videos")) {
                host.shared().tutorialReturn = "extras";
                host.request("vid_tutorial", SwitchMode::Replace);
            } else if (named("hiscores")) {
                host.request("viewhiscores", SwitchMode::Replace);
            } else if (named("replay")) {
                if (data != nullptr) data->race.mode = RaceMode::Arcade;
                host.request("loadreplay", SwitchMode::Replace);
            } else if (named("save")) {
                host.message(MessageType::YesNo, texts.at(saving ? kTextSaveAsk : kTextEnableAutosaveAsk), false,
                             &m_answer, true);
            } else if (named("done")) {
                host.request("menu", SwitchMode::Replace);
            }
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
    }
    if (m_refresh != 0) {
        if (m_refresh > 1) {
            --m_refresh;
        } else {
            for (size_t i = 0; i < s.menuSize("Menu"); ++i) {
                if (_stricmp(s.menuItemCommand("Menu", i).c_str(), "save") != 0) continue;
                s.setMenuItemText("Menu", i, texts.at(saving ? kTextSaveItem : kTextEnableAutosave));
            }
            m_refresh = 0;
        }
    }
    if (m_answer != -1) {
        if (m_answer == 1) {
            host.shared().autosaveAsked = 1;
            host.request("autosave", SwitchMode::Push);
            m_refresh = 4;
        }
        m_answer = -1;
    }
}

void Credits::enter() {
    m_page = CreditsPage::JuiceGames;
    m_shown = 0;
}

// 0x00469D90: the page's text not written (+0x10): CREDITS multiline (+0x128), inside its box (+0x13C), scrolling
// (+0x140) from 0 (+0x144), +0x150 0xFF9CC4CE, and its text the page's lines each followed by a line feed: page 1
// lines 874 .. 1099, page 2 lines 1100 .. 1310, page 3 for each of 174 lines 1311 + n when line 1100 + n is not empty
// (stock problem: the third page reads the second page's line for its test), else lines 780 .. 873; +0x10 set.
// Stock problem: +0x150 is the Font line's colour kept for the transitions (0x005AC4EC writes it with +0x11C), of
// which the drawing takes only the alpha (0x005B2048); the text is drawn in +0x11C, the Font line's white, so the
// blue never shows.
// BACK (CLICK) extras; SELECT (CLICK) the next page (after 3 the first) to be written.
void Credits::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    if (m_shown == 0) {
        if (s.find("CREDITS") >= 0) {
            s.setMultiline("CREDITS", true);
            s.setScrolling("CREDITS", true, true);
            std::wstring text;
            const auto lines = [&](size_t first, size_t last) {
                for (size_t n = first; n <= last; ++n) text += texts.at(n) + L"\n";
            };
            switch (m_page) {
            case CreditsPage::Thq: lines(874, 1099); break;
            case CreditsPage::Licensing: lines(1100, 1310); break;
            case CreditsPage::Music:
                for (size_t n = 0; n < 174; ++n) {
                    if (!texts.at(1100 + n).empty()) text += texts.at(1311 + n);
                    text += L"\n";
                }
                break;
            default: lines(780, 873); break;
            }
            s.setText("CREDITS", text);
        }
        m_shown = 1;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("extras", SwitchMode::Replace);
    } else if (event == input::kSelect) {
        m_shown = 0;
        host.sound("CLICK");
        m_page = static_cast<CreditsPage>(static_cast<int32_t>(m_page) + 1);
        if (m_page > CreditsPage::Music) m_page = CreditsPage::JuiceGames;
    }
}

}  // namespace oj::frontend
