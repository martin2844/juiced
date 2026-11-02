// Integration tests: the respect changes window CMSRespectChanges on a career of the game's data, the game's window
// and texts through a front end of the tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Core/Saves/Save.h"
#include "Game/Frontend/Career/MeetingSummary.h"
#include "Game/Frontend/Career/RespectChanges.h"

#include <format>
#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// An entry shown (0x004D1050) colours type and counter (green for a rise, red for a fall); comment, the crew's name
// and what it thinks, keeps the colour of its Font line (it was coloured too).
OJ_INTEGRATION_TEST(RespectChanges, CommentKeepsItsColour) {
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
    const auto colours = [&](int32_t kind) {
        data.career->respectChanges = oj::CareerNotes{};
        data.career->respectChanges.add(kind, kind < 10 ? 25.0f : -25.0f, 2.0f, 0.0f);
        oj::test::FakeCareerHost host;
        host.data = &data;
        const std::shared_ptr<oj::ui::Screen> screen = host.open("CMSRespectChanges");
        if (screen == nullptr) return std::string("no window");
        oj::frontend::RespectChanges changes;
        changes.enter(host, *screen);
        changes.update(host, *screen, oj::input::kSelect);
        return std::format("{:08X} {:08X} {:08X}", screen->fontColourOf(screen->find("type")),
                           screen->fontColourOf(screen->find("counter")),
                           screen->fontColourOf(screen->find("comment")));
    };
    const std::string rise = colours(0);
    const std::string fall = colours(10);
    check(rise == "FF32FF32 FF32FF32 00000000", "a rise: type and counter green, comment its own (" + rise + ")");
    check(fall == "FFFF3232 FFFF3232 00000000", "a fall: type and counter red, comment its own (" + fall + ")");
}

// A privilege's entry (kinds 20 .. 27, 0x004BBFC0) holds its crew in the change and 0 as the crew: the stock's take
// gave the player's crew that crew's number of respect. The fixed build leaves the respect of such an entry be.
OJ_INTEGRATION_TEST(RespectChanges, PrivilegeLeavesRespect) {
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
    career.respectChanges = oj::CareerNotes{};
    career.respectChanges.add(20, 3.0f, 0.0f, 0.0f);  // crew 3's privilege gained
    const int32_t before = career.crews[0].respect;
    oj::test::FakeCareerHost host;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("CMSRespectChanges");
    if (screen == nullptr) return;
    oj::frontend::RespectChanges changes;
    changes.enter(host, *screen);
    check(career.crews[0].respect == before, "respect changes (fixed): a privilege's entry gives no respect (" +
                                                 std::to_string(career.crews[0].respect - before) + ")");
}

// The history's step (0x004BD8A0) is the event's at 0x004BAEE0 (afterRaceResults), once the summaries' lists are
// empty. The stock's leaves of CMSMeetingSummary and CMSRespectChanges took one more each, two or three points of the
// stats' graph for one event; the fixed build's leaves take none.
OJ_INTEGRATION_TEST(RespectChanges, LeavesTakeNoHistoryStep) {
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
    oj::test::FakeCareerHost host;
    host.data = &data;
    const int32_t before = career.history.position;
    oj::frontend::RespectChanges changes;
    changes.leave(host);
    oj::frontend::MeetingSummary summary;
    summary.leave(host);
    check(career.history.position == before, "summaries' leaves (fixed): no history step (" +
                                                 std::to_string(career.history.position - before) + ")");
}

