#include "GlesDevice.h"
#include "Core/Base/Log.h"
#include "Core/Formats/Ddt.h"
#include "GlShader.h"
#include "Render/HlslCompiler.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
namespace oj::render {
namespace {
std::array<float, 16> identity() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
std::array<float, 16> multiply(const std::array<float, 16> &a, const std::array<float, 16> &b) {
    std::array<float, 16> c{};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                c[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
    return c;
}
uint32_t vertexCount(uint32_t t, uint32_t n) {
    return t == 1 ? n : t == 2 ? n * 2 : t == 3 ? n + 1 : t == 4 ? n * 3 : n + 2;
}
GLenum primitive(uint32_t t) {
    static GLenum v[] = {GL_TRIANGLES, GL_POINTS,         GL_LINES,       GL_LINE_STRIP,
                         GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
    return t < 7 ? v[t] : GL_TRIANGLES;
}
GLenum blend(uint32_t t) {
    static GLenum v[] = {GL_ZERO,
                         GL_ZERO,
                         GL_ONE,
                         GL_SRC_COLOR,
                         GL_ONE_MINUS_SRC_COLOR,
                         GL_SRC_ALPHA,
                         GL_ONE_MINUS_SRC_ALPHA,
                         GL_DST_ALPHA,
                         GL_ONE_MINUS_DST_ALPHA,
                         GL_DST_COLOR,
                         GL_ONE_MINUS_DST_COLOR,
                         GL_SRC_ALPHA_SATURATE};
    return t < 12 ? v[t] : GL_ONE;
}
GLenum compare(uint32_t t) {
    static GLenum v[] = {GL_ALWAYS,  GL_NEVER,    GL_LESS,   GL_EQUAL, GL_LEQUAL,
                         GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};
    return t < 9 ? v[t] : GL_ALWAYS;
}
GLenum stencil(uint32_t t) {
    static GLenum v[] = {GL_KEEP, GL_KEEP,   GL_ZERO,      GL_REPLACE,  GL_INCR,
                         GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP};
    return t < 9 ? v[t] : GL_KEEP;
}
void enabled(GLenum e, bool on) {
    if (on)
        glEnable(e);
    else
        glDisable(e);
}
struct FixedVertex {
    float pos[4];
    float uv[8][4];
    uint32_t color = 0xffffffff;
};
const char *fixedVertex = R"(#version 300 es
precision highp float;
in vec4 oj_pos;in vec4 oj_color;
in vec4 oj_uv0;in vec4 oj_uv1;in vec4 oj_uv2;in vec4 oj_uv3;
in vec4 oj_uv4;in vec4 oj_uv5;in vec4 oj_uv6;in vec4 oj_uv7;
out highp vec4 io_5_0;out highp vec4 io_5_1;out highp vec4 io_5_2;out highp vec4 io_5_3;
out highp vec4 io_5_4;out highp vec4 io_5_5;out highp vec4 io_5_6;out highp vec4 io_5_7;
out highp vec4 io_10_0;out highp vec4 io_10_1;
uniform mat4 oj_mvp;uniform float oj_flip;
void main(){gl_Position=oj_mvp*oj_pos;gl_Position.z=2.0*gl_Position.z-gl_Position.w;gl_Position.y*=oj_flip;
io_5_0=oj_uv0;io_5_1=oj_uv1;io_5_2=oj_uv2;io_5_3=oj_uv3;io_5_4=oj_uv4;io_5_5=oj_uv5;io_5_6=oj_uv6;io_5_7=oj_uv7;io_10_0=oj_color.bgra;io_10_1=vec4(0);})";
} // namespace
struct GlesDevice::Impl {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    uint32_t w = 1280, h = 720, next = 1, target = 0, face = 0, vs = 0, ps = 0, decl = 0, stream = 0, indices = 0,
             stride = 0, fvf = 0;
    GLuint vao = 0, fixedVS = 0, upBuffer = 0;
    TextureId white = 0, backbuffer = 0;
    Viewport viewport{};
    bool clipped = false;
    ClipRect clip{};
    std::array<uint32_t, 256> states{};
    std::array<std::array<uint32_t, 32>, 16> sampling{}, stages{};
    std::array<uint32_t, 16> bound{};
    std::map<uint32_t, std::array<float, 16>> transforms;
    struct Texture {
        GLuint gl = 0, fbo = 0, depth = 0;
        uint32_t w = 0, h = 0, levels = 1;
        bool cube = false;
        std::vector<uint8_t> cpu;
    };
    struct Buffer {
        GLuint gl = 0;
        uint32_t size = 0;
        bool index = false;
    };
    std::map<uint32_t, Texture> textures;
    std::map<uint32_t, Buffer> buffers;
    std::map<uint32_t, std::unique_ptr<gles::Shader>> shaders;
    std::map<uint32_t, std::vector<VertexElement>> declarations;
    std::map<uint32_t, GLuint> fixedFragments;
    std::map<std::pair<GLuint, GLuint>, GLuint> programs;
    gles::Registers vertexRegisters, pixelRegisters;
    uint32_t tw() const {
        return target ? textures.at(target).w : w;
    }
    uint32_t th() const {
        return target ? textures.at(target).h : h;
    }
    void framebuffer() {
        if (!target) {
            glBindFramebuffer(GL_FRAMEBUFFER, backbuffer ? textures.at(backbuffer).fbo : 0);
            return;
        }
        auto &t = textures.at(target);
        glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               t.cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : GL_TEXTURE_2D, t.gl, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Incomplete render target");
    }
    GLuint fixedFragment(GLuint vertex, bool simple) {
        const std::string source = vertex == fixedVS ? fixedVertex : shaders.at(vs)->source;
        bool color = source.find("io_10_0") != std::string::npos, tex = source.find("io_5_0") != std::string::npos;
        unsigned key = (color ? 1 : 0) | (tex ? 2 : 0) | (simple ? 4 : 0);
        if (fixedFragments.contains(key))
            return fixedFragments.at(key);
        std::string s = "#version 300 es\nprecision highp float;\n";
        if (color)
            s += "in highp vec4 io_10_0;\n";
        if (tex)
            s += "in highp vec4 io_5_0;\n";
        s +=
            R"(uniform sampler2D oj_texture;uniform vec4 oj_factor;uniform ivec4 oj_ops;uniform ivec4 oj_args;uniform vec3 oj_alpha;
layout(location=0)out vec4 color;
vec4 arg(int a,vec4 d,vec4 t){vec4 v=(a&15)==2?t:(a&15)==3?oj_factor:d;if((a&32)!=0)v=v.aaaa;return (a&16)!=0?1.0-v:v;}
vec4 op(int o,vec4 a,vec4 b){if(o==2)return a;if(o==3)return b;if(o==4)return a*b;if(o==5)return 2.0*a*b;if(o==6)return 4.0*a*b;if(o==7)return a+b;if(o==8)return a+b-.5;if(o==9)return 2.0*(a+b-.5);if(o==10)return a-b;if(o==11)return a+b-a*b;return a*b;}
void main(){)";
        s += "vec4 d=" + std::string(color ? "io_10_0" : "vec4(1)") + ",t=texture(oj_texture," +
             std::string(tex ? "io_5_0.xy" : "vec2(0)") + ");\n";
        if (simple)
            s += "color=d*t;";
        else
            s += "color=oj_ops.x==1?d:op(oj_ops.x,arg(oj_args.x,d,t),arg(oj_args.y,d,t));if(oj_ops.x!=1)color.a=op(oj_"
                 "ops.y,arg(oj_args.z,d,t),arg(oj_args.w,d,t)).a;";
        s +=
            R"(color=clamp(color,0.0,1.0);if(oj_alpha.x>0.0){float a=color.a,r=oj_alpha.y;int f=int(oj_alpha.z);bool ok=f==1?false:f==2?a<r:f==3?a==r:f==4?a<=r:f==5?a>r:f==6?a!=r:f==7?a>=r:true;if(!ok)discard;}})";
        auto shader = gles::compile(GL_FRAGMENT_SHADER, s);
        fixedFragments[key] = shader;
        return shader;
    }
    GLuint prepare(bool fixed, bool simple = false) {
        framebuffer();
        glBindVertexArray(vao);
        GLuint v = fixed ? fixedVS : shaders.at(vs)->id;
        GLuint f = (!simple && ps) ? shaders.at(ps)->id : fixedFragment(v, simple);
        auto pair = std::make_pair(v, f);
        if (!programs.contains(pair))
            programs[pair] = gles::link(v, f);
        auto program = programs.at(pair);
        glUseProgram(program);
        if (!fixed)
            shaders.at(vs)->uniforms(program, vertexRegisters);
        if (ps && !simple)
            shaders.at(ps)->uniforms(program, pixelRegisters);
        glUniform1f(glGetUniformLocation(program, "oj_flip"), target ? -1.f : 1.f);
        glUniform3f(glGetUniformLocation(program, "oj_alpha"), states[15], states[24] / 255.f, states[25]);
        glUniform1i(glGetUniformLocation(program, "oj_texture"), 0);
        uint32_t c = states[60];
        glUniform4f(glGetUniformLocation(program, "oj_factor"), ((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f,
                    (c & 255) / 255.f, (c >> 24) / 255.f);
        glUniform4i(glGetUniformLocation(program, "oj_ops"), stages[0][1], stages[0][4], 0, 0);
        glUniform4i(glGetUniformLocation(program, "oj_args"), stages[0][2], stages[0][3], stages[0][5], stages[0][6]);
        glViewport(viewport.x, target ? viewport.y : th() - viewport.y - viewport.height, viewport.width,
                   viewport.height);
#ifdef __ANDROID__
        glDepthRangef(viewport.minZ, viewport.maxZ);
#else
        glDepthRange(viewport.minZ, viewport.maxZ);
#endif
        enabled(GL_DEPTH_TEST, states[7]);
        glDepthMask(states[14] ? GL_TRUE : GL_FALSE);
        glDepthFunc(compare(states[23]));
        enabled(GL_BLEND, states[27]);
        glBlendFuncSeparate(blend(states[19]), blend(states[20]), states[206] ? blend(states[207]) : blend(states[19]),
                            states[206] ? blend(states[208]) : blend(states[20]));
        static GLenum blendOps[] = {GL_FUNC_ADD, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT,
                                    GL_MIN,      GL_MAX};
        glBlendEquationSeparate(blendOps[std::min(states[171], 5u)],
                                blendOps[std::min(states[206] ? states[209] : states[171], 5u)]);
        enabled(GL_CULL_FACE, states[22] != 1);
        glCullFace(GL_BACK);
        bool ccw = states[22] == 2;
        if (target)
            ccw = !ccw;
        glFrontFace(ccw ? GL_CCW : GL_CW);
        enabled(GL_STENCIL_TEST, states[52]);
        glStencilMask(states[59]);
        glStencilFunc(compare(states[56]), states[57], states[58]);
        glStencilOp(stencil(states[53]), stencil(states[54]), stencil(states[55]));
        glColorMask(states[168] & 1, states[168] & 2, states[168] & 4, states[168] & 8);
        enabled(GL_SCISSOR_TEST, clipped);
        if (clipped)
            glScissor(std::max(0, (int)clip.x0), target ? (int)clip.y0 : (int)th() - (int)clip.y1,
                      std::max(0, (int)(clip.x1 - clip.x0)), std::max(0, (int)(clip.y1 - clip.y0)));
        float bias = std::bit_cast<float>(states[195]), slope = std::bit_cast<float>(states[175]);
        enabled(GL_POLYGON_OFFSET_FILL, bias != 0 || slope != 0);
        glPolygonOffset(slope, bias * 16777216.f);
        for (unsigned i = 0; i < 16; i++) {
            auto id = textures.contains(bound[i]) ? bound[i] : white;
            auto &t = textures.at(id);
            auto &s = sampling[i];
            glActiveTexture(GL_TEXTURE0 + i);
            GLenum kind = t.cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
            glBindTexture(kind, t.gl);
            auto wrap = [](unsigned n) { return n == 1 ? GL_REPEAT : n == 2 ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE; };
            glTexParameteri(kind, GL_TEXTURE_WRAP_S, wrap(s[1]));
            glTexParameteri(kind, GL_TEXTURE_WRAP_T, wrap(s[2]));
            if (t.cube)
                glTexParameteri(kind, GL_TEXTURE_WRAP_R, wrap(s[3]));
            bool linear = s[6] != 1;
            GLenum min = linear ? GL_LINEAR : GL_NEAREST;
            if (s[7] && t.levels > 1)
                min = s[7] == 1 ? (linear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST)
                                : (linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
            glTexParameteri(kind, GL_TEXTURE_MIN_FILTER, min);
            glTexParameteri(kind, GL_TEXTURE_MAG_FILTER, s[5] == 1 ? GL_NEAREST : GL_LINEAR);
        }
        glActiveTexture(GL_TEXTURE0);
        for (int i = 0; i < 16; i++)
            glDisableVertexAttribArray(i);
        return program;
    }
    void attribute(GLint location, int count, GLenum type, bool norm, uint32_t bytes, size_t offset) {
        if (location < 0)
            return;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, count, type, norm, bytes, (const void *)offset);
    }
    void declaration(GLuint program) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers.at(stream).gl);
        for (auto &e : declarations.at(decl)) {
            int loc = shaders.at(vs)->attribute(program, e.usage, e.usageIndex);
            if (loc < 0)
                continue;
            int count = 4;
            GLenum type = GL_FLOAT;
            bool norm = false;
            if (e.type <= 3)
                count = e.type + 1;
            else if (e.type == 4 || e.type == 5 || e.type == 8) {
                type = GL_UNSIGNED_BYTE;
                norm = e.type != 5;
            } else if (e.type == 6 || e.type == 7 || e.type == 9 || e.type == 10) {
                type = GL_SHORT;
                count = (e.type == 6 || e.type == 9) ? 2 : 4;
                norm = e.type >= 9;
            } else if (e.type == 11 || e.type == 12) {
                type = GL_UNSIGNED_SHORT;
                norm = true;
                count = e.type == 11 ? 2 : 4;
            } else if (e.type == 15 || e.type == 16) {
                type = GL_HALF_FLOAT;
                count = e.type == 15 ? 2 : 4;
            } else
                throw std::runtime_error("Unsupported vertex element " + std::to_string(e.type));
            attribute(loc, count, type, norm, stride, e.offset);
        }
    }
    void fixedDraw(const FixedVertex *data, size_t count, uint32_t type, const std::array<float, 16> &mvp, bool overlay,
                   bool simple) {
        auto z = states[7], zw = states[14], cull = states[22];
        if (overlay) {
            states[7] = 0;
            states[14] = 0;
            states[22] = 1;
        }
        GLuint program = prepare(true, simple);
        states[7] = z;
        states[14] = zw;
        states[22] = cull;
        glBindBuffer(GL_ARRAY_BUFFER, upBuffer);
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(FixedVertex), data, GL_STREAM_DRAW);
        glUniformMatrix4fv(glGetUniformLocation(program, "oj_mvp"), 1, GL_FALSE, mvp.data());
        attribute(glGetAttribLocation(program, "oj_pos"), 4, GL_FLOAT, false, sizeof(FixedVertex),
                  offsetof(FixedVertex, pos));
        attribute(glGetAttribLocation(program, "oj_color"), 4, GL_UNSIGNED_BYTE, true, sizeof(FixedVertex),
                  offsetof(FixedVertex, color));
        for (int i = 0; i < 8; i++)
            attribute(glGetAttribLocation(program, ("oj_uv" + std::to_string(i)).c_str()), 4, GL_FLOAT, false,
                      sizeof(FixedVertex), offsetof(FixedVertex, uv) + i * 16);
        glDrawArrays(primitive(type), 0, count);
    }
};
GlesDevice::GlesDevice(const DeviceDesc &desc) : p(std::make_unique<Impl>()) {
    p->window = reinterpret_cast<SDL_Window *>(desc.window);
    p->context = SDL_GL_CreateContext(p->window);
    if (!p->context)
        throw std::runtime_error(SDL_GetError());
    SDL_GL_SetSwapInterval(1);
    glGenVertexArrays(1, &p->vao);
    glBindVertexArray(p->vao);
    glGenBuffers(1, &p->upBuffer);
    p->fixedVS = gles::compile(GL_VERTEX_SHADER, fixedVertex);
    p->states[7] = p->states[14] = 1;
    p->states[19] = 5;
    p->states[20] = 6;
    p->states[22] = 3;
    p->states[23] = 4;
    p->states[24] = 0;
    p->states[25] = 8;
    p->states[60] = 0xffffffff;
    p->states[168] = 15;
    p->states[58] = p->states[59] = 255;
    p->stages[0][1] = 4;
    p->stages[0][2] = 2;
    p->stages[0][3] = 0;
    p->stages[0][4] = 2;
    p->stages[0][5] = 2;
    for (auto &s : p->sampling) {
        s[1] = s[2] = s[3] = 1;
        s[5] = s[6] = s[7] = 2;
    }
    p->transforms[2] = p->transforms[3] = p->transforms[256] = identity();
    uint32_t white = 0xffffffff;
    TextureData t{PixelFormat::A8R8G8B8, 1, 1, 1, (uint8_t *)&white, 4};
    p->white = createTexture(t);
    int width, height;
    SDL_GetWindowSizeInPixels(p->window, &width, &height);
    resize(width, height);
    log::info("OpenGL renderer: {}", adapterName());
}
GlesDevice::~GlesDevice() {
    for (auto &[_, program] : p->programs)
        glDeleteProgram(program);
    p->shaders.clear();
    for (auto &[_, shader] : p->fixedFragments)
        glDeleteShader(shader);
    glDeleteShader(p->fixedVS);
    glDeleteVertexArrays(1, &p->vao);
    glDeleteBuffers(1, &p->upBuffer);
    for (auto &[_, b] : p->buffers)
        glDeleteBuffers(1, &b.gl);
    for (auto &[_, t] : p->textures) {
        glDeleteTextures(1, &t.gl);
        if (t.fbo)
            glDeleteFramebuffers(1, &t.fbo);
        if (t.depth)
            glDeleteRenderbuffers(1, &t.depth);
    }
    SDL_GL_DestroyContext(p->context);
}
Backend GlesDevice::backend() const {
    return Backend::GLES;
}
std::string GlesDevice::adapterName() const {
    const auto *s = glGetString(GL_RENDERER);
    return s ? (const char *)s : "OpenGL ES";
}
std::string GlesDevice::description() const {
    return "Native OpenGL ES 3 / MojoShader";
}
bool GlesDevice::beginFrame(const Color &c) {
    int w, h;
    SDL_GetWindowSizeInPixels(p->window, &w, &h);
    if (w <= 0 || h <= 0)
        return false;
    resize(w, h);
    setRenderTarget(0, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glDepthMask(1);
    glStencilMask(255);
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    return true;
}
void GlesDevice::endFrame() {
    int width, height;
    SDL_GetWindowSizeInPixels(p->window, &width, &height);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, p->textures.at(p->backbuffer).fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0, 0, p->w, p->h, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    auto error = glGetError();
    if (error != GL_NO_ERROR) {
        static unsigned reported = 0;
        if (reported++ < 10)
            log::warning("OpenGL frame error: {}", (unsigned)error);
    }
    SDL_GL_SwapWindow(p->window);
}
void GlesDevice::resize(uint32_t width, uint32_t height) {
    if (!width || !height)
        return;
    // Keep the Retroid's internal resolution at 720p while matching the display aspect.
    float scale = std::min({1.f, 1280.f / width, 720.f / height});
    uint32_t w = std::max(1u, (uint32_t)(width * scale)), h = std::max(1u, (uint32_t)(height * scale));
    if (p->backbuffer && p->w == w && p->h == h)
        return;
    auto old = p->backbuffer;
    p->backbuffer = 0;
    if (old)
        destroyTexture(old);
    p->w = w;
    p->h = h;
    auto buffer = createRenderTarget(w, h, false);
    setRenderTarget(buffer, 0);
    p->backbuffer = buffer;
    setRenderTarget(0, 0);
}
uint32_t GlesDevice::width() const {
    return p->w;
}
uint32_t GlesDevice::height() const {
    return p->h;
}
uint32_t GlesDevice::vertexShaderVersion() const {
    return 0xfffe0300;
}
uint32_t GlesDevice::pixelShaderVersion() const {
    return 0xffff0300;
}
AdapterInfo GlesDevice::adapterInfo() const {
    return {};
}
uint32_t GlesDevice::backBufferFormat() const {
    return 21;
}
uint32_t GlesDevice::displayFormat() const {
    return 21;
}
uint32_t GlesDevice::sourceBlendCaps() const {
    return 0x1fff;
}
bool GlesDevice::fullScreenGamma() const {
    return false;
}
void GlesDevice::setGammaRamp(uint32_t, const uint16_t *, const uint16_t *, const uint16_t *) {}
uint32_t GlesDevice::textureCaps() const {
    return 0x10800;
}
bool GlesDevice::textureFormatOk(uint32_t, uint32_t f) const {
    return f == 21 || f == 22 || f == 23 || f == Ddt::kDxt1 || f == Ddt::kDxt3 || f == Ddt::kDxt5;
}
TextureId GlesDevice::createTexture(const TextureData &d) {
    if (!d.width || !d.height || !d.pixels)
        return 0;
    Impl::Texture t;
    t.w = d.width;
    t.h = d.height;
    t.levels = std::max(1u, d.levels);
    glGenTextures(1, &t.gl);
    glBindTexture(GL_TEXTURE_2D, t.gl);
    uint32_t fmt = d.format == PixelFormat::DXT1       ? Ddt::kDxt1
                   : d.format == PixelFormat::DXT3     ? Ddt::kDxt3
                   : d.format == PixelFormat::DXT5     ? Ddt::kDxt5
                   : d.format == PixelFormat::X8R8G8B8 ? 22
                                                       : 21;
    size_t offset = 0;
    for (unsigned l = 0; l < t.levels; l++) {
        unsigned w = std::max(1u, d.width >> l), h = std::max(1u, d.height >> l);
        size_t bytes = d.format == PixelFormat::R5G6B5 ? w * h * 2 : levelBytes(fmt, w, h);
        if (offset + bytes > d.size) {
            glDeleteTextures(1, &t.gl);
            return 0;
        }
        std::vector<uint32_t> pixels;
        if (d.format == PixelFormat::R5G6B5) {
            pixels.resize(w * h);
            for (size_t i = 0; i < pixels.size(); i++) {
                uint16_t v;
                memcpy(&v, d.pixels + offset + i * 2, 2);
                pixels[i] =
                    0xff000000 | ((v >> 11) * 255 / 31 << 16) | (((v >> 5) & 63) * 255 / 63 << 8) | (v & 31) * 255 / 31;
            }
        } else
            pixels = decodeLevel(fmt, w, h, d.pixels + offset);
        for (auto &v : pixels)
            v = (v & 0xff00ff00) | ((v & 255) << 16) | ((v >> 16) & 255);
        glTexImage2D(GL_TEXTURE_2D, l, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        offset += bytes;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, t.levels - 1);
    auto id = p->next++;
    p->textures[id] = std::move(t);
    return id;
}
TextureId GlesDevice::createCubeTexture(const TextureData &d) {
    if (!d.width || d.width != d.height || !d.pixels)
        return 0;
    Impl::Texture t;
    t.w = d.width;
    t.h = d.height;
    t.cube = true;
    t.levels = std::max(1u, d.levels);
    glGenTextures(1, &t.gl);
    glBindTexture(GL_TEXTURE_CUBE_MAP, t.gl);
    uint32_t fmt = d.format == PixelFormat::DXT1   ? Ddt::kDxt1
                   : d.format == PixelFormat::DXT3 ? Ddt::kDxt3
                   : d.format == PixelFormat::DXT5 ? Ddt::kDxt5
                                                   : 21;
    size_t off = 0;
    for (unsigned face = 0; face < 6; face++)
        for (unsigned l = 0; l < t.levels; l++) {
            unsigned w = std::max(1u, d.width >> l), h = w;
            size_t bytes = levelBytes(fmt, w, h);
            if (off + bytes > d.size) {
                glDeleteTextures(1, &t.gl);
                return 0;
            }
            auto pixels = decodeLevel(fmt, w, h, d.pixels + off);
            for (auto &v : pixels)
                v = (v & 0xff00ff00) | ((v & 255) << 16) | ((v >> 16) & 255);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, l, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());
            off += bytes;
        }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, t.levels - 1);
    auto id = p->next++;
    p->textures[id] = std::move(t);
    return id;
}
void GlesDevice::destroyTexture(TextureId id) {
    auto it = p->textures.find(id);
    if (it == p->textures.end())
        return;
    auto &t = it->second;
    glDeleteTextures(1, &t.gl);
    if (t.fbo)
        glDeleteFramebuffers(1, &t.fbo);
    if (t.depth)
        glDeleteRenderbuffers(1, &t.depth);
    p->textures.erase(it);
    for (auto &v : p->bound)
        if (v == id)
            v = 0;
    if (p->target == id)
        setRenderTarget(0, 0);
}
TextureId GlesDevice::createRenderTarget(uint32_t w, uint32_t h, bool cube) {
    if (!w || !h)
        return 0;
    Impl::Texture t;
    t.w = w;
    t.h = h;
    t.cube = cube;
    glGenTextures(1, &t.gl);
    GLenum kind = cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    glBindTexture(kind, t.gl);
    for (unsigned i = 0; i < (cube ? 6u : 1u); i++)
        glTexImage2D(cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + i : GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(kind, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(kind, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(kind, GL_TEXTURE_MAX_LEVEL, 0);
    glGenRenderbuffers(1, &t.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, t.depth);
    auto id = p->next++;
    p->textures[id] = std::move(t);
    p->framebuffer();
    return id;
}
void GlesDevice::setRenderTarget(TextureId id, uint32_t face) {
    p->target = id;
    p->face = face;
    p->framebuffer();
    p->viewport = {0, 0, p->tw(), p->th(), 0, 1};
}
uint32_t GlesDevice::targetWidth(TextureId id) const {
    return id ? p->textures.at(id).w : p->w;
}
uint32_t GlesDevice::targetHeight(TextureId id) const {
    return id ? p->textures.at(id).h : p->h;
}
uint32_t GlesDevice::resets() const {
    return 0;
}
bool GlesDevice::readTarget(TextureId id, std::vector<uint8_t> &pixels, uint32_t *pitch) {
    auto old = p->target, face = p->face;
    auto view = p->viewport;
    setRenderTarget(id, 0);
    unsigned w = p->tw(), h = p->th();
    pixels.resize(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    bool ok = glGetError() == GL_NO_ERROR;
    for (size_t i = 0; i < pixels.size(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
    if (!id)
        for (unsigned y = 0; y < h / 2; y++)
            for (unsigned x = 0; x < w * 4; x++)
                std::swap(pixels[y * w * 4 + x], pixels[(h - 1 - y) * w * 4 + x]);
    if (pitch)
        *pitch = w * 4;
    setRenderTarget(old, face);
    p->viewport = view;
    return ok;
}
TextureId GlesDevice::createDynamicTexture(uint32_t w, uint32_t h, uint32_t) {
    std::vector<uint32_t> data(w * h);
    TextureData d{PixelFormat::A8R8G8B8, w, h, 1, (const uint8_t *)data.data(), data.size() * 4};
    auto id = createTexture(d);
    if (id)
        p->textures.at(id).cpu.resize(w * h * 4);
    return id;
}
bool GlesDevice::lockTexture(TextureId id, void **bits, int32_t *pitch) {
    if (!p->textures.contains(id))
        return false;
    auto &t = p->textures.at(id);
    if (t.cpu.empty())
        return false;
    *bits = t.cpu.data();
    *pitch = t.w * 4;
    return true;
}
void GlesDevice::unlockTexture(TextureId id) {
    auto &t = p->textures.at(id);
    auto data = t.cpu;
    for (size_t i = 0; i < data.size(); i += 4)
        std::swap(data[i], data[i + 2]);
    glBindTexture(GL_TEXTURE_2D, t.gl);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t.w, t.h, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
}
ShaderId GlesDevice::createVertexShader(const uint32_t *code) {
    auto shader = std::make_unique<gles::Shader>(code);
    auto id = p->next++;
    p->shaders[id] = std::move(shader);
    return id;
}
ShaderId GlesDevice::createPixelShader(const uint32_t *code) {
    return createVertexShader(code);
}
DeclarationId GlesDevice::createDeclaration(const VertexElement *e) {
    auto id = p->next++;
    auto &d = p->declarations[id];
    while (e->stream != 0xff && e->type != 17)
        d.push_back(*e++);
    return id;
}
BufferId GlesDevice::createVertexBuffer(const void *data, uint32_t bytes) {
    Impl::Buffer b;
    b.size = bytes;
    glGenBuffers(1, &b.gl);
    glBindBuffer(GL_ARRAY_BUFFER, b.gl);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
    auto id = p->next++;
    p->buffers[id] = b;
    return id;
}
BufferId GlesDevice::createIndexBuffer(const uint16_t *indices, uint32_t count) {
    auto id = createVertexBuffer(indices, count * 2);
    p->buffers.at(id).index = true;
    return id;
}
void GlesDevice::destroyBuffer(BufferId id) {
    auto it = p->buffers.find(id);
    if (it == p->buffers.end())
        return;
    glDeleteBuffers(1, &it->second.gl);
    p->buffers.erase(it);
}
void GlesDevice::updateVertexBuffer(BufferId id, const void *data, uint32_t bytes) {
    auto &b = p->buffers.at(id);
    glBindBuffer(GL_ARRAY_BUFFER, b.gl);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_DYNAMIC_DRAW);
    b.size = bytes;
}
bool GlesDevice::occlusionQueries() const {
    return false;
}
QueryId GlesDevice::createOcclusionQuery() {
    return 0;
}
void GlesDevice::issueQuery(QueryId, bool) {}
int GlesDevice::queryResult(QueryId, bool, uint32_t *) {
    return -1;
}
void GlesDevice::setRenderState(RenderState s, uint32_t v) {
    auto i = (size_t)s;
    if (i < p->states.size())
        p->states[i] = v;
}
void GlesDevice::setTextureStageState(uint32_t stage, TextureStageState s, uint32_t v) {
    if (stage < 16 && (unsigned)s < 32)
        p->stages[stage][(unsigned)s] = v;
}
void GlesDevice::setSamplerState(uint32_t slot, SamplerState s, uint32_t v) {
    if (slot < 16 && (unsigned)s < 32)
        p->sampling[slot][(unsigned)s] = v;
}
void GlesDevice::setTexture(uint32_t slot, TextureId id) {
    if (slot < 16)
        p->bound[slot] = id;
}
void GlesDevice::setTransform(TransformState s, const float *m) {
    std::copy_n(m, 16, p->transforms[(unsigned)s].begin());
}
void GlesDevice::setMaterial(const FixedMaterial &) {}
void GlesDevice::setLight(uint32_t, const FixedLight &) {}
void GlesDevice::lightEnable(uint32_t, bool) {}
void GlesDevice::setNPatchMode(float) {}
void GlesDevice::setFvf(uint32_t v) {
    p->fvf = v;
}
void GlesDevice::setVertexShader(ShaderId id) {
    p->vs = id;
}
void GlesDevice::setPixelShader(ShaderId id) {
    p->ps = id;
}
void GlesDevice::setVertexShaderConstantF(uint32_t start, const float *d, uint32_t n) {
    if ((uint64_t(start) + n) * 4 <= p->vertexRegisters.floats.size())
        std::copy_n(d, n * 4, p->vertexRegisters.floats.begin() + start * 4);
}
void GlesDevice::setPixelShaderConstantF(uint32_t start, const float *d, uint32_t n) {
    if ((uint64_t(start) + n) * 4 <= p->pixelRegisters.floats.size())
        std::copy_n(d, n * 4, p->pixelRegisters.floats.begin() + start * 4);
}
void GlesDevice::setVertexShaderConstantI(uint32_t start, const int32_t *d, uint32_t n) {
    if ((uint64_t(start) + n) * 4 <= p->vertexRegisters.ints.size())
        std::copy_n(d, n * 4, p->vertexRegisters.ints.begin() + start * 4);
}
void GlesDevice::setPixelShaderConstantI(uint32_t start, const int32_t *d, uint32_t n) {
    if ((uint64_t(start) + n) * 4 <= p->pixelRegisters.ints.size())
        std::copy_n(d, n * 4, p->pixelRegisters.ints.begin() + start * 4);
}
void GlesDevice::setVertexShaderConstantB(uint32_t start, const int32_t *d, uint32_t n) {
    if (uint64_t(start) + n <= p->vertexRegisters.bools.size())
        std::copy_n(d, n, p->vertexRegisters.bools.begin() + start);
}
void GlesDevice::setPixelShaderConstantB(uint32_t start, const int32_t *d, uint32_t n) {
    if (uint64_t(start) + n <= p->pixelRegisters.bools.size())
        std::copy_n(d, n, p->pixelRegisters.bools.begin() + start);
}
void GlesDevice::setDeclaration(DeclarationId id) {
    p->decl = id;
}
void GlesDevice::setStream(BufferId id, uint32_t stride) {
    p->stream = id;
    p->stride = stride;
}
void GlesDevice::setIndices(BufferId id) {
    p->indices = id;
}
void GlesDevice::drawIndexed(uint32_t type, uint32_t, uint32_t start, uint32_t count) {
    auto program = p->prepare(false);
    p->declaration(program);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p->buffers.at(p->indices).gl);
    glDrawElements(primitive(type), vertexCount(type, count), GL_UNSIGNED_SHORT, (void *)(uintptr_t(start) * 2));
}
void GlesDevice::draw(uint32_t type, uint32_t start, uint32_t count) {
    auto program = p->prepare(false);
    p->declaration(program);
    glDrawArrays(primitive(type), start, vertexCount(type, count));
}
void GlesDevice::setViewport(const Viewport &v) {
    p->viewport = v;
}
void GlesDevice::clear(uint32_t flags, uint32_t argb, float z, uint32_t st) {
    p->framebuffer();
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glDepthMask(1);
    glStencilMask(255);
    glClearColor(((argb >> 16) & 255) / 255.f, ((argb >> 8) & 255) / 255.f, (argb & 255) / 255.f, (argb >> 24) / 255.f);
#ifdef __ANDROID__
    glClearDepthf(z);
#else
    glClearDepth(z);
#endif
    glClearStencil(st);
    glClear(((flags & 1) ? GL_COLOR_BUFFER_BIT : 0) | ((flags & 2) ? GL_DEPTH_BUFFER_BIT : 0) |
            ((flags & 4) ? GL_STENCIL_BUFFER_BIT : 0));
}
void GlesDevice::setClip(const ClipRect *r) {
    p->clipped = r;
    if (r)
        p->clip = *r;
}
void GlesDevice::drawQuad(const QuadVertex (&q)[4], TextureId id, Blend mode, bool) {
    FixedVertex data[6]{};
    int order[] = {0, 1, 2, 2, 1, 3};
    for (int i = 0; i < 6; i++) {
        auto &a = q[order[i]];
        auto &v = data[i];
        v.pos[0] = (a.x + .5f) / p->w * 2 - 1;
        v.pos[1] = 1 - (a.y + .5f) / p->h * 2;
        v.pos[3] = 1;
        v.uv[0][0] = a.u;
        v.uv[0][1] = a.v;
        v.color = a.argb;
    }
    auto tex = p->bound[0], blendOn = p->states[27], src = p->states[19], dst = p->states[20], alpha = p->states[15];
    p->bound[0] = id;
    p->states[27] = 1;
    p->states[19] = 5;
    p->states[20] = mode == Blend::Additive ? 2 : 6;
    p->states[15] = 0;
    p->fixedDraw(data, 6, 4, identity(), true, true);
    p->bound[0] = tex;
    p->states[27] = blendOn;
    p->states[19] = src;
    p->states[20] = dst;
    p->states[15] = alpha;
}
void GlesDevice::drawDebugTriangles(const DebugVertex *v, uint32_t count) {
    std::vector<FixedVertex> out(count);
    for (unsigned i = 0; i < count; i++) {
        out[i].pos[0] = v[i].x / p->w * 2 - 1;
        out[i].pos[1] = 1 - v[i].y / p->h * 2;
        out[i].pos[3] = 1;
        out[i].color = v[i].argb;
    }
    auto old = p->bound[0];
    p->bound[0] = 0;
    p->fixedDraw(out.data(), count, 4, identity(), true, true);
    p->bound[0] = old;
}
void GlesDevice::drawLine(float x0, float y0, float x1, float y1, uint32_t argb) {
    FixedVertex v[2]{};
    v[0].pos[0] = x0 / p->w * 2 - 1;
    v[0].pos[1] = 1 - y0 / p->h * 2;
    v[1].pos[0] = x1 / p->w * 2 - 1;
    v[1].pos[1] = 1 - y1 / p->h * 2;
    for (auto &a : v) {
        a.pos[3] = 1;
        a.color = argb;
    }
    auto old = p->bound[0];
    p->bound[0] = 0;
    p->fixedDraw(v, 2, 2, identity(), true, true);
    p->bound[0] = old;
}
void GlesDevice::drawUp(uint32_t type, uint32_t count, const void *data, uint32_t stride) {
    auto n = vertexCount(type, count);
    std::vector<FixedVertex> out(n);
    bool screen = (p->fvf & 0xe) == Fvf::XyzRhw;
    for (unsigned i = 0; i < n; i++) {
        const auto *b = (const uint8_t *)data + i * stride;
        auto &v = out[i];
        memcpy(v.pos, b, 12);
        b += 12;
        v.pos[3] = 1;
        if (screen) {
            float rhw;
            memcpy(&rhw, b, 4);
            b += 4;
            v.pos[3] = rhw ? 1 / rhw : 1;
            v.pos[0] = ((v.pos[0] + .5f) / p->tw() * 2 - 1) * v.pos[3];
            v.pos[1] = (1 - (v.pos[1] + .5f) / p->th() * 2) * v.pos[3];
            v.pos[2] *= v.pos[3];
        }
        if (p->fvf & Fvf::Normal)
            b += 12;
        if (p->fvf & Fvf::PointSize)
            b += 4;
        if (p->fvf & Fvf::Diffuse) {
            memcpy(&v.color, b, 4);
            b += 4;
        }
        if (p->fvf & Fvf::Specular)
            b += 4;
        unsigned texCount = std::min((p->fvf >> 8) & 15, 8u);
        for (unsigned j = 0; j < texCount; j++) {
            unsigned dims = (p->fvf >> (16 + j * 2)) & 3;
            unsigned components = dims == 0 ? 2 : dims == 1 ? 3 : dims == 2 ? 4 : 1;
            memcpy(v.uv[j], b, components * 4);
            b += components * 4;
        }
    }
    auto matrix = screen ? identity() : multiply(multiply(p->transforms[256], p->transforms[2]), p->transforms[3]);
    p->fixedDraw(out.data(), n, type, matrix, screen, false);
}
bool GlesDevice::capture(const std::wstring &file) {
    std::vector<uint8_t> pixels;
    uint32_t pitch;
    if (!readTarget(0, pixels, &pitch))
        return false;
    auto *s = SDL_CreateSurfaceFrom(p->w, p->h, SDL_PIXELFORMAT_BGRA32, pixels.data(), pitch);
    if (!s)
        return false;
    bool ok = SDL_SaveBMP(s, std::filesystem::path(file).string().c_str());
    SDL_DestroySurface(s);
    return ok;
}
std::unique_ptr<IRenderDevice> createDevice(Backend, const DeviceDesc &desc, std::string *error) {
    try {
        return std::make_unique<GlesDevice>(desc);
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return {};
    }
}
const char *backendName(Backend) {
    return "OpenGL ES";
}
} // namespace oj::render
namespace oj::render::hlsl {
std::optional<std::vector<uint8_t>> compile(const std::vector<std::byte> &, const char *, const char *, uint32_t,
                                            const IncludeReader &) {
    log::warning("Runtime HLSL compilation unavailable; use compiled game effects");
    return {};
}
} // namespace oj::render::hlsl
