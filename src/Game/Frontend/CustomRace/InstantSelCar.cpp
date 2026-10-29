// The custom race's window instant_selcar (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/CustomRace/InstantSelCar.h"

#include "Game/Frontend/CustomRace/CustomRaceInternal.h"

#include "Core/Network/OnlineSessions.h"

namespace oj::frontend {

using namespace customrace;

// instant_selcar's enter (0x0045EC40): the PROT_ elements hidden, shown (+0x4C) once every arcade race is completed
// (0x0042B9F0) off line (without the matchmaking, [0x0075EF84]); every arcade car repaired (0x0048EB20); paint 0. The
// group's place: an empty place goes to 0; a locked car (off line) to the last place of the group with a car not
// locked (0 when none). The car shown (+0x20) the first arcade car of that model (0 when none); the start kept (+0x2C
// .. +0x38) and on the network a copy of the car (+0x3C, 0x00434D90) for BACK. CARORIGIN hidden; the parts' buttons on
// (+0x48). Online outside an online career (0x0045EE8A) by the session's mods ([0x00728D38]): none (0) the parts'
// buttons off and the car shown's parts cleared (0x004600C0), prototypes (2) the PROT_ elements shown and prototypes
// on (+0x4C), standard (1) neither. The car view (+0x10, 0x0045A2C0 on FRAME_CAR3D, CARNAME,
// CARLOGO, CARORIGIN: the zoom 0.7, the mirror image, the car's folder kept and the shadow) shows the car rated
// (0x0048E8A0) turned pi/4, not turning, fading (0x0045A080); CARFADE (+0x18), +0x40 1; TARROW shown; MODDING hidden
// (+0x44 0); the options (0x0045F150); COUNTDOWNBOX hidden; FRAME_CAR3D's view in the map mode (0x005B4490, mode 4).
void InstantSelCar::enter(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    *this = InstantSelCar{};
    s.hideContaining("PROT_");
    if (data == nullptr) return;
    GameData& d = *data;
    if (d.arcade.finished() && net::matchmaking() == nullptr) {
        s.showContaining("PROT_", false);
        m_prototypes = true;
    }
    for (Car& car : d.arcadeCars) repairCar(car);
    ArcadeCarGroups& groups = d.arcadeGroups;
    if (groups.current() == 0) {
        groups.index = 0;
        m_place = 0;
    } else {
        m_place = groups.index;
        if (carLocked(d.arcade, groups.current()) && network() == nullptr) {
            const int32_t group = groups.group;
            groups.index = 0;
            for (int32_t k = 0; group >= 0 && group < 8 &&
                                static_cast<size_t>(k) < groups.ids[static_cast<size_t>(group)].size(); ++k) {
                const int32_t id = groups.ids[static_cast<size_t>(group)][static_cast<size_t>(k)];
                if (id == 0) break;
                if (!carLocked(d.arcade, id)) {
                    groups.index = k;
                    m_place = k;
                }
            }
        }
    }
    m_group = groups.group;
    m_car = arcadeCarOf(d.arcadeCars, groups.current());
    m_savedCar = m_car;
    m_savedPaint = m_paint;
    m_savedPlace = m_place;
    m_savedGroup = m_group;
    m_saved.reset();
    if (network() != nullptr && m_car < d.arcadeCars.size()) m_saved = copyCar(d.arcadeCars[m_car]);
    m_backAnswer = -1;
    s.setHidden("CARORIGIN", true);
    m_options = true;
    if (net::matchmaking() != nullptr && !host.onlineCareer()) {
        const int32_t mods = net::onlineHosted().arcade.mods;
        if (mods == 0) m_options = false;
        if (mods == 2) {
            s.showContaining("PROT_", false);
            m_prototypes = true;
        }
    }
    if (!m_options && m_car < d.arcadeCars.size()) clearParts(d.arcadeCars[m_car]);
    m_view.reset();
    if (host.engine() != nullptr && s.find("FRAME_CAR3D") >= 0) {
        scene::CarView::Setup setup;
        setup.element = "FRAME_CAR3D";
        setup.name = "CARNAME";
        setup.logo = "CARLOGO";
        setup.origin = "CARORIGIN";
        setup.zoom = 0.7f;
        setup.mirror = true;
        setup.keepFolder = true;
        setup.shadow = true;
        m_view = std::make_unique<scene::CarView>(*host.engine(), s, setup, d.catalogue, d.tables);
    }
    if (m_car < d.arcadeCars.size()) {
        Car& car = d.arcadeCars[m_car];
        rateCar(car, car.def != nullptr && d.dynamics ? d.dynamics->of(*car.def) : nullptr, d.catalogue);
        if (m_view) m_view->show(car, nullptr, m_car, std::bit_cast<float>(0x3F490FDBu), 0.0f, true, host.viewLoading());
    }
    m_fade = s.find("CARFADE") >= 0;
    s.showContaining("TARROW", false);
    s.hideContaining("MODDING");
    m_modding = false;
    showOptions(host, s);
    for (int e = 0; e < s.size(); ++e) {
        if (same(s.nameOf(e), "COUNTDOWNBOX")) s.setHidden(e, true);
    }
    m_answer = -1;
    if (auto* view = dynamic_cast<scene::View3d*>(s.view("FRAME_CAR3D"))) view->setMode(scene::MoveMode::Map, nullptr);
}

// 0x0045F150: OptionsBox, CarSel_Colour_Text and CarSel_ColourArrows shown; the car of the group's place locked off
// line: Locked and Locked_Text shown, else hidden; with the parts' buttons (+0x48) CarSel_AutoMod(_Text) and
// CarSel_ClearMod(_Text) shown, else hidden.
void InstantSelCar::showOptions(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    s.show("OptionsBox", false);
    s.show("CarSel_Colour_Text", false);
    s.show("CarSel_ColourArrows", false);
    const bool locked = data != nullptr && network() == nullptr &&
                        carLocked(data->arcade, carAt(data->arcadeGroups, m_group, m_place));
    for (const char* name : {"Locked", "Locked_Text"}) {
        if (locked) {
            s.show(name, false);
        } else {
            for (int e = 0; e < s.size(); ++e) {
                if (same(s.nameOf(e), name)) s.setHidden(e, true);
            }
        }
    }
    for (const char* name : {"CarSel_AutoMod_Text", "CarSel_AutoMod", "CarSel_ClearMod_Text", "CarSel_ClearMod"}) {
        if (m_options) {
            s.show(name, false);
        } else {
            for (int e = 0; e < s.size(); ++e) {
                if (same(s.nameOf(e), name)) s.setHidden(e, true);
            }
        }
    }
}

// 0x0045A8A0 on the car shown, while the view loads nothing: clear (0x004600C0) the parts, else the automatic
// modification (0x0048F2C0) as a prototype (any level) or of level 2; the model's parts again.
void InstantSelCar::setParts(CustomRaceHost& host, int clear, int prototype) {
    GameData* data = host.gameData();
    if (data == nullptr || m_car >= data->arcadeCars.size()) return;
    if (!m_view || m_view->busy() || !m_view->hasModel()) return;
    GameData& d = *data;
    Car& car = d.arcadeCars[m_car];
    if (clear != 0) {
        clearParts(car);
    } else {
        CarMaking making{d.defs, d.catalogue, gameRandom(), d.autoModifyCounter};
        autoModify(car, prototype != 0, prototype != 0 ? -1 : 2, making);
    }
    m_view->refit(car, d.defs);
}

// 0x0045F340. The question's answer (+0x54): yes, the automatic modification (0x0045A8A0, level 2); no, a car not
// locked goes on (as SELECT's end); a locked one makes NO (the answer kept). The network's BACK answered yes (+0x50):
// the car, the group, its place and the car's record (0x0045FFA0) as they were at the enter, link_selcar. FRAME_CAR3D's
// +0x1A0 0 (OpenJuice's view keeps it 0). The view's frame and CARFADE's alpha 1 - its +0x4C. While the view loads the
// car again (+0x40) MODDING shows and the events wait. COUNTDOWN emptied (0x00471740). Events: UP / DOWN the next / the
// previous paint of the CarDefs' list, the car painted (0x0045A640: the dial colours of a random [DialColour], rand()),
// CLICK; LEFT / RIGHT the group's place before / after (0x0042A830, 0x0042A800), the arcade car of its model shown (its
// parts cleared without the buttons), SELECT, the options again; SELECT: a car not modified (its first four body parts
// 0; the fixed build's: and no neon) with the buttons and not locked asks "AUTO-MOD CAR?" (2014; CLICK); a locked car
// NO; else CLICK, the race's +0x54 the automatic box (0 for a sprint) and instant_seltype; event 6 (CLICK) the
// automatic modification, 7 (CLICK) the parts cleared, 8 with the prototypes (CLICK) a prototype modification; BACK
// (CLICK) instant_manu. Then the arcade car's place to the save system's +0x50; with the view not loading the car
// rated: BHP "<rating> bhp" (cut toward zero), DRIVE its drive (1830 ..), else both hidden.
void InstantSelCar::update(CustomRaceHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    GameData& d = *data;
    ArcadeCarGroups& groups = d.arcadeGroups;
    const auto locked = [&] { return carLocked(d.arcade, carAt(groups, m_group, m_place)); };
    const auto toSelType = [&] {
        d.race.gearbox =
            d.race.type == RaceType::Sprint ? 0 : static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
        host.request("instant_seltype", SwitchMode::Replace);
    };
    if (m_backAnswer == 1) {
        d.arcadeCar = static_cast<int32_t>(m_savedCar);
        groups.group = m_savedGroup;
        groups.index = m_savedPlace;
        if (m_saved && m_savedCar < d.arcadeCars.size()) restoreCar(d.arcadeCars[m_savedCar], *m_saved);
        host.request("link_selcar", SwitchMode::Replace);
        return;
    }
    m_backAnswer = -1;
    if (m_answer != -1) {
        if (m_answer == 1) {
            setParts(host, 0, 0);
        } else if (locked()) {
            host.sound("NO");
            return;
        } else {
            toSelType();
        }
        m_answer = -1;
    }
    if (m_view) {
        m_view->frame(host.frameTime(), gameRandom());
        if (m_fade) {
            if (const int e = s.find("CARFADE"); e >= 0) s.setAlpha(e, 1.0f - m_view->shown());
        }
    }
    // 0x0045F508: while the view's parts are changed and not put in (+0x40) MODDING comes in (once) and nothing more
    // is done this frame; after, it goes out (once).
    if (m_view && m_view->refitting()) {
        if (!m_modding) {
            s.showContaining("MODDING", true);
            m_modding = true;
        }
        return;
    }
    if (m_modding) {
        s.hideContaining("MODDING", true);
        m_modding = false;
    }
    host.netCountdown();
    if (host.netWait(net::SessionStage::Cars, true)) return;
    Car* car = m_car < d.arcadeCars.size() ? &d.arcadeCars[m_car] : nullptr;
    switch (event) {
    case input::kUp:
    case input::kDown: {
        const auto paints = static_cast<int32_t>(d.defs.paints.size());
        if (event == input::kUp) {
            if (++m_paint >= paints) m_paint = 0;
        } else if (--m_paint < 0) {
            m_paint = paints - 1;
        }
        if (car != nullptr && m_view && m_view->hasModel() && m_paint >= 0 && m_paint < paints) {
            const CarPaint& p = d.defs.paints[static_cast<size_t>(m_paint)];
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
            m_view->repaint(*car);
        }
        host.sound("CLICK");
        break;
    }
    case input::kLeft:
    case input::kRight: {
        if (event == input::kRight) {
            groups.next();
        } else {
            groups.previous();
        }
        m_place = groups.index;
        m_car = arcadeCarOf(d.arcadeCars, groups.current());
        if (m_car < d.arcadeCars.size()) {
            Car& shown = d.arcadeCars[m_car];
            if (!m_options) clearParts(shown);
            host.sound("SELECT");
            if (m_view) {
                m_view->show(shown, nullptr, m_car, std::bit_cast<float>(0x3F490FDBu), 0.0f, true, host.viewLoading());
            }
        }
        showOptions(host, s);
        break;
    }
    case input::kSelect: {
        if (network() != nullptr) {
            host.sound("CLICK");
            d.arcadeCar = static_cast<int32_t>(m_car);
            host.request("link_selcar", SwitchMode::Replace);
            break;
        }
        const bool plain = car != nullptr && unmodified(*car);
        if (m_view && m_view->hasModel() && plain && m_options && !locked()) {
            host.sound("CLICK");
            m_answer = -1;
            host.message(MessageType::YesNo, texts.at(kTextAutoMod), false, &m_answer, true);
        } else if (locked()) {
            host.sound("NO");
            return;
        } else {
            host.sound("CLICK");
            toSelType();
        }
        break;
    }
    case input::kSelect2:
        if (m_options) {
            host.sound("CLICK");
            setParts(host, 0, 0);
        }
        break;
    case input::kSelect3:
        if (m_options) {
            host.sound("CLICK");
            setParts(host, 1, 0);
        }
        break;
    case input::kSelect4:
        if (m_prototypes && m_options) {
            host.sound("CLICK");
            setParts(host, 0, 1);
        }
        break;
    case input::kBack:
        host.sound("CLICK");
        if (network() != nullptr) {
            host.message(MessageType::YesNo, texts.at(kTextBackOut) + L"?\n" + texts.at(kTextAreYouSure), false,
                         &m_backAnswer, true);
            break;
        }
        host.request("instant_manu", SwitchMode::Replace);
        break;
    default:
        break;
    }
    d.arcadeCar = static_cast<int32_t>(m_car);
    const bool loading = m_view && m_view->busy();
    if (!loading && m_car < d.arcadeCars.size()) {
        Car& shown = d.arcadeCars[m_car];
        rateCar(shown, shown.def != nullptr && d.dynamics ? d.dynamics->of(*shown.def) : nullptr, d.catalogue);
        s.setHidden("BHP", false);
        s.setText("BHP", toWide(std::format("{} ", x87::chop(std::bit_cast<float>(shown.rating)))) +
                             texts.at(kTextBhp));
        s.setHidden("drive", false);
        s.setText("drive", texts.at(kTextDrives + static_cast<size_t>(driveOf(shown, d.tables))));
    } else {
        s.setHidden("BHP", true);
        s.setHidden("drive", true);
    }
}

// 0x0045F100: the view freed (0x0045AE90, 0x0045A340); the network's car (+0x3C) none here.
void InstantSelCar::leave() {
    if (m_view) m_view->clear();
    m_view.reset();
}

}  // namespace oj::frontend
