// The session windows of the matchmaking (docs/spec/28-network.md, "The session windows"): online_bg,
// online_mode, online_arcade_create and _search, online_career_create and _search, and the results
// (online_arcade_results, online_career_results).

#include "Game/Frontend/Network/OnlineSessionWindows.h"

#include "Core/Career/Career.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Data/JetTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace oj::frontend {

namespace {

constexpr uint32_t kYellow = 0xFFECC003;  // an option's text (its +0x11C and +0x150)
constexpr uint32_t kRed = 0xFFBC381D;     // an option that cannot be had

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

std::wstring wide(std::u16string_view text) {
    return std::wstring(text.begin(), text.end());
}

std::wstring latin(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

// swprintf("%d ") and the text "bhp" (the classes' shown values).
std::wstring bhp(int32_t value, const std::wstring& word) {
    return std::format(L"{} ", value) + word;
}

}  // namespace

// The text of an option and its colours (yellow, red when it cannot be had).
void onlineOption(ui::Screen& s, const std::string& element, const std::wstring& text, bool invalid) {
    s.setText(element, text);
    const uint32_t colour = invalid ? kRed : kYellow;
    s.setFontColours(element, colour, colour);
}

// 0x004739C0: the base enter, then the window named in [0x0072182C] opened over it (mode 5).
void OnlineBg::enter(WindowHost& host) {
    host.request(host.shared().onlineForms.background, SwitchMode::Push);
}

void OnlineMode::enter() {}

// 0x00473A30. BACKMENU follows MENU. SELECT on an item (CLICK): [0x0075EF88] = the item not "arcade" (0 arcade, 1 career);
// the career without a career save ([[game + 0x7C]] 0) "You must have a Career Mode save game", with an empty garage
// "You don't have any cars in your garage" (type 1; the window kept); else online_main's choice quick (0) or search
// (1) the search window (mode 1), create (2) the create window over online_bg. BACK: CLICK, online_main.
void OnlineMode::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (s.find("MENU") >= 0 && s.find("BACKMENU") >= 0) s.selectMenuItem("BACKMENU", s.selectedMenuItem("MENU"));
    if (event == input::kSelect) {
        if (s.find("MENU") < 0 || s.menuSize("MENU") == 0) return;
        const std::string item = s.menuItemCommand("MENU", s.selectedMenuItem("MENU"));
        host.sound("CLICK");
        const net::OnlineKind mode = same(item, "arcade") ? net::OnlineKind::Arcade : net::OnlineKind::Career;
        net::onlineHosted().mode = mode;
        if (mode == net::OnlineKind::Career) {
            const Career* career = data != nullptr ? data->career.get() : nullptr;
            if (career == nullptr || career->head == 0) {
                host.message(MessageType::Continue, texts.at(kTextNoCareer), false, nullptr);
                return;
            }
            if (career->garage.empty()) {
                host.message(MessageType::Continue, texts.at(kTextNoCars), false, nullptr);
                return;
            }
        }
        if (host.shared().onlineForms.mode == OnlineChoice::Quick ||
            host.shared().onlineForms.mode == OnlineChoice::Search) {
            host.request(mode == net::OnlineKind::Arcade ? "online_arcade_search" : "online_career_search",
                         SwitchMode::Replace);
        } else {
            host.shared().onlineForms.background =
                mode == net::OnlineKind::Arcade ? "online_arcade_create" : "online_career_create";
            host.request("online_bg", SwitchMode::Replace);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("online_main", SwitchMode::Replace);
    }
}

// The session joined (0x004734E0 answered 1) waited for: once no message moves in, the session lost (0x00471860)
// stops the waiting; in the session (the transport's +0xE4) the message closed and link_lobby. True while waiting.
bool onlineJoinWait(NetworkHost& host, int32_t& joining) {
    if (joining == 0) return false;
    if (host.onlineTopMoving()) return true;
    if (host.netDropped()) {
        joining = 0;
        return true;
    }
    net::NetGame* net = net::netGame();
    if (net != nullptr && net->transport.inSession()) {
        joining = 0;
        host.closeMessage();
        host.request("link_lobby", SwitchMode::Replace);
    }
    return true;
}

// The searches' quick match (0x00475360 / 0x00476F90 with +0x20/+0x30): once no message moves in, the timer runs while
// some results are valid (0x004E6300; 0 while none); past 3 s (0x006FF7E8) or with the details in (MM+0x10) the search
// stopped, the results sorted (0x005645E0) and the first acceptable one (0x004E6360) joined (joining on 1), or with none
// the question "No game sessions found. Do you want to create one?" (type 3) answered into `answer`.
void onlineQuickWait(NetworkHost& host, int32_t& quick, int32_t& joining, float& timer, int32_t& answer, bool career) {
    (void)career;
    if (host.onlineTopMoving()) return;
    timer = net::countValidResults() == 0 ? 0.0f : host.frameTime() + timer;
    net::Matchmaking* mm = net::matchmaking();
    const bool details = mm != nullptr && mm->detailsIn != 0;
    if (!(3.0f < timer) && !details) return;
    quick = 0;
    if (mm != nullptr) {
        mm->stopSearch();
        mm->sortResults();
    }
    const int32_t position = net::quickMatchResult();
    if (position != -1) {
        if (host.onlineJoinSession(position)) joining = 1;
        return;
    }
    host.closeMessage();
    host.message(MessageType::YesNo, host.jetText(kJetCreateOne), false, &answer, false);
}

// 0x00474770: ACCESS Public / Private; LEVEL Any or GROUP G .. A, ALL CARS; MODS NONE / STANDARD / PROTOTYPE;
// COLLISION NO / YES.
void OnlineArcadeCreate::show(NetworkHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    const net::ArcadeSession& a = m_settings;
    onlineOption(s, "ACCESS", texts.at(a.access == 0 ? kTextPublic : kTextPrivate));
    onlineOption(s, "LEVEL", texts.at(a.level == net::kAny ? kTextAny : kTextGroups + static_cast<size_t>(a.level)));
    onlineOption(s, "MODS", texts.at(kTextMods + static_cast<size_t>(a.mods)));
    onlineOption(s, "COLLISION", texts.at(a.collision == 0 ? kTextNo : kTextYes));
}

// 0x00474840: every element shown; the settings a friend's session's (details, [0x0075EEA4]: 0x004E64F0 of the
// position 0) or the profile's (+0x270); shown; ARROW1, ARROW2 and CHANGETEXT hidden; the details hide SEL_TEXT,
// SEL_BUT and B_SUBMIT and title "DETAILS"; creating and started 0; every "CARMENU" element hidden, its level -1.
void OnlineArcadeCreate::enter(NetworkHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    s.showContaining("", false);
    if (host.shared().onlineForms.details != 0) {
        net::readArcade(0, m_settings);
    } else {
        m_settings = net::ArcadeSession::read(host.optionSettings());
    }
    show(host, s);
    s.setHidden("ARROW1", true);
    s.setHidden("ARROW2", true);
    s.setHidden("CHANGETEXT", true);
    if (host.shared().onlineForms.details != 0) {
        s.setHidden("SEL_TEXT", true);
        s.setHidden("SEL_BUT", true);
        s.setHidden("B_SUBMIT", true);
        s.setText("TITLE", texts.at(kTextDetails));
    }
    m_creating = 0;
    m_hostStarted = 0;
    s.hideContaining("CARMENU", false);
    m_carMenuHidden = 1;
    m_carMenuLevel = -1;
}

// 0x00474A30. Details: B_SUBMIT kept highlighted, the options untouched, BACK (CLICK) online_friends (mode 7).
// Creating, once no message moves in: the host started (0x004E1AE0; failing: not creating, CLICK, "Connection to network
// lost. Please try again." type 1), then once the matchmaking knows our public address (+0x08) the message closed and
// the host into link_lobby (0x004739A0) — no time limit. CARMENU shown (SWOOSH) while B_LEVEL is highlighted, hidden
// (SWOOSH) else; refilled when the level changes (+0x50): 7 "ALL CARS" highlighted, Any emptied only, else the group's
// cars ([G+0x84]), the middle one highlighted and the menu taking no events; a list made deletes the menu's text
// modifier (its highlighted item does not blink). LEFT / RIGHT on an option: ACCESS and COLLISION turned, LEVEL
// stepped (Any, G .. A, ALL CARS), MODS 0 .. 2 round; CLICK and shown. UP / DOWN: on B_SUBMIT SEL_TEXT and SEL_BUT,
// else ARROW1, ARROW2 and CHANGETEXT. SELECT on B_SUBMIT: everything reset (0x004EA4E0 with 1), the settings kept in the
// profile (+0x270), creating, CLICK, "CREATING GAME..." (type 0). BACK (CLICK): online_main.
void OnlineArcadeCreate::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const bool details = host.shared().onlineForms.details != 0;
    if (details) {
        s.setCurrentButton("B_SUBMIT");
        if (event == input::kBack) {
            host.sound("CLICK");
            host.request("online_friends", SwitchMode::Swap);
        }
        return;
    }
    if (m_creating != 0) {
        if (host.onlineTopMoving() || !s.transitionsDone()) return;
        if (m_hostStarted == 0) {
            if (!net::hostArcade(m_settings)) {
                m_creating = 0;
                host.sound("CLICK");
                host.message(MessageType::Continue, host.jetText(kJetNetworkLost), false, nullptr);
                return;
            }
            m_hostStarted = 1;
        }
        const net::Matchmaking* mm = net::matchmaking();
        if (mm != nullptr && mm->publicAddressKnown != 0) {
            m_creating = 0;
            host.closeMessage();
            host.onlineHostIntoLobby();
        }
        return;
    }
    const std::string focus = s.currentButton();
    const bool onLevel = same(focus, "B_LEVEL");
    if (onLevel && m_carMenuHidden != 0) {
        s.showContaining("CARMENU", true);
        host.sound("SWOOSH");
        m_carMenuHidden = 0;
    } else if (!onLevel && m_carMenuHidden == 0) {
        s.hideContaining("CARMENU", true);
        host.sound("SWOOSH");
        m_carMenuHidden = 1;
    }
    if (m_settings.level != m_carMenuLevel) {
        m_carMenuLevel = m_settings.level;
        if (s.find("CARMENU") >= 0 && data != nullptr) {
            s.clearMenu("CARMENU");
            if (m_settings.level == 7) {
                // 0x00474BF8: the one item, the text modifier deleted (0x005AF170), the item highlighted; the menu
                // takes events as it did.
                s.addMenuItem("CARMENU", texts.at(kTextAllCars), 0.0f);
                s.removeMenuModifier("CARMENU");
                s.selectMenuItem("CARMENU", 0);
            } else if (m_settings.level != net::kAny &&
                       static_cast<size_t>(m_settings.level) < data->arcadeGroups.ids.size()) {
                for (int32_t id : data->arcadeGroups.ids[static_cast<size_t>(m_settings.level)]) {
                    if (id == 0) break;
                    const CarDef* def = data->defs.byId(id);
                    s.addMenuItem("CARMENU", def != nullptr ? latin(def->displayName) : std::wstring(), 0.0f);
                }
                // 0x00474CB6: with items, the middle one highlighted, the text modifier deleted (0x005AF170) and the
                // menu taking no events (+0x178 0).
                if (const size_t items = s.menuSize("CARMENU"); items != 0) {
                    s.selectMenuItem("CARMENU", items / 2);
                    s.removeMenuModifier("CARMENU");
                    s.setMenuActive("CARMENU", false);
                }
            }
        }
    }
    if (event == input::kLeft || event == input::kRight) {
        const int32_t dir = event == input::kLeft ? -1 : 1;
        net::ArcadeSession& a = m_settings;
        if (same(focus, "B_ACCESS")) {
            a.access = a.access == 0 ? 1 : 0;
        } else if (onLevel) {
            a.level = net::stepLevel(a.level, dir);
        } else if (same(focus, "B_MODS")) {
            a.mods = net::stepCreateMods(a.mods, dir);
        } else if (same(focus, "B_COLLISION")) {
            a.collision = a.collision == 0 ? 1 : 0;
        }
        host.sound("CLICK");
        show(host, s);
    } else if (event == input::kUp || event == input::kDown) {
        const bool submit = same(focus, "B_SUBMIT");
        s.setHidden("SEL_TEXT", !submit);
        s.setHidden("SEL_BUT", !submit);
        s.setHidden("ARROW1", submit);
        s.setHidden("ARROW2", submit);
        s.setHidden("CHANGETEXT", submit);
    } else if (event == input::kSelect) {
        if (!same(focus, "B_SUBMIT")) return;
        if (net::NetGame* net = net::netGame()) net->session.reset(true);
        m_settings.write(host.optionSettings());
        m_hostStarted = 0;
        m_creating = 1;
        host.sound("CLICK");
        host.message(MessageType::Note, texts.at(kTextCreating), false, nullptr);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("online_main", SwitchMode::Replace);
    }
}

