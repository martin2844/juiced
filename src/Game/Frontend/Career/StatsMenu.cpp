// CMSStatsMenu: the career's stats' menu.

#include "Game/Frontend/Career/StatsMenu.h"

#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// CMSStatsMenu (0x004E0700): Cash and Date; menu's item of value 1 (the car collection) enabled with cars in the
// garage.
void StatsMenu::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    careerCashTexts(s, *data->career, host.language().index, texts);
    for (size_t i = 0; i < s.menuSize("menu"); ++i) {
        if (s.menuItemValue("menu", i) == 1.0f) {
            s.setMenuItemEnabledAt("menu", i, !data->career->garage.empty());
            break;
        }
    }
}

// 0x004E0760: MenuBG follows menu; BACK (CLICK) CMSMainMenu; SELECT (CLICK) by the item's value: 0 CMSStats, 1
// CMSStatsCarCollection, 2 CMSRespect.
void StatsMenu::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    const size_t selected = s.selectedMenuItem("menu");
    s.selectMenuItem("MenuBG", selected);
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    if (event != input::kSelect || s.menuSize("menu") == 0) return;
    host.sound("CLICK");
    const float value = s.menuItemValue("menu", selected);
    if (value == 0.0f) host.request("CMSStats", SwitchMode::Replace);
    if (value == 1.0f) host.request("CMSStatsCarCollection", SwitchMode::Replace);
    if (value == 2.0f) host.request("CMSRespect", SwitchMode::Replace);
}

}  // namespace oj::frontend
