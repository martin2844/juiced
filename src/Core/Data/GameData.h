#pragma once
// The game's data, loaded at start in the order of the stock game object (0x0040BAC0): the executable's tables, the
// car models (cardefs.txt draws rand()), the parts catalogue, the arcade cars, the career (a new career is made at
// start already, and again for each new driver over the same objects), the start driver's id.

#include "Core/Race/Arcade.h"
#include "Core/Career/Career.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/ExeTables.h"
#include "Core/Base/GameFiles.h"
#include "Core/Saves/HighScores.h"
#include "Core/Base/Random.h"
#include "Core/Race/RaceSession.h"
#include "Core/Saves/SaveEntries.h"
#include "Core/Saves/SaveDevice.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace oj {

// Where careerload (and garageload) goes (the career manager's +0xA4; CareerLoad's window table).
enum class CareerScreen : int32_t {
    CarSel = 0,             // carsel (carbuy with the dealer's offers)
    GridWalk = 1,           // CMSGridWalk
    Character = 2,          // character
    CharacterPostRace = 3,  // characterPostRace
    CutScene = 4,           // cmsCutScene: the test track
    RollingRoad = 5,        // car_rolling: the dyno
    CarSelBack = 6,         // carsel, back from the test track or the dyno
    CarSelRepaired = 7,     // carsel after a repair
    OnlineGridWalk = 8,     // CMSOnlineGridWalk
};

class GameData {
public:
    explicit GameData(std::filesystem::path gameFolder);

    bool load(CrtRandom& random, std::string* error);
    bool loaded() const { return m_loaded; }
    const GameFiles& files() const { return m_files; }

    // A new career over the career objects (0x004A2300) for a driver; driverName as the crew keeps it (8-bit).
    void newCareer(CrtRandom& random, const std::string& driverName);

    // 0x004B8570: the race of the career's current event (Core/Race/RaceSession.h).
    void setupRace(CrtRandom& random);

    // The way into the career from DemoFlyby (0x00435434, Core/Race/RaceSession.h): race mode 2, a new career for the
    // driver, the first event's race set up; careerload then opens the betting window.
    void enterCareer(CrtRandom& random, const std::string& driverName);

    // CMSNewCont's new game (0x004C3830): a new career for the driver and the first event's race set up
    // (Core/Race/RaceSession.h enterCareer), the race's own fields left as they are.
    void newGameCareer(CrtRandom& random, const std::string& driverName);

    // The dealer's cars (0x004B5330) and a car taken to the garage (0x004B5070) with the game's making of cars
    // (Core/Career/CareerInit.h).
    void makeDealerCars(CrtRandom& random);
    // A cheat (0x004A6E50, Core/Career/CareerInit.h; 0 opens the arcade, 0x0042B6B0).
    void applyCheat(CrtRandom& random, Cheat cheat);

    // 0x0042AD40: the arcade's race cars (+0x5108) made again: for each race of each championship the player's car
    // (the race's first entry when none is the player's) of its model (0x0048CC30), modified automatically by the
    // entry's level (0 none, 4 as a prototype of any level, else of the level less 1, 0x0048F2C0), rated, its grid
    // made, painted by the race (0x0042B720) and its bonnet (+0x06) ((race + 1) x (championship + 1)) % the
    // CarDef's bonnets when it has any.
    void makeArcadeCars(CrtRandom& random);
    // 0x0042B000: the cars of the race being raced (+0x5108 made again): each entry's car of its model, modified
    // automatically by its level (4 as a prototype of any level, else of that level), the player's painted by the race
    // and its bonnet as above (championship and race), another a paint of the CarDefs' list ((championship + entry) x 5
    // + race) % the paints; rated, its grid made.
    void makeArcadeRaceCars(CrtRandom& random);
    int32_t dealerTake(CrtRandom& random, int32_t place);

    // 0x004A3CC0: the race of the current event into `race` (Core/Race/RaceSession.h).
    void startRace(CrtRandom& random, int32_t automaticGearbox);

    // 0x004A3A40: the career session's end: 0x004BA8A0 with the route's record, then 0x004B9E20 (Core/Race/RaceSession.h).
    // random: the generator of the thread that runs it.
    void endSession(CrtRandom& random);

    // 0x004A6530: the career's next day (Core/Career/CareerEvents.h).
    void nextDay(CrtRandom& random);

    // The phone's events (Core/Career/CareerEvents.h): 0x004A2DC0 / 0x004A2F80 and 0x004A3130.
    void phoneEvent(CrtRandom& random, int crew, int month, int day, bool host);
    void challengeEvent(CrtRandom& random, int crew, int month, int day, int challenge);

