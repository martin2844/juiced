#include "Core/Formats/Ocd.h"

#include "Core/Base/ByteReader.h"

namespace oj {

std::optional<Ocd> parseOcd(const uint8_t* data, size_t size) {
    ByteReader r(data, size);
    Ocd o;
    const uint32_t vertices = r.u32();
    if (!r.fits(vertices, 16)) return std::nullopt;
    o.vertices.resize(vertices);
    for (Vec4& v : o.vertices) {
        for (float& f : v) f = r.f32();
    }
    const uint32_t faces = r.u32();
    if (!r.fits(faces, 32)) return std::nullopt;
    o.faces.resize(faces);
    for (Ocd::Face& f : o.faces) {
        for (uint32_t& i : f.vertices) i = r.u32();
        r.u32();  // not read by the game
        for (float& n : f.normal) n = r.f32();
    }
    if (!r.ok()) return std::nullopt;
    if (r.atEnd()) return o;
    const uint32_t mapped = r.u32();
    if (!r.fits(mapped, 4)) return std::nullopt;
    o.faceMap.resize(mapped);
    for (uint32_t& f : o.faceMap) f = r.u32();
    if (r.atEnd()) return o;
    const uint32_t nodes = r.u32();
    if (!r.fits(nodes, 48)) return std::nullopt;
    o.nodes.resize(nodes);
    for (Ocd::Node& n : o.nodes) {
        for (float& f : n.centre) f = r.f32();
        n.radius = r.f32();
        r.skip(12);
        n.sibling = r.i32();
        n.child = r.i32();
        n.first = r.u32();
        n.count = r.u32();
    }
    if (!r.ok()) return std::nullopt;
    for (const Ocd::Face& f : o.faces) {
        for (uint32_t i : f.vertices) {
            if (i >= o.vertices.size()) return std::nullopt;
        }
    }
    for (uint32_t f : o.faceMap) {
        if (f >= o.faces.size()) return std::nullopt;
    }
    const auto nodeCount = static_cast<int32_t>(o.nodes.size());
    for (const Ocd::Node& n : o.nodes) {
        if (n.sibling < -1 || n.sibling >= nodeCount || n.child < -1 || n.child >= nodeCount) return std::nullopt;
        if (n.first > o.faceMap.size() || n.count > o.faceMap.size() - n.first) return std::nullopt;
    }
    for (const Vec4& v : o.vertices) {
        if (v[1] < o.lowestY) o.lowestY = v[1];
    }
    return o;
}

}  // namespace oj