// 0x004761B0 (both create windows): [0x0075EEA4] 0; "Menu"'s highlight kept when it is a menu.
void leaveOnlineCreate(WindowHost& host) {
    host.shared().onlineForms.details = 0;
}

// 0x00475120: MIN_PLAYERS "%d"; STATUS Any / Waiting / Racing; LEVEL Any or GROUP ..; MODS Any / NONE / STANDARD /
// PROTOTYPE; COLLISION Any / NO / YES.
void OnlineArcadeSearch::show(NetworkHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    const net::ArcadeSearch& a = m_settings;
    onlineOption(s, "MIN_PLAYERS", std::format(L"{}", a.minPlayers));
    onlineOption(s, "STATUS", texts.at(a.status == net::kAny ? kTextAny : a.status == 0 ? kTextWaiting : kTextRacing));
    onlineOption(s, "LEVEL", texts.at(a.level == net::kAny ? kTextAny : kTextGroups + static_cast<size_t>(a.level)));
    onlineOption(s, "MODS", texts.at(a.mods == net::kAny ? kTextAny
                                        : a.mods == 0     ? kTextMods
                                        : a.mods == 1     ? kTextMods + 1
                                                          : kTextMods + 2));
    onlineOption(s, "COLLISION",
                 texts.at(a.collision == net::kAny ? kTextAny : a.collision == 0 ? kTextNo : kTextYes));
}

