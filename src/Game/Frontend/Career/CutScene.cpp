// CMSCutScene (record 0x007220A8, vtable 0x006F5560: enter 0x004C6B80, update 0x004C7BF0, leave 0x004C7030;
// docs/spec/14-career-windows.md "CMSCutScene"): the garage's test track. The player's cars drive out of the garage in
// front of the camera on the ground plane of a world without a route, then the test track's race loads.

#include "Game/Frontend/Career/CutScene.h"

#include "Core/Base/Log.h"
#include "Core/Cars/CarPhysics.h"
#include "Game/Cars/CarSound.h"
#include "Game/Frontend/WindowTools.h"
#include "Game/Media/Music.h"

#include <algorithm>
#include <cstring>

namespace oj::frontend {

namespace {

// The camera's first place ([0x007A2F50]) and those its stages 1 .. 3 start at.
constexpr Vec4 kCutFirstPlace{1800.0f, 75.0f, 2000.0f, 0.0f};
constexpr Vec4 kCutPlaces[3] = {{-3800.0f, 150.0f, 3000.0f, 0.0f},
                                {600.0f, 70.0f, 250.0f, 0.0f},
                                {250.0f, 75.0f, 700.0f, 0.0f}};

}  // namespace

scene::View3d* CutScene::sceneView(ui::Screen& s) {
    return dynamic_cast<scene::View3d*>(s.view("CAR3D"));
}

// 0x00485530 for a car of a garage's scene (CMSCutScene, car_rolling; not of the AI, the record's wear and nitro kept,
// the model in the view's world): 0x00485600 clears the record's flickering neon (+0x6C); the body of the car's
// dynamics with its parts fitted and the meshes of its folder at the place (centimetres; Stock problem: the place's
// heading is not set, a word of the stack: 0 here); the model (the main thread's rand()); its drive, nitro and turbo;
// the wheels' poses (0x00485270, 0x00485D20), the axles weighed, the race's tune; the brake lights and the reversing
// lights hidden (0x00485200). The view draws the model. The silhouettes of 0x004861CC are made in the view's world
// too but never laid: the scenes' frames (0x004C72D0, 0x004DE350) call 0x00488EE0 alone, never 0x004890B0 (their [1]),
// so they keep no vertices and draw nothing; not made here.
std::unique_ptr<CutCar> makeSceneCar(WindowHost& host, scene::View3d& view, Car& record, const CarDynamics& dynamics, const Vec4& place) {
    GameData* data = host.gameData();
    auto car = std::make_unique<CutCar>();
    record.neonFlickers = 0;
    CarDynamics fitted = dynamics;
    Car fittedCar = record;
    fitParts(fitted, fittedCar, data->catalogue);
    loadShapes(fitted.body, data->files(), std::filesystem::path("cars") / record.def->name);
    car->body = std::make_unique<CarBody>(fitted, Vec4{place[0] * 0.01f, place[1] * 0.01f, place[2] * 0.01f, 0.0f});
    car->model = scene::CarModel::load(*host.engine(), record, data->catalogue, gameRandom(), false);
    if (!car->model || car->model->root() == nullptr) return nullptr;
    car->record = &record;
    car->car.kind = CarKind::Player;
    car->car.body = car->body.get();
    car->car.car = &record;
    car->car.model = record.def->id;
    car->car.drive = driveOf(record, data->tables);
    car->car.hasNitro = record.mods[kNitro] != kNoMod;
    car->car.hasTurbo = record.mods[kTurbo] != kNoMod || record.def->stockTurbo;
    // 0x00486164: the car's effects object (0x0048FA40), without its emitters (0x0048FEA0 is the race's): its scale 1
    // in a race of type 4, else 0.4.
    car->car.effects.made = true;
    car->car.effects.scale = data->race.type == RaceType::Cruise ? 1.0f : 0.4f;
    // 0x00485A0E: the exhausts' nodes (EXHAUST_C, else EXHAUST_S with EXHAUST_O, else EXHAUST_S) at their places.
    scene::CarModel& m = *car->model;
    if (m.hasNode("EXHAUST_C")) {
        car->car.exhausts = {true, false};
    } else if (m.hasNode("EXHAUST_S") && m.hasNode("EXHAUST_O")) {
        car->car.exhausts = {true, true};
    } else {
        car->car.exhausts = {m.hasNode("EXHAUST_S"), false};
    }
    if (car->car.exhausts[0]) {
        const char* first = car->car.exhausts[1] || !m.hasNode("EXHAUST_C") ? "EXHAUST_S" : "EXHAUST_C";
        car->exhaustNodes = {m.node(first), car->car.exhausts[1] ? m.node("EXHAUST_O") : nullptr};
    }
    for (size_t i = 0; i < 2; ++i) {
        if (const render::Node* n = car->exhaustNodes[i]; n != nullptr) {
            car->exhaustPlaces[i] = {n->local[3], n->local[7], n->local[11], 0.0f};
        }
    }
    makeWheelPoses(car->wheels, *car->model, *car->body);
    car->body->weighAxles(car->wheels[0].base[2], car->wheels[2].base[2]);
    car->body->raceTune(false, car->car.drive);
    car->chassis = car->model->node("_CHASS");
    car->brakeLight = car->model->meshNode("_BRKLIGHT");
    if (car->brakeLight != nullptr) car->brakeLight->flags |= 4u;
    car->reverseLight = car->model->meshNode("_REVLIGHT");
    if (car->reverseLight != nullptr) car->reverseLight->flags |= 4u;
    scene::CarModel* model = car->model.get();
    view.worldDraws.push_back([model](render::Renderer3d& r, render::Camera& cam) { model->draw(r, cam, 0); });
    return car;
}

// 0x004DD300 with the world ([G+0x50]): the engine's bank (the CarDef's sound, "b" added with an exhaust; its
// instances once), the car's engine sounds (0x00492D20) on its node, turned to the camera (vt[10] 1) with the rolloff
// scale 1 (vt[4]).
void makeSceneSound(WindowHost& host, CutCar& car, const Car& record) {
    GameData* data = host.gameData();
    audio::World* world = gameSounds().world;
    if (world == nullptr || record.def == nullptr) return;
    std::string bank = record.def->engineSound;
    if (record.mods[kExhaust] != kNoMod) bank += "b";
    addWindowBank(bank);
    const void* node = car.model != nullptr ? static_cast<const void*>(car.model->root()) : nullptr;
    car.sound = std::make_unique<CarSound>(*world, GameFiles(std::filesystem::current_path()), car.car, record,
                                           data->catalogue, node, -1, false, 0);
    car.sound->setFocus(true, nullptr, nullptr);
    car.sound->setRolloffScale(1.0f);
}

// 0x004C6B80: the race's type 9; the front end's black over the screen (+0x10C) 1; the camera's stage (+0x24) and
// time (+0x28) 0; LOCATION found (+0x2C); the front end's music at a quarter (0x004C6BCD); the cut scene's 3D sounds
// (0x005B9260: a world of 256 voices); CAR3D (+0x10)
// with outsidegarage in its list at (0, 52, 0) (0x005B3E60), its atmosphere (0x005B4610) and its sky (0x005B4660);
// a world without a route (+0x0C, 0x0053DEA0) with the ground plane at (0, 0, 0) facing up (0x004C7DF0). Between the
// loaders' bracket (0x00498780 / 0x00498840) a car (0x00485530, not of the AI) for each car of the profile's list
// (+0x514, count +0x5F0) at its place: x 825 cm for the second and the third, else 0; z 400 cm for the first two,
// -200 for the others, 300 less with fewer than three cars; y [0x00718FA8] + 2 = 1 cm (Stock problem: the place's
// heading is not set, a word of the stack: 0 here); at rest (0x00486E90), its engine's sounds (0x004DD300), its
// gen_shadow (0x004C71F0), in the world (0x0053E450); its stage (+0x50) 0 and its time (+0x60) its number + 7 s. The
// view in its move mode 2 (CAMERA_AUTO, 0x005B4490) and the camera at (1800, 75, 2000).
void CutScene::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    *this = CutScene{};
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    data->race.type = RaceType::TestTrack;
    host.setScreenFade(1.0f);
    musicQuarter(host);
    // 0x004C6C26: the cut scene's 3D world ([G+0x50], 256 voices); the leave keeps it.
    makeWindowWorld();
    scene::View3d* view = sceneView(s);
    if (view == nullptr || host.engine() == nullptr) return;
    view->addModel("outsidegarage", {0.0f, 52.0f, 0.0f, 0.0f}, gameRandom());
    view->setAtmosphere("outsidegarage");
    view->buildSky();
    m_world = std::make_unique<PhysicsWorld>();
    m_world->collision().planes.push_back({{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}});
    const CareerProfile& p = career.profile;
    const int32_t count = p.raceChoiceCount;
    // Stock problem: the record keeps four cars (+0x30 .. +0x7C); a fifth and more write over its next lists. The list
    // holds one car for the test track (0x004B9670).
    for (int32_t i = 0; i < count && i < 4; ++i) {
        const int32_t at = p.raceChoices[static_cast<size_t>(i)];
        if (at < 0 || static_cast<size_t>(at) >= career.garage.size()) continue;
        Car& record = career.garage[static_cast<size_t>(at)];
        if (record.def == nullptr) continue;
        const CarDynamics* dynamics = data->dynamics->of(*record.def);
        if (dynamics == nullptr) continue;
        float x = 0.0f;
        float z = 400.0f;
        if (i == 1) {
            x = 825.0f;
        } else if (i == 2) {
            x = 825.0f;
            z = -200.0f;
        } else if (i == 3) {
            z = -200.0f;
        }
        if (count < 3) z = z - 300.0f;
        const float y = std::bit_cast<float>(0xBF800000u) + 2.0f;
        std::unique_ptr<CutCar> car = makeSceneCar(host, *view, record, *dynamics, {x, y, z, 0.0f});
        if (!car) continue;
        car->body->resetForStart();
        makeSceneSound(host, *car, record);
        // 0x004C71F0: gen_shadow listed at the car's place, its x and z scaled to the chassis's box over its own
        // (Stock problem: the script puts the shadow's matrix back to a turn and a place before each drawn frame, the
        // scale with it; not made).
        {
            // The cars' folders in use (0x004C6DAF .. 0x004C6FB5).
            const scene::CarFolders folders(view->engine());
            car->shadow = view->addModel("gen_shadow", {x, y, z, 0.0f}, gameRandom());
        }
        m_world->add(*car->body);
        car->stage = CutCarStage::Waiting;
        car->timer = static_cast<float>(i) + 7.0f;
        m_cars.push_back(std::move(car));
    }
    view->setMode(scene::MoveMode::CameraAuto, nullptr);
    m_place = kCutFirstPlace;
    log::info("frontend: the test track's cut scene with {} car(s)", m_cars.size());
}

