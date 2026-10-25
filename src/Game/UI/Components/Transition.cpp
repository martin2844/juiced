// The elements' transitions (Transition, 0x005B2970 stepped by 0x005B56B0).

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

namespace oj::ui {

using namespace detail;

// 0x005B56B0: the progress, then the slide offsets or the alpha of the type; the delay runs first, then the
// duration. A finished transition rests at its end (or starts again when it loops). The diagonal slides and the
// one-sided GROW types change nothing.
void Transition::step(float dt) {
    if (remaining > 0.0f) {
        progress = std::clamp(1.0f - remaining / duration, 0.0f, 1.0f);
        const float in = 1.0f - progress;
        switch (type) {
        case kSlideLR:
            offsetY = 0.0f;
            offsetX = -(out ? width * progress : in * width);
            break;
        case kSlideRL:
            offsetY = 0.0f;
            offsetX = out ? width * progress : in * width;
            break;
        case kSlideTB:
            offsetX = 0.0f;
            offsetY = -(out ? height * progress : in * height);
            break;
        case kSlideBT:
            offsetX = 0.0f;
            offsetY = out ? height * progress : in * height;
            break;
        case kGrow:
            grow = out ? in : progress;
            [[fallthrough]];
        case kFadeIn:
            offsetX = 0.0f;
            offsetY = 0.0f;
            alpha = out ? in : progress;
            break;
        case kFlash:
            offsetY = 0.0f;
            alpha = out ? progress : in;
            flash = alpha;
            break;
        default:
            break;
        }
        if (delayLeft > 0.0f) {
            delayLeft -= dt;
            if (delayLeft <= 0.0f) delayLeft = 0.0f;
        } else {
            remaining -= dt;
        }
        return;
    }
    if (loop) {
        progress = 0.0f;
        alpha = 0.0f;
        out = false;
        remaining = duration;
        delayLeft = delay;
        return;
    }
    flash = 0.0f;
    progress = 1.0f;
    grow = out ? 0.0f : 1.0f;
    if (out) {
        alpha = 0.0f;
        offsetX = offsetY = -1000.0f;
    } else {
        alpha = 1.0f;
        offsetX = offsetY = 0.0f;
    }
}

void Screen::restartTransition(const std::string& element, bool out) {
    const int i = indexOf(element);
    if (i < 0) return;
    State& s = m_states[static_cast<size_t>(i)];
    Transition& t = s.transition;
    if (!out) {
        if (s.hasTransition) {
            t.remaining = t.duration;
            t.delayLeft = t.delay;
            t.progress = 0.0f;
            t.alpha = 0.0f;
            t.out = false;
        }
        s.hidden = false;
    } else if (!s.hasTransition) {
        s.hidden = true;
    } else if (!t.out) {
        t.out = true;
        t.remaining = t.duration;
        t.progress = 0.0f;
        t.delayLeft = 0.0f;
    }
}

void Screen::restartTransition(const std::string& element) {
    const int i = indexOf(element);
    if (i < 0) return;
    State& s = m_states[static_cast<size_t>(i)];
    if (!s.hasTransition) return;
    Transition& t = s.transition;
    t.remaining = t.duration;
    t.delayLeft = t.delay;
    t.progress = 0.0f;
    t.alpha = 0.0f;
    t.out = false;
    s.hidden = false;
}

const Transition* Screen::transitionOf(const std::string& element) const {
    const int i = indexOf(element);
    if (i < 0) return nullptr;
    const State& s = m_states[static_cast<size_t>(i)];
    return s.hasTransition ? &s.transition : nullptr;
}

void Screen::restartTransitions() {
    for (size_t i : m_order) {
        State& s = m_states[i];
        if (!s.hasTransition) continue;
        Transition& t = s.transition;
        t.remaining = t.duration;
        t.delayLeft = t.delay;
        t.progress = 0.0f;
        t.alpha = 0.0f;
        t.out = false;
        s.hidden = false;
    }
}

void Screen::setTransitionDelay(const std::string& element, float delay) {
    const int i = indexOf(element);
    if (i < 0 || !m_states[static_cast<size_t>(i)].hasTransition) return;
    Transition& t = m_states[static_cast<size_t>(i)].transition;
    t.delay = delay < 0.0f ? 0.0f : (delay <= 9999.0f ? delay : 10000.0f);
}

void Screen::finishTransitions() {
    for (State& s : m_states) {
        if (s.hasTransition) {
            if (s.transition.out) {
                s.hidden = true;
            }
            s.transition.remaining = 0.0f;
            s.transition.progress = 1.0f;
            s.transition.out = false;
        }
    }
}

bool Screen::transitionsDone() const {
    for (const State& s : m_states) {
        if (s.hasTransition && s.transition.progress < 1.0f) {
            return false;
        }
    }
    return true;
}

void endTransitions(Screen& window, const std::vector<Screen*>& open) {
    if (window.transitionsDone()) return;
    for (Screen* s : open) {
        if (s != nullptr) s->finishTransitions();
    }
    window.finishTransitions();
}

void Screen::transitionsOut() {
    for (State& s : m_states) {
        if (s.hasTransition && !s.transition.out) {
            s.transition.out = true;
            s.transition.remaining = s.transition.duration;
            s.transition.progress = 0.0f;
            s.transition.delayLeft = 0.0f;
        }
    }
}

}  // namespace oj::ui