// 0x00475250: every element shown, the profile's search (+0x2A8), quick / joining / timer 0, the answer -1, shown,
// ARROW1, ARROW2 and CHANGETEXT hidden.
void OnlineArcadeSearch::enter(NetworkHost& host, ui::Screen& s) {
    s.showContaining("", false);
    m_settings = net::ArcadeSearch::read(host.optionSettings());
    m_quick = 0;
    m_joining = 0;
    m_timer = 0.0f;
    m_createAnswer = -1;
    show(host, s);
    s.setHidden("ARROW1", true);
    s.setHidden("ARROW2", true);
    s.setHidden("CHANGETEXT", true);
}

// 0x00475360. Quick searching: the quick match's wait. Joining: the join's. Else the answer yes to "create one?" goes
// to online_arcade_create over online_bg. LEFT / RIGHT: MIN_PLAYERS 1 .. 5 (held: shown without CLICK), STATUS and
// COLLISION 0 -> 1 -> Any, LEVEL as the create's, MODS 0 -> 1 -> 2 -> Any; CLICK, shown. UP / DOWN as the create.
// SELECT on B_SUBMIT: the settings into the profile (0x004716D0), the search (0x004E27D0, quick when online_main's
// choice is 0), CLICK; quick: everything reset (0x004EA4E0 with 1), the timer 0, quick searching, "JOINING GAME..."
// (type 0); else online_arcade_results. BACK: CLICK, online_main.
void OnlineArcadeSearch::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    if (m_quick != 0) {
        onlineQuickWait(host, m_quick, m_joining, m_timer, m_createAnswer, false);
        return;
    }
    if (onlineJoinWait(host, m_joining)) return;
    if (m_createAnswer == 1) {
        m_createAnswer = -1;
        host.shared().onlineForms.background = "online_arcade_create";
        host.request("online_bg", SwitchMode::Replace);
        return;
    }
    const std::string focus = s.currentButton();
    if (event == input::kLeft || event == input::kRight) {
        const int32_t dir = event == input::kLeft ? -1 : 1;
        net::ArcadeSearch& a = m_settings;
        if (same(focus, "B_MIN_PLAYERS")) {
            const int32_t v = a.minPlayers + dir;
            if (static_cast<uint32_t>(v) > 5u || v < 1) {
                a.minPlayers = std::clamp(v, 1, 5);
                show(host, s);
                return;
            }
            a.minPlayers = v;
        } else if (same(focus, "B_STATUS")) {
            a.status = net::stepAnyOfTwo(a.status, dir);
        } else if (same(focus, "B_LEVEL")) {
            a.level = net::stepLevel(a.level, dir);
        } else if (same(focus, "B_MODS")) {
            a.mods = net::stepSearchMods(a.mods, dir);
        } else if (same(focus, "B_COLLISION")) {
            a.collision = net::stepAnyOfTwo(a.collision, dir);
        }
        host.sound("CLICK");
        show(host, s);
    } else if (event == input::kUp || event == input::kDown) {
        const bool submit = same(focus, "B_SUBMIT");
        s.setHidden("SEL_TEXT", !submit);
        s.setHidden("SEL_BUT", !submit);
        s.setHidden("ARROW1", submit);
        s.setHidden("ARROW2", submit);
        s.setHidden("CHANGETEXT", submit);
    } else if (event == input::kSelect) {
        if (!same(focus, "B_SUBMIT")) return;
        m_settings.write(host.optionSettings());
        const bool quick = host.shared().onlineForms.mode == OnlineChoice::Quick;
        net::searchArcade(m_settings, quick);
        host.sound("CLICK");
        if (quick) {
            if (net::NetGame* net = net::netGame()) net->session.reset(true);
            m_timer = 0.0f;
            m_quick = 1;
            host.message(MessageType::Note, texts.at(kTextJoining), false, nullptr);
        } else {
            host.request("online_arcade_results", SwitchMode::Replace);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("online_main", SwitchMode::Replace);
    }
}

