#pragma once
// An element's transition: how it comes in or goes away (slides, fades, grows, flashes).

namespace oj::ui {

// A Transition of an element (0x005B2970, stepped by 0x005B56B0).
struct Transition {
    enum Type {
        kNone = 0,
        kSlideLR = 1, kSlideRL = 2, kSlideTB = 3, kSlideBT = 4,
        kSlideLRBT = 5, kSlideRLBT = 6, kSlideLRTB = 7, kSlideRLTB = 8,
        kFadeIn = 9,
        kGrow = 10, kGrowHori = 11, kGrowVert = 12, kGrowUp = 13, kGrowDown = 14, kGrowLeft = 15, kGrowRight = 16,
        kFlash = 17,
    };
    int type = kNone;
    bool loop = false;
    bool out = false;          // the element goes away
    float duration = 0.0f;
    float remaining = 0.0f;
    float delay = 0.0f;
    float delayLeft = 0.0f;
    float width = 0.0f;        // the screen, the distance of the slides
    float height = 0.0f;
    float progress = 0.0f;
    float grow = 1.0f;         // GROW: the share of the element's size
    float alpha = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float flash = 0.0f;        // FALSH: the alpha of the white flash

    void step(float dt);
    bool done() const { return progress >= 1.0f; }
};

}  // namespace oj::ui
