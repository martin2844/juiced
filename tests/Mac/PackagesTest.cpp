#include "Mods/Packages.h"
#include "../Framework/Fakes/NullRenderDevice.h"
#include "Core/Base/GameFiles.h"
#include "Core/Data/GameData.h"
#include "Mac/Controls.h"
#include "Mods/CreativeMenu.h"
#include "Mods/Runtime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace oj;
namespace fs = std::filesystem;
void check(bool ok, const char *label) {
    if (!ok)
        throw std::runtime_error(label);
    std::cout << "PASS " << label << '\n';
}
void put(const fs::path &p, const std::string &s) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << s;
}
int main(int argc, char **argv) {
    if (argc == 4 && std::string(argv[3]) == "--assemble") {
        mods::packages::discover(fs::absolute(argv[2]));
        std::cout << mods::packages::assemble(fs::absolute(argv[1])).string() << '\n';
        return 0;
    }
    const auto temp = fs::temp_directory_path() / ("juiced-packages-" + std::to_string(getpid()));
    try {
        const auto support = temp / "shared-careers";
        put(support / "ModSaves/SAVE/PDriver/USERDATA", "current driver");
        put(support / "ModSaves/SAVE/PDriver/juicey.dat", "current progress");
        put(support / "saves/SAVE/PDriver/USERDATA", "old stock driver");
        put(support / "saves/SAVE/PDriver/juicey.dat", "stock progress");
        put(support / "ModSaves/variants/vinyls/SAVE/PVariant/USERDATA", "variant driver");
        put(support / "ModSaves/variants/vinyls/SAVE/PVariant/juicey.dat", "variant progress");
        put(support / "ModSaves/variants/vinyls/CreativeMods/test/creative.ini", "personal settings");
        const auto shared = mods::packages::sharedSaveDirectory(support);
        check(shared == support / "ModSaves", "one shared directory independent of installed mods");
        size_t recovered = 0;
        for (const auto &entry : fs::directory_iterator(shared / "SAVE")) {
            ++recovered;
            check(fs::exists(entry.path() / "juicey.dat"), "recovered profiles include the archive");
        }
        check(recovered == 3, "colliding stock and mod careers are both preserved");
        auto contents = [](const fs::path &p) {
            std::ifstream in(p); return std::string(std::istreambuf_iterator<char>(in), {});
        };
        check(contents(shared / "SAVE/PDriver/juicey.dat") == "current progress" &&
                  contents(support / "saves/SAVE/PDriver/juicey.dat") == "stock progress",
              "recovery never overwrites current or original saves");
        check(fs::exists(shared / "CreativeMods/test/creative.ini"), "creative settings recovered");
        put(shared / "SAVE/PVariant/juicey.dat", "newer progress");
        mods::packages::sharedSaveDirectory(support);
        check(contents(shared / "SAVE/PVariant/juicey.dat") == "newer progress" &&
                  std::distance(fs::directory_iterator(shared / "SAVE"), fs::directory_iterator{}) == 3,
              "repeated recovery neither duplicates careers nor rolls back progress");
        fs::remove_all(shared / "SAVE/PVariant");
        mods::packages::sharedSaveDirectory(support);
        check(!fs::exists(shared / "SAVE/PVariant"), "deleted imported careers do not reappear");
        const auto root = temp / "mods";
        put(temp / "defaults/new/main.lua", "bundled");
        put(temp / "defaults/.cache/stage-old/stale", "not runtime defaults");
        put(temp / "defaults/new/tools/import.py", "not needed to play");
        mods::packages::seedMissing(temp / "defaults", root);
        check(!fs::exists(root / ".cache") && !fs::exists(root / "new/tools"), "installation excludes cache and authoring tools");
        put(root / "new/main.lua", "edited");
        put(temp / "defaults/new/mod.ini", "manifest");
        mods::packages::seedMissing(temp / "defaults", root);
        std::ifstream existing(root / "new/main.lua");
        std::string edited;
        existing >> edited;
        check(edited == "edited" && fs::exists(root / "new/mod.ini"),
              "reinstall preserves edits and adds missing package files");
        fs::remove_all(root);
        put(root / "test/mod.ini", "name=Test Mod\nversion=1\nkind=script\n");
        put(root / "test/main.lua",
            "return {api=1,feature=function(n) if n=='controls' then return true end end,control_default=function(n) "
            "if n=='throttle' then return 'DIKEYBOARD_UP' end end}");
        mods::packages::discover(root);
        check(mods::packages::list().size() == 1 && mods::packages::active("test"), "manifest discovered");
        mods::runtime::load(root);
        check(mac::defaultBindings()[0] == "DIKEYBOARD_UP", "control default comes from runtime package");
        check(mods::packages::toggle("test").empty() && mods::packages::active("test") &&
                  !mods::packages::list()[0].enabled,
              "toggle persisted without changing active session");
        mods::packages::discover(root);
        mods::runtime::load(root);
        check(!mods::packages::active("test") && !mods::runtime::feature("controls"),
              "disabled package skipped on next launch");
        check(mods::packages::toggle("test").empty(), "disabled mod can be enabled again");
        check(!mods::packages::toggle("missing").empty(), "unknown toggle rejected");
        fs::create_directory(root / "test/disabled.tmp");
        check(!mods::packages::toggle("test").empty() && mods::packages::list()[0].enabled,
              "failed persistence preserves desired state");
        fs::remove_all(root);
        put(temp / "game/scripts/base.txt", "original\n");
        put(root / "a/mod.ini", "name=Asset\nversion=1\nkind=assets\n");
        put(root / "a/patches/scripts/base.txt.patch", "append\ntext 61646465640a\n");
        put(root / "a/assets/mesh.txt", "mesh");
        mods::packages::discover(root);
        auto overlay = mods::packages::assemble(temp / "game");
        GameFiles::setOverrideRoot(overlay);
        auto bytes = GameFiles(temp / "game").read("scripts/base.txt");
        check(bytes && std::string(reinterpret_cast<const char *>(bytes->data()), bytes->size()) == "original\nadded\n",
              "package patch composes with retail source");
        GameFiles::setOverrideRoot({});
        put(root / "a/patches/scripts/base.txt.patch", "base scripts/base.txt 0000\ncopy 0 9\n");
        bool failed = false;
        try {
            mods::packages::assemble(temp / "game");
        } catch (...) {
            failed = true;
        }
        check(failed, "mismatched retail data rejected");
        fs::remove_all(root);
        if (argc == 3) {
            const fs::path source = fs::absolute(argv[2]), game = fs::absolute(argv[1]);
            fs::create_directories(root);
            fs::copy_file(source / "collection.ini", root / "collection.ini");
            for (const auto &entry : fs::directory_iterator(source)) {
                if (!fs::is_regular_file(entry.path() / "mod.ini"))
                    continue;
                const auto target = root / entry.path().filename();
                fs::create_directories(target);
                fs::copy_file(entry.path() / "mod.ini", target / "mod.ini");
                for (const char *part : {"assets", "patches", "main.lua"})
                    if (fs::exists(entry.path() / part))
                        fs::create_symlink(entry.path() / part, target / part);
            }
            for (int combination = 0; combination < 4; ++combination) {
                const bool ferrari = combination & 1, wheels = combination & 2;
                for (auto [id, enabled] : {std::pair{"ferrari", ferrari}, {"wheels", wheels}}) {
                    fs::remove(root / id / "disabled");
                    if (!enabled)
                        put(root / id / "disabled", "");
                }
                mods::packages::discover(root);
                overlay = mods::packages::assemble(game);
                GameFiles::setOverrideRoot(overlay);
                CrtRandom random;
                GameData data(game);
                std::string error;
                if (!data.load(random, &error))
                    throw std::runtime_error(error);
                check(true, "game data loads with selected packages");
                check((data.defs.byId(53) != nullptr) == ferrari, "Ferrari availability follows package state");
                check(data.catalogue.lists[kWheel].parts.size() == (wheels ? 202 : 74),
                      "wheel catalogue follows package state");
                const auto saves = mods::packages::saveDirectory(temp / "saves");
                check(saves == temp / "saves",
                      "asset combinations keep the same careers");
                std::cout << "COMBINATION " << combination << " loaded\n";
                GameFiles::setOverrideRoot({});
            }
            fs::current_path(game);
            test::NullRenderDevice device;
            const auto language = *Language::byIndex(0);
            {
                mods::CreativeMenu menu(device, language, {false, {}, true});
                for (size_t i = 0; i < mods::packages::list().size(); ++i) {
                    menu.update({{28, true}});
                    check(!mods::packages::list()[i].enabled && mods::packages::list()[i].active,
                          "native menu toggles selected package for next launch");
                    menu.update({{208, true}});
                }
                check(!menu.update({{28, true}}), "native Back row exits manager");
            }
            mods::packages::discover(root);
            mods::runtime::load(root);
            check(mods::runtime::loaded().empty() && mods::packages::assemble(game).empty(),
                  "all mods disabled loads neither scripts nor assets");
            check(mods::packages::saveDirectory(temp / "saves") == temp / "saves",
                  "all-off keeps the same careers");
            {
                mods::CreativeMenu menu(device, language, {false, {}, true});
                for (size_t i = 0; i < mods::packages::list().size(); ++i) {
                    menu.update({{28, true}});
                    check(mods::packages::list()[i].enabled, "disabled mods can be re-enabled from menu");
                    menu.update({{208, true}});
                }
            }
            mods::packages::discover(root);
            mods::runtime::load(root);
            check(mods::runtime::feature("controls") && mods::runtime::feature("neon") &&
                      mods::runtime::feature("logo") && mods::runtime::feature("portrait"),
                  "all script features restored after re-enabling");
            check(mods::packages::toggle("custom-logo").empty(), "disable custom logo");
            mods::packages::discover(root);
            check(mods::packages::saveDirectory(temp / "saves") == temp / "saves",
                  "custom decal toggle keeps the same careers");
            mods::runtime::load({});
        }
        fs::remove_all(temp);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        GameFiles::setOverrideRoot({});
        fs::remove_all(temp);
        return 1;
    }
}
