#include "Core/Formats/Cld.h"

#include "Core/Base/ByteReader.h"

#include <functional>

namespace oj {

namespace {

// The bytes of a block padded to a multiple of 4 from the start of the file.
void alignTo4(ByteReader& r) {
    const size_t pad = (4 - r.offset() % 4) % 4;
    r.skip(pad);
}

}  // namespace

std::optional<Cld> parseCld(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    Cld c;
    const uint32_t zones = r.u32();
    if (!r.fits(zones, 24)) return std::nullopt;
    c.zones.resize(zones);
    for (CldZone& z : c.zones) {
        z.flag = r.i32();
        z.friction = r.f32();
        z.bounce = r.f32();
        z.unused = r.f32();
        z.rangeMin = r.f32();
        z.rangeMax = r.f32();
    }
    const uint32_t vertices = r.u32();
    if (!r.fits(vertices, 16)) return std::nullopt;
    c.vertices.resize(vertices);
    for (auto& v : c.vertices) {
        for (float& f : v) f = r.f32();
    }
    const uint32_t faces = r.u32();
    if (!r.fits(faces, 29)) return std::nullopt;
    c.faces.resize(faces);
    for (CldFace& f : c.faces) {
        for (uint32_t& i : f.vertices) i = r.u32();
        for (float& n : f.normal) n = r.f32();
        f.d = r.f32();
    }
    // 0x0053DFF0: bit 7 of the zone bytes is cleared (the query marks collected faces with it).
    for (CldFace& f : c.faces) {
        uint8_t b = 0;
        r.bytes(&b, 1);
        f.zone = static_cast<uint8_t>(b & 0x7F);
    }
    alignTo4(r);
    const uint32_t markers = r.u32();
    if (!r.fits(markers, 33)) return std::nullopt;
    c.markers.resize(markers);
    for (CldMarker& m : c.markers) {
        for (float& f : m.position) f = r.f32();
        for (float& f : m.direction) f = r.f32();
    }
    for (CldMarker& m : c.markers) {
        uint8_t b = 0;
        r.bytes(&b, 1);
        m.zone = static_cast<uint8_t>(b & 0x7F);
    }
    alignTo4(r);
    for (float& f : c.area) f = r.f32();
    const uint32_t treeSize = r.u32();
    if (!r.ok() || treeSize != r.left()) return std::nullopt;
    const size_t treeEnd = r.offset() + treeSize;

    // The tree, depth first: bit 0 of the first word tells a leaf from an inner node.
    std::function<int32_t()> node = [&]() -> int32_t {
        if (!r.ok() || r.offset() >= treeEnd) return -1;
        const auto index = static_cast<int32_t>(c.nodes.size());
        c.nodes.emplace_back();
        const uint32_t flags = r.u32();
        if ((flags & 1) == 0) {
            r.skip(16);  // the child pointers the loader fills in
            const float splitX = r.f32();
            const float splitZ = r.f32();
            r.u32();
            c.nodes[static_cast<size_t>(index)].split = {splitX, splitZ};
            for (int k = 0; k < 4; ++k) {
                const int32_t child = node();
                if (child < 0) return -1;
                c.nodes[static_cast<size_t>(index)].children[static_cast<size_t>(k)] = child;
            }
        } else {
            Cld::Node& n = c.nodes[static_cast<size_t>(index)];
            n.leaf = true;
            const uint32_t faceCount = r.u32();
            r.u32();
            const uint32_t segmentCount = r.u32();
            r.u32();
            if (!r.fits(faceCount, 4)) return -1;
            n.faces.resize(faceCount);
            for (uint32_t& f : n.faces) f = r.u32();
            if (!r.fits(segmentCount, 4)) return -1;
            n.segments.resize(segmentCount);
            for (uint32_t& s : n.segments) s = r.u32();
        }
        return index;
    };
    if (treeSize != 0 && node() != 0) return std::nullopt;
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    for (const Cld::Node& n : c.nodes) {
        for (uint32_t f : n.faces) {
            if (f >= c.faces.size()) return std::nullopt;
        }
        for (uint32_t s : n.segments) {
            if (s + 1 >= c.markers.size() || c.markers[s].zone >= c.zones.size()) return std::nullopt;
        }
    }
    for (const CldFace& f : c.faces) {
        for (uint32_t v : f.vertices) {
            if (v >= c.vertices.size()) return std::nullopt;
        }
        if (f.zone >= c.zones.size()) return std::nullopt;
    }
    return c;
}

void Cld::query(float minX, float minZ, float maxX, float maxZ, std::vector<uint32_t>* outFaces,
                std::vector<uint32_t>& outSegments) const {
    static const std::vector<uint32_t> kNone;
    if (outFaces) outFaces->clear();
    outSegments.clear();
    if (nodes.empty()) return;
    std::vector<uint8_t> takenFace(outFaces ? faces.size() : 0, 0);
    std::vector<uint8_t> takenSegment(markers.size(), 0);
    // A node is entered when the box meets its area (edges count); a face or a segment is taken when one of its
    // corners lies strictly past each side of the box.
    std::function<void(int32_t, float, float, float, float)> visit = [&](int32_t at, float x0, float z0, float x1,
                                                                         float z1) {
        if (!(minZ <= z1 && z0 <= maxZ && minX <= x1 && x0 <= maxX)) return;
        const Node& n = nodes[static_cast<size_t>(at)];
        if (!n.leaf) {
            const float sx = n.split[0], sz = n.split[1];
            visit(n.children[0], x0, z0, sx, sz);
            visit(n.children[1], sx, z0, x1, sz);
            visit(n.children[2], x0, sz, sx, z1);
            visit(n.children[3], sx, sz, x1, z1);
            return;
        }
        for (uint32_t f : outFaces ? n.faces : kNone) {
            const auto& a = vertices[faces[f].vertices[0]];
            const auto& b = vertices[faces[f].vertices[1]];
            const auto& c = vertices[faces[f].vertices[2]];
            if ((minX < a[0] || minX < b[0] || minX < c[0]) && (a[0] < maxX || b[0] < maxX || c[0] < maxX) &&
                (minZ < a[2] || minZ < b[2] || minZ < c[2]) && (a[2] < maxZ || b[2] < maxZ || c[2] < maxZ) &&
                takenFace[f] == 0) {
                outFaces->push_back(f);
                takenFace[f] = 1;
            }
        }
        for (uint32_t s : n.segments) {
            const auto& a = markers[s].position;
            const auto& b = markers[s + 1].position;
            if ((minX < a[0] || minX < b[0]) && (a[0] < maxX || b[0] < maxX) && (minZ < a[2] || minZ < b[2]) &&
                (a[2] < maxZ || b[2] < maxZ) && takenSegment[s] == 0) {
                outSegments.push_back(s);
                takenSegment[s] = 1;
            }
        }
    };
    visit(0, area[0], area[1], area[2], area[3]);
}

}  // namespace oj
