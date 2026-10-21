// The car's stats over a window (Car_Stats2, Car_Stats2purple) and the makers' logos over the windows
// (car_StatsLogos).

#include "Game/Frontend/Garage/CarStats.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>

namespace oj::frontend {

namespace {

// The kinds of catalogue part by the number of their P (V for kind 4) icons; -1 none (0x004C520C).
constexpr int kPartIcons[10] = {2, 3, 7, 6, 6, 4, 5, -1, 1, -1};
// The body categories by the number of their V icons and VB box; -1 none (0x004C5234).
constexpr int kBodyIcons[10] = {1, 4, 2, 5, 3, -1, -1, -1, -1, 8};

}  // namespace

// 0x004C4920: the parts' icons, each the first element of its name: KeyIcon3 blinks. For each kind of catalogue part
// with icons (kind 4 "V<n>-<l>", the others "P<n>-<l>", levels 1 .. 4): shown; ui_mod_got for the level of the car's
// part (its +0x2B; the kind's first part past the list), else ui_mod_locked, and unless the level is past the
// profile's +0x50 one of the model and kind with a garage car (+0x10), ui_mod_available, blinking from the car's
// level on. For each body category with icons: with parts (more than one, or one not standard, the list's +0x14) VB<n>
// white and each "V<n>-<l>" (levels 1 .. 3) shown: ui_mod_got below the car's part (+0x04), else ui_mod_locked and
// unless past the profile's +0x262 one with a garage car ui_mod_available and blinking; without, VB<n> dark grey
// (0xFF282828) and every "V<n>-<l>" hidden. The car's +0x50 is set when its metallic colour is not its base colour
// (RGB), +0x54 the same for the pearl. V7-1 and V7-2 (the paints) shown: ui_mod_locked past the profile's +0x474 of
// the model with a garage car, else ui_mod_available, and ui_mod_got for a metallic (V7-1) or pearl (V7-2) car, else
// blinking. Stock problem: a missing VB or V7 element is written through a null pointer; here it is passed over.
void CarStats::parts(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Car& car = *g_statsCar;
    const CareerProfile& p = data->career->profile;
    const auto model = static_cast<size_t>(std::clamp(m_model, 0, 52));
    m_blink.clear();
    m_blink.push_back(s.find("KeyIcon3"));
    for (size_t kind = 0; kind < 10; ++kind) {
        if (kPartIcons[kind] < 0) continue;
        const int unlocked = p.partsA[model][kind];
        for (int level = 0; level < 4; ++level) {
            const std::string name = std::format("{}{}-{}", kind == kWheel ? "V" : "P", kPartIcons[kind], level + 1);
            const int e = s.find(name);
            if (e < 0) continue;
            s.setHidden(e, false);
            const auto part = static_cast<int8_t>(car.mods[kind]);
            int carLevel = part;
            if (part != -1) {
                const std::vector<CarPart>& parts = data->catalogue.lists[kind].parts;
                const size_t at = static_cast<size_t>(part) < parts.size() ? static_cast<size_t>(part) : 0;
                carLevel = at < parts.size() ? parts[at].level : -1;
            }
            if (level == carLevel) {
                s.setTexture(e, "ui_mod_got");
                continue;
            }
            s.setTexture(e, "ui_mod_locked");
            if (level > unlocked && m_garage != 0) continue;
            s.setTexture(e, "ui_mod_available");
            if (level >= carLevel) m_blink.push_back(e);
        }
    }
    for (size_t kind = 0; kind < 10; ++kind) {
        if (kBodyIcons[kind] < 0) continue;
        const int unlocked = p.partsB[model][kind];
        const std::string box = std::format("VB{}", kBodyIcons[kind]);
        for (int level = 0; level < 3; ++level) {
            const CarPartList* list = car.def != nullptr ? &car.def->parts[kind] : nullptr;
            const size_t n = list != nullptr ? list->parts.size() : 0;
            const std::string name = std::format("V{}-{}", kBodyIcons[kind], level + 1);
            if (n == 0 || (n == 1 && list->standard)) {
                s.setBoxColour(box, 0xFF282828);
                hideNamed(s, name, true);
                continue;
            }
            s.setBoxColour(box, 0xFFFFFFFF);
            const int e = s.find(name);
            if (e < 0) continue;
            s.setHidden(e, false);
            if (level < static_cast<int8_t>(car.body[kind])) {
                s.setTexture(e, "ui_mod_got");
                continue;
            }
            s.setTexture(e, "ui_mod_locked");
            if (level > unlocked && m_garage != 0) continue;
            s.setTexture(e, "ui_mod_available");
            m_blink.push_back(e);
        }
    }
    car.metallicBought = (car.base & 0xFFFFFF) != (car.metallic & 0xFFFFFF) ? 1 : 0;
    car.pearlBought = (car.base & 0xFFFFFF) != (car.pearl & 0xFFFFFF) ? 1 : 0;
    const int paints = p.carLevel[model];
    for (int k = 0; k < 2; ++k) {
        const int e = s.find(std::format("V7-{}", k + 1));
        if (e < 0) continue;
        s.setHidden(e, false);
        if (k > paints && m_garage != 0) {
            s.setTexture(e, "ui_mod_locked");
            continue;
        }
        s.setTexture(e, "ui_mod_available");
        if ((car.metallicBought != 0 && k == 0) || (car.pearlBought != 0 && k == 1)) {
            s.setTexture(e, "ui_mod_got");
        } else {
            m_blink.push_back(e);
        }
    }
}

// 0x004C5260 (car_StatsLogos): the base enter; on the car [0x00728C60] for the catalogue's kinds 8, 0, 1, 5, 6, 3 and 2 the elements
// Logo1 .. Logo7 and Stock1 .. Stock7: a car's part of the kind shows Logo<n> with the picture "ui_<the part's
// supplier>_on" (its +0x2A, the names at 0x00717970; the kind's first part past its list) and hides Stock<n>, else
// Logo<n> is hidden and Stock<n> shown.
void CarStats::enterLogos(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    host.baseEnter(s, "Car_StatsLogos");
    if (data == nullptr || g_statsCar == nullptr) return;
    const Car& car = *g_statsCar;
    for (size_t kind = 0; kind < 10; ++kind) {
        const int n = kind == kWheel ? -1 : kPartIcons[kind];
        if (n < 0) continue;
        const int logo = s.find(std::format("Logo{}", n));
        const int stock = s.find(std::format("Stock{}", n));
        const auto part = static_cast<int8_t>(car.mods[kind]);
        if (part == -1) {
            if (logo >= 0) s.setHidden(logo, true);
            if (stock >= 0) s.setHidden(stock, false);
            continue;
        }
        const std::vector<CarPart>& parts = data->catalogue.lists[kind].parts;
        const size_t at = static_cast<size_t>(part) < parts.size() ? static_cast<size_t>(part) : 0;
        const int brand = at < parts.size() ? static_cast<int8_t>(parts[at].brand) : -1;
        std::string name;
        for (const ExeTables::Brand& b : data->tables.brands) {
            if (b.id == brand) name = b.name;
        }
        if (logo >= 0) {
            s.setTexture(logo, "ui_" + name + "_on");
            s.setHidden(logo, false);
        }
        if (stock >= 0) s.setHidden(stock, true);
    }
}

// 0x004C42D0 (Car_Stats2 and Car_Stats2purple) on the car [0x00728C60]: +0x0C its model's id; BHPVAL its rating (cut),
// CARVAL the model's display name, CLASSVAL its class + 1 (0x0048E9E0), Spent the money spent on it (+0xA8) and
// Winnings the money it won (+0xAC) (each "NA", 432, when not over 0), Value its sale worth (0x0048DA40); the parts'
// icons (0x004C4920). A garage car (+0x10) shows Logo_But and Logo_Text, [0x00728C64] 0; else car_StatsLogos over
// the window (mode 8), every Logo_But and Logo_Text hidden, [0x00728C64] 1.
void CarStats::enter(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career || g_statsCar == nullptr) return;
    Car& car = *g_statsCar;
    const int language = host.language().index;
    m_model = car.def != nullptr ? car.def->id : 0;
    s.setText("BHPVAL", toWide(std::format("{}", x87::chop(std::bit_cast<float>(car.rating)))));
    s.setText("CARVAL", car.def != nullptr ? widenSigned(car.def->displayName) : std::wstring());
    s.setText("CLASSVAL", toWide(std::format("{}", carClass(car) + 1)));
    const auto money = [&](uint32_t bits) {
        const float v = std::bit_cast<float>(bits);
        return v > 0.0f ? toWide(moneyText(x87::chop(v), language)) : texts.at(kTextNotAvailable);
    };
    s.setText("Spent", money(car.spent));
    s.setText("Winnings", money(car.winnings));
    s.setText("Value", toWide(moneyText(saleWorth(car, data->catalogue), language)));
    parts(host, s);
    if (m_garage != 0) {
        s.show("Logo_But", false);
        s.show("Logo_Text", false);
        m_logosShown = 0;
    } else {
        host.request("car_StatsLogos", SwitchMode::Overlay);
        hideNamed(s, "Logo_But", true);
        hideNamed(s, "Logo_Text", true);
        m_logosShown = 1;
    }
}

