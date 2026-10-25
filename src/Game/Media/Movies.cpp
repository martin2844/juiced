#include "Game/Media/Movies.h"

#include "Audio/SoundSystem.h"

#include "Core/Base/Log.h"
#include "Core/Base/CasePath.h"
#include "Core/Base/Random.h"
#include "Game/Media/BinkLibrary.h"
#include "Render/Renderer3d.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace oj::movie {

using render::BlendFactor;
using render::ColourWrite;
using render::Cull;
using render::Fvf;
using render::RenderState;
using render::SamplerState;
using render::TextureAddress;
using render::TextureArg;
using render::TextureFilter;
using render::TextureOp;
using render::TextureStageState;
using render::TextureTransform;

namespace {

System* g_system = nullptr;

// The BINK structure (binkw32 1.x).
constexpr uint32_t kWidth = 0x00;
constexpr uint32_t kHeight = 0x04;
constexpr uint32_t kFrames = 0x08;
constexpr uint32_t kFrameNum = 0x0C;
constexpr uint32_t kNumTracks = 0xF0;

// BinkOpen's flags of 0x00559B10: BINKFILEHANDLE (the file system's handle) and BINKSNDTRACK.
constexpr uint32_t kOpenFlags = 0x00804000;
// BinkCopyToBufferRect's BINKCOPYALL.
constexpr uint32_t kCopyAll = 0x80000000;

// D3DFORMATs.
constexpr uint32_t kR8G8B8 = 20, kA8R8G8B8 = 21, kX8R8G8B8 = 22, kR5G6B5 = 23, kX1R5G5B5 = 24, kA1R5G5B5 = 25,
                   kA4R4G4B4 = 26;

// The formats 0x0055AB80 tries, in order: format, rad3d index, bytes a pixel, alpha.
struct FormatChoice {
    uint32_t format, index, bytes;
    bool alpha;
};
constexpr FormatChoice kFormats[] = {
    {kX8R8G8B8, 0, 4, false}, {kR8G8B8, 8, 3, false},  {kR5G6B5, 3, 2, false},  {kX1R5G5B5, 2, 2, false},
    {kA8R8G8B8, 1, 4, true},  {kA4R4G4B4, 5, 2, true}, {kA1R5G5B5, 4, 2, true},
};

// D3DPTEXTURECAPS_POW2 and D3DPTEXTURECAPS_NONPOW2CONDITIONAL.
constexpr uint32_t kPow2 = 0x2;
constexpr uint32_t kNonPow2Conditional = 0x100;

// 0x0055AD20: a side made a power of two from 4 to 512 (2 and less stay as they are; past 512, 1024).
uint32_t powerOfTwo(uint32_t v) {
    if (v <= 2) return v;
    for (uint32_t p = 4; p <= 512; p *= 2) {
        if (v <= p) return p;
    }
    return 1024;
}

// 0x00424230: cut toward zero (the x87's chop, or cvtss2si under the game's MXCSR).
int32_t chop(float v) {
    if (!(v > -2147483648.0f && v < 2147483648.0f)) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(v);
}

// An unsigned whole number as the x87 loads it (fild, plus 2^32 when its top bit is set).
float asFloat(uint32_t v) {
    return static_cast<float>(static_cast<double>(v));
}

}  // namespace

// The rad3d object (0x28 bytes; [0x0075F1D8], [0x0075F1E0]): +0x00 the IDirect3D9 (the renderer's +0x7CC), +0x04
// the device (+0x7D0), +0x08 [0x0075F79C]+0x0C (read by nothing of the sample).
struct System::Rad3d {
    uint32_t alphaFormat = 0;    // +0x0C
    uint32_t format = 0;         // +0x10
    uint32_t alphaBytes = 0;     // +0x14
    uint32_t bytes = 0;          // +0x18
    uint32_t alphaSurface = 0;   // +0x1C: the rad3d index of the format
    uint32_t surface = 0;        // +0x20
    bool anySize = true;         // +0x24: textures of any size (not POW2, or POW2 with NONPOW2CONDITIONAL)
};