// 0x00475A50: ACCESS; UPPER_CLASS the upper class's most bhp and LOWER_CLASS the lower's least ("%d bhp"); TEAM_RACE;
// CRITERIA_TYPE; CRITERIA (by its type); PINKSLIP and COLLISION NO / YES. Red: TEAM_RACE with fewer crew mates than the
// team (a team over 100 counts 1), CRITERIA and CRITERIA_TYPE without the team's cars of it (0x004A48D0), the classes
// without the team's cars in the range (0x004A4830), PINKSLIP set with 55 cars in the garage.
void OnlineCareerCreate::show(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const net::CareerSession& c = m_settings;
    const Career* career = data != nullptr ? data->career.get() : nullptr;
    int32_t lo = 0, hi = 0, unused = 0;
    net::classBhp(c.upper, unused, hi);
    net::classBhp(c.lower, lo, unused);
    const std::wstring word = texts.at(kTextBhp);
    const int32_t team = c.team > 100 ? 1 : c.team;
    const bool crewShort = career != nullptr && career->profile.memberCount < team;
    const bool criteriaShort = career != nullptr && !net::garageHasCriteria(*career, c.team, c.criteria, c.criteriaType);
    const bool rangeShort =
        career != nullptr && !net::garageHasRange(*career, c.team, c.lower, c.upper, c.criteriaType, c.criteria);
    const bool full = c.pinkslip != 0 && career != nullptr && career->garage.size() == 55;
    onlineOption(s, "ACCESS", texts.at(c.access == 0 ? kTextPublic : kTextPrivate));
    onlineOption(s, "UPPER_CLASS", bhp(hi, word), rangeShort);
    onlineOption(s, "LOWER_CLASS", bhp(lo, word), rangeShort);
    onlineOption(s, "TEAM_RACE", texts.at(kTextTeamSizes + static_cast<size_t>(c.team)), crewShort);
    onlineOption(s, "CRITERIA_TYPE", texts.at(kTextCriteriaTypes + static_cast<size_t>(c.criteriaType)),
                 criteriaShort);
    size_t criteria = kTextNone;
    if (c.criteriaType == 1) criteria = kTextRegions + static_cast<size_t>(c.criteria);
    if (c.criteriaType == 2) criteria = kTextMakes + static_cast<size_t>(c.criteria);
    if (c.criteriaType == 3) criteria = kTextModels + static_cast<size_t>(c.criteria);
    onlineOption(s, "CRITERIA", texts.at(criteria), criteriaShort);
    onlineOption(s, "PINKSLIP", texts.at(c.pinkslip == 0 ? kTextNo : kTextYes), full);
    onlineOption(s, "COLLISION", texts.at(c.collision == 0 ? kTextNo : kTextYes));
}

// 0x00475F20: every element shown; the settings a friend's session's (details: 0x004E6570 of the position 0) or the
// profile's (+0x2BC); the pink slip 0 always; the classes widened to the garage's (the upper no more than its weakest
// car's, 0x004A9AF0, the lower no less than its strongest's, 0x004A9A70); shown; creating and started 0; the arrows and
// CHANGETEXT hidden; the details hide SEL_TEXT, SEL_BUT, B_SUBMIT and title "DETAILS".
void OnlineCareerCreate::enter(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    s.showContaining("", false);
    if (host.shared().onlineForms.details != 0) {
        net::readCareer(0, m_settings);
    } else {
        m_settings = net::CareerSession::read(host.optionSettings());
    }
    m_settings.pinkslip = 0;
    if (const Career* career = data != nullptr ? data->career.get() : nullptr) {
        m_settings.upper = std::min(m_settings.upper, net::weakestClass(*career));
        m_settings.lower = std::max(m_settings.lower, net::strongestClass(*career));
    }
    show(host, s);
    m_creating = 0;
    m_hostStarted = 0;
    s.setHidden("ARROW1", true);
    s.setHidden("ARROW2", true);
    s.setHidden("CHANGETEXT", true);
    if (host.shared().onlineForms.details != 0) {
        s.setHidden("SEL_TEXT", true);
        s.setHidden("SEL_BUT", true);
        s.setHidden("B_SUBMIT", true);
        s.setText("TITLE", texts.at(kTextDetails));
    }
}

// 0x004761F0. Details and creating as the arcade's (the career's host 0x004E20F0). LEFT / RIGHT (CLICK and shown unless
// a class is held at its end): ACCESS and COLLISION turned; TEAM_RACE 1 .. 3 round (over 1 the pink slip off); the upper
// class against the step (more power right), 0 .. 7 held, the lower following it up; the lower class the same, the upper
// following it down; CRITERIA_TYPE 0 .. 3 round with the criteria 1; CRITERIA by its type round (type 0 kept); PINKSLIP
// 1 -> 0, 0 -> 1 with the team SOLO. UP / DOWN as the arcade's. SELECT on B_SUBMIT: a pink slip with the autosave off
// "Autosave is disabled .." (type 1); the garage's checks (0x004A4590 with its notes); everything reset (0x004EA4E0
// with 1), the settings into the profile (+0x2BC), creating, CLICK, "CREATING GAME..." (type 0). BACK as the arcade's.
void OnlineCareerCreate::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (host.shared().onlineForms.details != 0) {
        s.setCurrentButton("B_SUBMIT");
        if (event == input::kBack) {
            host.sound("CLICK");
            host.request("online_friends", SwitchMode::Swap);
        }
        return;
    }
    if (m_creating != 0) {
        if (host.onlineTopMoving() || !s.transitionsDone()) return;
        if (m_hostStarted == 0) {
            if (!net::hostCareer(m_settings)) {
                m_creating = 0;
                host.sound("CLICK");
                host.message(MessageType::Continue, host.jetText(kJetNetworkLost), false, nullptr);
                return;
            }
            m_hostStarted = 1;
        }
        const net::Matchmaking* mm = net::matchmaking();
        if (mm != nullptr && mm->publicAddressKnown != 0) {
            m_creating = 0;
            host.closeMessage();
            host.onlineHostIntoLobby();
        }
        return;
    }
    const std::string focus = s.currentButton();
    net::CareerSession& c = m_settings;
    if (event == input::kLeft || event == input::kRight) {
        const int32_t dir = event == input::kLeft ? -1 : 1;
        if (same(focus, "B_ACCESS")) {
            c.access = c.access == 0 ? 1 : 0;
        } else if (same(focus, "B_COLLISION")) {
            c.collision = c.collision == 0 ? 1 : 0;
        } else if (same(focus, "B_TEAM_RACE")) {
            c.team = net::stepCreateTeam(c.team, dir);
            if (c.team > 1) c.pinkslip = 0;
        } else if (same(focus, "B_UPPER_CLASS")) {
            const int32_t v = c.upper - dir;
            if (v > 7 || v < 0) {
                c.upper = std::clamp(v, 0, 7);
                show(host, s);
                return;
            }
            c.upper = v;
            if (c.lower < c.upper) c.lower = c.upper;
        } else if (same(focus, "B_LOWER_CLASS")) {
            const int32_t v = c.lower - dir;
            if (v > 7 || v < 0) {
                c.lower = std::clamp(v, 0, 7);
                show(host, s);
                return;
            }
            c.lower = v;
            if (c.upper > c.lower) c.upper = c.lower;
        } else if (same(focus, "B_CRITERIA_TYPE")) {
            int32_t v = c.criteriaType + dir;
            if (v >= 4) v = 0;
            if (v < 0) v = 3;
            c.criteriaType = v;
            c.criteria = 1;
        } else if (same(focus, "B_CRITERIA")) {
            c.criteria = net::stepCreateCriteria(c.criteriaType, c.criteria, dir);
        } else if (same(focus, "B_PINKSLIP")) {
            if (c.pinkslip == 1) {
                c.pinkslip = 0;
            } else {
                c.pinkslip = 1;
                c.team = 1;
            }
        }
        host.sound("CLICK");
        show(host, s);
    } else if (event == input::kUp || event == input::kDown) {
        const bool submit = same(focus, "B_SUBMIT");
        s.setHidden("SEL_TEXT", !submit);
        s.setHidden("SEL_BUT", !submit);
        s.setHidden("ARROW1", submit);
        s.setHidden("ARROW2", submit);
        s.setHidden("CHANGETEXT", submit);
    } else if (event == input::kSelect) {
        if (!same(focus, "B_SUBMIT")) return;
        if (c.pinkslip == 1 && data != nullptr && data->saving == 0) {
            host.message(MessageType::Continue, texts.at(kTextAutosave), false, nullptr);
            return;
        }
        const Career* career = data != nullptr ? data->career.get() : nullptr;
        size_t note = 0;
        if (career != nullptr &&
            !net::careerCheck(*career, c.team, c.lower, c.upper, c.criteriaType, c.criteria, c.pinkslip, true, &note)) {
            if (note != 0) host.message(MessageType::Continue, texts.at(note), false, nullptr);
            return;
        }
        if (net::NetGame* net = net::netGame()) net->session.reset(true);
        c.write(host.optionSettings());
        m_creating = 1;
        m_hostStarted = 0;
        host.sound("CLICK");
        host.message(MessageType::Note, texts.at(kTextCreating), false, nullptr);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("online_main", SwitchMode::Replace);
    }
}

