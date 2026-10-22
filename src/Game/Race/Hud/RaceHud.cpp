// The race's HUD: its window and the race's map, entering hud_race, its texts (the frame is RaceHudUpdate.cpp, the
// career's parts RaceHudCareer.cpp, the crew's orders HudCrewOrders.cpp).

#include "Game/Race/Hud/RaceHud.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Career/Money.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Physics/CarBody.h"
#include "Core/Saves/SaveEntries.h"
#include "Render/Materials.h"
#include "Render/Model.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace oj {

using namespace hud;

namespace {

// The race map's camera: the height [0x0072539C] (held within 8000 .. 200000), back by [0x00725390] x it, its pitch
// [0x00725394]; the car markers' scale [0x00725398].
float g_mapHeight = 12000.0f;
constexpr float kMapBack = 1.75f;
constexpr float kMapPitch = -0.5f;
constexpr float kMarkerScale = 3.0f;

}  // namespace

RaceHud::RaceHud(render::IRenderDevice& device, const Language& language, const TextTable& texts)
    : m_screen(device, language), m_texts(texts), m_languageIndex(language.index) {}

bool RaceHud::load(const std::string& window, scene::Engine* engine) {
    if (engine != nullptr) {
        m_screen.setView3dFactory([engine](const ui::Element& e) -> std::unique_ptr<ui::View3d> {
            CrtRandom none;
            return std::make_unique<scene::View3d>(*engine, e, none);
        });
    }
    return m_screen.load(window);
}

void RaceHud::prepareMap(const HudRace& race, CrtRandom& random) {
    ui::Screen& s = m_screen;
    m_map3d = s.find("MAP3D");
    m_mapView = m_map3d >= 0 ? dynamic_cast<scene::View3d*>(s.view(s.nameOf(m_map3d))) : nullptr;
    if (m_map3d < 0 || m_mapReady) return;
    if (m_mapView == nullptr || m_mapView->addModel("UI_" + race.route + "_MAP", {}, random) < 0) {
        s.hideContaining("MAP");
        return;
    }
    // The markers from the last vehicle to the first with the mesh cache off ([G+0x1C]+0x14 0): each marker's mesh
    // and its material ui_map_car its own, taking its car's paint (the material found by its name, painted, then
    // let go of the materials' cache, vt+0x2C, so that the next marker makes another).
    m_mapView->engine().setMeshCaching(false);
    for (size_t k = race.cars.size(); k-- > 0;) {
        const int marker = m_mapView->addModel("ui_map_car", {0.0f, 1000000.0f, 0.0f, 0.0f}, random);
        if (marker < 0) continue;
        const RaceCar* car = race.cars[k];
        if (car == nullptr || car->carRecord == nullptr) continue;
        const uint32_t paint = car->carRecord->base;
        for (const auto& node : m_mapView->listedModel(marker)->meshNodes) {
            if (node->mesh == nullptr) continue;
            for (const render::Mesh::Part& part : node->mesh->parts()) {
                if (part.material != nullptr) part.material->setParam(0, &paint);
            }
        }
    }
    m_mapView->engine().setMeshCaching(true);
    m_mapReady = true;
    m_mapShown = race.settings != nullptr && race.settings->u32(save::Settings::kMap) != 0;
    if (m_mapShown) {
        s.showContaining("MAP", false);
    } else {
        s.hideContaining("MAP");
    }
}

// 0x0044A7A0: each vehicle's marker (from the last) 10 cm right and ahead of its car at a height of 10 cm, three
// times its size, turned as the car; the camera at the height, behind the first player's camera's car (P+0x0C, +0x08:
// in a watched race or on a replay's next car not the player's own) by 1.75 x the height along its heading, looking
// down by 0.5.
void RaceHud::moveMap(const HudRace& race) {
    const RaceCar* followed = race.cameraCar;
    if (m_mapView == nullptr || followed == nullptr || followed->body == nullptr) return;
    const Mat34 at = followed->body->modelMatrix();
    const float yaw = followed->body->angles[1];
    int index = 1;
    for (size_t k = race.cars.size(); k-- > 0; ++index) {
        const RaceCar* car = race.cars[k];
        if (car == nullptr || car->body == nullptr || index >= m_mapView->modelCount()) continue;
        const Mat34 m = car->body->modelMatrix();
        const Vec4& a = car->body->angles;
        m_mapView->placeModel(index, {m[3] + 10.0f, 10.0f, m[11] + 10.0f, 0.0f}, {-a[0], -a[1], -a[2], 0.0f},
                              kMarkerScale);
    }
    g_mapHeight = std::clamp(g_mapHeight, 8000.0f, 200000.0f);
    const float back = -(kMapBack * g_mapHeight);
    const double s = std::sin(static_cast<double>(yaw));
    const double c = std::cos(static_cast<double>(yaw));
    const float x = static_cast<float>(back * s) + static_cast<float>(c * 0.0);
    const float z = static_cast<float>(back * c) - static_cast<float>(s * 0.0);
    m_mapView->setCameraNow({at[3] + x, g_mapHeight, at[11] + z, 0.0f}, kMapPitch, -yaw);
}

