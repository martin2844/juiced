// The race's frame: the drive, each player's frame, the laps' statistics and the cruise's.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Cars/CarRepair.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Network/VoiceChat.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace app {

using DriverSettings = oj::save::Settings;

namespace {

// 1.01's [0x00760F40]: in a network game the player's car's race distance as the cars' frame last took it.
float g_netPlayerDistance = 0.0f;

}  // namespace

bool TrackView::allFinished() const {
    for (const std::unique_ptr<RacePlayer>& p : players) {
        if (!p->finished) return false;
    }
    return true;
}

void TrackView::drive(float dt) {
    for (std::unique_ptr<RacePlayer>& p : players) p->actions = p->keys.update(*input, p->controller);
    const oj::DriverActions& actions = first().actions;
    const bool impacts = impactFrame;
    impactFrame = !impactFrame;
    if (!racing) return;
    // The game's clock runs at the speed the race's frame set it to last (0x00425D40: the replay's +0x30 while it
    // plays, else 1).
    dt = dt * clockScale;
    drawScale = clockScale;
    clockScale = replay.playing != 0 ? replay.timeScale : 1.0f;
    syncReplayFlags();
    // While paused the race's frame (0x00425D40) leaves out the physics, the cars, the scene objects and the count;
    // the drivers' levels wait too (0x004BE238). The players' frame goes on.
    frameActions = actions;
    // 0x00425D40: the frame's time smoothed every frame (0x00425C00), then a replay's restart asked (+0x824)
    // started again (0x00426300) and cleared, the player's car left to the AI outside a replay's race.
    world->smoothTime(dt);
    // 0x00425D6F: the race's first frame told to the session (0x004EA910).
    if (!netLoaded) {
        netLoaded = true;
        if (net != nullptr) net->loaded();
    }
    if (replay.restartAsked != 0) {
        replayRestart();
        replay.restartAsked = 0;
        if (rules.mode != RaceMode::Attract && first().slot != nullptr) first().slot->car.kind = oj::CarKind::Ai;
    }
    // 0x00425E04: the profiler's timer "Physics" (taken at the first race frame, [0x007A2F18]) runs from here to the
    // players' frame (0x004260A1).
    static const int physicsTimer =
        oj::debug::profiler() != nullptr ? oj::debug::profiler()->add("Physics") : -1;
    if (oj::debug::Profiler* p = oj::debug::profiler()) p->start(physicsTimer);
    // 0x00425E87: the physics steps of the smoothed time counted paused or not (the stock camera's time divides by
    // them).
    world->countSteps();
    if (!paused) {
        raceFrame(dt, impacts);
    } else if (net != nullptr) {
        // 0x00426000: paused on the network the race's state goes on (0x004A0220: the count, the competitors,
        // the places) and each car's lights (0x004890B0), from the second car to the last, then the first.
        oj::CrtRandom& random = oj::gameRandom();
        const oj::RaceState::Events counted = race->update(world->stepTime(), dt, random);
        if (counted.go) airhorn(55000);
        if (counted.beep) airhorn(44000);
        for (size_t i = 1; i <= cars.size() && !cars.empty(); ++i) projectorsFrame(*cars[i % cars.size()], random);
    }
    if (oj::debug::Profiler* p = oj::debug::profiler()) p->stop();
    if (!hasCamera()) return;
    const float step = paused ? 0.0f : world->stepTime();
    for (std::unique_ptr<RacePlayer>& p : players) playerFrame(*p, p->actions, step, dt);
    frontEndFrame(dt);
    for (std::unique_ptr<RacePlayer>& p : players) playerFrameEnd(*p);
    soundListener();
    crowdFrame(dt);
    pauseSounds();
}

