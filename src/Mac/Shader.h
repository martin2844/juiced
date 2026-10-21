#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <map>
#include <memory>
#include <mojoshader.h>
#include <string>
#include <vector>
namespace oj::mac {
struct Registers {
    std::array<float, 8192 * 4> floats{};
    std::array<int32_t, 2048 * 4> ints{};
    std::array<int32_t, 2048> bools{};
};
class Shader {
  public:
    Shader(SDL_GPUDevice *, const uint32_t *, uint32_t cubeMask = 0);
    ~Shader();
    Shader(const Shader &) = delete;
    SDL_GPUShader *gpu = nullptr;
    const MOJOSHADER_parseData *parsed = nullptr;
    unsigned slots = 0;
    int attribute(MOJOSHADER_usage, int) const;
    void uniforms(SDL_GPUCommandBuffer *, const Registers &) const;
    SDL_GPUShader *forCubeSamplers(uint32_t mask);

  private:
    SDL_GPUDevice *device;
    uint32_t samplerMask = 0;
    std::vector<uint32_t> legacyCode;
    std::map<uint32_t, std::unique_ptr<Shader>> variants;
};
} // namespace oj::mac
