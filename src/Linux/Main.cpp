#include "Linux/Paths.h"
#include "Game/App/Version.h"
#include "Mac/Controls.h"
#include "Core/Base/Config.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Game/App/GameLoop.h"
#include "Game/Platform/CommandLine.h"
#include <csignal>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    try {
        std::filesystem::path game;
        std::string command;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--game") {
                if (++i == argc) { std::cerr << "--game requires a folder\n"; return 2; }
                game = argv[i];
            } else if (arg == "--controls") {
                oj::mac::requestControlsEditor();
            } else if (arg == "--help") {
                std::cout << "OpenJuice " OJ_VERSION_TEXT " for Linux\nUsage: juiced --game DATA [-i] [--autopilot] "
                             "[--frames N] [--capture /absolute/file.bmp] [--controls]\n";
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
        const int result = app::gameMain(oj::parseCommandLine(command.c_str()), config);
        oj::log::close();
        return result;
    } catch (const std::exception& e) {
        std::cerr << "OpenJuice: " << e.what() << '\n';
        oj::log::error("fatal: {}", e.what());
        oj::log::close();
        return 1;
    }
}
