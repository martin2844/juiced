// The career's phone: its calls, menu, contacts and choice.

#include "Game/Frontend/Career/PhoneWindows.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <iterator>

namespace oj::frontend {

namespace {

// The phones' prices (0x00718F70) by the model.
constexpr int32_t kPhonePrices[] = {100, 100, 150, 150, 200, 200};

int32_t phoneCrew(int32_t pointer) {
    return pointer - 1;
}

}  // namespace

std::wstring MobilePhone::lineText(CareerHost& host, int32_t text) {
    const TextTable& texts = host.gameTexts();
    if (text >= 0) return texts.at(static_cast<size_t>(text));
    const int32_t k = kPhoneChallenge - text;
    if (k >= 0 && k < 3) return m_challenges[static_cast<size_t>(k)];
    return std::wstring();
}

// 0x004B65E0: the call's sample played once (below 0x21, not -1, with a speech object: its line stopped, 0x0040B600,
// then its +0x14 with the sample's crew and the sample); its text, Mr. Jeff's after the player's leader's name.
std::wstring MobilePhone::callText(CareerHost& host, PhoneMessage& call) {
    GameData* data = host.gameData();
    if (call.played == 0) {
        call.played = 1;
        if (call.sample != -1 && call.sample < 0x21) {
            if (Speech* speech = gameSounds().speech) {
                speech->stop();
                speech->say(call.param, call.sample);
            }
        }
    }
    std::wstring text;
    if (call.textAfterLeader != 0) {
        const auto& name = data->career->crews[0].memberNames[0];
        text = widenSigned(std::string_view(name.data(), strnlen(name.data(), name.size())));
    }
    return text + lineText(host, call.text);
}

// 0x004D23B0: PIC_PICTURE by the caller: a crew's leader (crew 0 .. 8) its leader's picture, a crewmate
// ui_crewmember_<nn>, Mr. Jeff ui_MrJeff.
void MobilePhone::showCaller(CareerHost& host, ui::Screen& s, PhoneCaller kind, int32_t crew) {
    GameData* data = host.gameData();
    if (kind == PhoneCaller::CrewLeader) {
        if (crew < 0 || crew >= 9) return;
        const auto id = static_cast<size_t>(data->career->crews[static_cast<size_t>(crew)].id);
        if (id < std::size(kLeaderPictures)) s.setTexture("PIC_PICTURE", kLeaderPictures[id]);
    } else if (kind == PhoneCaller::Crewmate) {
        s.setTexture("PIC_PICTURE", std::format("ui_crewmember_{:02}", crew));
    } else if (kind == PhoneCaller::Dealer) {
        s.setTexture("PIC_PICTURE", "ui_MrJeff");
    }
}

// The phone model's picture on Mobile: 1 ui_moto_02, 2 ui_sharp_01, 3 ui_sharp_02, 4 ui_sony_02, 5 ui_sony, else
// ui_moto.
void MobilePhone::showModel(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    static const char* const kModels[] = {"ui_moto", "ui_moto_02", "ui_sharp_01", "ui_sharp_02", "ui_sony_02",
                                          "ui_sony"};
    const int32_t model = data->career->phone.model;
    s.setTexture("Mobile", kModels[model >= 1 && model <= 5 ? model : 0]);
}

// 0x004B60F0 in the career's windows: a respect change of the last call (+0x18) is shown in CMSRespectMsg over the
// window (the crew's respect before, the change; no comment) and becomes the crew's respect (+0x20, not below 0, cut);
// the crews' pink slip calls (0x004B6A10). A window shown for the first time waits 2 s (the front end's frame time);
// then the phone's timer (+0x04) runs down; at 0, with the phone idle, the first message waiting becomes the call
// (0x004B6580; a crewmate's kinds 0 and 1 its crewmate shown, +0xFA0), its caller joins the contacts unless it is a
// text message (type 10; 0x004B6640), the phone rings (3) and CMSMobilePhone opens over the window.
void MobilePhone::frame(CareerHost& host, ui::Screen& screen, const ui::Screen*& shown) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Phone& p = career.phone;
    if (p.respectPending != 0) {
        const auto crew = static_cast<size_t>(std::clamp(p.respectCrew, 0, 8));
        Crew& c = career.crews[crew];
        p.respectPending = 0;
        host.respectMessage(static_cast<int>(crew), static_cast<float>(c.respect), p.respectChange, std::wstring());
        c.respect = x87::chop(std::max(p.respectAfter, 0.0f));
    }
    crewPhoneCalls(career);
    if (shown != &screen && m_wait < 2.0f) {
        m_wait = m_wait + host.frameTime();
        return;
    }
    m_wait = 0.0f;
    shown = &screen;
    if (p.timer > 0.0f) {
        p.timer = p.timer - host.frameTime();
        return;
    }
    if (p.state != PhoneState::Idle) return;
    for (PhoneMessage& m : p.messages) {
        if (m.used == 0) continue;
        if (m.type == CallType::LetJoin || m.type == CallType::JoinMyCrew)
            p.shownCrewmate = m.crew == -1 ? 0 : m.crew + 1;
        m.used = 0;
        setPhoneCall(p, m.type, m.sample, m.param, m.text, m.answers[0], m.answers[1], m.answers[2], m.answers[3]);
        p.call.textAfterLeader = m.textAfterLeader;
        p.call.crew = m.crew;
        p.call.kind = m.kind;
        if (p.call.type != CallType::TextMessage) {
            addPhoneContact(p, m.kind, std::string(m.name.data(), strnlen(m.name.data(), m.name.size())), m.crew);
        }
        p.state = PhoneState::Ringing;
        p.call.name = m.name;
        host.request("CMSMobilePhone", SwitchMode::Push);
        return;
    }
}