// An image (0x50 bytes, its tiles' textures after it): one texture (0x0055B6A0, +0x20 1) or tiles (0x0055B060).
struct System::Image {
    uint32_t format = 0;     // +0x04
    uint32_t surface = 0;    // +0x08
    bool alpha = false;      // +0x0C
    uint32_t bytes = 0;      // +0x10
    uint32_t width = 0;      // +0x14
    uint32_t height = 0;     // +0x18
    uint32_t next = 0;       // +0x1C: the lock's place (the single texture: locked or not; tiles: the next tile)
    bool single = false;     // +0x20
    uint32_t rowBytes = 0;   // +0x24: width x bytes rounded up to 16 (read by nothing)
    uint32_t tiles = 0;      // +0x28
    uint32_t columns = 0;    // +0x2C: whole tiles across
    uint32_t rows = 0;       // +0x30: whole tiles down
    uint32_t lastWidth = 0;  // +0x34: the right column's width, the rest across made a power of two (0: none)
    uint32_t lastHeight = 0; // +0x38: the bottom row's height
    uint32_t restWidth = 0;  // +0x3C: the rest across
    uint32_t restHeight = 0; // +0x40
    uint32_t tile = 0;       // +0x44
    std::vector<render::TextureId> textures;  // +0x48: row by row, the right column's after each row's
    render::TextureId texture = 0;            // +0x4C
};

System::System(render::IRenderDevice& device, Game game) : m_device(device), m_game(std::move(game)) {
    g_system = this;
}

System::~System() {
    if (current != nullptr) closeCurrent();
    for (auto& [name, player] : m_players) delete player;
    m_players.clear();
    freeImage(m_image);
    if (g_system == this) g_system = nullptr;
}

System* System::get() {
    return g_system;
}

// 0x0055B7C0: the display mode's format (IDirect3D9::GetAdapterDisplayMode) picks the opaque format, the first of
// kFormats the device takes as a texture, and the alpha one, the first of the alpha formats (0x0055AB80); none,
// no rad3d. The texture caps give +0x24. BinkSetSoundSystem(BinkOpenDirectSound, the game's DirectSound
// [0x0075F858]).
bool System::startRad3d() {
    if (m_library == nullptr) {
        m_library = bink::Library::get(m_game.folder);
        if (m_library == nullptr) return false;
    }
    auto r = std::make_unique<Rad3d>();
    const uint32_t display = m_device.displayFormat();
    if (display == 0) return false;
    bool opaque = false;
    for (const FormatChoice& f : kFormats) {
        if (m_device.textureFormatOk(display, f.format)) {
            r->format = f.format;
            r->surface = f.index;
            r->bytes = f.bytes;
            opaque = true;
            break;
        }
    }
    if (!opaque) return false;
    bool alpha = false;
    for (const FormatChoice& f : kFormats) {
        if (f.alpha && m_device.textureFormatOk(display, f.format)) {
            r->alphaFormat = f.format;
            r->alphaSurface = f.index;
            r->alphaBytes = f.bytes;
            alpha = true;
            break;
        }
    }
    if (!alpha) return false;
    const uint32_t caps = m_device.textureCaps();
    r->anySize = !((caps & kPow2) != 0 && (caps & kNonPow2Conditional) == 0);
    m_library->setSoundSystem(reinterpret_cast<uintptr_t>(audio::system().device()));
    m_rad3d = std::move(r);
    return true;
}

// 0x0055AD80: CreateTexture(w, h, 1, D3DUSAGE_DYNAMIC, format, D3DPOOL_DEFAULT), else (w, h, 1, 0, format,
// D3DPOOL_MANAGED); locked, each row's w x bytes zeroed.
render::TextureId System::makeTexture(uint32_t width, uint32_t height, uint32_t format, uint32_t bytes) {
    const render::TextureId t = m_device.createDynamicTexture(width, height, format);
    if (t == 0) return 0;
    void* bits = nullptr;
    int32_t pitch = 0;
    if (m_device.lockTexture(t, &bits, &pitch)) {
        auto* row = static_cast<uint8_t*>(bits);
        for (uint32_t y = 0; y < height; ++y, row += pitch) std::memset(row, 0, static_cast<size_t>(bytes) * width);
        m_device.unlockTexture(t);
    }
    return t;
}