// 0x004768C0: MIN_PLAYERS "%d"; STATUS Any / Waiting / Racing; TEAM_RACE Any or its text; UPPER_CLASS the upper's most
// bhp, LOWER_CLASS the lower's least; CRITERIA_TYPE Any (CRITERIA Any with it) or its text, CRITERIA Any or by the type;
// PINKSLIP and COLLISION Any / NO / YES. Red: TEAM_RACE (not Any) with fewer crew mates; the criteria when neither is
// Any and the garage lacks them; the classes without the cars; PINKSLIP (any but 0, Any too) with 55 cars.
void OnlineCareerSearch::show(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const net::CareerSearch& c = m_settings;
    const Career* career = data != nullptr ? data->career.get() : nullptr;
    int32_t lo = 0, hi = 0, unused = 0;
    net::classBhp(c.upper, unused, hi);
    net::classBhp(c.lower, lo, unused);
    const std::wstring word = texts.at(kTextBhp);
    const int32_t team = c.team > 100 ? 1 : c.team;
    const bool crewShort = c.team != net::kAny && career != nullptr && career->profile.memberCount < team;
    const bool criteriaShort = c.criteriaType != net::kAny && c.criteria != net::kAny && career != nullptr &&
                               !net::garageHasCriteria(*career, c.team, c.criteria, c.criteriaType);
    const bool rangeShort =
        career != nullptr && !net::garageHasRange(*career, c.team, c.lower, c.upper, c.criteriaType, c.criteria);
    const bool full = c.pinkslip != 0 && career != nullptr && career->garage.size() == 55;
    onlineOption(s, "MIN_PLAYERS", std::format(L"{}", c.minPlayers));
    onlineOption(s, "STATUS", texts.at(c.status == net::kAny ? kTextAny : c.status == 0 ? kTextWaiting : kTextRacing));
    const size_t teamText = c.team == net::kAny ? kTextAny : kTextTeamSizes + static_cast<size_t>(c.team);
    onlineOption(s, "TEAM_RACE", texts.at(teamText), crewShort);
    onlineOption(s, "UPPER_CLASS", bhp(hi, word), rangeShort);
    onlineOption(s, "LOWER_CLASS", bhp(lo, word), rangeShort);
    if (c.criteriaType == net::kAny) {
        onlineOption(s, "CRITERIA_TYPE", texts.at(kTextAny), criteriaShort);
        onlineOption(s, "CRITERIA", texts.at(kTextAny), criteriaShort);
    } else {
        onlineOption(s, "CRITERIA_TYPE", texts.at(kTextCriteriaTypes + static_cast<size_t>(c.criteriaType)),
                     criteriaShort);
        size_t criteria = kTextAny;
        if (c.criteria != net::kAny) {
            criteria = kTextNone;
            if (c.criteriaType == 1) criteria = kTextRegions + static_cast<size_t>(c.criteria);
            if (c.criteriaType == 2) criteria = kTextMakes + static_cast<size_t>(c.criteria);
            if (c.criteriaType == 3) criteria = kTextModels + static_cast<size_t>(c.criteria);
        }
        onlineOption(s, "CRITERIA", texts.at(criteria), criteriaShort);
    }
    onlineOption(s, "PINKSLIP",
                 texts.at(c.pinkslip == net::kAny ? kTextAny : c.pinkslip == 0 ? kTextNo : kTextYes), full);
    onlineOption(s, "COLLISION",
                 texts.at(c.collision == net::kAny ? kTextAny : c.collision == 0 ? kTextNo : kTextYes));
}

// 0x00476DD0: every element shown, the profile's search (+0x304), quick / joining / timer 0, the answer -1, the classes
// widened to the garage's as the create's (the upper +0x10, the lower +0x0C), shown, the arrows and CHANGETEXT hidden.
void OnlineCareerSearch::enter(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    s.showContaining("", false);
    m_settings = net::CareerSearch::read(host.optionSettings());
    m_quick = 0;
    m_joining = 0;
    m_timer = 0.0f;
    m_createAnswer = -1;
    if (const Career* career = data != nullptr ? data->career.get() : nullptr) {
        m_settings.upper = std::min(m_settings.upper, net::weakestClass(*career));
        m_settings.lower = std::max(m_settings.lower, net::strongestClass(*career));
    }
    show(host, s);
    s.setHidden("ARROW1", true);
    s.setHidden("ARROW2", true);
    s.setHidden("CHANGETEXT", true);
}

