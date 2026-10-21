#include "Core/Base/Matrix.h"

#include <cmath>

namespace oj {

namespace {

// The x87 unit keeps sin and cos at full precision until a product rounds them to a float.
float mul(double x, float m) {
    return static_cast<float>(x * static_cast<double>(m));
}

}  // namespace

Mat34 identity34() {
    return {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
}

Mat34 multiply(const Mat34& p, const Mat34& l) {
    Mat34 o{};
    o[0] = (l[0] * p[0] + p[2] * l[8]) + p[1] * l[4];
    o[1] = (l[5] * p[1] + p[2] * l[9]) + l[1] * p[0];
    o[2] = (l[10] * p[2] + p[0] * l[2]) + l[6] * p[1];
    o[3] = ((l[3] * p[0] + l[7] * p[1]) + l[11] * p[2]) + p[3];
    o[4] = (p[6] * l[8] + l[4] * p[5]) + p[4] * l[0];
    o[5] = (p[6] * l[9] + l[1] * p[4]) + l[5] * p[5];
    o[6] = (l[6] * p[5] + p[4] * l[2]) + p[6] * l[10];
    o[7] = ((p[6] * l[11] + l[7] * p[5]) + p[4] * l[3]) + p[7];
    o[8] = (p[9] * l[4] + p[8] * l[0]) + l[8] * p[10];
    o[9] = (p[9] * l[5] + p[8] * l[1]) + l[9] * p[10];
    o[10] = (p[8] * l[2] + l[10] * p[10]) + l[6] * p[9];
    o[11] = ((l[11] * p[10] + l[7] * p[9]) + p[8] * l[3]) + p[11];
    return o;
}

Mat34 inverseRigid(const Mat34& m) {
    Mat34 o{};
    o[0] = m[0];
    o[1] = m[4];
    o[2] = m[8];
    o[4] = m[1];
    o[5] = m[5];
    o[6] = m[9];
    o[8] = m[2];
    o[9] = m[6];
    o[10] = m[10];
    o[3] = -((m[7] * m[4] + m[3] * m[0]) + m[11] * m[8]);
    o[7] = -((m[11] * m[9] + m[7] * m[5]) + m[1] * m[3]);
    o[11] = -((m[10] * m[11] + m[7] * m[6]) + m[3] * m[2]);
    return o;
}

Mat44 multiply(const Mat44& a, const Mat34& b) {
    Mat44 o{};
    o[0] = (a[0] * b[0] + a[2] * b[8]) + b[4] * a[1];
    o[1] = (b[9] * a[2] + b[5] * a[1]) + a[0] * b[1];
    o[2] = (a[2] * b[10] + b[6] * a[1]) + b[2] * a[0];
    o[3] = ((b[11] * a[2] + b[7] * a[1]) + b[3] * a[0]) + a[3];
    o[4] = (a[6] * b[8] + a[4] * b[0]) + a[5] * b[4];
    o[5] = (a[5] * b[5] + a[4] * b[1]) + a[6] * b[9];
    o[6] = (a[6] * b[10] + a[5] * b[6]) + b[2] * a[4];
    o[7] = ((b[11] * a[6] + a[5] * b[7]) + b[3] * a[4]) + a[7];
    o[8] = (a[10] * b[8] + a[8] * b[0]) + b[4] * a[9];
    o[9] = (b[5] * a[9] + a[8] * b[1]) + a[10] * b[9];
    o[10] = (a[10] * b[10] + b[2] * a[8]) + b[6] * a[9];
    o[11] = ((a[8] * b[3] + a[10] * b[11]) + a[9] * b[7]) + a[11];
    o[12] = (a[13] * b[4] + a[14] * b[8]) + a[12] * b[0];
    o[13] = (a[12] * b[1] + b[9] * a[14]) + b[5] * a[13];
    o[14] = (a[13] * b[6] + a[12] * b[2]) + a[14] * b[10];
    o[15] = ((b[11] * a[14] + a[12] * b[3]) + a[13] * b[7]) + a[15];
    return o;
}

Mat44 multiplyAffine(const Mat44& a, const Mat34& b) {
    Mat44 o{};
    o[0] = (b[0] * a[0] + a[2] * b[8]) + a[1] * b[4];
    o[1] = (b[5] * a[1] + a[2] * b[9]) + b[1] * a[0];
    o[2] = (b[10] * a[2] + a[0] * b[2]) + b[6] * a[1];
    o[3] = ((b[3] * a[0] + b[7] * a[1]) + b[11] * a[2]) + a[3];
    o[4] = (a[6] * b[8] + b[4] * a[5]) + a[4] * b[0];
    o[5] = (a[6] * b[9] + b[1] * a[4]) + b[5] * a[5];
    o[6] = (b[6] * a[5] + a[4] * b[2]) + a[6] * b[10];
    o[7] = ((a[6] * b[11] + b[7] * a[5]) + a[4] * b[3]) + a[7];
    o[8] = (a[9] * b[4] + a[8] * b[0]) + b[8] * a[10];
    o[9] = (a[9] * b[5] + a[8] * b[1]) + b[9] * a[10];
    o[10] = (a[8] * b[2] + b[10] * a[10]) + b[6] * a[9];
    o[11] = ((b[11] * a[10] + b[7] * a[9]) + a[8] * b[3]) + a[11];
    o[12] = (a[13] * b[4] + a[14] * b[8]) + b[0] * a[12];
    o[13] = (b[1] * a[12] + a[14] * b[9]) + a[13] * b[5];
    o[14] = (a[14] * b[10] + a[13] * b[6]) + a[12] * b[2];
    o[15] = ((b[7] * a[13] + b[3] * a[12]) + b[11] * a[14]) + a[15];
    return o;
}

Vec4 normalizeColumns(Mat34& m) {
    Vec4 scale{};
    for (int c = 0; c < 3; ++c) {
        const float length = std::sqrt((m[c] * m[c] + m[4 + c] * m[4 + c]) + m[8 + c] * m[8 + c]);
        scale[c] = length;
        const float inverse = 1.0f / length;
        m[c] = inverse * m[c];
        m[4 + c] = inverse * m[4 + c];
        m[8 + c] = inverse * m[8 + c];
    }
    return scale;
}

void rotateY(Mat34& m, float angle) {
    const double c = std::cos(static_cast<double>(angle));
    const double sx = std::sin(static_cast<double>(angle));
    const float s = static_cast<float>(sx);
    const float n00 = mul(sx, m[2]) + mul(c, m[0]);
    const float n10 = mul(c, m[4]) + s * m[6];
    const float n20 = mul(c, m[8]) + s * m[10];
    const float n02 = -(s * m[0]) + mul(c, m[2]);
    const float n12 = -(s * m[4]) + mul(c, m[6]);
    const float n22 = -(s * m[8]) + mul(c, m[10]);
    m[0] = n00;
    m[4] = n10;
    m[8] = n20;
    m[2] = n02;
    m[6] = n12;
    m[10] = n22;
}

void rotateYParent(Mat34& m, float angle) {
    const double c = std::cos(static_cast<double>(angle));
    const double sx = std::sin(static_cast<double>(angle));
    const float s = static_cast<float>(sx);
    const float n00 = -mul(sx, m[8]) + mul(c, m[0]);
    const float n01 = -(s * m[9]) + mul(c, m[1]);
    const float n02 = -(s * m[10]) + mul(c, m[2]);
    const float n20 = mul(c, m[8]) + s * m[0];
    const float n21 = mul(c, m[9]) + s * m[1];
    const float n22 = mul(c, m[10]) + s * m[2];
    m[0] = n00;
    m[1] = n01;
    m[2] = n02;
    m[8] = n20;
    m[9] = n21;
    m[10] = n22;
}

void rotateX(Mat34& m, float angle) {
    const double c = std::cos(static_cast<double>(angle));
    const double sx = std::sin(static_cast<double>(angle));
    const float s = static_cast<float>(sx);
    const float n01 = -mul(sx, m[2]) + mul(c, m[1]);
    const float n11 = -(s * m[6]) + mul(c, m[5]);
    const float n21 = -(s * m[10]) + mul(c, m[9]);
    const float n02 = mul(c, m[2]) + s * m[1];
    const float n12 = mul(c, m[6]) + s * m[5];
    const float n22 = mul(c, m[10]) + s * m[9];
    m[1] = n01;
    m[5] = n11;
    m[9] = n21;
    m[2] = n02;
    m[6] = n12;
    m[10] = n22;
}

void rotateZ(Mat34& m, float angle) {
    const double c = std::cos(static_cast<double>(angle));
    const double sx = std::sin(static_cast<double>(angle));
    const float s = static_cast<float>(sx);
    const float n00 = -mul(sx, m[1]) + mul(c, m[0]);
    const float n10 = -(s * m[5]) + mul(c, m[4]);
    const float n20 = -(s * m[9]) + mul(c, m[8]);
    const float n01 = mul(c, m[1]) + s * m[0];
    const float n11 = mul(c, m[5]) + s * m[4];
    const float n21 = mul(c, m[9]) + s * m[8];
    m[0] = n00;
    m[4] = n10;
    m[8] = n20;
    m[1] = n01;
    m[5] = n11;
    m[9] = n21;
}

Vec4 lerp(const Vec4& a, const Vec4& b, float t) {
    return {(b[0] - a[0]) * t + a[0], (b[1] - a[1]) * t + a[1], (b[2] - a[2]) * t + a[2], a[3]};
}

Vec4 scale(const Vec4& v, float s) {
    return {v[0] * s, v[1] * s, v[2] * s, v[3]};
}

Vec4 add(const Vec4& a, const Vec4& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3]};
}

