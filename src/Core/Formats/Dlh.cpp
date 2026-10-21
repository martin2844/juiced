#include "Core/Formats/Dlh.h"

#include "Core/Base/ByteReader.h"

namespace oj {

namespace {

std::array<float, 3> three(ByteReader& r) {
    std::array<float, 3> v{};
    for (float& f : v) f = r.f32();
    return v;
}

Vec4 four(ByteReader& r) {
    Vec4 v{};
    for (float& f : v) f = r.f32();
    return v;
}

bool readMesh(ByteReader& r, DlhMesh& m) {
    const uint32_t indices = r.u32();
    if (!r.fits(indices, 2)) return false;
    m.indices.resize(indices);
    for (uint16_t& i : m.indices) i = r.u16();
    const uint32_t submeshes = r.u32();
    if (!r.fits(submeshes, 32)) return false;
    m.submeshes.resize(submeshes);
    for (DlhSubmesh& s : m.submeshes) {
        s.material = r.u32();
        r.skip(28);  // pointers of the exporter
        s.indexStart = r.u32();
        s.indexCount = r.u32();
        s.list = r.u32();
        for (float& f : s.sphere) f = r.f32();
        s.boxMin = three(r);
        s.boxMax = three(r);
        s.lodMin = r.f32();
        s.lodMax = r.f32();
        s.flags = r.u32();
        const uint32_t elements = r.u32();
        if (!r.fits(elements, 8)) return false;
        s.declaration.resize(elements);
        for (VertexElement& e : s.declaration) {
            e.stream = r.u16();
            e.offset = r.u16();
            e.type = static_cast<uint8_t>(r.u8());
            e.method = static_cast<uint8_t>(r.u8());
            e.usage = static_cast<uint8_t>(r.u8());
            e.usageIndex = static_cast<uint8_t>(r.u8());
        }
        s.stride = r.u32();
        s.vertexCount = r.u32();
        if (!r.fits(s.vertexCount, s.stride)) return false;
        s.vertices.resize(static_cast<size_t>(s.stride) * s.vertexCount);
        r.bytes(s.vertices.data(), s.vertices.size());
    }
    for (float& f : m.sphere) f = r.f32();
    m.boxMin = three(r);
    m.boxMax = three(r);
    return r.ok();
}

bool readNode(ByteReader& r, DlhNode& n, size_t objects, int depth) {
    if (depth > 64) return false;
    for (float& f : n.matrix) f = r.f32();
    n.object = r.u32();
    if (n.object >= objects) return false;
    const uint32_t children = r.u32();
    if (!r.fits(children, 72)) return false;
    n.children.resize(children);
    for (DlhNode& c : n.children) {
        if (!readNode(r, c, objects, depth + 1)) return false;
    }
    return r.ok();
}

}  // namespace

std::optional<Ocd> readOcd(ByteReader& r) {
    Ocd o;
    const uint32_t vertices = r.u32();
    if (!r.fits(vertices, 16)) return std::nullopt;
    o.vertices.resize(vertices);
    for (Vec4& v : o.vertices) v = four(r);
    const uint32_t faces = r.u32();
    if (!r.fits(faces, 32)) return std::nullopt;
    o.faces.resize(faces);
    for (Ocd::Face& f : o.faces) {
        for (uint32_t& i : f.vertices) i = r.u32();
        r.u32();
        f.normal = four(r);
    }
    const uint32_t mapped = r.u32();
    if (!r.fits(mapped, 4)) return std::nullopt;
    o.faceMap.resize(mapped);
    for (uint32_t& f : o.faceMap) f = r.u32();
    const uint32_t nodes = r.u32();
    if (!r.fits(nodes, 48)) return std::nullopt;
    o.nodes.resize(nodes);
    for (Ocd::Node& n : o.nodes) {
        n.centre = four(r);
        n.radius = r.f32();
        r.skip(12);
        n.sibling = r.i32();
        n.child = r.i32();
        n.first = r.u32();
        n.count = r.u32();
    }
    if (!r.ok()) return std::nullopt;
    for (const Vec4& v : o.vertices) {
        if (v[1] < o.lowestY) o.lowestY = v[1];
    }
    return o;
}

std::optional<Dlh> parseDlh(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    Dlh d;
    const uint32_t objects = r.u32();
    if (!r.fits(objects, 44)) return std::nullopt;
    d.objects.resize(objects);
    for (DlhObject& o : d.objects) {
        o.type = r.u32();
        o.name = r.name(32);
        switch (o.type) {
        case DlhObject::Mesh:
            if (!readMesh(r, o.mesh)) return std::nullopt;
            break;
        case DlhObject::Dummy:
            break;
        case DlhObject::Light:
            o.lightKind = r.u32();
            if (o.lightKind == 1 || o.lightKind == 3) {
                o.lightColour = four(r);
            } else if (o.lightKind == 2) {
                o.lightColour = four(r);
                o.lightCones = {r.f32(), r.f32()};
            }
            break;
        case DlhObject::Effect:
            o.effectRef = r.name(32);
            break;
        case DlhObject::Spline: {
            o.closed = r.u32();
            const uint32_t points = r.u32();
            if (!r.fits(points, 48)) return std::nullopt;
            o.points.resize(points);
            for (DlhSplinePoint& p : o.points) {
                p.knot = four(r);
                p.in = four(r);
                p.out = four(r);
            }
            break;
        }
        case DlhObject::Helper:
            o.shape = r.u32();
            if (o.shape == 1) {
                o.boxMin = four(r);
                o.boxMax = four(r);
            } else if (o.shape == 2) {
                o.centre = four(r);
                o.radius = r.f32();
            }
            break;
        default:
            return std::nullopt;
        }
        // 0x0054E9C0: the flags, the hulls of their bits, the properties' text.
        o.flags = r.u32();
        if ((o.flags & 1) != 0) {
            o.hull = readOcd(r);
            if (!o.hull) return std::nullopt;
        }
        if ((o.flags & 2) != 0) {
            o.hull2 = readOcd(r);
            if (!o.hull2) return std::nullopt;
        }
        const uint32_t length = r.u32();
        if (!r.fits(length, 1)) return std::nullopt;
        std::string text(length, '\0');
        r.bytes(text.data(), length);
        text.resize(strnlen(text.data(), text.size()));
        o.text = std::move(text);
        if (!r.ok()) return std::nullopt;
    }
    const uint32_t roots = r.u32();
    if (!r.fits(roots, 72)) return std::nullopt;
    d.roots.resize(roots);
    for (DlhNode& n : d.roots) {
        if (!readNode(r, n, d.objects.size(), 0)) return std::nullopt;
    }
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return d;
}

}  // namespace oj
