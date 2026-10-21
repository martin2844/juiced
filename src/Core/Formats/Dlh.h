#pragma once
// A track's scene objects (<track>_<name>.dlh; the layout is the Blender add-on's docs/technical/DLH): the objects
// (meshes, dummies, lights, effects, splines, helpers) with their physics hulls and 3ds Max properties, and the tree
// of nodes placing them (world matrices). Read by 0x0054F520 (docs/spec/21-scene-objects.md).

#include "Core/Base/Matrix.h"
#include "Core/Formats/Model.h"
#include "Core/Formats/Ocd.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oj {

struct DlhSubmesh {
    uint32_t material = 0;              // the first word of its 32 bytes, through the route's material table
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t list = 0;                  // 1 a triangle list, 0 a strip
    std::array<float, 5> sphere{};      // centre, 1, radius
    std::array<float, 3> boxMin{};
    std::array<float, 3> boxMax{};
    float lodMin = 0.0f;                // squared distances
    float lodMax = 0.0f;
    uint32_t flags = 0;                 // 2 never drawn, 4 the alternative (hit) set
    std::vector<VertexElement> declaration;  // with the D3DDECL_END record
    uint32_t stride = 0;
    uint32_t vertexCount = 0;
    std::vector<uint8_t> vertices;
};

struct DlhMesh {
    std::vector<uint16_t> indices;
    std::vector<DlhSubmesh> submeshes;
    std::array<float, 5> sphere{};
    std::array<float, 3> boxMin{};
    std::array<float, 3> boxMax{};
};

struct DlhSplinePoint {
    Vec4 knot{};
    Vec4 in{};   // the left handle
    Vec4 out{};  // the right handle
};

struct DlhObject {
    enum Type : uint32_t { Mesh = 1, Dummy = 2, Light = 3, Effect = 4, Spline = 5, Helper = 6 };
    uint32_t type = 0;
    std::string name;                   // +0x08 (32 bytes)
    DlhMesh mesh;                       // type 1
    uint32_t lightKind = 0;             // type 3: 1, 2 or 3
    Vec4 lightColour{};
    std::array<float, 2> lightCones{};  // kind 2
    std::string effectRef;              // type 4 (32 bytes, empty in the game's files)
    uint32_t closed = 0;                // type 5
    std::vector<DlhSplinePoint> points;
    uint32_t shape = 0;                 // type 6: 1 a box, 2 a sphere
    Vec4 boxMin{};
    Vec4 boxMax{};
    Vec4 centre{};
    float radius = 0.0f;
    uint32_t flags = 0;                 // +0x28: bit 0 a hull (+0x34), bit 1 another (+0x30)
    std::optional<Ocd> hull;            // +0x34
    std::optional<Ocd> hull2;           // +0x30
    std::string text;                   // +0x2C: the properties' text
};

struct DlhNode {
    std::array<float, 16> matrix{};  // the world matrix, row-major, the position in [3], [7], [11]
    uint32_t object = 0;
    std::vector<DlhNode> children;
};

struct Dlh {
    std::vector<DlhObject> objects;
    std::vector<DlhNode> roots;
};

// None when the file does not read to its end.
std::optional<Dlh> parseDlh(const std::vector<uint8_t>& data);

// A physics hull (.ocd content) read from the reader's place (0x00535830); the reader is left after it.
class ByteReader;
std::optional<Ocd> readOcd(ByteReader& r);

}  // namespace oj
