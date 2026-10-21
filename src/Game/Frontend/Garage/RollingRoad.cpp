// car_rolling (record 0x007228D0, vtable 0x006F60EC: enter 0x004DD490, update 0x004DF260, draw 0x004DF660, leave
// 0x004DDF90; docs/spec/14-career-windows.md "car_rolling"): the garage's dyno. The garage's car stands on two rolling
// roads, held at its place, and runs up through its gears; its revs, speed, torque and power are read off, the best
// kept by the car and drawn as a graph.

#include "Game/Frontend/Garage/RollingRoad.h"

#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/GameTexts.h"
#include "Game/Cars/CarSound.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/WindowTools.h"
#include "Game/Media/Music.h"

#include <algorithm>
#include <format>

namespace oj::frontend {

namespace {

// [0x007191C8]: the revs between two points of the graph.
constexpr float kGraphStep = 74.62f;

// 0x00534E30: the first node below the model's top node (depth first) of that name, case aside.
render::Node* namedNode(scene::Model* model, const char* name) {
    if (model == nullptr) return nullptr;
    for (const auto& [nodeName, node] : model->named) {
        if (_stricmp(nodeName.c_str(), name) == 0) return node;
    }
    return nullptr;
}

// The external force (+0x278) gets (0, -10000, 0) (0x00489970 with [0x007A2EC4] or [0x007A2EB4]).
void pressDown(CarBody& body) {
    body.externalForce[0] = body.externalForce[0] + 0.0f;
    body.externalForce[1] = body.externalForce[1] + -10000.0f;
    body.externalForce[2] = body.externalForce[2] + 0.0f;
}

std::wstring widen(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

}  // namespace

scene::View3d* RollingRoad::frameView(ui::Screen& s) {
    return dynamic_cast<scene::View3d*>(s.view("FRAME_CAR3D"));
}

// MSG: the text (0x005B19C0), its transition started again (0x005B2A50) and its highlight
// (vtable +0x0C, 0x005B2930).
void RollingRoad::showMessage(GarageHost& host, ui::Screen& s, size_t text, bool highlighted) {
    const TextTable& texts = host.gameTexts();
    const int msg = m_msg;
    if (msg < 0) return;
    s.setText(msg, texts.at(text));
    s.restartTransition(s.nameOf(msg));
    s.setHighlighted(msg, highlighted);
}

// 0x004DD490: [game + 4]'s method +0x38 with 1 (empty on PC); the front end's music to 0 (its target only, +0x14),
// the game's music to a quarter of the settings' volume (its target, +0x1C); the dyno's 3D world (0x005B9260, 256
// voices, makeWindowWorld); FRAME_CAR3D (+0x10) with rollingarage listed at (0, -3, 0); DATA_TCLIGHT (+0x40), DATA_TCLIGHTTEXT (+0x44). Between
// the loaders' bracket (0x00498780 / 0x00498840) the garage's car (the dealer's list +0x27D8 at +0x27DC) made a car
// at (0, 1, 0) (0x00485530, Stock problem: the place's heading unset, 0 here; +0x14), at rest (0x00486E90), by hand
// (+0x364 0), on the stand (+0x19C 1), its engine sounds (0x004DD300), and gen_shadow at (0, 0, 0) sized to its
// _CHASS x 1.25 in x and x 1 in z (0x004DE250). The view looks at (0, 50, 0) at once (0x005B4380) in move mode 4
// (0x005B4490); a world without a route (+0x0C) with the ground plane (0x004C7DF0) and the car in it. The texts'
// elements (+0x18 .. +0x3C); MSG "START DYNO TEST" (97), its transition started again and highlighted; the
// readings' bests (+0x48 .. +0x5C) 0. The body's DragCoefficient 0. ROLLING_ROAD listed twice at (0, -3, z): under
// the front wheels (the first pose's z less the centre of gravity's z x 100), turned about y by 3.14 (0x005B3520 with
// [0x007A2F3C]), and under the rear wheels (the third pose's); their ROLLINGFRONT and ROLLINGREAR nodes (0x00534E30,
// +0x60 .. +0x6C). The state (+0x78) 0, the test's gear (+0x80) 4; every DATA_ and RESULT_ element hidden. The graph
// RESULT (+0x70, 0x004F8610) of the last TorqueCurveX point (rpm) / 74.62 + 10 points (cut) with the series TORQUE
// and BHP, running; its revs (+0x74) the first TorqueCurveX point, its points up to them 0 every 74.62 rpm. With
// DATA_SPEED_TEXT, RESULT_SPEEDTEXT, DATA_TORQUE_TEXT, RESULT_TORQUETEXT, DATA_BHP_TEXT and RESULT_BHPTEXT: by the
// settings' miles (+0x94) DATA_SPEED_TEXT "MPH" (1494) or "KPH" (1495), RESULT_SPEEDTEXT
// " (MPH)" or " (KPH)" more, both torque texts " (lbft)" or " (NM)" more; both power texts " (bhp)" (484) more. Every
// SEL_ element shown.
void RollingRoad::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    *this = RollingRoad{};
    host.setMusicVolume(0.0f, true);
    if (Jukebox& music = jukebox(); music.player() != nullptr && music.handle != 0) {
        music.target = host.settingsMusic() * 0.25f;  // [0x006FFBEC]
    }
    makeWindowWorld();
    if (data == nullptr || !data->career) return;
    scene::View3d* view = frameView(s);
    if (view == nullptr || host.engine() == nullptr) return;
    view->addModel("rollingarage", {0.0f, -3.0f, 0.0f, 0.0f}, gameRandom());
    m_tcLight = s.find("DATA_TCLIGHT");
    m_tcLightText = s.find("DATA_TCLIGHTTEXT");
    Car* record = host.garage().car(host);
    const CarDynamics* dynamics =
        record != nullptr && record->def != nullptr ? data->dynamics->of(*record->def) : nullptr;
    if (dynamics != nullptr) m_car = makeSceneCar(host, *view, *record, *dynamics, {0.0f, 1.0f, 0.0f, 0.0f});
    if (!m_car) return;
    CarBody& body = *m_car->body;
    body.resetForStart();
    body.automatic = false;
    m_car->onStand = true;
    m_car->car.onStand = true;
    makeSceneSound(host, *m_car, *record);
    // 0x004DE250, with the cars' folders in use (0x004DD5DB .. 0x004DD6FB).
    int shadow = -1;
    {
        const scene::CarFolders folders(view->engine());
        shadow = view->addModel("gen_shadow", {0.0f, 0.0f, 0.0f, 0.0f}, gameRandom());
    }
    if (shadow >= 0) {
        scene::Model& m = *view->listedModel(shadow);
        render::Node* mesh = m.meshNodes.empty() ? m.root : m.meshNodes.front().get();
        std::array<float, 3> carLow{}, carHigh{}, low{}, high{};
        if (m.root != nullptr && scene::meshBox(m_car->chassis, carLow, carHigh) && scene::meshBox(mesh, low, high)) {
            m.root->local[10] = (carHigh[2] - carLow[2]) * 1.0f / (high[2] - low[2]);
            m.root->local[0] = (carHigh[0] - carLow[0]) * 1.25f / (high[0] - low[0]);
            m.root->invalidate();
        }
    }
    view->lookAt({0.0f, 50.0f, 0.0f, 0.0f}, true);
    view->setMode(scene::MoveMode::Map, nullptr);
    m_world = std::make_unique<PhysicsWorld>();
    m_world->collision().planes.push_back({{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}});
    m_world->add(body);
    m_revs = s.find("DATA_REVS");
    m_gear = s.find("DATA_GEAR");
    m_speed = s.find("DATA_SPEED");
    m_torque = s.find("DATA_TORQUE");
    m_bhp = s.find("DATA_BHP");
    m_maxSpeed = s.find("DATA_MAXSPEED");
    m_maxTorque = s.find("DATA_MAXTORQUE");
    m_maxRevs = s.find("DATA_MAXREVS");
    m_maxBhp = s.find("DATA_MAXBHP");
    m_msg = s.find("MSG");
    showMessage(host, s, kTextStartDyno, true);
    body.setDrag(0.0f);
    const float gravity = body.dynamics().body.centreOfGravity[2] * 100.0f;
    const int front = view->addModel("ROLLING_ROAD", {0.0f, -3.0f, m_car->wheels[0].base[2] - gravity, 0.0f},
                                     gameRandom());
    if (front >= 0) {
        view->placeModel(front, {0.0f, -3.0f, m_car->wheels[0].base[2] - gravity, 0.0f},
                         {0.0f, std::bit_cast<float>(0x4048F5C3u), 0.0f, 0.0f}, 1.0f);
        m_rollers[0] = namedNode(view->listedModel(front), "ROLLINGFRONT");
        m_rollers[1] = namedNode(view->listedModel(front), "ROLLINGREAR");
    }
    const int rear = view->addModel("ROLLING_ROAD", {0.0f, -3.0f, m_car->wheels[2].base[2] - gravity, 0.0f},
                                    gameRandom());
    if (rear >= 0) {
        m_rollers[2] = namedNode(view->listedModel(rear), "ROLLINGFRONT");
        m_rollers[3] = namedNode(view->listedModel(rear), "ROLLINGREAR");
    }
    m_state = DynoState::Ready;
    m_testGear = 4;
    s.hideContaining("DATA_", false);
    s.hideContaining("RESULT_", false);
    const std::vector<float>& speeds = body.dynamics().torqueSpeeds;
    const float last = speeds.empty() ? 0.0f : speeds.back();
    m_graph = std::make_unique<Graph>("RESULT", static_cast<uint32_t>(x87::chop(last / kGraphStep * 9.549296f) + 10));
    m_graph->addSeries("TORQUE");
    m_graph->addSeries("BHP");
    m_graph->running = !m_graph->running;
    m_graphRevs = (speeds.empty() ? 0.0f : speeds.front()) * 9.549296f;
    for (float revs = 0.0f; revs < m_graphRevs; revs = revs + kGraphStep) {
        if (m_graph->running && !m_graph->series.empty()) m_graph->series[0].values[m_graph->next] = 0.0f;
        if (m_graph->running && m_graph->series.size() > 1) m_graph->series[1].values[m_graph->next] = 0.0f;
        m_graph->advance();
    }
    const int speedText = s.find("DATA_SPEED_TEXT");
    const int resultSpeed = s.find("RESULT_SPEEDTEXT");
    const int torqueText = s.find("DATA_TORQUE_TEXT");
    const int resultTorque = s.find("RESULT_TORQUETEXT");
    const int bhpText = s.find("DATA_BHP_TEXT");
    const int resultBhp = s.find("RESULT_BHPTEXT");
    if (speedText >= 0 && resultSpeed >= 0 && torqueText >= 0 && resultTorque >= 0 && bhpText >= 0 &&
        resultBhp >= 0) {
        const bool miles = host.settingsValue(save::Settings::kMph) != 0;
        const std::wstring& unit = texts.at(miles ? kTextMph : kTextKph);
        s.setText(speedText, unit);
        s.appendText(resultSpeed, L" (");
        s.appendText(resultSpeed, unit);
        s.appendText(resultSpeed, L")");
        s.appendText(torqueText, miles ? L" (lbft)" : L" (NM)");
        s.appendText(resultTorque, miles ? L" (lbft)" : L" (NM)");
        for (int e : {bhpText, resultBhp}) {
            s.appendText(e, L" (");
            s.appendText(e, texts.at(kTextBhp));
            s.appendText(e, L")");
        }
    }
    s.showContaining("SEL_", false);
    log::info("frontend: the dyno with {}", record->def->name);
}

// 0x004DDF90: [game + 4]'s method +0x38 with 2 (empty on PC); the game's music's target back to the settings'
// volume (the front end's is left at 0); the graph, the world, the
// car's sound and the car freed; the rollers let go; FRAME_CAR3D's list emptied; the 3D world released.
void RollingRoad::leave(GarageHost& host, ui::Screen& s) {
    if (Jukebox& music = jukebox(); music.player() != nullptr && music.handle != 0) music.target = host.settingsMusic();
    // The car's sounds unoccluded and freed with the car, then the world ([G+0x50]).
    if (m_car && m_car->sound) {
        m_car->sound->setOcclusion(0.0f);
        m_car->sound.reset();
    }
    m_car.reset();
    freeWindowWorld();
    if (scene::View3d* view = frameView(s); view != nullptr) {
        view->worldDraws.clear();
        while (view->modelCount() > 0) view->removeModel(0);
    }
    *this = RollingRoad{};
}

// 0x004DE350 by the state: 1 no brake, the throttle 1 while the countdown is over 3 s, else 0, by hand, neutral, the
// traction control's light and text hidden; 2 by hand, no brake, the throttle 0.49 in neutral, else 1; past 0.6 of the
// last TorqueCurveX point (rpm, 0x0044D930) in neutral or past 0.97 of it the wait (+0x5C) runs down by the window's
// step within 0 .. 2 s, and at 0 the next gear (0x004F7610) with 2 s to wait, or with the last gear reached state 3,
// each with (0, -10000, 0) on the external force; the last: WARNING, MSG "RUN COMPLETE" (100) not highlighted;
// 3 the lights hidden, the brake 0.512, no throttle; under 10 m/s forward state 4: SEL_ shown, WARNING, MSG
// "EXIT DYNO" (101) highlighted, the DATA elements going out and the RESULT_ ones coming in, RESULT_SPEED,
// RESULT_TORQUE and RESULT_BHP the texts of DATA_MAXSPEED, DATA_MAXTORQUE and DATA_MAXBHP, RESULT_SPEED_OLD,
// RESULT_BHP_OLD and RESULT_TORQUE_OLD the car's last run (+0x94, +0x90, +0x8C; "%03d" cut, "---" for -1 and below),
// RESULT_KEY5 "LAST RUN" (1600); the run's bests kept by the car; RESULT_BHPREV the revs of the best power ("%04d",
// cut); the graph's drawn points and next point 0, its scales 136 / (the best x 1.2); 4 nothing; else brake 1, no
// throttle, by hand. Then the pools emptied and the world stepping the game's frame time within 0.016 .. 0.02 s, the
// car's frame (0x00488EE0); the body held at x and z 0 (its node and state with it), its turn and its angles 0
// (0x004F3080, 0x004F3150); the poses of the wheels not driven not turning; the definition's traction control on
// (+0x174 1).
void RollingRoad::script(GarageHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    CarBody& body = *m_car->body;
    const auto hideLights = [&] {
        if (m_tcLight >= 0) s.setHidden(m_tcLight, true);
        if (m_tcLightText >= 0) s.setHidden(m_tcLightText, true);
    };
    switch (m_state) {
    case DynoState::Starting:
        body.brake = 0.0f;
        body.throttle = m_countdown > 3.0f ? 1.0f : 0.0f;
        body.automatic = false;
        body.setGear(1);
        hideLights();
        break;
    case DynoState::Running: {
        body.automatic = false;
        body.brake = 0.0f;
        body.throttle = body.gear == 1 ? std::bit_cast<float>(0x3EFAE147u) : 1.0f;
        const std::vector<float>& speeds = body.dynamics().torqueSpeeds;
        const float top = (speeds.empty() ? 0.0f : speeds.back()) * 9.549296f;
        const float revs = body.engineSpeed * 9.549296f;
        const bool rising = (body.gear == 1 && top * 0.6f < revs) || top * 0.97f < revs;
        if (!rising) break;
        if (m_shiftWait > 0.0f) m_shiftWait = m_shiftWait - host.frameTime();
        m_shiftWait = std::clamp(m_shiftWait, 0.0f, 2.0f);
        if (m_shiftWait != 0.0f) break;
        const auto gears = static_cast<uint32_t>(body.dynamics().forwardRatios.size() + 2);
        if (body.gear < gears - 1) {
            pressDown(body);
            body.setGear(body.gear + 1);
            m_shiftWait = 2.0f;
        } else {
            m_state = DynoState::Braking;
            pressDown(body);
            host.sound("WARNING");
            showMessage(host, s, kTextRunComplete, false);
        }
        break;
    }
    case DynoState::Braking: {
        hideLights();
        body.brake = std::bit_cast<float>(0x3F03126Fu);
        body.throttle = 0.0f;
        if (!(body.localVelocity[2] < 10.0f)) break;
        m_state = DynoState::Results;
        s.showContaining("SEL_", false);
        host.sound("WARNING");
        showMessage(host, s, kTextExitDyno, true);
        s.hideContaining("DATA", true);
        s.showContaining("RESULT_", true);
        s.setText("RESULT_SPEED", s.textOf(s.nameOf(m_maxSpeed)));
        s.setText("RESULT_TORQUE", s.textOf(s.nameOf(m_maxTorque)));
        s.setText("RESULT_BHP", s.textOf(s.nameOf(m_maxBhp)));
        Car& record = *m_car->record;
        const auto old = [&](uint32_t bits) {
            const float v = std::bit_cast<float>(bits);
            return v > -1.0f ? std::format("{:03}", x87::chop(v)) : std::string("---");
        };
        s.setText("RESULT_SPEED_OLD", widen(old(record.dynoRun[2])));
        s.setText("RESULT_BHP_OLD", widen(old(record.dynoRun[1])));
        s.setText("RESULT_TORQUE_OLD", widen(old(record.dynoRun[0])));
        s.setText("RESULT_KEY5", texts.at(kTextLastRun));
        s.showContaining("RESULT_", true);
        record.dynoRun[2] = std::bit_cast<uint32_t>(m_topSpeed);
        record.dynoRun[1] = std::bit_cast<uint32_t>(m_topBhp);
        record.dynoRun[0] = std::bit_cast<uint32_t>(m_topTorque);
        s.setText("RESULT_BHPREV", widen(std::format("{:04}", x87::chop(m_bhpRevs))));
        if (m_graph) {
            m_graph->drawn = 0;
            m_graph->next = 0;
            m_graph->setScale(0, 136.0f / (m_topTorque * 1.2f));
            m_graph->setScale(1, 136.0f / (m_topBhp * 1.2f));
        }
        break;
    }
    case DynoState::Results: break;
    default:
        body.brake = 1.0f;
        body.throttle = 0.0f;
        body.automatic = false;
        break;
    }
    const float step = std::clamp(host.gameFrameTime(), 0.016f, 0.02f);
    m_world->pools().empty();
    m_world->step(step);
    cutCarFrame(host, *m_car, step, m_world->pools());
    body.position[0] = 0.0f;
    body.position[2] = 0.0f;
    body.syncNode();
    body.state[RigidBody::kPosition] = body.position[0];
    body.state[RigidBody::kPosition + 1] = body.position[1];
    body.state[RigidBody::kPosition + 2] = body.position[2];
    body.setLocalAngularVelocity({0.0f, 0.0f, 0.0f, 0.0f});
    body.setAngles({0.0f, 0.0f, 0.0f, 0.0f});
    // The stock game's body's node is the car's model's top node: 0x004F01E0 (from 0x004F3150 with 1) puts the model
    // where the body is held at once. The car's frame placed the model by the body before the hold (where the step
    // left it, the speed's step ahead), so the model takes the held place again.
    if (render::Node* root = m_car->model->root(); root != nullptr) {
        root->local = body.modelMatrix();
        root->invalidate();
    }
    const std::vector<WheelDef>& wheels = body.dynamics().wheels;
    for (size_t i = 0; i < wheels.size() && i < m_car->wheels.size(); ++i) {
        if (wheels[i].drive == 0) m_car->wheels[i].spin = 0.0f;
    }
    body.setAsr(true);
}

// 0x004DEBA0: DATA_REVS the revs ("%03d", cut), the best of them (+0x54) in DATA_MAXREVS; DATA_GEAR "R", "N", "1" ..
// "6"; DATA_SPEED the speed forward (under 0 none) in mph (x 2.235) or km/h (x 3.6) by the settings, the best (+0x48)
// in DATA_MAXSPEED. In state 2: DATA_TORQUE the engine's torque out (+0x360, under 0 none) x 0.29 (x 1.335818 in
// newton metres) and rand() & 3 - 2, the best (+0x4C) in DATA_MAXTORQUE; DATA_BHP the torque x the engine's speed x
// 0.000481, a new best (+0x50) keeping its revs and rand() % 10 - 5 (+0x58), in DATA_MAXBHP; in fourth gear past the
// graph's revs without the traction control at work (+0x388) the graph's next point the torque and the power each
// with rand() % 6 - 3, the point after, the graph's revs 74.62 more.
void RollingRoad::readings(GarageHost& host, ui::Screen& s) {
    CarBody& body = *m_car->body;
    if (m_revs >= 0) {
        const float revs = body.engineSpeed * 9.549296f;
        s.setText(m_revs, widen(std::format("{:03}", x87::chop(revs))));
        if (revs > m_topRevs) m_topRevs = revs;
        if (m_maxRevs >= 0) s.setText(m_maxRevs, widen(std::format("{:03}", x87::chop(m_topRevs))));
    }
    if (m_gear >= 0 && body.gear <= 7) {
        static const char* const kGears[] = {"R", "N", "1", "2", "3", "4", "5", "6"};
        s.setText(m_gear, widen(kGears[body.gear]));
    }
    if (m_speed >= 0) {
        float v = body.localVelocity[2];
        if (v < 0.0f) v = 0.0f;
        v = v * (host.settingsValue(save::Settings::kMph) != 0 ? 2.235f : 3.6f);
        s.setText(m_speed, widen(std::format("{:03}", x87::chop(v))));
        if (v > m_topSpeed) m_topSpeed = v;
        if (m_maxSpeed >= 0) s.setText(m_maxSpeed, widen(std::format("{:03}", x87::chop(m_topSpeed))));
    }
    if (m_state != DynoState::Running) return;
    float torque = 0.0f;
    float power = 0.0f;
    if (m_torque >= 0) {
        torque = body.engineTorqueOut * 0.29f;
        const int jitter = (gameRandom().next() & 3) - 2;
        if (torque < 0.0f) torque = 0.0f;
        if (host.settingsValue(save::Settings::kMph) == 0) torque = torque * std::bit_cast<float>(0x3FAAFC16u);
        torque = static_cast<float>(jitter) + torque;
        s.setText(m_torque, widen(std::format("{:03}", x87::chop(torque))));
        if (torque > m_topTorque) m_topTorque = torque;
        if (m_maxTorque >= 0) s.setText(m_maxTorque, widen(std::format("{:03}", x87::chop(m_topTorque))));
    }
    if (m_bhp >= 0) {
        const float out = body.engineTorqueOut < 0.0f ? 0.0f : body.engineTorqueOut;
        power = out * body.engineSpeed * 1.0f * std::bit_cast<float>(0x39FC335Bu);
        s.setText(m_bhp, widen(std::format("{:03}", x87::chop(power))));
        if (power > m_topBhp) {
            m_topBhp = power;
            m_bhpRevs = body.engineSpeed * 9.549296f;
            m_bhpRevs = static_cast<float>(gameRandom().next() % 10 - 5) + m_bhpRevs;
        }
        if (m_maxBhp >= 0) s.setText(m_maxBhp, widen(std::format("{:03}", x87::chop(m_topBhp))));
    }
    if (!m_graph || body.gear != 4) return;
    if (!(body.engineSpeed * 9.549296f > m_graphRevs) || body.asrActive) return;
    m_graph->set(0, static_cast<float>(gameRandom().next() % 6 - 3) + torque);
    m_graph->set(1, static_cast<float>(gameRandom().next() % 6 - 3) + power);
    m_graph->advance();
    m_graphRevs = kGraphStep + m_graphRevs;
}

// 0x004DF180: a driven front wheel (0) turns the first road's rollers about x by -0.0175 x its spin, a driven rear
// wheel (2) the second road's by 0.0175 x its spin (0x00534A10).
void RollingRoad::rollers() {
    CarBody& body = *m_car->body;
    const auto turn = [&](int32_t wheel, size_t first, float sign) {
        CarWheel* w = body.wheelOf(wheel);
        if (w == nullptr || w->def->drive == 0) return;
        const float angle = sign * (0.0175f * w->spin);
        for (size_t i = first; i < first + 2; ++i) {
            if (m_rollers[i] == nullptr) continue;
            rotateX(m_rollers[i]->local, angle);
            m_rollers[i]->invalidate();
        }
    };
    turn(0, 0, -1.0f);
    turn(2, 2, 1.0f);
}

// 0x004DF260: with a car the script (0x004DE350), the 3D world's listener (the view's camera), the readings
// (0x004DEBA0) and the rollers (0x004DF180); the car's sounds' occlusion 1 and the world's frame (0x005B96C0); the
// view's zoom and its aim 1. BACK
// (CLICK): the career's +0xA4 6 and garageload. SELECT in state 0 (CLICK): state 1, the DATA_ elements coming in, the
// bests 0, the countdown (+0x7C) 4 s, MSG "INITIALIZING" (98) not highlighted, every SEL_ element hidden; in state 4
// (CLICK) as BACK. In state 1 the countdown runs down by the window's step; past 0: (0, -10000, 0) on the external
// force, the test's gear (+0x80), state 2, WARNING, the bests and the countdown 0, MSG "ANALYZING" (99) not
// highlighted.
void RollingRoad::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    audio::World* world = gameSounds().world;
    if (m_car) {
        script(host, s);
        // The 3D world's listener: one, the view's camera's matrix and place.
        if (scene::View3d* view = frameView(s); view != nullptr && world != nullptr) {
            const Mat34& m = view->camera().world();
            world->manager().setListenerData({m[3], m[7], m[11], 0.0f}, m.data());
        }
        readings(host, s);
        rollers();
        // The car's sounds unoccluded (+0x20 with 1), the 3D world's frame (0x005B96C0).
        if (m_car->sound) m_car->sound->setOcclusion(1.0f);
    }
    if (world != nullptr) world->update(host.gameFrameTime());
    if (scene::View3d* view = frameView(s); view != nullptr) {
        view->setZoom(1.0f);
        view->setZoomTarget(1.0f);
    }
    const auto back = [&] {
        host.sound("CLICK");
        data->careerScreen = CareerScreen::CarSelBack;
        host.request("garageload", SwitchMode::Replace);
    };
    if (event == input::kSelect) {
        if (m_state == DynoState::Ready) {
            m_state = DynoState::Starting;
            host.sound("CLICK");
            s.showContaining("DATA_", true);
            m_topSpeed = m_topTorque = m_topBhp = m_topRevs = m_bhpRevs = m_shiftWait = 0.0f;
            m_countdown = 4.0f;
            showMessage(host, s, kTextInitialising, false);
            s.hideContaining("SEL_", false);
        } else if (m_state == DynoState::Results) {
            back();
        }
    } else if (event == input::kBack) {
        back();
    }
    if (m_state != DynoState::Starting) return;
    m_countdown = m_countdown - host.frameTime();
    if (!(m_countdown < 0.0f)) return;
    m_countdown = 0.0f;
    CarBody& body = *m_car->body;
    pressDown(body);
    body.setGear(m_testGear);
    m_state = DynoState::Running;
    host.sound("WARNING");
    m_topSpeed = m_topTorque = m_topBhp = m_topRevs = m_bhpRevs = m_countdown = m_shiftWait = 0.0f;
    showMessage(host, s, kTextAnalyzing, false);
}

// 0x004DF660: in state 4 with the graph, once the window's transitions are done (0x005AA120): the graph at (130, 342)
// drawn (0x004F8940) without and then with its shadows; its drawn points one more (a quarter of the frame's time
// within 1 .. 10, cut) up to its points.
void RollingRoad::draw(GarageHost& host, ui::Screen& s) {
    if (m_state != DynoState::Results || !m_graph || !s.transitionsDone()) return;
    Graph& g = *m_graph;
    g.x = 130.0f;
    g.y = 342.0f;
    g.draw(host.device(), false);
    g.x = 130.0f;
    g.y = 342.0f;
    g.draw(host.device(), true);
    if (g.drawn >= g.points) return;
    const float more = std::clamp(0.25f * host.frameTime(), 1.0f, 10.0f);
    g.drawn = std::min(g.drawn + static_cast<uint32_t>(x87::chop(more)), g.points);
}

}  // namespace oj::frontend
