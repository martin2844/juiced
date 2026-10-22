// CMSRespect: the crews' territories on the city's map.

#include "Game/Frontend/Career/Respect.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Core/Saves/Save.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// The narration of the overview and each crew (0x00719080, 16 bytes each).
constexpr const char* kRespectNarration[] = {"narrAC", "narrSR",  "narrCH", "narrDT", "narrANC",
                                             "narrAWS", "narrSB", "narrEAI", "narrWA"};

// The territories' models (0x00719110, 0x14 bytes each) by the crew; crew 5's is West Anderson's too.
constexpr const char* kTerritories[9] = {"ui_westanderson", "ui_sanricardo", "ui_campbellhills",
                                         "ui_downtown",     "ui_angelnorth", "ui_westanderson",
                                         "ui_southside",    "ui_eastangel",  "ui_engstrom"};
// The points the map looks at for the overview and each crew (0x00722840, 16 bytes each).
constexpr oj::Vec4 kRespectPoints[9] = {
    {151209.0f, 9000.0f, -306204.0f, 0.0f}, {280589.0f, 9000.0f, -253165.0f, 0.0f},
    {355401.0f, 9000.0f, -409405.0f, 0.0f}, {161209.0f, 9000.0f, -146204.0f, 0.0f},
    {-152652.0f, 9000.0f, -2689.0f, 0.0f},  {-104294.0f, 9000.0f, -166310.0f, 0.0f},
    {-85101.0f, 9000.0f, -314828.0f, 0.0f}, {115305.0f, 9000.0f, -454087.0f, 0.0f},
    {144842.0f, 9000.0f, -298442.0f, 0.0f}};

}  // namespace

// CMSRespect (0x004DB2F0): [game + 4]'s method +0x34 with 1; MAP3D (+0x38) and its camera's range 3999 .. 5000000,
// its sky (0x005B4660); Menu emptied, "OVERVIEW" (532, value 0) and the crews 1 .. 8 (118 .., values 1 .. 8); +0x0C
// and +0x10 -1, +0x70 0; the elements whose name holds DATA hidden; the map's territories (territories); Cash
// and Date; the front end's music at a quarter; +0x3C and +0x40 0, +0x44 the settings' speech volume.
void Respect::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    m_map = s.find("MAP3D") >= 0;
    if (auto* view = dynamic_cast<scene::View3d*>(s.view("MAP3D")); view != nullptr) {
        view->camera().setNear(3999.0f);  // 0x4579F000 (0x004DB332)
        view->camera().setFar(5000000.0f);
        view->buildSky();
    }
    s.clearMenu("Menu");
    s.addMenuItem("Menu", texts.at(kTextOverview), 0.0f);
    for (size_t n = 1; n < 9; ++n) {
        s.addMenuItem("Menu", texts.at(kTextCrewNamesUpper + n), static_cast<float>(n));
    }
    m_crew = -1;
    m_shown = -1;
    m_dataShown = 0;
    s.hideContaining("DATA");
    territories(host, s);
    careerCashTexts(s, *data->career, host.language().index, texts);
    musicQuarter(host);
    m_timer = 0.0f;
    m_narration = std::bit_cast<float>(host.settingsValue(save::Settings::kSpeechVolume));
}

// 0x004DB4B0: Menu emptied down to one item (0x005B42A0); a line the speech says (0x0040B5A0) stopped as 0x0040B4A0
// does (its player's handle stopped, the system stepped, nothing waiting; the handle kept); the front end's music
// back; with the interface's manager the speech's master back to +0x44; [game + 4]'s method +0x34 with 0.
void Respect::leave(CareerHost& host) {
    Speech* speech = gameSounds().speech;
    if (speech != nullptr && speech->speaking()) {
        if (speech->handle != 0) {
            if (audio::Player* player = speech->player()) player->stop(speech->handle);
            audio::system().update();
        }
        speech->waiting = false;
    }
    host.setMusicVolume(host.settingsMusic(), false);
    if (speech != nullptr && speech->player() != nullptr) speech->player()->setMasterVolume(m_narration);
}

