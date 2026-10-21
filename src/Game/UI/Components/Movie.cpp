// Movie elements: the interface's movie played in an element.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

namespace oj::ui {

using namespace detail;

void Screen::playMovie(int element, const std::string& name, bool looped) {
    if (element < 0 || static_cast<size_t>(element) >= m_states.size()) return;
    State& s = m_states[static_cast<size_t>(element)];
    if (movie::System* m = movie::System::get()) m->closeCurrent();
    s.movie.name = name;
    s.movie.looped = looped;
    s.movie.volume = 1.0f;
}

movie::Player* Screen::elementMovie(int element) const {
    const movie::System* m = movie::System::get();
    if (m == nullptr || m->current == nullptr) return nullptr;
    return _stricmp(movieName(element).c_str(), m->currentName.c_str()) == 0 ? m->current : nullptr;
}

void Screen::holdMovie(int element, bool hold) {
    movie::Player* p = elementMovie(element);
    if (p == nullptr) return;
    if (hold) {
        p->restart();
    } else {
        p->held = false;
    }
}

const std::string& Screen::movieName(int element) const {
    static const std::string none;
    return element >= 0 && static_cast<size_t>(element) < m_states.size() ? m_states[static_cast<size_t>(element)].movie.name
                                                                           : none;
}

void Screen::setMovieVolume(int element, float volume) {
    if (element >= 0 && static_cast<size_t>(element) < m_states.size()) {
        m_states[static_cast<size_t>(element)].movie.volume = volume;
    }
}

// 0x005AA3A0: the interface's movie given back; unless the name is "(null)" the player of the name (0x0055A9B0,
// played once unless Looped) in the element's rectangle (its place +0xA4, +0xA8 and size +0x94, +0x98 on the screen,
// not the place its transition or InactivePos moves it to); a movie named "frontend" plays fast (+0x104).
void Screen::openMovie(size_t index, State& s) {
    movie::System* m = movie::System::get();
    if (m->current != nullptr) m->closeCurrent();
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    elementRect(m_window.elements[index], s, x0, y0, x1, y1);
    if (_stricmp(s.movie.name.c_str(), "(null)") != 0) {
        const float min[2] = {x0, y0};
        const float max[2] = {x1, y1};
        m->current = m->acquire(s.movie.name, !s.movie.looped, min, max);
    }
    if (m->current != nullptr) {
        m->current->playOnce = !s.movie.looped;
        m->currentName = s.movie.name;
        m->currentElement = static_cast<int>(index);
    }
    if (_stricmp("frontend", s.movie.name.c_str()) == 0) s.movie.playFast = true;
}

// 0x005AA4C0 after the box of a shown element: [0x0075F840] set; a movie of another name than the interface's is
// opened unless the frame holds the movies ([0x0075F064]); a player without a movie draws nothing. Its volume
// (+0x114); under a critical or network message the movie is muted and, after 4 draws, held and started again each
// draw, until no such message, nor a message of the front end's own, is up (then the volume back and the hold
// off). Stepped unless the frame holds the movies, drawn, its fast flag the element's PlayFast.
void Screen::drawMovie(size_t index, State& s) {
    movie::System* m = movie::System::get();
    if (m == nullptr) return;
    m->drawn = true;
    if (m->current == nullptr || _stricmp(s.movie.name.c_str(), m->currentName.c_str()) != 0) {
        if (m->hold) return;
        openMovie(index, s);
    }
    m->currentElement = static_cast<int>(index);
    movie::Player* p = m->current;
    if (p == nullptr || p->noMovie()) return;
    p->setVolume(s.movie.volume);
    const MovieMessages& msg = s_movieMessages;
    if (msg.critical || msg.network) {
        if (!s.movie.muting) {
            s.movie.muteFrames = 4;
            s.movie.muting = true;
        }
        if (s.movie.muteFrames > 0) --s.movie.muteFrames;
        if (s.movie.muteFrames == 0) m->current->restart();
        m->current->setVolume(0.0f);
    }
    if (s.movie.muting && !msg.critical && (!msg.message || msg.number == 0) && !msg.network) {
        s.movie.muting = false;
        m->current->setVolume(s.movie.volume);
        m->current->held = false;
    }
    if (!m->hold) m->current->advance();
    m->current->draw();
    m->current->setFast(s.movie.playFast);
}

}  // namespace oj::ui