// 0x0055B6A0: one texture of the size asked for.
std::unique_ptr<System::Image> System::makeSingle(uint32_t width, uint32_t height, bool alpha) {
    auto image = std::make_unique<Image>();
    const Rad3d& r = *m_rad3d;
    image->bytes = alpha ? r.alphaBytes : r.bytes;
    image->width = width;
    image->height = height;
    image->alpha = alpha;
    image->surface = alpha ? r.alphaSurface : r.surface;
    image->format = alpha ? r.alphaFormat : r.format;
    image->next = 0;
    image->texture = makeTexture(width, height, image->format, image->bytes);
    if (image->texture == 0) return nullptr;
    image->single = true;
    return image;
}

// 0x0055B060: tiles of tile x tile, the rest across and down in tiles whose sides are made powers of two; all of
// them or none.
std::unique_ptr<System::Image> System::makeTiles(uint32_t width, uint32_t height, bool alpha, uint32_t tile) {
    const Rad3d& r = *m_rad3d;
    auto image = std::make_unique<Image>();
    const uint32_t bytes = alpha ? r.alphaBytes : r.bytes;
    image->restWidth = width % tile;
    image->restHeight = height % tile;
    image->lastWidth = powerOfTwo(image->restWidth);
    image->lastHeight = powerOfTwo(image->restHeight);
    image->columns = width / tile;
    image->rows = height / tile;
    image->tiles = ((width + tile - 1) / tile) * ((height + tile - 1) / tile);
    image->rowBytes = (bytes * width + 15) & ~15u;
    image->height = height;
    image->width = width;
    image->bytes = bytes;
    image->alpha = alpha;
    image->tile = tile;
    image->surface = alpha ? r.alphaSurface : r.surface;
    image->format = alpha ? r.alphaFormat : r.format;
    for (uint32_t row = 0; row < image->rows; ++row) {
        for (uint32_t c = 0; c < image->columns; ++c) {
            image->textures.push_back(makeTexture(tile, tile, image->format, image->bytes));
        }
        if (image->lastWidth != 0) {
            image->textures.push_back(makeTexture(image->lastWidth, tile, image->format, image->bytes));
        }
    }
    if (image->lastHeight != 0) {
        for (uint32_t c = 0; c < image->columns; ++c) {
            image->textures.push_back(makeTexture(tile, image->lastHeight, image->format, image->bytes));
        }
        if (image->lastWidth != 0) {
            image->textures.push_back(makeTexture(image->lastWidth, image->lastHeight, image->format, image->bytes));
        }
    }
    image->textures.resize(image->tiles);
    for (render::TextureId t : image->textures) {
        if (t == 0) {
            for (render::TextureId& made : image->textures) {
                if (made != 0) m_device.destroyTexture(made);
                made = 0;
            }
            return nullptr;
        }
    }
    image->single = false;
    return image;
}

// 0x0055B740: one texture when both sides are powers of two or the device takes any size, else tiles.
std::unique_ptr<System::Image> System::makeImage(uint32_t width, uint32_t height, bool alpha) {
    if ((powerOfTwo(width) == width && powerOfTwo(height) == height) || m_rad3d->anySize) {
        return makeSingle(width, height, alpha);
    }
    return makeTiles(width, height, alpha, 0x100);
}

// 0x0055B790 / 0x0055B2B0.
void System::freeImage(std::unique_ptr<Image>& image) {
    if (!image) return;
    if (image->single) {
        if (image->texture != 0) m_device.destroyTexture(image->texture);
    } else {
        for (render::TextureId t : image->textures) {
            if (t != 0) m_device.destroyTexture(t);
        }
    }
    image.reset();
}

