#include "Core/Physics/TrackCollision.h"

#include "Core/Formats/Ocd.h"
#include "Core/Physics/PhysicsWorld.h"

#include <bit>
#include <cmath>
#include <utility>

namespace oj {

namespace {

// A face in the model's frame: its corners and its normal (the game's 0x40-byte triangle).
struct Triangle {
    Vec4 a{}, b{}, c{}, normal{};
};

Vec4 minus(const Vec4& p, const Vec4& q) {
    return {p[0] - q[0], p[1] - q[1], p[2] - q[2], p[3]};
}

// 0x005390E0: where a collider's segment enters the face from its front: the point and how far the segment reaches
// beyond it.
bool lineHits(const LineCollider& l, const Triangle& t, Vec4& point, float& depth) {
    const Vec4& n = t.normal;
    const Vec4& p = l.point;
    const Vec4& d = l.direction;
    const float towards = (n[2] * d[2] + n[1] * d[1]) + d[0] * n[0];
    if (!(towards < 0.0f)) return false;
    const float along =
        (((t.a[2] - p[2]) * n[2] + (t.a[1] - p[1]) * n[1]) + (t.a[0] - p[0]) * n[0]) / towards;
    if (!(along >= 0.0f) || along > l.length) return false;
    point = {p[0] + d[0] * along, p[1] + d[1] * along, p[2] + d[2] * along, p[3]};
    depth = l.length - along;
    // Inside: each edge turns towards the point about the normal.
    const Vec4 pa = minus(point, t.a), ab = minus(t.b, t.a);
    const Vec4 c1{ab[1] * pa[2] - pa[1] * ab[2], pa[0] * ab[2] - ab[0] * pa[2], pa[1] * ab[0] - pa[0] * ab[1], 0.0f};
    const Vec4 pb = minus(point, t.b), bc = minus(t.c, t.b);
    const Vec4 c2{bc[1] * pb[2] - bc[2] * pb[1], pb[0] * bc[2] - pb[2] * bc[0], bc[0] * pb[1] - pb[0] * bc[1], 0.0f};
    const Vec4 pc = minus(point, t.c), ca = minus(t.a, t.c);
    const Vec4 c3{ca[1] * pc[2] - ca[2] * pc[1], pc[0] * ca[2] - pc[2] * ca[0], pc[1] * ca[0] - ca[1] * pc[0], 0.0f};
    return (c1[2] * n[2] + c1[1] * n[1]) + c1[0] * n[0] > 0.0f &&
           (c2[2] * n[2] + c2[1] * n[1]) + c2[0] * n[0] > 0.0f &&
           (c3[2] * n[2] + c3[1] * n[1]) + c3[0] * n[0] > 0.0f;
}

// 0x00538620: where the line p + t d meets the sphere, t0 the root of + the square root, t1 of -; false when it
// does not (the discriminant not above 0).
bool sphereRoots(const Vec4& p, const Vec4& d, const Vec4& c, float radius, float& t0, float& t1) {
    const float a = (d[2] * d[2] + d[1] * d[1]) + d[0] * d[0];
    const float b = (((p[2] - c[2]) * d[2] + (p[1] - c[1]) * d[1]) + (p[0] - c[0]) * d[0]) * 2.0f;
    const float squares = ((((c[2] * c[2] + c[1] * c[1]) + c[0] * c[0]) + p[2] * p[2]) + p[1] * p[1]) + p[0] * p[0];
    const float across = (p[0] * c[0] + c[1] * p[1]) + p[2] * c[2];
    const float q = (squares - (across + across)) - radius * radius;
    const float disc = b * b - (q * a) * 4.0f;
    if (!(disc > 0.0f)) return false;
    const float half = 0.5f / (a + 1.1920929e-7f);
    const float root = std::sqrt(disc);
    t0 = (root - b) * half;
    t1 = (-b - root) * half;
    return true;
}

// 0x00538620 / 0x00538760: the segment p .. p + d (both ends) passes through the sphere.
bool segmentMeetsSphere(const Vec4& p, const Vec4& d, const Vec4& c, float radius) {
    float t0 = 0.0f, t1 = 0.0f;
    if (!sphereRoots(p, d, c, radius, t0, t1)) return false;
    if (t0 < 0.0f && t1 < 0.0f) return false;
    return !(t0 > 1.0f && t1 > 1.0f);
}

// 0x00539FB0: a node's sphere reaches the triangle: not beyond its front, and over its inside or one of its edges.
bool sphereTouches(const Ocd::Node& q, const Triangle& t) {
    const Vec4& n = t.normal;
    const float d = ((q.centre[1] - t.a[1]) * n[1] + (q.centre[2] - t.a[2]) * n[2]) + (q.centre[0] - t.a[0]) * n[0];
    if (d > q.radius) return false;
    const Vec4 p{q.centre[0] - n[0] * d, q.centre[1] - n[1] * d, q.centre[2] - n[2] * d, 0.0f};
    const Vec4 e0 = minus(t.b, t.a), e1 = minus(t.c, t.b), e2 = minus(t.a, t.c);
    // About the normal each edge turns towards the foot of the centre.
    auto turn = [&](const Vec4& corner, const Vec4& e) {
        const float rx = p[0] - corner[0], ry = p[1] - corner[1], rz = p[2] - corner[2];
        const float x = e[1] * rz - ry * e[2];
        const float y = e[2] * rx - rz * e[0];
        const float z = ry * e[0] - e[1] * rx;
        return (z * n[2] + y * n[1]) + x * n[0];
    };
    const float t0 = turn(t.a, e0), t1 = turn(t.b, e1), t2 = turn(t.c, e2);
    if (t0 > 0.0f && t1 > 0.0f && t2 > 0.0f) return true;
    const bool on0 = segmentMeetsSphere(t.a, e0, q.centre, q.radius);
    const bool on1 = segmentMeetsSphere(t.b, e1, q.centre, q.radius);
    const bool on2 = segmentMeetsSphere(t.c, e2, q.centre, q.radius);
    return on0 || on1 || on2;
}

// The sides of the plane (corner o, normal n) the point lies on, as a distance.
float above(const Vec4& p, const Vec4& o, const Vec4& n) {
    return ((p[2] - o[2]) * n[2] + (p[1] - o[1]) * n[1]) + (p[0] - o[0]) * n[0];
}

// The two corners' distances have opposite signs (the product's sign, which the x87 never loses to underflow).
bool apart(float u, float v) {
    return static_cast<double>(u) * static_cast<double>(v) < 0.0;
}

// 0x005396D0: the mesh's face a crosses the track's triangle t: where along the line of the two planes the crossings
// of a and t overlap, the point there next to a's edge, and how deep a's lowest corner lies behind t.
bool trianglesMeet(const Triangle& a, const Triangle& t, Vec4& point, float& depth) {
    const float da0 = above(a.a, t.a, t.normal), da1 = above(a.b, t.a, t.normal), da2 = above(a.c, t.a, t.normal);
    if (da0 >= 0.0f && da1 >= 0.0f && da2 >= 0.0f) return false;
    if (da0 <= 0.0f && da1 <= 0.0f && da2 <= 0.0f) return false;
    const float dt0 = above(t.a, a.a, a.normal), dt1 = above(t.b, a.a, a.normal), dt2 = above(t.c, a.a, a.normal);
    if (dt0 >= 0.0f && dt1 >= 0.0f && dt2 >= 0.0f) return false;
    if (dt0 <= 0.0f && dt1 <= 0.0f && dt2 <= 0.0f) return false;

    // The line of the two planes.
    const Vec4& an = a.normal;
    const Vec4& tn = t.normal;
    Vec4 dir{an[1] * tn[2] - an[2] * tn[1], an[2] * tn[0] - an[0] * tn[2], an[0] * tn[1] - an[1] * tn[0], 0.0f};
    const float unit = 1.0f / (std::sqrt((dir[0] * dir[0] + dir[1] * dir[1]) + dir[2] * dir[2]) + 1.1920929e-7f);
    dir = {unit * dir[0], unit * dir[1], unit * dir[2], 0.0f};

    // a's span along the line: where its edges from the lone corner cross t's plane; the first crossing's point.
    const float pa0 = (dir[0] * a.a[0] + dir[1] * a.a[1]) + dir[2] * a.a[2];
    const float pa1 = (dir[1] * a.b[1] + dir[2] * a.b[2]) + dir[0] * a.b[0];
    const float pa2 = (dir[1] * a.c[1] + dir[2] * a.c[2]) + dir[0] * a.c[0];
    bool valid = true;
    float aLow = 0.0f, aHigh = 0.0f;
    Vec4 cross{};
    if (!apart(da1, da0)) {  // a.c alone
        if (da0 != da2) {
            const float s = da0 / (da0 - da2);
            cross = add(scale(a.a, 1.0f - s), scale(a.c, s));
            aLow = (pa2 - pa0) * s + pa0;
        } else {
            valid = false;
        }
        if (da1 != da2) {
            aHigh = ((pa2 - pa1) * da1) / (da1 - da2) + pa1;
        } else {
            valid = false;
        }
    } else if (!apart(da2, da1)) {  // a.a alone
        if (da1 != da0) {
            const float s = da1 / (da1 - da0);
            cross = add(scale(a.b, 1.0f - s), scale(a.a, s));
            aLow = (pa0 - pa1) * s + pa1;
        } else {
            valid = false;
        }
        if (da0 != da2) {
            aHigh = ((pa0 - pa2) * da2) / (da2 - da0) + pa2;
        } else {
            valid = false;
        }
    } else {  // a.b alone
        if (da0 != da1) {
            const float s = da0 / (da0 - da1);
            cross = add(scale(a.a, 1.0f - s), scale(a.b, s));
            aLow = (pa1 - pa0) * s + pa0;
        } else {
            valid = false;
        }
        if (da1 != da2) {
            aHigh = ((pa1 - pa2) * da2) / (da2 - da1) + pa2;
        } else {
            valid = false;
        }
    }

    // t's span the same way; a lone corner level with another ends the test.
    const float pt0 = (dir[0] * t.a[0] + dir[2] * t.a[2]) + dir[1] * t.a[1];
    const float pt1 = (dir[2] * t.b[2] + dir[1] * t.b[1]) + dir[0] * t.b[0];
    const float pt2 = (dir[2] * t.c[2] + dir[0] * t.c[0]) + dir[1] * t.c[1];
    float tLow = 0.0f, tHigh = 0.0f;
    if (!apart(dt1, dt0)) {  // t.c alone
        if (dt0 != dt2) {
            tLow = ((pt2 - pt0) * dt0) / (dt0 - dt2) + pt0;
        } else {
            valid = false;
        }
        if (dt2 == dt1) return false;
        tHigh = ((pt2 - pt1) * dt1) / (dt1 - dt2) + pt1;
    } else if (!apart(dt2, dt1)) {  // t.a alone
        if (dt1 != dt0) {
            tLow = ((pt0 - pt1) * dt1) / (dt1 - dt0) + pt1;
        } else {
            valid = false;
        }
        if (dt0 == dt2) return false;
        tHigh = ((pt0 - pt2) * dt2) / (dt2 - dt0) + pt2;
    } else {  // t.b alone
        if (dt0 != dt1) {
            tLow = ((pt1 - pt0) * dt0) / (dt0 - dt1) + pt0;
        } else {
            valid = false;
        }
        if (dt1 == dt2) return false;
        tHigh = ((pt1 - pt2) * dt2) / (dt2 - dt1) + pt2;
    }
    if (!valid) return false;

    if (aLow > aHigh) std::swap(aLow, aHigh);
    if (tLow > tHigh) std::swap(tLow, tHigh);
    if (aHigh < tLow || tHigh < aLow) return false;
    // The middle of t's span held within a's (0x00407EC0), from the first crossing along the line.
    float middle = (tHigh + tLow) * 0.5f;
    if (middle < aLow) {
        middle = aLow;
    } else if (middle > aHigh) {
        middle = aHigh;
    }
    const float along = middle - ((cross[0] * dir[0] + cross[2] * dir[2]) + cross[1] * dir[1]);
    point = add(cross, scale(dir, along));
    float lowest = da0;
    if (da1 < lowest) lowest = da1;
    if (da2 < lowest) lowest = da2;
    depth = -lowest;
    return true;
}

// A body's mesh collider against one triangle of the track (0x0053A660, the walk 0x0053A3B0).
struct MeshPass {
    RigidBody& body;
    const Ocd& mesh;
    const Triangle& t;
    const CldZone* zone;
    float friction;
    float bounce;
    float offset;
    std::vector<uint32_t>& tested;
    std::vector<uint8_t>& marked;
    RigidBody* other = nullptr;  // the body the triangle belongs to (none for the route's)

