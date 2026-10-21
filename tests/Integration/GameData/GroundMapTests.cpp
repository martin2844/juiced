// Integration tests: GroundMap.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Tracks/SunMap.h"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// Every ground map of the tracks reads to its end; each pixel's value as the walk down the quadtree gives
// it equals the value of the map filled quarter by quarter.
OJ_INTEGRATION_TEST(GroundMaps, EverySqhPqh) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    size_t mapFiles = 0, read = 0, agree = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(game.root() / "tracks", error)) {
        const auto ext = entry.path().extension();
        if (!entry.is_regular_file() ||
            (_wcsicmp(ext.c_str(), L".sqh") != 0 && _wcsicmp(ext.c_str(), L".pqh") != 0)) {
            continue;
        }
        ++mapFiles;
        const auto file = game.read(entry.path().lexically_relative(game.root()));
        if (!file) continue;
        const std::vector<uint8_t> raw(reinterpret_cast<const uint8_t*>(file->data()),
                                       reinterpret_cast<const uint8_t*>(file->data()) + file->size());
        const auto map = oj::parseSunMap(raw);
        if (!map) continue;
        ++read;
        std::vector<uint8_t> codes(static_cast<size_t>(map->width) * static_cast<size_t>(map->height));
        std::function<void(uint32_t, int32_t, int32_t, int32_t)> fill = [&](uint32_t ref, int32_t x,
                                                                             int32_t y, int32_t size) {
            if (ref < 0xFF) {
                for (int32_t j = 0; j < size; ++j) {
                    for (int32_t i = 0; i < size; ++i) {
                        codes[static_cast<size_t>((y + j) * map->width + x + i)] = static_cast<uint8_t>(ref);
                    }
                }
            } else if (size == 8) {
                const auto& b = map->blocks[ref - 0xFF];
                for (int32_t j = 0; j < 8; ++j) {
                    for (int32_t i = 0; i < 8; ++i) {
                        codes[static_cast<size_t>((y + j) * map->width + x + i)] =
                            static_cast<uint8_t>((b[static_cast<size_t>(j)] >> (2 * i)) & 3);
                    }
                }
            } else {
                const int32_t h = size / 2;
                const auto& n = map->nodes[ref - 0xFF];
                fill(n[0], x, y, h);
                fill(n[1], x + h, y, h);
                fill(n[2], x, y + h, h);
                fill(n[3], x + h, y + h, h);
            }
        };
        for (int32_t r = 0; r < map->tilesDown; ++r) {
            for (int32_t c = 0; c < map->tilesAcross; ++c) {
                fill(map->tiles[static_cast<size_t>(r * map->tilesAcross + c)], c * 64, r * 64, 64);
            }
        }
        bool same = true;
        for (int32_t y = 0; y < map->height && same; y += 3) {
            for (int32_t x = 0; x < map->width; x += 3) {
                const uint8_t code = codes[static_cast<size_t>(y * map->width + x)];
                const auto expected = static_cast<uint8_t>(
                    map->bias + static_cast<uint32_t>(static_cast<float>(code) / map->step));
                if (map->value(y, x) != expected) {
                    same = false;
                    break;
                }
            }
        }
        if (same) ++agree;
    }
    std::printf("     ground maps: %zu of %zu read, %zu walked as filled\n", read, mapFiles, agree);
    check(mapFiles > 0 && read == mapFiles && agree == mapFiles,
          "ground maps: every .sqh and .pqh read to its end, its quadtree walked as filled");
}
