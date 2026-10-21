#include "Mac/Controls.h"
#include "Core/Base/Config.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Game/App/GameLoop.h"
#include "Game/Platform/CommandLine.h"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
std::filesystem::path chooseGameFolder();
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    std::filesystem::path game;
    std::string command;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--controls") {
            oj::mac::requestControlsEditor();
            continue;
        }
        if (arg == "--game") {
            if (i + 1 == argc) {
                std::cerr << "--game requires a folder path\n";
                return 2;
            }
            game = argv[++i];
            continue;
        }
        if (arg == "--help") {
            std::cout << "Juiced for macOS\nUsage: juiced --game /path/to/Juiced [--frames N] [--capture image.bmp] "
                         "[-i] [--autopilot] [--controls]\n";
            return 0;
        }
        command += '"' + arg + '"';
        command += ' ';
    }
    try {
        const char *home = getenv("HOME");
        auto support = std::filesystem::path(home ? home : ".") / "Library/Application Support/Juiced";
        std::filesystem::create_directories(support);
        if (game.empty()) {
            std::ifstream saved(support / "game-path.txt");
            std::string path;
            std::getline(saved, path);
            if (!path.empty() && oj::GameFiles(path).valid())
                game = path;
            else
                game = chooseGameFolder();
            if (game.empty())
                return 0;
        }
        game = std::filesystem::absolute(game);
        if (!oj::GameFiles(game).valid()) {
            std::cerr << "Invalid game folder: " << game << '\n';
            return 2;
        }
        std::ofstream(support / "game-path.txt") << game.string() << '\n';
        std::filesystem::current_path(game);
        oj::log::open(support / "Juiced.log");
        oj::log::info("Juiced macOS native port; game data {}", game.string());
        auto config = oj::Config::load(support / "OpenJuice.ini");
        config.skipIntroMovies = true;
        auto options = oj::parseCommandLine(command.c_str());
        int result = app::gameMain(options, config);
        oj::log::close();
        return result;
    } catch (const std::exception &e) {
        std::cerr << "Juiced: " << e.what() << '\n';
        oj::log::error("fatal: {}", e.what());
        oj::log::close();
        return 1;
    }
}
