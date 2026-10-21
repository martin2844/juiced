#include "Mac/Shader.h"
#include <algorithm>
#include <cstring>
#include <regex>
#include <stdexcept>
#include <vector>
namespace oj::mac {
Shader::Shader(SDL_GPUDevice *d, const uint32_t *code) : device(d) {
    // TEXCOORD in ps_1_1 saturates the interpolated texture register. Lower it
    // to MOV_SAT before translation; the other instructions retain their order.
    std::vector<uint32_t> lowered;
    const uint32_t *input = code;
    auto original = MOJOSHADER_parse("bytecode", "oj_shader", (const unsigned char *)code, 0, nullptr, 0, nullptr, 0,
                                     nullptr, nullptr, nullptr);
    if (original->error_count) {
        std::string error = original->errors[0].error;
        MOJOSHADER_freeParseData(original);
        throw std::runtime_error(error);
    }
    if (code[0] == 0xffff0101) {
        size_t words = original->output_len / 4;
        for (size_t i = 0; i < words; i++) {
            uint32_t token = code[i];
            if ((token & 0xffff) == 0xfffe) {
                size_t count = (token >> 16) & 0x7fff;
                lowered.insert(lowered.end(), code + i, code + i + count + 1);
                i += count;
                continue;
            }
            if (token == 64 && i + 1 < words) {
                uint32_t dst = code[++i];
                lowered.push_back(1);
                lowered.push_back(dst | 0x00100000);
                lowered.push_back((dst & 0xf0001fff) | 0x00e40000);
            } else
                lowered.push_back(token);
        }
        input = lowered.data();
    }
    bool vertex = original->shader_type == MOJOSHADER_TYPE_VERTEX;
    MOJOSHADER_freeParseData(original);
    MOJOSHADER_swizzle swizzles[2] = {{MOJOSHADER_USAGE_COLOR, 0, {2, 1, 0, 3}},
                                      {MOJOSHADER_USAGE_COLOR, 1, {2, 1, 0, 3}}};
    parsed = MOJOSHADER_parse("metal", "oj_shader", (const unsigned char *)input, 0, vertex ? swizzles : nullptr,
                              vertex ? 2 : 0, nullptr, 0, nullptr, nullptr, nullptr);
    if (parsed->error_count) {
        std::string error = parsed->errors[0].error;
        MOJOSHADER_freeParseData(parsed);
        parsed = nullptr;
        throw std::runtime_error(error);
    }
    std::string source(parsed->output, parsed->output_len);
    // MojoShader emits the ps_1_x texture register initializers but omits the
    // stage-input declarations. Supply the corresponding D3D texcoord semantics.
    source = "#include <metal_stdlib>\n" + source;
    bool needsInput = source.find("input.t") != std::string::npos;
    auto pos = source.find("struct oj_shader_Input");
    if (needsInput && pos == std::string::npos) {
        pos = source.find("struct oj_shader_Output");
        source.insert(pos, "struct oj_shader_Input\n{\n};\n\n");
    }
    if (pos != std::string::npos) {
        auto end = source.find("};", pos);
        for (int i = 0; i < 8; i++) {
            auto name = "t" + std::to_string(i);
            if (source.find("input." + name) != std::string::npos &&
                source.substr(pos, end - pos).find(" " + name + " [[") == std::string::npos) {
                auto field = "\tfloat4 " + name + " [[user(texcoord" + std::to_string(i) + ")]];\n";
                source.insert(end, field);
                end += field.size();
            }
        }
    }
    if (needsInput && source.find("input [[stage_in]]") == std::string::npos) {
        auto fn = source.find("fragment ");
        auto close = source.find(") {", fn);
        auto last = source.find_last_not_of(" \t\r\n", close - 1);
        std::string arg = (source[last] == ',' || source[last] == '(') ? "" : ",";
        arg += "\n\toj_shader_Input input [[stage_in]]\n";
        source.insert(close, arg);
    }

    if (parsed->shader_type == MOJOSHADER_TYPE_PIXEL) {
        auto fn = source.find("fragment ");
        auto close = source.find(") {", fn);
        auto last = source.find_last_not_of(" \t\r\n", close - 1);
        std::string arg = (source[last] == ',' || source[last] == '(') ? "" : ",";
        arg += "\nconstant float4& oj_alpha [[buffer(1)]]\n";
        source.insert(close, arg);
        auto ret = source.rfind("return output;");
        source.insert(
            ret,
            R"(if(oj_alpha.x>0){float a=output.oC0.a;float r=oj_alpha.y;int f=int(oj_alpha.z);bool ok=f==1?false:f==2?a<r:f==3?a==r:f==4?a<=r:f==5?a>r:f==6?a!=r:f==7?a>=r:true;if(!ok)discard_fragment();}
)");
    }
    for (int i = 0; i < parsed->sampler_count; i++)
        slots = std::max(slots, (unsigned)parsed->samplers[i].index + 1);
    SDL_GPUShaderCreateInfo ci{};
    ci.code = (const Uint8 *)source.data();
    ci.code_size = source.size();
    ci.entrypoint = parsed->mainfn;
    ci.format = SDL_GPU_SHADERFORMAT_MSL;
    ci.stage =
        parsed->shader_type == MOJOSHADER_TYPE_VERTEX ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    ci.num_samplers = slots;
    ci.num_uniform_buffers = parsed->shader_type == MOJOSHADER_TYPE_PIXEL ? 2 : (parsed->uniform_count ? 1 : 0);
    gpu = SDL_CreateGPUShader(device, &ci);
    if (!gpu) {
        std::string error = SDL_GetError();
        MOJOSHADER_freeParseData(parsed);
        parsed = nullptr;
        throw std::runtime_error(error);
    }
}
Shader::~Shader() {
    if (gpu)
        SDL_ReleaseGPUShader(device, gpu);
    if (parsed)
        MOJOSHADER_freeParseData(parsed);
}
int Shader::attribute(MOJOSHADER_usage usage, int index) const {
    for (int i = 0; i < parsed->attribute_count; i++)
        if (parsed->attributes[i].usage == usage && parsed->attributes[i].index == index) {
            const char *name = parsed->attributes[i].name;
            const char *digits = name + strlen(name);
            while (digits > name && digits[-1] >= '0' && digits[-1] <= '9')
                --digits;
            return digits[0] ? atoi(digits) : i;
        }
    return -1;
}
void Shader::uniforms(SDL_GPUCommandBuffer *cb, const Registers &r) const {
    std::vector<uint32_t> bytes;
    for (int i = 0; i < parsed->uniform_count; i++) {
        auto &u = parsed->uniforms[i];
        int count = std::max(u.array_count, 1);
        size_t start = bytes.size();
        bytes.resize(start + count * 4);
        if (u.type == MOJOSHADER_UNIFORM_FLOAT)
            memcpy(bytes.data() + start, r.floats.data() + u.index * 4, count * 16);
        else if (u.type == MOJOSHADER_UNIFORM_INT)
            memcpy(bytes.data() + start, r.ints.data() + u.index * 4, count * 16);
        else
            for (int j = 0; j < count; j++)
                bytes[start + j * 4] = r.bools[u.index + j] != 0;
    }
    if (bytes.empty())
        return;
    if (parsed->shader_type == MOJOSHADER_TYPE_VERTEX)
        SDL_PushGPUVertexUniformData(cb, 0, bytes.data(), bytes.size() * 4);
    else
        SDL_PushGPUFragmentUniformData(cb, 0, bytes.data(), bytes.size() * 4);
}
} // namespace oj::mac
