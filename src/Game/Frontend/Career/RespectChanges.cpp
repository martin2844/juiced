// CMSRespectChanges: the crews' respect changes after a race.

#include "Game/Frontend/Career/RespectChanges.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>

namespace oj::frontend {

namespace {

float g_changesBlink = 0.0f;  // [0x007A2D48]: 0 .. 0.3 s, the respect icon's blink
float g_countBlink = 0.0f;    // [0x0079E85C]: 0 .. 0.25 s, the type's and the privileges' blink
constexpr uint32_t kChangeUp = 0xFF32FF32;
constexpr uint32_t kChangeDown = 0xFFFF3232;

// The privileges' title and number of each kind 20 .. 27 (attend, race, illegal, host).
constexpr std::array<const char*, 4> kPrivilegeTitles = {"attendtitle", "racetitle", "illegaltitle", "hosttitle"};
constexpr std::array<const char*, 4> kPrivilegeNumbers = {"num1", "num2", "num3", "num4"};

}  // namespace

// A respect figure: "+n" in green above 0, else "n" in red (n cut toward zero).
void RespectChanges::changeText(CareerHost& /*host*/, ui::Screen& s, int element, float value, bool aboveZero) {
    if (element < 0) return;
    const int32_t n = x87::chop(value);
    s.setFontColour(element, aboveZero ? kChangeUp : kChangeDown);
    s.setText(element, toWide(aboveZero ? std::format("+{}", n) : std::format("{}", n)));
}

// 0x004CF5A0: fade1 .. 8 hidden; each crew's components comp<crew>1 and comp<crew>2 hidden, their places kept, their
// changes 0 and their start -(column x 0.1 + crew x 0.2) s; the point the components fly from: the centre of
// Respect_Icon less half a component's size; the icon blinks for 2 s; counter the respect of the race (the career's
// +0x34 object's +0x30). Each entry of the career's +0x3C list of kinds 0 .. 19 gives its crew's component (the
// second for kind 13, the rams', else the first) its change ("+n" green, a loss red; a later entry of the same
// component replaces it). Cash and Date (0x004A4130); the front end's music at a quarter. meter1 .. 8 go to
// the crews' respect / 1500 (at most 1), val1 .. 8 show it. The first entry is taken (its crew's respect shown from
// its respect now; the respect becomes that plus the change, none below 0); comment and type hidden, counter shown;
// the entries may be skipped; SEL2_ shown.
void RespectChanges::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    const CareerNotes& notes = career.respectChanges;
    for (int i = 1; i <= 8; ++i) {
        if (const int e = s.find(std::format("fade{}", i)); e >= 0) s.setHidden(e, true);
        const float row = static_cast<float>(i) * 0.2f;
        for (int col = 0; col < 2; ++col) {
            const auto at = static_cast<size_t>(i * 2 + col);
            m_changes[at] = 0.0f;
            m_progress[at] = -(static_cast<float>(col) * 0.1f + row);
            const int e = s.find(std::format("comp{}{}", i, col + 1));
            if (e < 0) continue;
            s.setHidden(e, true);
            float w = 0.0f;
            float h = 0.0f;
            s.placeOf(e, m_places[at][0], m_places[at][1], w, h);
        }
    }
    if (const int icon = s.find("Respect_Icon"), comp = s.find("comp11"); icon >= 0 && comp >= 0) {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, cx = 0.0f, cy = 0.0f, cw = 0.0f, ch = 0.0f;
        s.placeOf(icon, x, y, w, h);
        s.placeOf(comp, cx, cy, cw, ch);
        m_iconX = w * 0.5f + x - cw * 0.5f;
        m_iconY = h * 0.5f + y - ch * 0.5f;
    }
    s.setMultiline("comment", true);
    m_iconTimer = 2.0f;
    m_counter = career.news.respect;
    changeText(host, s, s.find("counter"), m_counter, m_counter > 0.0f);
    for (int32_t i = 0; i < notes.count; ++i) {
        const auto k = static_cast<size_t>(i);
        const int32_t kind = notes.kind[k];
        const int32_t crew = x87::chop(notes.b[k]);
        if (kind < 0 || kind > 19) continue;
        const int col = kind == 13 ? 1 : 0;
        // Stock problem: a crew outside 1 .. 8 reaches the record's fields before or after the components.
        if (crew < 1 || crew > 8) continue;
        m_changes[static_cast<size_t>(crew * 2 + col)] = notes.a[k];
        changeText(host, s, s.find(std::format("comp{}{}", crew, col + 1)), notes.a[k], !(notes.a[k] < 0.0f));
    }
    m_step = 0;
    m_countDelay = 2.0f;
    careerCashTexts(s, career, host.language().index, texts);
    musicQuarter(host);
    m_finished = false;
    m_first = true;
    for (size_t i = 1; i < 9; ++i) {
        const int32_t respect = career.crews[i].respect;
        s.setMeter(std::format("meter{}", i), std::min(1.0f, static_cast<float>(respect) * 0.00066666666f));
        s.setText(std::format("val{}", i), toWide(groupThousands(respect, host.language().index)));
    }
    take(host, career);
    s.setHidden("comment", true);
    s.setHidden("type", true);
    s.setHidden("counter", false);
    m_done = false;
    m_highlighted = -1;
    m_textDelay = 1.0f;
    m_canSkip = true;
    s.showContaining("SEL2_", false);
}