Vec4 transformPoint(const Mat34& m, const Vec4& p) {
    return {((m[2] * p[2] + m[1] * p[1]) + p[0] * m[0]) + m[3], ((m[4] * p[0] + m[6] * p[2]) + m[5] * p[1]) + m[7],
            ((m[8] * p[0] + m[10] * p[2]) + m[9] * p[1]) + m[11], 0.0f};
}

Mat34 transposeRotation(const Mat34& m) {
    return {m[0], m[4], m[8], 0.0f, m[1], m[5], m[9], 0.0f, m[2], m[6], m[10], 0.0f};
}

Mat34 inverse(const Mat34& m) {
    const float c0 = m[5] * m[10] - m[9] * m[6];
    const float c1 = m[1] * m[10] - m[9] * m[2];
    const float c2 = m[1] * m[6] - m[2] * m[5];
    const float d = 1.0f / ((c0 * m[0] - c1 * m[4]) + c2 * m[8]);
    Mat34 o{};
    o[0] = d * c0;
    o[1] = -(d * c1);
    o[2] = d * c2;
    o[4] = -((m[4] * m[10] - m[8] * m[6]) * d);
    o[5] = (m[0] * m[10] - m[8] * m[2]) * d;
    o[6] = -((m[0] * m[6] - m[2] * m[4]) * d);
    o[8] = (m[9] * m[4] - m[8] * m[5]) * d;
    o[9] = -((m[9] * m[0] - m[8] * m[1]) * d);
    o[10] = (m[5] * m[0] - m[1] * m[4]) * d;
    o[3] = -((o[2] * m[11] + o[1] * m[7]) + o[0] * m[3]);
    o[7] = -((o[6] * m[11] + o[5] * m[7]) + o[4] * m[3]);
    o[11] = -((o[10] * m[11] + o[9] * m[7]) + o[8] * m[3]);
    return o;
}