// 0x004D17A0: Mobile the phone's model, the caller's picture; the event not gone to (+0x0C); Message (over several
// lines) and Caller emptied. A phone opened by the player (idle: it becomes 1) shows Caller "CONTACTS" (314) and
// opens CMSMobilePhoneContacts over it with SEL2 shown; else SEL2 is hidden. Reject hidden, Answer shown (hidden for
// the opened phone). The front end's music at a quarter; the ring's time 10 s and its sound forgotten ([0x0075EF74]
// 0, not stopped). First the interface's movie,
// whatever its element, held and stepped twice (0x004D17A3).
void MobilePhone::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (movie::System* m = movie::System::get(); m != nullptr && m->current != nullptr) m->current->restart();
    if (data == nullptr || !data->career) return;
    Phone& p = data->career->phone;
    showModel(host, s);
    showCaller(host, s, p.call.kind, p.call.crew);
    p.goToEvent = 0;
    s.setText("Message", L"");
    s.setMultiline("Message", true);
    s.setText("Caller", L"");
    if (p.state == PhoneState::Idle) {
        p.state = PhoneState::Opened;
        s.setText("Caller", texts.at(phonetext::kContacts));
        host.request("CMSMobilePhoneContacts", SwitchMode::Push);
        s.showContaining("SEL2", false);
    } else {
        s.hideContaining("SEL2");
    }
    s.setHidden("Reject", true);
    s.show("Answer", false);
    if (p.state == PhoneState::Opened) s.setHidden("Answer", true);
    musicQuarter(host);
    m_ring = 10.0f;
    m_sound = 0;
}