// 0x004C4770: the phase [0x007A2D40] on by the frame time x 8, back to 0 past 6.282. A garage car: the blinking
// elements' alpha (sin(phase) + 1) x 0.5 (0x0042E4F0); PGDN (CLICK) takes car_StatsLogos off (the front end's +0x30
// left and freed) or puts it over the window (mode 8) and flips [0x00728C64]. BACK (CLICK) closes the window
// (mode 6). Leaving (0x004C4710) puts the blinking elements' alpha back to 1 and keeps a Menu element's highlighted
// item; the window's screen goes with it here.
void CarStats::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    m_blinkPhase = m_blinkPhase + host.frameTime() * 8.0f;
    if (static_cast<double>(m_blinkPhase) > 6.282) m_blinkPhase = 0.0f;
    if (m_garage != 0) {
        const float alpha = (static_cast<float>(std::sin(static_cast<double>(m_blinkPhase))) + 1.0f) * 0.5f;
        for (const int e : m_blink) {
            if (e >= 0) s.setAlpha(e, alpha);
        }
        if (event == input::kSelect2) {
            host.sound("CLICK");
            if (m_logosShown != 0) {
                host.closeOverlay();
            } else {
                host.request("car_StatsLogos", SwitchMode::Overlay);
            }
            m_logosShown = 1 - m_logosShown;
            return;
        }
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
    }
}

}  // namespace oj::frontend
