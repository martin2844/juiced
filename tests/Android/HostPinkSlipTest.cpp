#include "Core/Input/Input.h"
#include "Game/Frontend/Career/PinkSlipCrew.h"
#include "Game/Frontend/Career/CrewRaceSetup.h"
#include "Game/Frontend/Career/GridWalk.h"
#include "Game/Frontend/Career/CharacterScreen.h"
#include "Framework/Fakes/FakeCareerHost.h"
#include "Core/Career/CareerEvents.h"
#include "Core/Cars/CarPhysics.h"
#include "Mods/Runtime.h"
#include "Mods/Creative.h"
#ifdef OJ_LINUX
#include "Linux/Render/GlesDevice.h"
#else
#include "Android/Render/GlesDevice.h"
#endif
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

namespace oj::test {
void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
}
struct VisualHost : oj::test::FakeCareerHost {
    oj::render::GlesDevice* gpu = nullptr;
    std::shared_ptr<oj::ui::Screen> open(const std::string& name) {
        if (texts.size() == 0) texts.load(std::filesystem::path("scripts") / (std::string(lang.name) + ".txt"));
        auto s = std::make_shared<oj::ui::Screen>(device(), lang);
        oj::test::check(s->load(name), "Load screen " + name);
        base = s.get(); return s;
    }
    oj::render::IRenderDevice& device() override { if (gpu) return *gpu; return nullDevice; }
};
int main(int argc, char** argv) {
    using namespace oj;
    using test::check;
    if (argc < 3) return 2;
    try {
        auto game = std::filesystem::absolute(argv[1]), packages = std::filesystem::absolute(argv[2]);
        std::filesystem::current_path(game);
        mods::runtime::load(packages);
        std::cout << "Creative " << mods::enabled() << '\n';
        check(mods::runtime::feature("host_pink_slips"), "Package enables hosted pink slips");
        CrtRandom random; GameData data(game); std::string error;
        check(data.load(random, &error), error);
        auto& career = *data.career;
        career.garage.clear();
        auto car = makeCar(data.defs.byId(1));
        rateCar(car, data.dynamics->of(*car.def), data.catalogue);
        car.grid = GridDamage{};
        career.garage.push_back(car);
        career.profile.money = 12345;
        for (auto& crew : career.crews) { crew.respect = 0; crew.stage.fill(0); }
        career.crews[1].respect = 599;
        career.crews[2].respect = 600;
        career.crews[3].respect = 900;
        career.calendar.chosenMonth = career.calendar.month;
        career.calendar.chosenDayOfMonth = career.calendar.dayOfMonth;
        // Test calendar only: no collisions or simulated days needed.
        for (auto& event : career.calendar.events) event.used = 0;
        std::unique_ptr<render::GlesDevice> gpu;
        std::filesystem::path captures;
        if (argc > 3) {
            captures = std::filesystem::absolute(argv[3]);
            std::filesystem::create_directories(captures);
            SDL_Init(SDL_INIT_VIDEO);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            auto* window = SDL_CreateWindow("Pink slip setup validation", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
            render::DeviceDesc desc; desc.window = reinterpret_cast<HWND>(window);
            gpu = std::make_unique<render::GlesDevice>(desc);
        }
        VisualHost host; host.data = &data; host.gpu = gpu.get();
        auto capture = [&](ui::Screen& screen, const char* name) {
            if (!gpu) return;
            screen.finishTransitions();
            for (int i=0;i<10;++i) {
                gpu->beginFrame(); screen.update(1.f/60);
                screen.draw(i/60.f, 1.f/60); gpu->endFrame();
            }
            check(gpu->capture((captures / name).wstring()), "Capture setup screen");
        };
        auto setup = host.open("CMSHostMeetingSetup");
        auto race = host.open("CMSHostMeetingSetupRace");
        auto summary = host.open("CMSHostMeetingSummary");
        check(setup && race && summary, "Real game setup screens load");
        host.meeting.askDefaults(); host.meeting.enterSetup(host, *setup);
        setup->setCurrentButton("Type");
        for (int i = 0; i < 3; ++i) host.meeting.updateSetup(host, *setup, input::kRight);
        check(setup->textOf("TypeText") == L"PINK SLIPS", "Pink slips selectable past Sprint");
        check(setup->textOf("LocationText").find(L"600 respect") != std::wstring::npos,
              "Exactly 600 qualifies and 599 is skipped");
        setup->setCurrentButton("Location");
        host.meeting.updateSetup(host, *setup, input::kRight);
        check(setup->textOf("LocationText").find(L"900 respect") != std::wstring::npos, "Eligible rival cycles");
        host.meeting.updateSetup(host, *setup, input::kRight);
        check(setup->textOf("LocationText").find(L"600 respect") != std::wstring::npos, "Selection wraps eligible rivals only");
        capture(*setup, "pink-setup.bmp");
        career.crews[2].respect = career.crews[3].respect = 599;
        host.meeting.updateSetup(host, *setup, input::kRight);
        check(setup->textOf("Attendance") == L"0", "No eligible rival blocks attendance");
        const auto requests = host.requests.size();
        host.meeting.updateSetup(host, *setup, input::kSelect);
        check(host.requests.size() == requests, "No eligible rival cannot advance");
        career.crews[2].respect = 600; career.crews[3].respect = 900;
        host.meeting.updateSetup(host, *setup, input::kLeft);
        host.meeting.updateSetup(host, *setup, input::kRight); // wrap back to 600
        host.meeting.enterRace(host, *race);
        check(race->textOf("prizeMoneyText1") == L"YOUR CAR", "Setup explains car stakes");
        auto raceInput = [&](uint32_t key) {
            race->moveHighlight(key); // Same order as Frontend::update.
            host.meeting.updateRace(host, *race, key);
        };
        check(_stricmp(race->currentButton().c_str(), "Type") == 0, "Pink race starts on selectable type");
        raceInput(input::kDown);
        check(_stricmp(race->currentButton().c_str(), "Route") == 0, "Type navigates down to route");
        raceInput(input::kUp);
        check(_stricmp(race->currentButton().c_str(), "Type") == 0, "Route navigates back up to type");
        const auto circuitText = race->textOf("TypeText");
        raceInput(input::kRight);
        const auto sprintText = race->textOf("TypeText");
        check(sprintText != circuitText, "Directional input selects Sprint");
        raceInput(input::kLeft);
        check(race->textOf("TypeText") == circuitText, "Directional input selects Circuit");
        raceInput(input::kLeft);
        check(race->textOf("TypeText") != circuitText && race->textOf("TypeText") != sprintText,
              "Directional input selects Point to Point");
        capture(*race, "pink-race.bmp");
        const auto raceRequests = host.requests.size();
        career.garage.resize(55, car);
        host.meeting.updateRace(host, *race, input::kSelect);
        check(host.requests.size() == raceRequests, "Full garage blocks the car wager");
        career.garage.clear();
        host.meeting.updateRace(host, *race, input::kSelect);
        check(host.requests.size() == raceRequests, "No player car blocks the car wager");
        career.garage.push_back(car);
        host.meeting.updateRace(host, *race, input::kSelect);
        host.meeting.enterSummary(host, *summary);
        capture(*summary, "pink-summary.bmp");
        for (size_t i = 0; i < summary->menuSize("menu"); ++i)
            if (summary->menuItemValue("menu", i) == 2.f) summary->selectMenuItem("menu", i);
        host.meeting.updateSummary(host, *summary, input::kSelect);
        check(!host.messages.empty() && host.messages.back().answer, "Explicit stakes confirmation");
        *host.messages.back().answer = 1;
        career.crews[2].respect = 599;
        host.meeting.updateSummary(host, *summary, 0);
        check(host.meetings == 0 && career.profile.money == 12345, "Respect drop before commit prevents event without charge");
        career.crews[2].respect = 600;
        host.meeting.enterSummary(host, *summary);
        host.meeting.updateSummary(host, *summary, input::kSelect);
        check(host.messages.back().answer != nullptr, "Confirmation available again at 600");
        *host.messages.back().answer = 1;
        host.meeting.updateSummary(host, *summary, 0);
        auto* event = career.meetingEvent;
        check(event && host.meetings == 1, "Hosted event created");
        check(event->type == EventKind::PointToPoint && event->round.type == EventKind::PointToPoint,
              "Chosen physical race type survives pink-slip event creation");
        check(event->pinkSlip == 1 && event->crew == 2 && event->hostedByPlayer == 0 && event->challengeIndex == -1,
              "Ordinary pink-slip semantics, not a host-rights challenge");
        check(event->round.drivers == 2 && event->round.teamSize == 1 && event->round.crews[0] && event->round.crews[2],
              "Exactly player and selected opponent");
        check(event->entranceFee == 0 && event->prizeTotal == 0 && career.profile.money == 12345, "Cars are the stakes, no cash charge");
        Career restored;
        check(restored.read(career.write(), &error), "Existing career serialization accepts event");
        bool saved = false;
        for (const auto& e : restored.calendar.events) saved |= e.used && e.pinkSlip && e.crew == 2;
        check(saved, "Opponent and pink-slip flag survive reload");
        career.profile.raceCarCount = 1; career.profile.raceCars[0] = 0; career.profile.raceMembers[0] = 0;
        data.setupRace(random);
        check(career.session.count == 2, "Native race session has two racers");
        check(frontend::pinkSlipCrewSelection(*event, event->round), "Crew package permits driver selection for a pink duel");
        career.profile.memberCount = 2;
        career.members[1] = 1;
        auto crewScreen = host.open("CMSCrewRaceSetup");
        frontend::CrewRaceSetup crewSetup;
        crewSetup.enter(host, *crewScreen);
        crewScreen->setCurrentButton("driver2value");
        crewSetup.update(host, *crewScreen, input::kSelect);
        crewSetup.update(host, *crewScreen, input::kSelect); // Finished, selected by the first press.
        check(career.profile.raceMembers[0] == 1 && career.profile.raceCarsChosen == 0,
              "Crew selector delegates the duel without a human driver");
        check(host.requests.back().first == "CMSRaceSetup", "Delegated driver proceeds to car selection");
        career.garage[0].base = 0xFF112233;
        car.base = 0xFF445566;
        car.grid->made[7] = true;
        car.grid->saved[7] = 6.0f;
        career.garage.push_back(car);
        car.base = 0xFF778899;
        career.garage.push_back(car); // Unselected car after the stake catches wrong-slot loss.
        career.profile.raceCars[0] = 1;
        career.profile.racing = 1;
        career.profile.watching = 0;
        career.session.needsSetup = 1;
        data.setupRace(random);
        bool delegated = false;
        for (unsigned i = 0; i < career.session.count; ++i) {
            const auto& racer = career.session.racers[i];
            if (racer.crew == 0) delegated = racer.member == 1 && racer.driver == 0 && racer.car == 1;
        }
        check(delegated && leaderCar(career, 0) == 1, "AI teammate and title preview use the selected car, not garage slot zero");
        frontend::GridWalk grid;
        auto gridScreen = host.open("CMSGridWalk");
        grid.enter(host, *gridScreen);
        gridScreen->setCurrentButton("Go");
        host.requests.clear();
        grid.update(host, *gridScreen, input::kSelect);
        check(career.bets.placed && career.bets.pinkSlip && career.bets.crew == 2,
              "Delegated driver must stake the title");
        check(host.requests.size() == 1 && host.requests[0].first == "careerload",
              "Title confirmation cannot be overwritten by immediate race loading");
        grid.leave();
        // Exercise the actual confirmation/autosave/reinsert contract before settlement.
        frontend::CharacterScreen character;
        auto characterScreen = host.open("character");
        data.saving = 1;
        host.requests.clear();
        host.messages.clear();
        host.messageOpen = false;
        host.switchPending = false;
        character.enter(host, *characterScreen);
        bool autosaved = false;
        for (int step = 0; step < 100 && !autosaved; ++step) {
            character.update(host, *characterScreen, input::kSelect);
            for (auto& message : host.messages) {
                if (message.answer) *message.answer = 1;
            }
            if (host.messageOpen) host.closeMessage();
            for (const auto& request : host.requests) autosaved |= request.first == "autosave";
        }
        check(autosaved, "Actual title confirmation reaches autosave");
        check(career.garage.size() == 2 && career.garage[0].base == 0xFF112233 &&
                  career.garage[1].base == 0xFF778899,
              "Autosave excludes exactly the selected middle car");
        Career autosave;
        check(autosave.read(career.write(), &error) && autosave.garage.size() == 2 &&
                  autosave.garage[0].base == 0xFF112233 && autosave.garage[1].base == 0xFF778899,
              "Serialized autosave preserves both unselected cars");
        host.switchPending = false;
        character.update(host, *characterScreen, 0);
        check(career.garage.size() == 3 && career.garage.back().base == 0xFF445566 &&
                  career.garage.back().grid && career.garage.back().grid->saved[7] == 6.0f,
              "Stake returns at garage end without repairing existing damage");
        check(career.profile.raceCars[0] == 2 && leaderCar(career, 0) == 2,
              "Delegated racer references reinserted stake, not old garage index");
        character.leave(host);
        const auto beforeSettlement = career.garage;
        const auto rival = career.opponentCars.at(static_cast<size_t>(leaderCar(career, 2)));
        career.bets.crew = 2;
        career.bets.won = 1;
        pinkSlipCar(career, data.catalogue);
        check(career.garage.size() == 4 && career.garage[2].base == 0xFF445566 &&
                  career.garage.back().modelName() == rival.modelName(),
              "Win keeps own stake and transfers the selected rival car");
        Career won;
        check(won.read(career.write(), &error) && won.garage.size() == 4 &&
                  won.garage[2].base == 0xFF445566 && won.garage.back().modelName() == rival.modelName(),
              "Win ownership survives save reload");
        // Start loss from the same pre-settlement garage, not from the winning garage.
        career.garage = beforeSettlement;
        career.bets.won = 0;
        pinkSlipCar(career, data.catalogue);
        check(career.garage.size() == 2 && career.profile.raceCarKept == 0 &&
                  career.garage[0].base == 0xFF112233 && career.garage[1].base == 0xFF778899,
              "Loss removes own selected stake and preserves both unselected cars");
        Career lost;
        check(lost.read(career.write(), &error) && lost.garage.size() == 2 &&
                  lost.garage[0].base == 0xFF112233 && lost.garage[1].base == 0xFF778899,
              "Loss ownership survives save reload");
        host.meeting.askDefaults(); host.meeting.enterSetup(host, *setup);
        check(setup->textOf("Location") != L"OPPONENT", "Regular setup restores location label");
        host.meeting.enterRace(host, *race);
        check(race->textOf("Crewnum") != L"OPPONENT" && race->textOf("prize") != L"STAKES",
              "Regular race restores team and prize labels");
        mods::runtime::load({});
        check(!frontend::pinkSlipCrewSelection(*event, event->round), "Disabling crew mod restores player-only selection");
        test::FakeCareerHost disabled; disabled.data = &data;
        auto ordinary = disabled.open("CMSHostMeetingSetup");
        disabled.meeting.askDefaults(); disabled.meeting.enterSetup(disabled, *ordinary);
        ordinary->setCurrentButton("Type");
        for (int i = 0; i < 4; ++i) disabled.meeting.updateSetup(disabled, *ordinary, input::kRight);
        check(ordinary->textOf("TypeText") != L"PINK SLIPS", "Disabling mod restores ordinary type choices");
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