void TrackView::raceFrame(float dt, bool impacts) {
    // The game's frame (0x0040CCA0): the drivers' frame (0x004BDF00) before the race's, with the crew's orders of a
    // career race that is not a cruise. 0x004BDF00: [0x0075EF64] turned over; outside the attract replay (mode 4)
    // and a replay's race, not only watching a network game, the racers' names' time runs down by the frame time
    // and goes to 2 s on a tactics key (DRIVER_TACTICS_UP .. _LEFT, the controller's +0x74 .. +0x80) or, watching a
    // career race off the network (the profile's +0x614 0), on the third driver's two keys (+0x84, +0x88); NEXTCAR
    // and SELECT in watchedCar; then the drivers' levels (0x004BE242: 0x004BEAF0, which may open the crew's orders,
    // 0x004BEA60) before the orders' state (crewOrders).
    driversParity = !driversParity;
    const bool drivers = rules.mode != RaceMode::Attract && !rules.replayRace && !(net != nullptr && rules.spectator);
    if (drivers) {
        float& names = namesTime();
        names = names - dt;
        const bool careerWatched = gameData != nullptr && gameData->career && gameData->career->profile.raceCarsChosen == 0;
        if ((frameActions.tactics[4] || frameActions.tactics[5]) && careerWatched && net == nullptr) names = 2.0f;
        for (size_t k = 0; k < 4; ++k) {
            if (frameActions.tactics[k]) names = 2.0f;
        }
    }
    if (rules.mode == RaceMode::Career && !rules.replay) watchedCar();
    if (drivers) race->driverLevels(dt);
    if (frontEnd.hud != nullptr && rules.mode == RaceMode::Career && rules.type != RaceType::Cruise && !rules.replay) {
        frontEnd.hud->crewOrders(hudRace(), frameActions.tactics, dt);
    }
    world->stepFrame();
    // The fixed build: the frame's hits between our cars and the others' told to their machines.
    if (net != nullptr) netContacts(world->stepTime());
    // The bodies' nodes are the models' (0x004F2C00 after every body stepped): the cars' frames below see the
    // models where the step put them.
    placeCars();
    const float step = world->stepTime();
    oj::CrtRandom& random = oj::gameRandom();
    // The world's atmosphere after the physics (0x00534300).
    if (oj::render::AtmosphereSet* set = scene->atmosphere(); set != nullptr && set->current != nullptr) {
        oj::stepRain(*set->current, step, random);
    }
    // 0x004270C0 before the cars: under the count's 4.005 s the record begins once (the step first), unless the
    // replay plays; then the replay's step (the record's frames or the play's). Neither on the network
    // (0x00426EA0, 0x004270C0).
    if (net == nullptr) {
        const oj::ReplayRace r = replayRace();
        bool pause = false;
        if (!replayBegun && race->count < 4.005f) {
            if (replay.playing == 0) {
                replay.step(r, dt, pause);
                beginReplay(r);
            }
            replayBegun = true;
        }
        if (replay.step(r, dt, pause)) {
            // The play's first frame: the first player's camera reset (0x00401F20), the racers placed on their
            // lines (0x00497BB0).
            first().raceCamera.resetView();
            for (std::unique_ptr<RaceSlot>& s : cars) {
                if (s->car.competitor == nullptr) continue;
                const oj::Vec4 o = s->body->origin();
                oj::placeOnLine(*s->car.competitor, {o[0] * 100.0f, o[1] * 100.0f, o[2] * 100.0f, o[3]});
                oj::placeTracker(s->car);
            }
        }
        if (pause) paused = true;
        syncReplayFlags();
    }
    // 0x00497C50: the cars' slipstream links cleared, then each car's frame (0x00488EE0, then 0x004890B0).
    std::vector<oj::RaceCar*> raceCars;
    for (std::unique_ptr<RaceSlot>& s : cars) {
        s->car.followed = nullptr;
        s->car.follower = nullptr;
        raceCars.push_back(&s->car);
    }
    for (std::unique_ptr<RaceSlot>& s : cars) {
        oj::RaceCar& car = s->car;
        wheelsFrame(*s, step);
        oj::tiltGuard(car);
        oj::carFrame(car, step, dt, race->rules().type);
        oj::slipstream(car, raceCars);
        // The car's model (+0x430): its chassis node put at (0, -MAXDROPHEIGHT x the car's +0x58, 0).
        if (s->chassis != nullptr && s->record.def != nullptr) {
            const float drop = s->record.def->maxDropHeight * std::bit_cast<float>(s->record.lowering);
            s->chassis->local[3] = 0.0f;
            s->chassis->local[7] = -drop;
            s->chassis->local[11] = 0.0f;
            s->chassis->invalidate();
        }
        carLights(*s);
        // 0x004875F0: the road's sound (0x00487490); +0x43C falls to 0; the engine's frame, then a gear up or down
        // told to it (+0x438 the gear it was told of, +0x43C 2 after a change up).
        // 0x00428180: a replay's frame applied turns the horn (from 1300) on or off by it, off in a race of
        // mode 4.
        if (car.replayHorn >= 0) {
            if (s->horn != nullptr) {
                s->horn->setMinDistance(1300.0f);
                if (rules.mode != RaceMode::Attract && car.replayHorn != 0) {
                    s->horn->play();
                } else {
                    s->horn->stop();
                }
            }
            car.replayHorn = -1;
        }
        roadSound(*s);
        car.gearChange = car.gearChange - 1 < 0 ? 0 : car.gearChange - 1;
        if (s->sound) {
            oj::CarSound::Frame f;
            f.dt = dt;
            f.replayRace = rules.replayRace;
            f.replayLoad = car.replayEngineSound;
            f.raceState = race != nullptr;
            f.count = race != nullptr ? race->count : 0.0f;
            s->sound->frame(f, random);
            const uint32_t g = s->body->gear;
            if (g > car.lastGear) {
                s->sound->gearUp(f, random);
                car.lastGear = g;
                car.gearChange = 2;
            } else if (g < car.lastGear) {
                s->sound->gearDown();
                car.lastGear = g;
            }
            car.engineLoad = s->sound->load();
        }
        car.hornSounding = s->horn != nullptr && s->horn->sounding();
        exhaustsFrame(*s);
        const bool rearPartOut = car.rearPartOut;
        const bool headlightsOut = car.headlightsOut;
        const bool rearLightsOut = car.rearLightsOut;
        oj::damageEffects(car, random);
        // The rear bumper's damage past the model's threshold hides the reversing lights for good (0x00488C5E);
        // the lights put out turn their flares off.
        if (!rearPartOut && car.rearPartOut && s->reverseLight != nullptr) s->reverseLight->flags |= 4u;
        if (!headlightsOut && car.headlightsOut) {
            for (oj::scene::CarFlare* f : s->headFlares) {
                if (f != nullptr) f->on = false;
            }
            if (s->headBeam != nullptr) s->headBeam->on = false;
        }
        if (!rearLightsOut && car.rearLightsOut) {
            for (oj::scene::CarFlare* f : s->brakeFlares) {
                if (f != nullptr) f->on = false;
            }
        }
        car.effects.puddles = scene->puddleMap();
        car.effects.fastForward = replay.fast != 0;
        if (const oj::render::AtmosphereSet* a = scene->atmosphere(); a != nullptr && a->current != nullptr) {
            car.effects.roadWetness = a->current->roadWetness;
        }
        // 0x0048880E: the player's car put back with one player blacks out the screen ([[G+0x3C]+8]+0x10C 1).
        if (oj::wheelEffects(car, step, dt, random, world->pools(), race.get(), impacts) &&
            car.kind == oj::CarKind::Player && rules.players == 1) {
            frontEnd.raceFade = 1.0f;
        }
        carParticles(*s);
        flameParticles(*s);
        impactParticles(*s);
        if (car.kind == oj::CarKind::Ai && car.competitor->respawnAsked)
            oj::respawn(car, race->rules(), world->pools());
        if (s->chassis != nullptr) {
            const oj::Mat34& w = s->chassis->world();
            oj::trackLine(car, {w[3], w[7], w[11], 0.0f});
        }
        projectorsFrame(*s, random);
        // 1.01 (0x004996F5, after 0x004890B0): in a network game the player's car keeps its race distance and
        // another machine's car within 30000 of it (strictly) collides again (the body's +0x78 1); with the
        // matchmaking ([0x0075EF84]) a session hosted without collisions (by the online kind [0x0075EF88]: the
        // arcade one's +0x34, the career one's +0x44) takes the car's collisions off (0).
        if (oj::net::netGame() != nullptr && car.competitor != nullptr) {
            if (car.kind == oj::CarKind::Player) g_netPlayerDistance = car.competitor->raceDistance;
            if (car.kind == oj::CarKind::Remote) {
                const double d = car.competitor->raceDistance;
                if (static_cast<double>(g_netPlayerDistance) - 30000.0 < d &&
                    d < static_cast<double>(g_netPlayerDistance) + 30000.0) {
                    s->body->collides = true;
                }
            }
        }
        if (oj::net::matchmaking() != nullptr) {
            const oj::net::OnlineHosted& hosted = oj::net::onlineHosted();
            if ((hosted.mode == oj::net::OnlineKind::Arcade && hosted.arcade.collision == 0) ||
                (hosted.mode == oj::net::OnlineKind::Career && hosted.career.collision == 0)) {
                s->body->collides = false;
            }
        }
        oj::applyControls(car, *race, random, dt);
        // 0x00497FE0: a parked car's centre at (0, 0, -1000000) (w a value of the stock code's stack), its node and
        // its state with it.
        if (car.parked) {
            oj::CarBody& b = *s->body;
            b.position = {0.0f, 0.0f, -1000000.0f, b.position[3]};
            b.syncNode();
            for (size_t k = 0; k < 3; ++k) b.state[oj::RigidBody::kPosition + k] = b.position[k];
        }
    }
    // The scene objects after the cars (0x00422BB0): the splines' riders move.
    std::vector<oj::Vec4> carPlaces;
    for (const std::unique_ptr<RaceSlot>& s : cars) {
        if (s->chassis == nullptr) continue;
        const oj::Mat34& w = s->chassis->world();
        carPlaces.push_back({w[3], w[7], w[11], 0.0f});
    }
    scene->sceneObjects().update(step, impacts, carPlaces);
    // The objects' steps (the manager's +0x70): the scene's, then each car's in the order the cars were made.
    if (impacts) {
        const float objectsTime = scene->sceneObjects().stepTime();
        scene->stepObjectLights(objectsTime, dt);
        for (std::unique_ptr<RaceSlot>& s : cars) {
            for (oj::scene::LightStep& l : s->lights) {
                l.frame(scene->sunMap(), scene->sceneObjects(), objectsTime);
            }
        }
    }
    const oj::RaceState::Events counted = race->update(step, dt, random);
    if (counted.beep) oj::log::info("race: {}", race->second + 1);
    if (counted.go) oj::log::info("race: go");
    // 0x004A02B7 / 0x004A032C: the start's airhorn at 55000 Hz, each whole second before it at 44000.
    if (counted.go) airhorn(55000);
    if (counted.beep) airhorn(44000);
    placeCars();
    // Every 2 s of the race: where the cars are (not of the game).
    if (++framesRaced % 120 == 0) {
        for (std::unique_ptr<RaceSlot>& s : cars) {
            const oj::Competitor& c = *s->car.competitor;
            oj::log::info("race: car {} place {} lap {} next {} line {} distance {:.0f} speed {:.1f} steer {:.2f} "
                          "throttle {:.2f} brake {:.2f}{}", c.index, c.place, c.lap, c.next, c.line,
                          c.raceDistance, s->body->localVelocity[2], c.steering, c.throttle, c.brake,
                          c.finished ? " finished" : "");
        }
    }
}

