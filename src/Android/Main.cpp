#include <SDL3/SDL.h>
#ifdef __ANDROID__
#include <SDL3/SDL_main.h>
#endif
#include "Core/Base/Config.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Game/App/GameLoop.h"
#include "Game/Platform/CommandLine.h"
#include <csignal>
#include <filesystem>
#include <iostream>
int runDiagnostics(const std::filesystem::path &support, const std::filesystem::path &game);
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    try {
        std::filesystem::path support, game;
        std::string command;
        bool diagnostics = false;
#ifdef __ANDROID__
        if (argc < 2)
            return 2;
        support = argv[1];
        game = support / "game";
        int first = 2;
#else
        support = std::filesystem::path(getenv("HOME")) / "Library/Application Support/JuicedAndroidHost";
        int first = 1;
#endif
        for (int i = first; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--diagnostics") {
                diagnostics = true;
                continue;
            }
            if (a == "--game" && i + 1 < argc) {
                game = argv[++i];
                continue;
            }
            if (a == "--help") {
                std::cout << "Juiced Android renderer host check: --game DATA [-i] [--frames N] [--capture FILE]\n";
                return 0;
            }
            command += '"' + a + '"';
            command += ' ';
        }
        if (game.empty() || !oj::GameFiles(game).valid()) {
            std::cerr << "Import Juiced game data first\n";
            return 2;
        }
        game = std::filesystem::absolute(game);
        std::filesystem::create_directories(support);
        setenv("JUICED_SUPPORT_DIR", support.c_str(), 1);
        if (diagnostics)
            return runDiagnostics(support, game);
        oj::log::open(support / "Juiced-android.log");
        std::filesystem::current_path(game);
        auto config = oj::Config::load(support / "OpenJuice.ini");
        config.skipIntroMovies = true;
        config.saveFolder = support / "saves";
        auto options = oj::parseCommandLine(command.c_str());
        int result = app::gameMain(options, config);
        oj::log::close();
        return result;
    } catch (const std::exception &e) {
        oj::log::error("Android port: {}", e.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Juiced", e.what(), nullptr);
        oj::log::close();
        return 1;
    }
}