    // The node's children whose spheres reach the triangle first, then its faces not met yet for this triangle.
    void walk(int32_t node) {
        for (int32_t c = mesh.nodes[node].child; c != -1; c = mesh.nodes[c].sibling) {
            if (sphereTouches(mesh.nodes[c], t)) walk(c);
        }
        const Ocd::Node& n = mesh.nodes[node];
        for (uint32_t i = 0; i < n.count; ++i) {
            const uint32_t f = mesh.faceMap[n.first + i];
            if (marked[f]) continue;
            marked[f] = 1;
            tested.push_back(f);
            const Ocd::Face& face = mesh.faces[f];
            Triangle a;
            a.a = mesh.vertices[face.vertices[0]];
            a.b = mesh.vertices[face.vertices[1]];
            a.c = mesh.vertices[face.vertices[2]];
            a.normal = face.normal;
            Vec4 point{};
            float depth = 0.0f;
            if (!trianglesMeet(a, t, point, depth)) continue;
            point = {t.normal[0] * offset + point[0], t.normal[1] * offset + point[1],
                     t.normal[2] * offset + point[2], point[3]};
            depth = depth + offset;
            if (!(depth > 0.0f)) continue;
            Touch touch;
            touch.id = body.def().collision.meshColliderId;
            touch.zone = zone;
            touch.friction = friction;
            touch.bounce = bounce;
            touch.point = point;
            touch.normal = t.normal;
            touch.depth = depth;
            touch.other = other;
            body.addTouch(touch);
        }
    }
};

}  // namespace

bool sphereOnPlane(const Vec4& point, const Vec4& normal, const Vec4& centre, float radius, SphereHit& hit) {
    const float dx = centre[0] - point[0];
    const float d = ((centre[2] - point[2]) * normal[2] + (centre[1] - point[1]) * normal[1]) + dx * normal[0];
    if (!(std::fabs(d) < radius)) return false;
    hit.point = {centre[0] - normal[0] * d, centre[1] - normal[1] * d, centre[2] - normal[2] * d, centre[3]};
    hit.depth = radius - d;
    return true;
}

uint32_t clipPolygon(const Vec4* in, uint32_t count, Vec4* out, const ClippingPlane& plane) {
    if (count < 3) return 0;
    const auto& o = plane.point;
    const auto& n = plane.normal;
    const float limit = (n[2] * o[2] + n[1] * o[1]) + o[0] * n[0];
    auto side = [&](const Vec4& v) { return (n[2] * v[2] + n[0] * v[0]) + n[1] * v[1]; };
    // 0x005394E0: the edge from `from` to `to` (as a segment of length 1) where it crosses the plane, in place of
    // out[made]. Stock problem: an edge parallel to the plane or crossing it past its end writes no corner, yet the
    // corner is counted: out[made] keeps what the buffer held.
    uint32_t made = 0;
    auto cross = [&](const Vec4& from, const Vec4& to) {
        const Vec4 d{to[0] - from[0], to[1] - from[1], to[2] - from[2], to[3]};
        const float towards = (n[2] * d[2] + n[1] * d[1]) + n[0] * d[0];
        if (towards != 0.0f) {
            const float dx = o[0] - from[0];
            const float t = (((o[2] - from[2]) * n[2] + (o[1] - from[1]) * n[1]) + dx * n[0]) / towards;
            if (!(t < 0.0f) && t <= 1.0f) {
                out[made] = {from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t, from[3]};
            }
        }
        ++made;
    };
    const float first = (n[2] * in[0][2] + n[1] * in[0][1]) + in[0][0] * n[0];
    bool inside = first < limit;
    for (uint32_t i = 1; i < count; ++i) {
        if (inside) out[made++] = in[i - 1];
        const float s = side(in[i]);
        if (inside) {
            if (s > limit) {
                cross(in[i - 1], in[i]);
                inside = false;
            }
        } else if (s < limit) {
            cross(in[i - 1], in[i]);
            inside = true;
        }
    }
    if (inside) out[made++] = in[count - 1];
    if (inside ? first > limit : first < limit) cross(in[count - 1], in[0]);
    return made;
}

bool sphereOnFace(const Vec4* polygon, uint32_t count, const Vec4& normal, const Vec4& centre, float radius,
                  SphereHit& hit) {
    if (count < 3) return false;
    const Vec4& n = normal;
    const Vec4& p0 = polygon[0];
    const float dx = centre[0] - p0[0];
    const float dy = centre[1] - p0[1];
    const float dz = centre[2] - p0[2];
    const float d = (dx * n[0] + dz * n[2]) + dy * n[1];
    hit.depth = radius - d;
    if (std::fabs(d) > radius) return false;
    hit.point = {centre[0] - n[0] * d, centre[1] - n[1] * d, centre[2] - n[2] * d, centre[3]};
    // The edges, each to the next corner (the last back to the first), w of the corner it goes to.
    std::vector<Vec4> edges(count);
    for (uint32_t i = 0; i < count; ++i) {
        const Vec4& a = polygon[i];
        const Vec4& b = polygon[i + 1 < count ? i + 1 : 0];
        edges[i] = {b[0] - a[0], b[1] - a[1], b[2] - a[2], b[3]};
    }
    // Inside: about the normal every edge turns towards the foot of the centre.
    bool inside = true;
    for (uint32_t i = 0; i < count && inside; ++i) {
        const Vec4& e = edges[i];
        const float rx = hit.point[0] - polygon[i][0];
        const float ry = hit.point[1] - polygon[i][1];
        const float rz = hit.point[2] - polygon[i][2];
        const float x = rz * e[1] - ry * e[2];
        const float y = rx * e[2] - rz * e[0];
        const float z = ry * e[0] - rx * e[1];
        if ((x * n[0] + z * n[2]) + y * n[1] < 0.0f) inside = false;
    }
    if (inside) return true;
    // 0x00538760 on each edge (a segment of length 1): the middle of where the edge's line meets the sphere, held
    // to the edge. Stock problem: the least distance so far starts as the count less 1 read as a float (a few
    // 1e-45), and only the hits after the first compare with it: the first edge met keeps its point.
    bool found = false;
    float nearest = std::bit_cast<float>(count - 1);
    for (uint32_t i = 0; i < count; ++i) {
        const Vec4& p = polygon[i];
        const Vec4& e = edges[i];
        float t0 = 0.0f, t1 = 0.0f;
        if (!sphereRoots(p, e, centre, radius, t0, t1)) continue;
        if (t0 < 0.0f && t1 < 0.0f) continue;
        if (t0 > 1.0f && t1 > 1.0f) continue;
        float t = (t0 + t1) * 0.5f;
        if (t < 0.0f) {
            t = 0.0f;
        } else if (t > 1.0f) {
            t = 1.0f;
        }
        const Vec4 q{p[0] + e[0] * t, p[1] + e[1] * t, p[2] + e[2] * t, p[3]};
        if (!found) {
            hit.point = q;
            found = true;
            continue;
        }
        const float qx = q[0] - centre[0], qy = q[1] - centre[1], qz = q[2] - centre[2];
        const float squared = (qz * qz + qy * qy) + qx * qx;
        if (squared < nearest) {
            nearest = squared;
            hit.point = q;
        }
    }
    return found;
}

bool sphereOnSphere(const Vec4& collider, float colliderRadius, const Vec4& collidee, float collideeRadius,
                    SphereHit& hit) {
    Vec4 v{collider[0] - collidee[0], collider[1] - collidee[1], collider[2] - collidee[2], collider[3]};
    // 0x00489910: made unit, its length before.
    const float length = std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]);
    const float unit = 1.0f / (length + 1.1920929e-7f);
    hit.normal = {unit * v[0], unit * v[1], unit * v[2], v[3]};
    const float reach = colliderRadius + collideeRadius;
    if (!(length < reach)) return false;
    hit.point = {(collider[0] + collidee[0]) * 0.5f, (collider[1] + collidee[1]) * 0.5f,
                 (collider[2] + collidee[2]) * 0.5f, collider[3]};
    hit.depth = (reach - length) * 0.5f;
    return true;
}

