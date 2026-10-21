// Integration tests: the car view of the interface (Game/Cars/CarView.h) in the game's cmscarcollection2 window.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Base/Random.h"
#include "Core/Data/Language.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/CustomRace/CustomRaceInternal.h"
#include "Game/Scene/Scene3d.h"
#include "Game/UI/UiScreen.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The parts changed on the car shown (0x0045A8A0) put its chassis at the car's lowering, as the view's frame does once
// the stock code's new load is over (0x0045A027): the automatic modification's 0.8 x MAXDROPHEIGHT down.
OJ_INTEGRATION_TEST(CarView, RefitLowersTheChassis) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead || shared.defs.cars.size() < 2) {
        check(false, "cardefs.txt read");
        return;
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    const oj::Language language = *oj::Language::byName("english");
    auto screen = std::make_shared<oj::ui::Screen>(device, language);
    screen->setView3dFactory([&](const oj::ui::Element& e) -> std::unique_ptr<oj::ui::View3d> {
        return std::make_unique<oj::scene::View3d>(engine, e, random);
    });
    if (!screen->load("cmscarcollection2")) {
        check(false, "cmscarcollection2.ui read");
        return;
    }
    oj::Car car;
    car.def = &shared.defs.cars[1];
    oj::frontend::customrace::clearParts(car);
    oj::scene::CarView::Setup setup;
    setup.element = "CAR3D";
    oj::scene::CarView view(engine, *screen, setup, shared.catalogue, shared.tables);
    view.show(car, nullptr, 0, 0.0f, 0.5f, false, random);
    view.finish(0.05f, random);
    oj::render::Node* chassis = view.hasModel() ? view.model()->node("_CHASS") : nullptr;
    if (chassis == nullptr) {
        check(false, std::string("the car shown, its chassis (") + (view.hasModel() ? "no _CHASS" : "no model") + ")");
        return;
    }
    const bool levelFirst = chassis->local[7] == 0.0f;
    car.lowering = oj::Car::bitsOf(0.8f);
    view.refit(car, shared.defs, random);
    check(levelFirst && chassis->local[7] == -(car.def->maxDropHeight * 0.8f),
          "the parts changed: the chassis down by MAXDROPHEIGHT x 0.8");
}