namespace {

// Capture at the actual save request boundary, before leave/resume can repair
// the in-memory state. A late correct value must not mask a stale snapshot.
class SummarySaveHost : public oj::test::FakeCareerHost {
public:
    std::vector<uint8_t> snapshot;
    int autosaves = 0;
    bool request(const std::string& state, oj::frontend::SwitchMode mode) override {
        if (state == "autosave") {
            ++autosaves;
            snapshot = data->career->write();
        }
        return FakeCareerHost::request(state, mode);
    }
};

void summarySaveCase(bool skip, bool saving, bool meetingFirst, bool respect) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "summary save: game data loaded: " + error);
        return;
    }
    oj::Career& career = *data.career;
    data.saving = saving ? 1 : 0;
    career.profile.money = 985400;
    career.history = oj::History{};
    career.history.position = 49; // exercise the 50-sample ring wrap, exactly once
    for (size_t i = 1; i < career.crews.size(); ++i) {
        career.crews[i].respect = career.crews[i].respectNow = static_cast<int>(i * 100);
    }
    career.meeting = oj::CareerNotes{};
    career.respectChanges = oj::CareerNotes{};
    if (respect) {
        // A host-loss clamp has already reduced the original 600 to its final
        // 424. Animating its -176 note again gives 248; that is not the value
        // that may be saved. Crew2 additionally exercises a respect floor.
        career.crews[1].respect = career.crews[1].respectNow = 424;
        career.crews[2].respect = 30;
        career.crews[2].respectNow = 50;
        career.respectChanges.add(10, -176.0f, 1.0f, 600.0f);
        career.respectChanges.add(10, -40.0f, 2.0f, 30.0f);
        career.respectChanges.add(20, 3.0f, 0.0f, 0.0f);
    }
    SummarySaveHost host;
    host.data = &data;
    oj::frontend::MeetingSummary meeting;
    std::shared_ptr<oj::ui::Screen> meetingScreen;
    if (meetingFirst) {
        career.meeting.add(0x1D, 0.0f, 0.0f, 0.0f);
        meetingScreen = host.open("CMSMeetingSummary");
        if (!meetingScreen) { check(false, "summary save: meeting screen loaded"); return; }
        meeting.enter(host, *meetingScreen);
        meeting.update(host, *meetingScreen, oj::input::kSelect);
        if (respect) {
            check(host.autosaves == 0 && career.history.position == 49,
                  "nonterminal meeting waits for respect before saving/recording history");
            check(!host.requests.empty() && host.requests.back().first == "CMSRespectChanges",
                  "nonterminal meeting routes to respect summary");
            meeting.leave(host);
            host.switchPending = false;
        }
    }
    oj::frontend::RespectChanges changes;
    std::shared_ptr<oj::ui::Screen> screen;
    if (respect) {
        screen = host.open("CMSRespectChanges");
        if (!screen) { check(false, "summary save: respect screen loaded"); return; }
        changes.enter(host, *screen);
        if (skip) changes.update(host, *screen, oj::input::kSelect2);
        for (int i = 0; i < 200 && host.autosaves == 0 && host.resultsContinued == 0; ++i) {
            changes.update(host, *screen, oj::input::kSelect);
        }
    }
    check(career.history.position == 0, "terminal summary records one event before save, including ring wrap");
    check(career.history.series[0][49] == 985400.0f, "terminal history captures actual final money");
    for (size_t i = 1; i < career.crews.size(); ++i) {
        check(career.crews[i].respect == career.crews[i].respectNow &&
                  career.history.series[i][49] == static_cast<float>(career.crews[i].respectNow),
              "terminal history/career use canonical final respect for crew " + std::to_string(i));
    }
    if (screen) {
        check(screen->textOf("val1") == L"424" && screen->textOf("val2") == L"50",
              "final respect screen agrees with saved host clamp and floor");
    }
    if (saving) {
        check(host.autosaves == 1 && !host.snapshot.empty(), "terminal summary requests exactly one autosave");
        oj::save::Archive archive;
        archive.entries["CMSGlobal.sav"] = host.snapshot;
        oj::save::Archive restoredArchive;
        auto restored = std::make_unique<oj::Career>();
        const auto encoded = archive.write();
        std::vector<std::byte> file;
        for (uint8_t byte : encoded) file.push_back(static_cast<std::byte>(byte));
        const bool valid = restoredArchive.read(file, &error) &&
            restored->read(restoredArchive.entries.at("CMSGlobal.sav"), &error);
        check(valid, "snapshot at save request decodes through real archive and Career readers: " + error);
        if (valid) {
            check(restored->history.position == 0 && restored->history.series == career.history.series,
                  "autosave snapshot already contains the single complete event history");
            for (size_t i = 1; i < career.crews.size(); ++i) {
                check(restored->crews[i].respect == career.crews[i].respectNow,
                      "reload preserves canonical respect for crew " + std::to_string(i));
            }
        }
    } else {
        check(host.autosaves == 0 && host.snapshot.empty(), "saving disabled: no autosave is requested");
    }
    const auto completed = career.write();
    host.switchPending = false; // emulate the autosave closing, not a new summary entry
    if (respect) {
        changes.update(host, *screen, oj::input::kNone);
        changes.leave(host);
    } else {
        meeting.update(host, *meetingScreen, oj::input::kNone);
        meeting.leave(host);
    }
    check(host.resultsContinued > 0 && host.resultsHistoryRecorded,
          "continuation tells the real frontend this event's history was already recorded");
    check(career.write() == completed, "resume/leave changes no persisted field after autosave");
}

} // namespace

OJ_INTEGRATION_TEST(RespectChanges, FinalStatePrecedesAutosave) {
    summarySaveCase(false, true, false, true);
    summarySaveCase(true, true, false, true);
}

OJ_INTEGRATION_TEST(RespectChanges, DisabledSavingStillFinalizesOnce) {
    summarySaveCase(true, false, false, true);
}

OJ_INTEGRATION_TEST(RespectChanges, MeetingAndRespectShareOneHistoryStep) {
    summarySaveCase(false, true, true, false);
    summarySaveCase(true, true, true, true);
}