void TrackCollision::planePass(RigidBody& body, const Mat34& m, const WorldPlane& plane) {
    // 0x00538250: the point (0x00407F40) and the normal (0x00407D80) into the model's frame.
    const Vec4& wp = plane.point;
    const Vec4& wn = plane.normal;
    const Vec4 p{((m[2] * wp[2] + m[1] * wp[1]) + wp[0] * m[0]) + m[3],
                 ((m[4] * wp[0] + m[6] * wp[2]) + m[5] * wp[1]) + m[7],
                 ((m[8] * wp[0] + m[10] * wp[2]) + m[9] * wp[1]) + m[11], wp[3]};
    const Vec4 n{(m[2] * wn[2] + m[1] * wn[1]) + wn[0] * m[0], (m[4] * wn[0] + m[6] * wn[2]) + m[5] * wn[1],
                 (m[8] * wn[0] + m[10] * wn[2]) + m[9] * wn[1], wn[3]};
    const Vec4& c = body.boundsCentre;
    const float d = std::fabs(((c[1] - p[1]) * n[1] + n[2] * (c[2] - p[2])) + n[0] * (c[0] - p[0]));
    if (d > body.boundsRadius) return;
    for (const SphereCollider& s : body.def().collision.sphereColliders) {
        SphereHit hit;
        if (!sphereOnPlane(p, n, s.point, s.radius, hit)) continue;
        Touch touch;
        touch.id = s.id;
        touch.zone = nullptr;
        touch.friction = 0.8f;
        touch.bounce = 0.0f;
        touch.point = hit.point;
        touch.normal = n;
        touch.depth = hit.depth;
        body.addTouch(touch);
    }
    for (const LineCollider& l : body.def().collision.lines) {
        // 0x00539410
        const auto& lp = l.point;
        const auto& ld = l.direction;
        const float towards = (n[0] * ld[0] + n[1] * ld[1]) + n[2] * ld[2];
        if (!(towards < 0.0f)) continue;
        const float along = (((p[0] - lp[0]) * n[0] + (p[1] - lp[1]) * n[1]) + (p[2] - lp[2]) * n[2]) / towards;
        if (!(along >= 0.0f) || along > l.length) continue;
        Touch touch;
        touch.id = l.id;
        touch.zone = nullptr;
        touch.friction = 0.8f;
        touch.bounce = 0.0f;
        touch.point = {lp[0] + ld[0] * along, lp[1] + ld[1] * along, lp[2] + ld[2] * along, lp[3]};
        touch.normal = n;
        touch.depth = l.length - along;
        body.addTouch(touch);
    }
}