// 0x004D1C10: BACK_ hidden, then by the phone's state: idle closes the window; opened hides Answer and Reject, shows
// BACK_, and BACK closes it and the phone goes idle (0x004B69B0); calling the same with SEL2 hidden, then the call's
// ringing (0x004D2140); ringing (0x004D1F10) with SEL2 hidden; talking with SEL2 hidden: message the call's text and
// CMSMobilePhoneMenu over it.
void MobilePhone::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Phone& p = data->career->phone;
    s.hideContaining("BACK_");
    switch (p.state) {
    case PhoneState::Idle: host.request("", SwitchMode::Pop); break;
    case PhoneState::Opened:
        s.setHidden("Answer", true);
        s.setHidden("Reject", true);
        s.showContaining("BACK_", false);
        if (event == input::kBack) {
            host.request("", SwitchMode::Pop);
            setPhoneState(p, PhoneState::Idle, gameRandom());
        }
        break;
    case PhoneState::Calling:
        s.hideContaining("SEL2");
        s.showContaining("BACK_", false);
        if (event == input::kBack) {
            host.request("", SwitchMode::Pop);
            setPhoneState(p, PhoneState::Idle, gameRandom());
        }
        calling(host, s, event);
        break;
    case PhoneState::Ringing:
        s.hideContaining("SEL2");
        ringing(host, s, event);
        break;
    case PhoneState::Talking:
        s.hideContaining("SEL2");
        s.setText("message", callText(host, p.call));
        host.request("CMSMobilePhoneMenu", SwitchMode::Push);
        break;
    default:
        break;
    }
}

// 0x004D1F10: RingIn each 2 s (its handle kept, [0x0075EF74]); caller "NEW MESSAGE" (336) for a text message (type
// 10), else the caller's name. SELECT stops the ring; on answer: Answer and Reject hidden, talking (4), message the call's text, caller the name
// for a text message, CMSMobilePhoneMenu over it.
void MobilePhone::ringing(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Phone& p = data->career->phone;
    m_ring = m_ring + host.frameTime();
    if (m_ring > 2.0f) {
        m_ring = 0.0f;
        m_sound = host.uiSounds().play("RingIn");
    }
    const std::string name(p.call.name.data(), strnlen(p.call.name.data(), p.call.name.size()));
    s.setText("caller", p.call.type == CallType::TextMessage ? texts.at(phonetext::kNewMessage) : widenSigned(name));
    if (event != input::kSelect) return;
    stopRing(host);
    if (_stricmp(s.currentButton().c_str(), "answer") != 0) return;
    s.setHidden("Answer", true);
    s.setHidden("Reject", true);
    p.state = PhoneState::Talking;
    s.setText("message", callText(host, p.call));
    if (p.call.type == CallType::TextMessage) s.setText("caller", widenSigned(name));
    host.request("CMSMobilePhoneMenu", SwitchMode::Push);
}

// 0x004D2140: caller "RINGING" (1638) over 0.2 of each second, else empty; RingOut each 2 s. The ringing (+0xF94) gone
// below 0: caller emptied, talking (4), message the call's text, BACK_ hidden, CMSMobilePhoneMenu over it, the ring
// stopped. Else, unless the call answers at once (+0xF98), the ringing runs down, and SELECT ends it (-1).
void MobilePhone::calling(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Phone& p = data->career->phone;
    m_blink = m_blink + host.frameTime();
    if (m_blink > 1.0f) m_blink = m_blink - 1.0f;
    s.setText("caller", m_blink > 0.2f ? texts.at(kTextRinging) : std::wstring());
    m_ring = m_ring + host.frameTime();
    if (m_ring > 2.0f) {
        m_ring = 0.0f;
        m_sound = host.uiSounds().play("RingOut");
    }
    if (p.dialTimer < 0.0f) {
        s.setText("caller", L"");
        p.state = PhoneState::Talking;
        s.setText("message", callText(host, p.call));
        s.hideContaining("BACK_");
        host.request("CMSMobilePhoneMenu", SwitchMode::Push);
        stopRing(host);
        return;
    }
    // Stock problem: a crewmate who joined answers at once (+0xF98), so the ringing never runs down nor ends.
    if (p.answersAtOnce == 0) p.dialTimer = p.dialTimer - host.frameTime();
    if (event == input::kSelect && p.answersAtOnce == 0) p.dialTimer = -1.0f;
}

