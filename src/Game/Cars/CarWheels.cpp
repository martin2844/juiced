#include "Game/Cars/CarWheels.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace oj {

namespace {

float wrapAngle(float a) {
    const double pi = static_cast<double>(std::bit_cast<float>(0x40490FDBu));
    const double turn = 6.2831854820251465;
    if (-pi <= a) return static_cast<float>(std::fmod(a - -pi, turn) - pi);
    return static_cast<float>(std::fmod(a - -pi, turn) + pi);
}

}  // namespace

void makeWheelPoses(std::array<WheelPose, 4>& wheels, scene::CarModel& model, CarBody& body) {
    for (size_t i = 0; i < 4; ++i) {
        WheelPose& w = wheels[i];
        w.node = model.wheelNode(i);
        w.right = i == 1 || i == 3;
        if (w.node == nullptr) continue;
        const Mat34& l = w.node->local;
        w.base = {l[3], l[7], l[11], 0.0f};
        w.scale = {l[0], l[5], l[10]};
        const Mat34& g = w.node->world();
        w.last = {g[3], g[7], g[11], 0.0f};
    }
    std::array<Vec4, 4> bases{};
    for (size_t i = 0; i < 4; ++i) bases[i] = wheels[i].base;
    const std::array<float, 4> sag = body.restWheels(bases);
    for (size_t i = 0; i < 4; ++i) wheels[i].rest = -sag[i];
}

void wheelsFrame(std::array<WheelPose, 4>& wheels, CarBody& body, scene::CarModel& model, bool pulls, int32_t side,
                 bool onStand, float dt, std::array<float, 4>* speeds) {
    const float most = dt * 150.0f;
    const float turnMost = dt * 4.0f;
    for (size_t i = 0; i < 4; ++i) {
        WheelPose& w = wheels[i];
        if (w.node == nullptr) continue;
        const Mat34& g = w.node->world();
        const Vec4 now{g[3], g[7], g[11], 0.0f};
        w.moved = {now[0] - w.last[0], now[1] - w.last[1], now[2] - w.last[2], 0.0f};
        w.last = now;
        w.speed =
            std::sqrt((w.moved[2] * w.moved[2] + w.moved[1] * w.moved[1]) + w.moved[0] * w.moved[0]) * (1.0f / dt);
        if (speeds != nullptr) (*speeds)[i] = w.speed;
        CarWheel* wheel = body.wheelOf(static_cast<int32_t>(i));
        if (wheel == nullptr) continue;
        const bool weak = pulls && ((side == 0 && i == 0) || (side != 0 && i == 1));
        w.spin = w.spin - dt * wheel->spin * (weak ? 0.05f : 1.0f);
        w.spin = wrapAngle(w.spin);
        if (wheel->spin > 45.0f && !w.blurred) {
            if (!onStand) {
                model.blurWheel(i, true);
                w.blurred = true;
                if (pulls && i == (side == 0 ? 0u : 1u)) {
                    model.blurWheel(i, false);
                    w.blurred = false;
                }
            } else if (wheel->def->drive != 0) {
                model.blurWheel(i, true);
                w.blurred = true;
            }
        } else if (wheel->spin < 45.0f && w.blurred) {
            model.blurWheel(i, false);
            w.blurred = false;
        }
        if (onStand && wheel->def->drive == 0) w.spin = 0.0f;
        const float travel = std::clamp(wheel->touched * 100.0f + w.rest - w.travel, -most, most);
        w.travel = travel + w.travel;
        const float steering = body.steering;
        float target = (3.0f - (std::fabs(steering) + std::fabs(steering))) * steering;
        if (pulls && side == 0 && i == 0) target = target - 0.5f;
        if (pulls && side != 0 && i == 1) target = target + 0.5f;
        const float turn = std::clamp(target * wheel->def->maxSteerAngle - w.steer, -turnMost, turnMost);
        w.steer = turn + w.steer;
        Mat34& l = w.node->local;
        l = {w.scale[0], 0.0f, 0.0f, w.base[0], 0.0f, w.scale[1], 0.0f, w.travel + w.base[1],
             0.0f, 0.0f, w.scale[2], w.base[2]};
        float spin = w.spin;
        if (!w.right) {
            rotateY(l, -w.steer);
        } else {
            rotateY(l, wrapAngle(std::bit_cast<float>(0x40490FDBu) - w.steer));
            spin = -w.spin;
        }
        rotateX(l, spin);
        w.node->invalidate();
    }
}

}  // namespace oj