// 0x00476F90. Quick searching, joining and "create one?" (online_career_create) as the arcade's. LEFT / RIGHT:
// MIN_PLAYERS 1 .. 5 (held: shown without CLICK; over 1 a pink slip off); STATUS and COLLISION 0 -> 1 -> Any;
// TEAM_RACE 1 -> 2 -> 3 -> Any (over 1 a pink slip off); the classes as the create's; CRITERIA_TYPE 0 -> 1 -> 2 -> 3 ->
// Any and back (Stock problem: LEFT on 3 stays 3) with the criteria 1; CRITERIA by the type (type 0: nothing, no CLICK);
// PINKSLIP 0 <-> 1 (on: the team and the least players 1). SELECT on B_SUBMIT: a pink slip with the autosave off
// "Autosave is disabled .."; the garage's checks (0x004A46F0); the settings into the profile (+0x304), the search
// (0x004E3300), CLICK; quick: everything reset, the timer 0, quick searching, "JOINING GAME..."; else
// online_career_results. BACK: CLICK, online_main.
void OnlineCareerSearch::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (m_quick != 0) {
        onlineQuickWait(host, m_quick, m_joining, m_timer, m_createAnswer, true);
        return;
    }
    if (onlineJoinWait(host, m_joining)) return;
    if (m_createAnswer == 1) {
        m_createAnswer = -1;
        host.shared().onlineForms.background = "online_career_create";
        host.request("online_bg", SwitchMode::Replace);
        return;
    }
    const std::string focus = s.currentButton();
    net::CareerSearch& c = m_settings;
    if (event == input::kLeft || event == input::kRight) {
        const int32_t dir = event == input::kLeft ? -1 : 1;
        if (same(focus, "B_MIN_PLAYERS")) {
            const int32_t v = c.minPlayers + dir;
            if (static_cast<uint32_t>(v) > 5u || v < 1) {
                c.minPlayers = std::clamp(v, 1, 5);
                show(host, s);
                return;
            }
            c.minPlayers = v;
            if (c.minPlayers > 1 && c.pinkslip == 1) c.pinkslip = 0;
        } else if (same(focus, "B_STATUS")) {
            c.status = net::stepAnyOfTwo(c.status, dir);
        } else if (same(focus, "B_COLLISION")) {
            c.collision = net::stepAnyOfTwo(c.collision, dir);
        } else if (same(focus, "B_TEAM_RACE")) {
            c.team = net::stepSearchTeam(c.team, dir);
            if (c.team > 1 && c.pinkslip == 1) c.pinkslip = 0;
        } else if (same(focus, "B_UPPER_CLASS")) {
            const int32_t v = c.upper - dir;
            if (v > 7 || v < 0) {
                c.upper = std::clamp(v, 0, 7);
                show(host, s);
                return;
            }
            c.upper = v;
            if (c.lower < c.upper) c.lower = c.upper;
        } else if (same(focus, "B_LOWER_CLASS")) {
            const int32_t v = c.lower - dir;
            if (v > 7 || v < 0) {
                c.lower = std::clamp(v, 0, 7);
                show(host, s);
                return;
            }
            c.lower = v;
            if (c.upper > c.lower) c.upper = c.lower;
        } else if (same(focus, "B_CRITERIA_TYPE")) {
            c.criteriaType = net::stepSearchCriteriaType(c.criteriaType, dir);
            c.criteria = 1;
        } else if (same(focus, "B_CRITERIA")) {
            if (c.criteriaType == 0) return;
            c.criteria = net::stepSearchCriteria(c.criteriaType, c.criteria, dir);
        } else if (same(focus, "B_PINKSLIP")) {
            if (c.pinkslip == 0) {
                c.pinkslip = 1;
            } else if (c.pinkslip == 1) {
                c.pinkslip = 0;
            }
            if (c.pinkslip == 1) {
                c.team = 1;
                c.minPlayers = 1;
            }
        }
        host.sound("CLICK");
        show(host, s);
    } else if (event == input::kUp || event == input::kDown) {
        const bool submit = same(focus, "B_SUBMIT");
        s.setHidden("SEL_TEXT", !submit);
        s.setHidden("SEL_BUT", !submit);
        s.setHidden("ARROW1", submit);
        s.setHidden("ARROW2", submit);
        s.setHidden("CHANGETEXT", submit);
    } else if (event == input::kSelect) {
        if (!same(focus, "B_SUBMIT")) return;
        if (c.pinkslip == 1 && data != nullptr && data->saving == 0) {
            host.message(MessageType::Continue, texts.at(kTextAutosave), false, nullptr);
            return;
        }
        const Career* career = data != nullptr ? data->career.get() : nullptr;
        size_t note = 0;
        if (career != nullptr &&
            !net::careerCheck(*career, c.team, c.lower, c.upper, c.criteriaType, c.criteria, c.pinkslip, true, &note)) {
            if (note != 0) host.message(MessageType::Continue, texts.at(note), false, nullptr);
            return;
        }
        c.write(host.optionSettings());
        const bool quick = host.shared().onlineForms.mode == OnlineChoice::Quick;
        net::searchCareer(c, quick);
        host.sound("CLICK");
        if (quick) {
            if (net::NetGame* net = net::netGame()) net->session.reset(true);
            m_timer = 0.0f;
            m_quick = 1;
            host.message(MessageType::Note, texts.at(kTextJoining), false, nullptr);
        } else {
            host.request("online_career_results", SwitchMode::Replace);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("online_main", SwitchMode::Replace);
    }
}

// 0x00478950: the sorted position whose record's address is the highlighted row's (+0x14); -1 none.
int32_t OnlineResults::position() const {
    const net::Matchmaking* mm = net::matchmaking();
    if (mm == nullptr || !m_highlighted) return -1;
    for (size_t pos = 0; pos < mm->sorted.size(); ++pos) {
        const net::SearchResult* r = mm->sortedResult(static_cast<uint32_t>(pos));
        if (r != nullptr && r->address == m_highlighted) return static_cast<int32_t>(pos);
    }
    return -1;
}

// 0x00478A50: the search again (not quick) with the profile's settings: the arcade's (+0x2A8, 0x004E27D0) or the
// career's (+0x304, 0x004E3300).
void OnlineResults::refresh(WindowHost& host) {
    if (net::onlineHosted().mode == net::OnlineKind::Arcade) {
        net::searchArcade(net::ArcadeSearch::read(host.optionSettings()), false);
    } else {
        net::searchCareer(net::CareerSearch::read(host.optionSettings()), false);
    }
}

