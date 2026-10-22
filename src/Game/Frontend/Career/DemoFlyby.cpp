// DemoFlyby: the city's fly-by before a new career.

#include "Game/Frontend/Career/DemoFlyby.h"

#include "Core/Base/Log.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

// The map of the career (0x0045B6B0, 0x0045BE00): the MAP3D view's camera from 3999 to 5 000 000, the rig's pitch
// within -1.5 .. -0.15, the atmosphere "map"; the camera looks at the city place 4 and goes into the map mode on an
// arm of 2400 m, pitch -0.4 with the yaw it had; the rig turns 0.1 rad a second by itself and the stick waits 100 s.
// Then (0x0043514C) the front end's music at a fifth of the settings' volume under the narration.
void DemoFlyby::enter(CareerHost& host, ui::Screen& s) {
    // 0x00435090: the black FADE box over the map starts opaque.
    m_fade = 1.0f;
    m_narrationStarted = false;
    m_fadeOut = false;
    m_done = false;
    s.setBoxColour("FADE", 0xFF000000);
    GameData* data = host.gameData();
    host.setMusicVolume(std::bit_cast<float>(host.settingsValue(save::Settings::kMusicVolume)) * 0.2f, false);  // [0x006FF934]
    auto* view = dynamic_cast<scene::View3d*>(s.view("MAP3D"));
    if (view == nullptr) return;
    view->camera().setNear(3999.0f);  // 0x4579F000 (0x0045B6CC)
    view->camera().setFar(5000000.0f);
    view->buildSky();
    view->pitchLimits[0] = -1.5f;
    view->pitchLimits[1] = -0.15f;
    view->setAtmosphere("map");
    if (data != nullptr && data->tables.cityLocations.size() > 4) {
        const ExeTables::CityLocation& place = data->tables.cityLocations[4];
        view->lookAt({place.x, place.y, place.z, 0.0f}, false);
    }
    const float yaw = view->rig[1];
    const oj::Vec4 arm{0.0f, 0.0f, -240000.0f, 0.0f};
    view->setMode(scene::MoveMode::Map, &arm);
    view->rig[0] = -0.4f;
    view->rig[1] = yaw;
    view->rig[2] = 0.0f;
    view->rig[3] = 0.0f;
    view->autoSpeed = 0.1f;
    view->inputDelay = 100.0f;
}

// 0x004351C0, DemoFlyby's leave: the speech's line stopped (0x0040B600 inlined: a narration skipped with SELECT is
// cut) and the front end's music back to the settings' volume.
void DemoFlyby::leave(CareerHost& host) {
    if (Speech* speech = gameSounds().speech) speech->stop();
    host.setMusicVolume(std::bit_cast<float>(host.settingsValue(save::Settings::kMusicVolume)), false);
}

// The career's first sight of the city (0x004352B0): the FADE box clears over a second of the game's frame time (the
// alpha of its corners is the fade x 255, cut, 0x0042E560; after the narration started 1 - the fade). Once clear the
// narration "narrDemo" is said through the speech object [game + 0x4C] (its +0x1C); when it has ended (0x0040B5A0)
// the box goes black again over a second (+0x18), and once black the way into the career follows; SELECT takes it at
// once. Without the speech object nothing is said and the career follows a frame after the box cleared. Stock problem:
// without it the counter +0x20 (10, the frame time taken off and rounded each frame) never changes at frame times
// under half a second and is only compared below 0 as an unsigned number; it leads nowhere.
void DemoFlyby::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (m_fade > 0.0f) {
        m_fade = std::clamp(m_fade - host.gameFrameTime(), 0.0f, 1.0f);
        const float shown = m_narrationStarted ? 1.0f - m_fade : m_fade;
        const uint32_t alpha = static_cast<uint32_t>(static_cast<int32_t>(shown * 255.0f)) & 0xFF;
        s.setBoxColour("FADE", alpha << 24);
    }
    Speech* speech = gameSounds().speech;
    if (speech != nullptr && !m_fadeOut && m_narrationStarted && !speech->speaking()) {
        m_fade = 1.0f;
        m_fadeOut = true;
    }
    if (m_fade == 0.0f) {
        if (!m_narrationStarted) {
            if (speech != nullptr) speech->sayName("narrDemo");
            m_narrationStarted = true;
        } else if (speech == nullptr || !speech->speaking()) {
            m_done = true;
        }
    }
    if (event != input::kSelect && !m_done) {
        return;
    }
    // The way into the career (0x00435434): the music stops, race mode 2, a new career for the driver and the
    // first event's race (Core/Race/RaceSession), then careerload with the betting window next; the front end's +0x38
    // cleared (0x0043542B, its controllers no longer watched).
    host.sound("CLICK");
    host.watchControllers(false);
    if (data != nullptr) {
        const uint64_t before = gameRandom().draws();
        data->enterCareer(gameRandom(), crewName8(data->profileName));
        const RaceSession& session = data->career->session;
        log::info("frontend: into the career ({} rand() values), {} racers, bet with crew {}",
                  gameRandom().draws() - before, session.count, data->career->bets.crew);
    }
    host.request("CareerLoad", SwitchMode::Replace);
}

}  // namespace oj::frontend