void TrackCollision::collide(RigidBody& body) {
    body.computeBounds();
    // The world into the model's frame: the inverse of the model's matrix (centimetres).
    const Mat34 m = inverseRigid(body.modelMatrix());
    for (const WorldPlane& plane : planes) planePass(body, m, plane);
    if (!m_track) {
        for (size_t i = body.nearBodies.size(); i-- > 0;) collideWith(body, *body.nearBodies[i]);
        return;
    }
    const Mat34 back = inverseRigid(m);
    const Vec4& o = body.boundsCentre;
    const float r = body.boundsRadius;
    const float x = ((back[2] * o[2] + back[1] * o[1]) + back[0] * o[0]) + back[3];
    const float z = ((back[10] * o[2] + back[9] * o[1]) + back[8] * o[0]) + back[11];
    m_cld.query(x - r, z - r, x + r, z + r, &m_faces, m_segments);

    const CollisionDef& def = body.def().collision;
    const std::vector<LineCollider>& lines = def.lines;
    const Ocd* mesh = def.colliderShape.get();
    if (mesh && m_marked.size() < mesh->faces.size()) m_marked.resize(mesh->faces.size());
    float offset = 0.0f;  // +0x8EC

    // 0x0053A660: the mesh against a triangle, when the triangle reaches the mesh's lowest corner.
    auto meshPass = [&](const Triangle& t, const CldZone* zone, float friction, float bounce) {
        if (!mesh) return;
        const float low = mesh->lowestY;
        if (t.a[1] < low && t.b[1] < low && t.c[1] < low) return;
        if (mesh->nodes.empty()) return;
        m_tested.clear();
        MeshPass pass{body, *mesh, t, zone, friction, bounce, offset, m_tested, m_marked};
        pass.walk(0);
        for (uint32_t f : m_tested) m_marked[f] = 0;
    };

    for (uint32_t f : m_faces) {
        const CldFace& face = m_cld.faces[f];
        const auto& v0 = m_cld.vertices[face.vertices[0]];
        const auto& v1 = m_cld.vertices[face.vertices[1]];
        const auto& v2 = m_cld.vertices[face.vertices[2]];
        const auto& n = face.normal;
        Triangle t;
        t.a = {((v0[2] * m[2] + v0[1] * m[1]) + v0[0] * m[0]) + m[3],
               ((v0[1] * m[5] + v0[2] * m[6]) + v0[0] * m[4]) + m[7],
               ((v0[0] * m[8] + v0[1] * m[9]) + v0[2] * m[10]) + m[11], 0.0f};
        t.b = {((v1[2] * m[2] + v1[1] * m[1]) + v1[0] * m[0]) + m[3],
               ((v1[1] * m[5] + v1[2] * m[6]) + v1[0] * m[4]) + m[7],
               ((v1[2] * m[10] + v1[1] * m[9]) + v1[0] * m[8]) + m[11], 0.0f};
        t.c = {((v2[2] * m[2] + v2[1] * m[1]) + v2[0] * m[0]) + m[3],
               ((v2[1] * m[5] + v2[2] * m[6]) + v2[0] * m[4]) + m[7],
               ((v2[2] * m[10] + v2[1] * m[9]) + v2[0] * m[8]) + m[11], 0.0f};
        t.normal = {(n[2] * m[2] + n[1] * m[1]) + n[0] * m[0], (n[1] * m[5] + n[2] * m[6]) + n[0] * m[4],
                    (n[2] * m[10] + n[1] * m[9]) + n[0] * m[8], 0.0f};
        // The zone's offset moves the surface along its normal (below -100 none).
        const CldZone& zone = m_cld.zones[face.zone];
        offset = zone.rangeMin;
        if (offset < -100.0f) offset = 0.0f;

        if (body.standing) {
            const uint32_t corners = clipFace(t.a, t.b, t.c, def.clippingPlanes);
            for (const SphereCollider& s : def.sphereColliders) {
                SphereHit hit;
                if (!sphereOnFace(m_clipA.data(), corners, t.normal, s.point, s.radius, hit)) continue;
                const Vec4 point{t.normal[0] * offset + hit.point[0], offset * t.normal[1] + hit.point[1],
                                 offset * t.normal[2] + hit.point[2], hit.point[3]};
                const float depth = offset + hit.depth;
                if (!(depth > 0.0f)) continue;
                Touch touch;
                touch.id = s.id;
                touch.zone = &zone;
                touch.friction = zone.friction;
                touch.bounce = zone.bounce;
                touch.point = point;
                touch.normal = t.normal;
                touch.depth = depth;
                body.addTouch(touch);
            }
            for (const LineCollider& l : lines) {
                Vec4 point{};
                float depth = 0.0f;
                if (!lineHits(l, t, point, depth)) continue;
                point = {t.normal[0] * offset + point[0], offset * t.normal[1] + point[1],
                         offset * t.normal[2] + point[2], point[3]};
                depth = offset + depth;
                if (!(depth > 0.0f)) continue;
                Touch touch;
                touch.id = l.id;
                touch.zone = &zone;
                touch.friction = zone.friction;
                touch.bounce = zone.bounce;
                touch.point = point;
                touch.normal = t.normal;
                touch.depth = depth;
                body.addTouch(touch);
            }
        }
        if (body.lying) meshPass(t, &zone, zone.friction, zone.bounce);
    }

    // The walls: between a marker and the next, from the zone's bottom to its top above them, two triangles facing
    // the first marker's direction. Their segments' touches take no offset.
    for (uint32_t s : m_segments) {
        const CldMarker& m0 = m_cld.markers[s];
        const CldMarker& m1 = m_cld.markers[s + 1];
        const CldZone& zone = m_cld.zones[m0.zone];
        const auto& p0 = m0.position;
        const auto& p1 = m1.position;
        const auto& n = m0.direction;
        const float bottom0 = p0[1] + zone.rangeMin, bottom1 = p1[1] + zone.rangeMin;
        const float top0 = p0[1] + zone.rangeMax, top1 = p1[1] + zone.rangeMax;

        auto first = [&](float px, float py, float pz) {
            return Vec4{((px * m[0] + pz * m[2]) + py * m[1]) + m[3], ((py * m[5] + px * m[4]) + pz * m[6]) + m[7],
                        ((px * m[8] + pz * m[10]) + py * m[9]) + m[11], 0.0f};
        };
        Triangle lower;
        lower.a = first(p0[0], bottom0, p0[2]);
        lower.b = first(p1[0], bottom1, p1[2]);
        lower.c = first(p0[0], top0, p0[2]);
        lower.normal = {(n[0] * m[0] + n[2] * m[2]) + n[1] * m[1], (n[1] * m[5] + n[0] * m[4]) + n[2] * m[6],
                        (n[0] * m[8] + n[2] * m[10]) + n[1] * m[9], 0.0f};

        auto second = [&](float px, float py, float pz) {
            return Vec4{((px * m[0] + pz * m[2]) + py * m[1]) + m[3], ((px * m[4] + pz * m[6]) + py * m[5]) + m[7],
                        ((px * m[8] + py * m[9]) + pz * m[10]) + m[11], 0.0f};
        };
        Triangle upper;
        upper.a = second(p0[0], top0, p0[2]);
        upper.b = second(p1[0], bottom1, p1[2]);
        upper.c = second(p1[0], top1, p1[2]);
        upper.normal = {(n[0] * m[0] + n[2] * m[2]) + n[1] * m[1], (n[0] * m[4] + n[2] * m[6]) + n[1] * m[5],
                        (n[0] * m[8] + n[1] * m[9]) + n[2] * m[10], 0.0f};

        if (body.standing) {
            // The sphere colliders against each triangle as it is (no clipping planes).
            for (const SphereCollider& sphere : def.sphereColliders) {
                for (const Triangle* t : {&lower, &upper}) {
                    const Vec4 corners[3] = {t->a, t->b, t->c};
                    SphereHit hit;
                    if (!sphereOnFace(corners, 3, t->normal, sphere.point, sphere.radius, hit)) continue;
                    Touch touch;
                    touch.id = sphere.id;
                    touch.zone = &zone;
                    touch.friction = zone.friction;
                    touch.bounce = zone.bounce;
                    touch.point = hit.point;
                    touch.normal = t->normal;
                    touch.depth = hit.depth;
                    body.addTouch(touch);
                }
            }
            for (const LineCollider& l : lines) {
                for (const Triangle* t : {&lower, &upper}) {
                    Vec4 point{};
                    float depth = 0.0f;
                    if (!lineHits(l, *t, point, depth)) continue;
                    Touch touch;
                    touch.id = l.id;
                    touch.zone = &zone;
                    touch.friction = zone.friction;
                    touch.bounce = zone.bounce;
                    touch.point = point;
                    touch.normal = t->normal;
                    touch.depth = depth;
                    body.addTouch(touch);
                }
            }
        }
        // Stock problem: the mesh's touches with a wall carry no zone and take the offset of the last face tested
        // (0 without faces).
        if (body.lying) {
            meshPass(lower, nullptr, zone.friction, zone.bounce);
            meshPass(upper, nullptr, zone.friction, zone.bounce);
        }
    }

    // The bodies the world found near, the last first (0x0053EC30 → 0x0053D0C0).
    for (size_t i = body.nearBodies.size(); i-- > 0;) collideWith(body, *body.nearBodies[i]);
}

