#pragma once
// The showoff (a cruise, race type 4; docs/spec/23-showoff.md): the cruise manager (the career's +0x60, 0x460 bytes,
// made by 0x004AA5F0) sees the player's car do its tricks, counts their points into a combo, banks the combo and pays
// the bank into the score while the time runs.

#include "Core/Base/Matrix.h"
#include "Core/Race/RaceKinds.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace oj {

struct RaceCar;
class Replay;
struct PartCatalogue;
struct SceneMoveZone;
class TextTable;

// A crowd zone of the scene (0x148 bytes, 0x004B0F40: a Move* helper; the cruise manager's +0x00 list): the crowd
// there cheers by its state. The constructor leaves +0x0C and +0x144 as the heap gives them; here they start at 0.
struct CruiseZone {
    // +0x08: the helper's node; its slot 0 tells whether a point (centimetres) is inside (0x00553E50 a box: the
    // point in the node's frame by the inverse of its world matrix, 0x004F36C0, within the box's corners;
    // 0x00553F10 a sphere: the point's distance from the centre moved by the world matrix not over the radius).
    Mat34 matrix = identity34();
    uint32_t shape = 0;
    Vec4 boxMin{};
    Vec4 boxMax{};
    Vec4 centre{};
    float radius = 0.0f;
    int32_t active = 0;       // +0x0C
    int32_t kind = 0;         // +0x10: CRUISE_MOVE, the trick's crowd kind (0x00718870) that pleases it
    int32_t difficulty = 5;   // +0x14: CRUISE_DIFFICULTY, at least 5
    std::string crowdName;    // +0x18: CRUISE_NAME
    int32_t pleased = 0;      // +0x118: CRUISE_CROWD
    int32_t pleasedOnce = 0;  // +0x11C
    int32_t state = 0;        // +0x128: 0 calm .. 6 (a crash)
    float stateTime = 0.0f;   // +0x12C: how long the state holds
    float time = 0.0f;        // +0x130: in the state
    int32_t done = 0;         // +0x144: its bonus given
    std::wstring name;        // +0x134 .. +0x13C: by CRUISE_MESSAGE

    bool contains(const Vec4& point) const;
};

// The cruise's tricks (their slots in tricks; the game's lines kTextTricks + trick name them). A crowd's kind is the
// trick of its group: High Speed, Right Donut, Bootleg Turn, Right 360, Right 180, Boomerang, Right J-Turn, Drift.
enum Trick : int32_t {
    kHighSpeed = 1,
    kHighSpeedReverse = 2,
    kLeftDonut = 3,
    kRightDonut = 4,
    kLeftDonutReverse = 5,
    kRightDonutReverse = 6,
    kBootlegTurn = 7,
    kRight360 = 8,
    kLeft360 = 9,
    kRight180 = 10,
    kLeft180 = 11,
    kBoomerang = 12,
    kRightJTurn = 13,
    kLeftJTurn = 14,
    kDrift = 15,
    kNitrousDrift = 16,
    kWipeout = 17,
};

// A turn trick's state (its slot's +0x10).
enum class TurnState : int32_t { Idle = 0, UnderWay = 1 };

// A line of the trick list (0x38 bytes from +0x50; eight, the newest last).
struct CruiseMessage {
    int32_t bonus = 0;    // +0x00: a bonus or a crash line (its texts written once)
    int32_t trick = 0;    // +0x04: a Trick
    int32_t move = 0;     // +0x08: a special move, 1 .. 9
    std::wstring name;    // +0x0C
    std::wstring points;  // +0x1C
    float value = 0.0f;   // +0x2C: its points
    int32_t active = 0;   // +0x30: the trick goes on
    float time = 0.0f;    // +0x34: how long it shows
};

// A trick's slot (0x18 bytes from +0x210, by the trick's number).
struct CruiseTrick {
    int32_t index = 0;                 // +0x00
    float time = 0.0f;                 // +0x04: of the trick going on
    float time2 = 0.0f;                // +0x08
    float hold = 0.0f;                 // +0x0C: how long its condition holds (a turn's own time)
    TurnState state = TurnState::Idle;  // +0x10: a turn's state
    CruiseMessage* message = nullptr;  // +0x14: the line it writes (the list's last place)
    // The fixed build's: the trick's line once it went past the top of the list (Cruise::shift), its points kept.
    CruiseMessage offList;
};