// 0x0055B5F0 (one texture: the whole of it once, then no more) and 0x0055B2F0 (the tiles one by one, their place
// in the frame and their size; the right column's and the bottom row's the power-of-two sides).
bool System::lock(void** bits, int32_t* pitch, uint32_t* surface, uint32_t* x, uint32_t* y, uint32_t* w,
                  uint32_t* h) {
    Image& image = *m_image;
    if (image.single) {
        if (image.next != 0) {
            image.next = 0;
            return false;
        }
        if (!m_device.lockTexture(image.texture, bits, pitch)) return false;
        *x = 0;
        *y = 0;
        *w = image.width;
        *h = image.height;
        *surface = image.surface;
        image.next = 1;
        return true;
    }
    if (image.next >= image.tiles) {
        image.next = 0;
        return false;
    }
    if (!m_device.lockTexture(image.textures[image.next], bits, pitch)) {
        image.next = 0;
        return false;
    }
    *surface = image.surface;
    const uint32_t across = image.columns + (image.lastWidth != 0 ? 1 : 0);
    const uint32_t row = image.next / across;
    const uint32_t column = image.next % across;
    *x = image.tile * column;
    *y = image.tile * row;
    *w = column < image.columns ? image.tile : image.lastWidth;
    *h = row < image.rows ? image.tile : image.lastHeight;
    ++image.next;
    return true;
}

void System::unlock() {
    Image& image = *m_image;
    if (image.single) {
        m_device.unlockTexture(image.texture);
    } else {
        m_device.unlockTexture(image.textures[image.next - 1]);
    }
}

// 0x0055AF90 on the device: texture stage 0's colour the texture's; opaque (alpha at least 0.9999 and an opaque
// format) without blending and the texture's alpha, else SRCALPHA / INVSRCALPHA with the texture's alpha by the
// vertices'. The rest of the device's state is the interface's: OpenJuice's interface sets it with each quad
// (IRenderDevice::drawQuad), so the same base is set first.
void System::states(float alpha) {
    render::IRenderDevice& d = m_device;
    d.setVertexShader(0);
    d.setPixelShader(0);
    d.setRenderState(RenderState::Lighting, 0);
    d.setRenderState(RenderState::CullMode, Cull::None);
    d.setRenderState(RenderState::ZEnable, 0);
    d.setRenderState(RenderState::ZWriteEnable, 0);
    d.setRenderState(RenderState::AlphaTestEnable, 0);
    d.setRenderState(RenderState::FogEnable, 0);
    d.setRenderState(RenderState::ColourWriteEnable, ColourWrite::All);
    d.setSamplerState(0, SamplerState::AddressU, TextureAddress::Wrap);
    d.setSamplerState(0, SamplerState::AddressV, TextureAddress::Wrap);
    d.setSamplerState(0, SamplerState::MagFilter, TextureFilter::Linear);
    d.setSamplerState(0, SamplerState::MinFilter, TextureFilter::Linear);
    d.setSamplerState(0, SamplerState::MipFilter, TextureFilter::Linear);
    render::samplerCache()[0] = nullptr;  // stage 0's states no longer a texture's
    d.setTextureStageState(0, TextureStageState::TexCoordIndex, 0);
    d.setTextureStageState(0, TextureStageState::TextureTransformFlags, TextureTransform::Disable);
    d.setTextureStageState(1, TextureStageState::ColourOp, TextureOp::Disable);
    d.setTextureStageState(1, TextureStageState::AlphaOp, TextureOp::Disable);

    d.setTextureStageState(0, TextureStageState::ColourOp, TextureOp::SelectArg1);
    if (alpha >= 0.9999 && !m_image->alpha) {
        d.setRenderState(RenderState::AlphaBlendEnable, 0);
        d.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::SelectArg1);
        d.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Texture);
    } else {
        d.setRenderState(RenderState::AlphaBlendEnable, 1);
        d.setRenderState(RenderState::SrcBlend, BlendFactor::SrcAlpha);
        d.setRenderState(RenderState::DestBlend, BlendFactor::InvSrcAlpha);
        d.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::Modulate);
        d.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Texture);
        d.setTextureStageState(0, TextureStageState::ColourArg2, TextureArg::Diffuse);
    }
    d.setTextureStageState(0, TextureStageState::AlphaArg1, TextureArg::Texture);
}

