#include "Linux/Paths.h"
#include "Game/App/Version.h"
#include "Mac/Controls.h"
#include "Mods/Creative.h"
#include "Mods/Packages.h"
#include "Mods/Runtime.h"
#include "Core/Base/Config.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Game/App/GameLoop.h"
#include "Game/Platform/CommandLine.h"
#include <csignal>
#include <fstream>
#include <iostream>

namespace {
struct GeneratedOverlay {
    std::filesystem::path path;
    ~GeneratedOverlay() { if (!path.empty()) { std::error_code ec; std::filesystem::remove_all(path, ec); } }
};
}
int main(int argc, char** argv) {
    GeneratedOverlay generated;
    signal(SIGPIPE, SIG_IGN);
    try {
        std::filesystem::path game, scripts, defaults, mod;
        bool creative = false, modsMenu = false, noMods = false, controls = false;
        std::string command;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--game") {
                if (++i == argc) { std::cerr << "--game requires a folder\n"; return 2; }
                game = argv[i];
            } else if (arg == "--mods" || arg == "--mod-defaults" || arg == "--mod") {
                if (++i == argc) { std::cerr << arg << " requires a folder\n"; return 2; }
                auto path = std::filesystem::absolute(argv[i]);
                if (arg == "--mods") { scripts = path; creative = true; }
                else if (arg == "--mod-defaults") defaults = path;
                else mod = path;
            } else if (arg == "--mods-menu" || arg == "--creative-mods") {
                creative = true;
                if (arg == "--mods-menu") modsMenu = true;
            } else if (arg == "--no-mods") {
                noMods = true;
            } else if (arg == "--controls") {
                controls = true;
            } else if (arg == "--help") {
                std::cout << "OpenJuice " OJ_VERSION_TEXT " for Linux\nUsage: juiced --game DATA [-i] [--autopilot] "
                             "[--frames N] [--capture /absolute/file.bmp] [--controls]\n"
                             "Mods: --mods DIR [--mod-defaults DIR] [--mods-menu] | --no-mods\n";
                return 0;
            } else { command += '"' + arg + "\" "; }
        }
        const auto configDir = oj::linux_port::configPath();
        const auto dataDir = oj::linux_port::dataPath();
        const auto stateDir = oj::linux_port::statePath();
        for (const auto& dir : {configDir, dataDir, stateDir}) std::filesystem::create_directories(dir);
        if (game.empty()) {
            std::ifstream saved(configDir / "game-path.txt");
            std::string path;
            std::getline(saved, path);
            game = path;
        }
        if (game.empty() || !oj::GameFiles(game).valid()) {
            std::cerr << "Supply your Juiced installation: juiced --game /path/to/Juiced\n";
            return 2;
        }
        game = std::filesystem::absolute(game);
        std::ofstream(configDir / "game-path.txt") << game.string() << '\n';
        oj::log::open(stateDir / "Juiced.log");
        oj::log::info("OpenJuice " OJ_VERSION_TEXT " native Linux x86_64; game data {}", game.string());
        auto config = oj::Config::load(configDir / "OpenJuice.ini");
        config.skipIntroMovies = true;
        config.saveFolder = dataDir / "saves";
        std::filesystem::create_directories(config.saveFolder);
        std::filesystem::current_path(game);
        auto options = oj::parseCommandLine(command.c_str());
        if (noMods) { creative = false; mod.clear(); }
        if (creative) {
            if (scripts.empty()) scripts = dataDir / "mods";
            if (!defaults.empty()) oj::mods::packages::seedMissing(defaults, scripts);
            if (!std::filesystem::is_directory(scripts)) throw std::runtime_error("Mods directory does not exist: " + scripts.string());
            oj::mods::packages::discover(scripts);
            if (oj::mods::packages::available()) {
                if (!mod.empty()) throw std::runtime_error("Use --mods packages or --mod legacy overlay, not both");
                generated.path = oj::mods::packages::assemble(game);
                mod = generated.path;
                const auto saves = options.saves.empty() ? dataDir / "ModSaves" : std::filesystem::absolute(options.saves);
                options.saves = oj::mods::packages::saveDirectory(saves).string();
                for (const auto& package : oj::mods::packages::list())
                    oj::log::info("mod package {}: {}", package.id, package.active ? "enabled" : "disabled");
                oj::log::info("mod overlay {}", mod.string());
            }
            oj::mods::runtime::load(scripts);
            oj::mods::configure((options.saves.empty() ? dataDir : std::filesystem::path(options.saves)) / "CreativeMods");
            if (modsMenu) oj::mods::requestManager();
        }
        oj::GameFiles::setOverrideRoot(mod);
        if (!mod.empty() && options.saves.empty()) options.saves = (dataDir / "LegacyModSaves").string();
        if (controls) oj::mac::requestControlsEditor();
        const int result = app::gameMain(options, config);
        oj::log::close();
        return result;
    } catch (const std::exception& e) {
        std::cerr << "OpenJuice: " << e.what() << '\n';
        oj::log::error("fatal: {}", e.what());
        oj::log::close();
        return 1;
    }
}
