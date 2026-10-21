// Integration tests: SceneObjects.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Matrix.h"
#include "Core/Base/Random.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Formats/Cld.h"
#include "Core/Formats/Dlh.h"
#include "Core/Formats/Model.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Physics/PhysicsWorld.h"
#include "Core/Physics/RigidBody.h"
#include "Core/Tracks/SceneObjects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <vector>

namespace {

using oj::test::check;
using oj::test::bytesOf;

}  // namespace

// Every scene objects' file of the tracks reads to its end.
OJ_INTEGRATION_TEST(SceneObjects, EveryDlh) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    size_t dlhFiles = 0, read = 0, objects = 0, nodes = 0, hulls = 0, meshesDrawn = 0, meshesHidden = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(game.root() / "tracks", error)) {
        if (!entry.is_regular_file() || _wcsicmp(entry.path().extension().c_str(), L".dlh") != 0) continue;
        ++dlhFiles;
        const auto file = game.read(entry.path().lexically_relative(game.root()));
        if (!file) continue;
        const std::vector<uint8_t> raw(reinterpret_cast<const uint8_t*>(file->data()),
                                       reinterpret_cast<const uint8_t*>(file->data()) + file->size());
        const auto dlh = oj::parseDlh(raw);
        if (!dlh) {
            std::printf("     not read: %s\n", entry.path().filename().string().c_str());
            continue;
        }
        ++read;
        objects += dlh->objects.size();
        for (const oj::DlhObject& o : dlh->objects) hulls += (o.hull ? 1 : 0) + (o.hull2 ? 1 : 0);
        // The mesh nodes under a mesh root and under another (a spline's riders, nodes under a dummy).
        std::function<void(const oj::DlhNode&, bool)> count = [&](const oj::DlhNode& n, bool drawn) {
            ++nodes;
            if (n.object < dlh->objects.size() && dlh->objects[n.object].type == oj::DlhObject::Mesh) {
                ++(drawn ? meshesDrawn : meshesHidden);
            }
            for (const oj::DlhNode& c : n.children) count(c, drawn);
        };
        for (const oj::DlhNode& n : dlh->roots) {
            const bool meshRoot =
                n.object < dlh->objects.size() && dlh->objects[n.object].type == oj::DlhObject::Mesh;
            count(n, meshRoot);
        }
    }
    std::printf("     scene objects: %zu of %zu files, %zu objects, %zu nodes (%zu mesh nodes under a mesh root, "
                "%zu under another), %zu hulls\n",
                read, dlhFiles, objects, nodes, meshesDrawn, meshesHidden, hulls);
    check(dlhFiles > 0 && read == dlhFiles, "scene objects: every .dlh of the tracks read to its end");
}
// The riders of Campbell Hills' splines (balloons, planes, the cable cars): they start on their curves and
// after 50 frames without time move along their paths, keeping near them.
OJ_INTEGRATION_TEST(SceneObjects, SplineRiders) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const std::filesystem::path base = std::filesystem::path("tracks") / "Campbell Hills";
    const auto dlhBytes = game.read(base / "Campbell Hills_ch_dynamic_day.dlh");
    bool rode = false;
    if (dlhBytes) {
        const std::vector<uint8_t> raw(reinterpret_cast<const uint8_t*>(dlhBytes->data()),
                                       reinterpret_cast<const uint8_t*>(dlhBytes->data()) + dlhBytes->size());
        const auto dlh = oj::parseDlh(raw);
        oj::CrtRandom loadRandom;
        const oj::EffectLibrary noEffects;
        oj::SceneObjects sceneObjects;
        oj::SceneObjects::Load load;
        load.random = &loadRandom;
        load.effects = &noEffects;
        if (dlh) {
            sceneObjects.loadDlh(*dlh, load);
            auto worldOf = [&](const oj::SceneSpline& s, const oj::SceneRider& r) {
                const std::array<float, 16>& w = sceneObjects.nodes()[s.node].matrix;
                const oj::Mat34& m = sceneObjects.nodes()[r.node].local;
                return oj::Vec4{((w[0] * m[3] + w[1] * m[7]) + w[2] * m[11]) + w[3],
                                ((w[4] * m[3] + w[5] * m[7]) + w[6] * m[11]) + w[7],
                                ((w[8] * m[3] + w[9] * m[7]) + w[10] * m[11]) + w[11], 0.0f};
            };
            std::vector<oj::Vec4> start;
            size_t riders = 0;
            for (const oj::SceneSpline& s : sceneObjects.splines()) {
                for (const oj::SceneRider& r : s.riders) {
                    start.push_back(worldOf(s, r));
                    ++riders;
                }
            }
            for (int i = 0; i < 50 + 60 * 20; ++i) sceneObjects.update(1.0f / 60.0f, i % 2 == 0, {});
            size_t moved = 0;
            float farthest = 0.0f;
            size_t k = 0;
            for (const oj::SceneSpline& s : sceneObjects.splines()) {
                for (const oj::SceneRider& r : s.riders) {
                    const oj::Vec4 now = worldOf(s, r);
                    const float dx = now[0] - start[k][0], dz = now[2] - start[k][2];
                    if (dx * dx + dz * dz > 100.0f * 100.0f || !r.moving) ++moved;
                    // Away from the path: to the nearest of its points, in lengths of the segment from it.
                    float nearest = 1e30f;
                    for (const oj::SplinePathPoint& p : s.path) {
                        const float px = now[0] - p.position[0], py = now[1] - p.position[1],
                                    pz = now[2] - p.position[2];
                        nearest = std::min(nearest, std::sqrt(px * px + py * py + pz * pz) / std::max(p.length, 1.0f));
                    }
                    farthest = std::max(farthest, nearest);
                    ++k;
                }
            }
            std::printf("     riders of Campbell Hills (day): %zu on %zu splines, %zu moved or ended in 20 s, "
                        "%.2f segments from a path point at most\n",
                        riders, sceneObjects.splines().size(), moved, farthest);
            rode = riders > 0 && moved == riders && farthest < 1.5f;
        }
    }
    check(rode, "scene objects: the splines' riders move along their paths");
}
// The physics objects of Southside Beach R1 (bins and a barrel with PHYSICS, each object at every node
// placing it): rigid bodies of their profiles, the centre of gravity the middle of the first one's
// collider; in the world (which steps only with a primary body in it) on the route's collision they stand,
// then go to sleep (off the active list) within a few seconds.
OJ_INTEGRATION_TEST(SceneObjects, PhysicsObjects) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const std::filesystem::path base = std::filesystem::path("tracks") / "SOUTHSIDE BEACH";
    const auto dlhBytes = game.read(base / "SOUTHSIDE BEACH_SSB_DYNAMIC_r1.dlh");
    const auto cldBytes = game.read(base / "southside beach_southside_r1.cld");
    bool physicsMade = false, rested = false;
    if (dlhBytes && cldBytes) {
        auto bytesOf = [](const std::vector<std::byte>& b) {
            return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(b.data()),
                                        reinterpret_cast<const uint8_t*>(b.data()) + b.size());
        };
        const auto dlh = oj::parseDlh(bytesOf(*dlhBytes));
        const auto cld = oj::parseCld(bytesOf(*cldBytes));
        oj::CrtRandom loadRandom;
        const oj::EffectLibrary noEffects;
        oj::SceneObjects sceneObjects;
        oj::SceneObjects::Load load;
        load.random = &loadRandom;
        load.effects = &noEffects;
        load.files = &game;
        if (dlh && cld) {
            sceneObjects.loadDlh(*dlh, load);
            const auto& physics = sceneObjects.physics();
            physicsMade = physics.size() == 34;
            for (const oj::ScenePhysics& p : physics) {
                physicsMade = physicsMade && p.kind == oj::ObjectKind::Dynamic && p.body != nullptr &&
                              p.def->collision.colliderShape && p.def->sleepSet && p.def->collision.maxCollidees == 3;
            }
            const oj::ScenePhysics* bin = nullptr;
            for (const oj::ScenePhysics& p : physics) {
                if (_stricmp(p.profile.c_str(), "dynamic_bin01") == 0) bin = &p;
            }
            if (bin != nullptr) {
                std::printf("     %s: centre of gravity (%.4f, %.4f, %.4f) m, mass %.0f kg, collidee mu %.1f\n",
                            bin->profile.c_str(), bin->def->centreOfGravity[0], bin->def->centreOfGravity[1],
                            bin->def->centreOfGravity[2], bin->def->mass,
                            bin->def->collision.meshCollideeMu);
            }
            oj::PhysicsWorld world(*cld);
            for (const oj::ScenePhysics& p : physics) world.add(*p.body);
            oj::BodyDef primaryDef;
            primaryDef.collision.primary = true;
            primaryDef.mass = 1.0f;
            primaryDef.inertia = {1.0f, 1.0f, 1.0f, 0.0f};
            oj::RigidBody primary(primaryDef, {0.0f, 10000.0f, 0.0f, 0.0f});
            world.add(primary);
            // Those placed off the route's collision (behind its walls) find no ground and fall on.
            std::vector<float> startY;
            std::vector<bool> grounded;
            for (const oj::ScenePhysics& p : physics) {
                startY.push_back(p.body->position[1]);
                float ground = 0.0f;
                grounded.push_back(world.collision().groundBelow(p.body->node[3], p.body->node[11], ground));
            }
            size_t awakeAt2s = 0;
            for (int i = 0; i < 60 * 8; ++i) {
                world.update(1.0f / 60.0f);
                if (i == 120) awakeAt2s = world.active().size();
            }
            float moved = 0.0f;
            size_t standing = 0, falling = 0;
            rested = awakeAt2s == physics.size();
            for (size_t k = 0; k < physics.size(); ++k) {
                const oj::RigidBody& b = *physics[k].body;
                if (grounded[k]) {
                    ++standing;
                    moved = std::max(moved, std::fabs(b.position[1] - startY[k]));
                    rested = rested && !b.active;
                } else {
                    ++falling;
                    rested = rested && b.active && b.position[1] < startY[k] - 50.0f;
                }
            }
            std::printf("     physics objects of Southside Beach R1: %zu made, %zu awake after 2 s, %zu after 8 s; "
                        "%zu on the collision moved %.3f m up or down at most, %zu off it fell\n",
                        physics.size(), awakeAt2s, world.active().size(), standing, moved, falling);
            rested = rested && standing > 0 && moved < 0.05f;
        }
    }
    check(physicsMade, "physics objects: Southside Beach R1's bins and barrel from their profiles and hulls");
    check(rested, "physics objects: on the route's collision they stand and go to sleep, off it they fall");
}
