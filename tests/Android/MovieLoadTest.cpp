#if defined(OJ_LINUX)
#include "Linux/Render/GlesDevice.h"
#else
#include "Android/Render/GlesDevice.h"
#endif
#include "Game/Media/Movies.h"
#include "Audio/SoundSystem.h"
#include <cstdlib>
#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) { std::cerr << "Usage: movie_check GAME_DATA [CAPTURE_DIR]\n"; return 2; }
    const auto game = std::filesystem::absolute(argv[1]);
    const auto capture = argc == 3 ? std::filesystem::absolute(argv[2]) : std::filesystem::path{};
    if (!capture.empty()) std::filesystem::create_directories(capture);
    std::filesystem::current_path(game);
    const bool withAudio = std::getenv("OJ_MOVIE_CHECK_AUDIO") != nullptr;
    if (!SDL_Init(SDL_INIT_VIDEO | (withAudio ? SDL_INIT_AUDIO : 0))) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    auto *window = SDL_CreateWindow("Character movies", 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    int result = 0;
    try {
        if (withAudio && !oj::audio::system().start(reinterpret_cast<HWND>(window), 0))
            throw std::runtime_error("Movie audio device failed to start");
        oj::render::GlesDevice device({reinterpret_cast<HWND>(window)});
        oj::movie::Game setup;
        setup.folder = game;
        // Keep the sink open for a silent negative control when requested.
        setup.audioFlags = withAudio && std::getenv("OJ_MOVIE_CHECK_MUTE") == nullptr ? 0 : 1;
        setup.languageTrack = 0; // English, matching the normal English game loop.
        oj::movie::System movies(device, setup);
        const float low[2] = {0, 0}, high[2] = {640, 480};
        int clip = 0;
        for (const char *name : {"Biggi\\BIGGI_ACCEPT_BET1", "biggi\\biggi_accept_bet9"}) {
            auto *player = movies.acquire(name, false, low, high);
            if (!player || player->noMovie()) throw std::runtime_error(std::string("Character clip missing: ") + name);
            player->setFast(true);
            const auto deadline = SDL_GetTicks() + 5000;
            while (player->frameNum() < 30 && SDL_GetTicks() < deadline) {
                device.beginFrame();
                device.clear(3, 0xff000000, 1, 0);
                player->advance();
                player->draw();
#if defined(OJ_LINUX)
                if (player->frameNum() >= 30) {
                    std::vector<uint8_t> pixels;
                    uint32_t pitch = 0;
                    if (!device.readTarget(0, pixels, &pitch) || pitch < 640 * 4
                        || pixels.size() < static_cast<size_t>(pitch) * 480)
                        throw std::runtime_error("Movie framebuffer readback failed");
                    size_t lit = 0;
                    for (size_t y = 0; y < 480; ++y)
                        for (size_t x = 0; x < 640; ++x) {
                            const size_t p = y * pitch + x * 4;
                            lit += pixels[p] != 0 || pixels[p + 1] != 0 || pixels[p + 2] != 0;
                        }
                    if (lit == 0) throw std::runtime_error("Movie draw left the framebuffer black");
                    std::cout << name << ": " << lit << " nonblack pixels\n";
                }
#endif
                device.endFrame();
                SDL_Delay(1);
            }
            if (player->frameNum() != 30) throw std::runtime_error("Movie did not reach frame 30 before deadline");
            if (!capture.empty() && !device.capture((capture / ("clip-" + std::to_string(clip) + ".bmp")).wstring()))
                throw std::runtime_error("Movie capture failed");
            ++clip;
            if (player->frameNum() < 2) throw std::runtime_error("Character clip does not advance");
            std::cout << name << ": frame " << player->frameNum() << '\n';
            movies.release(player);
        }
        auto *missing = movies.acquire("absent\\missing_movie", false, low, high);
        if (!missing || !missing->noMovie()) throw std::runtime_error("Missing movie should remain missing");
        movies.release(missing);
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; result = 1; }
    if (withAudio) oj::audio::system().stop();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
