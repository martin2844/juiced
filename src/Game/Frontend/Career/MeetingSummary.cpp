// CMSMeetingSummary: the meeting's entries after a race.

#include "Game/Frontend/Career/MeetingSummary.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <iterator>

namespace oj::frontend {

namespace {

// The icon of a catalogue part's kind (1 .. 9; the suspension's otherwise).
constexpr std::array<const char*, 10> kPartKindIcons = {
    "ui_suspension_on", "ui_backbox_on", "ui_nitro_on", "ui_turbo_on", "ui_wheel_on",
    "ui_brake_on",      "ui_tyre_on",    "ui_ice_on",   "ui_airfilter_on", "ui_gears_on",
};

}  // namespace

// 0x004CEE50: the entry shown: logo, leader, leaderBorder, carLogo, crewPic, crewBorder, sponsorName and sponsorType
// hidden, then by the kind: 0x1C a prototype part won: sponsorName the supplier's UI_<name>_ON, sponsorType the part
// kind's icon, "WON PROTOTYPE PART", the car's make and model; 0x1D mods unlocked: carLogo the make's logo,
// "UNLOCKED NEW MODS", "Unlocked mods for " and the make and model; 0x1E a crew challenge won: leader the crew
// leader's picture and logo the crew's logo of the flyers (career +0x00), "WON CREW CHALLENGE",
// "MINIMUM RESPECT VALUE +50"; 0x1F and 0x20 a crewmate's skills (the crewmate of that number, of that member slot
// for 0x20): crewPic its ui_crewmember_<number>, "DRIVER SKILLS IMPROVED" / "WORSE" and its short name with what
// changed.
void MeetingSummary::showEntry(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    const Career& career = *data->career;
    for (const char* name :
         {"logo", "leader", "leaderBorder", "carLogo", "crewPic", "crewBorder", "sponsorName", "sponsorType"}) {
        s.setHidden(name, true);
    }
    const auto shown = [&](std::initializer_list<const char*> names) {
        for (const char* name : names) s.setHidden(name, false);
    };
    if (m_kind == 0x1E) shown({"logo", "leader", "leaderBorder"});
    if (m_kind == 0x1D) shown({"carLogo"});
    if (m_kind == 0x1C) shown({"sponsorName", "sponsorType"});
    if (m_kind == 0x20 || m_kind == 0x1F) shown({"crewPic", "crewBorder"});
    const auto modelText = [&](int id) {
        const int make = modelMake(id);
        return texts.at(kTextMakes + static_cast<size_t>(make)) + L" " +
               texts.at(kTextModels + static_cast<size_t>(std::max(id, 0)));
    };
    if (m_kind == 0x1D) {
        const int id = x87::chop(m_a);
        const int make = modelMake(id);
        s.setTexture("carLogo", make >= 1 && make <= 17 ? kMakeLogos[static_cast<size_t>(make)] : kMakeLogos[0]);
        s.setText("type", texts.at(kTextUnlocked));
        s.setText("comment", texts.at(kTextUnlocked + 1) + modelText(id));
    }
    if (m_kind == 0x1C) {
        const int id = x87::chop(m_c);
        const auto supplier = static_cast<size_t>(x87::chop(m_a));
        const std::vector<ExeTables::Brand>& brands = data->tables.brands;
        if (supplier < brands.size()) s.setTexture("sponsorName", std::format("UI_{}_ON", brands[supplier].name));
        const int kind = x87::chop(m_b);
        s.setTexture("sponsorType", kPartKindIcons[kind >= 1 && kind <= 9 ? static_cast<size_t>(kind) : 0]);
        s.setText("type", texts.at(kTextWonPart));
        s.setText("comment", modelText(id));
    }
    if (m_kind == 0x1E) {
        const int crew = x87::chop(m_b);
        if (crew >= 0 && crew < 9) {
            s.setTexture("logo", career.flyers.crews[static_cast<size_t>(crew)].logo);
            const auto id = static_cast<size_t>(career.crews[static_cast<size_t>(crew)].id);
            if (id < std::size(kLeaderPictures)) s.setTexture("leader", kLeaderPictures[id]);
        }
        s.setText("type", texts.at(kTextWonChallenge));
        s.setText("comment", texts.at(kTextWonChallenge + 1));
    }
    if (m_kind == 0x1F || m_kind == 0x20) {
        const int at = x87::chop(m_a);
        // Stock problem: crewmate -1 (0x1F) or a free member slot reads through a null or an old pointer.
        int number = -1;
        if (m_kind == 0x1F) {
            number = at;
        } else if (at >= 0 && at < 4) {
            number = career.members[static_cast<size_t>(at)];
        }
        if (number >= 0 && static_cast<size_t>(number) < career.crewmates.size()) {
            const Crewmate& mate = career.crewmates[static_cast<size_t>(number)];
            s.setTexture("crewPic", std::format("ui_crewmember_{:02}", mate.number));
            const std::string name(mate.shortName.data(), strnlen(mate.shortName.data(), mate.shortName.size()));
            s.setText("comment",
                      toWide(name) + texts.at(m_kind == 0x1F ? kTextSkillsUpLine : kTextSkillsDownLine));
        }
        s.setText("type", texts.at(kTextSkillsUp + (m_kind == 0x1F ? 0 : 1)));
    }
}

// 0x004CEAE0: type and comment take several lines; the first entry of the career's +0x38 list shown (0x004CEE50);
// Cash and Date (0x004A4130); the front end's music at a quarter of the settings' volume.
void MeetingSummary::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    const Career& career = *data->career;
    s.setMultiline("type", true);
    s.setMultiline("comment", true);
    m_step = 0;
    m_kind = career.meeting.kind[0];
    m_a = career.meeting.a[0];
    m_b = career.meeting.b[0];
    m_c = career.meeting.c[0];
    showEntry(host, s);
    careerCashTexts(s, career, host.language().index, texts);
    musicQuarter(host);
    m_finished = false;
}

// 0x004CECD0: finished, the window goes on (0x004BAEE0). SELECT (CLICK) shows the next entry; after the last the list
// is emptied (0x004B5CC0), the speech stopped (0x0040B600) and CMSRespectChanges follows while the +0x3C list has
// entries, else the window is finished (the save system's autosave over it when it is on).
void MeetingSummary::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    if (m_finished) {
        host.afterRaceResults();
        return;
    }
    if (event != input::kSelect) return;
    host.sound("CLICK");
    ++m_step;
    if (m_step == career.meeting.count) {
        clearNotes(career, career.meeting);
        host.stopSpeech();
        if (career.respectChanges.count != 0) {
            host.request("CMSRespectChanges", SwitchMode::Replace);
            return;
        }
        if (data->saving != 0) host.request("autosave", SwitchMode::Push);
        m_finished = true;
        return;
    }
    const auto k = static_cast<size_t>(std::clamp(m_step, 0, 49));
    m_kind = career.meeting.kind[k];
    m_a = career.meeting.a[k];
    m_b = career.meeting.b[k];
    m_c = career.meeting.c[k];
    showEntry(host, s);
}

// 0x004CEC30: leaving CMSMeetingSummary: its list emptied, the front end's music back to the settings' volume, the
// history's step (0x004BD8A0). Stock problem: the window is reached only from 0x004BAEE0, which takes the event's step
// once the lists are empty, so the event took two (three with CMSRespectChanges'); the fixed build takes none here.
void MeetingSummary::leave(CareerHost& host) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    clearNotes(career, career.meeting);
    host.setMusicVolume(host.settingsMusic(), false);
}

}  // namespace oj::frontend