// 0x004C7030: the first car's sounds unoccluded (vt[8] 0), the 3D world's sounds paused (its manager's +0x20; the world
// kept), the world and the cars freed (their sounds with them), CAR3D's list emptied.
void CutScene::leave(ui::Screen& s) {
    if (!m_cars.empty() && m_cars.front()->sound) m_cars.front()->sound->setOcclusion(0.0f);
    if (audio::World* world = gameSounds().world; world != nullptr) world->manager().pauseAll(true);
    if (scene::View3d* view = sceneView(s); view != nullptr) {
        view->worldDraws.clear();
        while (view->modelCount() > 0) view->removeModel(0);
    }
    *this = CutScene{};
}

// 0x004C72D0, for each car: its gen_shadow's matrix the identity at the body's centre of gravity (x 100, [0x00718FB0]
// = 36 cm lower) turned about y by minus its yaw (+0x21C); then by its stage: 0 brake 1, throttle 0, by hand; 1 brake
// and throttle 1, neutral, by hand; 2 brake 1, throttle 0, by hand, its push (+0x70) 0; 3 no brakes, no throttle, by
// hand, neutral, the velocity (0, 0, push) in its frame (0x004F2F80) and the push up by the window's step within
// 0 .. 1; 4 no brakes, the throttle the stage's time squared (1 from 1 s), by hand, neutral, the velocity (0, 0,
// push); 5 no brake, throttle 0.25, automatic, the steering 0 (0x004203E0, within -1 .. 1); 6 the same with throttle
// 0.16; then no brake, full throttle, automatic, the steering 0. The stage's time runs down by the window's step;
// below 0 the next stage with its time: 1, 2 and 5 1 s, 3 0.4 s, 4 0.2 s, 6 0.5 s, 7 10 s. Then the game's frame
// time within 0.016 .. 0.02 x 0.75: the world steps (0x0053F110; [0x0075F050 ..] cleared) and the car's frame
// (0x00488EE0) with it; the body's definition (+0xC8) loses its traction control (+0x174).
void CutScene::script(CareerHost& host, ui::Screen& s) {
    scene::View3d* view = sceneView(s);
    for (std::unique_ptr<CutCar>& owned : m_cars) {
        CutCar& car = *owned;
        CarBody& body = *car.body;
        if (view != nullptr && car.shadow >= 0 && car.shadow < view->modelCount()) {
            scene::Model& shadow = *view->listedModel(car.shadow);
            if (shadow.root != nullptr) {
                Mat34& m = shadow.root->local;
                m = {1.0f, 0.0f, 0.0f, body.position[0] * 100.0f, 0.0f, 1.0f, 0.0f,
                     body.position[1] * 100.0f - 36.0f, 0.0f, 0.0f, 1.0f, body.position[2] * 100.0f};
                rotateY(m, -body.angles[1]);
                shadow.root->invalidate();
            }
        }
        switch (car.stage) {
        case CutCarStage::Waiting:
            body.brake = 1.0f;
            body.throttle = 0.0f;
            body.automatic = false;
            break;
        case CutCarStage::Revving:
            body.brake = 1.0f;
            body.throttle = 1.0f;
            body.setGear(1);
            body.automatic = false;
            break;
        case CutCarStage::Braked:
            body.brake = 1.0f;
            body.throttle = 0.0f;
            body.automatic = false;
            car.push = 0.0f;
            break;
        case CutCarStage::Rolling:
            body.handbrake = 0.0f;
            body.brake = 0.0f;
            body.throttle = 0.0f;
            body.automatic = false;
            body.setGear(1);
            body.setLocalVelocity({0.0f, 0.0f, car.push, 0.0f});
            car.push = std::clamp(host.frameTime() + car.push, 0.0f, 1.0f);
            break;
        case CutCarStage::Launching:
            body.handbrake = 0.0f;
            body.brake = 0.0f;
            body.throttle = car.timer >= 1.0f ? 1.0f : car.timer * car.timer;
            body.automatic = false;
            body.setGear(1);
            body.setLocalVelocity({0.0f, 0.0f, car.push, 0.0f});
            break;
        case CutCarStage::Driving:
            body.brake = 0.0f;
            body.throttle = 0.25f;
            body.automatic = true;
            body.steering = 0.0f;
            break;
        case CutCarStage::Easing:
            body.brake = 0.0f;
            body.throttle = std::bit_cast<float>(0x3E23D70Bu);
            body.automatic = true;
            body.steering = 0.0f;
            break;
        default:
            body.brake = 0.0f;
            body.throttle = 1.0f;
            body.automatic = true;
            body.steering = 0.0f;
            break;
        }
        car.timer = car.timer - host.frameTime();
        if (car.timer < 0.0f) {
            car.stage = static_cast<CutCarStage>(static_cast<int32_t>(car.stage) + 1);
            switch (car.stage) {
            case CutCarStage::Revving:
            case CutCarStage::Braked:
            case CutCarStage::Driving: car.timer = 1.0f; break;
            case CutCarStage::Rolling: car.timer = std::bit_cast<float>(0x3ECCCCCDu); break;
            case CutCarStage::Launching: car.timer = std::bit_cast<float>(0x3E4CCCCDu); break;
            case CutCarStage::Easing: car.timer = 0.5f; break;
            case CutCarStage::FullThrottle: car.timer = 10.0f; break;
            default: break;
            }
        }
        const float step = std::clamp(host.gameFrameTime(), 0.016f, 0.02f) * 0.75f;
        m_world->pools().empty();
        m_world->step(step);
        cutCarFrame(host, car, step, m_world->pools());
        body.setAsr(false);
    }
}