// 0x004DBE60: each crew's territory added to MAP3D's list at (0, 0, 0) (0x005B3E60) and hidden (+0xBC | 4); the list's
// entry after the element's own model for it (counted as if every one before it were added) placed at the height
// 2000 (4000 for the seventh); its material (the model's +0xA8, its first) of Coloured Spec ABlend takes the colour
// 0x33FF0000 less n x 0xFF00 for n = 255 x the crew's respect (+0x70, at most 1000) x 0.001 cut (red to green),
// Additive 1, Specular Highlight and Shininess 0.5.
void Respect::territories(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    m_territories.fill(-1);
    auto* view = dynamic_cast<scene::View3d*>(s.view("MAP3D"));
    if (view == nullptr) return;
    const Career& career = *data->career;
    for (size_t k = 0; k < 9; ++k) {
        const int added = view->addModel(kTerritories[k], {0.0f, 0.0f, 0.0f, 0.0f}, host.viewLoading());
        m_territories[k] = added;
        if (added < 0) continue;
        view->listedModel(added)->setHidden(true);
        // Stock problem: the place goes to the list's entry k + 1, another model's when one before was not added.
        if (static_cast<int>(k) < view->modelCount()) {
            scene::Model& placed = *view->listedModel(static_cast<int>(k));
            if (placed.root != nullptr) placed.root->setPosition(0.0f, k == 6 ? 4000.0f : 2000.0f, 0.0f);
        }
        float respect = static_cast<float>(career.crews[k].respect);
        if (!(respect < 1000.0f)) respect = 1000.0f;
        const auto n = static_cast<uint32_t>(x87::chop(respect * 255.0f * 0.001f));
        const uint32_t argb = 0x33FF0000u - n * 0xFF00u;
        const int32_t one = 1;
        const float half = 0.5f;
        for (auto& node : view->listedModel(added)->meshNodes) {
            if (node->mesh == nullptr) continue;
            for (const auto& part : node->mesh->parts()) {
                if (part.material == nullptr) continue;
                part.material->setParam(0, &argb);
                part.material->setParam(4, &one);
                part.material->setParam(2, &half);
                part.material->setParam(3, &half);
            }
        }
    }
}

// 0x004DB610 with the map: a crew other than the one shown (+0x10): +0x3C 0, the speech's master 0 and a line it says
// stopped (as 0x0040B4A0, the system stepped again after),
// the shown crew kept, the view out of and back into the map mode on an arm of (0, 0, -400000) for the overview, else
// -300000, the pitch within -1.5 .. -0.4. Then the view turns to the crew's point (0x00722840 + 16 x crew), the texts
// (0x004DBA80) and the territories lit (0x004DC050).
void Respect::mapTo(CareerHost& host, ui::Screen& s) {
    auto* view = dynamic_cast<scene::View3d*>(s.view("MAP3D"));
    if (view == nullptr) return;
    if (m_crew != m_shown) {
        m_timer = 0.0f;
        Speech* speech = gameSounds().speech;
        if (speech != nullptr && speech->player() != nullptr) speech->player()->setMasterVolume(0.0f);
        if (speech != nullptr && speech->speaking()) {
            log::info("frontend: Stopping sound");
            if (speech->handle != 0) {
                if (audio::Player* player = speech->player()) player->stop(speech->handle);
                audio::system().update();
            }
            speech->waiting = false;
        }
        if (audio::system().on()) audio::system().update();
        m_shown = m_crew;
        const oj::Vec4 arm{0.0f, 0.0f, m_crew != 0 ? -300000.0f : -400000.0f, 0.0f};
        view->setMode(scene::MoveMode::ModelAuto, nullptr);
        view->setMode(scene::MoveMode::Map, &arm);
        view->pitchLimits[0] = -1.5f;
        view->pitchLimits[1] = -0.4f;
    }
    // Stock problem: a crew past 8 reads past the points; none here.
    if (m_crew >= 0 && m_crew < 9) view->lookAt(kRespectPoints[static_cast<size_t>(m_crew)], false);
    crewTexts(host, s);
    light(host, s);
}

// 0x004DC050: for a crew only its territory shown, every other hidden; for the overview (0) all nine shown.
void Respect::light(CareerHost& /*host*/, ui::Screen& s) {
    auto* view = dynamic_cast<scene::View3d*>(s.view("MAP3D"));
    if (view == nullptr) return;
    for (size_t k = 0; k < 9; ++k) {
        if (m_territories[k] < 0) continue;
        view->listedModel(m_territories[k])->setHidden(m_crew != 0 && static_cast<int32_t>(k) != m_crew);
    }
}

