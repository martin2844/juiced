// An earlier career menu, never registered ("unregistered:CMSMainMenu").

#include "Game/Frontend/Career/OldCareerMenu.h"

#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x004CDBF0: Date and Cash (0x004CE040, then again by 0x004A4130), +0x0C 0, SELECTIONIMAGE kept (+0x10), no item
// highlighted yet (+0x14).
void OldCareerMenu::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    *this = OldCareerMenu{};
    if (data != nullptr && data->career) {
        careerCashTexts(s, *data->career, host.language().index, texts);
        careerCashTexts(s, *data->career, host.language().index, texts);
    }
    m_hasImage = s.find("SELECTIONIMAGE") >= 0;
}

// 0x004CDC50: a new highlighted item of Menu gives SELECTIONIMAGE the picture of its name (calendar, cars, respect,
// crew, mobile, career: ui_hubbg_calendar, _cars, _respect, _crew, _mobile, _summary) and starts its transition again
// (any other name only the transition); the phone's frame (0x004B60F0). SELECT (CLICK) by the item: Cars a window
// named "" (Stock problem: no such window), Calendar CMSCalendar, Respect CMSRespect, Crew CMSCrew, Mobile
// CMSMobilePhone over this one, Career CMSCareerSummary. Leaving (+0x0C) goes to menu (mode 2). BACK (CLICK): with
// saving on the autosave over the window; +0x0C 1.
void OldCareerMenu::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const std::string item = s.menuSize("Menu") != 0 ? s.menuItemCommand("Menu", s.selectedMenuItem("Menu")) : "";
    if (m_hasImage && item != m_highlighted) {
        m_highlighted = item;
        static const std::pair<const char*, const char*> kPictures[] = {
            {"calendar", "ui_hubbg_calendar"}, {"cars", "ui_hubbg_cars"},     {"respect", "ui_hubbg_respect"},
            {"crew", "ui_hubbg_crew"},         {"mobile", "ui_hubbg_mobile"}, {"career", "ui_hubbg_summary"}};
        for (const auto& [name, picture] : kPictures) {
            if (_stricmp(item.c_str(), name) == 0) {
                s.setTexture("SELECTIONIMAGE", picture);
                break;
            }
        }
        s.restartTransition("SELECTIONIMAGE");
    }
    host.phoneFrame(s);
    if (event == input::kSelect) {
        host.sound("CLICK");
        const auto is = [&](const char* name) { return _stricmp(name, item.c_str()) == 0; };
        if (is("Cars")) {
            host.request("", SwitchMode::Replace);
        } else if (is("Calendar")) {
            host.request("CMSCalendar", SwitchMode::Replace);
        } else if (is("Respect")) {
            host.request("CMSRespect", SwitchMode::Replace);
        } else if (is("Crew")) {
            host.request("CMSCrew", SwitchMode::Replace);
        } else if (is("Mobile")) {
            host.request("CMSMobilePhone", SwitchMode::Push);
        } else if (is("Career")) {
            host.request("CMSCareerSummary", SwitchMode::Replace);
        }
    }
    if (m_leaving != 0) host.request("menu", SwitchMode::ReplaceNow);
    if (event == input::kBack) {
        host.sound("CLICK");
        if (data != nullptr && data->saving != 0) host.request("autosave", SwitchMode::Push);
        m_leaving = 1;
    }
}

}  // namespace oj::frontend
