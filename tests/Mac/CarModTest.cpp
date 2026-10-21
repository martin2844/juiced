#include "Core/Base/GameFiles.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/GameData.h"
#include "Game/Cars/CarModel.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <cstring>

int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: mac_car_mod_check GAME MOD\n"; return 2; }
    const auto game = std::filesystem::absolute(argv[1]);
    const auto mod = std::filesystem::absolute(argv[2]);
    std::filesystem::current_path(game);
    oj::GameFiles::setOverrideRoot(mod);
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::cout << (ok ? "PASS " : "FAIL ") << what << '\n';
        if (!ok) ++failures;
    };
    oj::CrtRandom random;
    oj::GameData data(game);
    std::string error;
    if (!data.load(random, &error)) { std::cerr << error; return 1; }
    const auto found = std::find_if(data.defs.cars.begin(), data.defs.cars.end(), [](const auto& d) {return d.name == "FALCON";});
    if (found == data.defs.cars.end()) return 1;
    check(found->displayName.find("FERRARI 458") != std::string::npos, "mod car definition overrides archived Falcon");
    check(!oj::carLocked(data.arcade, found->id), "imported car unlocked in custom race");
    check(data.arcadeGroups.current()==found->id, "custom-race selector starts on imported car");
    const auto* dynamics = data.dynamics->of(*found);
    if (!dynamics) return 1;
    check(dynamics->forwardRatios.size() == 7, "seven forward gears");
    const float power = oj::peakPower(*dynamics);
    check(power / 2.8f > 400000 && power / 2.8f < 440000, "stock peak power is within 400–440 kW");
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, data.tables, 3, false);
    for (bool garage : {false, true}) {
        oj::Car car; car.def = &*found; car.setModel(found->model);
        auto model = oj::scene::CarModel::load(engine, car, data.catalogue, random, garage);
        check(model != nullptr, garage ? "garage model loads" : "race model loads");
        if (!model) continue;
        for (size_t i=0;i<4;++i) check(model->wheelNode(i) && model->wheelNode(i)->mesh, "wheel fitted");
        auto* front = model->wheelNode(0); auto* rear = model->wheelNode(2);
        check(front && rear && std::abs(front->local[11]-rear->local[11]-265.f)<1.f, "265 cm wheelbase");
        oj::DamageGrid grid;
        std::vector<oj::DamagePoint> all;
        for (const char* zone : {"_CHASS", "FBUMP", "RBUMP", "BONNET", "SSKIRT_L", "SSKIRT_R"}) {
            auto points = model->damagePoints(zone);
            check(points && points->size()>20, "damage panel contains deformable vertices");
            if (!points || points->empty()) continue;
            grid.addNode(*points,true);
            all.insert(all.end(),points->begin(),points->end());
        }
        grid.finish(random);
        if (!all.empty()) {
            const auto point = all[all.size()/2];
            auto* part=point.vertex.part;
            const auto original=part->bytes;
            grid.hit(point.place, {0,0,-1,0}, 8.f);
            check(part->changed && part->bytes != original, "impact actually deforms imported vertex buffers");
            check(std::any_of(grid.zones().begin(),grid.zones().end(),[](const auto& z){return z->damage>0;}), "impact records panel damage");
            model->undent();
            check(part->bytes == original, "repair restores original geometry");
        }
        const oj::render::Mesh* stock[6];
        for (size_t k=0;k<6;++k) stock[k]=model->slotMesh(static_cast<oj::scene::CarModel::Slot>(k));
        for (size_t k=0;k<5;++k) car.body[k]=1;
        model->refitParts(car,data.catalogue);
        for (size_t k=0;k<6;++k) {
            auto* fitted=model->slotMesh(static_cast<oj::scene::CarModel::Slot>(k));
            check(fitted && stock[k] && fitted != stock[k], "sport body part replaces stock mesh");
        }
        auto uprated=*dynamics;
        for (auto kind : {oj::kTurbo,oj::kInduction,oj::kExhaust}) {
            const auto& list=data.catalogue.lists[kind].parts;
            if (!list.empty()) car.mods[kind]=0;
        }
        oj::fitParts(uprated,car,data.catalogue);
        check(oj::peakPower(uprated)>power, "purchased performance parts raise engine power");
        auto stockBody=*dynamics;
        oj::loadShapes(stockBody.body,oj::GameFiles(game),"cars/FALCON");
        check(stockBody.body.collision.colliderShape && stockBody.body.collision.collideeShape, "collision meshes load");
        check(model->hasNode("_BRKLIGHT") && model->hasNode("LIGHTSON"), "brake and headlight switching nodes exist");
    }
    oj::GameFiles::setOverrideRoot({});
    return failures ? 1 : 0;
}