// The entry of the step into the record: its kind, change, crew and the crew's respect now; the crew's respect
// becomes that plus the change (none below 0).
// Stock problem: a privilege's entry (kinds 20 .. 27, 0x004BBFC0) holds its crew in the change and 0 as the crew: the
// player's crew's respect grows by that crew's number.
void RespectChanges::take(CareerHost& /*host*/, Career& career) {
    const CareerNotes& notes = career.respectChanges;
    const auto k = static_cast<size_t>(std::clamp(m_step, 0, 49));
    m_kind = notes.kind[k];
    m_change = notes.a[k];
    m_crew = notes.b[k];
    const int32_t crew = x87::chop(m_crew);
    if (crew < 0 || crew > 8) return;
    Crew& c = career.crews[static_cast<size_t>(crew)];
    m_respect = static_cast<float>(c.respect);
    float now = static_cast<float>(c.respect) + m_change;
    if (0.0f > now) now = 0.0f;
    c.respect = x87::chop(now);
}

// 0x004D1050: the entry shown: of kinds 0 .. 19 (a rise, then a fall) the fades of the other crews' rows shown,
// type "RESPECT INCREASE" or "RESPECT DECREASE", comment the crew's name and what it thinks (lines 277 + kind or
// 287 + kind - 10), green or red, the crew's component highlighted (the second for kind 13); of kinds 20 .. 27 by the
// crew of its +0x14 "GAINED PRIVILEGE" (269 + kind - 20) or "LOST PRIVILEGE" (273 + kind - 24). The crew leader's
// voice line said at once (the speech object's +0x14 with the crew rounded and 0x1E, 0x1F (0x20 for kind 13), or the
// privilege's crew and the kind + 2; none for kinds 4 and 14 of crews past 1).
void RespectChanges::show(CareerHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    m_highlighted = -1;
    const auto show = [&](float crewValue, size_t title, size_t line, uint32_t colour) {
        const int32_t crew = x87::chop(crewValue);
        for (int i = 1; i < 9; ++i) s.setHidden(std::format("fade{}", i), i == crew);
        s.setText("type", texts.at(title));
        if (const int e = s.find("type"); e >= 0) s.setFontColour(e, colour);
        if (const int e = s.find("comment"); e >= 0) s.setFontColour(e, colour);
        const size_t name = kTextCrewNames + static_cast<size_t>(std::max(crew, 0));
        s.setText("comment", texts.at(name) + texts.at(line));
        return crew;
    };
    const int32_t kind = m_kind;
    Speech* speech = gameSounds().speech;
    if (kind >= 0 && kind <= 9) {
        const size_t line = kTextRespectRise + static_cast<size_t>(kind);
        const int32_t crew = show(m_crew, kTextRespectIncrease, line, kChangeUp);
        m_highlighted = s.find(std::format("comp{}1", crew));
        if ((m_crew <= 1.0f || kind != 4) && speech != nullptr) speech->say(x87::nearest(m_crew), 0x1E);
    }
    if (kind >= 10 && kind <= 19) {
        const int32_t crew =
            show(m_crew, kTextRespectIncrease + 1, kTextRespectFall + static_cast<size_t>(kind - 10), kChangeDown);
        m_highlighted = s.find(std::format("comp{}{}", crew, kind == 13 ? 2 : 1));
        if ((m_crew <= 1.0f || kind != 14) && speech != nullptr) {
            speech->say(x87::nearest(m_crew), kind == 13 ? 0x20 : 0x1F);
        }
    }
    if (kind >= 20 && kind <= 23) {
        show(m_change, kTextRespectIncrease + 2, kTextPrivilegeGain + static_cast<size_t>(kind - 20), kChangeUp);
        if (speech != nullptr) speech->say(x87::nearest(m_change), kind + 2);
    }
    if (kind >= 24 && kind <= 27) {
        show(m_change, kTextRespectIncrease + 3, kTextPrivilegeLoss + static_cast<size_t>(kind - 24), kChangeDown);
        if (speech != nullptr) speech->say(x87::nearest(m_change), kind + 2);
    }
}