void TrackCollision::collideWith(RigidBody& body, RigidBody& other) {
    other.computeBounds();
    // The other body's frame in this body's (centimetres): the inverse of this node by the other's node.
    const Mat34 r = multiply(inverseRigid(body.node), other.node);
    const Vec4 centre = transformPoint(r, other.boundsCentre);
    const Vec4& mine = body.boundsCentre;
    // 0x00539F50: the two spheres overlap.
    {
        const float dx = centre[0] - mine[0];
        const float dy = centre[1] - mine[1];
        const float dz = centre[2] - mine[2];
        const float reach = body.boundsRadius + other.boundsRadius;
        if (!(reach * reach > (dx * dx + dy * dy) + dz * dz)) return;
    }
    // A primary body wakes a body of the second list it reaches.
    if (body.def().collision.primary && !other.def().collision.primary && body.world != nullptr) {
        body.world->activate(other, true);
    }
    const CollisionDef& ours = body.def().collision;
    const CollisionDef& theirs = other.def().collision;
    const std::array<float, 4>& g = body.def().centreOfGravity;
    const float cx = g[0] * 100.0f, cy = g[1] * 100.0f, cz = g[2] * 100.0f;  // [1] 0x004F3540
    const std::vector<LineCollider>& lines = ours.lines;
    const Ocd* mesh = ours.colliderShape.get();
    if (mesh && m_marked.size() < mesh->faces.size()) m_marked.resize(mesh->faces.size());
    // A sphere collidee of the other body in this body's frame; its w the other's radius (the record's word after the
    // other's centre, 0x00407F40, left in place).
    auto collideeHere = [&](const SphereCollidee& s) {
        const auto& p = s.point;
        return Vec4{((p[0] * r[0] + r[2] * p[2]) + r[1] * p[1]) + r[3],
                    ((p[0] * r[4] + r[5] * p[1]) + r[6] * p[2]) + r[7],
                    ((r[9] * p[1] + r[10] * p[2]) + p[0] * r[8]) + r[11], other.boundsRadius};
    };
    if (body.standing) {
        for (const SphereCollider& s : ours.sphereColliders) {
            for (const SphereCollidee& c : theirs.sphereCollidees) {
                SphereHit hit;
                if (!sphereOnSphere(s.point, s.radius, collideeHere(c), c.radius, hit)) continue;
                Touch touch;
                touch.id = s.id;
                touch.zone = nullptr;
                touch.friction = c.mu;
                touch.bounce = c.restitution;
                touch.point = hit.point;
                touch.normal = hit.normal;
                touch.depth = hit.depth;
                touch.other = &other;
                body.addTouch(touch);
            }
        }
    }
    static const std::vector<Ocd::Face> kNoFaces;
    const Ocd* collidee = theirs.collideeShape.get();
    for (const Ocd::Face& face : collidee != nullptr ? collidee->faces : kNoFaces) {
        const Vec4& v0 = collidee->vertices[face.vertices[0]];
        const Vec4& v1 = collidee->vertices[face.vertices[1]];
        const Vec4& v2 = collidee->vertices[face.vertices[2]];
        const Vec4& n = face.normal;
        Triangle t;
        t.normal = {(n[2] * r[2] + n[1] * r[1]) + n[0] * r[0], (n[0] * r[4] + n[1] * r[5]) + n[2] * r[6],
                    (n[2] * r[10] + n[1] * r[9]) + n[0] * r[8], 0.0f};
        t.c = {((v2[2] * r[2] + v2[1] * r[1]) + v2[0] * r[0]) + r[3], ((v2[0] * r[4] + v2[1] * r[5]) + v2[2] * r[6]) + r[7],
               ((v2[2] * r[10] + v2[1] * r[9]) + v2[0] * r[8]) + r[11], 0.0f};
        t.b = {((v1[2] * r[2] + v1[1] * r[1]) + v1[0] * r[0]) + r[3], ((v1[0] * r[4] + v1[1] * r[5]) + v1[2] * r[6]) + r[7],
               ((v1[2] * r[10] + v1[1] * r[9]) + v1[0] * r[8]) + r[11], 0.0f};
        t.a = {((v0[0] * r[0] + v0[2] * r[2]) + v0[1] * r[1]) + r[3], ((v0[1] * r[5] + v0[0] * r[4]) + v0[2] * r[6]) + r[7],
               ((v0[2] * r[10] + v0[1] * r[9]) + v0[0] * r[8]) + r[11], 0.0f};
        // Only the faces this body's centre of gravity lies in front of.
        const float facing = ((t.a[0] - cx) * t.normal[0] + (t.a[2] - cz) * t.normal[2]) + (t.a[1] - cy) * t.normal[1];
        if (!(facing <= 0.0f)) continue;
        if (body.standing) {
            const uint32_t corners = clipFace(t.a, t.b, t.c, ours.clippingPlanes);
            for (const SphereCollider& s : ours.sphereColliders) {
                SphereHit hit;
                if (!sphereOnFace(m_clipA.data(), corners, t.normal, s.point, s.radius, hit)) continue;
                Touch touch;
                touch.id = s.id;
                touch.zone = nullptr;
                touch.friction = theirs.meshCollideeMu;
                touch.bounce = theirs.meshCollideeRestitution;
                touch.point = hit.point;
                touch.normal = t.normal;
                touch.depth = hit.depth;
                touch.other = &other;
                body.addTouch(touch);
            }
            for (const LineCollider& l : lines) {
                Vec4 point{};
                float depth = 0.0f;
                if (!lineHits(l, t, point, depth)) continue;
                Touch touch;
                touch.id = l.id;
                touch.zone = nullptr;
                touch.friction = theirs.meshCollideeMu;
                touch.bounce = theirs.meshCollideeRestitution;
                touch.point = point;
                touch.normal = t.normal;
                touch.depth = depth;
                touch.other = &other;
                body.addTouch(touch);
            }
        }
        if (body.lying && mesh && !mesh->nodes.empty()) {
            const float low = mesh->lowestY;
            if (t.a[1] < low && t.b[1] < low && t.c[1] < low) continue;
            m_tested.clear();
            MeshPass pass{body, *mesh, t, nullptr, theirs.meshCollideeMu, theirs.meshCollideeRestitution, 0.0f,
                          m_tested, m_marked, &other};
            pass.walk(0);
            for (uint32_t f : m_tested) m_marked[f] = 0;
        }
    }
    // This body's mesh collider against each sphere collidee of the other (no zone, the collidee's Mu and
    // Restitution).
    if (mesh != nullptr && body.lying) {
        for (const SphereCollidee& c : theirs.sphereCollidees) {
            meshSphere(body, *mesh, collideeHere(c), c.radius, c.mu, c.restitution, &other);
        }
    }
}

