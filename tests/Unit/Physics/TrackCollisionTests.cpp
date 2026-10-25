// Unit tests: the fixed build's collision with the track and the other bodies (Core/Physics/TrackCollision.h): the
// walls' touches of the mesh collider, a face cut by an odd number of clipping planes, a clipping plane's crossings
// and a sphere beside a face.

#include "Framework/Test.h"

#include "Core/Formats/Cld.h"
#include "Core/Formats/Ocd.h"
#include "Core/Physics/Contact.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Physics/RigidBody.h"
#include "Core/Physics/TrackCollision.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using oj::test::check;

// A body that keeps its touches as they come.
class Toucher final : public oj::RigidBody {
public:
    using RigidBody::RigidBody;
    std::vector<oj::Touch> touches;
    void addTouch(const oj::Touch& t) override { touches.push_back(t); }
};

// A route of one face (zone 0) and one wall (markers 0 and 1, zone 1) in one leaf.
oj::Cld route(const std::vector<std::array<float, 4>>& face, float faceOffset, const std::array<float, 4>& from,
              const std::array<float, 4>& to, const std::array<float, 4>& facing) {
    oj::Cld c;
    oj::CldZone ground;
    ground.rangeMin = faceOffset;
    oj::CldZone wall;
    wall.rangeMin = -10000.0f;
    wall.rangeMax = 10000.0f;
    c.zones = {ground, wall};
    c.vertices = face;
    oj::CldFace f;
    f.vertices = {0, 1, 2};
    f.normal = {0.0f, 1.0f, 0.0f};
    f.zone = 0;
    c.faces = {f};
    oj::CldMarker m0, m1;
    m0.position = from;
    m0.direction = facing;
    m0.zone = 1;
    m1.position = to;
    m1.direction = facing;
    m1.zone = 1;
    c.markers = {m0, m1};
    c.area = {-2000.0f, -2000.0f, 2000.0f, 2000.0f};
    oj::Cld::Node leaf;
    leaf.leaf = true;
    leaf.faces = {0};
    leaf.segments = {0};
    c.nodes = {leaf};
    return c;
}

}  // namespace

// 0x0053B8D0: the mesh collider against a wall. The stock's touches carry no zone and take the offset of the last face
// of the pass (a face of offset -12 left them 12 shallower and 12 behind the wall); the fixed build's take no offset,
// as the wall's sphere and line touches, and the wall's zone.
OJ_UNIT_TEST(Collision, WallMeshTouches) {
    // One face of the mesh level at y 0 reaching from x 30 to 60 through the wall at x 50 that faces -x.
    auto shape = std::make_shared<oj::Ocd>();
    shape->vertices = {{30, 0, -20, 0}, {30, 0, 20, 0}, {60, 0, 0, 0}};
    oj::Ocd::Face face;
    face.vertices = {0, 1, 2};
    face.normal = {0, 1, 0, 0};
    shape->faces = {face};
    shape->faceMap = {0};
    oj::Ocd::Node root;
    root.centre = {45, 0, 0, 0};
    root.radius = 30.0f;
    root.count = 1;
    shape->nodes = {root};
    shape->lowestY = 0.0f;
    oj::BodyDef def;
    def.collision.colliderShape = shape;
    def.collision.meshColliderId = 4;
    // The route's face deep below (it touches nothing but sets the pass's offset), the wall at x 50.
    const oj::Cld cld = route({{-1000, -500, -1000, 0}, {-1000, -500, 1000, 0}, {1000, -500, 0, 0}}, -12.0f,
                              {50, 0, -100, 0}, {50, 0, 100, 0}, {-1, 0, 0, 0});
    oj::TrackCollision track(cld);
    Toucher body(def, {0, 0, 0, 0});
    track.collide(body);
    bool ok = body.touches.size() == 2;
    for (const oj::Touch& t : body.touches) {
        ok = ok && t.id == 4 && t.zone == &cld.zones[1] && std::fabs(t.depth - 10.0f) < 1e-3f &&
             std::fabs(t.point[0] - 50.0f) < 1e-3f && t.normal[0] == -1.0f;
    }
    check(ok, "collision (fixed): the mesh collider's touches with a wall take no offset and the wall's zone");
}

