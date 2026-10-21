#include "Render/Camera.h"

#include "Core/Base/X87.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace oj::render {

namespace {

const float kWideFov = std::bit_cast<float>(0x3FAA9FBEu);   // 1.333 (0x006FFAEC)
const float kWideFovY = std::bit_cast<float>(0x3F933333u);  // 1.15 (0x0072578C)

}  // namespace

void Node::attach(Node* child) {
    if (child->m_parent != nullptr) return;
    child->m_parent = this;
    m_children.push_back(child);
    child->invalidate();
}

void Node::detach() {
    if (m_parent == nullptr) return;
    local = world();
    auto& siblings = m_parent->m_children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    m_parent = nullptr;
    invalidate();
}

void Node::invalidate() {
    m_dirty = true;
    for (Node* c : m_children) c->invalidate();
}

const Mat34& Node::world() {
    if (m_dirty) {
        m_world = m_parent != nullptr ? multiply(m_parent->world(), local) : local;
        m_dirty = false;
    }
    return m_world;
}

void Node::resetMatrices() {
    local = identity34();
    m_world = identity34();
}

void Node::setPosition(float x, float y, float z) {
    local[3] = x;
    local[7] = y;
    local[11] = z;
    invalidate();
}

Camera::Camera() {
    setFov(100.0f, 100.0f, false);
}

void Camera::setFov(float fov, float fovY, bool wideScreen) {
    m_fov = wideScreen ? fov * kWideFov : fov;
    const double s = std::sin(static_cast<double>(m_fov));
    m_sin = static_cast<float>(s);
    m_sin2 = static_cast<float>(s * s) * 1.5f;
    if (fovY != 0.0f) {
        m_fovY = fovY;
    } else if (m_orthographic) {
        m_fovY = fov * m_aspect;
    } else {
        const float t = static_cast<float>(std::tan(static_cast<double>(m_fov * 0.5f)) * m_aspect);
        m_fovY = static_cast<float>(std::atan(static_cast<double>(t))) * 2.0f;
        if (wideScreen) m_fovY *= kWideFovY;
    }
    projectionChanged();
}

void Camera::setNear(float value) {
    m_near = value;
    projectionChanged();
}

void Camera::setFar(float value) {
    m_far = value;
    projectionChanged();
}

void Camera::projectionChanged() {
    m_projection.fill(0.0f);
    if (m_orthographic) {
        m_projection[15] = 1.0f;
        m_projection[0] = 1.0f / m_fov;
        m_projection[5] = 1.0f / m_fovY;
        m_projection[10] = 1.0f / m_far;
        m_projection[11] = -(m_projection[10] * m_near);
        return;
    }
    const float nearPlane = m_near < 15.0f ? 15.0f : m_near;
    m_projection[14] = 1.0f;
    m_projection[0] = static_cast<float>(1.0 / std::tan(static_cast<double>(m_fov * 0.5f)));
    m_projection[5] = static_cast<float>(1.0 / std::tan(static_cast<double>(m_fovY * 0.5f)));
    m_projection[10] = m_far / (m_far - nearPlane);
    m_projection[11] = -(m_projection[10] * nearPlane);
}

void Camera::setFocus(const Vec4& point) {
    focus = point;
    const Mat44& m = m_worldToClip;
    const float x = point[0], y = point[1], z = point[2];
    const float clipZ = ((m[8] * x + m[9] * y) + m[10] * z) + m[11];
    const float clipW = ((m[12] * x + m[13] * y) + m[14] * z) + m[15];
    focusDepth = x87::chop(clipZ / clipW);
}