uint32_t TrackCollision::clipFace(const Vec4& a, const Vec4& b, const Vec4& c,
                                  const std::vector<ClippingPlane>& cuts) {
    // A cut makes at most twice the corners it takes; the buffers keep what they held (the stock's stack).
    auto room = [](std::vector<Vec4>& buffer, uint32_t corners) {
        if (buffer.size() < 2 * static_cast<size_t>(corners)) buffer.resize(2 * static_cast<size_t>(corners));
    };
    room(m_clipA, 3);
    m_clipA[0] = a;
    m_clipA[1] = b;
    m_clipA[2] = c;
    uint32_t inA = 3, inB = 0;
    // Stock problem: the planes cut the first buffer into the second and back, and the spheres take the first
    // buffer with its count: after an odd number of planes the last cut is not used (one plane: the face uncut).
    for (size_t i = 0; i < cuts.size(); ++i) {
        if ((i & 1) == 0) {
            room(m_clipB, inA);
            inB = clipPolygon(m_clipA.data(), inA, m_clipB.data(), cuts[i]);
        } else {
            room(m_clipA, inB);
            inA = clipPolygon(m_clipB.data(), inB, m_clipA.data(), cuts[i]);
        }
    }
    return inA;
}

void TrackCollision::walkSphere(const Ocd& mesh, int32_t node, const Vec4& centre, float radius) {
    // 0x0053A750: the children whose spheres reach the sphere first, then the node's faces not met yet.
    for (int32_t c = mesh.nodes[node].child; c != -1; c = mesh.nodes[c].sibling) {
        const Ocd::Node& q = mesh.nodes[c];
        const float dx = centre[0] - q.centre[0];
        const float dy = centre[1] - q.centre[1];
        const float dz = centre[2] - q.centre[2];
        const float reach = q.radius + radius;
        if (reach * reach > (dz * dz + dx * dx) + dy * dy) walkSphere(mesh, c, centre, radius);
    }
    const Ocd::Node& n = mesh.nodes[node];
    for (uint32_t i = 0; i < n.count; ++i) {
        const uint32_t f = mesh.faceMap[n.first + i];
        if (m_marked[f]) continue;
        m_tested.push_back(f);
        m_marked[f] = 1;
    }
}

