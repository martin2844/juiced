// careerload (and garageload): three frames, then the career's window.

#include "Game/Frontend/Career/CareerLoad.h"

#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// careerload (0x004358A0 / 0x00435960): three frames, then the window the career manager's +0xA4 names (mode 3):
// 0 carsel (carbuy while the dealer's +0 is set), 1 CMSGridWalk, 2 character, 3 characterPostRace, 4 cmsCutScene,
// 5 car_rolling, 6 and 7 carsel, 8 CMSOnlineGridWalk. With +0xA4 0 and the dealer's +0 1 the GR_ elements are
// hidden, else shown. The enter clears the front end's +0x38 (its controllers not watched; the leave, 0x00435930,
// sets it again).
void CareerLoad::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    m_frames = 3;
    host.watchControllers(false);
    const DealerMode dealer0 = data != nullptr ? data->career->dealer.mode : DealerMode::Garage;
    if (data != nullptr && data->careerScreen == CareerScreen::CarSel && dealer0 == DealerMode::Offers) {
        s.hideContaining("GR_");
    } else {
        s.showContaining("GR_", false);
    }
}

void CareerLoad::update(CareerHost& host, ui::Screen& /*s*/) {
    GameData* data = host.gameData();
    if (m_frames != 0) {
        --m_frames;
        return;
    }
    if (data == nullptr) {
        return;
    }
    const DealerMode dealer0 = data->career->dealer.mode;
    static const char* const kNext[] = {"carsel",      "CMSGridWalk", "character", "characterPostRace", "cmsCutScene",
                                        "car_rolling", "carsel",      "carsel",    "CMSOnlineGridWalk"};
    const CareerScreen screen = data->careerScreen;
    if (screen == CareerScreen::CarSel && dealer0 != DealerMode::Garage) {
        host.request("carbuy", SwitchMode::ReplaceAfterTransitions);
    } else if (screen >= CareerScreen::CarSel && screen <= CareerScreen::OnlineGridWalk) {
        host.request(kNext[static_cast<size_t>(screen)], SwitchMode::ReplaceAfterTransitions);
    }
}

}  // namespace oj::frontend