void Camera::update() {
    m_view = inverseRigid(world());
    m_worldToClip = multiply(m_projection, m_view);
    // 0x005521D0: the columns of the world matrix (right, up, forward) and the position.
    const Mat34& w = world();
    const Vec4 right{w[0], w[4], w[8], 0.0f};
    const Vec4 up{w[1], w[5], w[9], 0.0f};
    m_forward = {w[2], w[6], w[10], 0.0f};
    const float px = w[3], py = w[7], pz = w[11];
    auto dot = [&](const Vec4& n) { return (px * n[0] + py * n[1]) + pz * n[2]; };
    m_forwardD = dot(m_forward);
    auto mix = [](const Vec4& a, float ka, const Vec4& b, float kb) {
        return Vec4{a[0] * ka + b[0] * kb, a[1] * ka + b[1] * kb, a[2] * ka + b[2] * kb, 0.0f};
    };
    if (m_orthographic) {
        m_top = up;
        m_bottom = mix(up, -1.0f, up, 0.0f);
        m_left = mix(right, -1.0f, right, 0.0f);
        m_right = right;
        m_topD = dot(m_top) + m_fovY;
        m_bottomD = dot(m_bottom) + m_fovY;
        m_leftD = dot(m_left) + m_fov;
        m_rightD = dot(m_right) + m_fov;
        return;
    }
    // The half angles' cosines and negative sines (the x87 rounds them to floats when they are stored).
    const double a = static_cast<double>(m_fov) * 0.5;
    const auto c1 = static_cast<float>(std::cos(a));
    const auto s1 = static_cast<float>(-std::sin(a));
    const double b = static_cast<double>(m_fovY) * 0.5;
    const auto c2 = static_cast<float>(std::cos(b));
    const auto s2 = static_cast<float>(-std::sin(b));
    m_top = mix(up, c2, m_forward, s2);
    m_bottom = mix(up, -c2, m_forward, s2);
    m_left = mix(right, -c1, m_forward, s1);
    m_right = mix(right, c1, m_forward, s1);
    m_topD = dot(m_top);
    m_bottomD = dot(m_bottom);
    m_leftD = dot(m_left);
    m_rightD = dot(m_right);
}

bool Camera::sphereVisible(const Vec4& c, float radius) const {
    auto dot = [&](const Vec4& n) { return (n[0] * c[0] + n[2] * c[2]) + n[1] * c[1]; };
    const float along = dot(m_forward);
    return radius != 0.0f && dot(m_right) <= m_rightD + radius && dot(m_left) <= m_leftD + radius &&
           (m_near + m_forwardD) - radius <= along && dot(m_top) <= m_topD + radius &&
           dot(m_bottom) <= m_bottomD + radius && along <= (m_far + m_forwardD) + radius;
}

bool Camera::sphereVisible(const Mat34& object, const Vec4& centre, float radius) const {
    return sphereVisible(transformPoint(object, centre), radius);
}

int Camera::boxVisible(const Vec4& low, const Vec4& high) const {
    // The box's least and greatest distance along a normal: per axis the corner's value by the normal's sign.
    auto span = [&](const Vec4& n, float& least, float& most) {
        least = 0.0f;
        most = 0.0f;
        for (size_t k = 0; k < 3; ++k) {
            const float a = low[k] * n[k];
            const float b = n[k] * high[k];
            if (n[k] > 0.0f) {
                least = k == 0 ? a : least + a;
                most = k == 0 ? b : b + most;
            } else {
                least = k == 0 ? b : b + least;
                most = k == 0 ? a : a + most;
            }
        }
    };
    const Vec4* planes[4] = {&m_left, &m_right, &m_top, &m_bottom};
    const float distances[4] = {m_leftD, m_rightD, m_topD, m_bottomD};
    int inside = 0;
    for (size_t i = 0; i < 4; ++i) {
        float least = 0.0f, most = 0.0f;
        span(*planes[i], least, most);
        if (least > distances[i]) return 0;
        if (most < distances[i]) ++inside;
    }
    float least = 0.0f, most = 0.0f;
    span(m_forward, least, most);
    const float farD = m_far + m_forwardD;
    if (least > farD) return 0;
    const float nearD = m_near + m_forwardD;
    if (most < nearD) return 0;
    if (most < farD) ++inside;
    if (least > nearD) ++inside;
    return inside == 6 ? 1 : 2;
}

}  // namespace oj::render