void TrackView::cruiseReset() {
    if (rules.type != RaceType::Cruise || rules.career == nullptr) return;
    rules.career->cruise.setup(oj::x87::chop(rules.minutes), rules.mode, rules.type);
    rules.career->cruise.started = 1;
}

void TrackView::cruiseFrame(const oj::DriverActions& actions, float dt) {
    if (rules.career == nullptr || first().slot == nullptr || frontEnd.hudTexts == nullptr) return;
    oj::Cruise& c = rules.career->cruise;
    oj::CruiseRace r;
    r.car = &first().slot->car;
    r.player = first().slot;
    r.mode = rules.mode;
    r.type = rules.type;
    r.networked = net != nullptr;
    r.paused = paused;
    r.frameTime = dt;
    r.steering = actions.steering;
    r.texts = frontEnd.hudTexts;
    r.language = frontEnd.hudLanguage != nullptr ? frontEnd.hudLanguage->index : 7;
    for (std::unique_ptr<RaceSlot>& s : cars) r.cars.push_back(&s->car);
    if (gameData != nullptr) {
        r.carValue = oj::carValue(first().slot->record, gameData->catalogue);
        if (rules.mode == RaceMode::Arcade) {
            const oj::Arcade& a = gameData->arcade;
            r.target = oj::x87::chop(std::bit_cast<float>(a.i32(a.current() + oj::Arcade::kRaceChallengeValue)));
        } else if (rules.mode == RaceMode::Career && gameData->career) {
            oj::Career& career = *gameData->career;
            if (const oj::CalendarEvent* e = oj::currentEvent(career.calendar)) {
                if (e->challengeIndex == -1) {
                    r.target = oj::x87::chop(std::bit_cast<float>(career.crews[3].moneyMark));
                } else if (e->crew >= 0 && static_cast<size_t>(e->crew) < career.crews.size()) {
                    r.target = career.crews[static_cast<size_t>(e->crew)]
                                   .challenges[static_cast<size_t>(e->challengeIndex)].value;
                }
            }
        }
    }
    if (first().slot->record.grid) r.carDamage = oj::damageLevel(*first().slot->record.grid);
    std::vector<std::pair<std::wstring, float>> said;
    c.frame(r, said);
    if (frontEnd.hud != nullptr) {
        for (const auto& [text, seconds] : said) frontEnd.hud->messages(0).push(text, seconds);
    }
}