// 0x00488EE0 for a car of a garage's scene: the damage's events of the frame cleared (+0x4F4 .. +0x504), the wheels
// (0x00487670), the tilt held (0x00489740), the car's own frame (0x004888B0) and the slipstream (0x00487D50, which
// returns at once without a race); the chassis node at (0, -the model's MAXDROPHEIGHT x the car's +0x58, 0); the brake
// and reversing lights (0x00487C70); the exhausts (0x00487220); the damage's effects (0x00488B50) and the wheels'
// (0x00487FE0, without a race); the sounds (0x004875F0, car_rolling's engine). Such a car has no competitor (+0x494)
// and no tracker (+0x4D4). The model follows the body (the world's step, 0x004F01E0).
void cutCarFrame(WindowHost& host, CutCar& car, float step, ContactPools& pools) {
    GameData* data = host.gameData();
    CarBody& body = *car.body;
    car.car.steeringFault = false;
    car.car.turboLost = false;
    car.car.nitroLost = false;
    car.car.neonDamaged = false;
    const bool pulls = car.record != nullptr && car.record->steeringPulls != 0;
    wheelsFrame(car.wheels, body, *car.model, pulls, car.car.pullSide, car.onStand, step, nullptr);
    tiltGuard(car.car);
    carFrame(car.car, step, host.gameFrameTime(), data->race.type);
    if (car.chassis != nullptr && car.record != nullptr && car.record->def != nullptr) {
        const float drop = car.record->def->maxDropHeight * std::bit_cast<float>(car.record->lowering);
        car.chassis->local[3] = 0.0f;
        car.chassis->local[7] = -drop;
        car.chassis->local[11] = 0.0f;
        car.chassis->invalidate();
    }
    if (car.brakeLight != nullptr && !car.car.rearLightsOut) {
        if (body.brake != 0.0f) {
            car.brakeLight->flags &= ~4u;
        } else {
            car.brakeLight->flags |= 4u;
        }
    }
    if (car.reverseLight != nullptr && !car.car.rearPartOut) {
        if (body.gear == 0) {
            car.reverseLight->flags &= ~4u;
        } else {
            car.reverseLight->flags |= 4u;
        }
    }
    // 0x004875F0: +0x43C down to 0; with the engine's sounds (car_rolling's) their frame and the gear told.
    car.car.gearChange = car.car.gearChange - 1 < 0 ? 0 : car.car.gearChange - 1;
    if (car.sound) {
        CarSound::Frame f;
        f.dt = host.gameFrameTime();
        f.raceState = false;
        car.sound->frame(f, gameRandom());
        const uint32_t g = body.gear;
        if (g > car.car.lastGear) {
            car.sound->gearUp(f, gameRandom());
            car.car.lastGear = g;
            car.car.gearChange = 2;
        } else if (g < car.car.lastGear) {
            car.sound->gearDown();
            car.car.lastGear = g;
        }
    }
    // 0x00487220: the exhausts moved in y by -0.2 x the acceleration in the body's y within -6 .. 6; on the stand past
    // 2000 rpm by rand() % 100 x 0.01 - 0.5 instead.
    float lift = -(body.localAcceleration[1] * 0.2f);
    if (lift < -6.0f) {
        lift = -6.0f;
    } else if (lift > 6.0f) {
        lift = 6.0f;
    }
    if (car.onStand && body.engineSpeed * 9.549296f > 2000.0f) {
        lift = static_cast<float>(gameRandom().next() % 100) * 0.01f - 0.5f;
    }
    for (size_t i = 0; i < 2; ++i) {
        render::Node* n = car.exhaustNodes[i];
        if (n == nullptr) continue;
        n->local[3] = car.exhaustPlaces[i][0];
        n->local[7] = car.exhaustPlaces[i][1] + lift;
        n->local[11] = car.exhaustPlaces[i][2];
        n->invalidate();
    }
    const bool rearPartOut = car.car.rearPartOut;
    damageEffects(car.car, gameRandom());
    if (!rearPartOut && car.car.rearPartOut && car.reverseLight != nullptr) car.reverseLight->flags |= 4u;
    wheelEffects(car.car, step, host.gameFrameTime(), gameRandom(), pools, nullptr, false);
    if (render::Node* root = car.model->root(); root != nullptr) {
        root->local = body.modelMatrix();
        root->invalidate();
    }
}