// 0x0055AE40: a strip of two triangles (FVF 0x1C4: XYZRHW, DIFFUSE, SPECULAR, TEX1), width x height texels of the
// texture bound, scaled; the texture coordinates run to the movie's size over the image's ([0x0075F1D0] /
// [0x0075F1C8], [0x0075F1D4] / [0x0075F1CC]) on every tile alike; diffuse and specular white with the alpha (x 255,
// cut). [0x0075F144] (the engine's current vertex declaration) 0 after.
void System::quad(float x, float y, float scaleX, float scaleY, uint32_t width, uint32_t height, float alpha) {
    struct Vertex {
        float x, y, z, rhw;
        uint32_t diffuse, specular;
        float u, v;
    };
    static_assert(sizeof(Vertex) == 0x20);
    const float uMax = static_cast<float>(static_cast<int32_t>(m_frameWidth)) /
                       static_cast<float>(static_cast<int32_t>(m_imageWidth));
    const float vMax = static_cast<float>(static_cast<int32_t>(m_frameHeight)) /
                       static_cast<float>(static_cast<int32_t>(m_imageHeight));
    const uint32_t colour = (static_cast<uint32_t>(chop(alpha * 255.0f)) << 24) | 0x00FFFFFFu;
    const float right = static_cast<float>(static_cast<int32_t>(width)) * uMax * scaleX + x;
    const float bottom = static_cast<float>(static_cast<int32_t>(height)) * vMax * scaleY + y;
    const Vertex v[4] = {
        {x, y, 0.0f, 1.0f, colour, colour, 0.0f, 0.0f},
        {right, y, 0.0f, 1.0f, colour, colour, uMax, 0.0f},
        {x, bottom, 0.0f, 1.0f, colour, colour, 0.0f, vMax},
        {right, bottom, 0.0f, 1.0f, colour, colour, uMax, vMax},
    };
    m_device.setFvf(Fvf::XyzRhw | Fvf::Diffuse | Fvf::Specular | Fvf::Tex1);
    m_device.drawUp(5, 2, v, sizeof(Vertex));  // D3DPT_TRIANGLESTRIP
}

// 0x0055B5A0 (one texture) and 0x0055B3E0 (the tiles row by row, each a tile x the scale on from the last).
void System::blit(float x, float y, float scaleX, float scaleY, float alpha) {
    Image& image = *m_image;
    states(alpha);
    if (image.single) {
        m_device.setTexture(0, image.texture);
        quad(x, y, scaleX, scaleY, image.width, image.height, alpha);
        return;
    }
    const float tile = asFloat(image.tile);
    const float stepX = tile * scaleX;
    const float stepY = tile * scaleY;
    size_t t = 0;
    float rowY = y;
    for (uint32_t row = 0; row < image.rows; ++row) {
        float columnX = x;
        for (uint32_t c = 0; c < image.columns; ++c) {
            m_device.setTexture(0, image.textures[t++]);
            quad(columnX, rowY, scaleX, scaleY, image.tile, image.tile, alpha);
            columnX += stepX;
        }
        if (image.lastWidth != 0) {
            m_device.setTexture(0, image.textures[t++]);
            quad(columnX, rowY, scaleX, scaleY, image.lastWidth, image.tile, alpha);
        }
        rowY += stepY;
    }
    if (image.lastHeight != 0) {
        float columnX = x;
        for (uint32_t c = 0; c < image.columns; ++c) {
            m_device.setTexture(0, image.textures[t++]);
            quad(columnX, rowY, scaleX, scaleY, image.tile, image.lastHeight, alpha);
            columnX += stepX;
        }
        if (image.lastWidth != 0) {
            m_device.setTexture(0, image.textures[t++]);
            quad(columnX, rowY, scaleX, scaleY, image.lastWidth, image.lastHeight, alpha);
        }
    }
}