// 0x00477970: the four lists (M_HOST_NAME, M_USED, M_OPEN, M_QUALITY) emptied; a row for each sorted position whose
// record is valid: the session's name (its item's data the record's address), "%d" of its players (key50) and of its
// open slots (key51), its quality's '+'s (0x004E6650). Online career results with a career: a row the garage cannot
// take (no car, too few crew mates, the class range or the criteria of its settings, 0x004E6570) disabled in all four.
// The highlighted address's row is the matchmaking's +0x48 and the four lists' highlight (else the first); SEL_TEXT
// and SEL_BUT shown with any row.
void OnlineResults::fillRows(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    static const char* const kLists[] = {"M_HOST_NAME", "M_USED", "M_OPEN", "M_QUALITY"};
    for (const char* list : kLists) {
        if (s.find(list) >= 0) s.clearMenu(list);
    }
    m_rows.clear();
    net::Matchmaking* mm = net::matchmaking();
    const Career* career = data != nullptr ? data->career.get() : nullptr;
    const bool careerRows =
        net::onlineHosted().mode == net::OnlineKind::Career && career != nullptr && career->head != 0;
    size_t highlight = 0;
    bool any = false;
    if (mm != nullptr) {
        for (size_t pos = 0; pos < mm->sorted.size(); ++pos) {
            const net::SearchResult* r = mm->sortedResult(static_cast<uint32_t>(pos));
            if (r == nullptr || r->valid == 0) continue;
            int32_t access = 0, playing = 0, careerFlag = 0;
            std::array<char16_t, 16> name{};
            net::readSession(static_cast<int32_t>(pos), access, name, playing, careerFlag);
            const size_t row = m_rows.size();
            s.addMenuItem("M_HOST_NAME", wide(net::nameText(name)), 0.0f);
            s.addMenuItem("M_USED", std::format(L"{}", r->players), 0.0f);
            s.addMenuItem("M_OPEN", std::format(L"{}", r->openSlots), 0.0f);
            s.addMenuItem("M_QUALITY", std::wstring(static_cast<size_t>(net::qualityMarks(static_cast<uint32_t>(pos))), L'+'),
                          0.0f);
            m_rows.push_back(r->address);
            any = true;
            if (careerRows) {
                net::CareerSession c;
                net::readCareer(static_cast<int32_t>(pos), c);
                const int32_t team = c.team > 100 ? 1 : c.team;
                const bool takes = !career->garage.empty() && career->profile.memberCount >= team &&
                                   net::garageHasRange(*career, c.team, c.lower, c.upper, c.criteriaType, c.criteria) &&
                                   net::garageHasCriteria(*career, c.team, c.criteria, c.criteriaType);
                if (!takes) {
                    for (const char* list : kLists) s.setMenuItemEnabledAt(list, row, false);
                }
            }
            if (m_highlighted && r->address == m_highlighted) {
                mm->selected = static_cast<int32_t>(mm->sorted[pos]);
                highlight = row;
            }
        }
    }
    for (const char* list : kLists) {
        if (s.find(list) >= 0 && s.menuSize(list) != 0) s.selectMenuItem(list, highlight);
    }
    s.setHidden("SEL_TEXT", !any);
    s.setHidden("SEL_BUT", !any);
}

// 0x00478570: the highlighted session's settings (0x00478950; none: the texts emptied, M_STATUS ""). Arcade: LEVEL,
// MODS (0 NONE, 1 STANDARD, 2 PROTOTYPE, else Any), COLLISION NO / YES; career: TEAM_RACE, UPPER_CLASS (its most bhp),
// LOWER_CLASS (its least), TEAM_RACE again (as the stock sets it twice), CRITERIA_TYPE, CRITERIA, PINKSLIP NO / YES;
// M_STATUS Racing or Waiting.
void OnlineResults::details(NetworkHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    const int32_t pos = position();
    const bool arcade = net::onlineHosted().mode == net::OnlineKind::Arcade;
    if (pos == -1) {
        if (arcade) {
            for (const char* e : {"LEVEL", "MODS", "COLLISION"}) s.setText(e, L"");
        } else {
            for (const char* e : {"TEAM_RACE", "UPPER_CLASS", "LOWER_CLASS", "TEAM_RACE", "CRITERIA_TYPE", "CRITERIA",
                                  "PINKSLIP"}) {
                s.setText(e, L"");
            }
        }
        s.setText("M_STATUS", L"");
        return;
    }
    if (arcade) {
        net::ArcadeSession a;
        net::readArcade(pos, a);
        s.setText("LEVEL", texts.at(a.level == net::kAny ? kTextAny : kTextGroups + static_cast<size_t>(a.level)));
        s.setText("MODS", texts.at(a.mods == 0   ? kTextMods
                                         : a.mods == 1 ? kTextMods + 1
                                         : a.mods == 2 ? kTextMods + 2
                                                       : kTextAny));
        s.setText("COLLISION", texts.at(a.collision == 0 ? kTextNo : kTextYes));
        s.setText("M_STATUS", texts.at(a.playing != 0 ? kTextRacing : kTextWaiting));
        return;
    }
    net::CareerSession c;
    net::readCareer(pos, c);
    int32_t lo = 0, hi = 0, unused = 0;
    net::classBhp(c.upper, unused, hi);
    net::classBhp(c.lower, lo, unused);
    const std::wstring word = texts.at(kTextBhp);
    s.setText("TEAM_RACE", texts.at(kTextTeamSizes + static_cast<size_t>(c.team)));
    s.setText("UPPER_CLASS", bhp(hi, word));
    s.setText("LOWER_CLASS", bhp(lo, word));
    s.setText("TEAM_RACE", texts.at(kTextTeamSizes + static_cast<size_t>(c.team)));
    s.setText("CRITERIA_TYPE", texts.at(kTextCriteriaTypes + static_cast<size_t>(c.criteriaType)));
    size_t criteria = kTextNone;
    if (c.criteriaType == 1) criteria = kTextRegions + static_cast<size_t>(c.criteria);
    if (c.criteriaType == 2) criteria = kTextMakes + static_cast<size_t>(c.criteria);
    if (c.criteriaType == 3) criteria = kTextModels + static_cast<size_t>(c.criteria);
    s.setText("CRITERIA", texts.at(criteria));
    s.setText("PINKSLIP", texts.at(c.pinkslip == 0 ? kTextNo : kTextYes));
    s.setText("M_STATUS", texts.at(c.playing != 0 ? kTextRacing : kTextWaiting));
}

