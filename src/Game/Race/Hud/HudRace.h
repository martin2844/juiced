#pragma once
// What the race's HUD and its windows read of the race and its players, the players' messages, and what a race window's
// frame asks of the race front end.

#include "Core/Race/RaceKinds.h"
#include "Core/Saves/HighScores.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oj {

namespace net {
class NetSession;
}

class Cruise;

struct Car;
struct Career;
struct PartCatalogue;
struct RaceCar;
struct RaceSession;
class CarBody;
class RaceState;
namespace save {
struct Settings;
}
namespace scene {
class Engine;
class View3d;
}

// "%02d:%02d:%02d" of a time (0x0044FBD0): minutes, seconds and the fraction of the second x 99, each cut toward
// zero; below 0 the time is 0.
std::string hudTime(float seconds);

// The messages of a player (0x1C bytes, 0x004560B0): at most 10, the first shown in MSG.
class HudMessages {
public:
    struct Message {
        std::wstring text;
        float time = 0.0f;
        uint32_t colour = 0xFFFFFFFF;
    };
    // 0x00456140: all gone.
    void clear();
    // 0x004561A0 (with no colour of its own: the object's +0x18): false when 10 wait.
    bool push(const std::wstring& text, float time);
    // With a colour of its own (the colour's pointer given).
    bool push(const std::wstring& text, float time, uint32_t ownColour);
    // 0x00456300: the first's time runs down by the frame time (held within 0 .. 0.1); its alpha is 1, twice the
    // time below 0.5 s; below 0 it goes and the next is new.
    void update(float dt);

    std::vector<Message> list;
    bool changed = false;           // +0x10
    float alpha = 0.0f;             // +0x14
    uint32_t colour = 0xFFFFFFFF;   // +0x18
};

// What the HUD reads of a player's car.
struct HudCar {
    const RaceCar* car = nullptr;   // the vehicle (its competitor +0x494, its body +0x168)
    const Car* record = nullptr;    // +0x4E4: the HUD colours (+0x30, +0x34), the turbo lost to damage (+0x68)
    // The player's own car (P+4); a player without one (a watched race) has its camera's car (P+0x0C, +0x08) here
    // (0x0044B41C).
    bool own = true;
};

// What the HUD reads of the race.
struct HudRace {
    RaceState* race = nullptr;              // [G+0x34]: the count, the laps, the competitors
    RaceMode mode = RaceMode::Arcade;       // [G+0x6C]+0x00
    RaceType type = RaceType::TimedLap;     // [G+0x6C]+0x04
    float challengeLapTime = 0.0f;          // [G+0x6C]+0x74
    float challengeSpeed = 0.0f;            // [G+0x6C]+0x78
    ChallengeOutcome challenge = ChallengeOutcome::None;  // [G+0x6C]+0x7C
    Cruise* cruise = nullptr;               // [[G+0x80]+0x60]: the cruise manager
    int32_t heat = 0;                       // [G+0x6C]+0x08: a sprint's heat
    int32_t heats = 0;                      // [G+0x6C]+0x0C
    bool saving = false;                    // [G+0x64]'s +0x24: the save system saves the driver
    bool shiftedUp = false;                 // the first player's +0x30: a gear up by hand this frame
    const RaceSession* session = nullptr;   // [G+0x80]+0x30: the career's race (its round's +0x17)
    const save::Settings* settings = nullptr;  // [G+0x64]+0x54
    float lapRecord = 300.0f;               // [G+0xA8]: the route's record +0x2C
    HighScores* highScores = nullptr;       // [G+0xA8]
    int routeIndex = 0;                     // 0x00426490: the route in the routes' table
    const ExeTables* tables = nullptr;
    std::wstring driverName;                // [G+0x64]+0x20
    bool raceCarsChosen = false;            // [[G+0x80]+4]+0x614 (the career's)
    const Career* career = nullptr;         // [G+0x80]: the crews' and their members' names
    const PartCatalogue* catalogue = nullptr;  // the parts' costs (the garage's worth in the race's respect)
    float* directorTime = nullptr;          // the first player's camera director's time ([G+0x34]+0x0C [0] +0x10 +0x0C)
    bool networked = false;                 // [0x0075EF8C]
    bool host = false;                      // the transport's +0x0C: this machine hosts the network game
    bool spectator = false;                 // [0x0075EF9C]: the network game only watched
    net::NetSession* net = nullptr;         // the network game's session: its players and their order
    std::string route;                      // [G+0x6C]+0x30: the route's name (its map, UI_<route>_MAP)
    std::vector<const RaceCar*> cars;       // the race manager's vehicles, in its order
    std::vector<HudCar> players;            // [G+0x6C]+0x58 of them
    int32_t pausing = -1;                   // the number of the player who paused (the race track's +0x10), -1 none
    const RaceCar* cameraCar = nullptr;     // the first player's camera's car (P+0x0C, +0x08): the map's
    // The first player's camera (P+0x0C): its view (+0x18, a RaceCamera::Mode), looking back (+0xA8) and its pan
    // (+0xCC), by which the HUD hides the windows (0x0044D827).
    int cameraMode = 0;
    bool cameraLookBack = false;
    float cameraPan = 0.0f;
    // The online statistics' lap time of the player with the best lap, sent (the game's, 0x004EC6D0): the pause's
    // (0x0044F199: not a replay's race, a race of type 5, 3, 6 or 9) and the sprint's results' (0x00454EDF, `pause`
    // 0: neither checked); nothing without the statistics.
    std::function<void(size_t player, bool pause)> lapTimeStatistics;
    // The race track's first player's race complete ([G+0x34]+0x0C [0] +0x1C), taken (made 0).
    std::function<bool()> takeFirstComplete;
};

// What a window's frame asks of the race front end: a window (asked with mode 2, kept while one waits), the race to
// go on (0x0044ED90), a yes / no question whose answer goes to a flag of the window.
struct RaceWindowFrame {
    std::string window;
    bool goOn = false;
    // With goOn, the player whose controller takes DRIVER1 again (0x004080D0); -1 every player's.
    int player = -1;
    bool highScores = false;  // newhighscore_cms asked over the window (mode 5)
    std::wstring question;
    int32_t* answer = nullptr;
    bool startNo = true;  // the question's menu starts on "no" (the message's +0x28; every race window's)
    bool useUi = false;   // 0x00407FB0: the first player's controller let go (the keyboard's map UI again)
    bool stopRecord = false;  // 0x00426F60: the replay's record ended
    bool replayOff = false;   // the replay's +0x828 0 (and its method +0x2C, empty)
    // The network game: 1 everything reset, the session left (0x004EA4E0 with 1; then with the matchmaking its hosting
    // stopped, 0x004E2740); 2 every machine back to the lobby (0x004EA850 with the stage 0).
    int netEnd = 0;
    // [0x0072182C]: the online window online_ingame_bg opens.
    std::string onlineWindow;
};

// What hud_replay asks of the race front end besides its windows.
struct ReplayWindowFrame : RaceWindowFrame {
    bool pause = false;       // 0x004263E0 with 1: the race paused
    bool unpause = false;     // 0x004263E0 with 0
    bool nextCar = false;     // the cameras to the next car of the cars' manager (0x00497530, 0x00408AC0)
    bool changeView = false;  // each player's camera's next view (0x004069F0)
    bool release = false;     // 0x0058FEA0: the keyboard's first player let go (the attract's end)
};

class Replay;

}  // namespace oj
