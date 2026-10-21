#pragma once
// A route's collision (tracks\<Track>\<track>_<route>.cld; the byte layout is the Blender add-on's docs/technical
// CLD): the surface zones, the triangles the cars drive on, the invisible walls between markers, and the quadtree the
// game searches (docs/spec/15-tracks.md, "The collision").

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace oj {

struct CldZone {
    int32_t flag = 0;       // the surface: 0 tarmac, 1 sand, 2 grass
    float friction = 1.0f;  // multiplies a shape's Mu in a contact with a face of the zone
    float bounce = 1.0f;    // multiplies a shape's Restitution
    float unused = 0.0f;
    float rangeMin = 0.0f;  // faces: the contact offset along the normal; walls: their bottom above the marker
    float rangeMax = 0.0f;  // walls: their top above the marker
};

struct CldFace {
    std::array<uint32_t, 3> vertices{};
    std::array<float, 3> normal{};
    float d = 0.0f;  // 0 in every file
    uint8_t zone = 0;
};

struct CldMarker {
    std::array<float, 4> position{};
    std::array<float, 4> direction{};  // the wall's horizontal normal
    uint8_t zone = 0;
};

struct Cld {
    std::vector<CldZone> zones;
    std::vector<std::array<float, 4>> vertices;
    std::vector<CldFace> faces;
    std::vector<CldMarker> markers;
    std::array<float, 4> area{};  // minX, minZ, maxX, maxZ of the tree's root

    // The quadtree, depth first: an inner node's four children cover its area split at (split x, split z) in the order
    // low-low, high x, high z, high-high; a leaf lists faces and the markers that start a wall segment.
    struct Node {
        bool leaf = false;
        std::array<float, 2> split{};
        std::array<int32_t, 4> children{-1, -1, -1, -1};
        std::vector<uint32_t> faces;
        std::vector<uint32_t> segments;
    };
    std::vector<Node> nodes;

    // 0x0053B200 (the world's lists +0x8F0, +0x900; a body's collision) and 0x0053AD70 (the caller's lists; the ground
    // under a point): the faces and wall segments of the leaves whose area the box (x, z) overlaps, each once (bit 7
    // of the face's and the segment's flag); without `faces` only the segments (0x0053B640). A node holding the whole
    // box is copied to the body being collided (the world's +0x884: its +0x84, +0xA0 1), which changes no result.
    void query(float minX, float minZ, float maxX, float maxZ, std::vector<uint32_t>* faces,
               std::vector<uint32_t>& segments) const;
};

// None when the data does not read to its end.
std::optional<Cld> parseCld(const std::vector<uint8_t>& data);

}  // namespace oj