// 0x004C7870: the camera by its stage: 1 looks at (0, 0, 0) at once and moves 450 cm a second along x, the front
// end's black 1 - the stage's time while under 1 s; 2 looks at (0, 50, 100), 25 cm a second back along x, the black
// likewise; 3 looks at (0, 0, 0), 150 cm a second along z, the black likewise (and the music told, not made); 4 the
// black 1. Its time runs down by the window's step; below 0 the next stage: 1 at (-3800, 150, 3000) for 5 s, 2 at
// (600, 70, 250) for 5 s, 3 at (250, 75, 700) for 8 s with the black 1, 4 the test track's race (0x004A4060) when the
// career's +0xA8 is set, else CMSAtMeeting (mode 3), and +0xA8 cleared. The view's camera goes to the place.
void CutScene::camera(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    scene::View3d* view = sceneView(s);
    if (view == nullptr) return;
    switch (m_stage) {
    case CutCameraShot::Sweep:
        view->lookAt({0.0f, 0.0f, 0.0f, 0.0f}, true);
        m_place[0] = host.frameTime() * 450.0f + m_place[0];
        if (m_timer < 1.0f) host.setScreenFade(1.0f - m_timer);
        break;
    case CutCameraShot::CloseUp:
        view->lookAt({0.0f, 50.0f, 100.0f, 0.0f}, true);
        m_place[0] = m_place[0] - host.frameTime() * 25.0f;
        if (m_timer < 1.0f) host.setScreenFade(1.0f - m_timer);
        break;
    case CutCameraShot::Follow:
        view->lookAt({0.0f, 0.0f, 0.0f, 0.0f}, true);
        m_place[2] = host.frameTime() * 150.0f + m_place[2];
        if (m_timer < 1.0f) host.setScreenFade(1.0f - m_timer);
        break;
    case CutCameraShot::End: host.setScreenFade(1.0f); break;
    default:
        break;
    }
    m_timer = m_timer - host.frameTime();
    if (m_timer < 0.0f) {
        m_stage = static_cast<CutCameraShot>(static_cast<int32_t>(m_stage) + 1);
        switch (m_stage) {
        case CutCameraShot::Sweep:
            m_place = kCutPlaces[0];
            m_timer = 5.0f;
            break;
        case CutCameraShot::CloseUp:
            m_place = kCutPlaces[1];
            m_timer = 5.0f;
            break;
        case CutCameraShot::Follow:
            m_place = kCutPlaces[2];
            m_timer = 8.0f;
            host.setScreenFade(1.0f);
            break;
        case CutCameraShot::End:
            if (data->careerTestTrack != 0) {
                testTrackRace(host);
            } else {
                host.request("CMSAtMeeting", SwitchMode::ReplaceAfterTransitions);
            }
            data->careerTestTrack = 0;
            break;
        default:
            break;
        }
    }
    view->setCameraPlace(m_place);
}