// 0x004D1AD0 / 0x004B75D0: the speech's line stopped (0x0040B600), the front end's music back, the ring's or dial's
// sound stopped ([0x0075EF74]). With an event to go to (+0x0C) on the
// phone's day (+0x10, +0x14): it becomes the event chosen, both note lists emptied, the days passed to it and the
// meeting (0x004C27A0). Then an advice to call (+0x1C 9 or 7) is given: the advisor's +0x18 takes it, +0x1C 13.
void MobilePhone::leave(CareerHost& host) {
    GameData* data = host.gameData();
    // First the interface's movie lets go of its hold (0x004D1AE0).
    if (movie::System* m = movie::System::get(); m != nullptr && m->current != nullptr) m->current->held = false;
    host.stopSpeech();
    host.setMusicVolume(host.settingsMusic(), false);
    stopRing(host);
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Phone& p = career.phone;
    if (p.goToEvent != 0) {
        CalendarEvent* event = eventAt(career.calendar, p.eventDay, p.eventMonth);
        if (event != nullptr) {
            career.meetingEvent = event;
            g_selectedMonth = p.eventMonth;
            g_selectedDay = p.eventDay;
            clearNotes(career, career.meeting);
            clearNotes(career, career.respectChanges);
            while (!(career.calendar.dayOfMonth == event->dayOfMonth && career.calendar.month == event->month)) {
                data->nextDay(gameRandom());
            }
            host.goToMeeting();
        }
    }
    const int32_t advice = career.advisor.advice;
    if (advice == 9 || advice == 7) {
        career.advisor.lastAdvice = advice;
        career.advisor.advice = 13;
        career.adviceEvent = nullptr;
    }
}

// 0x004D2460: Menu emptied, the call's answers as items (their place the value); data1 .. 4 over several lines and
// meter1 .. 4 found, all hidden.
void MobilePhone::enterMenu(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    const Phone& p = data->career->phone;
    s.clearMenu("Menu");
    for (size_t k = 0; k < 4; ++k) {
        if (p.call.answers[k] == kPhoneNone) continue;
        s.addMenuItem("Menu", lineText(host, p.call.answers[k]), static_cast<float>(k));
    }
    for (const char* name : {"data1", "data2", "data3", "data4"}) s.setMultiline(name, true);
    for (const char* name : {"data1", "data2", "data3", "data4", "meter1", "meter2", "meter3", "meter4"}) {
        s.setHidden(name, true);
    }
}