// 0x0053B8D0, 0x0053D0C0: a face cut by the clipping planes. The stock cut the first buffer into the second and back
// and gave the sphere colliders the first: after one plane the face uncut. The fixed build gives the last cut.
OJ_UNIT_TEST(Collision, OddClippingPlanes) {
    const oj::Cld cld = route({{-100, -10, -100, 0}, {-100, -10, 300, 0}, {300, -10, -100, 0}}, -10000.0f,
                              {1500, 0, 1500, 0}, {1500, 0, 1600, 0}, {-1, 0, 0, 0});
    // A sphere of 20 at the origin over the face (its foot inside, 10 deep), cut by planes keeping x below theirs.
    auto touches = [&](const std::vector<float>& cuts) {
        oj::BodyDef def;
        oj::SphereCollider s;
        s.id = 3;
        s.radius = 20.0f;
        def.collision.sphereColliders = {s};
        for (float x : cuts) {
            oj::ClippingPlane p;
            p.point = {x, 0, 0, 0};
            p.normal = {1, 0, 0, 0};
            def.collision.clippingPlanes.push_back(p);
        }
        oj::TrackCollision track(cld);
        Toucher body(def, {0, 0, 0, 0});
        track.collide(body);
        return body.touches.size();
    };
    check(touches({}) == 1 && touches({50.0f}) == 1 && touches({-50.0f}) == 0 && touches({200.0f, -50.0f}) == 0 &&
              touches({200.0f, 150.0f, -50.0f}) == 0 && touches({-50.0f, 200.0f, 150.0f}) == 0,
          "collision (fixed): a face cut by an odd number of clipping planes takes the last cut");
}

// 0x00538310, 0x005394E0: a clipping plane's crossing. The stock counted a corner also when the crossing was not
// found (an edge whose ends the side test puts on two sides, yet the crossing rounds to just before its start or past
// its end): the corner kept what the buffer held. The fixed build's crossing always lies on the edge.
OJ_UNIT_TEST(Collision, ClippingCrossings) {
    uint32_t seed = 12345u;
    auto next = [&]() {
        seed = seed * 1103515245u + 12345u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
    };
    bool ok = true;
    uint32_t cut = 0;
    for (int i = 0; i < 20000 && ok; ++i) {
        oj::ClippingPlane plane;
        float nx = next(), ny = next(), nz = next();
        const float length = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-6f;
        nx /= length;
        ny /= length;
        nz /= length;
        plane.point = {next() * 100.0f, next() * 100.0f, next() * 100.0f, 0.0f};
        plane.normal = {nx, ny, nz, 0.0f};
        // A corner within a hair of the plane, the others anywhere near.
        const float hair = next() * 1e-3f;
        const float u = next() * 50.0f;
        const oj::Vec4 tri[3] = {
            {plane.point[0] + nx * hair + ny * u, plane.point[1] + ny * hair - nx * u, plane.point[2] + nz * hair, 0},
            {plane.point[0] + next() * 80.0f, plane.point[1] + next() * 80.0f, plane.point[2] + next() * 80.0f, 0},
            {plane.point[0] + next() * 80.0f, plane.point[1] + next() * 80.0f, plane.point[2] + next() * 80.0f, 0}};
        oj::Vec4 out[6];
        for (oj::Vec4& v : out) v = {1e30f, 1e30f, 1e30f, 1e30f};
        const uint32_t corners = oj::clipPolygon(tri, 3, out, plane);
        if (corners > 3) ++cut;
        for (uint32_t k = 0; k < corners; ++k) ok = ok && out[k][0] != 1e30f;
    }
    check(ok && cut > 1000, "collision (fixed): every corner a clipping plane counts is written");
}

// 0x005388C0, 0x00538760: a sphere beside a face meeting two of its edges. The stock's least distance started as the
// count less 1 read as a float and the first edge met never set it: the first edge's point (the corner (0, 0, 0)),
// though the other's lies nearer. The fixed build takes the nearest edge's point.
OJ_UNIT_TEST(Collision, SphereNearestEdge) {
    const oj::Vec4 tri[3] = {{0, 0, 0, 0}, {0, 0, 100, 0}, {100, 0, 0, 0}};
    oj::SphereHit hit;
    const bool touched = oj::sphereOnFace(tri, 3, {0, 1, 0, 0}, {20, 2, -3, 0}, 30.0f, hit);
    check(touched && std::fabs(hit.point[0] - 20.0f) < 1e-3f && std::fabs(hit.point[2]) < 1e-3f &&
              hit.point[1] == 0.0f && hit.depth == 28.0f,
          "collision (fixed): a sphere beside a face touches at the nearest edge's point");
}
