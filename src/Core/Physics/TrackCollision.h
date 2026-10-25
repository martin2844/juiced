#pragma once
// A body's touches with a route's collision (.cld) in its step (0x0053EC30, the static part 0x0053B8D0; docs/spec/
// 16-physics.md, "The collision with the track").

#include "Core/Formats/Cld.h"
#include "Core/Physics/RigidBody.h"

#include <cstdint>
#include <vector>

namespace oj {

// A plane of the world's list (+0x34, 0x20 bytes; 0x004C7DF0 adds one): a point and the unit normal, in the world
// (centimetres).
struct WorldPlane {
    Vec4 point{};
    Vec4 normal{};
};

// What a sphere's test finds (the world's +0x8BC .. +0x8DC), in the model's frame: the point of the touch, how deep
// it is, and (a sphere against a sphere only) the normal.
struct SphereHit {
    Vec4 point{};        // +0x8BC
    float depth = 0.0f;  // +0x8CC
    Vec4 normal{};       // +0x8D0
};

// 0x00539330: a sphere against a plane (a point and its unit normal): its centre within its radius of the plane on
// either side; the centre's foot on the plane (w of the centre), the depth the radius less the centre's distance in
// front of the plane.
bool sphereOnPlane(const Vec4& point, const Vec4& normal, const Vec4& centre, float radius, SphereHit& hit);

// 0x00538310: a polygon cut by a clipping plane (Sutherland-Hodgman): the corners that lie behind the plane (their
// dot with its normal below the plane's) kept, a corner where an edge crosses it (0x005394E0; the fixed build's held
// to the edge) added; the corners written to `out` (room for twice the count), their count returned (none for fewer
// than 3).
uint32_t clipPolygon(const Vec4* in, uint32_t count, Vec4* out, const ClippingPlane& plane);

// 0x005388C0: a sphere against a polygon of `count` corners with its unit normal: the centre within its radius of the
// polygon's plane (either side); the depth is the radius less the centre's distance in front, also when it does not
// touch. When the centre's foot lies inside every edge the foot is the point, else of the edges whose lines meet the
// sphere within the edge (0x00538760) the nearest (the stock's the first) gives the middle of the two meetings held
// to the edge.
bool sphereOnFace(const Vec4* polygon, uint32_t count, const Vec4& normal, const Vec4& centre, float radius,
                  SphereHit& hit);

// 0x005395A0: a sphere collider against a sphere collidee (both in the collider's model): the normal the unit vector
// from the collidee's centre to the collider's; when the centres are nearer than the radii added, the point midway
// between them (w of the collider's) and the depth half the overlap.
bool sphereOnSphere(const Vec4& collider, float colliderRadius, const Vec4& collidee, float collideeRadius,
                    SphereHit& hit);

// 0x00504630 / 0x0053F500: the height of the highest face under (x, z) (a ray down from y 1000000 over 2000000
// centimetres; the first face met, then any met deeper); false when none.
bool groundBelow(const Cld& cld, float x, float z, float& y);

// 0x0053F2A0: the heights where a ray down through (x, z), from y 1000000 over 2000000 centimetres, meets the
// route's faces from their front: the least and the greatest (FLT_MAX and -FLT_MAX when it meets none); false when it
// meets none.
bool groundSpan(const Cld& cld, float x, float z, float& lowest, float& highest);

class TrackCollision : public Collision {
public:
    // track: the world has the route's collision (+0x30); a world without a route (0x0053DEA0) has only its planes.
    explicit TrackCollision(const Cld& cld, bool track = true) : m_cld(cld), m_track(track) {}

    // 0x0053B8D0: first each plane of the world against the body (planePass), then with a route the faces and the
    // walls near the body's sphere, taken into the model's frame (centimetres): while the body stands (+0x7C) a face
    // cut by the definition's clipping planes against the sphere colliders, the face against the line colliders;
    // while it lies (+0x80) against the mesh collider; then the bodies found near (0x0053D0C0).
    void collide(RigidBody& body) override;

    std::vector<WorldPlane> planes;  // the world's +0x34

    // 0x005046E0 / 0x0053F7F0: the walls between two points of the world (centimetres): of the walls whose bottom
    // edge `from` does not lie behind, the touch of the segment nearest to `from`; false when none.
    bool firstWall(const Vec4& from, const Vec4& to, Vec4& point);

    // 0x00504630 / 0x0053F500: the height of the highest face under (x, z) (a ray down from y 1000000 over 2000000
    // centimetres); false when none.
    bool groundBelow(float x, float z, float& y);

    // 0x0053D0C0: the other body against this body, when their spheres overlap: while this body stands (+0x7C) its
    // sphere colliders against the other's sphere collidees (0x005395A0); each face of the other's collidee mesh
    // taken into this body's frame that the centre of gravity lies in front of, while it stands cut by the clipping
    // planes against the sphere colliders and against the line colliders, while it lies (+0x80) against the mesh
    // collider (no zone; the collidee's Mu and Restitution); last, while it lies, its mesh collider against each of
    // the other's sphere collidees (0x0053A890, the collidee's Mu and Restitution).
    void collideWith(RigidBody& body, RigidBody& other);

private:
    // 0x0053AB00 with the plane in the model's frame (0x00538250): when the body's sphere reaches the plane (its
    // centre within the radius of it), each sphere collider within its radius of the plane (0x00539330) touches it
    // at its centre's foot, the depth its radius less its centre's distance in front; then each segment of the
    // colliders (0x00539410) entering the plane from its front within its length touches it there, the depth the
    // length beyond the plane. Both with the plane's normal, Mu 0.8, restitution 0, the zone of the world's +0x8E0
    // (none in a world without a route).
    void planePass(RigidBody& body, const Mat34& m, const WorldPlane& plane);

    // The face cut by the definition's clipping planes in turn (0x00538310), the first buffer into the second and
    // back; the corners of the first buffer and their count are what the sphere colliders take (the fixed build's
    // first buffer holds the last cut).
    uint32_t clipFace(const Vec4& a, const Vec4& b, const Vec4& c, const std::vector<ClippingPlane>& cuts);

    // 0x0053A890: the body's mesh collider against a sphere (in the model's frame): its nodes whose spheres reach
    // the sphere walked (0x0053A750), then each of their faces against it (0x005388C0).
    void meshSphere(RigidBody& body, const Ocd& mesh, const Vec4& centre, float radius, float friction, float bounce,
                    RigidBody* other);
    void walkSphere(const Ocd& mesh, int32_t node, const Vec4& centre, float radius);

    const Cld& m_cld;
    bool m_track = true;
    std::vector<Vec4> m_clipA, m_clipB;          // the polygons of the clipping (the stack's buffers)
    std::vector<uint32_t> m_faces, m_segments;  // +0x8F0, +0x900
    std::vector<uint32_t> m_tested;             // the mesh's faces met for a triangle (the body's +0xA4)
    std::vector<uint8_t> m_marked;              // the mark of each face in m_tested (bit 7 of its first index)
};

}  // namespace oj
