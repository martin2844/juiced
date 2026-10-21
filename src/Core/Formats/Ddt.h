#pragma once
// .ddt, a texture of the game: a 44-byte header and the mip levels (format details: the Blender add-on's
// docs/technical/DDT.md).
//
//   u32 format          FourCC 'DXT1' / 'DXT3' / 'DXT5', or 21 A8R8G8B8, 22 X8R8G8B8
//   u32 width, height, mipLevels
//   u32 minFilter, magFilter, mipFilter, addressU, addressV   (D3DTEXTUREFILTERTYPE, D3DTEXTUREADDRESS; the game
//                                                            sets them as the sampler states of the texture's stage)
//   f32 mipLodBias
//   u32 maxAnisotropy
//   u8  pixels[]        levels largest first, as in a DDS file

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oj {

struct Ddt {
    uint32_t format = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 0;
    uint32_t minFilter = 0, magFilter = 0, mipFilter = 0;
    uint32_t addressU = 0, addressV = 0;
    float mipLodBias = 0.0f;
    uint32_t maxAnisotropy = 0;
    uint32_t faces = 1;  // 6 for a cube .dds: the faces one after another, each with its levels
    std::vector<uint8_t> pixels;

    static constexpr uint32_t kDxt1 = 0x31545844;  // 'DXT1'
    static constexpr uint32_t kDxt2 = 0x32545844;  // DXT3 blocks, colours multiplied by alpha
    static constexpr uint32_t kDxt3 = 0x33545844;
    static constexpr uint32_t kDxt4 = 0x34545844;  // DXT5 blocks, colours multiplied by alpha
    static constexpr uint32_t kDxt5 = 0x35545844;
    static constexpr uint32_t kA8R8G8B8 = 21;
    static constexpr uint32_t kX8R8G8B8 = 22;

    bool parse(const std::vector<std::byte>& file);
    // A .dds file (the textures of effects\, the environment cubes of textures\): DXT1 .. DXT5, 32-bit with or
    // without alpha, one face or a cube; the sampler fields stay 0.
    bool parseDds(const std::vector<std::byte>& file);
};

bool isDxt(uint32_t format);
// The bytes of a level of the format (DXT in 4 x 4 blocks).
size_t levelBytes(uint32_t format, uint32_t width, uint32_t height);
// A level as 0xAARRGGBB pixels, the colours as stored (DXT2 / DXT4 keep them multiplied by alpha, as the hardware
// samples them).
std::vector<uint32_t> decodeLevel(uint32_t format, uint32_t width, uint32_t height, const uint8_t* data);
// D3DX's load of a texture file with the default mip levels (the stock cube textures): the file's levels, then the
// rest down to 1 x 1, each pixel the box average of four of the level above (in floats, rounded to 8 bits); DXT2 /
// DXT4 colours are divided by alpha before the filter and multiplied after. The faces one after another, each with
// all its levels, as A8R8G8B8; `levels` gets the count of a face.
std::vector<uint32_t> fullMipChain(const Ddt& texture, uint32_t& levels);

}  // namespace oj
