// The custom race's window split_selcar (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/CustomRace/SplitSelCar.h"

#include "Game/Frontend/CustomRace/CustomRaceInternal.h"

namespace oj::frontend {

using namespace customrace;

// split_selcar's enter (0x0046CE10): [game + 4]'s method +0x34 with 1 (empty on the PC); the second player's group the
// first's ([game + 0x88]+0x784). Each player's place (its groups' +0x780, in the ids of the first player's group): 0,
// then on past locked cars, a place without a car back to 0; its car the first of its arcade cars ([game + 0x64]+0x40,
// [game + 0x68]+0x40) of that model (0x00489D70 inlined; 0 when none). Both cars repaired (0x0048EB20), their parts
// cleared (0x004600C0) and rated (0x0048E8A0). Each view (0x0045A2C0 on CAR3D / CAR3D2 with CARNAME(2) and CARLOGO(2),
// no origin, the mirror image, the folder kept, the zoom 0.7, the shadow) shows its car turned 3 pi / 4, turning 0.4 a
// second, fading. Paints 0, not ready, MODDING not shown and every element holding "MODDING" hidden; Locked1 / Locked2
// hidden for a car not locked.
void SplitSelCar::enter(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    for (auto& view : m_views) {
        if (view) view->clear();
        view.reset();
    }
    *this = SplitSelCar{};
    if (data == nullptr) return;
    GameData& d = *data;
    d.arcadeGroups2.group = d.arcadeGroups.group;
    const std::array<ArcadeCarGroups*, 2> groups = {&d.arcadeGroups, &d.arcadeGroups2};
    const std::array<std::vector<Car>*, 2> cars = {&d.arcadeCars, &d.arcadeCars2};
    const int32_t group = d.arcadeGroups.group;
    for (size_t i = 0; i < 2; ++i) {
        ArcadeCarGroups& g = *groups[i];
        g.index = 0;
        while (carLocked(d.arcade, carAt(g, group, g.index))) {
            ++g.index;
            if (carAt(g, group, g.index) == 0) {
                g.index = 0;
                break;
            }
        }
        m_place[i] = g.index;
        m_car[i] = arcadeCarOf(*cars[i], carAt(g, group, m_place[i]));
    }
    for (size_t i = 0; i < 2; ++i) {
        if (m_car[i] >= cars[i]->size()) continue;
        Car& car = (*cars[i])[m_car[i]];
        repairCar(car);
        clearParts(car);
        rateCar(car, car.def != nullptr && d.dynamics ? d.dynamics->of(*car.def) : nullptr, d.catalogue);
    }
    for (size_t i = 0; i < 2; ++i) {
        const std::string n = i == 0 ? "" : "2";
        if (host.engine() == nullptr || s.find("CAR3D" + n) < 0) continue;
        scene::CarView::Setup setup;
        setup.element = "CAR3D" + n;
        setup.name = "CARNAME" + n;
        setup.logo = "CARLOGO" + n;
        setup.zoom = 0.7f;
        setup.mirror = true;
        setup.keepFolder = true;
        setup.shadow = true;
        m_views[i] = std::make_unique<scene::CarView>(*host.engine(), s, setup, d.catalogue, d.tables);
        if (m_car[i] < cars[i]->size()) {
            m_views[i]->show((*cars[i])[m_car[i]], nullptr, m_car[i], std::bit_cast<float>(0x4016CBE4u),
                             std::bit_cast<float>(0x3ECCCCCDu), true, host.viewLoading());
        }
    }
    s.hideContaining("MODDING");
    for (size_t i = 0; i < 2; ++i) {
        if (carLocked(d.arcade, carAt(*groups[i], group, m_place[i]))) continue;
        hideNamed(s, i == 0 ? "Locked1" : "Locked2");
    }
}

// 0x0046D4A0, each player with its own event (the window's +0x88 / +0x8C). While its view loads (+0x40) MODDING1 /
// MODDING2 comes in (once) and the event is dropped; else it goes out (once). Not ready: UP / DOWN its paint after /
// before, the car painted (0x0045A640: the dial colours of a random [DialColour], rand()), CLICK; LEFT / RIGHT its
// groups' place before / after (0x0042A830 / 0x0042A800; the ids of the first player's group), its car the arcade
// car of that model, SELECT, repaired, its parts cleared and shown turned pi / 2 (turning 0.4, fading), locked1 /
// locked2 shown for a locked car, else hidden; 6 and 7 (CLICK) the automatic modification or the parts cleared
// (0x0045A8A0). SELECT: a car not locked ready (CLICK), else NO. BACK (CLICK): not ready, the first player's asks for
// instant_setup; either lets the second player's device go (the input manager's +0x2C -1) and clears the front end's
// +0x20 and +0x24 (its UI map registered again with a device, its note counted as up); ready, not ready. READY1 /
// READY2 shown while ready; Drive1 / Drive2 the car's drive (1830 .. by 0x0048CE20).
void SplitSelCar::updatePlayer(CustomRaceHost& host, ui::Screen& s, size_t i) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    GameData& d = *data;
    const std::array<ArcadeCarGroups*, 2> groups = {&d.arcadeGroups, &d.arcadeGroups2};
    std::vector<Car>& cars = i == 0 ? d.arcadeCars : d.arcadeCars2;
    std::unique_ptr<scene::CarView>& view = m_views[i];
    uint32_t event = host.playerEvent(i);
    const char* modding = i == 0 ? "MODDING1" : "MODDING2";
    if (!view || !view->busy()) {
        if (m_modding[i] != 0) {
            s.hideContaining(modding, true);
            m_modding[i] = 0;
        }
    } else {
        event = input::kNone;
        if (m_modding[i] == 0) {
            s.showContaining(modding, true);
            m_modding[i] = 1;
        }
    }
    Car* car = m_car[i] < cars.size() ? &cars[m_car[i]] : nullptr;
    const int32_t group = d.arcadeGroups.group;
    const auto showPlace = [&] {
        m_place[i] = groups[i]->index;
        const int32_t id = carAt(*groups[i], group, m_place[i]);
        m_car[i] = arcadeCarOf(cars, id);
        host.sound("SELECT");
        if (m_car[i] < cars.size()) {
            Car& shown = cars[m_car[i]];
            repairCar(shown);
            clearParts(shown);
            if (view) {
                view->show(shown, nullptr, m_car[i], std::bit_cast<float>(0x3FC90FDBu),
                           std::bit_cast<float>(0x3ECCCCCDu), true, host.viewLoading());
            }
        }
        const std::string name = std::format("locked{}", i + 1);
        if (!carLocked(d.arcade, id)) {
            hideNamed(s, name.c_str());
        } else {
            s.show(name, false);
        }
    };
    switch (event) {
    case input::kUp:
    case input::kDown: {
        if (m_ready[i] != 0) break;
        const auto paints = static_cast<int32_t>(d.defs.paints.size());
        if (event == input::kUp) {
            if (++m_paint[i] >= paints) m_paint[i] = 0;
        } else if (--m_paint[i] < 0) {
            m_paint[i] = paints - 1;
        }
        if (car != nullptr && view && view->hasModel() && m_paint[i] >= 0 && m_paint[i] < paints) {
            const CarPaint& p = d.defs.paints[static_cast<size_t>(m_paint[i])];
            car->base = p.base;
            car->metallic = p.metallic;
            car->pearl = p.pearl;
            car->metallicAmount = std::bit_cast<uint32_t>(p.metallicAmount);
            car->pearlAmount = std::bit_cast<uint32_t>(p.pearlAmount);
            const size_t dials = d.defs.dials.size();
            if (dials != 0) {
                const auto r = static_cast<uint32_t>(gameRandom().next()) % static_cast<uint32_t>(dials);
                car->dialColour = d.defs.dials[r][0];
                car->dialTextColour = d.defs.dials[r][1];
            }
            view->repaint(*car);
        }
        host.sound("CLICK");
        break;
    }
    case input::kLeft:
        if (m_ready[i] != 0) break;
        groups[i]->previous();
        showPlace();
        break;
    case input::kRight:
        if (m_ready[i] != 0) break;
        groups[i]->next();
        showPlace();
        break;
    case input::kSelect:
        if (!carLocked(d.arcade, carAt(*groups[i], group, m_place[i]))) {
            host.sound("CLICK");
            m_ready[i] = 1;
        } else {
            host.sound("NO");
        }
        break;
    case input::kSelect2:
    case input::kSelect3:
        if (m_ready[i] != 0) break;
        if (car != nullptr && view && !view->busy() && view->hasModel()) {
            if (event == input::kSelect3) {
                clearParts(*car);
            } else {
                CarMaking making{d.defs, d.catalogue, gameRandom(), d.autoModifyCounter};
                autoModify(*car, false, 2, making);
            }
            view->refit(*car, d.defs, gameRandom());
        }
        host.sound("CLICK");
        break;
    case input::kBack:
        host.sound("CLICK");
        if (m_ready[i] == 0) {
            if (i == 0) host.request("instant_setup", SwitchMode::Replace);
            host.askSecondPlayerRelease();
            host.forgetSecondPlayerUi();
        } else {
            m_ready[i] = 0;
        }
        break;
    default:
        break;
    }
    s.setHidden(i == 0 ? "READY1" : "READY2", m_ready[i] == 0);
    if (m_car[i] < cars.size()) {
        s.setText(std::format("Drive{}", i + 1),
                  texts.at(kTextDrives + static_cast<size_t>(driveOf(cars[m_car[i]], d.tables))));
    }
}

