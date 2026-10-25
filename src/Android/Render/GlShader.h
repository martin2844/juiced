#pragma once
#include "Gl.h"
#include <array>
#include <map>
#include <memory>
#include <mojoshader.h>
#include <vector>
namespace oj::gles {
struct Registers {
    std::array<float, 8192 * 4> floats{};
    std::array<int32_t, 2048 * 4> ints{};
    std::array<int32_t, 2048> bools{};
};
class Shader {
  public:
    explicit Shader(const uint32_t *code, uint32_t cubeMask = 0);
    Shader &forCubeSamplers(uint32_t mask);
    ~Shader();
    Shader(const Shader &) = delete;
    GLuint id = 0;
    const MOJOSHADER_parseData *parsed = nullptr;
    std::string source;
    std::vector<uint32_t> legacyCode;
    uint32_t samplerMask = 0;
    std::map<uint32_t, std::unique_ptr<Shader>> variants;
    void uniforms(GLuint program, const Registers &registers) const;
    int attribute(GLuint program, uint8_t usage, uint8_t index) const;
};
} // namespace oj::gles
