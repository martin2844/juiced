#pragma once
// The files of a race track (docs/spec/15-tracks.md; the byte layouts: the Blender add-on's docs/technical TDF, D8G,
// D8M_ROUTE, D8W_D8T, SHD_TXB, SPL): the track's routes and the layers each is built of, the geometry layers, a
// route's materials, its textures and baked-light tiles, its racing lines and starting grid.

#include "Core/Formats/Model.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oj {

// tracks\<Track>\<track>.tdf: the routes and the layers (.lyr geometry, .dlh scene objects) of each.
struct TdfEntry {
    uint32_t hidden = 0;    // flagsA: the race conditions the entry is not loaded / drawn in
    uint32_t reflected = 0;  // flagsB: the conditions a geometry layer is drawn into the wet road's reflection in
    std::string name;       // "<name>.lyr" or "<name>.dlh"
};
struct TdfRoute {
    std::string name;  // "<Route>.wld"
    std::vector<TdfEntry> entries;
};
struct Tdf {
    std::string track;
    std::vector<TdfRoute> routes;
};
std::optional<Tdf> parseTdf(const std::vector<uint8_t>& data);

// The race condition bit of the .tdf flags: 1 day dry, 2 day wet, 4 night dry, 8 night wet.
uint32_t conditionBit(bool night, bool wet);

// A mesh of a .d8g geometry layer.
struct TrackMesh {
    uint32_t materialId = 0;               // through the route's .d8m remap
    std::array<float, 4> centre{};         // bounding sphere centre (w = 1)
    float radius = 0.0f;
    std::array<float, 6> range{};          // (near², far²) of the main view, the cars' environment, the reflection
    std::vector<VertexElement> declaration;  // with the D3DDECL_END record
    uint32_t stride = 0;
    uint32_t vertexCount = 0;
    std::vector<uint8_t> vertices;
    std::vector<uint16_t> indices;         // none for line strips
    std::vector<std::array<float, 16>> nodes;  // instance matrices, row-major, position in [3], [7], [11]

    // Instances: one per node, one for a mesh without nodes.
    size_t instanceCount() const { return nodes.empty() ? 1 : nodes.size(); }
};
struct TrackGroup {
    uint32_t primitive = 0;  // 1 triangle list, 2 triangle strip (0xFFFF restarts), 3 line strip
    std::vector<TrackMesh> meshes;
};
// A .d8g of the release layout (6 range floats); none when it does not parse to its end.
std::optional<std::vector<TrackGroup>> parseD8g(const std::vector<uint8_t>& data);

// A route's .d8m: its materials and the table from the geometry's material ids to them.
struct RouteMaterial {
    uint32_t classId = 0;
    std::vector<std::vector<uint8_t>> params;  // as stored: 4 bytes, or 256 for a string
    std::vector<uint32_t> textures;            // indices into the route .d8w's texture list
};
struct RouteMaterials {
    std::vector<RouteMaterial> materials;
    std::vector<int32_t> remap;  // material id -> material, -1 not used by the route
};
std::optional<RouteMaterials> parseRouteMaterials(const std::vector<uint8_t>& data);

// A texture directory (the head of a .d8w, a .shd): where each texture's header and pixels are in the pixel file
// (.d8t, .txb).
struct TextureEntry {
    uint32_t offset = 0;    // in the pixel file
    uint32_t dataSize = 0;
    std::array<uint8_t, 44> header{};  // a .ddt header
};
struct TextureDirectory {
    std::vector<TextureEntry> textures;
    uint32_t end = 0;  // where the pixels of the directory end in the pixel file (the cursor after the last table)
};
// A directory counts from the start of its pixel file; the `skip` of a table moves past the bytes of other
// directories (the four .shd of a track share one .txb, their tables lying between one another).
bool readTextureDirectory(class ByteReader& r, TextureDirectory& out);
std::optional<TextureDirectory> parseShd(const std::vector<uint8_t>& data);

