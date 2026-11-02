// Unit tests: the fixed build's environment of the cars' materials of the world's environment
// (Game/Scene/TrackScene.h, carsEnvironment): the headlights' glass (Glass) reflects the cars' cube with the
// reflections HI, the world's cube below; REFLECTIONS ULTRA's car of the cube and the cars drawn into it.

#include "Framework/Test.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/ExeTables.h"
#include "Game/Scene/TrackScene.h"
#include "Render/Materials.h"
#include "Render/Program.h"
#include "Render/Renderer3d.h"

#include <array>
#include <memory>
#include <vector>

namespace {

using oj::test::check;

// The textures set on each stage.
class StagesDevice : public oj::test::NullRenderDevice {
public:
    std::array<oj::render::TextureId, 8> stages{};
    void setTexture(uint32_t stage, oj::render::TextureId texture) override {
        if (stage < stages.size()) stages[stage] = texture;
    }
};

}  // namespace

// HI (3) with the cubes drawn (0.8 and more): the cars' cube; MED and below, a lower draw quality or no cars' cube:
// the environment given.
OJ_UNIT_TEST(CarsEnvironment, ByReflections) {
    oj::render::GameTexture car, world;
    car.id = 7;
    world.id = 3;
    using oj::scene::carsEnvironment;
    check(carsEnvironment(3, 1.0f, &car, &world) == &car && carsEnvironment(3, 0.8f, &car, &world) == &car,
          "HI: the cars' cube");
    check(carsEnvironment(2, 1.0f, &car, &world) == &world && carsEnvironment(1, 1.0f, &car, &world) == &world &&
              carsEnvironment(0, 1.0f, &car, &world) == &world,
          "MED, LOW, OFF: the world's, as the stock");
    check(carsEnvironment(3, 0.7f, &car, &world) == &world && carsEnvironment(3, 1.0f, nullptr, &world) == &world,
          "HI without the cubes drawn or without the cars' cube: the world's");
}

// The game's Glass (class 9: the headlights' glass) binds the renderer's environment on its stage 1: the cars' cube
// while the cars are drawn with HI, the world's cube with MED.
OJ_UNIT_TEST(CarsEnvironment, HeadlightGlass) {
    StagesDevice device;
    oj::render::ProgramContext programs(device, 0);
    oj::render::Renderer3d r(device, programs);
    const oj::ExeTables& tables = oj::ExeTables::stock();
    const oj::ExeTables::MaterialClass* glassClass = tables.materialClass(oj::kGlass);
    check(glassClass != nullptr, "the Glass class in the tables");
    if (glassClass == nullptr) return;
    std::unique_ptr<oj::render::Material> glass =
        oj::render::createMaterial(*glassClass, r, [](const std::string&) { return nullptr; }, tables);
    check(glass != nullptr, "a Glass material");
    if (glass == nullptr) return;
    oj::render::GameTexture car, world;
    car.id = 7;
    world.id = 3;
    r.environment = oj::scene::carsEnvironment(3, 1.0f, &car, &world);
    glass->bind(0);
    const oj::render::TextureId high = device.stages[1];
    r.environment = oj::scene::carsEnvironment(2, 1.0f, &car, &world);
    glass->bind(0);
    const oj::render::TextureId medium = device.stages[1];
    check(high == car.id && medium == world.id, "HI: the cars' cube on stage 1; MED: the world's");
}

// REFLECTIONS ULTRA: the cube's car the one nearest the focus within 4 m (none farther); the cars drawn into it those
// within 30 m but that one, the nearest first, four at most.
OJ_UNIT_TEST(CarsEnvironment, UltraCars) {
    using oj::Vec4;
    const Vec4 focus{1000.0f, 50.0f, 1000.0f, 0.0f};
    const std::vector<Vec4> places = {
        {1000.0f + 2500.0f, 0.0f, 1000.0f, 0.0f},  // 25 m
        {1000.0f, 0.0f, 1000.0f + 150.0f, 0.0f},   // the focus's car
        {1000.0f + 500.0f, 0.0f, 1000.0f, 0.0f},   // 5 m
        {1000.0f + 3100.0f, 0.0f, 1000.0f, 0.0f},  // 31 m: out of reach
        {1000.0f, 0.0f, 1000.0f - 900.0f, 0.0f},   // 9 m
        {1000.0f - 1200.0f, 0.0f, 1000.0f, 0.0f},  // 12 m
        {1000.0f - 2000.0f, 0.0f, 1000.0f, 0.0f},  // 20 m
    };
    const int own = oj::scene::cubeCar(places, focus);
    check(own == 1, "the cube's car: the nearest to the focus");
    check(oj::scene::cubeCar({{1000.0f + 450.0f, 50.0f, 1000.0f, 0.0f}}, focus) == -1, "none within 4 m: no car");
    const std::vector<size_t> cars = oj::scene::reflectedCars(places, focus, own);
    check(cars == std::vector<size_t>{2, 4, 5, 6}, "the cars in it: within 30 m, not its own, the nearest four");
    check(oj::scene::reflectedCars(places, focus, -1).front() == 1, "without a cube's car every car may go in");
}