// 0x004D27C0: a host challenge's call (type 2) the first time (+0xFA4) shows the host rights note (1636). SELECT on an
// item answers (0x004B6B20), CLICK, the window goes. Else the highlighted item shows: with the crew of the challenges
// (+0xF9C) the challenge (the advised one, +0x6C, for an advice to take one, 9; else the item, below 3): PIC_PICTURE
// and PIC_BORDER hidden, data1 its kind (372 + type) with " (Completed)" when done, data2 its car's name, data3 its
// target: a cruise "Score <n>", speed "<n> MPH" or by the settings' units "<n x 1.6093> KPH", else "Time <time>"; with
// the crewmate (+0xFA0) data1 "Skill" with meter1 its driving x 0.01 and data3 "Composure" with meter3 its recovery x
// 0.01; else all hidden (0x004D2700).
void MobilePhone::updateMenu(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Phone& p = career.phone;
    if (p.hostNoteShown == 0 && p.call.type == CallType::HostChallenge) {
        p.hostNoteShown = 1;
        host.message(MessageType::Continue, texts.at(kTextHostRights), false, nullptr);
        return;
    }
    const bool item = s.menuSize("Menu") != 0;
    const float value = item ? s.menuItemValue("Menu", s.selectedMenuItem("Menu")) : 0.0f;
    if (event == input::kSelect && item) {
        answerCall(host, x87::chop(value));
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
        return;
    }
    ui::Screen* phone = host.phoneScreen();
    const auto pictures = [&](bool hidden) {
        if (phone == nullptr) return;
        phone->setHidden("PIC_PICTURE", hidden);
        phone->setHidden("PIC_BORDER", hidden);
    };
    const auto shown = [&](const char* name, const std::wstring& text) {
        s.setHidden(name, false);
        s.setText(name, text);
    };
    const int32_t crew = phoneCrew(p.challengesCrew);
    const int32_t mate = phoneCrew(p.shownCrewmate);
    const int32_t advice = career.advisor.advice;
    const long near = x87::nearest(value);
    const bool challenge = crew >= 0 && crew < 9 && (advice == 9 ? near < 1 : near < 3);
    if (challenge) {
        pictures(true);
        const auto k = static_cast<size_t>(std::clamp<long>(advice == 9 ? career.advisor.challenge
                                                                         : near, 0, 2));
        const CrewChallenge& c = career.crews[static_cast<size_t>(crew)].challenges[k];
        std::wstring kind =
            texts.at(kTextChallengeTypes + static_cast<size_t>(std::max(static_cast<int32_t>(c.type), 0)));
        if (c.done != 0) kind += L" (" + texts.at(kTextCompleted) + L")";
        shown("data1", kind);
        const CarDef* def = data->defs.byId(c.car);
        shown("data2", def != nullptr ? toWide(def->displayName) : std::wstring());
        std::wstring target;
        const int language = host.language().index;
        if (c.type == ChallengeType::Showoff) {
            target = texts.at(kTextScore) + L" " + toWide(groupThousands(c.value, language));
        } else if (c.type == ChallengeType::Speed) {
            if (host.settingsValue(save::Settings::kMph) != 0) {
                target = toWide(groupThousands(c.value, language)) + L" " + texts.at(kTextMph);
            } else {
                const int32_t kph = x87::chop(static_cast<float>(c.value) * 1.6093f);
                target = toWide(groupThousands(kph, language)) + L" " + texts.at(kTextKph);
            }
        } else {
            target = texts.at(kTextTime) + L" " + toWide(hudTime(static_cast<float>(c.value)));
        }
        shown("data3", target);
    } else if (mate >= 0 && mate < static_cast<int32_t>(career.crewmates.size())) {
        const Crewmate& m = career.crewmates[static_cast<size_t>(mate)];
        pictures(true);
        shown("data1", texts.at(kTextSkill));
        s.setHidden("meter1", false);
        s.setMeter("meter1", m.drivingAbility * 0.01f);
        shown("data3", texts.at(kTextComposure));
        s.setHidden("meter3", false);
        s.setMeter("meter3", m.recover * 0.01f);
    } else {
        for (const char* name : {"data1", "data2", "data3", "data4", "meter1", "meter2", "meter3", "meter4"}) {
            s.setHidden(name, true);
        }
    }
}

// 0x004D2CB0: Menu emptied, each contact's name an item (its place the value).
void MobilePhone::enterContacts(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    const Phone& p = data->career->phone;
    s.clearMenu("Menu");
    for (int32_t i = 0; i < p.contactCount && i < static_cast<int32_t>(p.contacts.size()); ++i) {
        const auto& name = p.contacts[static_cast<size_t>(i)].name;
        s.addMenuItem("Menu", widenSigned(std::string_view(name.data(), strnlen(name.data(), name.size()))),
                      static_cast<float>(i));
    }
}

