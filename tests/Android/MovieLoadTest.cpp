#include "Android/Render/GlesDevice.h"
#include "Game/Media/Movies.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) {
    if (argc != 2) { std::cerr << "Usage: android_movie_check GAME_DATA\n"; return 2; }
    const auto game = std::filesystem::absolute(argv[1]);
    std::filesystem::current_path(game);
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    auto *window = SDL_CreateWindow("Character movies", 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    int result = 0;
    try {
        oj::render::GlesDevice device({reinterpret_cast<HWND>(window)});
        oj::movie::Game setup;
        setup.folder = game;
        setup.audioFlags = 1;
        oj::movie::System movies(device, setup);
        const float low[2] = {0, 0}, high[2] = {640, 480};
        for (const char *name : {"Biggi\\BIGGI_ACCEPT_BET1", "biggi\\biggi_accept_bet9"}) {
            auto *player = movies.acquire(name, false, low, high);
            if (!player || player->noMovie()) throw std::runtime_error(std::string("Character clip missing: ") + name);
            player->setFast(true);
            for (int i = 0; i < 3; ++i) {
                device.beginFrame();
                player->advance();
                player->draw();
                device.endFrame();
            }
            if (player->frameNum() < 2) throw std::runtime_error("Character clip does not advance");
            std::cout << name << ": frame " << player->frameNum() << '\n';
            movies.release(player);
        }
        auto *missing = movies.acquire("absent\\missing_movie", false, low, high);
        if (!missing || !missing->noMovie()) throw std::runtime_error("Missing movie should remain missing");
        movies.release(missing);
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; result = 1; }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
