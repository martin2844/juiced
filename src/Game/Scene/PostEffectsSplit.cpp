// The fixed build's split screen in the post effects (Game/Scene/PostEffects.h): a view that is not the whole screen
// has its own pair of the scene's targets, of its size, and its passes at its own places from (0, 0); the back buffer
// takes the view's place as its viewport. The stock's views share the screen's pair (the previous frame the other
// view's), draw the passes at the screen's places with the view's own constants and the scene into the whole target,
// so the lower view lost its picture with the motion blur, the depth of field, a cut or the screen's smears.

#include "Game/Scene/PostEffects.h"

#include "Core/Saves/GraphicsConfig.h"

namespace oj::scene {

void PostEffects::splitBegin(const View& view, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    m_viewX = x;
    m_viewY = y;
    m_viewWidth = width;
    m_viewHeight = height;
    const bool whole = x == 0 && y == 0 && width == m_device.width() && height == m_device.height();
    if (whole || graphicsConfig().sceneWidth == 0 || m_targets[0].texture.id == 0) {
        m_split = nullptr;
        if (whole) destroySplit();
        return;
    }
    // By the view's place, not the view object: the players (and their views) are made again for every race.
    (void)view;
    ViewTargets& own = m_splitTargets[{x, y}];
    for (Target& t : own.targets) {
        if (t.texture.id != 0 && t.width == width && t.height == height) continue;
        if (t.texture.id != 0) m_device.destroyTexture(t.texture.id);
        makeTarget(t, width, height);
        m_taken = -1;
    }
    m_split = &own;
}

PostEffects::Target& PostEffects::frameTarget() {
    return m_split != nullptr ? m_split->targets[m_split->frame] : m_targets[m_frame];
}

PostEffects::Target& PostEffects::previousTarget() {
    return m_split != nullptr ? m_split->targets[1 - m_split->frame] : m_targets[1 - m_frame];
}

void PostEffects::flipFrame() {
    if (m_split != nullptr) {
        m_split->frame = 1 - m_split->frame;
    } else {
        m_frame = 1 - m_frame;
    }
}

void PostEffects::backViewport() {
    render::Viewport port;
    port.x = m_viewX;
    port.y = m_viewY;
    port.width = m_viewWidth;
    port.height = m_viewHeight;
    m_device.setViewport(port);
}

void PostEffects::destroySplit() {
    for (auto& [place, own] : m_splitTargets) {
        for (Target& t : own.targets) {
            if (t.texture.id != 0) m_device.destroyTexture(t.texture.id);
        }
    }
    if (!m_splitTargets.empty()) m_taken = -1;
    m_splitTargets.clear();
    m_split = nullptr;
}

}  // namespace oj::scene
