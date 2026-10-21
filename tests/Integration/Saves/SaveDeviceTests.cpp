// Integration tests: SaveDevice.

#include "Framework/Test.h"

#include "Core/Saves/Save.h"
#include "Core/Saves/SaveDevice.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The save device (0x004FDB30): a driver written a file a step, listed, read back and deleted, in a folder
// next to the tests.
OJ_INTEGRATION_TEST(SaveDevice, DriverWrittenListedRead) {
    struct Side : oj::save::SaveGameSide {
        int regWrites = 0, regReads = 0;
        std::wstring readName;
        std::vector<uint8_t> readJuicey;
        void buildProfile(oj::save::SaveSource& source) override {
            oj::save::UserData u;
            u.value = 1.38f;
            u.name = L"Device Test";
            source.folder = oj::save::driverFolderName(u.name);
            source.files["USERDATA"] = u.write();
            source.files["juicey.dat"] = {1, 2, 3, 4};
        }
        bool applyProfile(const oj::save::SaveSlot& slot, const oj::save::SaveSource& source) override {
            readName = slot.name;
            const auto it = source.files.find("juicey.dat");
            if (it == source.files.end()) return false;
            readJuicey = it->second;
            return true;
        }
        void buildHighScores(oj::save::SaveSource&) override {}
        bool applyHighScores(const oj::save::SaveSource&) override { return true; }
        void writeRegCf1() override { ++regWrites; }
        void readRegCf1() override { ++regReads; }
    } side;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::filesystem::path save = std::filesystem::path(exe).parent_path() / L"SaveDeviceTest" / L"SAVE";
    std::error_code ec;
    std::filesystem::remove_all(save.parent_path(), ec);
    std::filesystem::create_directories(save.parent_path(), ec);
    oj::save::SaveDevice device;
    device.setFolder(save);
    oj::save::SaveRequest r;
    std::string trace;
    auto run = [&](int frames, auto&& before) {
        for (int i = 0; i < frames; ++i) {
            before(i);
            const oj::save::DeviceState state = device.stepProfile(r, side);
            trace += std::to_string(static_cast<int32_t>(state));
            for (size_t k = 0; k < oj::save::kFlagCount; ++k) {
                if (r.flags[k] != 0) trace += "k" + std::to_string(k);
            }
            trace += ' ';
        }
    };
    // Write: the request 1, then 10 steps; the end left with "next".
    r.request = oj::save::Request::Write;
    run(10, [&](int i) { if (i == 9) r.next = 1; });
    const bool written = std::filesystem::exists(save / L"PDevice_Test" / L"USERDATA") &&
                         std::filesystem::file_size(save / L"PDevice_Test" / L"juicey.dat", ec) == 4;
    std::printf("     save device write: %s\n", trace.c_str());
    check(written && trace == "3 3 3 3 3 3k9 3 3 3 0k12k24 " && side.regWrites == 2,
          "save device: a driver written a file a step (USERDATA, juicey.dat), Saving and Saved flags");
    // List: the request 0; one driver; then read it (next with the slot).
    trace.clear();
    r.request = oj::save::Request::List;
    run(4, [&](int i) {
        if (i == 3) {
            r.slot = 0;
            r.next = 1;
        }
    });
    const bool listed = device.slots().size() == 1 && device.slots()[0].name == L"Device Test" &&
                        device.slots()[0].folder == L"PDevice_Test";
    trace += "| ";
    run(5, [&](int i) { if (i == 4) r.next = 1; });
    std::printf("     save device list and read: %s\n", trace.c_str());
    check(listed && side.readName == L"Device Test" && side.readJuicey == std::vector<uint8_t>{1, 2, 3, 4} &&
              side.regReads == 2,
          "save device: the driver listed and read back a file a step");
    // Write again: the folder is there, the question k7 until answered yes.
    trace.clear();
    r.request = oj::save::Request::Write;
    run(6, [&](int i) { if (i == 5) r.answer = 1; });
    std::printf("     save device overwrite: %s\n", trace.c_str());
    check(trace.find("3k7k26") != std::string::npos, "save device: writing over a driver asks first");
    run(6, [&](int i) { if (i == 5) r.next = 1; });
    // Delete: list, remove the slot, answer yes.
    trace.clear();
    r.request = oj::save::Request::List;
    run(8, [&](int i) {
        if (i == 3) {
            r.slot = 0;
            r.remove = 1;
        }
        if (i == 5) r.answer = 1;
    });
    std::printf("     save device delete: %s\n", trace.c_str());
    check(!std::filesystem::exists(save / L"PDevice_Test") && trace.find("k21") != std::string::npos,
          "save device: a driver deleted after its question");
    std::filesystem::remove_all(save.parent_path(), ec);
}

// A player's AppData without THQ\Juiced (the stock game never run there): the device makes only SAVE (0x00502C50),
// so a write fails as the stock's does (its end with no flag); the folders made as the game starts (makeFolders, the
// file object 0x00502E60) let it write.
OJ_INTEGRATION_TEST(SaveDevice, FreshRoot) {
    struct Side : oj::save::SaveGameSide {
        void buildProfile(oj::save::SaveSource&) override {}
        bool applyProfile(const oj::save::SaveSlot&, const oj::save::SaveSource&) override { return true; }
        void buildHighScores(oj::save::SaveSource& source) override {
            source.folder = L"HS";
            source.files["USERDATA"] = {1, 2, 3};
        }
        bool applyHighScores(const oj::save::SaveSource&) override { return true; }
        void writeRegCf1() override {}
        void readRegCf1() override {}
    } side;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::filesystem::path appData = std::filesystem::path(exe).parent_path() / L"SaveFreshRootTest";
    const std::filesystem::path save = appData / L"THQ" / L"Juiced" / L"SAVE";
    std::error_code ec;
    std::filesystem::remove_all(appData, ec);
    std::filesystem::create_directories(appData, ec);
    const auto write = [&] {
        oj::save::SaveDevice device;
        device.setFolder(save);
        oj::save::SaveRequest r;
        r.request = oj::save::Request::Write;
        bool saved = false;
        for (int i = 0; i < 10; ++i) {
            if (i == 9) r.next = 1;
            device.stepHighScores(r, side);
            saved = saved || r.flag(oj::save::SaveFlag::Saved) != 0;
        }
        return saved;
    };
    check(!write() && !std::filesystem::exists(save, ec), "without THQ\\Juiced the device's write fails, no flag");
    oj::save::makeFolders(save);
    oj::save::makeFolders(save);
    check(std::filesystem::is_directory(save, ec), "the folders down to SAVE made (twice: the second passes over)");
    check(write() && std::filesystem::file_size(save / L"HS" / L"USERDATA", ec) == 3,
          "then the records written in SAVE\\HS");
    std::filesystem::remove_all(appData, ec);
}