// 0x004A4060: the test track's race (type 9, 99999 laps, the morning, dry, Angel SuperSpeedway's Speedway_R1, one
// player, the settings' +0x84 for +0x54), loading asked for, the cruise's +0x10 set.
void CutScene::testTrackRace(CareerHost& host) {
    GameData* data = host.gameData();
    RaceSetup& race = data->race;
    race.type = RaceType::TestTrack;
    race.laps = 99999;
    race.timeOfDay = 1;
    race.weather = 1;
    race.track = "Angel SuperSpeedway";
    race.route = "Speedway_R1";
    race.players = 1;
    race.gearbox = static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
    host.request("loading", SwitchMode::Replace);
    data->career->cruise.started = 1;
}

// 0x004C7BF0: with a car the script (0x004C72D0) and the camera (0x004C7870), the 3D world's listener (its +0x28 ..);
// the first car's sounds' occlusion (vt[8]: 1 while its stage is under 7, else 0.3; Stock problem: asked without a car
// too, none here); the 3D world's frame (0x005B96C0); the view's zoom and its aim 1. SELECT (CLICK):
// before the camera's third stage its time and every car's -1 (each goes on at once); else the test track's race
// (0x004A4060) when the career's +0xA8 is set, else CMSAtMeeting, and +0xA8 cleared. BACK (CLICK): the career's +0xA4
// 6 and garageload.
void CutScene::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    audio::World* world = gameSounds().world;
    if (!m_cars.empty()) {
        script(host, s);
        camera(host, s);
        // 0x004C7C14: the 3D world's listener: one, the view's camera's place and its view matrix (its +0x48,
        // the world matrix's inverse).
        if (scene::View3d* view = sceneView(s); view != nullptr && world != nullptr) {
            const Mat34& m = view->camera().world();
            world->manager().setListenerData({m[3], m[7], m[11], 0.0f}, view->camera().view().data());
        }
    }
    if (!m_cars.empty() && m_cars.front()->sound) {
        m_cars.front()->sound->setOcclusion(m_cars.front()->stage < CutCarStage::FullThrottle ? 1.0f : 0.3f);
    }
    // 0x005B96C0 with the game's frame time.
    if (world != nullptr) world->update(host.gameFrameTime());
    if (scene::View3d* view = sceneView(s); view != nullptr) {
        view->setZoom(1.0f);
        view->setZoomTarget(1.0f);
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        if (m_stage < CutCameraShot::Follow) {
            m_timer = -1.0f;
            for (std::unique_ptr<CutCar>& car : m_cars) car->timer = -1.0f;
        } else {
            if (data->careerTestTrack == 0) {
                host.request("CMSAtMeeting", SwitchMode::Replace);
            } else {
                testTrackRace(host);
            }
            data->careerTestTrack = 0;
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        data->careerScreen = CareerScreen::CarSelBack;
        host.request("garageload", SwitchMode::Replace);
    }
}

}  // namespace oj::frontend
