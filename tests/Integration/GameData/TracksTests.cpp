// Integration tests: Tracks.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Matrix.h"
#include "Core/Data/ExeTables.h"
#include "Core/Formats/Cld.h"
#include "Core/Physics/TrackCollision.h"
#include "Core/Tracks/Route.h"
#include "Core/Tracks/Track.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// Every track: its routes, the geometry layers each lists (instances: a mesh's nodes, or one), the
// route's materials (every material id of its layers mapped), its .d8w (a light tile per instance;
// textures inside the .d8t), its .spl (a grid, three lines of one length); the four .shd
// directories cover the _shadowMaps.txb.
OJ_INTEGRATION_TEST(Tracks, EveryTrack) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    auto readFile = [&](const std::filesystem::path& p) -> std::optional<std::vector<uint8_t>> {
        const auto b = game.read(p);
        if (!b) return std::nullopt;
        return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(b->data()),
                                    reinterpret_cast<const uint8_t*>(b->data()) + b->size());
    };
    auto lower = [](std::string s) {
        for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    };
    size_t tracks = 0, routes = 0, badRoutes = 0, layers = 0, instances = 0, badTxb = 0, cldRead = 0;
    size_t corners = 0;
    std::error_code ec;
    for (const auto& dir : std::filesystem::directory_iterator(game.root() / "tracks", ec)) {
        if (!dir.is_directory()) continue;
        const std::string trackFolder = dir.path().filename().string();
        std::filesystem::path tdfPath;
        for (const auto& f : std::filesystem::directory_iterator(dir.path(), ec)) {
            if (_stricmp(f.path().extension().string().c_str(), ".tdf") == 0) tdfPath = f.path();
        }
        const auto tdfBytes = tdfPath.empty() ? std::nullopt : readFile(tdfPath);
        const auto tdf = tdfBytes ? oj::parseTdf(*tdfBytes) : std::nullopt;
        if (!tdf) continue;
        ++tracks;
        const std::filesystem::path base = std::filesystem::path("tracks") / trackFolder;
        const std::string prefix = tdf->track + "_";
        // Each directory counts from the start of the .txb; their tables lie between one another.
        uint64_t tileBytes = 0;
        uint32_t txbEnd = 0;
        for (const char* time : {"morn", "noon", "eve", "night"}) {
            const auto shd = readFile(base / (prefix + "shad_" + time + ".shd"));
            const auto directory = shd ? oj::parseShd(*shd) : std::nullopt;
            if (!directory) {
                ++badTxb;
                break;
            }
            for (const oj::TextureEntry& t : directory->textures) tileBytes += t.dataSize;
            txbEnd = std::max(txbEnd, directory->end);
        }
        const auto txb = game.read(base / (prefix + "shadowMaps.txb"));
        if (!txb || txb->size() != txbEnd || txb->size() != tileBytes) ++badTxb;
        const auto d8t = game.read(base / (tdf->track + ".d8t"));
        for (const oj::TdfRoute& route : tdf->routes) {
            ++routes;
            const std::string routeBase =
                lower(tdf->track) + "_" + lower(route.name.substr(0, route.name.size() - 4));
            const auto mats = readFile(base / (routeBase + ".d8m"));
            const auto materials = mats ? oj::parseRouteMaterials(*mats) : std::nullopt;
            bool ok = materials.has_value();
            size_t routeInstances = 0;
            for (const oj::TdfEntry& e : route.entries) {
                if (e.name.size() < 4 || _stricmp(e.name.c_str() + e.name.size() - 4, ".lyr") != 0) {
                    continue;
                }
                const std::string layer = e.name.substr(0, e.name.size() - 4);
                auto geometry = readFile(base / (prefix + layer + ".d8g"));
                if (!geometry) geometry = readFile(base / (layer + ".d8g"));
                const auto groups = geometry ? oj::parseD8g(*geometry) : std::nullopt;
                if (!groups) {
                    ok = false;
                    continue;
                }
                ++layers;
                for (const oj::TrackGroup& g : *groups) {
                    for (const oj::TrackMesh& m : g.meshes) {
                        routeInstances += m.instanceCount();
                        if (materials && (m.materialId >= materials->remap.size() ||
                                          materials->remap[m.materialId] < 0)) {
                            ok = false;
                        }
                    }
                }
            }
            instances += routeInstances;
            const auto world = readFile(base / (routeBase + ".d8w"));
            const auto w = world ? oj::parseD8w(*world, routeInstances) : std::nullopt;
            // The render tree: every instance of the route in exactly one leaf; a node's box holds its
            // children's.
            if (w) {
                const auto tree = oj::parseRenderTree(w->tree);
                std::vector<uint32_t> held(routeInstances, 0);
                bool treeOk = tree.has_value();
                if (tree) {
                    for (const oj::RenderTreeNode& n : *tree) {
                        for (uint32_t i : n.instances) {
                            if (i < held.size()) ++held[i];
                        }
                        if (n.leaf) continue;
                        for (uint32_t c : n.children) {
                            const oj::RenderTreeNode& k = (*tree)[c];
                            if (k.leaf && k.instances.empty()) continue;
                            treeOk = treeOk && k.low[0] >= n.low[0] && k.high[0] <= n.high[0] &&
                                     k.low[2] >= n.low[2] && k.high[2] <= n.high[2];
                        }
                    }
                    for (uint32_t h : held) treeOk = treeOk && h == 1;
                }
                if (!treeOk) std::printf("     render tree wrong: %s\n", route.name.c_str());
                ok = ok && treeOk;
            }
            ok = ok && w && w->sets.size() == 4;
            if (w && d8t) {
                for (const oj::TextureEntry& t : w->textures.textures) {
                    ok = ok && static_cast<size_t>(t.offset) + t.dataSize <= d8t->size();
                }
            }
            if (const auto spl = game.read(base / (routeBase + ".spl"))) {
                const oj::Spl lines =
                    oj::parseSpl(std::string(reinterpret_cast<const char*>(spl->data()), spl->size()));
                ok = ok && !lines.grid.empty() && !lines.fast.empty() &&
                     lines.fast.size() == lines.attack.size() && lines.fast.size() == lines.centre.size();
                // The route: the lap's length, the fifths, the curve through the points (at t = 0 the
                // point itself, a flat direction of unit length).
                const oj::Route r(lines);
                oj::Vec4 at{}, heading{};
                r.curve(oj::Route::kFast, 3, 0.0f, at, heading);
                const oj::RoutePoint& p3 = r.lines[0][3];
                const bool routeOk = r.lengths[0] > 0.0f && r.lengths[1] > 0.0f &&
                                     r.sectors[4].first == r.count() / 5 * 4 && at[0] == p3.x &&
                                     at[1] == p3.y && at[2] == p3.z && heading[1] == 0.0f &&
                                     std::fabs(oj::length3(heading) - 1.0f) < 1e-5f;
                if (!routeOk) std::printf("     route's lines wrong: %s\n", route.name.c_str());
                ok = ok && routeOk;
                corners += r.corners.size();
                // The collision: the box of a metre around the first grid place finds road faces.
                const auto collision = readFile(base / (routeBase + ".cld"));
                const auto cld = collision ? oj::parseCld(*collision) : std::nullopt;
                if (cld && !lines.grid.empty()) {
                    std::vector<uint32_t> faces, segments;
                    const oj::Spl::GridPoint& g = lines.grid[0];
                    cld->query(g.x - 100.0f, g.z - 100.0f, g.x + 100.0f, g.z + 100.0f, &faces, segments);
                    ok = ok && !faces.empty();
                    // The road's heights under the place (0x0053F2A0): a face, the least not above the
                    // greatest.
                    float lowest = 0.0f, highest = 0.0f;
                    ok = ok && oj::groundSpan(*cld, g.x, g.z, lowest, highest) && lowest <= highest;
                    ++cldRead;
                } else {
                    ok = false;
                }
            } else {
                ok = false;
            }
            // The regions: a record per instance; the box holds the grid's cells.
            const auto regions = readFile(base / (routeBase + ".ocl"));
            const auto ocl = regions ? oj::parseOcl(*regions) : std::nullopt;
            ok = ok && ocl && ocl->records.size() == routeInstances && ocl->region(ocl->min[0], ocl->min[1],
                                                                               ocl->min[2]) == ocl->grid[0];
            if (!ok) {
                ++badRoutes;
                std::printf("     route not read whole: %s %s (materials %d, world %d, regions %d, "
                            "instances %zu)\n",
                            trackFolder.c_str(), route.name.c_str(), materials.has_value() ? 1 : 0,
                            w.has_value() ? 1 : 0, ocl ? static_cast<int>(ocl->records.size()) : -1,
                            routeInstances);
            }
        }
    }
    std::printf("     tracks: %zu, %zu routes (%zu not whole), %zu layer files, %zu instances, %zu collisions, "
                "%zu corners\n",
                tracks, routes, badRoutes, layers, instances, cldRead, corners);
    check(tracks == 9 && routes == 108 && badRoutes == 0 && badTxb == 0,
          "tracks: routes, geometry layers, materials, textures, light tiles, regions, racing lines and "
          "collisions read");
}
