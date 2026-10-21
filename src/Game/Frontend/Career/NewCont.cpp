// CMSNewCont: a new career or the one going on.

#include "Game/Frontend/Career/NewCont.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// CMSNewCont (0x004C37E0): the button Continue highlighted, Menu at its first item, +0x0C 0, +0x14 0, +0x10 -1.
void NewCont::enter(CareerHost& /*host*/, ui::Screen& s) {
    s.setCurrentButton("Continue");
    s.selectMenuItem("Menu", 0);
    m_leaving = 0;
    m_asked = 0;
    m_answer = -1;
}

// 0x004C3910: backmenu follows menu. Asked (+0x14): a "yes" asks for careerload with the career's +0xA4 2 (the betting
// window of a new career's first event), a "no" clears +0x14. SELECT (CLICK): NewGame, not asked yet, asks (CLICK, type
// 4) "CURRENT GAME WILL BE LOST" (42) and "ARE YOU SURE?" (86) into +0x10 (then -1) and sets +0x14; Continue goes to
// CMSMainMenu. BACK, not asked: +0x0C set, CLICK and the main menu.
void NewCont::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    s.selectMenuItem("backmenu", s.selectedMenuItem("menu"));
    if (m_asked != 0) {
        if (m_answer == 1) {
            host.request("careerload", SwitchMode::Replace);
            if (data != nullptr) data->careerScreen = CareerScreen::Character;
        } else if (m_answer == 0) {
            m_asked = 0;
            return;
        }
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        const std::string item = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
        if (_stricmp(item.c_str(), "NewGame") == 0 && m_asked == 0) {
            host.sound("CLICK");
            host.message(MessageType::YesNoModeless, texts.at(kTextGameWillBeLost) + L"\n" + texts.at(kTextAreYouSure),
                         false, &m_answer, true);
            m_answer = -1;
            m_asked = 1;
        }
        if (_stricmp(item.c_str(), "Continue") == 0) host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    if (event == input::kBack && m_asked == 0) {
        m_leaving = 1;
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
    }
}

// 0x004C3830: the state keeps a Menu element's highlighted item; leaving after a "yes" to a new game (+0x0C 0, +0x14
// set): the career's head 0, a new career and the first event's race set up (0x004A2300 and the lines of
// 0x00435434).
void NewCont::leave(CareerHost& host) {
    GameData* data = host.gameData();
    if (m_leaving != 0 || m_asked == 0 || data == nullptr) return;
    data->newGameCareer(gameRandom(), crewName8(data->profileName));
}

}  // namespace oj::frontend
