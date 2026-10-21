#include "Core/Race/DriverControl.h"

#include "Core/Base/X87.h"

#include <cmath>
#include <cstring>
#include <string>

namespace oj {

DriverActions DriverKeys::update(const input::Input& device, int player) {
    DriverActions a;
    a.pause = device.digital(0, player) != 0;
    a.select = device.digital(1, player) != 0;
    a.rearView = device.digital(2, player) != 0;
    a.gearDown = device.digital(3, player) != 0;
    a.gearUp = device.digital(4, player) != 0;
    a.changeView = device.digital(5, player) != 0;
    a.reset = device.digital(6, player) != 0;
    a.nitro = device.digital(7, player) != 0;
    // 0x0041E2C7: the horn's slot read as a number (unsigned) and compared with 0.2.
    a.horn = x87::asUnsigned(static_cast<uint32_t>(device.digital(8, player)));
    a.nextCar = device.digital(9, player) != 0;
    for (size_t t = 0; t < 6; ++t) a.tactics[t] = device.digital(10 + t, player) != 0;
    a.steering = device.analogue(0, player);
    a.throttle = device.analogue(1, player);
    a.brake = device.analogue(2, player);
    a.handbrake = device.analogue(3, player);
    a.reverse = device.analogue(4, player);
    a.cameraPan = device.analogue(5, player);
    return a;
}

namespace {

// 0x00407890: -1, 1 or 0 (a NaN too).
float signOf(float v) {
    if (v < 0.0f) return -1.0f;
    if (v > 0.0f) return 1.0f;
    return 0.0f;
}

}  // namespace

namespace {

// 0x00407EC0 with -1 and 1.
float within1(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// |s| to the power (0x006B2240), its sign kept; 0 left as it is.
float shaped(float s, float power) {
    if (s == 0.0f) return s;
    return s >= 0.0f || std::isnan(s)
               ? static_cast<float>(std::pow(static_cast<double>(s), static_cast<double>(power)))
               : -static_cast<float>(std::pow(static_cast<double>(-s), static_cast<double>(power)));
}

}  // namespace

float DriverControl::steerToward(float s, float speed, float dt) {
    float target = 0.0f;
    bool lessened = true;
    float power = 0.0f;
    if (wheel && multiTurn) {
        // 0x0041E436: (the sensitivity + 1) x the input, unshaped.
        target = within1((steeringSensitivity + 1.0f) * s);
        lessened = false;
    } else if (wheel && notSpeedSensitive) {
        // 0x0041E470: 1.65 ([0x007252F0]) - 1.65 x the sensitivity + 0.8 ([0x007252EC]); no lessening at speed.
        power = (1.65f - 1.65f * steeringSensitivity) + 0.8f;
        s = within1(shaped(s, power));
        target = within1(s);
        lessened = false;
    } else if (wheel) {
        // 0x0041E508: the same power ([0x007252E8], [0x007252E4]) with the lessening.
        power = (1.65f - 1.65f * steeringSensitivity) + 0.8f;
    } else {
        // 0x0041E523: 1.1 - 1.1 x the sensitivity, + 0.05 in double; 1.01 (0x0041FF7E): + 0.07 as a float
        // ([0x007272E4]).
        power = static_cast<float>(static_cast<double>(1.1f - steeringSensitivity * 1.1f) + static_cast<double>(0.07f));
    }
    if (lessened) {
        s = within1(shaped(s, power));
        float mph = speed * 2.235f;
        if (!(mph < 140.0f)) mph = 140.0f;
        target = ((1.0f - mph * 0.0071428572f) * 0.75f + 0.25f) * s;
    }
    float change = target - steer;
    float rate = s * change > 0.0f ? steerSpeed : steerSpeed * 3.0f;
    if (wheel) rate = 3000.0f;
    if (rate * dt < std::fabs(change)) change = signOf(change) * rate * dt;
    steer = change + steer;
    if (target * steer < 0.0f) steer = 0.0f;
    return steer;
}

void DriverControl::apply(CarBody& car, const DriverActions& a, float dt, float count) {
    if (!(count < 10.0f)) return;
    // The pedals: past 0.2 the throttle squared, the brake cubed.
    car.throttle = a.throttle > 0.2f ? a.throttle * a.throttle : 0.0f;
    car.brake = a.brake > 0.2f ? a.brake * a.brake * a.brake : 0.0f;

    // The steering (0x00407E60 / 0x00407C50: the speed, the local velocity's length), times SteeringFactor and the
    // car's sensitivity.
    const Vec4& v = car.localVelocity;
    const float speed = std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]);
    steerToward(a.steering, speed, dt);
    float turn = car.dynamics().steeringFactor * sensitivity * steer;
    if (turn < -1.0f) {
        turn = -1.0f;
    } else if (turn > 1.0f) {
        turn = 1.0f;
    }
    car.steering = turn;

    // The gears: by hand GEAR_DOWN / GEAR_UP; the automatic box takes reverse on REVERSE (its throttle squared),
    // neutral on the throttle while standing on the brake, first gear on the throttle from reverse or neutral.
    shiftedUp = false;
    car.automatic = automaticGearbox;
    if (car.automatic) {
        if (a.reverse > 0.2f) {
            car.shift(0);
            car.throttle = a.reverse * a.reverse;
        } else if (a.throttle > 0.2f) {
            if (std::fabs(car.localVelocity[2]) < 0.1f && car.brake > 0.9f) {
                car.shift(1);
            } else if (car.gear < 2) {
                car.shift(2);
            }
        }
    } else {
        const uint32_t gear = car.gear;
        const auto gears = static_cast<uint32_t>(car.dynamics().forwardRatios.size() + 2);
        if (a.gearDown) {
            if (gear > 0) car.shift(gear - 1);
        } else if (a.gearUp && gear < gears - 1) {
            if (gear > 1) shiftedUp = true;
            car.shift(gear + 1);
        }
    }

    car.handbrake = a.handbrake > 0.2f ? 1.0f : 0.0f;
    if (count != 0.0f) car.handbrake = 1.0f;
}

}  // namespace oj