// A route's .d8w: its textures, the baked-light tile of every instance per time of day, the render tree.
struct D8w {
    TextureDirectory textures;
    struct LightSet {
        std::string name;  // shad_morn, shad_noon, shad_eve, shad_night
        std::vector<int32_t> tiles;
    };
    std::vector<LightSet> sets;
    std::array<float, 4> area{};  // x0, z0, x1, z1 of the tree's root
    std::vector<uint8_t> tree;
};
// `instances`: the instance count of the route's geometry layers (the sets hold a tile per instance).
std::optional<D8w> parseD8w(const std::vector<uint8_t>& data, size_t instances);

// A .ddt image of a directory entry: its header and the pixels it takes from the pixel file.
std::vector<std::byte> textureFile(const TextureEntry& entry, const std::vector<uint8_t>& pixels);

// A node of the render tree at the end of a route's .d8w (0x00548790, 0x00548AC0): the tree is the memory the game
// walks, depth first, each inner node followed by its four children. An inner node (0x5C bytes): u32 flags (bit 0
// clear), four child places, f32 the split's x and z (+0x14), the box's low and high corners (+0x1C, +0x2C, x y z w)
// and the 256 bits of its regions (+0x3C). A leaf (0x4C bytes): u32 flags (bit 0 set), u32 its records' count, a place
// for their start, the box (+0x0C, +0x1C), the regions (+0x2C), then 0xCC bytes per record: u32 the instance's number
// in the .ocl order (0x00543230 puts its mesh there and a copy of the instance after it).
struct RenderTreeNode {
    bool leaf = false;
    std::array<float, 4> low{};
    std::array<float, 4> high{};
    float splitX = 0.0f;
    float splitZ = 0.0f;
    std::array<uint32_t, 8> regions{};
    // An inner node's children: (low x, low z), (high x, low z), (low x, high z), (high x, high z).
    std::array<uint32_t, 4> children{};
    std::vector<uint32_t> instances;
};
// The nodes with the root first; none when the tree does not end where its bytes do.
std::optional<std::vector<RenderTreeNode>> parseRenderTree(const std::vector<uint8_t>& tree);

// A route's .ocl: a grid of region numbers over the route's box and, per instance of the route's geometry layers (in
// the .tdf order), the regions it is drawn from (256 bits).
struct Ocl {
    uint32_t size[3] = {};  // cells along x, y, z
    std::array<float, 4> min{};
    std::array<float, 4> max{};
    std::vector<uint8_t> grid;  // x fastest, then y, then z
    std::vector<std::array<uint32_t, 8>> records;

    // 0x00551C00: the region of a point, brought into the box (0 without a grid).
    uint32_t region(float x, float y, float z) const;
};
std::optional<Ocl> parseOcl(const std::vector<uint8_t>& data);

// scripts\<route>_intro.cam: the shots of the fly-by before a race (0x0041C8D0 reads it).
struct IntroKey {
    float time = 0.0f;                // seconds from this key to the next; after the last, before the next shot
    float fov = 0.0f;                 // radians
    std::array<float, 4> position{};  // x, y, z and the file's word after the fov as w
    std::array<float, 4> rotation{};  // w, x, y, z: camera to world
    float pitch = 0.0f;               // kept for the tool, not used
    float yaw = 0.0f;
};
struct IntroShot {
    float wait = 0.0f;  // seconds at the first key before the camera moves
    std::vector<IntroKey> keys;
};
std::optional<std::vector<IntroShot>> parseIntroCam(const std::vector<uint8_t>& data);

// A route's .spl: the starting grid and the racing lines.
struct SplPoint {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};
struct Spl {
    struct GridPoint {
        float x = 0.0f, y = 0.0f, z = 0.0f, heading = 0.0f;  // forward = (sin h, 0, cos h)
    };
    std::vector<GridPoint> grid;  // at most 6 are kept
    bool pointToPoint = false;
    int32_t lastPoint = 0;  // ROUTETYPE POINT2POINT <count>
    float minCornerSpeed = 13.41f;
    std::vector<SplPoint> fast, attack, centre;
};
Spl parseSpl(const std::string& text);

}  // namespace oj
