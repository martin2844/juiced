// Integration tests: Replay.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Race/Replay.h"
#include "Core/Saves/SaveDevice.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

using oj::test::check;
using oj::test::bytesOf;

}  // namespace

// A replay of the game's attract mode (attracts\attract1.hdr, .dat; 0x00429720): its header, its cars, its
// blocks played to the end (0x00427560) with each frame's car in the race and the time reaching the end.
OJ_INTEGRATION_TEST(Replay, AttractReplay) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const auto hdr = game.read(L"attracts\\attract1.hdr");
    const auto dat = game.read(L"attracts\\attract1.dat");
    if (hdr && dat) {
        oj::Replay replay;
        const bool loaded = replay.load(bytesOf(*hdr), bytesOf(*dat));
        oj::ReplayRace race;
        race.cars.assign(replay.header.cars, nullptr);
        race.mode = static_cast<oj::RaceMode>(replay.header.mode);
        race.type = static_cast<oj::RaceType>(replay.header.type);
        int frames = 0;
        bool pause = false;
        bool carsHeld = true;
        while (replay.state == oj::ReplayState::Playing && frames < 100000) {
            replay.step(race, 1.0f / 60.0f, pause);
            for (uint32_t i = 0; i < replay.cars; ++i) {
                carsHeld = carsHeld && replay.next[i].car == static_cast<int32_t>(i) &&
                           std::isfinite(replay.next[i].position[0]);
            }
            ++frames;
        }
        std::printf("     replay: %s %s, %u cars, mode %u type %u, %.2f s, %d frames to its end at %.2f s\n",
                    replay.header.track.c_str(), replay.header.route.c_str(), replay.header.cars,
                    replay.header.mode, replay.header.type, replay.header.length, frames, replay.readTime);
        check(loaded && replay.savedCars.size() == replay.header.cars && replay.header.cars == 6 &&
                  replay.header.route == "Downtown_R1" && replay.state == oj::ReplayState::Stopped && carsHeld &&
                  replay.readTime > replay.header.length - 1.0f && replay.readTime <= replay.header.length,
              "replay: the attract replay's header, cars and frames read to its end");
        // Saved as the replays' object saves (0x004292C0) with its blocks written (the file's), the replay's
        // files are the attract's byte for byte; read back the blocks written are its last block's place.
        oj::Replay resaved;
        resaved.load(bytesOf(*hdr), bytesOf(*dat));
        resaved.written = resaved.header.bytes / static_cast<uint32_t>(oj::Replay::kBlockSize);
        oj::save::SaveSource source;
        resaved.saveTo(source, "1/2/2026  3:04", L"Player");
        const std::vector<uint8_t>& user = source.files["USERDATA"];
        const bool same =
            source.files["replay.hdr"] == bytesOf(*hdr) && source.files["replay.dat"] == bytesOf(*dat);
        const bool user6 = user.size() == 0x79 && user[0] == 6 &&
                           std::memcmp(user.data() + 0x48, "1/2/2026  3:04", 15) == 0 && user[0x59] == 'P' &&
                           user[0x5A] == 0;
        oj::Replay back;
        const bool readBack = back.loadFrom(source) && back.written == back.header.lastBlock &&
                           back.savedCars.size() == 6 && back.header.bytes == resaved.header.bytes;
        check(same && user6 && readBack,
              "replay: the attract replay saved again is its files; read back from them");
    }
}

// The fixed build plays attracts\attract3 (0x00467260 never reached it): its header and cars read and its blocks
// played to the end as the others'.
OJ_INTEGRATION_TEST(Replay, ThirdAttractReplay) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const auto hdr = game.read(L"attracts\\attract3.hdr");
    const auto dat = game.read(L"attracts\\attract3.dat");
    check(hdr && dat, "replay: attracts\\attract3 in the game's files");
    if (!hdr || !dat) return;
    oj::Replay replay;
    const bool loaded = replay.load(bytesOf(*hdr), bytesOf(*dat));
    oj::ReplayRace race;
    race.cars.assign(replay.header.cars, nullptr);
    race.mode = static_cast<oj::RaceMode>(replay.header.mode);
    race.type = static_cast<oj::RaceType>(replay.header.type);
    int frames = 0;
    bool pause = false;
    bool carsHeld = true;
    while (replay.state == oj::ReplayState::Playing && frames < 100000) {
        replay.step(race, 1.0f / 60.0f, pause);
        for (uint32_t i = 0; i < replay.cars; ++i) {
            carsHeld = carsHeld && replay.next[i].car == static_cast<int32_t>(i) &&
                       std::isfinite(replay.next[i].position[0]);
        }
        ++frames;
    }
    std::printf("     replay: %s %s, %u cars, mode %u type %u, %.2f s, %d frames to its end at %.2f s\n",
                replay.header.track.c_str(), replay.header.route.c_str(), replay.header.cars, replay.header.mode,
                replay.header.type, replay.header.length, frames, replay.readTime);
    check(loaded && replay.header.cars > 0 && replay.savedCars.size() == replay.header.cars &&
              replay.state == oj::ReplayState::Stopped && carsHeld &&
              replay.readTime > replay.header.length - 1.0f && replay.readTime <= replay.header.length,
          "replay: attract3's header, cars and frames read to its end");
}
