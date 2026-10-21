// The race's restart and end, and a car put back on the road.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/Log.h"

#include <bit>

namespace app {

void TrackView::restartRace() {
    replayBegun = false;
    if (rules.career != nullptr) rules.career->cruise.car = nullptr;
    if (race != nullptr && race->rules().type == RaceType::Sprint) ++race->rules().heat;
    for (std::unique_ptr<RaceSlot>& s : cars) resetCar(*s);
    settleRace(oj::gameRandom());
    // 0x0041E110 for each player: its director stopped (0x00408750) and its flags cleared (+0x18, +0x1C, +0x20).
    for (std::unique_ptr<RacePlayer>& p : players) {
        p->director.stop();
        p->finished = false;
        p->raceComplete = false;
    }
    cruiseReset();
    // 0x004261F0's rest: the challenge's outcome ([G+0x6C]+0x7C) 0, each car's skids quiet (0x00492710) and the
    // next 50 frames on their own time (+0x78 0).
    if (race != nullptr) race->rules().challenge = oj::ChallengeOutcome::None;
    rules.challenge = oj::ChallengeOutcome::None;
    for (std::unique_ptr<RaceSlot>& s : cars) quietSkids(*s);
    world->restartSmoothing();
    // With the sound: every emitter of the 3D world anew, its player's master at 1 x the effects' volume, the
    // speech's +0x24 cleared.
    if (sound3d) {
        sound3d->manager().resetEmitters();
        sound3d->player().setMasterVolume(1.0f * effectsVolume());
    }
    if (oj::Speech* speech = oj::gameSounds().speech) speech->holdWorld = false;
    // 0x0041E110 for each player: its car the player's again (+8 0; the test's autopilot keeps it with the AI);
    // the camera on the car of 0x004BB1A0 in the career with the race's cars not chosen, else on the player's.
    for (std::unique_ptr<RacePlayer>& each : players) {
        RacePlayer& player = *each;
        if (player.slot != nullptr) player.slot->car.kind = autopilot ? oj::CarKind::Ai : oj::CarKind::Player;
        if (rules.mode == RaceMode::Career && rules.career != nullptr && rules.career->profile.raceCarsChosen == 0) {
            RaceSlot* const on = racerCamera();
            player.viewed = on == player.slot ? nullptr : on;
        } else {
            player.viewed = nullptr;
        }
        cameraTakes(player, followedSlot(player));
    }
    // 0x00455B1A, 0x0041E1E5: each player's controller made again (the keyboard's map DRIVER1).
    for (const std::unique_ptr<RacePlayer>& p : players) input->useDriver(p->controller);
    // 0x00455B2C: with the race paused the music's song (+0x58) and the speech given back; the 3D world's voices
    // always (its manager's +0x20 with 0). Unpaused, the music and the speech stay paused as the enter left them
    // until the window leaves (0x00455F50, openAskedWindow).
    if (soundsHeld) {
        oj::Jukebox& music = oj::jukebox();
        if (paused && music.player() != nullptr) {
            music.paused = false;
            if (music.handle != 0) music.player()->resume(music.handle);
        }
        oj::Speech* speech = oj::gameSounds().speech;
        if (paused && speech != nullptr && speech->player() != nullptr) {
            speech->paused = false;
            speech->player()->resumeAll();
        }
        if (sound3d) sound3d->manager().pauseAll(false);
        soundsHeld = false;
        soundsPaused = false;
    }
    paused = false;
    frontEnd.screenWindow.reset();
    // 0x00401E80 for each player's camera: a solo race or a cruise the first player's kept view (the settings'
    // +0x8C, whichever the player), a sprint's later heats the sweep before the start with the race's count 8.
    if (rules.type == RaceType::Solo || rules.type == RaceType::Cruise) {
        for (std::unique_ptr<RacePlayer>& each : players) {
            RacePlayer& player = *each;
            player.raceCamera.take(viewKept(first()), true, cameraTarget(player), cameraRace(player),
                                   player.camera);
            for (const int asked : player.raceCamera.takeWindowAsks()) chooseWindow(asked != 0);
        }
    } else if (rules.type == RaceType::Sprint && race != nullptr && race->rules().heat != 0) {
        race->count = 8.0f;
        for (std::unique_ptr<RacePlayer>& each : players) {
            RacePlayer& player = *each;
            if (hasCamera()) {
                player.raceCamera.take(oj::scene::RaceCamera::kBeforeStart, true, cameraTarget(player),
                                       cameraRace(player), player.camera);
            }
            for (const int asked : player.raceCamera.takeWindowAsks()) chooseWindow(asked != 0);
        }
    } else {
        startIntro(routeLine, wideScreen);
        for (std::unique_ptr<RacePlayer>& p : players) {
            if (!p->introPlaying) countAndSweep(*p);
        }
    }
    oj::log::info("race: started again");
}

void TrackView::resetCar(RaceSlot& s) {
    const bool full =
        (rules.mode == RaceMode::Arcade || rules.mode == RaceMode::Custom) && !rules.replayRace && net == nullptr;
    if (s.damage) s.damage->repair(full);
    // 0x0048A700: the grid's vertex lists and zones dropped (0x00556130), the scene objects of the model's nodes
    // gone (0x00420F30); 0x00489440: the meshes made again; then (0x004896E9) the model given to the grid again,
    // its kept damage dented again with the main thread's rand(), and its scene objects made again.
    if (s.damage && s.model) {
        s.damage->clear();
        s.car.zones.fill(nullptr);
    }
    dropLightSteps(s);
    if (s.damage && s.model) s.model->undent();
    if (oj::Car* kept = s.car.car; kept != nullptr) {
        if (kept->neonFlickers != 0) kept->body[9] = 0;
        kept->nitro = std::bit_cast<uint32_t>(15.0f);
        kept->brakeWear = 0;
        kept->tyreWear = 0;
        kept->steeringPulls = 0;
        kept->neonFlickers = 0;
        kept->nitroLeaks = 0;
        if (kept->turboLost != 0) {
            s.car.hasTurbo = true;
            kept->turboLost = 0;
            s.body->restoreTurbo();
        }
    }
    if (s.model) giveModelToGrid(s, oj::gameRandom());
    s.car.rearLightsOut = false;
    s.car.headlightsOut = false;
    s.car.rearPartOut = false;
    if (s.lightsOn) {
        for (oj::scene::CarFlare* f : s.headFlares) {
            if (f != nullptr) f->on = true;
        }
    }
}

void TrackView::endRace() {
    // 0x00425BD7 (the race's end, 0x004259A0): the debug pages hidden ([G+0x78] +0x08 with 0), the only call of it.
    if (oj::debug::Pages* pages = oj::debug::pages()) pages->setVisible(false);
    // The cars' records keep their grid objects with the race's dents.
    for (const std::unique_ptr<RaceSlot>& s : cars) {
        if (s->damage && s->car.car != nullptr && s->car.car->grid) s->damage->give(*s->car.car->grid);
    }
    first().raceCamera.release(first().camera);
    first().director = oj::scene::CameraDirector{};
    frontEnd.closeWindows();
    netNoteAnswer = 0;
    netErrorAnswer = 0;
    netQuitting = false;
    netLostFrames = 0;
    frontEnd.closedownFrames = 0;
    frontEnd.unloadAsked = false;
    paused = false;
    frontEnd.window = {};
    frontEnd.windowAsked.clear();
    for (std::unique_ptr<RacePlayer>& p : players) {
        p->finished = false;
        p->raceComplete = false;
        p->introPlaying = false;
    }
    frontEnd.raceFade = 0.0f;
    // 0x00420B30: the cruise manager's zones let go (0x004B0110); 0x004AA9E0: its car (+0x3C0) let go and its
    // messages' texts emptied (its +0x454 .. +0x45C, the HUD's mplay1 .. 3 elements of 0x004B09D0, which the
    // race's HUD keeps here, cleared).
    if (rules.career != nullptr) {
        oj::Cruise& cruise = rules.career->cruise;
        cruise.zones.clear();
        cruise.zone = nullptr;
        cruise.lastZone = nullptr;
        cruise.car = nullptr;
        for (oj::CruiseMessage& m : cruise.messages) {
            m.name.clear();
            m.points.clear();
        }
    }
    netDetach();
    // Camera effects keep shared ownership beyond the scene's particle lists. Drop every
    // player's references while the scene and audio world still exist: a wet camera's
    // rain emitter releases its sound instance in its destructor.
    for (const std::unique_ptr<RacePlayer>& p : players) {
        p->cameraRain.reset();
        p->cameraSplash.reset();
        p->cameraBlobs.reset();
        p->rainSound = nullptr;
    }
    race.reset();
    cars.clear();
    first().slot = nullptr;
    world.reset();
    scene.reset();
    route.reset();
    places.clear();
    racing = false;
    airhornSound = nullptr;
    oj::raceSounds() = nullptr;
    oj::gameSounds().world = nullptr;
    sound3d.reset();
    sound3dBanks.clear();
    oj::ui::Screen::defaultSound = nullptr;
    frontEnd.raceUi = oj::UiSound{};
}

}  // namespace app