// 0x004D2D90: a new phone model ([0x00725618]) shown on the phone's Mobile. An advice to take a challenge (9) or race
// for pinks (7) calls the contact of its crew (+0x68, +0x70; past the contacts when none) at once (0x004B6710), the
// window goes, Dial, the ring's time -1. PIC_PICTURE the highlighted contact's crew's leader. SELECT calls the
// highlighted contact the same way (CLICK). With the phone opened, PGDN (CLICK) opens CMSMobileChoose over it; else
// BACK idles the phone (5 .. 15 s), CLICK, the window goes.
void MobilePhone::updateContacts(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Phone& p = career.phone;
    ui::Screen* phone = host.phoneScreen();
    if (p.model != m_shownModel) {
        m_shownModel = p.model;
        if (phone != nullptr) showModel(host, *phone);
    }
    const bool item = s.menuSize("Menu") != 0;
    const int32_t value = item ? x87::chop(s.menuItemValue("Menu", s.selectedMenuItem("Menu"))) : 0;
    const auto call = [&](int32_t contact) {
        dialContact(career, contact, gameRandom());
        host.request("", SwitchMode::Pop);
        m_sound = host.uiSounds().play("Dial");
        m_ring = -1.0f;
    };
    const auto contactOf = [&](int32_t crew) {
        int32_t i = 0;
        while (i < p.contactCount && p.contacts[static_cast<size_t>(i)].crew != crew) ++i;
        return i;
    };
    int32_t shown = value;
    const int32_t advice = career.advisor.advice;
    if (advice == 9) {
        shown = contactOf(career.advisor.crew);
        call(shown);
    }
    if (advice == 7) {
        shown = contactOf(career.advisor.targetCrew);
        call(shown);
    }
    // Stock problem: past the contacts the picture reads the contact after the last.
    if (shown >= 0 && shown < static_cast<int32_t>(p.contacts.size())) {
        const int32_t crew = p.contacts[static_cast<size_t>(shown)].crew;
        if (crew >= 0 && crew < 9 && phone != nullptr) {
            const auto id = static_cast<size_t>(career.crews[static_cast<size_t>(crew)].id);
            if (id < std::size(kLeaderPictures)) phone->setTexture("PIC_PICTURE", kLeaderPictures[id]);
        }
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        call(value);
    }
    if (p.state == PhoneState::Opened && event == input::kSelect2) {
        host.sound("CLICK");
        host.request("CMSMobileChoose", SwitchMode::Push);
    } else if (event == input::kBack) {
        setPhoneState(p, PhoneState::Idle, gameRandom());
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
    }
}

// 0x004C3DB0: +0x0C and +0x10 -1.
void MobilePhone::enterChoose() {
    m_choose = Choose{};
}

// 0x004C3DD0 over the phone: a new model highlighted (MOBILENAMES, +0x0C) shows the pictures 0 .. 3 and hides the
// model's. A "yes" (+0x10) pays the model's price (0x00718F70: 100, 100, 150, 150, 200, 200), the model becomes the
// phone's and the window goes. BACK (CLICK) closes it (a new career's choice is the window cmsmobilechoose). SELECT
// (CLICK): the model in use closes it; with the price in money "Buy this phone for " (1824) and the price asked
// (type 3, on NO) into +0x10; else "Cannot buy this phone. Insufficient cash." (1825).
void MobilePhone::updateChoose(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Phone& p = career.phone;
    const bool item = s.menuSize("MOBILENAMES") != 0;
    const float value = item ? s.menuItemValue("MOBILENAMES", s.selectedMenuItem("MOBILENAMES")) : 0.0f;
    const int32_t model = x87::chop(value);
    const int32_t price = kPhonePrices[std::clamp(model, 0, 5)];
    if (static_cast<float>(m_choose.item) != value) {
        m_choose.item = model;
        for (const char* name : {"0", "1", "2", "3"}) s.show(name, false);
        s.setHidden(std::format("{}", model), true);
    }
    if (m_choose.answer == 1) {
        pay(career, price);
        p.model = model;
        host.request("", SwitchMode::Pop);
        return;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
    } else if (event == input::kSelect) {
        host.sound("CLICK");
        if (static_cast<float>(p.model) == value) {
            host.request("", SwitchMode::Pop);
        } else if (career.profile.money >= price) {
            host.message(MessageType::YesNo, texts.at(kTextBuyPhone) + toWide(moneyText(price, host.language().index)),
                         false, &m_choose.answer, true);
        } else {
            host.message(MessageType::Continue, texts.at(kTextNoPhoneCash), false, nullptr);
        }
    }
}

