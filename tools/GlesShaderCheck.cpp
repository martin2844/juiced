#include "Android/Render/GlShader.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc, char **argv) {
    if (argc != 2 && argc != 3)
        return 2;
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    auto *w = SDL_CreateWindow("GLES shader check", 32, 32, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!w) {
        std::cerr << SDL_GetError();
        return 1;
    }
    auto context = SDL_GL_CreateContext(w);
    if (!context) {
        std::cerr << SDL_GetError();
        return 1;
    }
    unsigned pass = 0, fail = 0;
    for (auto &e : std::filesystem::directory_iterator(argv[1])) {
        if (e.path().extension() != ".bin")
            continue;
        auto n = e.file_size();
        if (n < 8 || n % 4) {
            ++fail;
            continue;
        }
        std::vector<uint32_t> code(n / 4);
        std::ifstream in(e.path(), std::ios::binary);
        in.read((char *)code.data(), n);
        if (!in || code.back() != 0xffff) {
            ++fail;
            continue;
        }
        try {
            oj::gles::Shader shader(code.data());
            if (argc == 3) {
                std::filesystem::create_directories(argv[2]);
                auto output = std::filesystem::path(argv[2]) /
                              (e.path().stem().string() + ((code[0] >> 16) == 0xfffe ? ".vert" : ".frag"));
                std::ofstream(output) << shader.source;
            }
            ++pass;
        } catch (const std::exception &e2) {
            ++fail;
            std::cerr << e.path() << ": " << e2.what() << '\n';
        }
    }
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(w);
    SDL_Quit();
    std::cout << pass << " shaders compiled; " << fail << " failures\n";
    return fail || !pass ? 1 : 0;
}
