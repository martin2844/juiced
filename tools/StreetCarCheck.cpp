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
#include <set>
#include <array>
#include <string_view>
int main(int argc, char **argv) {
    if (argc != 3 && (argc != 4 || std::string_view(argv[3]) != "--full-roster"))
        return 2;
    const bool fullRoster = argc == 4;
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
    check(data.tables.carModels.size() == (fullRoster ? 64 : 57),
          fullRoster ? "stable roster 0..63" : "stable roster 0..56");
    if (fullRoster) {
        constexpr std::array names{"INTEGRA_DB8", "BMW_E36", "LEVIN_AE92", "PEUGEOT_405_GLX",
                                   "PEUGEOT_406_COUPE", "GOLF_MK5_R32", "JETTA_MK5_FSI", "OUTBACK_2002",
                                   "VOLVO_240_TURBO", "VOLVO_V70_24"};
        bool identities = true;
        for (size_t i = 0; i < names.size(); ++i) {
            const auto *def = data.defs.byId(static_cast<int>(54 + i));
            identities &= def != nullptr && def->name == names[i];
        }
        check(identities, "all ten import IDs 54..63 preserved");
    }
    check(data.defs.byId(53)->name == "FERRARI_458", "Ferrari ID preserved");
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, data.tables, 3, false);
    for (int id = 54; id <= 56; ++id) {
        auto *def = data.defs.byId(id);
        check(def != nullptr, "definition exists");
        if (!def)
            continue;
        std::cout << "CAR " << def->name << '\n';
        bool threeKits = true;
        for (int part : {oj::kFrontBumper, oj::kRearBumper, oj::kSkirt}) {
            const auto &choices = def->parts[part].parts;
            std::set<std::string> names;
            for (size_t i = 1; i < choices.size(); ++i)
                names.insert(choices[i].model);
            const bool complete = choices.size() >= 4 && names.size() >= 3;
            check(complete, "stock plus three distinct bumper/skirt alternatives");
            threeKits &= complete;
        }
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
        if (threeKits) {
            // Use the actual career archive codec, including both old part
            // indices and appended ones. This only changes this in-memory game.
            for (uint8_t variant : {0, 1, 2, 3}) {
                auto fitted = oj::makeCar(def);
                for (int part : {oj::kFrontBumper, oj::kRearBumper, oj::kSkirt}) {
                    fitted.body[part] = variant;
                    data.career->profile.bodyParts(id)[part] = 3;
                }
                fitted.body[oj::kBonnet] = 1;
                fitted.body[oj::kDecal] = 8;
                data.career->garage.push_back(fitted);
                oj::Career loaded;
                loaded.profile.modCount = 4;
                const bool read = loaded.read(data.career->write(), &error);
                check(read && !loaded.garage.empty() && loaded.garage.back().model == fitted.model &&
                          loaded.garage.back().body == fitted.body,
                      "career archive preserves old/new kit indices with carbon and vinyl");
                check(read && loaded.profile.bodyParts(id)[oj::kFrontBumper] == 3 &&
                          loaded.profile.bodyParts(id)[oj::kRearBumper] == 3 &&
                          loaded.profile.bodyParts(id)[oj::kSkirt] == 3,
                      "career archive preserves appended kit unlock levels");
                data.career->garage.pop_back();
            }
        }
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
            // None may remove a detachable wing, never the boot/hatch shell.
            // Compare the actual body panels across a carbon-hood / no-spoiler
            // refit; a now-detachable factory wing is excluded from this set.
            const auto bodyPoints = [&] {
                std::set<std::array<int32_t, 3>> result;
                for (const char *zone : {"_CHASS", "FBUMP", "RBUMP", "BONNET", "SSKIRT_L", "SSKIRT_R"}) {
                    if (const auto points = model->damagePoints(zone))
                        for (const auto &point : *points)
                            result.insert({int32_t(std::lround(point.place[0] * 100)),
                                           int32_t(std::lround(point.place[1] * 100)),
                                           int32_t(std::lround(point.place[2] * 100))});
                }
                return result;
            };
            car.body.fill(0);
            car.body[oj::kDecal] = 0x7f;
            model->refitParts(car, data.catalogue);
            const auto originalBody = bodyPoints();
            car.body[oj::kBonnet] = 1;
            car.body[oj::kSpoiler] = 0x7f;
            model->refitParts(car, data.catalogue);
            const auto refittedBody = bodyPoints();
            check(!originalBody.empty() &&
                      std::includes(refittedBody.begin(), refittedBody.end(), originalBody.begin(), originalBody.end()),
                  "carbon hood/no spoiler preserves integral boot and hatch geometry");
            for (int k : {0, 1, 2, 4})
                car.body[k] = 1;
            model->refitParts(car, data.catalogue);
            for (int k : {0, 1, 2, 4, 5})
                check(model->slotMesh(static_cast<oj::scene::CarModel::Slot>(k)) != nullptr, "sport part fits");
            if (threeKits) {
                for (uint8_t variant : {2, 3}) {
                    for (int part : {oj::kFrontBumper, oj::kRearBumper, oj::kSkirt})
                        car.body[part] = variant;
                    model->refitParts(car, data.catalogue);
                    for (int slot : {0, 1, 4, 5})
                        check(model->slotMesh(static_cast<oj::scene::CarModel::Slot>(slot)) != nullptr,
                              "additional kit fits front/rear/both skirts");
                    allMaterials = true;
                    inspect(model->root());
                    check(allMaterials, "additional kit material references resolve");
                    const auto fittedBody = bodyPoints();
                    check(std::includes(fittedBody.begin(), fittedBody.end(), originalBody.begin(), originalBody.end()),
                          "additional kit retains original body surface with carbon/no spoiler");
                }
            }
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
