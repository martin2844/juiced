#pragma once
// Functions of the stock runtime as the main thread's x87 unit computes them (single precision, round to nearest):
// the sums and products round to floats, the transcendental instructions do not.

#include <climits>
#include <cmath>
#include <cstdint>

namespace oj::x87 {

// The runtime's acos (0x006B2010): fpatan(√((1 + x)(1 − x)), x), the square root rounded to a float.
inline float acos(float x) {
    if (!(std::fabs(x) < 1.0f)) return static_cast<float>(std::acos(static_cast<double>(x)));
    const float a = 1.0f + x;
    const float b = 1.0f - x;
    const float s = std::sqrt(a * b);
    return static_cast<float>(std::atan2(static_cast<double>(s), static_cast<double>(x)));
}

// The runtime's pow (0x006B2240; the SSE2 path, in double).
inline float pow(float x, float y) {
    return static_cast<float>(std::pow(static_cast<double>(x), static_cast<double>(y)));
}

// 0x00407AE0: rand() / 32767 (a value of 0 .. 1).
inline float unit(int r) {
    return static_cast<float>(r) * 3.0518509e-5f;
}

// A float made a whole number cut toward zero (fistp under the chop mode, or cvtss2si under the game's MXCSR); out of
// range the indefinite value.
inline int32_t chop(float v) {
    if (!(v > -2147483648.0f && v < 2147483648.0f)) return INT32_MIN;
    return static_cast<int32_t>(v);
}

// A float made the nearest whole number, halves to even (0x005CA370: fistp under the round-to-nearest mode); out of
// range the indefinite value.
inline int32_t nearest(float v) {
    if (!(v > -2147483648.0f && v < 2147483648.0f)) return INT32_MIN;
    return static_cast<int32_t>(std::nearbyint(v));
}

// The same of a value the x87 unit holds in double precision (a thread other than the main one).
inline int32_t chopDouble(double v) {
    if (!(v > -2147483649.0 && v < 2147483648.0)) return INT32_MIN;
    return static_cast<int32_t>(v);
}

// A whole number loaded as the stock code loads an unsigned one (fild, plus 2^32 when negative), rounded to a float.
inline float asUnsigned(uint32_t i) {
    return static_cast<float>(static_cast<double>(i));
}

}  // namespace oj::x87