// 0x004B6B20 by the call's type, with the answer (the item's place):
// 0 (a crewmate asks to join): +0xFA0 0; yes: it joins (0x004B80D0), sample 0x25, line 316; no: sample 0x26, line 317.
// 1 (the player asks a crewmate): yes: it joins, sample 0x22, line 320; no: the phone idles.
// 2 (host challenge): yes: the next free day (0x004A6A10, +0x14 and +0x10), the host rights race (0x004A2DC0),
// sample 2, line 322, the event gone to (+0x0C); no: the crew's respect to at most its last step less 100 (and not
// below 0) noted for its message (+0x18 .. +0x24), sample 3, line 323.
// 5 (pink slip race): yes: the next free day, the pink slip race (0x004A2F80), sample 6, line 325, gone to; no: the
// crew's respect less 25 noted, sample 5, line 326.
// 8 (the player calls a crew's leader; with the advice to race for pinks the first answer is the second, with the
// advice to take a challenge the second the third; without cars the second idles the phone): first: the challenge
// texts (the advised challenge's kind, else each of the three with " (c)" when done) as the answers with END CALL,
// sample 0x15, line 332, the crew shown (+0xF9C); second: a crew past its second step with fewer than 3 pink slip
// races and fewer than 55 cars: the next free day, the pink slip race, sample 8, line 328, gone to; else sample 7,
// line 329; third: the phone idles.
// 9 (the challenges offered): END CALL (the second with the advice, else the fourth) idles; else the next free day,
// +0xF9C 0, and with a garage car of the challenge's model (0x004A8C80) its event (0x004A3130), sample 9, line 333,
// gone to; else sample 10, line 334.
// 10 .. 15 idle the phone; 16 .. 19 set the crew's respect to a step less 20 (100, 300, 1000, 600; stock problem: 18
// goes on into 19), its gains (0x004BBD80) and idle the phone. The changed calls are of type 11 with END CALL.
void MobilePhone::answerCall(CareerHost& host, int32_t answer) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Career& career = *data->career;
    Phone& p = career.phone;
    CrtRandom& random = gameRandom();
    const int32_t crew = p.call.crew;
    const auto reply = [&](int32_t sample, int32_t text) {
        setPhoneCall(p, CallType::Reply, sample, crew, text, phonetext::kEndCall, kPhoneNone, kPhoneNone, kPhoneNone);
    };
    const auto freeDay = [&] { nextFreeDay(career.calendar, p.eventDay, p.eventMonth); };
    const auto crewAt = [&](int32_t c) -> Crew& { return career.crews[static_cast<size_t>(std::clamp(c, 0, 8))]; };
    const auto setRespect = [&](int32_t step) {
        Crew& c = crewAt(crew);
        setCrewRespect(c, static_cast<float>(step - 20));
        oj::updateRespect(career, c, data->tables);
        setPhoneState(p, PhoneState::Idle, random);
    };
    switch (p.call.type) {
    case CallType::LetJoin:
        p.shownCrewmate = 0;
        if (answer == 0) {
            crewmateJoins(career, crew);
            reply(0x25, 316);
        } else if (answer == 1) {
            reply(0x26, 317);
        }
        break;
    case CallType::JoinMyCrew:
        if (answer == 0) {
            crewmateJoins(career, crew);
            reply(0x22, 320);
        } else if (answer == 1) {
            setPhoneState(p, PhoneState::Idle, random);
        }
        break;
    case CallType::HostChallenge:
        if (answer == 0) {
            freeDay();
            data->phoneEvent(random, crew, p.eventMonth, p.eventDay, true);
            reply(2, 322);
            p.goToEvent = 1;
        } else if (answer == 1) {
            const auto respect = static_cast<float>(crewAt(crew).respect);
            const float most = static_cast<float>(data->tables.respectSteps[3]) - 100.0f;
            const float kept = respect < 0.0f ? 0.0f : (respect > most ? most : respect);
            p.respectAfter = kept;
            p.respectPending = 1;
            p.respectCrew = crew;
            p.respectChange = kept - respect;
            reply(3, 323);
        }
        break;
    case CallType::PinkSlip:
        if (answer == 0) {
            freeDay();
            data->phoneEvent(random, crew, p.eventMonth, p.eventDay, false);
            reply(6, 325);
            p.goToEvent = 1;
        } else if (answer == 1) {
            p.respectPending = 1;
            p.respectChange = -25.0f;
            p.respectCrew = crew;
            p.respectAfter = static_cast<float>(crewAt(crew).respect) - 25.0f;
            reply(5, 326);
        }
        break;
    case CallType::LeaderOffer: {
        const int32_t advice = career.advisor.advice;
        int32_t a = answer;
        if (advice == 7 && a == 0) a = 1;
        if (advice == 9 && a == 1) a = 2;
        if (career.garage.empty() && a == 1) a = 2;
        Crew& c = crewAt(crew);
        if (a == 0) {
            if (advice == 9) {
                const auto k = static_cast<size_t>(std::clamp(career.advisor.challenge, 0, 2));
                m_challenges[0] = texts.at(
                    kTextChallengeTypes + static_cast<size_t>(std::max(static_cast<int32_t>(c.challenges[k].type), 0)));
                setPhoneCall(p, CallType::Challenges, 0x15, c.id, 332, kPhoneChallenge, phonetext::kEndCall, kPhoneNone,
                             kPhoneNone);
            } else {
                for (size_t k = 0; k < 3; ++k) {
                    const CrewChallenge& ch = c.challenges[k];
                    m_challenges[k] =
                        texts.at(kTextChallengeTypes + static_cast<size_t>(std::max(static_cast<int32_t>(ch.type), 0)));
                    if (ch.done != 0) m_challenges[k] += L" (c)";
                }
                setPhoneCall(p, CallType::Challenges, 0x15, c.id, 332, kPhoneChallenge, kPhoneChallenge - 1,
                             kPhoneChallenge - 2, phonetext::kEndCall);
            }
            p.challengesCrew = crew + 1;
        } else if (a == 1) {
            if (c.stage[2] != 0 && c.pinkSlipsWon < 3 && career.garage.size() < 55) {
                freeDay();
                data->phoneEvent(random, crew, p.eventMonth, p.eventDay, false);
                setPhoneCall(p, CallType::Reply, 8, crew, 328, phonetext::kEndCall, kPhoneNone, kPhoneNone, kPhoneNone);
                p.goToEvent = 1;
            } else {
                reply(7, 329);
            }
        } else if (a == 2) {
            setPhoneState(p, PhoneState::Idle, random);
        }
        break;
    }
    case CallType::Challenges: {
        const int32_t advice = career.advisor.advice;
        int32_t k = answer;
        if (advice == 9) {
            if (answer == 1) {
                setPhoneState(p, PhoneState::Idle, random);
                break;
            }
            freeDay();
            p.challengesCrew = 0;
            k = career.advisor.challenge;
        } else {
            if (answer == 3) {
                setPhoneState(p, PhoneState::Idle, random);
                break;
            }
            freeDay();
            p.challengesCrew = 0;
        }
        const Crew& c = crewAt(crew);
        const CrewChallenge& ch = c.challenges[static_cast<size_t>(std::clamp(k, 0, 2))];
        if (playerCarsOfModel(career, ch.car) != 0) {
            data->challengeEvent(random, c.id, p.eventMonth, p.eventDay, k);
            setPhoneCall(p, CallType::Reply, 9, crew, 333, phonetext::kEndCall, kPhoneNone, kPhoneNone, kPhoneNone);
            p.goToEvent = 1;
        } else {
            setPhoneCall(p, CallType::Reply, 10, crew, 334, phonetext::kEndCall, kPhoneNone, kPhoneNone, kPhoneNone);
        }
        break;
    }
    case CallType::TextMessage:
    case CallType::Reply:
    case CallType::Unused12:
    case CallType::Unused13:
    case CallType::Unused14:
    case CallType::Unused15: setPhoneState(p, PhoneState::Idle, random); break;
    case CallType::RespectFirst: setRespect(data->tables.respectSteps[0]); break;
    case CallType::RespectSecond: setRespect(data->tables.respectSteps[1]); break;
    case CallType::RespectLast:
        // Stock problem: no break; 19 follows.
        setRespect(data->tables.respectSteps[3]);
        setRespect(data->tables.respectSteps[2]);
        break;
    case CallType::RespectThird: setRespect(data->tables.respectSteps[2]); break;
    default:
        break;
    }
}

void MobilePhone::stopRing(CareerHost& host) {
    if (m_sound != 0) {
        if (audio::Player* player = host.uiSounds().player()) player->stop(m_sound);
        m_sound = 0;
    }
}

}  // namespace oj::frontend