float TrackView::partValue(const oj::Car& car, size_t kind) const {
    if (gameData == nullptr) return 0.0f;
    const auto index = static_cast<int8_t>(car.mods[kind]);
    if (index == -1) return 0.0f;
    const std::vector<oj::CarPart>& parts = gameData->catalogue.lists[kind].parts;
    if (parts.empty()) return 0.0f;
    if (index >= 0 && index < static_cast<int>(parts.size())) return parts[static_cast<size_t>(index)].dataValue;
    return parts[0].dataValue;
}

void TrackView::playerFrame(RacePlayer& player, const oj::DriverActions& actions, float step, float dt) {
    oj::CarBody* const own = player.slot != nullptr ? player.slot->body.get() : nullptr;
    // The fly-by (mode 9) and Change View (0x00406AB0, not paused): with the count above 4 the cars to the grid
    // again and the count from 4, the race front end's black 1; mode 9 takes the view behind the car (0) with a
    // reset, kept in the settings (+0x8C); the fly-by is not played on. That is the frame's one Change View: the
    // controls below only look back and pan.
    bool flyBySkipped = false;
    if (player.introPlaying && !paused && actions.changeView) {
        if (race->count > 4.0f) {
            settleRace(oj::gameRandom());
            race->count = 4.0f;
            frontEnd.raceFade = 1.0f;
        }
        player.introPlaying = false;
        player.raceCamera.take(oj::scene::RaceCamera::kChase, true, cameraTarget(player), cameraRace(player),
                               player.camera);
        keepView(player, player.raceCamera.mode());
        flyBySkipped = true;
        oj::log::info("race: the fly-by skipped (Change View)");
    }
    if (!player.introPlaying) {
        // 0x0041FE50: the director first (0x00408830, with the frame's step; none while paused).
        player.director.update(player.raceCamera, step, paused, cameraTarget(player), cameraRace(player),
                               player.camera);
        // Stock problem: the camera's time is the frame's step over the physics steps it was split into, so with
        // two or three steps (below 20 frames a second) the views ease two or three times slower. The fixed build
        // gives it the frame's step.
        // The race's step (+0x74) goes on while paused; the camera takes 0 then (0x00406322), but the free views
        // 14 and 15 take it again (0x004064F7).
        player.raceCamera.update(cameraTarget(player), cameraRace(player), world->stepTime(), player.camera);
        if (player.raceCamera.takeBlobClear()) {
            auto* blobs = dynamic_cast<oj::scene::SmearEmitter*>(player.cameraBlobs.get());
            if (blobs != nullptr) blobs->clear(oj::gameRandom());
        }
        if (player.raceCamera.takeSoundsReset() && sound3d) sound3d->manager().resetEmitters();
        cameraOcclusion(player);
        lightBlurFrame(player);
        atmosZonesFrame(player);
        weatherFrame(player);
        // 0x00406AB0 for a network game only watched: CHANGE_CAMERA (not paused) the replay's next view
        // (0x004069F0), nothing else. Else nothing while paused or in the free view (14): there Change View,
        // REAR_VIEW and the pan do nothing.
        const bool changeView = actions.changeView && !flyBySkipped;
        if (net != nullptr && rules.spectator) {
            if (!paused && actions.changeView) {
                player.raceCamera.changeView(cameraTarget(player), cameraRace(player), player.camera);
            }
        } else if (!paused && player.raceCamera.mode() != oj::scene::RaceCamera::kFree) {
            if (changeView && race->count > 4.0f) {
                // Change View before the count's last 4 s: off the network the race to the grid again (its reset
                // draws the game's rand()) and the count from 4; the race front end's black 1.
                if (net == nullptr) {
                    settleRace(oj::gameRandom());
                    race->count = 4.0f;
                }
                frontEnd.raceFade = 1.0f;
            }
            player.raceCamera.controls(changeView, actions.rearView, actions.cameraPan, cameraTarget(player),
                                       cameraRace(player), player.camera);
            if (changeView) keepView(player, player.raceCamera.mode());
        }
        for (const int asked : player.raceCamera.takeWindowAsks()) {
            if (!netPauseShown()) chooseWindow(asked != 0);
        }
    }
    // 0x0041E200: paused, the controller's motors (+0x10C, +0x110) cleared and nothing more; else it reads the
    // controls only while the player's car is the player's (+8 = 0), then PAUSE (a held action) pauses the race
    // while some player has not finished, outside a replay, with a window shown that is not hud_pause: the
    // window asked for in place of any waiting, CLICK, the race paused (0x004263E0), the controller let go.
    // 0x0041E200: no cruise for a network game only watched.
    if (!(net != nullptr && rules.spectator)) cruiseFrame(actions, dt);
    if (paused) input->rumble(player.controller) = oj::ff::Rumble{};
    if (!paused) {
        bool held = oj::kHoldCars;
        if (!held && own != nullptr && player.slot->car.kind == oj::CarKind::Player && race->count < 10.0f) {
            oj::CarBody& body = *own;
            // 0x0041E2CF: DRIVER_HORN over 0.2 plays the horn (each frame it is held), else stops it; from 1300.
            if (player.slot->horn != nullptr) {
                player.slot->horn->setMinDistance(1300.0f);
                if (actions.horn > 0.2f) {
                    player.slot->horn->play();
                } else {
                    player.slot->horn->stop();
                }
            }
            // 0x0041F244: the gearbox by hand in a drag race (type 6), else the settings' automatic gearbox
            // (+0x84); 0x0041E43F: the steering's sensitivity, the settings' +0xC4.
            static const oj::save::Settings kDefaults = oj::save::Settings::defaults(0);
            const oj::save::Settings& set = settings != nullptr && settings->has_value() ? **settings : kDefaults;
            // The player's own settings by its number (+4 x the number: +0x84, +0xAC, +0xC4, +0xB4, +0xA4; the
            // wheel +0x78 the first's), its force feedback on the FF manager's channel of its number.
            const uint32_t byNumber = 4u * static_cast<uint32_t>(player.index);
            player.control.automaticGearbox =
                rules.type != RaceType::Sprint && set.u32(DriverSettings::kAutomatic + byNumber) != 0;
            player.control.steeringSensitivity = std::bit_cast<float>(set.u32(DriverSettings::kSensitivity + byNumber));
            // 0x0041E2A4: the steering wheel (+0x78) and the player's not speed sensitive (+0xAC), MultiTurn.
            player.control.wheel = set.u32(DriverSettings::kSteeringWheel) != 0;
            player.control.notSpeedSensitive = set.u32(DriverSettings::kWheelSpeed + byNumber) != 0;
            player.control.multiTurn = multiTurn;
            player.control.apply(body, actions, step, race->count);
            // 0x0041E5D9: a car whose steering pulls (the car's +0x64) turns 0.05 more to its side (+0x4F8, 0
            // the left), within -1 .. 1 (0x004203E0).
            const oj::Car& record = player.slot->car.car != nullptr ? *player.slot->car.car : player.slot->record;
            if (record.steeringPulls != 0) {
                const float pull = player.slot->car.pullSide == 0 ? -0.05f : 0.05f;
                body.steering = std::clamp(body.steering + pull, -1.0f, 1.0f);
            }
            // 0x0041E6D6: with the steering wheel and the player's force feedback (+0xB4) the car's forces on
            // the player's channel (0x0041E6F5 .. 0x0041F244), its strength the settings' +0xA4.
            if (player.control.wheel && set.u32(DriverSettings::kWheelForce + byNumber) != 0 && wheels != nullptr) {
                oj::ff::FeedbackCar fc;
                const oj::RaceCar& rc = player.slot->car;
                for (size_t i = 0; i < 4; ++i) fc.travel[i] = player.slot->wheels[i].travel;
                for (size_t i = 0; i < 2; ++i) {
                    fc.skid[i] = rc.wheels[i].skid;
                    fc.smoke[i] = rc.wheels[i].smoke;
                    fc.down[i] = rc.wheels[i].down;
                    fc.surface[i] = rc.wheels[i].surface;
                }
                const oj::Vec4& v = body.localVelocity;
                fc.speed = std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]);
                fc.impact = rc.impact;
                fc.hitPoint = {rc.hitPoint[0], rc.hitPoint[1], rc.hitPoint[2]};
                fc.engineSpeed = body.engineSpeed;
                fc.nitroOn = body.nitroOn;
                fc.part0 = partValue(record, 0);
                fc.part2 = partValue(record, 2);
                const float strength = std::bit_cast<float>(set.u32(DriverSettings::kWheelStrength + byNumber));
                oj::ff::playerForces(*wheels, player.index, strength, fc, forceLoadMax);
            }
            oj::playerNitro(player.slot->car, actions.nitro, race->count,
                            rules.mode == RaceMode::Arcade && rules.arcadeNitro, rules.replayRace);
            // RESET held after the start ends the player's controls here (the pause, the controller's
            // rumble and the car's +0x38C are left as they are).
            held = race->count == 0.0f && actions.reset;
            // 0x0041F5F0: on the network RESET held tells the session (0x004EAA40; not bound on the PC).
            if (held && net != nullptr) net->carReset();
        }
        // 0x0041F703: a network game only watched: the camera's car's player gone, or SELECT, takes the next
        // player in the race and not gone (from the one after, round); none: the director on mode 13 for good.
        if (!held && net != nullptr && rules.spectator) netWatchNext(actions.select);
        if (!held) {
            bool pausedNow = false;
            if (actions.pause && !allFinished() && !rules.replay && frontEnd.window.id != RaceWindowId::None &&
                frontEnd.window.id != RaceWindowId::Pause) {
                frontEnd.windowAsked.clear();
                frontEnd.askWindow(RaceWindowId::Pause);
                frontEnd.raceUi.play("CLICK");  // 0x0041F6A7
                // Stock problem: in a network game the pause stops this machine's world as off line (its car and
                // the others' as it sees them), while the race's clock and the other machines go on. The fixed
                // build keeps a network game going under the pause window: its car gets no controls while the
                // window has them (the controller's driver map let go below).
                paused = net == nullptr;
                // 0x0041F6EB: the race track's pausing player (+0x10) this one; 0x0041F6F4: its controller let go
                // (the keyboard's map UI).
                pausing = &player;
                input->useUi(player.controller);
                pausedNow = true;
                oj::log::info("race: paused");
            } else if (actions.select && !allFinished() && race->count > 4.0f) {
                // 0x0041F7C1: SELECT (RETURN) with the count above 4: off the network the cars to the grid again,
                // the player's car in the first gear when below it (0x004F7710 with 2), the count from 4; the
                // view kept in the settings (+0x8C) taken with a reset when it is not the camera's (0x00406990,
                // the fly-by ends), the race front end's black 1.
                if (net == nullptr) {
                    settleRace(oj::gameRandom());
                    if (own != nullptr && own->gear < 2) own->shift(2);
                    race->count = 4.0f;
                }
                const int kept = viewKept(player);
                if (player.raceCamera.mode() != kept || player.introPlaying) {
                    player.introPlaying = false;
                    player.raceCamera.take(kept, true, cameraTarget(player), cameraRace(player), player.camera);
                    frontEnd.raceFade = 1.0f;
                }
                oj::log::info("race: the start skipped (SELECT)");
            }
            if (!pausedNow && own != nullptr && player.slot->car.kind == oj::CarKind::Player) {
                playerRumble(player, step, player.control.wheel);
            }
            // 0x0041FB2E: the car's traction control's mode (+0x38C) off in a cruise, else on.
            if (own != nullptr) own->asrMode = rules.type != RaceType::Cruise;
        }
    }
    // 0x0041FB60: the player's finish, once: the player's race-complete flags (+0x18, +0x1C), the competitor's
    // progress timer 5 s and progress 1000, no respawn asked, and the car to the AI (+8 = 1); then the camera
    // director or the chase view.
    // 0x0041FDD9: without a car, while the director is not active, once every racer of the session has
    // finished: both flags and the director on view 12 for 5 s (0x00408670).
    if (player.slot == nullptr && !player.director.active && !player.finished && rules.career != nullptr &&
        !(net != nullptr && rules.spectator)) {
        const oj::RaceSession& session = rules.career->session;
        bool all = true;
        for (const std::unique_ptr<RaceSlot>& s : cars) {
            if (s->car.record >= 0 && static_cast<uint32_t>(s->car.record) < session.count &&
                (s->car.competitor == nullptr || !s->car.competitor->finished)) {
                all = false;
            }
        }
        if (all) {
            player.finished = true;
            player.raceComplete = true;
            player.director.start(player.raceCamera, 12, 5.0f, cameraTarget(player), cameraRace(player),
                                  player.camera);
            oj::log::info("race: every racer of the watched race finished");
            for (const int asked : player.raceCamera.takeWindowAsks()) chooseWindow(asked != 0);
        }
    }
    if (player.slot != nullptr && !player.finished && player.slot->car.competitor != nullptr &&
        player.slot->car.competitor->finished) {
        player.finished = true;
        player.raceComplete = true;
        // 0x0041FBCF: outside a replay's race ([G+0x6C]+0x68) with one player the controller let go (the
        // keyboard's map UI: the driver's keys, the views' too, read nothing more).
        if (!rules.replayRace && rules.players <= 1) input->useUi(player.controller);
        oj::Competitor& c = *player.slot->car.competitor;
        c.progressTimer = 5.0f;
        c.progress = 1000.0f;
        c.respawnAsked = false;
        player.slot->car.kind = oj::CarKind::Ai;
        // The director on a circuit outside a sprint (0x00408670: the chase view with a reset, 5 s); else the
        // chase view with a reset (0x00406990).
        if (route != nullptr && !route->pointToPoint && rules.type != RaceType::Sprint) {
            player.director.start(player.raceCamera, 0, 5.0f, cameraTarget(player), cameraRace(player),
                                  player.camera);
        } else {
            player.raceCamera.take(oj::scene::RaceCamera::kChase, true, cameraTarget(player), cameraRace(player),
                                   player.camera);
        }
        player.director.started = true;
        finishStatistics(player);
        oj::log::info("race: the player finished in place {} at {:.2f} s", c.finishPlace, c.raceTime);
        for (const int asked : player.raceCamera.takeWindowAsks()) chooseWindow(asked != 0);
    }
}

