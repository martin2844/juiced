// Integration tests: a car model's parts (Game/Cars/CarModel.cpp) with the game's cars.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Base/Random.h"
#include "Core/Formats/Model.h"
#include "Game/Cars/CarModel.h"
#include "Game/Frontend/CustomRace/CustomRaceInternal.h"
#include "Game/Scene/Scene3d.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// The parts of a mesh file of the game whose material has that name (case aside).
std::vector<size_t> partsNamed(const std::string& file, const char* material) {
    std::vector<size_t> out;
    const auto bytes = oj::test::gameFiles().read(file);
    if (!bytes) return out;
    std::vector<uint8_t> data(bytes->size());
    std::memcpy(data.data(), bytes->data(), data.size());
    const auto mesh = oj::parseMesh(data);
    if (!mesh) return out;
    for (size_t i = 0; i < mesh->parts.size(); ++i) {
        if (_stricmp(mesh->parts[i].name.c_str(), material) == 0) out.push_back(i);
    }
    return out;
}

// Whether every part listed of a mesh has that material.
bool partsOf(const oj::render::Mesh* mesh, const std::vector<size_t>& parts, const oj::render::Material* material) {
    if (mesh == nullptr || parts.empty()) return false;
    for (size_t i : parts) {
        if (i >= mesh->parts().size() || mesh->parts()[i].material != material) return false;
    }
    return true;
}

}  // namespace

// The custom race's car with the automatic modification's decal, then its parts cleared (0x004600C0: the decal -1) and
// the model's parts put again (0x0045A8A0): the decal's slot is emptied (0x0048BC30), as the model makes none of a
// decal not chosen.
OJ_INTEGRATION_TEST(CarModelParts, DecalTakenOff) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) {
        check(false, "cardefs.txt read");
        return;
    }
    const oj::CarDef* def = nullptr;
    for (const oj::CarDef& d : shared.defs.cars) {
        if (!d.parts[8].parts.empty()) {
            def = &d;
            break;
        }
    }
    if (def == nullptr) {
        check(false, "a car with decals");
        return;
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    oj::Car car;
    car.def = def;
    oj::frontend::customrace::clearParts(car);
    car.body[8] = 0;
    const std::unique_ptr<oj::scene::CarModel> model =
        oj::scene::CarModel::load(engine, car, shared.catalogue, random, true);
    if (!model) {
        check(false, "the car's model loaded");
        return;
    }
    const bool decalOn = model->slotMesh(oj::scene::CarModel::kDecal) != nullptr;
    oj::frontend::customrace::clearParts(car);
    model->refitParts(car, shared.catalogue);
    const bool decalOff = model->slotMesh(oj::scene::CarModel::kDecal) == nullptr;
    check(decalOn && decalOff && car.body[8] == 0xFF,
          "the decal fitted, then gone with the parts cleared (the record's -1 kept)");
}