// What the cruise's frame reads of the race (0x004AAAB0 from the player's frame 0x0041E200).
struct CruiseRace {
    RaceCar* car = nullptr;        // the first player's car (its +0x04), else the race's first vehicle
    const void* player = nullptr;  // the first player (+0x3C4)
    RaceMode mode = RaceMode::Arcade;    // [G+0x6C]+0x00
    RaceType type = RaceType::TimedLap;  // [G+0x6C]+0x04
    bool networked = false;        // [0x0075EF8C]
    bool paused = false;           // [G+0x34]+0x20
    float frameTime = 0.0f;        // [G+4]+0x1C
    float steering = 0.0f;         // the first player's steering axis (its controller's +0xD4, 0x00401640)
    std::vector<RaceCar*> cars;    // the cars' manager's list ([G+0x34]+0x10, +0x08)
    int32_t target = 0;            // the arcade race's target (0x0042B240), the career event's (+0x2E0 or 0x004AA5C0)
    int32_t carValue = 0;          // 0x0048CFE0 of the car's record
    float carDamage = 0.0f;        // 0x00557E80 of the record's grid (0 without one)
    const TextTable* texts = nullptr;
    int language = 7;
    Replay* replay = nullptr;      // [G+0x9C]: its marks of the showoff (+0xCB4) and their interest
};

// The network's side of a cruise ([0x0075EF8C]; none off it), the game's: our score to the others (0x19, not
// reliable, our player's +0xE8 its value), a trick done (0x1A), the cruise's end (0x004AB460) and the players' board
// (0x004B0A50).
struct CruiseNet {
    std::function<void(int32_t score)> score;
    std::function<void(int32_t trick)> trick;
    std::function<void()> end;
    std::function<void()> board;
};

class Cruise {
public:
    // 0x004AA5F0 / 0x004AA730: the cruise of `minutes` (3 in the career) set up for a race.
    void setup(int32_t minutes, RaceMode mode, RaceType type);
    // 0x004AAAB0: the cruise's frame; the messages for the first player go to `messages` (text, seconds).
    void frame(const CruiseRace& race, std::vector<std::pair<std::wstring, float>>& messages);
    // 0x004AB7C0: a hit of `impulse` on `car` (its contact's change over 750 while the cruise runs, the car's race
    // time over 1 s); `car` null for 0x004B0280's (1000, then each zone told).
    void crash(float impulse, const RaceCar* car, float frameTime);
    // 0x004AB640 (from the HUD's list once the time is out): the car's competitor finished (+0x118, +0xA8 10) with its
    // damage kept as it is (0x00556FB0).
    void timeUp();
    // 0x00420390: the score.
    int32_t score() const;
    // 0x004B01D0 with 0x004B0F40, as the race's track is made: a zone of a Move* helper; its name by CRUISE_MESSAGE
    // from the game's texts. 0x004B0110 at the race's end lets them go.
    void addZone(const SceneMoveZone& helper, const TextTable& texts);
    // 0x004B1510: a zone ready for the cruise (its crowd calm).
    static void resetZone(CruiseZone& zone);

