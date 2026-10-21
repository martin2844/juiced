#pragma once
// Scene nodes and the camera of the stock engine (docs/spec/12-rendering.md).

#include "Core/Base/Matrix.h"

#include <vector>

namespace oj::render {

// A node: its matrix relative to the parent and its world matrix, made again when it is asked for after a change of
// the node or of a parent (0x00534740 marks, 0x00534790 makes).
class Node {
public:
    virtual ~Node() = default;

    Mat34 local = identity34();

    // 0x005348E0: a node without a parent becomes a child of this one.
    void attach(Node* child);
    // 0x005347F0: the node leaves its parent; its matrix becomes its world matrix.
    void detach();
    // The node and the nodes under it need their world matrix made again.
    void invalidate();
    const Mat34& world();
    Node* parent() const { return m_parent; }
    const std::vector<Node*>& children() const { return m_children; }

    // The rotation and the position become the identity (0x00534D80); the world matrix too.
    void resetMatrices();
    void setPosition(float x, float y, float z);

private:
    Mat34 m_world = identity34();
    Node* m_parent = nullptr;
    std::vector<Node*> m_children;
    bool m_dirty = true;
};

class Camera : public Node {
public:
    Camera();

    // 0x005528A0: the field of view across (radians; × 1.333 on a wide screen); with fovY 0 the height's angle comes
    // from the aspect value.
    void setFov(float fov, float fovY, bool wideScreen);
    void setAspect(float aspect) { m_aspect = aspect; }  // 0x00552890
    void setNear(float value);                            // 0x00552970
    // A camera made orthographic (0x0054CA10 with 1): setFov's values are then the half width and height.
    void setOrthographic(bool on) {
        m_orthographic = on;
        projectionChanged();
    }
    void setFar(float value);                             // 0x00552990
    float fov() const { return m_fov; }
    float nearPlane() const { return m_near; }  // 0x005529E0
    float farPlane() const { return m_far; }    // 0x005529F0
    // The scale of the parts' distance ranges (0x0054CA00): +0x1DC × the field of view's sin² × 1.5.
    float lodScale() const { return lodFactor * m_sin2; }
    float lodFactor = 1.0f;  // +0x1DC
    // [11] 0x0054C9F0: the sine of half the field of view across (+0xAC).
    float fovSin() const { return m_sin; }

    // The post effects the view's end draws for this camera (0x00516D20): the motion blur's middle (+0x224) and
    // edge (+0x228) amounts, the depth of field (+0x22C), its mode (+0x230: set to 2 with it by the views of the race's
    // camera, read by nothing) and the screen's smears (+0x234, the effects manager's SMEAR emitters, 0x005224E0).
    float blurMiddle = 0.0f;   // +0x224
    float blurEdge = 0.0f;     // +0x228
    bool depthOfField = false; // +0x22C
    int32_t depthOfFieldMode = 0; // +0x230
    bool smears = false;       // +0x234
    // [24] 0x0054CD90: the point the depth of field keeps sharp (+0x250); +0x23C the point's clip depth (z / w
    // through the world-to-clip matrix, rounded), read by nothing found. [25] 0x0054CE60 gives the point back.
    void setFocus(const Vec4& point);
    Vec4 focus{};              // +0x250
    int32_t focusDepth = 0;    // +0x23C

    // 0x00552180: the view matrix (the world matrix inverted) and the world-to-clip matrix, then the planes of the
    // view (0x005521D0).
    void update();
    // 0x00552BC0: a sphere (centre x, y, z; radius) lies at least partly inside the view; a radius of 0 never.
    bool sphereVisible(const Vec4& centre, float radius) const;
    // 0x00552A70: the same for a sphere around a point of an object's space.
    bool sphereVisible(const Mat34& object, const Vec4& centre, float radius) const;
    // 0x00552D20: a box (its low and high corners) against the side planes and the near and far distances: 0 outside
    // one of them, 1 inside all six, 2 across some.
    int boxVisible(const Vec4& low, const Vec4& high) const;
    const Mat34& view() const { return m_view; }
    const Mat44& projection() const { return m_projection; }
    const Mat44& worldToClip() const { return m_worldToClip; }

private:
    void projectionChanged();  // 0x0054CB10

    float m_fov = 0.0f;     // +0xA8
    float m_sin = 0.0f;     // +0xAC
    float m_sin2 = 0.0f;    // +0xB0
    float m_fovY = 0.0f;    // +0xB4
    float m_near = 15.0f;   // +0xB8
    float m_far = 10000.0f; // +0xBC
    float m_aspect = 0.75f; // +0xC0
    bool m_orthographic = false;  // +0x178
    Mat34 m_view = identity34();  // +0xC8
    Mat44 m_projection{};         // +0xF8
    Mat44 m_worldToClip{};        // +0x138
    // The view's side planes (0x005521D0): normals and their distances along them.
    Vec4 m_left{}, m_right{}, m_top{}, m_bottom{};  // +0x17C, +0x18C, +0x19C, +0x1AC
    float m_leftD = 0.0f, m_rightD = 0.0f, m_topD = 0.0f, m_bottomD = 0.0f;  // +0x1BC .. +0x1C8
    Vec4 m_forward{};                // +0x200: the world matrix's third column
    float m_forwardD = 0.0f;         // +0x220: forward . position
};

}  // namespace oj::render