// 0x004D0D40: the next entry: counter and the highlighted component emptied; an entry of kinds 0 .. 19 leaves its
// crew's val showing the respect and its meter at it (jumping). After the last entry the fades hidden, the
// privileges' titles and numbers shown, comment, Respect_Icon and type hidden, and the window done (the voice stopped,
// 0x0040B600); else the next entry is taken, shown after 0.75 s and counted after 0.5 s (1.5 s for kinds 20 .. 27).
void RespectChanges::next(CareerHost& host, ui::Screen& s, Career& career) {
    if (const int e = s.find("counter"); e >= 0) s.clearText(e);
    if (m_highlighted >= 0) s.clearText(m_highlighted);
    if (m_kind >= 0 && m_kind <= 19) {
        const int32_t crew = x87::chop(m_crew);
        if (crew >= 0 && crew <= 8) {
            const int32_t respect = career.crews[static_cast<size_t>(crew)].respect;
            s.setText(std::format("val{}", crew), toWide(groupThousands(respect, host.language().index)));
            s.setMeter(std::format("meter{}", crew), std::min(1.0f, static_cast<float>(respect) * 0.00066666666f),
                       true);
        }
    }
    ++m_step;
    if (m_step == career.respectChanges.count) {
        host.stopSpeech();
        for (int i = 1; i < 9; ++i) s.setHidden(std::format("fade{}", i), true);
        for (size_t p = 0; p < 4; ++p) {
            s.setHidden(kPrivilegeTitles[p], false);
            s.setHidden(kPrivilegeNumbers[p], false);
        }
        s.setHidden("comment", true);
        s.setHidden("Respect_Icon", true);
        s.setHidden("type", true);
        m_done = true;
        return;
    }
    m_first = true;
    m_showDelay = 0.75f;
    take(host, career);
    m_countDelay = m_kind >= 20 && m_kind <= 27 ? 1.5f : 0.5f;
    show(host, s);
}

