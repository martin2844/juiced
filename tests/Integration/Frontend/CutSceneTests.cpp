// Integration tests: the garage's cut scene CMSCutScene on a career of the game's data, the game's window, a car of
// the dealer's, an engine without a device and the sound system (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Audio/SoundSystem.h"
#include "Audio/Voices.h"
#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Game/Frontend/Career/CutScene.h"
#include "Game/Media/Music.h"
#include "Game/Scene/Scene3d.h"

#include <windows.h>

#include <memory>
#include <string>

namespace {

using oj::test::check;

// The career's front end with an engine for the window's 3D views.
class EngineHost : public oj::test::FakeCareerHost {
public:
    explicit EngineHost(oj::scene::Engine& engine) : m_engine(engine) {}
    oj::scene::Engine* engine() override { return &m_engine; }

private:
    oj::scene::Engine& m_engine;
};

}  // namespace

// The camera's third shot (0x004C7870) pauses the scene's 3D sounds ([G+0x50]'s manager) in its last second, as it
// fades to black: the engines go quiet with the black, not only when the window is left.
OJ_INTEGRATION_TEST(CutScene, LastShotPausesTheSounds) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "game data and a first career loaded: " + error);
        return;
    }
    data.makeDealerCars(random);
    const int32_t at = data.dealerTake(random, 0);
    if (at < 0) {
        check(false, "a dealer's car taken to the garage");
        return;
    }
    data.career->profile.raceChoices[0] = at;
    data.career->profile.raceChoiceCount = 1;
    data.careerTestTrack = 1;
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    EngineHost host(engine);
    host.data = &data;
    host.dt = 0.25f;
    auto screen = std::make_shared<oj::ui::Screen>(device, host.lang);
    screen->setView3dFactory([&](const oj::ui::Element& e) -> std::unique_ptr<oj::ui::View3d> {
        return std::make_unique<oj::scene::View3d>(engine, e, random);
    });
    if (!screen->load("CMSCutScene")) {
        check(false, "cmscutscene.ui read");
        return;
    }
    oj::audio::System& sound = oj::audio::system();
    if (!sound.start(GetDesktopWindow(), 0x20)) {
        check(false, "the sound system started (the tests need a DirectSound device)");
        return;
    }
    oj::frontend::CutScene scene;
    scene.enter(host, *screen);
    oj::audio::World* const world = oj::gameSounds().world;
    if (world == nullptr) {
        check(false, "the scene's 3D sounds made");
        scene.leave(*screen);
        sound.stop();
        return;
    }
    // SELECT before the third shot ends the shot at once: the first frame starts shot 1, two SELECTs shot 3 (8 s).
    scene.update(host, *screen, 0);
    scene.update(host, *screen, oj::input::kSelect);
    scene.update(host, *screen, 0);
    scene.update(host, *screen, oj::input::kSelect);
    scene.update(host, *screen, 0);
    bool early = false;
    for (int frame = 0; frame < 26; ++frame) {  // 6.5 s of the shot: more than a second left
        scene.update(host, *screen, 0);
        early = early || world->manager().paused();
    }
    for (int frame = 0; frame < 4; ++frame) scene.update(host, *screen, 0);  // into the last second
    const bool late = world->manager().paused();
    scene.leave(*screen);
    oj::freeWindowWorld();
    sound.stop();
    check(!early, "the scene's sounds playing while the third shot has more than a second left");
    check(late, "the scene's sounds paused in the third shot's last second");
}
