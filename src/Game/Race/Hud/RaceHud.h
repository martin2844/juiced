#pragma once
// The race's HUD (docs/spec/22-race-hud.md): the window state of hud_race and the other hud_* windows (0x0044D9F0,
// vtable 0x006F1438) as it enters (0x004476D0) and as it follows the race each frame (0x0044B1C0).

#include "Core/Base/Random.h"
#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudCrewOrders.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oj {

class RaceHud {
public:
    RaceHud(render::IRenderDevice& device, const Language& language, const TextTable& texts);

    // ui\windows\<name>.ui, its 3D elements as views of the engine (the race front end's windows, 0x004563E0).
    bool load(const std::string& window, scene::Engine* engine = nullptr);
    // 0x0044A100 as the window loads: the MAP3D element's models, the route's map at the origin and a car marker
    // for each vehicle (loaded from the last, with the loading thread's rand()); without the map every element
    // with MAP in its name is hidden; the map's setting taken.
    void prepareMap(const HudRace& race, CrtRandom& random);
    const std::string& window() const { return m_screen.name(); }
    // 0x004476D0
    void enter(const HudRace& race);
    // 0x0044D0BD .. 0x0044D329: the cruise's list (0x004ABE70), score, multiplier (0x004AC390) and zone.
    void cruise(const HudRace& race);
    // 0x0044B1C0 (the players' messages run down in the race front end's frame, 0x00456300: TrackView).
    void update(const HudRace& race, float dt);
    // 0x0044A450: the window let go (+0x18); the crew's orders' window and lines (the race drivers' +0x38 .. +0x5C)
    // cleared; each player's messages emptied.
    void leave();
    // The crew's orders in the drivers' levels' frame (0x004BDF00, the race's frame unless paused; a career race
    // that is not a cruise or a replay): the flash toggles each frame; with the player's crew racing with members
    // (the race drivers' +0x10) the members' lines are shown (0x004BE720) and the tactics keys change their levels
    // (0x004BE340). tactics: DRIVER_TACTICS_UP, _RIGHT, _DOWN, _LEFT, 3_UP, 3_DOWN went down.
    void crewOrders(const HudRace& race, const std::array<bool, 6>& tactics, float dt) {
        m_orders.frame(race, tactics, dt);
    }
    void draw(float seconds, float dt);

    ui::Screen& screen() { return m_screen; }
    HudMessages& messages(size_t player) { return m_messages[player]; }
    // 0x004B0A50 into the window's mplay1 .. 3: each line shown (with the text's effect) with its text, none left as
    // it is.
    void netBoard(const std::array<std::wstring, 3>& lines, const std::array<bool, 3>& shown);

private:
    struct Player {
        int revs = -1, speedo = -1, speed = -1, mph = -1, gear = -1, gearLight = -1;   // +0x84 .. +0xDC
        int pos = -1, posTh = -1, posNum = -1, lap = -1, lapNum = -1, time = -1;
        int absLight = -1, tcLight = -1, winner = -1, winnerBox = -1, msg = -1;
        int revsBg = -1, speedoBg = -1, revsBgAdditive = -1, speedoBgAdditive = -1;
        int revsOpaque = -1, speedoOpaque = -1, nosOpaque = -1, countdown = -1;
        int driverAid = -1, curveWarning = -1, wrongWay = -1, split = -1;
        int lrTime = -1, wrTime = -1, digitalBg = -1, digitalRecord = -1, digitalWorld = -1;
        int timeBack = -1, lrTimeBack = -1, wrTimeBack = -1, lrLapTime = -1, wrLapTime = -1, nosMeter = -1;
        float wrongWayFade = 0.0f;  // +0x124
        float splitTimer = 0.0f;    // +0x134
        float topSpeed = -1.0f;     // +0x1A4: the speedometer's end (0x004F7460), once
        float fade = 0.0f;          // +0x1AC: the records' panels fade out below 1 and in above 3
        float aidAlpha = 0.0f;      // +0x1F0: the driver aid's text
        float curveTimer = 0.0f;    // +0x1F8: the curve warning's
        float curveAlpha = 0.0f;    // +0x200
        bool aidBrake = false;      // +0x208: BRAKE NOW! given
        bool aidFlash = false;      // +0x210
    };

    void fadePanels(Player& p, float alpha);
    void respect(const HudRace& race, float dt);  // 0x004BCB60
    void respectNews(Career& career, const HudRace& race, const std::vector<const RaceCar*>& cars,
                     float dt);  // 0x004BD130
    void rivalsAndAid(const HudRace& race, Player& p, size_t index, const RaceCar& car, float dt);  // 0x0044AA80
    void raceNews(const HudRace& race, const RaceCar& car);                                       // 0x0044A4C0
    void moveMap(const HudRace& race);                                                            // 0x0044A7A0
    void text(int element, const std::string& narrow);
    void text(int element, const std::wstring& wide);
    const std::wstring& line(size_t index) const;

    ui::Screen m_screen;
    const TextTable& m_texts;
    std::array<Player, 2> m_players{};
    std::array<HudMessages, 2> m_messages{};
    bool m_countdownDone = false;   // +0x10
    int32_t m_countdown = 0;        // +0x14: the next number of the count shown
    int m_score = -1, m_combo = -1, m_regionMessage = -1, m_menu = -1, m_menuPoints = -1, m_points = -1;  // +0x1C ..
    static inline float m_regionBlink = 0.5f;  // [0x00725368]
    float m_frameTime = 0.0f;    // the game's frame time of the update
    int m_languageIndex = 7;
    int m_best = -1, m_dist = -1, m_carAhead = -1, m_carBehind = -1;        // +0x34, +0x38, +0x3C, +0x40
    int m_topScore = -1, m_topScoreTitle = -1, m_wrTopScore = -1, m_wrTopScoreTitle = -1;  // +0x44 .. +0x50
    int m_respect = -1, m_respectIcon = -1, m_respectText = -1;            // +0x54, +0x58, +0x5C
    float m_carAheadTimer = 0.0f;   // +0x60
    float m_carBehindTimer = 0.0f;  // +0x64
    float m_lapRecord = 0.0f;       // +0x68: the route's
    float m_worldRecord = -1.0f;    // +0x6C: none without the online records
    int m_speedChallenge = -1;      // +0x230
    int m_turbo = -1;               // +0x234
    int m_revMeter = -1;            // +0x23C
    bool m_mapShown = false;        // +0x244: the map's setting as last applied
    int m_map3d = -1;               // +0x240: MAP3D
    scene::View3d* m_mapView = nullptr;
    bool m_mapReady = false;        // +0x248
    std::array<int, 5> m_rivals{-1, -1, -1, -1, -1};  // +0x1CC
    HudCrewOrders m_orders{m_screen, m_texts};
};

}  // namespace oj
