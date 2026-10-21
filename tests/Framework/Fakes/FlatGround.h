#pragma once
// A stand-in of the world for the physics tests.

#include "Core/Base/Matrix.h"
#include "Core/Physics/Contact.h"
#include "Core/Physics/RigidBody.h"

namespace oj::test {

// A flat ground at y = 0 for the checks of the physics (not the game's collision): each wheel line that reaches it
// gives a touch in the model's frame, in centimetres.
class FlatGround : public oj::Collision {
public:
    void collide(oj::RigidBody& body) override {
        const oj::Mat34 model = body.modelMatrix();
        const oj::Mat34 back = oj::inverseRigid(model);
        const oj::Vec4 up = oj::transformPoint(oj::transposeRotation(model), {0.0f, 1.0f, 0.0f, 0.0f});
        const oj::Vec4 ground = oj::transformPoint(back, {model[3], 0.0f, model[11], 0.0f});
        for (const oj::LineCollider& l : body.def().collision.lines) {
            const float towards = l.direction[0] * up[0] + l.direction[1] * up[1] + l.direction[2] * up[2];
            if (!(towards < 0.0f)) continue;
            const float t = ((ground[0] - l.point[0]) * up[0] + (ground[1] - l.point[1]) * up[1] +
                             (ground[2] - l.point[2]) * up[2]) / towards;
            if (t < 0.0f || t > l.length) continue;
            oj::Touch touch;
            touch.id = l.id;
            touch.point = {l.point[0] + l.direction[0] * t, l.point[1] + l.direction[1] * t,
                           l.point[2] + l.direction[2] * t, 0.0f};
            touch.normal = up;
            touch.depth = l.length - t;
            body.addTouch(touch);
        }
    }
};

}  // namespace oj::test
