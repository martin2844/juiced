// Integration tests: UserSaves.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Text.h"
#include "Core/Career/Career.h"
#include "Core/Cars/Car.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Saves/Save.h"
#include "Core/Saves/SaveEntries.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;
using oj::test::bytesOf;

}  // namespace

OJ_INTEGRATION_TEST(UserSaves, RegCf1) {
    const oj::GameFiles& files = oj::test::saveFiles();
    if (const auto reg = files.read(L"reg.cf1")) {
        oj::save::RegCf1 r;
        const bool ok = r.read(*reg);
        check(ok, "reg.cf1 version and sum");
        check(r.write() == bytesOf(*reg), "reg.cf1 written back equals the file");
    }
}
OJ_INTEGRATION_TEST(UserSaves, Drivers) {
    const std::filesystem::path& root = oj::test::saveRoot();
    std::vector<std::pair<std::string, oj::Car>> savedCars;
    for (const oj::save::Driver& d : oj::save::listDrivers(root / L"SAVE")) {
        const std::string label = oj::toUtf8(d.folder.filename().wstring());
        const oj::GameFiles folder(d.folder);
        const auto userData = folder.read(L"USERDATA");
        check(d.userData.write() == bytesOf(*userData),
              label + " USERDATA (" + oj::toUtf8(d.userData.name) + ") written back equals the file");
        if (!d.hasArchive) {
            continue;
        }
        const auto file = folder.read(L"juicey.dat");
        oj::save::Archive archive;
        std::string error;
        const bool ok = archive.read(*file, &error);
        check(ok, label + " juicey.dat read, hash holds" + (ok ? "" : ": " + error));
        if (!ok) {
            continue;
        }
        std::string names;
        for (const auto& [name, data] : archive.entries) {
            names += " " + name + "(" + std::to_string(data.size()) + ")";
        }
        std::printf("     entries:%s\n", names.c_str());
        check(archive.write() == bytesOf(*file), label + " juicey.dat written back equals the file");
        if (const auto found = archive.entries.find("UIHelp.sav"); found != archive.entries.end()) {
            oj::save::UiHelp help;
            check(help.read(found->second) && help.write() == found->second &&
                      help.shown.size() == std::size(oj::save::UiHelp::kPages),
                  label + " UIHelp.sav: a flag per help page, written back equal");
        }
        if (const auto found = archive.entries.find("ArcadeUnlock.sav"); found != archive.entries.end()) {
            oj::save::ArcadeUnlock arcade;
            check(arcade.read(found->second) && arcade.write() == found->second && arcade.series.size() == 15,
                  label + " ArcadeUnlock.sav: 15 series, written back equal");
        }
        if (const auto found = archive.entries.find("settings.dat"); found != archive.entries.end()) {
            oj::save::Settings settings;
            check(settings.read(found->second) && settings.write() == found->second,
                  label + " settings.dat: the stock fields fill it, written back equal");
        }
        if (const auto found = archive.entries.find("CMSGlobal.sav"); found != archive.entries.end()) {
            auto careerObject = std::make_unique<oj::Career>();
            oj::Career& career = *careerObject;
            std::string why;
            const bool read = career.read(found->second, &why);
            check(read, label + " CMSGlobal.sav read to its last byte" + (read ? "" : ": " + why));
            if (read) {
                check(career.write() == found->second, label + " CMSGlobal.sav written back equals the entry");
                std::string cars;
                for (const auto& car : career.garage) {
                    cars += " " + car.modelName();
                    savedCars.emplace_back(label, car);
                }
                std::printf("     garage %zu:%s\n", career.garage.size(), cars.c_str());
                std::printf("     crew:");
                for (const auto& name : career.crews[0].memberNames) std::printf(" %.25s", name.data());
                std::printf(", money %d, %d members, %d offers\n", career.profile.money, career.profile.memberCount,
                            career.dealer.offerCount);
            }
        }
    }
}
