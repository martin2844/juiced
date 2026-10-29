// Exercise real native car loading and customization without touching user profiles.
#include "Core/Cars/CarDamage.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/GameData.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include "Game/Cars/CarModel.h"
#include "Render/Model.h"
#include <cmath>
#include <algorithm>
#include <array>
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
    int checks = 0;
    auto check = [&](bool b, const char *s) {
        ++checks;
        std::cout << (b ? "PASS " : "FAIL ") << s << '\n';
        if (!b)
            ++failed;
    };
    check(data.tables.carModels.size() == 64, "stable roster 0..63");
    const auto* ferrari = data.defs.byId(53);
    check(ferrari && ferrari->name == "FERRARI_458", "Ferrari ID preserved");
    for (int id = 57; id <= 63; ++id) {
        const auto& all = data.arcadeGroups.ids.back();
        check(std::find(all.begin(), all.end(), id) != all.end(), "added car selectable in final arcade group");
    }
    {
        oj::Career old;
        old.profile.modCount = 4;
        old.profile.performance(56)[3] = 2;
        const auto bytes = old.write();
        oj::Career expanded;
        expanded.profile.modCount = 11;
        check(expanded.read(bytes, &error) && expanded.profile.performance(56)[3] == 2 &&
                  expanded.profile.performance(63)[3] == 0 && expanded.profile.modCount == 11,
              "older roster save loads with appended cars");
        expanded.profile.performance(63)[3] = 3;
        oj::Career reduced;
        reduced.profile.modCount = 4;
        check(reduced.read(expanded.write(), &error) && reduced.profile.modCount == 11 &&
                  reduced.profile.performance(63)[3] == 3,
              "temporarily disabled cars keep upgrade progress");
        auto truncated = bytes;
        truncated.pop_back();
        oj::Career invalid;
        invalid.profile.modCount = 11;
        check(!invalid.read(truncated, &error), "truncated progress is rejected");
        auto oversized = bytes;
        oversized[oversized.size() - 4 * sizeof(oj::CareerProfile::ModProgress) - 4] = 255;
        check(!invalid.read(oversized, &error), "oversized progress count is rejected");
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, data.tables, 3, false);
    for (int id = 57; id <= 63; ++id) {
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
        const auto rated = std::array<float, 7>{101.918f, 191.343f, 246.748f, 147.513f, 165.f, 152.877f, 168.f}[id - 57];
        check(std::abs(power - rated) < rated * .01f, "stock power within one percent of specification");
        check(data.tables.modelClasses[id] == (id == 59 || id == 61 ? 2 : id == 62 ? 1 : 0), "correct drive layout");
        for (const auto& wheel : d->wheels)
            check(bool(wheel.drive) == (id == 59 || id == 61 || (id == 62 ? wheel.id >= 2 : wheel.id < 2)),
                  "actual driven wheel matches drivetrain");
        data.career->profile.performance(id)[0] = 2;
        data.career->profile.bodyParts(id)[0] = 1;
        auto archive = data.career->write();
        oj::Career restored;
        restored.profile.modCount = 11;
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
            const auto geometrySize = [&]() {
                size_t indices = 0;
                std::function<void(oj::render::Node *)> visit = [&](auto *n) {
                    if (auto *mn = dynamic_cast<oj::render::ModelNode *>(n); mn && mn->mesh)
                        for (const auto &part : mn->mesh->parts())
                            indices += part.indexCount;
                    for (auto *child : n->children())
                        visit(child);
                };
                visit(model->root());
                return indices;
            };
            const auto stockGeometrySize = geometrySize();
            check(def->parts[3].parts.empty() && !model->slotMesh(oj::scene::CarModel::kSpoiler),
                  "rear body panels are not detachable spoilers");
            car.body[3] = 0x7f;
            model->refitParts(car, data.catalogue);
            check(geometrySize() == stockGeometrySize, "no-spoiler selection preserves stock body geometry");
            for (int i = 0; i < 4; ++i)
                check(model->wheelNode(i) && model->wheelNode(i)->mesh, "wheel attached");
            auto *f = model->wheelNode(0);
            auto *r = model->wheelNode(2);
            check(f && r &&
                      std::abs(f->local[11] - r->local[11] -
                               std::array<float, 7>{266.9f,270.f,257.8f,257.8f,265.f,264.f,275.5f}[id-57]) < .1f,
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
            for (int k : {0, 1, 2, 4})
                car.body[k] = 1;
            model->refitParts(car, data.catalogue);
            for (int k : {0, 1, 2, 4, 5})
                check(model->slotMesh(static_cast<oj::scene::CarModel::Slot>(k)) != nullptr, "sport part fits");
            for (int level = 1; level <= 3; ++level) {
                for (int part : {0, 1, 4}) {
                    check(def->parts[part].parts.size() == 4, "stock plus three body kit choices");
                    car.body[part] = level;
                }
                model->refitParts(car, data.catalogue);
                for (int slot : {0, 1, 4, 5})
                    check(model->slotMesh(static_cast<oj::scene::CarModel::Slot>(slot)) != nullptr,
                          "Blender kit fits native slot");
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
    std::cout << "CHECKS " << checks << " PASSED " << checks - failed << " FAILED " << failed << std::endl;
    return failed ? 1 : 0;
}
