// The race's replay: recorded, played and started again.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/Log.h"

#include <bit>

namespace app {

void TrackView::syncReplayFlags() {
    rules.replay = replay.playing != 0;
    rules.replayRace = replay.replayRace != 0;
    if (race != nullptr) {
        race->rules().replay = rules.replay;
        race->rules().replayRace = rules.replayRace;
    }
}

oj::ReplayRace TrackView::replayRace() const {
    oj::ReplayRace r;
    for (const std::unique_ptr<RaceSlot>& s : cars) r.cars.push_back(const_cast<oj::RaceCar*>(&s->car));
    r.mode = rules.mode;
    r.type = rules.type;
    if (race != nullptr) {
        const oj::RaceState& state = *race;
        r.allFinished = state.finishers.size() == const_cast<oj::RaceState&>(state).competitors().size();
    }
    if (rules.career != nullptr) r.cruiseTime = rules.career->cruise.time;
    return r;
}

void TrackView::beginReplay(const oj::ReplayRace& r) {
    replay.begin(r, trackName, routeLine, timeOfDay, weather, static_cast<uint32_t>(laps));
    replay.carsStream.clear();
    for (const std::unique_ptr<RaceSlot>& s : cars) {
        oj::Car car = s->car.car != nullptr ? *s->car.car : s->record;
        car.write(replay.carsStream);
    }
    replay.header.carsBytes = static_cast<uint32_t>(replay.carsStream.size());
    oj::log::info("race: the replay's record begins ({} cars)", cars.size());
}

void TrackView::startReplay(bool fromStart) {
    if (fromStart && replay.readCars() && replay.savedCars.size() == cars.size()) {
        for (size_t i = 0; i < cars.size(); ++i) {
            RaceSlot& s = *cars[i];
            const oj::Car& from = replay.savedCars[i];
            oj::Car& to = s.car.car != nullptr ? *s.car.car : s.record;
            to.brakeWear = from.brakeWear;
            to.steeringPulls = from.steeringPulls;
            to.turboLost = from.turboLost;
            to.tyreWear = from.tyreWear;
            to.nitro = from.nitro;
            to.rating = from.rating;
            if (s.damage && from.grid) s.damage->restore(*from.grid);
        }
    }
    first().raceCamera.resetView();
    for (std::unique_ptr<RaceSlot>& s : cars) resetCar(*s);
    settleRace(oj::gameRandom());
    first().viewed = cars.empty() ? nullptr : cars[0].get();
    cameraTakes(first(), followedSlot(first()));
    first().raceCamera.resetView();
    if (fromStart && rules.career != nullptr) rules.career->cruise.car = nullptr;
    if (fromStart) replay.play();
    replay.restart();
    paused = false;
    replay.resume();
    syncReplayFlags();
    frontEnd.raceFade = 1.0f;
    oj::log::info("race: the replay plays from the start ({:.2f} s)", replay.length);
}

void TrackView::followReplayWindow(const oj::ReplayWindowFrame& f) {
    if (f.release) input->release();
    if (f.unpause) paused = false;
    if (f.pause) paused = true;
    if (f.nextCar && !cars.empty()) {
        size_t at = 0;
        for (size_t i = 0; i < cars.size(); ++i) {
            if (cars[i].get() == followedSlot(first())) at = i;
        }
        first().viewed = cars[(at + 1) % cars.size()].get();
        cameraTakes(first(), followedSlot(first()));
        first().director.start(first().raceCamera, 8, 0.0f, cameraTarget(first()), cameraRace(first()),
                               first().camera);
        first().director.update(first().raceCamera, 0.0f, paused, cameraTarget(first()), cameraRace(first()),
                                first().camera);
    }
    if (f.changeView) {
        // 0x0044E7D6: the first player's camera's next view (0x004069F0), with more players the second's too;
        // the first's on the bumper (5): the director on the trackside view for 9999 s, the second's too.
        const size_t count = players.size() > 1 ? 2 : 1;
        for (size_t i = 0; i < count; ++i) {
            RacePlayer& player = *players[i];
            player.raceCamera.changeView(cameraTarget(player), cameraRace(player), player.camera);
        }
        if (first().raceCamera.mode() == oj::scene::RaceCamera::kBumper) {
            for (size_t i = 0; i < count; ++i) {
                RacePlayer& player = *players[i];
                player.director.start(player.raceCamera, oj::scene::RaceCamera::kTrackside,
                                      std::bit_cast<float>(0x461C3C00u), cameraTarget(player), cameraRace(player),
                                      player.camera);
            }
        }
        // The replay's view (0x004069F0) is not kept in the settings.
    }
    syncReplayFlags();
    followWindow(f);
}

void TrackView::replayRestart() {
    settleRace(oj::gameRandom());
    replayBegun = false;
    // 0x00420500, each player: the director, then its +0x18, +0x1C and +0x20 cleared.
    for (std::unique_ptr<RacePlayer>& each : players) {
        RacePlayer& player = *each;
        if (player.slot != nullptr) {
            player.director.start(player.raceCamera, 11, std::bit_cast<float>(0x461C3C00u), cameraTarget(player),
                                  cameraRace(player), player.camera);
        }
        player.finished = false;
        player.raceComplete = false;
    }
    for (std::unique_ptr<RaceSlot>& s : cars) quietSkids(*s);
    world->restartSmoothing();
    if (sound3d) sound3d->manager().resetEmitters();
    if (oj::Speech* speech = oj::gameSounds().speech) speech->holdWorld = false;
}

}  // namespace app