    // The fields by their offsets in the stock object.
    std::vector<CruiseZone> zones;      // +0x00: the crowd zones
    int32_t started = 0;                // +0x10
    float pause = 0.0f;                 // +0x14: the crash's quiet time (0.25 while the race is paused)
    float time = 0.0f;                  // +0x18: left
    float warning = 10.0f;              // +0x1C: the next second to warn at
    float dt = 0.0f;                    // +0x20: the frame's time
    float bank = 0.0f;                  // +0x24: the combo's points paid into the score bit by bit
    float combo = 0.0f;                 // +0x28
    float comboShown = 0.0f;            // +0x2C
    float total = 0.0f;                 // +0x30: the score
    float scoreShown = 0.0f;            // +0x34
    float soundTime = -1.0f;            // +0x38
    float comboTime = 0.0f;             // +0x3C: the combo goes to the bank when it runs out
    float carFactor = 1.0f;             // +0x40: the car's value x 0.00005 within 1 .. 3, less its damage
    float donutTime = 0.0f;             // +0x44: of donuts; past 30 s they cost points
    float speedTime = 0.0f;             // +0x48: of high speed; past 30 s it costs points
    float flash = 0.0f;                 // +0x4C: the multiplier shown blinking
    std::array<CruiseMessage, 8> messages;  // +0x50
    std::array<CruiseTrick, 18> tricks;     // +0x210
    RaceCar* car = nullptr;             // +0x3C0
    const void* player = nullptr;       // +0x3C4
    float ground = 0.0f;                // +0x3C8: the time left of the wheels' last touch (0.5 when they touch)
    std::array<int32_t, 4> history{};   // +0x3CC: the last tricks, the newest last
    std::array<float, 4> ages{};        // +0x3DC
    int32_t multiplier = 1;             // +0x3EC: 1 .. 25
    int32_t multiplierShown = 1;        // +0x3F0
    float idle = 0.0f;                  // +0x3F4: since the last trick
    float decay = 0.0f;                 // +0x3F8
    float volumeTarget = 1.0f;          // +0x3FC
    float volume = 1.0f;                // +0x400
    std::array<int32_t, 10> moves{};    // +0x404: the special moves done (by the move)
    int32_t changed = 0;                // +0x42C: the list changed
    int32_t crashed = 0;                // +0x430
    CruiseZone* zone = nullptr;         // +0x434: the zone the car was in the frame before
    CruiseZone* lastZone = nullptr;     // +0x438
    float comboBlink = 0.25f;           // [0x007255C8]: the HUD's multiplier blink
    CruiseNet net;                      // the network game's (set by the game for a network race)

private:
    // A turn's state kept between frames (the stock game's statics, 0x007A2DAC .. 0x007A2E88 and 0x007255B8 ..).
    struct Turn {
        Vec4 start{};
        bool handbrake = false;
        bool reversed = false;
        float most = 0.0f;
        float least = 0.0f;
    };

    CruiseMessage& newTrick(int32_t trick);  // 0x004ABD10
    CruiseMessage& newMove(int32_t move);    // 0x004ABDC0
    void shift();
    void trickDone(int32_t trick, int32_t points);  // 0x004AC240
    void specialMoves();                     // 0x004AC0A0
    void crowdBonus(int32_t trick);          // 0x004AC560
    void listTexts();                        // 0x004ABA30
    void decayMultiplier();                  // 0x004AB740
    void excite(int32_t level, float seconds);  // the zone the car is in (0x004B0440) cheering
    CruiseZone* carZone();                   // 0x004B0440
    void crowd(CruiseZone& z);               // 0x004B04A0

    void highSpeed();                        // 0x004AC780
    void donut(int32_t trick);               // 0x004AC9F0, 0x004ACCA0, 0x004ACF50, 0x004AD200
    void bootleg();                          // 0x004AD4B0
    void spin(int32_t trick);                // 0x004AD960, 0x004ADE70 (360s), 0x004AECE0 (boomerang)
    void half(int32_t trick);                // 0x004AE380, 0x004AE830 (180s)
    void jTurn(int32_t trick);               // 0x004AF220, 0x004AF690
    void drift(bool nitro);                  // 0x004AFB00, 0x004AFE30
    void turnDone(int32_t trick);

    const CruiseRace* m_race = nullptr;
    const TextTable* m_texts = nullptr;     // the race's texts (the crash's line)
    Replay* m_replay = nullptr;             // the replay the marks go to
    // 0x00429F50 on the race manager's +0x1C (the replay's interest of the frames, its lines' texts not kept) and the
    // replay's mark of the frame (+0xCB4 the trick or the crash, +0xCB5 the special move).
    void mark(size_t slot, int32_t value, float interest);
    RaceType m_type = RaceType::TimedLap;
    float m_speedHold = 0.0f;               // [0x0079E814]
    bool m_drift = false;                   // [0x0079E810]
    bool m_nitroDrift = false;              // [0x0079E80C]
    bool m_maxComboShown = true;            // [0x007255D0]
    float m_netTimer = 0.0f;                // [0x0079E818]: since our score was last sent
    std::array<Turn, 18> m_turns{};
};

}  // namespace oj