Player* System::acquire(const std::string& name, bool playOnce, const float (&min)[2], const float (&max)[2]) {
    const auto it = m_players.find(name);
    if (it != m_players.end()) {
        ++it->second->m_users;
        return it->second;
    }
    auto* player = new Player(*this, name, playOnce, min, max);
    // The base's constructor (0x00559850) keeps a player of a name in the texture manager (0x0055A960).
    if (!name.empty()) m_players.emplace(name, player);
    player->open();
    return player;
}

// 0x0055A300: the volume of its track 0; the close (0x0055A140): with a Bink the volume 0 (with the game object)
// and BinkClose, the movies open counted down and with none left the shared image freed; the name out of the
// texture manager (0x005599B0).
void System::release(Player* player) {
    if (player == nullptr) return;
    if (--player->m_users > 0 && !player->m_noMovie) return;
    if (player->m_bink != 0) {
        player->binkVolume(0);
        m_library->setVolume(player->m_bink, 0, 0);
        m_library->close(player->m_bink);
        if (--m_open == 0 && m_image) freeImage(m_image);
    }
    const auto it = m_players.find(player->m_name);
    if (it != m_players.end() && it->second == player) m_players.erase(it);
    if (current == player) current = nullptr;
    delete player;
}

void System::closeCurrent() {
    Player* p = current;
    current = nullptr;
    release(p);
    currentName.clear();
    currentElement = -1;
}

Player::Player(System& system, std::string name, bool once, const float (&min)[2], const float (&max)[2])
    : playOnce(once), m_system(system), m_name(std::move(name)) {
    m_min[0] = min[0];
    m_min[1] = min[1];
    m_max[0] = max[0];
    m_max[1] = max[1];
}

uint32_t Player::field(uint32_t offset) const {
    return m_bink != 0 ? m_system.m_library->field(m_bink, offset) : 0;
}

void Player::binkVolume(int32_t volume) const {
    if (m_bink != 0) m_system.m_library->setVolume(m_bink, m_track, volume);
}

