// Compile a directory of caller-supplied D3D shader bytecode through the native Metal backend.
#include "Mac/Shader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: juiced_shader_check directory\n";
        return 2;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << SDL_GetError();
        return 1;
    }
    auto gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, true, "metal");
    if (!gpu) {
        std::cerr << SDL_GetError();
        SDL_Quit();
        return 1;
    }
    unsigned passed = 0, failed = 0;
    try {
        for (auto &entry : std::filesystem::directory_iterator(argv[1])) {
            if (!entry.is_regular_file() || entry.path().extension() != ".bin")
                continue;
            auto size = entry.file_size();
            if (size < 8 || size % 4) {
                ++failed;
                continue;
            }
            std::vector<uint32_t> bytes(size / 4);
            std::ifstream file(entry.path(), std::ios::binary);
            if (!file.read(reinterpret_cast<char *>(bytes.data()), size)) {
                ++failed;
                continue;
            }
            // The engine's parser requires an END marker and accepts only trusted game data.
            if (bytes.back() != 0xffff) {
                ++failed;
                continue;
            }
            try {
                oj::mac::Shader shader(gpu, bytes.data());
                ++passed;
            } catch (const std::exception &e) {
                ++failed;
                std::cerr << entry.path() << ": " << e.what() << '\n';
            }
        }
    } catch (const std::exception &e) {
        ++failed;
        std::cerr << e.what() << '\n';
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    std::cout << passed << " shaders compiled; " << failed << " failures\n";
    return failed || !passed ? 1 : 0;
}