// 0x004D06D0: each frame of an entry: type shown, blinking (0.25 s) while it waits to be shown, with the privileges'
// titles and numbers; then the count: of kinds 0 .. 19, once its delay is over, 0.06 of the change left moves into the
// respect shown each frame (SWOOSH while more than 1 is left), and with less than 1 left the respect is the target
// (the respect and the change when the count began; a fall none below 0); counter and the highlighted component show
// the change left, val and meter the crew's respect (the meter jumping on the entry's first frame). Of kinds 20 .. 27
// the privilege's title and number blink.
void RespectChanges::count(CareerHost& host, ui::Screen& s) {
    const float dt = host.gameFrameTime();
    g_countBlink = dt + g_countBlink;
    if (g_countBlink > 0.25f) g_countBlink = g_countBlink - 0.25f;
    m_showDelay = m_showDelay - dt;
    s.setHidden("type", false);
    if (m_showDelay > 0.0f) {
        if (g_countBlink > 0.1f) s.setHidden("type", true);
        for (size_t p = 0; p < 4; ++p) {
            s.setHidden(kPrivilegeTitles[p], false);
            s.setHidden(kPrivilegeNumbers[p], false);
        }
        return;
    }
    m_countDelay = m_countDelay - dt;
    const bool rise = m_kind >= 0 && m_kind <= 9;
    const bool fall = m_kind >= 10 && m_kind <= 19;
    if (rise || fall) {
        if (m_countDelay < 0.0f) {
            const float step = m_change * 0.06f;
            m_change = m_change - step;
            m_respect = step + m_respect;
            if (std::fabs(m_change) > 1.0f) host.sound("SWOOSH");
        } else {
            m_target = m_respect + m_change;
        }
        if (std::fabs(m_change) < 1.0f) {
            if (m_target < 0.0f) m_target = 0.0f;
            m_respect = m_target;
        }
        if (fall && m_respect < 0.0f) m_respect = 0.0f;
        const int counter = s.find("counter");
        changeText(host, s, counter, m_change, m_change > 0.0f);
        if (m_highlighted >= 0 && counter >= 0) s.setText(m_highlighted, s.textOf("counter"));
        const int32_t crew = x87::chop(m_crew);
        s.setText(std::format("val{}", crew), toWide(groupThousands(x87::chop(m_respect), host.language().index)));
        s.setMeter(std::format("meter{}", crew), std::min(1.0f, m_respect * 0.00066666666f), m_first);
    }
    if (m_kind >= 20 && m_kind <= 27) {
        for (size_t p = 0; p < 4; ++p) {
            s.setHidden(kPrivilegeTitles[p], true);
            s.setHidden(kPrivilegeNumbers[p], true);
        }
        if (g_countBlink > 0.1f) {
            const auto p = static_cast<size_t>((m_kind - 20) % 4);
            s.setHidden(kPrivilegeTitles[p], false);
            s.setHidden(kPrivilegeNumbers[p], false);
        }
    } else {
        for (size_t p = 0; p < 4; ++p) {
            s.setHidden(kPrivilegeTitles[p], false);
            s.setHidden(kPrivilegeNumbers[p], false);
        }
    }
    m_first = false;
}