// 0x0046DA60: each player's part (0x0046D4A0), its view's frame (0x00459EE0) and CARFADE1 / CARFADE2's alpha 1 - its
// +0x4C. Both ready: the race's automatic gearbox (+0x54) the settings', loading. The players' cars to their save
// system objects' +0x50. Without the second player's device (the input manager's +0x2C) PLAYER2COVER and PLAYER2LOGO
// shown and a device taken for it (vt+0x3C with 1), else both hidden. Neither view loading (+0x40, +0x3C): both cars
// rated, BHP1 / BHP2 "<rating to the nearest> bhp" shown; else both hidden.
void SplitSelCar::update(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    GameData& d = *data;
    bool bothReady = true;
    for (size_t i = 0; i < 2; ++i) {
        updatePlayer(host, s, i);
        if (m_ready[i] == 0) bothReady = false;
        if (m_views[i]) {
            m_views[i]->frame(host.frameTime(), gameRandom());
            if (const int e = s.find(i == 0 ? "CARFADE1" : "CARFADE2"); e >= 0) {
                s.setAlpha(e, 1.0f - m_views[i]->shown());
            }
        }
    }
    if (bothReady) {
        d.race.gearbox = static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
        host.request("loading", SwitchMode::Replace);
    }
    d.arcadeCar = static_cast<int32_t>(m_car[0]);
    d.arcadeCar2 = static_cast<int32_t>(m_car[1]);
    s.setHidden("PLAYER2COVER", host.secondPlayer());
    s.setHidden("PLAYER2LOGO", host.secondPlayer());
    if (!host.secondPlayer()) host.askSecondPlayerTake();
    const auto loading = [](const std::unique_ptr<scene::CarView>& v) { return v && v->busy(); };
    if (loading(m_views[0]) || loading(m_views[1])) {
        s.setHidden("BHP1", true);
        s.setHidden("BHP2", true);
        return;
    }
    const std::array<std::vector<Car>*, 2> cars = {&d.arcadeCars, &d.arcadeCars2};
    for (size_t i = 0; i < 2; ++i) {
        if (m_car[i] >= cars[i]->size()) continue;
        Car& car = (*cars[i])[m_car[i]];
        rateCar(car, car.def != nullptr && d.dynamics ? d.dynamics->of(*car.def) : nullptr, d.catalogue);
    }
    for (size_t i = 0; i < 2; ++i) {
        if (m_car[i] >= cars[i]->size()) continue;
        const std::string bhp = i == 0 ? "BHP1" : "BHP2";
        const float rating = std::bit_cast<float>((*cars[i])[m_car[i]].rating);
        s.setText(bhp, toWide(std::format("{} ", x87::nearest(rating))) + texts.at(kTextBhp));
        s.setHidden(bhp, false);
    }
}

// 0x0046D3F0: the windows' leave (the Menu's item kept); both views' loads let go and the views freed (0x0045ACE0,
// 0x0045A340); [game + 4]'s method +0x34 with 0.
void SplitSelCar::leave() {
    for (auto& view : m_views) {
        if (view) view->clear();
        view.reset();
    }
}

}  // namespace oj::frontend