bool TrackView::lapTimeStatistics(const RacePlayer& player) {
    oj::net::OnlineUser* users = oj::net::onlineUser();
    if (users == nullptr || player.slot == nullptr || player.slot->car.competitor == nullptr) return false;
    const oj::Competitor& c = *player.slot->car.competitor;
    int32_t rating = 0;
    if (oj::Car* car = player.slot->car.car) {
        rating = static_cast<int32_t>(std::bit_cast<float>(car->rating));
        if (rating == 0 && gameData != nullptr) {
            const oj::CarDynamics* dynamics =
                gameData->dynamics && car->def != nullptr ? gameData->dynamics->of(*car->def) : nullptr;
            oj::rateCar(*car, dynamics, gameData->catalogue);
            rating = static_cast<int32_t>(std::bit_cast<float>(car->rating));
        }
    }
    const int32_t routeIndex = gameData != nullptr ? gameData->tables.routeIndex(trackName, routeLine) : 0;
    const size_t at = static_cast<size_t>(player.index);
    const int32_t user = users->playerUser(at < users->localPlayers.size() ? users->localPlayers[at] : -1);
    oj::net::stats::reportLapTime(user, routeIndex, c.bestLap, player.slot->car.model, rating, c.topSpeed);
    // Let go after the report in the fixed build (the stock's 0x0041FCD8 kept the reference).
    users->userRelease(user);
    return true;
}