// The primer (<car>_PRIMER) takes the body material in the meshes of the model's slots (0x0048C210 after the body
// parts, 0x0048C2E0): the 3000GT's standard front bumper is painted. Stock problem: the S2000's chassis, in no slot,
// keeps its own primer part (grey); the fixed build paints every mesh's.
OJ_INTEGRATION_TEST(CarModelParts, PrimerOnEveryMesh) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) {
        check(false, "cardefs.txt read");
        return;
    }
    const oj::CarDef* gt = nullptr;
    const oj::CarDef* s2000 = nullptr;
    for (const oj::CarDef& d : shared.defs.cars) {
        if (_stricmp(d.name.c_str(), "3000gt") == 0) gt = &d;
        if (_stricmp(d.name.c_str(), "s2000") == 0) s2000 = &d;
    }
    if (gt == nullptr || s2000 == nullptr) {
        check(false, "the 3000GT and the S2000 in cardefs.txt");
        return;
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    oj::Car gtCar;
    gtCar.def = gt;
    oj::Car s2000Car;
    s2000Car.def = s2000;
    const std::unique_ptr<oj::scene::CarModel> gtModel =
        oj::scene::CarModel::load(engine, gtCar, shared.catalogue, random, false);
    const std::unique_ptr<oj::scene::CarModel> s2000Model =
        oj::scene::CarModel::load(engine, s2000Car, shared.catalogue, random, true);
    if (!gtModel || !s2000Model) {
        check(false, "the cars' models loaded");
        return;
    }
    const oj::render::Mesh* bumper = gtModel->slotMesh(oj::scene::CarModel::kFrontBumper);
    const std::vector<size_t> gtBody = partsNamed("cars/3000gt/mesh/3000gt_fbump.d8m", "3000gt_body");
    const std::vector<size_t> gtPrimer = partsNamed("cars/3000gt/mesh/3000gt_fbump.d8m", "3000gt_primer");
    const oj::render::Material* gtPaint = bumper != nullptr && !gtBody.empty() && gtBody[0] < bumper->parts().size()
                                              ? bumper->parts()[gtBody[0]].material
                                              : nullptr;
    check(gtPaint != nullptr && partsOf(bumper, gtBody, gtPaint) && partsOf(bumper, gtPrimer, gtPaint),
          "the 3000GT's bumper in its slot: its primer parts of the body material");
    const oj::render::ModelNode* chassis = s2000Model->meshNode("S2000_CHASS");
    const oj::render::Mesh* s2000Mesh = chassis != nullptr ? chassis->mesh : nullptr;
    const std::vector<size_t> s2000Body = partsNamed("cars/s2000_ui/mesh/s2000_chass.d8m", "s2000_body");
    const std::vector<size_t> s2000Primer = partsNamed("cars/s2000_ui/mesh/s2000_chass.d8m", "s2000_primer");
    const bool found = s2000Mesh != nullptr && !s2000Body.empty() && !s2000Primer.empty() &&
                       s2000Body[0] < s2000Mesh->parts().size() && s2000Primer[0] < s2000Mesh->parts().size();
    const oj::render::Material* s2000Paint = found ? s2000Mesh->parts()[s2000Body[0]].material : nullptr;
    const oj::render::Material* s2000Primed = found ? s2000Mesh->parts()[s2000Primer[0]].material : nullptr;
    check(s2000Paint != nullptr && s2000Primed == s2000Paint && partsOf(s2000Mesh, s2000Primer, s2000Paint),
          "the S2000's chassis, in no slot: its primer parts of the body material too");
}

// The wheel colour reaches a catalogue wheel's materials alone (0x0048ABC0: the standard wheels have none), each kept
// with its file's colour (+0x134 ..) until the car's colour is kept for them (0x0048C5D0).
OJ_INTEGRATION_TEST(CarModelParts, WheelColours) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead || shared.defs.cars.empty() || shared.catalogue.lists[oj::kWheel].parts.empty()) {
        check(false, "cardefs.txt and wheels.txt read");
        return;
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    oj::Car car;
    for (const oj::CarDef& d : shared.defs.cars) {
        if (_stricmp(d.name.c_str(), "s2000") == 0) car.def = &d;
    }
    if (car.def == nullptr) {
        check(false, "the S2000 in cardefs.txt");
        return;
    }
    car.wheelColour = 0xFF112233;
    car.wheelsColoured = 1;
    const std::unique_ptr<oj::scene::CarModel> standard =
        oj::scene::CarModel::load(engine, car, shared.catalogue, random, true);
    car.mods[oj::kWheel] = 0;
    const std::unique_ptr<oj::scene::CarModel> catalogue =
        oj::scene::CarModel::load(engine, car, shared.catalogue, random, true);
    if (!standard || !catalogue) {
        check(false, "the car's models loaded");
        return;
    }
    bool none = true;
    for (size_t i = 0; i < 5; ++i) none = none && standard->wheelMaterial(i) == nullptr;
    check(none, "the standard wheels: no wheel materials");
    // The catalogue wheel's own material and its file's Colour line.
    const std::string& wheel = shared.catalogue.lists[oj::kWheel].parts[0].model;
    uint32_t fileColour = 0;
    for (const char* mats : {"carmesh/mats/", "mesh/mats/"}) {
        const auto bytes = oj::test::gameFiles().read(std::string(mats) + wheel + ".pmd");
        if (!bytes) continue;
        const oj::Material pmd =
            oj::parseMaterial(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
        if (const std::string* colour = pmd.find("Colour"); colour != nullptr) {
            unsigned r = 0, g = 0, b = 0;
            if (sscanf_s(colour->c_str(), "%u %u %u", &r, &g, &b) == 3) {
                fileColour = 0xFF000000u | r << 16 | g << 8 | b;
            }
        }
        break;
    }
    check(catalogue->wheelMaterial(0) != nullptr && fileColour != 0 && catalogue->keptWheelColour(0) == fileColour,
          "a catalogue wheel: its material found with its file's colour kept (" + wheel + ")");
    catalogue->keepWheelColours(car);
    check(catalogue->keptWheelColour(0) == 0xFF112233, "the car's wheel colour kept for it");
}
