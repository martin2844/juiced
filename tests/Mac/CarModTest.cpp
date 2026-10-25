#include "Core/Base/GameFiles.h"
#include "Audio/Bank.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Career/CareerInit.h"
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
    const auto found = std::find_if(data.defs.cars.begin(), data.defs.cars.end(), [](const auto& d) {return d.name == "FERRARI_458";});
    if (found == data.defs.cars.end()) return 1;
    check(found->displayName.find("FERRARI 458") != std::string::npos, "added Ferrari has its own definition");
    check(!oj::carLocked(data.arcade, found->id), "imported car unlocked in custom race");
    check(data.arcadeGroups.current()==found->id, "custom-race selector starts on imported car");
    check(found->id == 53 && data.tables.carModels.size() == 54, "Ferrari added after all 52 retail cars");
    const auto* falcon = data.defs.byId(46);
    check(falcon && falcon->name == "FALCON" && falcon->displayName.find("FERRARI") == std::string::npos,
          "original Falcon retains its own ID and definition");
    check(found->value == 185000, "Ferrari base price is 185000 credits");
    check(found->engineSound == "f458", "Ferrari uses its own engine bank, not a donor car");
    for (const char* bankName : {"f458", "f458b"}) {
        auto bank = oj::audio::Bank::load(data.files(), std::filesystem::path("audio/cars") / bankName, bankName, nullptr);
        check(bank && bank->count() == 7 && bank->samples().size() == 7,
              "stock and upgraded Ferrari audio banks load all seven samples");
        if (bank) for (const auto& sample : bank->samples()) {
            check(sample.data && sample.size > 1000 && sample.rate() == 48000,
                  "engine sample contains playable 48 kHz PCM audio");
        }
    }
    check(found->parts[8].parts.size() == 3 && found->parts[8].allowNone,
          "decal catalogue offers stripes, side graphics, custom logo and removal");
    check(!found->parts[9].parts.empty(), "native neon purchase is available");
    oj::CarMaking making{data.defs, data.catalogue, random, data.autoModifyCounter};
    bool validAutoDecals=true;
    for (int i=0;i<32;++i) {
        auto candidate=oj::makeCar(&*found);
        oj::autoModify(candidate,false,-1,making);
        validAutoDecals &= candidate.body[8] < found->parts[8].parts.size();
    }
    check(validAutoDecals, "Auto-Mod keeps small additional-car decal catalogues in bounds");
    oj::CareerContext context{data.tables, making, *data.dynamics, data.files(), data.currentCrewmate};
    data.career->profile.respectLevel = 36;
    oj::stockDealer(*data.career, context);
    const auto& offers = data.career->dealer.offers;
    const auto offerEnd = offers.begin() + data.career->dealer.offerCount;
    const auto offer = std::find_if(offers.begin(), offerEnd, [](const auto& item) {return item.model == 53;});
    check(offer != offerEnd, "Ferrari appears in dealership at supercar respect threshold");
    if (offer != offerEnd) {
        data.career->dealer.mode = oj::DealerMode::Offers;
        oj::dealerTake(*data.career, static_cast<int32_t>(offer - offers.begin()), context);
        data.career->dealer.deal = 1;
        oj::dealerFinish(*data.career);
        check(!data.career->garage.empty() && data.career->garage.back().def == &*found,
              "dealer transfers the additional Ferrari into the garage");
    }
    auto& progress = data.career->profile;
    progress.performance(53)[0] = 2;
    progress.bodyParts(53)[0] = 1;
    progress.paintLevel(53) = 1;
    const auto saved = data.career->write();
    oj::Career restored;
    restored.profile.modCount = 1;
    check(restored.read(saved, &error), "extended career save reloads");
    check(!restored.garage.empty() && oj::findCarDef(data.defs, restored.garage.back().modelName()) == &*found,
          "owned Ferrari model identity survives save/load");
    check(restored.profile.performance(53)[0] == 2 && restored.profile.bodyParts(53)[0] == 1 &&
          restored.profile.paintLevel(53) == 1, "added-car upgrade progress survives save/load");
    check(progress.performance(46)[0] != 2, "Ferrari upgrades do not unlock Falcon upgrades");
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
        for (uint8_t decal=0;decal<3;++decal) {
            car.body[8]=decal;
            model->refitParts(car,data.catalogue);
            check(model->slotMesh(oj::scene::CarModel::kDecal) != nullptr, "selected decal has a real fitted mesh");
            const auto points=model->damagePoints("DECAL");
            check(points && points->size()>20, "decal shell participates in native damage deformation");
        }
        const auto sponsorPoints=model->damagePoints("SPONS");
        check(sponsorPoints && sponsorPoints->size()>20, "sponsor logos have a fitted deformable shell");
        car.body[8]=0x7f;
        model->refitParts(car,data.catalogue);
        check(model->slotMesh(oj::scene::CarModel::kDecal) == nullptr, "removing decal clears its mesh");
        auto uprated=*dynamics;
        for (auto kind : {oj::kTurbo,oj::kInduction,oj::kExhaust}) {
            const auto& list=data.catalogue.lists[kind].parts;
            if (!list.empty()) car.mods[kind]=0;
        }
        oj::fitParts(uprated,car,data.catalogue);
        check(oj::peakPower(uprated)>power, "purchased performance parts raise engine power");
        auto stockBody=*dynamics;
        oj::loadShapes(stockBody.body,oj::GameFiles(game),"cars/FERRARI_458");
        check(stockBody.body.collision.colliderShape && stockBody.body.collision.collideeShape, "collision meshes load");
        check(model->hasNode("_BRKLIGHT") && model->hasNode("LIGHTSON"), "brake and headlight switching nodes exist");
    }
    oj::GameFiles::setOverrideRoot({});
    return failures ? 1 : 0;
}
