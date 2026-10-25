#include "GlShader.h"
#include <algorithm>
#include <cstring>
#include <regex>
#include <stdexcept>
#include <vector>
namespace oj::gles {
std::string platformSource(std::string source) {
#ifndef __ANDROID__
    if (auto p = source.find("#version 300 es"); p != std::string::npos)
        source.replace(p, 15, "#version 410 core");
#endif
    return source;
}
GLuint compile(GLenum type, const std::string &input) {
    std::string source = platformSource(input);
    GLuint shader = glCreateShader(type);
    const char *p = source.c_str();
    glShaderSource(shader, 1, &p, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);
        std::string error(std::max(n, 1), '\0');
        glGetShaderInfoLog(shader, error.size(), nullptr, error.data());
        error.resize(strlen(error.c_str()));
        glDeleteShader(shader);
        throw std::runtime_error("GL shader: " + error + "\n" + source);
    }
    return shader;
}
GLuint link(GLuint vs, GLuint ps) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, ps);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &n);
        std::string error(std::max(n, 1), '\0');
        glGetProgramInfoLog(program, error.size(), nullptr, error.data());
        error.resize(strlen(error.c_str()));
        glDeleteProgram(program);
        throw std::runtime_error("GL program: " + error);
    }
    return program;
}
Shader::Shader(const uint32_t *code, uint32_t cubeMask) {
    auto original = MOJOSHADER_parse("bytecode", "main", reinterpret_cast<const unsigned char *>(code), 0,
                                     nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr);
    if (original->error_count) {
        std::string error = original->errors[0].error;
        MOJOSHADER_freeParseData(original);
        throw std::runtime_error(error);
    }
    bool vertex = original->shader_type == MOJOSHADER_TYPE_VERTEX;
    size_t words = original->output_len / 4;
    const bool legacyPixel = !vertex && original->major_ver == 1;
    if (legacyPixel)
        legacyCode.assign(code, code + words);
    MOJOSHADER_freeParseData(original);
    std::vector<uint32_t> lowered;
    if (code[0] == 0xffff0101) {
        // Legacy TEXCOORD saturates its interpolated register; MOV_SAT is equivalent.
        for (size_t i = 0; i < words; ++i) {
            auto token = code[i];
            if ((token & 0xffff) == 0xfffe) {
                size_t count = (token >> 16) & 0x7fff;
                if (i + count >= words)
                    throw std::runtime_error("Invalid shader comment length");
                lowered.insert(lowered.end(), code + i, code + i + count + 1);
                i += count;
            } else if (token == 64 && i + 1 < words) {
                uint32_t dst = code[++i];
                lowered.insert(lowered.end(), {1, dst | 0x00100000, (dst & 0xf0001fff) | 0x00e40000});
            } else
                lowered.push_back(token);
        }
        code = lowered.data();
        words = lowered.size();
    }
    MOJOSHADER_swizzle swizzles[] = {{MOJOSHADER_USAGE_COLOR, 0, {2, 1, 0, 3}},
                                     {MOJOSHADER_USAGE_COLOR, 1, {2, 1, 0, 3}}};
    std::vector<MOJOSHADER_samplerMap> samplerTypes;
    if (legacyPixel)
        for (int i = 0; i < 16; ++i)
            if (cubeMask & (1u << i))
                samplerTypes.push_back({i, MOJOSHADER_SAMPLER_CUBE});
    parsed = MOJOSHADER_parse("glsles3", "main", reinterpret_cast<const unsigned char *>(code), words * 4,
                              vertex ? swizzles : nullptr, vertex ? 2 : 0, samplerTypes.data(),
                              samplerTypes.size(), nullptr, nullptr, nullptr);
    if (parsed->error_count) {
        std::string error = parsed->errors[0].error;
        MOJOSHADER_freeParseData(parsed);
        parsed = nullptr;
        throw std::runtime_error(error);
    }
    for (int i = 0; i < parsed->sampler_count; ++i)
        samplerMask |= 1u << parsed->samplers[i].index;
    source.assign(parsed->output, parsed->output_len);
    // Pixel math requires high precision for the game's large world-space values.
    if (auto p = source.find("precision mediump float;"); p != std::string::npos)
        source.replace(p, strlen("precision mediump float;"), "precision highp float;");
    // MojoShader's ES profile declares fog as vec4 even though the legacy output is scalar.
    source = std::regex_replace(source, std::regex("(#define vs_oFog io_11_[0-9]+)(\\n)"), "$1.x$2");
    std::string legacyInputs, legacyInit;
    if (!vertex)
        for (int i = 0; i < 8; i++) {
            std::string name = "ps_t" + std::to_string(i), input = "io_5_" + std::to_string(i);
            std::string init = "vec4 " + name + " = " + input + ";";
            auto p = source.find(init);
            if (p != std::string::npos) {
                source.replace(p, init.size(), "vec4 " + name + ";");
                if (source.find("in highp vec4 " + input + ";") == std::string::npos)
                    legacyInputs += "in highp vec4 " + input + ";\n";
                legacyInit += name + "=" + input + ";";
            }
        }
    // Wrap main rather than patching individual RET paths. D3D alpha testing is pipeline state.
    auto main = source.find("void main()");
    if (main == std::string::npos)
        throw std::runtime_error("Shader entry point missing");
    source.replace(main, strlen("void main()"), "void oj_main()");
    if (vertex)
        source += "\nuniform float oj_flip;\nvoid "
                  "main(){oj_main();gl_Position.z=2.0*gl_Position.z-gl_Position.w;gl_Position.y*=oj_flip;}\n";
    else {
        source += "\n" + legacyInputs + "\nvoid oj_initialize(){" + legacyInit + "}\n";
        source += R"(
uniform vec3 oj_alpha;
void main(){oj_initialize();oj_main();if(oj_alpha.x>0.0){float a=_gl_FragData_0.a,r=oj_alpha.y;int f=int(oj_alpha.z);bool ok=f==1?false:f==2?a<r:f==3?a==r:f==4?a<=r:f==5?a>r:f==6?a!=r:f==7?a>=r:true;if(!ok)discard;}}
)";
    }
    try {
        id = compile(vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER, source);
    } catch (...) {
        MOJOSHADER_freeParseData(parsed);
        parsed = nullptr;
        throw;
    }
}
Shader &Shader::forCubeSamplers(uint32_t mask) {
    mask &= samplerMask;
    if (legacyCode.empty() || !mask)
        return *this;
    auto &variant = variants[mask];
    if (!variant)
        variant = std::make_unique<Shader>(legacyCode.data(), mask);
    return *variant;
}
Shader::~Shader() {
    if (id)
        glDeleteShader(id);
    if (parsed)
        MOJOSHADER_freeParseData(parsed);
}
int Shader::attribute(GLuint program, uint8_t usage, uint8_t index) const {
    for (int i = 0; i < parsed->attribute_count; ++i) {
        auto &a = parsed->attributes[i];
        if (a.usage == usage && a.index == index)
            return glGetAttribLocation(program, a.name);
    }
    return -1;
}
void Shader::uniforms(GLuint program, const Registers &r) const {
    std::vector<float> floats;
    std::vector<int> ints, bools;
    for (int i = 0; i < parsed->uniform_count; ++i) {
        auto &u = parsed->uniforms[i];
        int count = std::max(u.array_count, 1);
        if (u.constant) {
            std::vector<float> values(count * 4);
            for (int j = 0; j < parsed->constant_count; ++j) {
                auto &c = parsed->constants[j];
                if (c.type == MOJOSHADER_UNIFORM_FLOAT && c.index >= u.index && c.index < u.index + count)
                    memcpy(values.data() + (c.index - u.index) * 4, c.value.f, 16);
            }
            glUniform4fv(glGetUniformLocation(program, u.name), count, values.data());
        } else if (u.type == MOJOSHADER_UNIFORM_FLOAT) {
            if (u.index < 0 || size_t(u.index + count) * 4 > r.floats.size())
                throw std::runtime_error("Float registers out of range");
            floats.insert(floats.end(), r.floats.begin() + u.index * 4,
                          r.floats.begin() + (u.index + count) * 4);
        } else if (u.type == MOJOSHADER_UNIFORM_INT) {
            if (u.index < 0 || size_t(u.index + count) * 4 > r.ints.size())
                throw std::runtime_error("Integer registers out of range");
            ints.insert(ints.end(), r.ints.begin() + u.index * 4, r.ints.begin() + (u.index + count) * 4);
        } else {
            if (u.index < 0 || size_t(u.index + count) > r.bools.size())
                throw std::runtime_error("Boolean registers out of range");
            bools.insert(bools.end(), r.bools.begin() + u.index, r.bools.begin() + u.index + count);
        }
    }
    const std::string prefix = parsed->shader_type == MOJOSHADER_TYPE_VERTEX ? "vs" : "ps";
    if (!floats.empty())
        glUniform4fv(glGetUniformLocation(program, (prefix + "_uniforms_vec4").c_str()), floats.size() / 4,
                     floats.data());
    if (!ints.empty())
        glUniform4iv(glGetUniformLocation(program, (prefix + "_uniforms_ivec4").c_str()), ints.size() / 4,
                     ints.data());
    if (!bools.empty())
        glUniform1iv(glGetUniformLocation(program, (prefix + "_uniforms_bool").c_str()), bools.size(),
                     bools.data());
    for (int i = 0; i < parsed->sampler_count; ++i) {
        auto &s = parsed->samplers[i];
        glUniform1i(glGetUniformLocation(program, s.name), s.index);
    }
}
} // namespace oj::gles
