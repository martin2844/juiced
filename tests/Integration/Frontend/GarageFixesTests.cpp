// Integration tests: the garage's windows modshop and colourchoose on a career of the game's data, the game's windows
// and texts through a front end of the tests (Framework/Fakes/FakeGarageHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeGarageHost.h"

#include "Core/Base/Random.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/GameData.h"
#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Game/Frontend/Garage/Customise/Paint.h"
#include "Game/Frontend/Garage/ModShop.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

namespace {

using oj::test::check;

// The game's data with its first career, the career's first garage car (made of the first model when it has none)
// the car the garage shows.
std::unique_ptr<oj::GameData> careerWithCar() {
    auto data = std::make_unique<oj::GameData>(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data->load(random, &error) || !data->career || data->defs.cars.empty()) {
        check(false, "game data and a first career loaded: " + error);
        return nullptr;
    }
    oj::Career& career = *data->career;
    if (career.garage.empty()) {
        oj::Car car = oj::makeCar(&data->defs.cars.front());
        car.grid.emplace();
        career.garage.push_back(car);
    }
    career.dealer.mode = oj::DealerMode::Garage;
    career.dealer.chosen = 0;
    return data;
}

}  // namespace

// modshop's upgrades (0x00442A80): every set of the turbo, exhaust and induction levels is rated and the best of each
// class kept. The stock kept the meters and parts of a class's earlier set when a stronger one took it, so the meters
// added up; the fixed build's each upgrade shows its own parts' meters: recomputed from its parts they agree.
OJ_INTEGRATION_TEST(ModShop, UpgradesOwnMeters) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    const std::unique_ptr<oj::GameData> data = careerWithCar();
    if (!data) return;
    oj::Career& career = *data->career;
    oj::Car& car = career.garage.front();
    car.mods.fill(oj::kNoMod);
    const auto model = static_cast<size_t>(std::clamp(car.def->id, 0, 52));
    career.profile.partsA[model].fill(3);  // every level unlocked: many sets for each class
    oj::test::FakeGarageHost host;
    host.data = data.get();
    const std::shared_ptr<oj::ui::Screen> screen = host.open("modshop");
    if (screen == nullptr) return;
    oj::frontend::ModShop shop;
    shop.enter(host, *screen);
    const auto& lists = data->catalogue.lists;
    const auto part = [&](size_t kind, int32_t at) -> const oj::CarPart& {
        return lists[kind].parts[at >= 0 && static_cast<size_t>(at) < lists[kind].parts.size() ? at : 0];
    };
    int agreeing = 0;
    int entries = 0;
    for (const oj::frontend::ModShopEntry& e : shop.entries()) {
        if (e.found == 0) continue;
        ++entries;
        float speed = 0.0f, accel = 0.0f, braking = 0.0f, handling = 0.0f;
        for (size_t kind : {oj::kInduction, oj::kExhaust, oj::kTurbo}) {
            if (e.parts[kind] == -1) continue;
            accel = static_cast<float>(part(kind, e.parts[kind]).physics[0]) * oj::frontend::kByteShare + accel;
            speed = static_cast<float>(part(kind, e.parts[kind]).physics[1]) * oj::frontend::kByteShare + speed;
        }
        if (e.parts[oj::kNitro] != -1) {
            const int level = part(oj::kNitro, e.parts[oj::kNitro]).level;
            if (level == 2) speed = 0.35f;
            if (level == 1) speed = 0.25f;
            if (level == 0) speed = 0.1f;
        }
        if (e.parts[oj::kSuspension] != -1) {
            handling = static_cast<float>(part(oj::kSuspension, e.parts[oj::kSuspension]).physics[3]) *
                           oj::frontend::kByteShare + handling;
        }
        if (e.parts[oj::kBrake] != -1) {
            braking = static_cast<float>(part(oj::kBrake, e.parts[oj::kBrake]).physics[2]) * oj::frontend::kByteShare +
                      braking;
        }
        if (e.parts[oj::kTyre] != -1) {
            braking = static_cast<float>(part(oj::kTyre, e.parts[oj::kTyre]).physics[2]) * oj::frontend::kByteShare +
                      braking;
            handling = static_cast<float>(part(oj::kTyre, e.parts[oj::kTyre]).physics[3]) * oj::frontend::kByteShare +
                       handling;
        }
        accel = std::clamp(accel * 0.3f, 0.0f, 1.0f);
        handling = std::clamp(handling * 0.4f, 0.0f, 1.0f);
        braking = std::clamp(braking * 0.4f, 0.0f, 1.0f);
        speed = std::clamp(speed, 0.0f, 1.0f);
        const bool same = std::fabs(speed - e.speed) < 1e-4f && std::fabs(accel - e.accel) < 1e-4f &&
                          std::fabs(braking - e.braking) < 1e-4f && std::fabs(handling - e.handling) < 1e-4f;
        if (same) ++agreeing;
    }
    std::printf("     modshop: %d upgrades, %d with their parts' meters\n", entries, agreeing);
    check(entries > 0 && agreeing == entries, "modshop (fixed): each upgrade's meters are its own parts'");
}

// colourchoose's SELECT of a metallic or pearl colour (0x0043DD60): colourmenu lets the money equal to the price in,
// and the stock went on only with more (else it took the colour as a base paint, the metallic unpaid). The fixed
// build goes on to colourfocus at the price and refuses under it ("INSUFFICIENT CASH"), nothing charged.
OJ_INTEGRATION_TEST(ColourChoose, MetallicAtThePrice) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    const std::unique_ptr<oj::GameData> data = careerWithCar();
    if (!data) return;
    oj::test::FakeGarageHost host;
    host.data = data.get();
    if (!host.texts.load(std::filesystem::path("scripts") / "english.txt")) {
        check(false, "scripts\\english.txt read");
        return;
    }
    const std::shared_ptr<oj::ui::Screen> screen = host.open("colourchoose");
    if (screen == nullptr) return;
    const auto select = [&](int32_t money) {
        host.requests.clear();
        host.messages.clear();
        host.shared.paintMode = oj::frontend::PaintMode::Metallic;
        host.shared.texts.price = 3000;
        host.shared.paintPending = 0;
        host.shared.paintCharged = {};
        data->career->profile.money = money;
        oj::frontend::ColourChoose choose;
        choose.enter(host, *screen);
        choose.update(host, *screen, oj::input::kSelect);
        const bool focus = std::any_of(host.requests.begin(), host.requests.end(),
                                       [](const auto& r) { return r.first == "colourfocus"; });
        const bool refused =
            !host.messages.empty() && host.messages.back().text == host.texts.at(oj::kTextInsufficientCash);
        return std::string(focus ? "focus" : "") + (refused ? "refused" : "") +
               (host.shared.paintPending != 0 ? " charged" : "");
    };
    const std::string atPrice = select(3000);
    const std::string under = select(2999);
    check(atPrice == "focus", "colourchoose (fixed): a metallic at the price goes on to colourfocus (" + atPrice + ")");
    check(under == "refused", "colourchoose (fixed): under the price refused, nothing charged (" + under + ")");
}