// 0x00559B10. The rad3d made at the first open (0x0055B7C0, then the surface table 0x00559AA0). movies\<name>.bik;
// without it a name ending in a digit tries that digit as 1 and 2 in the order of rand() & 1 (1 + it first); with none
// the player has no movie (0x40000000 of its refcount word). The sound: unless [0x0071E718] & 1 the language's track
// (+0x68) is asked for (BinkSetSoundTrack(1, track)); a movie of one track is opened again with track 0 (+0x68 stays
// the language's; Stock problem: BinkSetVolume then names a track the movie does not play, so the volume and the muting
// never reach its sound; the fixed build's +0x68 0 then). With [0x0071E718] & 1 no track. The rectangle gives +0x40,
// +0x44 (its size over the movie's) and +0x50, +0x54 (its corner cut to whole pixels). The shared image is made again
// when the movie is larger than it (0x0055B740; tiles of 0x100), else kept; the movie's size is the one drawn.
bool Player::open() {
    System& s = m_system;
    if (!s.m_rad3d) {
        s.startRad3d();
        // 0x00559AA0: Bink's surface types by the rad3d index (BINKSURFACE32, 32A, 555, 565, 5551, 4444, 32R, 32RA,
        // 24, 24R).
        static constexpr uint32_t kTable[10] = {3, 5, 9, 10, 8, 7, 4, 6, 1, 2};
        std::memcpy(s.m_surfaces, kTable, sizeof(kTable));
    }
    m_bink = 0;
    const std::filesystem::path folder = std::filesystem::current_path() / "movies";
    std::string file = m_name + ".bik";
    auto resolved = gameNativePath(folder / file);
    bool found = std::filesystem::exists(resolved);
    if (!found && !m_name.empty() && m_name.back() >= '0' && m_name.back() <= '9') {
        const int first = s.m_game.random != nullptr ? (s.m_game.random->next() & 1) : 0;
        const int order[2] = {first, 1 - first};
        for (int k = 0; k < 2 && !found; ++k) {
            std::string tried = m_name;
            tried.back() = static_cast<char>('1' + order[k]);
            file = tried + ".bik";
            resolved = gameNativePath(folder / file);
            found = std::filesystem::exists(resolved);
        }
    }
    if (!found) {
        m_noMovie = true;
        return false;
    }
    if (s.m_library == nullptr || !s.m_rad3d) {
        // Stock problem: without a rad3d (a display of no usable format) the open reads its +0x24 through a null
        // pointer; OpenJuice leaves the player without a movie.
        m_noMovie = true;
        return false;
    }
    bink::Library& lib = *s.m_library;
    if ((s.m_game.audioFlags & 1) == 0) {
        uint32_t track = static_cast<uint32_t>(s.m_game.languageTrack);
        m_track = track;
        lib.setSoundTrack(1, &track);
        m_bink = lib.open(resolved, kOpenFlags);
        if (m_bink != 0 && lib.field(m_bink, kNumTracks) == 1) {
            lib.close(m_bink);
            const uint32_t zero = 0;
            lib.setSoundTrack(1, &zero);
            m_bink = lib.open(resolved, kOpenFlags);
            m_track = 0;
        }
    } else {
        lib.setSoundTrack(0, nullptr);
        m_bink = lib.open(resolved, kOpenFlags);
    }
    if (m_bink == 0) {
        // Stock problem: a file that Bink does not open leaves +0x30 0, and the open goes on to read the movie's size
        // through it; OpenJuice leaves the player without a movie.
        log::warning("movies: {} did not open", file);
        m_noMovie = true;
        return false;
    }
    const uint32_t width = lib.field(m_bink, kWidth);
    const uint32_t height = lib.field(m_bink, kHeight);
    m_scaleX = (m_max[0] - m_min[0]) / asFloat(width);
    m_scaleY = (m_max[1] - m_min[1]) / asFloat(height);
    m_fast = false;
    m_alpha = 1.0f;
    m_offsetX = 0.0f;
    m_offsetY = 0.0f;
    m_x = chop(m_min[0]);
    m_y = chop(m_min[1]);
    m_volume = 1.0f;
    s.m_firstFrames = false;
    m_first = true;
    if (s.m_image && width <= s.m_imageWidth && height <= s.m_imageHeight) {
        s.m_frameWidth = width;
    } else {
        s.freeImage(s.m_image);
        s.m_imageWidth = width;
        s.m_imageHeight = height;
        s.m_image = s.makeImage(width, height, false);
        s.m_frameWidth = width;
    }
    ++s.m_open;
    s.m_frameHeight = height;
    log::info("movies: {} ({}x{}, {} frames)", file, width, height, lib.field(m_bink, kFrames));
    return true;
}

uint32_t Player::frameNum() const {
    return field(kFrameNum);
}

bool Player::finished() const {
    if (!playOnce) return false;
    if (m_bink != 0 && field(kFrames) != 0) return frameNum() >= field(kFrames);
    return true;
}

void Player::setVolume(float volume) {
    m_volume = volume;
    // Stock problem: the volume in hundredths of decibels (0 for full), which Bink takes as silence until the step
    // sets it again. The fixed build gives Bink the step's volume (x 32768, cut).
    if (m_bink != 0) binkVolume(chop(volume * 32768.0f));
}

void Player::restart() {
    held = true;
    advance();
    advance();
}

