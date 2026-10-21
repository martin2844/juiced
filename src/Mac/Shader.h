#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <mojoshader.h>
#include <string>
namespace oj::mac {
struct Registers {
    std::array<float, 8192 * 4> floats{};
    std::array<int32_t, 2048 * 4> ints{};
    std::array<int32_t, 2048> bools{};
};
class Shader {
  public:
    Shader(SDL_GPUDevice *, const uint32_t *);
    ~Shader();
    Shader(const Shader &) = delete;
    SDL_GPUShader *gpu = nullptr;
    const MOJOSHADER_parseData *parsed = nullptr;
    unsigned slots = 0;
    int attribute(MOJOSHADER_usage, int) const;
    void uniforms(SDL_GPUCommandBuffer *, const Registers &) const;

  private:
    SDL_GPUDevice *device;
};
} // namespace oj::mac