// 0x00478AD0: M_HOST_NAME emptied, the timers, the re-query and the join 0, no highlighted address, the details, GAMESPY
// shown (the search was started by the search window).
void OnlineResults::enter(NetworkHost& host, ui::Screen& s) {
    if (s.find("M_HOST_NAME") >= 0) s.clearMenu("M_HOST_NAME");
    m_rows.clear();
    m_sortTimer = 0.0f;
    m_listTimer = 0.0f;
    m_requerying = 0;
    m_joining = 0;
    m_highlighted.reset();
    details(host, s);
    s.showContaining("GAMESPY", false);
}

// 0x00478B30: "Menu"'s highlight kept, the rows' addresses let go, M_HOST_NAME emptied, the highlighted address let go.
void OnlineResults::leave(ui::Screen& s) {
    if (s.find("Menu") >= 0) m_menu = s.selectedMenuItem("Menu");
    m_rows.clear();
    if (s.find("M_HOST_NAME") >= 0) s.clearMenu("M_HOST_NAME");
    m_highlighted.reset();
}

// 0x00478C00. Re-querying the chosen session, once no message moves in: waited while no result is valid and the search
// is not over; then the search stopped and position 0 joined (0x004734E0; joining), or none "This game session is no
// longer available." (type 1) and the search again. Joining: the session lost stops it and searches again; in the
// session link_lobby. Else: TITLE "SEARCHING..." (no list yet), "ANALYZING..." (no details) or "select a game" / "NO
// GAMES FOUND"; REFRESH shown once the details are in; each second the results sorted (UP / DOWN start that second
// again) and each second the rows and the details made again; the highlighted row's address followed (the details
// with it); SEL_ elements with rows, ARROW elements with more than one. SELECT: everything reset (0x004EA4E0 with 1),
// the highlighted session asked again alone (0x004E4E90), re-querying, "JOINING GAME..." (type 0). SELECT2 with the
// search over: the search again (CLICK), the highlight let go, the rows, the details and the timers anew. SELECT3:
// the mode's create window over online_bg (CLICK). BACK: the search stopped (0x004E62F0), CLICK, online_main.
void OnlineResults::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    net::Matchmaking* mm = net::matchmaking();
    if (m_requerying != 0) {
        if (host.onlineTopMoving()) return;
        if (net::countValidResults() == 0 && mm != nullptr && mm->listIn == 0) return;
        m_requerying = 0;
        if (mm != nullptr) mm->stopSearch();
        if (net::countValidResults() != 0) {
            if (host.onlineJoinSession(0)) m_joining = 1;
        } else {
            host.closeMessage();
            host.message(MessageType::Continue, host.jetText(kJetNoLongerAvailable), false, nullptr);
            refresh(host);
        }
        return;
    }
    if (m_joining != 0) {
        if (host.onlineTopMoving()) return;
        if (host.netDropped()) {
            m_joining = 0;
            refresh(host);
            return;
        }
        net::NetGame* net = net::netGame();
        if (net != nullptr && net->transport.inSession()) {
            m_joining = 0;
            host.closeMessage();
            host.request("link_lobby", SwitchMode::Replace);
        }
        return;
    }
    if (mm == nullptr || mm->listIn == 0) {
        s.setText("TITLE", texts.at(kTextSearching));
    } else if (mm->detailsIn == 0) {
        s.setText("TITLE", texts.at(kTextAnalyzing) + L"...");
    } else {
        s.setText("TITLE", texts.at(net::countValidResults() > 0 ? kTextSelectGame : kTextNoGames));
    }
    if (mm != nullptr && mm->detailsIn != 0) {
        s.showContaining("REFRESH", false);
    } else {
        s.hideContaining("REFRESH", false);
    }
    if (event == input::kUp || event == input::kDown) m_sortTimer = 0.0f;
    m_sortTimer = host.frameTime() + m_sortTimer;
    if (1.0f < m_sortTimer) {
        m_sortTimer = 0.0f;
        if (mm != nullptr) mm->sortResults();
    }
    m_listTimer = host.frameTime() + m_listTimer;
    if (1.0f < m_listTimer) {
        m_listTimer = 0.0f;
        fillRows(host, s);
        details(host, s);
    }
    if (s.find("M_HOST_NAME") >= 0 && s.menuSize("M_HOST_NAME") != 0) {
        const size_t row = s.selectedMenuItem("M_HOST_NAME");
        const std::optional<net::Address> address = row < m_rows.size() ? m_rows[row] : std::nullopt;
        if (address != m_highlighted) {
            m_highlighted = address;
            details(host, s);
        }
    }
    const size_t rows = s.find("M_HOST_NAME") >= 0 ? s.menuSize("M_HOST_NAME") : 0;
    if (rows != 0) {
        s.showContaining("SEL_", false);
    } else {
        s.hideContaining("SEL_", false);
    }
    if (rows > 1) {
        s.showContaining("ARROW", false);
    } else {
        s.hideContaining("ARROW", false);
    }
    if (event == input::kSelect) {
        if (rows == 0) return;
        const int32_t pos = position();
        if (net::NetGame* net = net::netGame()) net->session.reset(true);
        net::requery(pos);
        m_requerying = 1;
        host.message(MessageType::Note, texts.at(kTextJoining), false, nullptr);
    } else if (event == input::kSelect2) {
        if (mm == nullptr || mm->detailsIn == 0) return;
        refresh(host);
        host.sound("CLICK");
        m_highlighted.reset();
        fillRows(host, s);
        details(host, s);
        m_sortTimer = 0.0f;
        m_listTimer = 0.0f;
    } else if (event == input::kSelect3) {
        // 0x004E1AA0: the matchmaking there and the arcade.
        host.shared().onlineForms.background = mm != nullptr && net::onlineHosted().mode == net::OnlineKind::Arcade
                                                   ? "online_arcade_create"
                                                   : "online_career_create";
        host.request("online_bg", SwitchMode::Replace);
        host.sound("CLICK");
    } else if (event == input::kBack) {
        if (mm != nullptr) mm->stopSearch();
        host.sound("CLICK");
        host.request("online_main", SwitchMode::Replace);
    }
}

}  // namespace oj::frontend