// 0x004DBA80: for a crew (not the overview) data_logo its flyer's logo, data_location_val the location of its first
// circuit's route (575 + it), data_leader_val the leader's name, data_respect_val its respect (thousands marked),
// data_respect_meter its respect / 1500 (at most 1), data_bio_val1 .. 3 over several lines its bio (724, 732 and 740 +
// the crew's number), data_respects_val what it respects most (297 + 0x004BC110), data_chall1 .. 3 ui_yes for a
// challenge done, else ui_no.
void Respect::crewTexts(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const Career& career = *data->career;
    const int32_t crew = m_crew;
    if (crew <= 0 || crew >= 9) return;
    const Crew& c = career.crews[static_cast<size_t>(crew)];
    const auto id = static_cast<size_t>(c.id);
    if (id < career.flyers.crews.size()) s.setTexture("data_logo", career.flyers.crews[id].logo);
    const std::vector<ExeTables::Route>& routes = data->tables.routes;
    const auto route = static_cast<size_t>(c.circuitRoute[0]);
    const int location = route < routes.size() ? routes[route].location : 0;
    s.setText("data_location_val", texts.at(kTextLocations + static_cast<size_t>(std::max(location, 0))));
    s.setText("data_leader_val", widenSigned(std::string_view(c.memberNames[0].data(), c.memberNames[0].size())));
    s.setText("data_respect_val", toWide(groupThousands(c.respect, host.language().index)));
    s.setMeter("data_respect_meter", std::min(1.0f, static_cast<float>(c.respect) * 0.00066666666f));
    for (size_t k = 0; k < 3; ++k) {
        const std::string name = std::format("data_bio_val{}", k + 1);
        s.setMultiline(name, true);
        s.setText(name, texts.at(kTextCrewBios + 8 * k + id));
    }
    s.setText("data_respects_val", texts.at(kTextRespected + static_cast<size_t>(respectedMost(c))));
    for (size_t k = 0; k < 3; ++k) {
        s.setTexture(std::format("data_chall{}", k + 1), c.challenges[k].done != 0 ? "ui_yes" : "ui_no");
    }
}

// 0x004DB740: while +0x3C passes 4 s the crew's narration is asked for (0x00719080 by +0x0C: the speech player's
// +0x74 finds it into the line to say, +0x18 1, the sounds made quieter and the line said 0.3 s later, 0x0040B420;
// the speech's master +0x44; the system stepped) and the frame ends; +0x3C takes the frame time. BACK with the DATA panel hidden (CLICK):
// CMSStatsMenu (mode 3). For a crew, SELECT with the panel hidden or BACK with it shown (CLICK) turns it over: shown,
// the DATA elements come in (with their transitions) and sel_but, sel_text are hidden; hidden, the other way. With the
// map, a new value of Menu's highlighted item (+0x0C): the map goes to it (mapTo); the overview hides the
// panel (+0x70 0) and sel_but, sel_text; a crew with the panel hidden shows sel_but and sel_text; SELECT is played.
// Stock problem: without a speech object (no speech in the configuration) the narration writes through a null
// pointer and the game crashes; here nothing is said.
void Respect::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    if (!(m_timer >= 4.0f) && !(m_timer + host.frameTime() < 4.0f)) {
        const size_t narration = static_cast<size_t>(std::clamp(m_crew, 0, 8));
        log::info("frontend: Playing sound {}", kRespectNarration[narration]);
        if (Speech* speech = gameSounds().speech) {
            if (speech->player() != nullptr && !speech->paused) {
                speech->player()->find(kRespectNarration[narration], speech->pending);
            }
            speech->pendingVolume = 1.0f;
            speech->duck();
            if (speech->player() != nullptr) speech->player()->setMasterVolume(m_narration);
        }
        if (audio::system().on()) audio::system().update();
        m_timer = host.frameTime() + m_timer;
        return;
    }
    m_timer = host.frameTime() + m_timer;
    if (event == input::kBack && m_dataShown == 0) {
        host.sound("CLICK");
        host.request("CMSStatsMenu", SwitchMode::ReplaceAfterTransitions);
        return;
    }
    const auto selButtons = [&](bool hidden) {
        for (const char* name : {"sel_but", "sel_text"}) {
            if (hidden) {
                s.setHidden(name, true);
            } else {
                s.show(name, false);
            }
        }
    };
    if (m_crew != 0 && ((event == input::kSelect && m_dataShown == 0) || (event == input::kBack && m_dataShown != 0))) {
        host.sound("CLICK");
        m_dataShown = 1 - m_dataShown;
        if (m_dataShown == 0) {
            s.hideContaining("DATA", true);
            selButtons(false);
        } else {
            s.showContaining("DATA", true);
            selButtons(true);
        }
    }
    if (!m_map || s.menuSize("Menu") == 0) return;
    const int32_t value = x87::chop(s.menuItemValue("Menu", s.selectedMenuItem("Menu")));
    if (value == m_crew) return;
    m_crew = value;
    mapTo(host, s);
    if (m_crew == 0) {
        m_dataShown = 0;
        s.hideContaining("DATA", true);
        selButtons(true);
    } else if (m_dataShown == 0) {
        selButtons(false);
    }
    host.sound("SELECT");
}

}  // namespace oj::frontend