void TrackView::finishStatistics(const RacePlayer& player) {
    if (oj::net::stats::gameStatistics() == nullptr || rules.replayRace || rules.players >= 2) return;
    oj::net::OnlineUser* users = oj::net::onlineUser();
    if (users == nullptr) return;  // the stock's call through none
    const auto localUser = [&] {
        const size_t at = static_cast<size_t>(player.index);
        return users->playerUser(at < users->localPlayers.size() ? users->localPlayers[at] : -1);
    };
    const RaceType type = rules.type;
    if (type == RaceType::Circuit || type == RaceType::Solo || type == RaceType::TestTrack) {
        if (!lapTimeStatistics(player)) return;
        if ((oj::net::matchmaking() != nullptr || type == RaceType::Sprint) && (net == nullptr || net->racing() != 1))
            return;
    } else if (type == RaceType::Cruise) {
        const int32_t location = oj::net::stats::trackLocation(trackName);
        const int32_t score =
            rules.career != nullptr ? static_cast<int32_t>(rules.career->cruise.total) : 0;  // 0x00420390
        if (score == 0) return;  // unsigned: a score below 0 goes on
        // The user let go after the report in the fixed build (the stock kept the reference).
        const int32_t user = localUser();
        oj::net::stats::reportShowoff(user, location, score, player.slot->car.model);
        users->userRelease(user);
    } else {
        return;
    }
    oj::net::stats::reporter()->send();
}

void TrackView::playerFrameEnd(RacePlayer& player) {
    if (player.slot != nullptr && player.slot->car.kind == oj::CarKind::Player) {
        player.slot->body->collides = player.slot->car.ghostTime == 0.0f;
    }
    hideFollowed(player);
}

}  // namespace app
