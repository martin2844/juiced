#pragma once
#include "Gl.h"
#include <array>
#include <memory>
#include <mojoshader.h>
namespace oj::gles {
struct Registers {
    std::array<float, 8192 * 4> floats{};
    std::array<int32_t, 2048 * 4> ints{};
    std::array<int32_t, 2048> bools{};
};
class Shader {
  public:
    explicit Shader(const uint32_t *code);
    ~Shader();
    Shader(const Shader &) = delete;
    GLuint id = 0;
    const MOJOSHADER_parseData *parsed = nullptr;
    std::string source;
    void uniforms(GLuint program, const Registers &registers) const;
    int attribute(GLuint program, uint8_t usage, uint8_t index) const;
};
} // namespace oj::gles