// 0x004D00F0: by the game's frame time. PGDN (event 6) while allowed skips: CLICK, SEL2_ hidden, every entry passed
// (0x004D0D40) and every component hidden and arrived. While the icon blinks (2 s, shown after 0.2 s of each 0.3)
// SELECT ends it: every component arrived and the first entry shown at once. Then each component, from its start,
// flies from the icon to its place over a second (shown unless the window is done); as one starts, its change is
// taken off counter (SWOOSH; after crew 8's second counter is 0). Once all have arrived counter and the icon are
// hidden; the window, when finished, empties both message lists and goes on (0x004BAEE0); else after 1 s the entry is
// shown (comment, type and counter shown, 0x004D1050), and SELECT (CLICK) passes to the next entry or, after the
// last, finishes the window (the save system's autosave over it when it is on); the entry is counted
// (0x004D06D0) until the window is done.
void RespectChanges::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    const float dt = host.gameFrameTime();
    g_changesBlink = g_changesBlink + dt;
    if (g_changesBlink > 0.3f) g_changesBlink = 0.0f;
    const auto components = [&](bool hidden) {
        for (int i = 1; i < 9; ++i) {
            for (int col = 0; col < 2; ++col) {
                s.setHidden(std::format("comp{}{}", i, col + 1), hidden);
                m_progress[static_cast<size_t>(i * 2 + col)] = 2.0f;
            }
        }
    };
    if (event == input::kSelect2 && m_canSkip) {
        host.sound("CLICK");
        m_canSkip = false;
        s.hideContaining("SEL2_");
        m_iconTimer = 0.0f;
        m_textDelay = 0.0f;
        host.stopSpeech();
        while (m_step < career.respectChanges.count) next(host, s, career);
        components(true);
        return;
    }
    if (m_iconTimer > 0.0f) {
        if (event == input::kSelect) {
            m_iconTimer = 0.0f;
            m_textDelay = 0.0f;
            s.setHidden("comment", false);
            s.setHidden("type", false);
            s.setHidden("counter", false);
            show(host, s);
            components(false);
            return;
        }
        m_iconTimer = m_iconTimer - dt;
        s.setHidden("Respect_Icon", !(g_changesBlink > 0.2f));
        return;
    }
    s.setHidden("Respect_Icon", false);
    bool arrived = true;
    for (int i = 1; i < 9; ++i) {
        for (int col = 0; col < 2; ++col) {
            const auto at = static_cast<size_t>(i * 2 + col);
            const int e = s.find(std::format("comp{}{}", i, col + 1));
            if (!m_done && e >= 0) s.setHidden(e, false);
            const float before = m_progress[at];
            m_progress[at] = dt + before;
            if (m_progress[at] > 1.0f) {
                m_progress[at] = 1.0f;
            } else {
                arrived = false;
            }
            if (before < 0.0f && !(m_progress[at] < 0.0f)) {
                m_counter = m_counter - m_changes[at];
                if (i == 8 && col == 1) m_counter = 0.0f;
                host.sound("SWOOSH");
                changeText(host, s, s.find("counter"), m_counter, m_counter > 0.0f);
            }
            const float t = m_progress[at] < 0.0f ? 0.0f : m_progress[at];
            if (e < 0) continue;
            float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
            s.placeOf(e, x, y, w, h);
            s.setPlace(e, (m_places[at][0] - m_iconX) * t + m_iconX, (m_places[at][1] - m_iconY) * t + m_iconY, w, h);
        }
    }
    if (!arrived) return;
    s.setHidden("Respect_Icon", true);
    s.setHidden("counter", true);
    if (m_finished) {
        clearNotes(career, career.respectChanges);
        clearNotes(career, career.meeting);
        host.afterRaceResults();
        return;
    }
    if (m_textDelay > 0.0f) {
        m_textDelay = m_textDelay - dt;
        if (!(m_textDelay > 0.0f)) {
            s.setHidden("comment", false);
            s.setHidden("type", false);
            s.setHidden("counter", false);
            show(host, s);
        }
        return;
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        if (m_step >= career.respectChanges.count) {
            host.stopSpeech();
            if (data->saving != 0) host.request("autosave", SwitchMode::Push);
            m_finished = true;
            return;
        }
        next(host, s, career);
    }
    if (!m_done) count(host, s);
}

// 0x004D0000: leaving CMSRespectChanges: both message lists emptied, the front end's music back, crews
// 1 .. 8's respect their respect now (+0x74) and the history's step (0x004BD8A0). Stock problem: 0x004BAEE0 has
// taken a step already; the history takes two for the event.
void RespectChanges::leave(CareerHost& host) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    clearNotes(career, career.respectChanges);
    clearNotes(career, career.meeting);
    host.setMusicVolume(host.settingsMusic(), false);
    for (size_t i = 1; i < 9; ++i) career.crews[i].respect = career.crews[i].respectNow;
    recordHistory(career);
}

}  // namespace oj::frontend
