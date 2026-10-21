#pragma once
// The matrix arithmetic of the stock engine (docs/spec/12-rendering.md, docs/spec/16-physics.md). A Mat34 is 3 rows of 4 floats: the
// rotation in the first three columns, the position in the fourth; a Mat44 is 4 rows of 4. Every function adds and
// multiplies in the game's order, so that the floats come out the same.

#include <array>

namespace oj {

using Mat34 = std::array<float, 12>;
using Mat44 = std::array<float, 16>;
using Vec4 = std::array<float, 4>;

Mat34 identity34();

// parent × local (0x0041B520): the world matrix of a child node.
Mat34 multiply(const Mat34& parent, const Mat34& local);

// The inverse of a rotation and a position (0x0049F860): the rotation transposed, the position -Rᵀt.
Mat34 inverseRigid(const Mat34& m);

// a (4 x 4) × b (3 x 4 with the row 0 0 0 1) (0x0054CE90).
Mat44 multiply(const Mat44& a, const Mat34& b);

// The same product as the engine's object draw makes it (0x00511620), adding in another order.
Mat44 multiplyAffine(const Mat44& a, const Mat34& b);

// The lengths of the first three columns (0x00556350); the columns are divided by them in place.
Vec4 normalizeColumns(Mat34& m);

// m × a rotation about Y, X, Z by the angle (0x00534960, 0x00534A10, 0x00534AC0), in place.
void rotateY(Mat34& m, float angle);
// A rotation about Y by the angle × m (0x00534B70): the parent's axis; the position stays.
void rotateYParent(Mat34& m, float angle);
void rotateX(Mat34& m, float angle);
void rotateZ(Mat34& m, float angle);

// a + (b - a) × t on x, y, z; w of a (0x00407980).
Vec4 lerp(const Vec4& a, const Vec4& b, float t);

// v × s on x, y, z; w kept (0x00407C90).
Vec4 scale(const Vec4& v, float s);

// a + b on x, y, z; w of a (0x00407CE0).
Vec4 add(const Vec4& a, const Vec4& b);

// m × a point (x, y, z) (0x00407F40); w is a value of the stock code's stack, 0 here.
Vec4 transformPoint(const Mat34& m, const Vec4& p);

// The rotation part transposed, the position 0 (0x004F3890).
Mat34 transposeRotation(const Mat34& m);

// The inverse of a 3 x 4 matrix by its cofactors (0x004F36C0).
Mat34 inverse(const Mat34& m);

// A quaternion as the engine keeps it: w, x, y, z.
using Quat = std::array<float, 4>;

// The rotation of a unit quaternion, the position 0 (0x0041D790).
Mat34 rotationOf(const Quat& q);

// q / (|q| + 1.1920929e-7), in place (0x00429DC0).
void normalize(Quat& q);

// The length of x, y and z (0x00407C50): √((x² + y²) + z²).
float length3(const Vec4& v);

// x, y and z divided by their length + 1.1920929e-7, in place (0x00407C00).
void normalize3(Vec4& v);

// The engine's quaternion product (0x004EC490): w = a.w b.w - a.v·b.v, v = a.w b.v + b.w a.v + b.v × a.v.
Quat product(const Quat& a, const Quat& b);

// 0x00554830: from a toward b by t the shorter way (b negated for a negative dot product); within 1e-6 of no angle a
// straight blend.
Quat slerp(const Quat& a, const Quat& b, float t);

}  // namespace oj