const std::wstring& RaceHud::line(size_t index) const {
    return m_texts.at(index);
}

void RaceHud::text(int element, const std::string& narrow) {
    if (element >= 0) m_screen.setText(element, widen(narrow));
}

void RaceHud::text(int element, const std::wstring& wide) {
    if (element >= 0) m_screen.setText(element, wide);
}

// 0x004B0A50's lines: the place ("1st" .. "3rd", 37 ..) before each.
void RaceHud::netBoard(const std::array<std::wstring, 3>& lines, const std::array<bool, 3>& shown) {
    for (size_t i = 0; i < 3; ++i) {
        const int e = m_screen.find("mplay" + std::to_string(i + 1));
        if (e < 0 || !shown[i]) continue;
        m_screen.setHidden(e, false);
        m_screen.setText(e, std::to_wstring(i + 1) + line(kTextOrdinals + i) + L" " + lines[i]);
    }
}

void RaceHud::enter(const HudRace& race) {
    ui::Screen& s = m_screen;
    // 0x004495EE (0x004B09D0): mplay1 .. 3 hidden but in a network cruise.
    if (!race.networked || race.type != RaceType::Cruise) {
        for (const char* name : {"mplay1", "mplay2", "mplay3"}) {
            if (const int e = s.find(name); e >= 0) s.setHidden(e, true);
        }
    }
    m_lapRecord = race.lapRecord;
    // 0x00447746: with the statistics the route's world record (0x004EE0C0's: the one read, else asked for and -1),
    // without them -1.
    m_worldRecord = net::stats::gameStatistics() != nullptr ? net::stats::worldBest(race.routeIndex, false) : -1.0f;
    const bool mph = race.settings != nullptr && race.settings->u32(save::Settings::kMph) != 0;
    uint32_t dialColour = 0, dialTextColour = 0;
    for (size_t i = 0; i < race.players.size() && i < m_players.size(); ++i) {
        Player& p = m_players[i];
        const HudCar& hc = race.players[i];
        // The car's HUD colours (+0x30, +0x34), their alpha 255.
        dialColour = hc.record != nullptr ? (hc.record->dialColour | 0xFF000000u) : 0xFF000000u;
        dialTextColour = hc.record != nullptr ? (hc.record->dialTextColour | 0xFF000000u) : 0xFF000000u;
        const std::string n = std::to_string(i + 1);
        auto find = [&](const std::string& name) { return s.find(name); };
        p.revs = find("REVS" + n);
        p.speedo = find("SPEEDO" + n);
        p.speed = find("SPEED" + n);
        if (p.speed >= 0) s.setFontColour(p.speed, dialTextColour);
        p.mph = find("MPH" + n);
        if (p.mph >= 0) s.setText(p.mph, line(mph ? kTextMph : kTextKph));
        p.gear = find("GEAR" + n);
        if (p.gear >= 0) s.setFontColour(p.gear, dialTextColour);
        p.gearLight = find("GEARLIGHT" + n);
        p.pos = find("POS" + n);
        p.posTh = find("POSTH" + n);
        p.posNum = find("POSNUM" + n);
        p.lap = find("LAP" + n);
        p.lapNum = find("LAPNUM" + n);
        p.time = find("TIME" + n);
        // 0x005B1BF0: the time's fonts step by the screen's width x 12 / 640 (0x0071E5B8 x 12 x 0.0015625).
        const int advance =
            static_cast<int>(static_cast<float>(static_cast<int32_t>(s.width()) * 12) * 0.0015625f);
        if (p.time >= 0) {
            s.setFixedAdvance(p.time, advance);
            s.setFontColour(p.time, dialTextColour);
        }
        p.absLight = find("ABSLIGHT" + n);
        p.tcLight = find("TCLIGHT" + n);
        p.winner = find("WINNER" + n);
        if (p.winner >= 0) s.setHidden(p.winner, true);
        p.winnerBox = find("WINNERBOX" + n);
        if (p.winnerBox >= 0) s.setHidden(p.winnerBox, true);
        p.msg = find("MSG" + n);
        if (p.msg >= 0) {
            s.clearText(p.msg);
            // The larger of the two colours as numbers, the messages of the first player too.
            const uint32_t c = dialTextColour > dialColour ? dialTextColour : dialColour;
            s.setFontColour(p.msg, c);
            m_messages[0].colour = c;
            s.setHighlighted(p.msg, false);
        }
        if (const int glow = find("MAPGLOW"); glow >= 0) s.setColour(glow, dialColour);
        if (p.revs >= 0) s.setNeedleColour(p.revs, dialTextColour > dialColour ? dialTextColour : dialColour);
        p.revsBg = find("REVSBG" + n);
        if (p.revsBg >= 0) s.setColour(p.revsBg, dialColour);
        p.speedoBg = find("SPEEDOBG" + n);
        if (p.speedoBg >= 0) s.setColour(p.speedoBg, dialColour);
        p.revsBgAdditive = find("REVSBG_ADDITIVE" + n);
        if (p.revsBgAdditive >= 0) s.setColour(p.revsBgAdditive, dialColour);
        p.speedoBgAdditive = find("SPEEDOBG_ADDITIVE" + n);
        if (p.speedoBgAdditive >= 0) s.setColour(p.speedoBgAdditive, dialColour);
        p.revsOpaque = find("REVSOPAQUE" + n);
        if (p.revsOpaque >= 0) s.setColour(p.revsOpaque, dialColour);
        p.speedoOpaque = find("SPEEDOOPAQUE" + n);
        if (p.speedoOpaque >= 0) s.setColour(p.speedoOpaque, dialColour);
        p.nosOpaque = find("NOSOPAQUE" + n);
        if (p.nosOpaque >= 0) s.setColour(p.nosOpaque, dialColour);
        p.countdown = find("COUNTDOWN" + n);
        if (p.countdown >= 0) s.setHidden(p.countdown, true);
        p.driverAid = find("DRIVERAID" + n);
        p.aidAlpha = 0.0f;
        p.aidBrake = false;
        p.aidFlash = false;
        // The curve warning: a Box of the HUD's own, yellow (0xFFFFFF00), hidden (made once).
        if (p.curveWarning < 0) {
            const bool split = race.mode == RaceMode::SplitScreen;
            const float x = split ? 0.445f : 0.45f;
            const float y = split ? (i == 0 ? 0.148f : 0.604f) : 0.185f;
            const float w = split ? 0.1f : 0.1f;
            const float h = split ? 0.077f : 0.133f;
            p.curveWarning = s.addBox("CURVEWARNING" + n, x, y, w, h);
            s.setColour(p.curveWarning, 0xFFFFFF00);
            s.setHidden(p.curveWarning, true);
            p.curveTimer = 0.0f;
            p.curveAlpha = 0.0f;
        }
        if (p.driverAid >= 0) s.moveToEnd(p.driverAid);
        if (p.msg >= 0) s.moveToEnd(p.msg);
        p.wrongWay = find("WRONGWAY" + n);
        if (p.wrongWay >= 0) s.setHidden(p.wrongWay, true);
        p.wrongWayFade = 0.0f;
        p.split = find("SPLIT" + n);
        if (p.split >= 0) s.moveToEnd(p.split);
        p.splitTimer = 0.0f;
        p.lrTime = find("LR_TIME" + n);
        if (p.lrTime >= 0) {
            s.setTextAlpha(p.lrTime, 1.0f);
            s.setHidden(p.lrTime, false);
            s.setFixedAdvance(p.lrTime, advance);
            s.setFontColour(p.lrTime, dialTextColour);
        }
        // The DIGITAL_BG_ boxes of the combo, the points, the best, the score, the bonus and the time share +0x13C
        // (the last found stays).
        for (const std::string& name : {"DIGITAL_BG_COMBO" + n, std::string("DIGITAL_BG_POINTS"),
                                         std::string("DIGITAL_BG_BEST"), "DIGITAL_BG_SCORE" + n,
                                         std::string("DIGITAL_BG_BONUS"), "DIGITAL_BG_TIME" + n}) {
            p.digitalBg = find(name);
            if (p.digitalBg >= 0) s.setColour(p.digitalBg, dialColour);
        }
        p.digitalRecord = find("DIGITAL_BG_RECORD" + n);
        if (p.digitalRecord >= 0) s.setColour(p.digitalRecord, dialColour);
        p.digitalWorld = find("DIGITAL_BG_WORLD" + n);
        if (p.digitalWorld >= 0) s.setColour(p.digitalWorld, dialColour);
        for (auto [field, name] : {std::pair{&p.timeBack, "TIME_BACK"}, std::pair{&p.lrTimeBack, "LR_TIME_BACK"},
                                   std::pair{&p.wrTimeBack, "WR_TIME_BACK"}}) {
            *field = find(name + n);
            if (*field >= 0) {
                s.setFontColour(*field, dialTextColour);
                s.setHidden(*field, true);
            }
        }
        p.fade = keepsPanels(race.type) ? kNeverFades : 3.0f;
        p.wrTime = find("WR_TIME" + n);
        if (p.wrTime >= 0) {
            s.setTextAlpha(p.wrTime, 1.0f);
            s.setFixedAdvance(p.wrTime, advance);
            s.setFontColour(p.wrTime, dialTextColour);
        }
        const bool world = !(m_worldRecord < 0.0f);
        if (p.wrTime >= 0) s.setHidden(p.wrTime, !world);
        if (p.digitalWorld >= 0) s.setHidden(p.digitalWorld, !world);
        // The player's best lap of the race so far takes the records when better.
        if (hc.car != nullptr && hc.car->competitor != nullptr) {
            const float best = hc.car->competitor->bestLap;
            if (best > 0.0f) {
                if (best < m_worldRecord) {
                    m_worldRecord = best;
                    net::stats::worldBestTime() = best;  // 0x00448FF7
                }
                if (best < m_lapRecord) m_lapRecord = best;
            }
        }
        p.lrLapTime = find("LR_LAPTIME" + n);
        if (p.lrLapTime >= 0) {
            s.setTextAlpha(p.lrLapTime, 1.0f);
            s.setHidden(p.lrLapTime, false);
        }
        p.wrLapTime = find("WR_LAPTIME" + n);
        if (p.wrLapTime >= 0) {
            if (m_worldRecord < 0.0f) {
                s.setHidden(p.wrLapTime, true);
            } else {
                s.setTextAlpha(p.wrLapTime, 1.0f);
                s.setHidden(p.wrLapTime, false);
            }
        }
        p.topSpeed = -1.0f;
        // Without a nitro part the meter of the player goes and its background with it.
        if (hc.car != nullptr && hc.car->hasNitro) {
            p.nosMeter = find("NOSMETER" + n);
        } else {
            p.nosMeter = -1;
            for (const char* name : i == 0 ? std::array{"NOSBG", "NOSMETER1"} : std::array{"NOSBG2", "NOSMETER2"}) {
                for (int k = 0; k < static_cast<int>(s.size()); ++k) {
                    if (_stricmp(s.nameOf(k).c_str(), name) == 0) s.setHidden(k, true);
                }
            }
        }
    }
    // The arrows of the rivals behind (made once): Boxes of ui_behindyou, yellow (0xFFFFFF00), hidden.
    for (size_t k = 0; k < m_rivals.size(); ++k) {
        if (m_rivals[k] >= 0) continue;
        m_rivals[k] = s.addBox("RIVAL" + std::to_string(k), 0.0f, 0.881f, 0.094f, 0.135f);
        s.setTexture(m_rivals[k], "ui_behindyou");
        s.setColour(m_rivals[k], 0xFFFFFF00);
        s.setHidden(m_rivals[k], true);
    }
    m_countdown = race.race != nullptr ? static_cast<int32_t>(race.race->count) : 0;
    m_countdownDone = m_countdown == 0;
    const HudCar* last = race.players.empty() ? nullptr : &race.players.back();
    dialTextColour = last != nullptr && last->record != nullptr ? last->record->dialTextColour : 0;
    m_score = s.find("score");
    m_combo = s.find("combo");
    m_regionMessage = s.find("regionmessage");
    m_menu = s.find("Menu");
    m_menuPoints = s.find("MenuPoints");
    m_points = s.find("Points");
    if (m_points >= 0) s.setFontColour(m_points, dialTextColour);
    m_best = s.find("best");
    m_dist = s.find("dist");
    m_revMeter = s.find("REV_METER");
    m_carAhead = s.find("CARAHEAD");
    m_carBehind = s.find("CARBEHIND");
    m_carAheadTimer = 0.0f;
    m_carBehindTimer = 0.0f;
    m_respect = s.find("RESPECT");
    m_respectIcon = s.find("RESPECT_ICON");
    m_respectText = s.find("RESPECT_TEXT");
    if (race.type == RaceType::Speed) {
        if (const int bg = s.find("SPEEDCHALLENGEBG"); bg >= 0 && last != nullptr && last->record != nullptr) {
            s.setColour(bg, last->record->dialColour);
        }
        m_speedChallenge = s.find("SPEEDCHALLENGE");
        if (m_speedChallenge >= 0) s.setFontColour(m_speedChallenge, dialTextColour);
    } else {
        s.hideContaining("SPEEDCHALLENGE");
        m_speedChallenge = -1;
    }
    if (m_regionMessage >= 0) s.setFontColour(m_regionMessage, dialTextColour);
    m_topScore = s.find("TOPSCORE");
    if (m_topScore >= 0) {
        s.setFontColour(m_topScore, dialTextColour);
        s.setTextAlpha(m_topScore, 1.0f);
    }
    m_topScoreTitle = s.find("TOPSCORETITLE");
    m_wrTopScore = s.find("WR_TOPSCORE");
    if (m_wrTopScore >= 0) {
        s.setFontColour(m_wrTopScore, dialTextColour);
        s.setTextAlpha(m_wrTopScore, 1.0f);
    }
    m_wrTopScoreTitle = s.find("WR_TOPSCORETITLE");
    if (!(m_worldRecord >= 0.0f)) {
        if (m_wrTopScore >= 0) s.setHidden(m_wrTopScore, true);
        if (m_wrTopScoreTitle >= 0) s.setHidden(m_wrTopScoreTitle, true);
    }
    const int advance = static_cast<int>(static_cast<float>(static_cast<int32_t>(s.width()) * 12) * 0.0015625f);
    for (int e : {m_best, m_score, m_combo}) {
        if (e < 0) continue;
        s.setFixedAdvance(e, advance);
        s.setFontColour(e, dialTextColour);
    }
    // The challenges' targets in the first player's local record (0x00449C77): the speed challenge's in miles an hour,
    // or x 1.6093 ([0x006FFE94]) in kilometres an hour by the settings' units, cut.
    const int lrTime = m_players[0].lrTime;
    if (race.type == RaceType::Speed) {
        const float speed = race.challengeSpeed;
        const bool mphUnits = race.settings != nullptr && race.settings->u32(save::Settings::kMph) != 0;
        const int shown = static_cast<int>(mphUnits ? speed : speed * 1.6093f);
        if (lrTime >= 0) s.setText(lrTime, widen(format(" %d ", shown)) + line(mphUnits ? kTextMph : kTextKph));
        if (const int target = s.find("LR_LAPTIME1"); target >= 0) s.setText(target, line(kTextTarget));
    } else if (race.type == RaceType::TimedLap || race.type == RaceType::PerfectLap) {
        if (lrTime >= 0) text(lrTime, hudTime(race.challengeLapTime));
        if (const int target = s.find("LR_LAPTIME1"); target >= 0) s.setText(target, line(kTextTarget));
    }
    // The turbo's dial: with a turbo, or one lost to damage; else gone.
    const RaceCar* lastCar = last != nullptr ? last->car : nullptr;
    const bool turboLost = last != nullptr && last->record != nullptr && last->record->turboLost != 0;
    if ((lastCar != nullptr && lastCar->hasTurbo) || turboLost) {
        m_turbo = s.find("TURBO1");
        if (m_turbo >= 0) s.setNeedleColour(m_turbo, 0xFFFF5050);
    } else {
        m_turbo = -1;
        s.hideContaining("TURBO1");
    }
    for (int e : {m_respect, m_respectIcon, m_respectText}) {
        if (e >= 0) s.setHidden(e, true);
    }
    s.hideContaining("DONTDRAW");
    // 0x004BDA30: the crew's orders and the cars of the crew. Outside the career the orders are hidden; in the career
    // RP_ elements show while the race's cars are not chosen.
    if (race.mode == RaceMode::Career && !race.raceCarsChosen) {
        s.showContaining("RP_", true);
    } else {
        s.hideContaining("RP_");
    }
    m_orders.enter(race);
    // A sprint (0x00449FE7): "HEAT n / heats" a message of 4 s to the first player; its LR_TIME and LR_LAPTIME
    // shown. (The text's string, 0x004095E0 as every string made, counts in [0x0075F04C] with a debug break at
    // [0x007191DC]; strings here are the language's own.)
    if (race.type == RaceType::Sprint) {
        m_messages[0].push(line(kTextHeat) + widen(std::format(" {} / {}", race.heat + 1, race.heats)), 4.0f);
        for (const int e : {m_players[0].lrTime, m_players[0].lrLapTime}) {
            if (e >= 0) s.setHidden(e, false);
        }
    }
}

// The window manager steps the shown window's transitions (0x005B56B0) before it draws it.
void RaceHud::draw(float seconds, float dt) {
    m_screen.update(dt);
    m_screen.draw(seconds, dt);
}

void RaceHud::leave() {
    m_orders.leave();
    for (HudMessages& m : m_messages) m.clear();
}

}  // namespace oj