    // 0x004A2670: the calendar filled up to six open events (Core/Career/CareerEvents.h).
    void topUpCalendar(CrtRandom& random);

    // The music playlist (0x00408D20): the songs of the music bank the settings allow (a bit per song at +0x208),
    // in a random order (random_shuffle, 0x00409460: song i swaps with song rand() % (i + 1)). The stock game makes it
    // at start, when "start" opens and when a driver is loaded.
    void shuffleMusic(const save::Settings& driverSettings, CrtRandom& random);

    ExeTables tables;
    CarDefs defs;
    PartCatalogue catalogue;
    std::unique_ptr<DynamicsStore> dynamics;
    std::unique_ptr<Career> career;
    std::vector<Car> arcadeCars;  // arcadecollection.txt (the save system's list)
    std::string arcadeRaces;      // ArcadeRaces.txt, for the arcade progress of new drivers
    ArcadeCarGroups arcadeGroups;  // [game + 0x84]: the custom race's cars by group (ArcadeUnlock.txt)
    // [game + 0x8C]: the arcade object: ArcadeRaces.txt at start, the progress of the driver made or loaded.
    Arcade arcade;
    std::vector<Car> arcadeRaceCars;  // [game + 0x8C] + 0x5108: the cars of the arcade's races
    int32_t arcadeCar = 0;        // [game + 0x64] + 0x50: the arcade car of the custom race (its place in the list)
    // [game + 0x68]: the second player's save system object (a split screen race; 0x0040A510 with its start driver
    // "Player2", line 168, 0x0040AB80): its arcade cars (+0x40, arcadecollection.txt) and its car among them (+0x50);
    // [game + 0x88]: its custom race's cars by group.
    std::vector<Car> arcadeCars2;
    int32_t arcadeCar2 = 0;
    ArcadeCarGroups arcadeGroups2;
    std::wstring profileName2;  // [game + 0x68] + 0x20: "Player2" (the game's text line 168, [0x00760410])
    uint32_t audioFlags = 0;      // [0x0071E718]: the configuration's audioflags (8: no sound)
    int autoModifyCounter = 1;    // 0x00717868
    int currentCrewmate = 0;      // 0x0075EF60
    int32_t careerTip = -1;       // 0x00715C40: the career tip of the loading window (-1 none yet)
    int32_t generalTip = -1;      // 0x00715C44: the general tip
    RaceSetup race;               // [game + 0x6C]: the race to start (race.mode: 0 arcade, 1 custom, 2 career)
    std::string replayName;       // the replay's +0xCCC (0x004296F0): the files a replay's race (mode 4) reads
    // [0x0075ED18]: the front end's first window after the race when set (0x00461A10 empties it): "menu" after a
    // saved replay is read (0x00429550); the network's quits set it in the stock game (0x00457180), not here.
    std::string afterRaceWindow;
    int32_t attractNext = 0;      // [0x0075ECBC]: the attract replay played next
    int32_t attractsStarted = 0;  // [0x0075ED4C]: attract replays started (read by nothing)
    HighScores highScores;        // [game + 0xA8]: the routes' records
    save::UiHelp help;            // the help pages shown (the table at 0x00715D48; UIHelp.sav)
    std::wstring profileName;     // [game + 0x64] + 0x20: the driver made or loaded
    int32_t saving = 0;           // [game + 0x64] + 0x08 (its method +0x24): set once a driver is made or loaded
    int32_t driverChanged = 0;    // [game + 0x64] + 0x04 (its methods +0x0C, +0x10): a write over the driver's folder asks
    save::SaveDevice saveDevice;  // [game + 0x60] and the records' [game + 0xA4], their manager [0x00728D98]
    save::SaveRequest saveRequest;  // 0x00767FE8: the windows' requests to the save device and its flags
    int32_t recordsAsked = 0;     // [0x0075EE68]: the main menu asked for loadhighscores
    // [game + 0x64] + 0x54: the settings of the driver made or loaded (none before one is).
    std::optional<save::Settings> settings;
    CareerScreen careerScreen = CareerScreen::CarSel;  // the career manager's +0xA4: where careerload goes
    int32_t careerTestTrack = 0;  // the career manager's +0xA8: set for the garage's test track
    size_t songCount = 0;         // the [BeginSS] songs of audio\music\music.cfg
    std::vector<std::wstring> songNames;  // their displayname (the bank's song name, opts_songlist)
    std::vector<uint32_t> playlist;

private:
    GameFiles m_files;
    bool m_loaded = false;
};

// A driver's name as the crew keeps it (0x0040AC50): the low byte of each character up to a low byte of 0.
std::string crewName8(const std::wstring& name);

}  // namespace oj
