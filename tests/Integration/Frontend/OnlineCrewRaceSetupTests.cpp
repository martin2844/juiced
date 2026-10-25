// Integration tests: the online career's crew race setup CMSOnlineCrewRaceSetup on a career of the game's data, the
// game's window and texts through a front end of the tests (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/Frontend/Network/OnlineCrewRaceSetup.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

// The network windows' front end with no session.
class FakeNetworkHost : public oj::test::FakeWindowHost, public oj::frontend::NetworkHost {
public:
    bool netWait(oj::net::SessionStage, bool) override { return false; }
    bool netKeys(uint32_t, oj::net::SessionStage, bool) override { return false; }
    void netCountdown() override {}
    bool onlineCareer() const override { return true; }
    void onlineCareerRaceSetup() override {}
    void netHook() override {}
    bool netDropped() override { return false; }
    bool onlineTopMoving() const override { return false; }
    bool onlineJoinSession(int32_t) override { return false; }
    void delayGridWalkTimeout() override {}
    bool racePause() const override { return false; }
    void netFollow() override {}
    void onlineHostIntoLobby() override {}
};

}  // namespace

// The enter (0x004D6090) hides driver<n>value and selection<n> only: with two members border3 and crewpic3 stay as the
// .ui has them, shown (the offline window, 0x004D8AC0, hides them).
OJ_INTEGRATION_TEST(OnlineCrewRaceSetup, EmptySlotsKeepTheirBoxes) {
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
    career.profile.memberCount = 2;
    career.members = {0, 1, 0, 0};
    FakeNetworkHost host;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("CMSOnlineCrewRaceSetup");
    if (screen == nullptr) return;
    oj::frontend::OnlineCrewRaceSetup window;
    window.enter(host, *screen);
    check(!screen->hidden("driver2value") && !screen->hidden("border2") && !screen->hidden("crewpic2"),
          "a member's slot: its name, border and picture shown");
    check(screen->hidden("driver3value") && !screen->hidden("border3") && !screen->hidden("crewpic3") &&
              !screen->hidden("border4") && !screen->hidden("crewpic4"),
          "past the members: driver<n>value hidden, border<n> and crewpic<n> as the .ui has them (shown)");
}
