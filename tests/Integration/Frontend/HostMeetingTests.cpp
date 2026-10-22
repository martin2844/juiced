// Integration tests: the hosted meeting's windows (CMSHostMeetingSetup, CMSHostMeetingSetupRace,
// CMSHostMeetingSummary) on a career of the game's data, the game's windows and texts through a front end of the
// tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Career/CareerEvents.h"
#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Core/Race/RaceKinds.h"
#include "Game/Frontend/Career/HostMeeting.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

// The menu item of that value, or the menu's size.
size_t itemValued(const oj::ui::Screen& s, float value) {
    for (size_t i = 0; i < s.menuSize("menu"); ++i) {
        if (s.menuItemValue("menu", i) == value) return i;
    }
    return s.menuSize("menu");
}

}  // namespace

// The event the player hosts is raced as its round's race, not as a crew's challenge: the stock code marks it as no
// crew's challenge (0x004CDAE1: +0x06 -1), which the race's set up reads (0x004A3CC0). Left 0 the race took the
// player's crew's first challenge, a lap time of 0, and every car finished out of time at the start.
OJ_INTEGRATION_TEST(HostMeeting, EventRacesItsRound) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "game data and a first career loaded: " + error);
        return;
    }
    oj::Career& career = *data.career;
    career.profile.money = 1000000;
    // The location's crew (the setup's first, 0) may be hosted at: its fourth step and a respect of 1000.
    career.crews[0].stage[3] = 1;
    career.crews[0].respect = 1000;
    // The day chosen in the calendar (+0xF8C, +0xF90): the first after today with no event.
    oj::Calendar& calendar = career.calendar;
    int day = calendar.dayOfMonth;
    int month = calendar.month;
    do {
        oj::addDays(day, month, 1);
    } while (oj::eventAt(calendar, day, month) != nullptr);
    calendar.chosenDayOfMonth = day;
    calendar.chosenMonth = month;
    oj::test::FakeCareerHost host;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> setup = host.open("CMSHostMeetingSetup");
    const std::shared_ptr<oj::ui::Screen> race = host.open("CMSHostMeetingSetupRace");
    const std::shared_ptr<oj::ui::Screen> summary = host.open("CMSHostMeetingSummary");
    if (setup == nullptr || race == nullptr || summary == nullptr) return;
    host.meeting.askDefaults();
    host.meeting.enterSetup(host, *setup);
    host.meeting.enterRace(host, *race);
    host.meeting.updateRace(host, *race, oj::input::kSelect);  // the round kept
    host.meeting.enterSummary(host, *summary);
    summary->selectMenuItem("menu", itemValued(*summary, 2.0f));
    host.meeting.updateSummary(host, *summary, oj::input::kSelect);  // "Create event. Cost: .."
    const bool asked = !host.messages.empty() && host.messages.back().answer != nullptr;
    if (asked) *host.messages.back().answer = 1;
    host.meeting.updateSummary(host, *summary, 0);  // the event made, the days passed to it
    const oj::CalendarEvent* event = career.meetingEvent;
    check(asked && host.meetings == 1 && event != nullptr && event->crew == 0 && event->challengeIndex == -1,
          "the hosted event made, of no crew's challenge (+0x06 -1)");
    if (event == nullptr) return;
    data.startRace(random, 0);
    check(data.race.type == oj::RaceType::PointToPoint && data.race.challenge == oj::ChallengeOutcome::None &&
              data.race.lapTime == 0.0f,
          "its race: the round's point to point, no challenge");
}

// The summary's texts (0x004CC820) give fewer than two crews coming the reason 4 as well as the red: creating the event
// shows "Cannot Host Event - You need at least 2 crews in attendance" (480) instead of asking to create it (the setup's
// and the race's windows only colour the number).
OJ_INTEGRATION_TEST(HostMeeting, FewCrewsCannotHost) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "game data and a first career loaded: " + error);
        return;
    }
    oj::Career& career = *data.career;
    career.profile.money = 1000000;
    career.crews[0].stage[3] = 1;
    career.crews[0].respect = 1000;
    // No crew comes (0x004A9480): none of the first two (+0x78 1 or 2 always come), none past its second step.
    for (size_t k = 1; k < 9; ++k) {
        career.crews[k].id = 3;
        career.crews[k].stage[1] = 0;
    }
    oj::test::FakeCareerHost host;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> setup = host.open("CMSHostMeetingSetup");
    const std::shared_ptr<oj::ui::Screen> race = host.open("CMSHostMeetingSetupRace");
    const std::shared_ptr<oj::ui::Screen> summary = host.open("CMSHostMeetingSummary");
    if (setup == nullptr || race == nullptr || summary == nullptr) return;
    host.meeting.askDefaults();
    host.meeting.enterSetup(host, *setup);
    host.meeting.enterRace(host, *race);
    host.meeting.updateRace(host, *race, oj::input::kSelect);
    host.meeting.enterSummary(host, *summary);
    summary->selectMenuItem("menu", itemValued(*summary, 2.0f));
    host.messages.clear();
    host.meeting.updateSummary(host, *summary, oj::input::kSelect);
    const bool note = host.messages.size() == 1 && host.messages.back().answer == nullptr &&
                      host.messages.back().text == host.texts.at(480);
    check(note, "no crew coming: the note \"You need at least 2 crews in attendance\" (480), no question");
}