// 0x0055A580: the image locked piece by piece, each piece's rectangle of the frame copied into it
// (BinkCopyToBufferRect with the surface type of the image's format, BINKCOPYALL when asked) and unlocked.
void Player::copy(bool all) {
    System& s = m_system;
    while (s.m_image) {
        void* bits = nullptr;
        int32_t pitch = 0;
        uint32_t surface = 0, x = 0, y = 0, w = 0, h = 0;
        if (!s.lock(&bits, &pitch, &surface, &x, &y, &w, &h)) return;
        const uint32_t flags = s.m_surfaces[surface] | (all ? kCopyAll : 0u);
        // Stock problem: the right column's and the bottom row's tiles copy their power-of-two sides, past the frame's
        // edge. The fixed build cuts them to the frame (a tile wholly past it copies nothing).
        if (x < s.m_frameWidth && y < s.m_frameHeight) {
            w = std::min(w, s.m_frameWidth - x);
            h = std::min(h, s.m_frameHeight - y);
            s.m_library->copyToBufferRect(m_bink, bits, pitch, h, 0, 0, x, y, w, h, flags);
        }
        if (!s.m_image) return;
        s.unlock();
    }
}

void Player::decode(bool all) {
    m_system.m_library->doFrame(m_bink);
    copy(all);
}

// 0x0055A3C0. The window inactive ([0x0075F780] 0): silent, nothing more. Held: silent. Else the volume (+0x60 x
// 32768, cut) and the step: fast (+0x34) the next frame asked for unless it is past the first and not yet its time
// (BinkWait), then decoded and copied; otherwise the first frame waited for (BinkWait spun), then decoded and copied
// whole and the next asked for when its time has come, else the frame copied again. A movie played once is held and
// silent at its last frame (false).
bool Player::advance() {
    System& s = m_system;
    if (!s.m_active) {
        binkVolume(0);
        return true;
    }
    if (!held) binkVolume(chop(m_volume * 32768.0f));
    if (m_bink == 0) return true;
    bink::Library& lib = *s.m_library;
    if (held) {
        binkVolume(0);
        return true;
    }
    binkVolume(chop(m_volume * 32768.0f));
    if (m_fast) {
        if (s.m_firstFrames) {
            // Never run: [0x0075F1C4] is only ever written 0.
            if (m_first) {
                decode(false);
                m_first = false;
            }
            lib.pause(m_bink, 1);
            lib.setField(m_bink, kFrameNum, 0);
            draw();
        } else {
            if (lib.field(m_bink, kFrameNum) <= 1 || lib.wait(m_bink) == 0) lib.nextFrame(m_bink);
            decode(false);
        }
    } else {
        if (lib.field(m_bink, kFrameNum) == 1) {
            while (lib.wait(m_bink) != 0) {
            }
        }
        if (lib.wait(m_bink) == 0) {
            decode(true);
            lib.nextFrame(m_bink);
        } else {
            copy(true);
        }
    }
    if (playOnce && lib.field(m_bink, kFrameNum) >= lib.field(m_bink, kFrames)) {
        binkVolume(0);
        held = true;
        return false;
    }
    return true;
}

// 0x0055A6A0: the movie's size is the one drawn; the image at (width x +0x48 x +0x40 + +0x50, height x +0x4C x
// +0x44 + +0x54), scaled by +0x40, +0x44, at the alpha +0x58; then the 3D renderer's material ended (0x0051BDA0 of
// [0x0075F16C], [0x00719540] 0x10009, [0x0075F16C] 0; [0x0075F178], never written, would be called).
void Player::draw() {
    System& s = m_system;
    const uint32_t width = field(kWidth);
    const uint32_t height = field(kHeight);
    const float x = asFloat(width) * m_offsetX * m_scaleX + static_cast<float>(m_x);
    const float y = asFloat(height) * m_offsetY * m_scaleY + static_cast<float>(m_y);
    s.m_frameWidth = width;
    s.m_frameHeight = height;
    if (s.m_image) s.blit(x, y, m_scaleX, m_scaleY, m_alpha);
    if (s.m_afterDraw) s.m_afterDraw();
}

}  // namespace oj::movie