void TrackCollision::meshSphere(RigidBody& body, const Ocd& mesh, const Vec4& centre, float radius, float friction,
                                float bounce, RigidBody* other) {
    if (mesh.nodes.empty()) return;
    if (m_marked.size() < mesh.faces.size()) m_marked.resize(mesh.faces.size());
    m_tested.clear();
    walkSphere(mesh, 0, centre, radius);
    for (uint32_t f : m_tested) {
        m_marked[f] = 0;
        const Ocd::Face& face = mesh.faces[f];
        const Vec4 corners[3] = {mesh.vertices[face.vertices[0]], mesh.vertices[face.vertices[1]],
                                 mesh.vertices[face.vertices[2]]};
        SphereHit hit;
        if (!sphereOnFace(corners, 3, face.normal, centre, radius, hit)) continue;
        // The face's normal turned round: the touch pushes this body away from the sphere. Its w is a value of the
        // stock code's stack; 0 here.
        Touch touch;
        touch.id = body.def().collision.meshColliderId;
        touch.zone = nullptr;
        touch.friction = friction;
        touch.bounce = bounce;
        touch.point = hit.point;
        touch.normal = {-face.normal[0], -face.normal[1], -face.normal[2], 0.0f};
        touch.depth = hit.depth;
        touch.other = other;
        body.addTouch(touch);
    }
}

