// Unit tests: Dynamics.

#include "Framework/Test.h"

#include "Core/Base/Matrix.h"
#include "Core/Formats/Cld.h"
#include "Core/Formats/Ocd.h"
#include "Core/Physics/Contact.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Physics/RigidBody.h"
#include "Core/Physics/TrackCollision.h"

#include <cmath>

namespace {

using oj::test::check;

}  // namespace

// HandbrakeMu (0x004F5700): atof's double compared with 100 and 10000, rounded to a float as it is stored, so
// 100.000001 (a float of 100) becomes 10000.
OJ_UNIT_TEST(Dynamics, HandbrakeMu) {
    const auto parsed = oj::parseDynamics(
        "[BeginCar]\n[BeginWheel]\nHandbrakeMu 100.000001\n[EndWheel]\n[BeginWheel]\nHandbrakeMu 100\n[EndWheel]\n"
        "[BeginWheel]\nHandbrakeMu 9999.9999999\n[EndWheel]\n[BeginWheel]\nHandbrakeMu 20000\n[EndWheel]\n[EndCar]\n");
    check(parsed && parsed->wheels.size() == 4 && parsed->wheels[0].handbrakeMu == 10000.0f &&
              parsed->wheels[1].handbrakeMu == 100.0f && parsed->wheels[2].handbrakeMu == 10000.0f &&
              parsed->wheels[3].handbrakeMu == 20000.0f,
          "dynamics: HandbrakeMu between 100 and 10000 by atof's double becomes 10000");
}
// The sphere colliders, the sphere collidees and the clipping planes of a definition (0x00536CA0, 0x00536D00,
// 0x00536DE0): the records kept between the blocks, a missing Radius 0, Centre read by no block; the body's
// sphere around them (0x005371A0).
OJ_UNIT_TEST(Dynamics, SphereColliders) {
    {
        const auto parsed = oj::parseObjectDynamics(
            "[BeginDynamicObject]\nName Ball\nMass 10\n"
            "[BeginSphereCollider]\nID 3\nPoint 1 2 3\nRadius 20\n[EndSphereCollider]\n"
            "[BeginClippingPlane]\nPoint -5 0 0\nNormal -1 0 0\n[EndClippingPlane]\nCentre 7 8 9\n"
            "[BeginSphereCollidee]\nSurfaceType GLASS\nMu 0.4\nRestitution 0.25\nPoint 0 4 0\nRadius 12\n"
            "[EndSphereCollidee]\n[BeginSphereCollidee]\nPoint 0 -4 0\nRadius\n[EndSphereCollidee]\n"
            "[BeginRigidContact]\nContactId 3\nMu 0.5\n[EndRigidContact]\n[EndDynamicObject]\n");
        bool ok = parsed && parsed->kind == oj::ObjectKind::Dynamic;
        const oj::CollisionDef& c = parsed ? parsed->body.collision : oj::CollisionDef{};
        ok = ok && c.sphereColliders.size() == 1 && c.sphereColliders[0].id == 3 &&
             c.sphereColliders[0].radius == 20.0f && c.sphereColliders[0].point[2] == 3.0f;
        ok = ok && c.clippingPlanes.size() == 1 && c.clippingPlanes[0].point[0] == -5.0f &&
             c.clippingPlanes[0].normal[0] == -1.0f && c.clippingPlanes[0].normal[1] == 0.0f;
        ok = ok && c.sphereCollidees.size() == 2 && c.sphereCollidees[0].surface == 9 &&
             c.sphereCollidees[0].mu == 0.4f && c.sphereCollidees[0].restitution == 0.25f &&
             c.sphereCollidees[0].radius == 12.0f && c.sphereCollidees[0].point[1] == 4.0f &&
             c.sphereCollidees[1].radius == 0.0f && c.sphereCollidees[1].point[1] == -4.0f &&
             c.sphereCollidees[1].mu == 0.4f && c.sphereCollidees[1].surface == 9;
        ok = ok && !oj::parseObjectDynamics("[EndSphereCollider]\n[EndSphereCollidee]\n[EndClippingPlane]\n");
        oj::BodyDef def = parsed ? parsed->body : oj::BodyDef{};
        oj::RigidBody ball(def, {});
        ball.computeBounds();
        ok = ok && ball.boundsCentre[0] == 1.0f && ball.boundsCentre[1] == 2.0f && ball.boundsCentre[2] == 3.0f &&
             ball.boundsRadius == 20.0f;
        check(ok, "dynamics: sphere colliders, sphere collidees and clipping planes read; the sphere around them");

        // 0x00539330 / 0x005395A0: a sphere against a plane (either side) and against a sphere.
        oj::SphereHit hit;
        ok = oj::sphereOnPlane({0, 0, 0, 0}, {0, 1, 0, 0}, {3, 4, 5, 0}, 10.0f, hit) && hit.point[0] == 3.0f &&
             hit.point[1] == 0.0f && hit.point[2] == 5.0f && hit.depth == 6.0f;
        ok = ok && oj::sphereOnPlane({0, 0, 0, 0}, {0, 1, 0, 0}, {0, -4, 0, 0}, 10.0f, hit) && hit.depth == 14.0f;
        ok = ok && !oj::sphereOnPlane({0, 0, 0, 0}, {0, 1, 0, 0}, {0, 12, 0, 0}, 10.0f, hit);
        ok = ok && oj::sphereOnSphere({0, 0, 0, 0}, 10.0f, {15, 0, 0, 0}, 10.0f, hit) && hit.point[0] == 7.5f &&
             hit.depth == 2.5f && std::fabs(hit.normal[0] + 1.0f) < 1e-6f && hit.normal[1] == 0.0f;
        ok = ok && !oj::sphereOnSphere({0, 0, 0, 0}, 10.0f, {25, 0, 0, 0}, 10.0f, hit);
        check(ok, "collision: a sphere against a plane and against a sphere");

        // 0x00538310: a face cut by a plane keeps what lies behind it, the crossings added; 0x005388C0: a sphere over
        // the face's inside touches at its centre's foot, one beside it at the middle of an edge's meeting with it.
        const oj::Vec4 tri[3] = {{0, 0, 0, 0}, {0, 0, 100, 0}, {100, 0, 0, 0}};
        const oj::Vec4 up{0, 1, 0, 0};
        oj::ClippingPlane cut;
        cut.point = {50, 0, 0, 0};
        cut.normal = {1, 0, 0, 0};
        oj::Vec4 clipped[6]{};
        const uint32_t corners = oj::clipPolygon(tri, 3, clipped, cut);
        ok = corners == 4 && clipped[1][2] == 100.0f && clipped[2][0] == 50.0f && clipped[2][2] == 50.0f &&
             clipped[3][0] == 50.0f && clipped[3][2] == 0.0f;
        ok = ok && oj::sphereOnFace(tri, 3, up, {10, 5, 10, 0}, 8.0f, hit) && hit.point[0] == 10.0f &&
             hit.point[1] == 0.0f && hit.depth == 3.0f;
        ok = ok && oj::sphereOnFace(tri, 3, up, {-5, 2, 50, 0}, 8.0f, hit) && hit.point[0] == 0.0f &&
             std::fabs(hit.point[2] - 50.0f) < 1e-3f && hit.depth == 6.0f;
        ok = ok && !oj::sphereOnFace(tri, 3, up, {-50, 2, 50, 0}, 8.0f, hit);
        ok = ok && oj::sphereOnFace(tri, 3, up, {70, 3, 10, 0}, 5.0f, hit) &&
             !oj::sphereOnFace(clipped, corners, up, {70, 3, 10, 0}, 5.0f, hit);
        ok = ok && oj::sphereOnFace(clipped, corners, up, {52, 2, 10, 0}, 5.0f, hit) && hit.point[0] == 50.0f &&
             std::fabs(hit.point[2] - 10.0f) < 1e-3f && hit.depth == 3.0f;
        check(ok, "collision: a face cut by a clipping plane, a sphere against a face and its edges");

        // 0x0053D0C0: a body's sphere collider against another's sphere collidee, and its mesh collider (one face
        // looking up, under a node) against it (0x0053A890): the touches with the collidee's Mu, the mesh's normal
        // turned round.
        const auto otherDef = oj::parseObjectDynamics(
            "[BeginDynamicObject]\nMass 10\n[BeginSphereCollidee]\nMu 0.5\nRestitution 0.75\nPoint 0 0 0\n"
            "Radius 30\n[EndSphereCollidee]\n[EndDynamicObject]\n");
        oj::BodyDef mine = def;
        mine.collision.sphereCollidees.clear();
        mine.collision.clippingPlanes.clear();
        mine.collision.sphereColliders[0].point = {0, 0, 0, 0};
        mine.collision.meshColliderId = 3;
        auto plate = std::make_shared<oj::Ocd>();
        plate->vertices = {{-100, 0, -100, 0}, {-100, 0, 300, 0}, {300, 0, -100, 0}};
        oj::Ocd::Face face;
        face.vertices = {0, 1, 2};
        face.normal = {0, 1, 0, 0};
        plate->faces = {face};
        plate->faceMap = {0};
        oj::Ocd::Node top, leaf;
        top.child = 1;
        top.radius = 400.0f;
        leaf.radius = 300.0f;
        leaf.count = 1;
        plate->nodes = {top, leaf};
        plate->lowestY = 0.0f;
        mine.collision.colliderShape = plate;
        oj::BodyDef theirs = otherDef ? otherDef->body : oj::BodyDef{};
        oj::RigidBody a(mine, {});
        oj::RigidBody b(theirs, {0.0f, 0.2f, 0.0f, 0.0f});
        a.computeBounds();
        a.syncNode();
        b.syncNode();
        oj::ContactPools pools;
        a.pools = &pools;
        oj::Cld none;
        oj::TrackCollision track(none, false);
        track.collideWith(a, b);
        ok = otherDef && a.rigidCount == 2 && a.contacts.size() == 2 && a.contacts[0]->other == &b &&
             std::fabs(a.contacts[0]->mu - 0.25f) < 1e-6f && std::fabs(a.contacts[0]->compression - 0.15f) < 1e-6f &&
             std::fabs(a.contacts[0]->localNormal[1] + 1.0f) < 1e-6f && a.contacts[1]->localNormal[1] == -1.0f &&
             std::fabs(a.contacts[1]->compression - 0.1f) < 1e-6f;
        check(ok, "collision: sphere collider and mesh collider against another body's sphere collidee");
    }
}
