// Integration tests: ReplaysDevice.

#include "Framework/Test.h"

#include "Core/Cars/Car.h"
#include "Core/Race/Arcade.h"
#include "Core/Race/Replay.h"
#include "Core/Saves/SaveDevice.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The replays' device (0x004FA860): a folder R<time> listed with its USERDATA's fields (0x005026B0); its read
// has no replays' object to take it (none given, as on the stock PC), so it counts as corrupt (flag 18).
OJ_INTEGRATION_TEST(ReplaysDevice, FolderListed) {
    struct Side : oj::save::SaveGameSide {
        void buildProfile(oj::save::SaveSource&) override {}
        bool applyProfile(const oj::save::SaveSlot&, const oj::save::SaveSource&) override { return false; }
        void buildHighScores(oj::save::SaveSource&) override {}
        bool applyHighScores(const oj::save::SaveSource&) override { return false; }
        void writeRegCf1() override {}
        void readRegCf1() override {}
    } side;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::filesystem::path save = std::filesystem::path(exe).parent_path() / L"ReplayDeviceTest" / L"SAVE";
    std::error_code ec;
    std::filesystem::remove_all(save.parent_path(), ec);
    std::filesystem::create_directories(save / L"R20261013120000", ec);
    std::vector<uint8_t> data(0x79, 0);
    const int32_t cars = 4, type = 5;
    std::memcpy(data.data(), &cars, 4);
    std::memcpy(data.data() + 4, &type, 4);
    std::memcpy(data.data() + 8, "Angel SuperSpeedway", 19);
    std::memcpy(data.data() + 0x28, "Speedway_r1", 11);
    std::memcpy(data.data() + 0x48, "10/13/2026  1:05", 16);
    const char16_t driver[] = u"Test";
    std::memcpy(data.data() + 0x59, driver, 8);
    std::ofstream(save / L"R20261013120000" / L"USERDATA", std::ios::binary)
        .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    oj::save::SaveDevice device;
    device.setFolder(save);
    oj::save::SaveRequest r;
    std::string trace;
    r.request = oj::save::Request::List;
    for (int i = 0; i < 10; ++i) {
        if (i == 3) {
            r.slot = 0;
            r.next = 1;
        }
        if (i == 7) r.next = 1;
        const oj::save::DeviceState state = device.stepReplay(r, side);
        trace += std::to_string(static_cast<int32_t>(state));
        for (size_t k = 0; k < oj::save::kFlagCount; ++k) {
            if (r.flags[k] != 0) trace += "k" + std::to_string(k);
        }
        trace += ' ';
    }
    std::printf("     replays' device: %s\n", trace.c_str());
    const auto& list = device.replays();
    const bool listed = list.size() == 1 && list[0].cars == 4 && list[0].type == 5 &&
                        list[0].track == "Angel SuperSpeedway" && list[0].route == "Speedway_r1" &&
                        list[0].date == "10/13/2026  1:05" && list[0].driver == L"Test" &&
                        list[0].folder == L"R20261013120000";
    check(listed && trace.find("k18") != std::string::npos,
          "replays' device: an R folder listed with its USERDATA's fields; its read has no replays' object");
    std::filesystem::remove_all(save.parent_path(), ec);
}
// The replays' device with a replays' object (0x004FA8B0) over a replay: written into a temporary SAVE
// (0x004FA600: the folder R and the local time, the date's text; 0x004292C0: USERDATA, replay.dat and
// replay.hdr), listed with its USERDATA (0x005026B0) and read back into another replay (0x00429550): the
// header, the cars and the blocks as written, the play from the start.
OJ_INTEGRATION_TEST(ReplaysDevice, ReplayWrittenRead) {
    struct Side : oj::save::SaveGameSide {
        void buildProfile(oj::save::SaveSource&) override {}
        bool applyProfile(const oj::save::SaveSlot&, const oj::save::SaveSource&) override { return false; }
        void buildHighScores(oj::save::SaveSource&) override {}
        bool applyHighScores(const oj::save::SaveSource&) override { return false; }
        void writeRegCf1() override {}
        void readRegCf1() override {}
    } side;
    struct Saves : oj::save::SaveReplays {
        oj::Replay* replay = nullptr;
        bool save(oj::save::SaveSource& source, const std::string& date) override {
            return replay->saveTo(source, date, L"Tester");
        }
        bool take(const oj::save::SaveSource& source) override { return replay->loadFrom(source); }
    } saves;
    oj::Replay written;
    written.header.track = "Angel SuperSpeedway";
    written.header.route = "Speedway_r1";
    written.header.timeOfDay = 4;
    written.header.weather = 2;
    written.header.cars = 2;
    written.header.mode = 1;
    written.header.type = 5;
    written.header.laps = 3;
    written.header.length = 42.5f;
    written.header.lastBlock = 2;
    written.header.lastFrames = 17;
    written.written = 3;
    for (size_t i = 0; i < 3 * oj::Replay::kBlockSize; ++i) written.buffer[i] = static_cast<uint8_t>(i * 7 + 3);
    oj::Car first;
    oj::Car second;
    first.rating = std::bit_cast<uint32_t>(300.0f);
    second.rating = std::bit_cast<uint32_t>(450.0f);
    first.write(written.carsStream);
    second.write(written.carsStream);
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::filesystem::path save = std::filesystem::path(exe).parent_path() / L"ReplaySaveTest" / L"SAVE";
    std::error_code ec;
    std::filesystem::remove_all(save.parent_path(), ec);
    std::filesystem::create_directories(save.parent_path(), ec);
    oj::save::SaveDevice device;
    device.setFolder(save);
    const bool refused = !device.setReplayObject(nullptr) && device.replayObject() == nullptr;
    saves.replay = &written;
    device.setReplayObject(&saves);
    oj::save::SaveRequest r;
    const auto stepUntil = [&](oj::save::SaveFlag flag) {
        for (int i = 0; i < 40; ++i) {
            device.stepReplay(r, side);
            if (r.flag(flag) != 0) return true;
        }
        return false;
    };
    r.request = oj::save::Request::Write;
    const bool saved = stepUntil(oj::save::SaveFlag::Saved);
    r.next = 1;
    device.stepReplay(r, side);
    std::wstring folder;
    std::map<std::wstring, uintmax_t> sizes;
    for (const auto& e : std::filesystem::directory_iterator(save, ec)) {
        folder = e.path().filename().wstring();
        for (const auto& f : std::filesystem::directory_iterator(e.path(), ec)) {
            sizes[f.path().filename().wstring()] = f.file_size();
        }
    }
    const bool named = folder.size() == 15 && folder[0] == L'R' &&
                       folder.find_first_not_of(L"0123456789", 1) == std::wstring::npos;
    const bool written3 = sizes.size() == 3 && sizes[L"USERDATA"] == 0x79 &&
                       sizes[L"replay.dat"] == 3 * oj::Replay::kBlockSize &&
                       sizes[L"replay.hdr"] == oj::ReplayHeader::kSize + written.carsStream.size();
    r.request = oj::save::Request::List;
    const bool listed = stepUntil(oj::save::SaveFlag::Listed);
    const auto& list = device.replays();
    int month = 0, day = 0, year = 0, hour = 0, minute = 0;
    const bool slot = list.size() == 1 && list[0].cars == 2 && list[0].type == 5 &&
                      list[0].track == "Angel SuperSpeedway" && list[0].route == "Speedway_r1" &&
                      list[0].driver == L"Tester" && list[0].folder == folder &&
                      sscanf_s(list[0].date.c_str(), "%d/%d/%d %d:%d", &month, &day, &year, &hour, &minute) ==
                          5 &&
                      hour <= 12;
    oj::Replay read;
    saves.replay = &read;
    r.slot = 0;
    r.next = 1;
    const bool loaded = stepUntil(oj::save::SaveFlag::Loaded);
    const oj::ReplayHeader& a = written.header;
    const oj::ReplayHeader& b = read.header;
    const bool header = a.track == b.track && a.route == b.route && a.timeOfDay == b.timeOfDay &&
                        a.weather == b.weather && a.cars == b.cars && a.mode == b.mode && a.type == b.type &&
                        a.laps == b.laps && a.length == b.length && a.lastBlock == b.lastBlock &&
                        a.lastFrames == b.lastFrames && b.bytes == 3 * oj::Replay::kBlockSize &&
                        b.carsBytes == written.carsStream.size();
    const bool cars = read.carsStream == written.carsStream && read.savedCars.size() == 2 &&
                      read.savedCars[1].rating == second.rating && read.cars == 2;
    const bool blocks = std::equal(written.buffer.begin(), written.buffer.begin() + 3 * oj::Replay::kBlockSize,
                                   read.buffer.begin());
    const bool playing = read.saved == 1 && read.loaded == 0 && read.written == 2 &&
                         read.state == oj::ReplayState::Playing && read.playing == 1 && read.replayRace == 1 &&
                         read.length == 42.5f;
    std::printf("     replay saved: %ls (%s), %zu files; read back: %s %s\n", folder.c_str(),
                list.empty() ? "" : list[0].date.c_str(), sizes.size(), b.track.c_str(), b.route.c_str());
    check(refused && saved && named && written3 && listed && slot && loaded && header && cars && blocks && playing,
          "replays' device: a replay written through its replays' object, listed and read back");
    std::filesystem::remove_all(save.parent_path(), ec);
}
