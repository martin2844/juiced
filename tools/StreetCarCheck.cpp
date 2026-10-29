// Exercise real native car loading and customization without touching user profiles.
#include "Core/Cars/CarDamage.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/GameData.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include "Game/Cars/CarModel.h"
#include "Render/Model.h"
#include <cmath>
#include <functional>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    auto game = std::filesystem::absolute(argv[1]);
    oj::GameFiles::setOverrideRoot(std::filesystem::absolute(argv[2]));
    std::filesystem::current_path(game);
    oj::CrtRandom random;
    oj::GameData data(game);
    std::string error;
    if (!data.load(random, &error)) {
        std::cerr << error;
        return 1;
    }
    int failed = 0;
    auto check = [&](bool b, const char *s) {
        std::cout << (b ? "PASS " : "FAIL ") << s << '\n';
        if (!b)
            ++failed;
    };
    check(data.tables.carModels.size() == 57, "stable roster 0..56");
    check(data.defs.byId(53)->name == "FERRARI_458", "Ferrari ID preserved");
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, data.tables, 3, false);
    for (int id = 54; id <= 56; ++id) {
        auto *def = data.defs.byId(id);
        check(def != nullptr, "definition exists");
        if (!def)
            continue;
        std::cout << "CAR " << def->name << '\n';
        auto *d = data.dynamics->of(*def);
        check(d != nullptr, "physics loaded");
        if (!d)
            continue;
        auto power = oj::peakPower(*d) / 2.8f / 745.7f;
        std::cout << "BHP " << power << '\n';
        check(power > 120 && power < 300, "stock power plausible");
        check(data.tables.modelClasses[id] == (id == 55 ? 1 : 0), "correct drive layout");
        data.career->profile.performance(id)[0] = 2;
        data.career->profile.bodyParts(id)[0] = 1;
        auto archive = data.career->write();
        oj::Career restored;
        restored.profile.modCount = 4;
        check(restored.read(archive, &error) && restored.profile.performance(id)[0] == 2 &&
                  restored.profile.bodyParts(id)[0] == 1,
              "upgrade save round trip");
        for (bool garage : {false, true}) {
            auto car = oj::makeCar(def);
            auto model = oj::scene::CarModel::load(engine, car, data.catalogue, random, garage);
            check(model != nullptr, "native model loads");
            if (!model)
                continue;
            bool allMaterials = true;
            std::function<void(oj::render::Node *)> inspect = [&](auto *n) {
                if (auto *mn = dynamic_cast<oj::render::ModelNode *>(n); mn && mn->mesh)
                    for (auto &p : mn->mesh->parts())
                        allMaterials &= p.material != nullptr;
                for (auto *c : n->children())
                    inspect(c);
            };
            inspect(model->root());
            check(allMaterials, "all stock material references resolve");
            for (int i = 0; i < 4; ++i)
                check(model->wheelNode(i) && model->wheelNode(i)->mesh, "wheel attached");
            auto *f = model->wheelNode(0);
            auto *r = model->wheelNode(2);
            check(f && r &&
                      std::abs(f->local[11] - r->local[11] -
                               (id == 54   ? 262.f
                                : id == 55 ? 270.f
                                           : 243.f)) < .1f,
                  "measured wheelbase");
            oj::DamageGrid grid;
            std::vector<oj::DamagePoint> all;
            for (const char *zone : {"_CHASS", "FBUMP", "RBUMP", "BONNET", "SSKIRT_L", "SSKIRT_R"}) {
                auto points = model->damagePoints(zone);
                check(points && !points->empty(), zone);
                if (points) {
                    grid.addNode(*points, true);
                    all.insert(all.end(), points->begin(), points->end());
                }
            }
            grid.finish(random);
            if (!all.empty()) {
                auto point = all[all.size() / 2];
                auto *part = point.vertex.part;
                auto original = part->bytes;
                grid.hit(point.place, {0, 0, -1, 0}, 8);
                check(part->bytes != original, "damage changes vertices");
                model->undent();
                check(part->bytes == original, "repair restores vertices");
            }
            for (int k = 0; k < 5; ++k)
                car.body[k] = 1;
            model->refitParts(car, data.catalogue);
            for (int k = 0; k < 6; ++k)
                check(model->slotMesh(static_cast<oj::scene::CarModel::Slot>(k)) != nullptr, "sport part fits");
            check(def->parts[8].parts.size() == 31, "31 retail vinyls");
            for (int i = 0; i < 31; ++i) {
                car.body[8] = i;
                model->refitParts(car, data.catalogue);
                check(model->slotMesh(oj::scene::CarModel::kDecal) != nullptr, "vinyl mesh fits");
            }
            car.body[8] = 0x7f;
            model->refitParts(car, data.catalogue);
            check(!model->slotMesh(oj::scene::CarModel::kDecal), "vinyl removal");
            if (!data.catalogue.lists[oj::kWheel].parts.empty()) {
                car.mods[oj::kWheel] = 0;
                model->refitParts(car, data.catalogue);
                for (int i = 0; i < 4; ++i)
                    check(model->wheelNode(i)->mesh != nullptr, "aftermarket wheel fits");
            }

            auto physics = *d;
            oj::loadShapes(physics.body, data.files(), "cars/" + def->name);
            check(physics.body.collision.colliderShape && physics.body.collision.collideeShape, "collision cage loads");
            check(model->hasNode("_BRKLIGHT") && model->hasNode("LIGHTSON"), "switchable lights");
        }
    }
    return failed ? 1 : 0;
}
