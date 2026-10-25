// Integration tests: the records of the race's cars (Game/Race/TrackView, TrackView::raceRecord) with the game's cars.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Cars/CarCollection.h"
#include "Core/Network/NetSession.h"
#include "Core/Physics/CarBody.h"
#include "Core/Race/Racer.h"
#include "Game/Race/TrackView/TrackView.h"

#include <bit>
#include <cmath>
#include <memory>

namespace {

using oj::test::check;

}  // namespace

// A network game's car is a new record of the config the session passed (0x0048DB40, 0x0048E7D0), never rated: the
// parts fitted with the record itself (0x00485E98) give it its brake value (+0x80), so a second on the brake leaves
// every wheel the BrakeMu of its brakes' part over the wear's 1.01 (0x0048F080). With the parts fitted into a copy the
// record's 0 went on the wheels at the first press: no brakes on the network but the handbrake and the engine.
OJ_INTEGRATION_TEST(RaceCarRecord, NetworkCarBrakes) {
    const oj::test::SharedGameData& g = oj::test::sharedGameData();
    if (!g.carDefsRead) return;
    const oj::CarDef* def = g.defs.byModel("ECLIPSE_STOCK");
    const oj::CarDynamics* dynamics = def != nullptr ? g.dynamics->of(*def) : nullptr;
    const auto& brakes = g.catalogue.lists[oj::kBrake].parts;
    if (dynamics == nullptr || brakes.empty()) {
        check(false, "ECLIPSE_STOCK's dynamics and the brakes' parts");
        return;
    }
    oj::Car sent = oj::makeCar(def);
    sent.mods[oj::kBrake] = static_cast<uint8_t>(brakes.size() - 1);
    uint8_t passed[0x74];
    oj::net::CarConfig::of(sent, def->name).write(passed);
    oj::Car record = oj::makeCar(oj::findCarDef(g.defs, def->name));
    oj::net::CarConfig::read(passed).apply(record);
    check(record.brake == 0, "the config's record starts with no brake value");

    auto view = std::make_unique<app::TrackView>();
    int modifications = 0;
    app::TrackView::RaceRecord made = view->raceRecord(*dynamics, record, &record, g.defs, g.catalogue, modifications, true);
    const float brakeMu = made.dynamics.wheels.back().brakeMu;
    check(brakeMu > dynamics->wheels.back().brakeMu, "the brakes' part raises the wheels' BrakeMu");
    check(std::bit_cast<float>(record.brake) == brakeMu, "the record keeps the BrakeMu its parts give");
    check(made.car.brake == record.brake, "the car as raced keeps it too");

    oj::CarBody body(made.dynamics, {0.0f, 0.0f, 0.0f, 0.0f});
    oj::RaceCar car;
    car.body = &body;
    car.car = &record;
    body.brake = 1.0f;
    for (int i = 0; i < 60; ++i) oj::carFrame(car, 1.0f / 60.0f, 1.0f / 60.0f, oj::RaceType::Circuit);
    bool kept = true;
    for (const oj::WheelDef& w : body.dynamics().wheels) {
        if (std::fabs(w.brakeMu - brakeMu / 1.01f) > brakeMu * 1.0e-4f) kept = false;
    }
    check(kept, "a second on the brake leaves every wheel its BrakeMu over 1.01");
}