Mat34 rotationOf(const Quat& q) {
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    auto twice = [](float v) { return v + v; };
    Mat34 m{};
    m[0] = 1.0f - twice(y * y + z * z);
    m[1] = twice(x * y - w * z);
    m[2] = twice(x * z + w * y);
    m[4] = twice(x * y + w * z);
    m[5] = 1.0f - twice(x * x + z * z);
    m[6] = twice(z * y - x * w);
    m[8] = twice(x * z - w * y);
    m[9] = twice(x * w + z * y);
    m[10] = 1.0f - twice(x * x + y * y);
    return m;
}

void normalize(Quat& q) {
    const float s = 1.0f / (std::sqrt(((q[0] * q[0] + q[1] * q[1]) + q[2] * q[2]) + q[3] * q[3]) + 1.1920929e-7f);
    for (float& v : q) v = s * v;
}

float length3(const Vec4& v) {
    return std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]);
}

void normalize3(Vec4& v) {
    const float s = 1.0f / (length3(v) + 1.1920929e-7f);
    v[0] = s * v[0];
    v[1] = s * v[1];
    v[2] = s * v[2];
}

Quat product(const Quat& a, const Quat& b) {
    return {((a[0] * b[0] - b[1] * a[1]) - a[2] * b[2]) - b[3] * a[3],
            ((a[1] * b[0] + b[1] * a[0]) + a[3] * b[2]) - a[2] * b[3],
            ((a[0] * b[2] + a[1] * b[3]) + a[2] * b[0]) - b[1] * a[3],
            ((b[3] * a[0] + a[2] * b[1]) + b[0] * a[3]) - a[1] * b[2]};
}

// 0x00554830: from a toward b by t the shorter way (b negated for a negative dot product); within 1e-6 of no angle a
// straight blend.
Quat slerp(const Quat& a, const Quat& b, float t) {
    float dot = ((a[3] * b[3] + a[2] * b[2]) + a[1] * b[1]) + a[0] * b[0];
    Quat to = b;
    if (!(0.0f <= dot)) {
        dot = -dot;
        for (float& v : to) v = v * -1.0f;
    }
    const auto angle = static_cast<float>(std::acos(static_cast<double>(dot)));
    float fromScale = 1.0f - t;
    float toScale = t;
    if (!(angle > -1e-6f) || !(angle < 1e-6f)) {
        const float inverse = 1.0f / static_cast<float>(std::sin(static_cast<double>(angle)));
        fromScale = static_cast<float>(std::sin(static_cast<double>((1.0f - t) * angle))) * inverse;
        toScale = static_cast<float>(std::sin(static_cast<double>(angle * t))) * inverse;
    }
    Quat out{};
    for (int i = 0; i < 4; ++i) out[i] = a[i] * fromScale + to[i] * toScale;
    return out;
}

}  // namespace oj