bool TrackCollision::groundBelow(float x, float z, float& y) {
    return m_track && oj::groundBelow(m_cld, x, z, y);
}

bool groundBelow(const Cld& cld, float x, float z, float& y) {
    LineCollider line;
    line.point = {x, 1000000.0f, z, 0.0f};
    line.direction = {0.0f, -1.0f, 0.0f, 0.0f};
    line.length = 2000000.0f;
    std::vector<uint32_t> faces, segments;
    cld.query(x, z, x, z, &faces, segments);
    float best = 0.0f;
    bool hit = false;
    for (uint32_t f : faces) {
        const CldFace& face = cld.faces[f];
        Triangle t;
        t.a = cld.vertices[face.vertices[0]];
        t.b = cld.vertices[face.vertices[1]];
        t.c = cld.vertices[face.vertices[2]];
        t.normal = {face.normal[0], face.normal[1], face.normal[2], face.d};
        Vec4 p{};
        float depth = 0.0f;
        if (!lineHits(line, t, p, depth)) continue;
        if (hit && !(depth > best)) continue;
        best = depth;
        y = p[1];
        hit = true;
    }
    return hit;
}

bool groundSpan(const Cld& cld, float x, float z, float& lowest, float& highest) {
    lowest = 3.4028235e38f;
    highest = -3.4028235e38f;
    // 0x0049F9B0: the segment from (x, 1000000, z) to (x, -1000000, z), its direction made unit (0x00489910).
    LineCollider line;
    line.point = {x, 1000000.0f, z, 0.0f};
    line.length = 2000000.0f;
    const float unit = 1.0f / (line.length + 1.1920929e-7f);
    line.direction = {0.0f * unit, -2000000.0f * unit, 0.0f * unit, 0.0f};
    std::vector<uint32_t> faces, segments;
    cld.query(x, z, x, z, &faces, segments);
    bool hit = false;
    for (uint32_t f : faces) {
        const CldFace& face = cld.faces[f];
        Triangle t;
        t.a = cld.vertices[face.vertices[0]];
        t.b = cld.vertices[face.vertices[1]];
        t.c = cld.vertices[face.vertices[2]];
        t.normal = {face.normal[0], face.normal[1], face.normal[2], face.d};
        Vec4 p{};
        float depth = 0.0f;
        if (!lineHits(line, t, p, depth)) continue;
        if (p[1] < lowest) lowest = p[1];
        if (p[1] > highest) highest = p[1];
        hit = true;
    }
    return hit;
}

bool TrackCollision::firstWall(const Vec4& from, const Vec4& to, Vec4& point) {
    if (!m_track) return false;
    // 0x0049F9B0: the segment from `from` towards `to`, its direction made unit (0x00489910).
    LineCollider line;
    line.point = from;
    const float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
    line.length = std::sqrt((dx * dx + dy * dy) + dz * dz);
    const float unit = 1.0f / (line.length + 1.1920929e-7f);
    line.direction = {unit * dx, unit * dy, unit * dz, 0.0f};
    const float minX = from[0] < to[0] ? from[0] : to[0];
    const float minZ = from[2] < to[2] ? from[2] : to[2];
    const float maxX = from[0] > to[0] ? from[0] : to[0];
    const float maxZ = from[2] > to[2] ? from[2] : to[2];
    m_cld.query(minX, minZ, maxX, maxZ, nullptr, m_segments);

    // The two triangles of each wall in the world; a deeper touch is one nearer to `from`.
    float best = 0.0f;
    bool hit = false;
    for (uint32_t s : m_segments) {
        const CldMarker& m0 = m_cld.markers[s];
        const CldMarker& m1 = m_cld.markers[s + 1];
        const CldZone& zone = m_cld.zones[m0.zone];
        const auto& p0 = m0.position;
        const auto& p1 = m1.position;
        const auto& n = m0.direction;
        const float bottom0 = p0[1] + zone.rangeMin, bottom1 = p1[1] + zone.rangeMin;
        const float top0 = p0[1] + zone.rangeMax, top1 = p1[1] + zone.rangeMax;
        Triangle lower;
        lower.a = {p0[0], bottom0, p0[2], 0.0f};
        lower.b = {p1[0], bottom1, p1[2], 0.0f};
        lower.c = {p0[0], top0, p0[2], 0.0f};
        lower.normal = {n[0], n[1], n[2], 0.0f};
        Triangle upper;
        upper.a = {p0[0], top0, p0[2], 0.0f};
        upper.b = {p1[0], bottom1, p1[2], 0.0f};
        upper.c = {p1[0], top1, p1[2], 0.0f};
        upper.normal = lower.normal;
        const float facing =
            (n[2] * (p0[2] - from[2]) + (bottom0 - from[1]) * n[1]) + (p0[0] - from[0]) * n[0];
        if (!(facing <= 0.0f)) continue;
        for (const Triangle* t : {&lower, &upper}) {
            Vec4 p{};
            float depth = 0.0f;
            if (!lineHits(line, *t, p, depth) || !(depth > best)) continue;
            best = depth;
            point = p;
            hit = true;
        }
    }
    return hit;
}

}  // namespace oj
