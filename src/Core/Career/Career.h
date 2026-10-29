#pragma once
// The career (docs/spec/08-career.md) and CMSGlobal.sav (docs/spec/03-saves.md). The objects keep the layout of the
// stock game's, so that the fields the saves keep are copied as they are and a field nothing sets keeps its value, as
// in the stock game; fields whose meaning is not traced are named by their offset.

#include "Core/Cars/Car.h"
#include "Core/Career/EventKinds.h"
#include "Core/Race/Cruise.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace oj {

#pragma pack(push, 1)

// The profile (career +0x04, 0x638 bytes).
struct CareerProfile {
    uint32_t introduction = 0;              // +0x000: the first race and its car, until the dealership (0x004A32B0)
    uint32_t introBillPaid = 0;             // +0x004: the first race's repair bill paid
    uint32_t completeShown = 0;             // +0x008: the career complete message shown
    int32_t money = 0;                      // +0x00C
    int32_t backedPrizes = 0;               // +0x010: the prizes of the crews the player's odds were on (0x004DAD7D)
    int32_t crewPrizes = 0;                 // +0x014: the prizes the player's crew took (0x004DAD3C)
    // Counters of 16 bits: the races won by the round's type (+0x018) and by the event's location (+0x026), the crews
    // the player's crew finished ahead of (+0x038), the races placed but not won (+0x04C).
    std::array<uint8_t, 14> winsByType{};
    std::array<uint8_t, 18> winsByLocation{};
    std::array<uint8_t, 18> crewsBeaten{};
    int16_t tenthCrewBeaten = 0;            // +0x04A: after the crews' words (a crew 9 would count here)
    int16_t racesNotWon = 0;
    int16_t respectLevel = 0;               // +0x04E: compared with the dealer's table x 3
    std::array<std::array<int8_t, 10>, 53> partsA{};  // +0x050: per car model
    std::array<std::array<int8_t, 10>, 53> partsB{};  // +0x262
    std::array<int8_t, 53> carLevel{};      // +0x474
    std::array<uint8_t, 53> unlockStep{};   // +0x4A9
    std::array<uint8_t, 2> pad4DE{};
    uint32_t raceCarKept = 0;               // +0x4E0: 0 after a pink slip race lost: the results go to CMSMainMenu once
    uint32_t moneySaved = 0;                // +0x4E4: the money when the garage or the dealership was entered; leaving
                                            // with other money autosaves (-1 before the test track forces it)
    uint32_t moneySpent = 0;                // +0x4E8: on cars, parts, repairs and looks (SpentValue)
    uint32_t prizesWon = 0;                 // +0x4EC: the races' and bets' winnings (WinningValue)
    uint32_t garageWorth = 0;               // +0x4F0: the garage's sale worth when CMSCarCollection shows it
    // +0x4F4: the greatest garage worth CMSCarCollection showed; the respect by the garage's value writes its own sum
    // of the cars' capped values over it.
    uint32_t garageWorthBest = 0;
    // +0x4F8: what the advice 4 measures the best car's worth (+0x4FC) from, as the advice 5 the greatest garage worth
    // from +0x4F0; only cleared, so the advice counts from 0.
    uint32_t bestCarWorthTold = 0;
    uint32_t bestCarWorth = 0;              // +0x4FC: the best car's value (the respect by the best car)
    std::array<uint32_t, 4> memberPointers{};  // +0x500: the crewmates of the player's crew (saved as addresses)
    int32_t memberCount = 0;                // +0x510
    // +0x514: the garage cars the meeting's race may take, the best rated first (0x004C27A0), or the test track's car
    // (0x004B9670); their count +0x5F0.
    std::array<int32_t, 55> raceChoices{};
    int32_t raceChoiceCount = 0;            // +0x5F0
    std::array<int32_t, 3> raceMembers{};   // +0x5F4: the member slot of each of the player's racers
    std::array<int32_t, 3> raceCars{};      // +0x600: the garage car of each
    int32_t raceCarCount = 0;               // +0x60C
    int32_t meetingEventAddress = 0;        // +0x610: the event gone to (an address; Career::meetingEvent here)
    int32_t raceCarsChosen = 0;             // +0x614: the player's cars of the race are chosen
    int32_t racing = 0;                     // +0x618: the meeting's race is entered
    int32_t watching = 0;                   // +0x61C: the meeting's race is watched
    int32_t lastHostedType = 0;             // +0x620: the EventKind of the race the player hosted last (7 none)
    int32_t lastHostedLocation = 0;         // +0x624
    float rivalsSkill = 0.0f;               // +0x628: added to the opponents' skill after the introduction
    float betShare = 0.0f;                  // +0x62C: the money bet on a race as a percentage of the money (0x004BC2E0)
    int32_t outOfRace = 0;                  // +0x630: the race quit, or the player's crew not among the first five
    int32_t showoffTutorialShown = 0;       // +0x634
    static constexpr size_t kModelCapacity = 64;
    struct ModProgress {
        std::array<int8_t, 10> performance{};
        std::array<int8_t, 10> body{};
        int8_t paint = -1;
        uint8_t step = 0;
    };
    std::array<ModProgress, kModelCapacity - 53> modProgress{};
    uint32_t modCount = 0;
    auto& performance(size_t id) { return id < 53 ? partsA.at(id) : modProgress.at(id - 53).performance; }
    const auto& performance(size_t id) const { return id < 53 ? partsA.at(id) : modProgress.at(id - 53).performance; }
    auto& bodyParts(size_t id) { return id < 53 ? partsB.at(id) : modProgress.at(id - 53).body; }
    const auto& bodyParts(size_t id) const { return id < 53 ? partsB.at(id) : modProgress.at(id - 53).body; }
    auto& paintLevel(size_t id) { return id < 53 ? carLevel.at(id) : modProgress.at(id - 53).paint; }
    const auto& paintLevel(size_t id) const { return id < 53 ? carLevel.at(id) : modProgress.at(id - 53).paint; }
    auto& unlockIndex(size_t id) { return id < 53 ? unlockStep.at(id) : modProgress.at(id - 53).step; }
};
static_assert(offsetof(CareerProfile, modProgress) == 0x638);
static_assert(sizeof(CareerProfile::ModProgress) == 22);

// One member of a crew as CMSCrews.txt describes it (0x18 bytes).
struct CrewMemberDef {
    float ability = 0.0f;                   // DriverName's first number
    float unused = 0.0f;                    // +0x04: only cleared (0x004A7240)
    float concentration = 0.0f;             // its second number
    std::array<float, 3> unusedTail{};      // +0x0C: only cleared (0x004A7240)
};

// A crew challenge (0x1C bytes, CMSCrewChallenges.txt).
struct CrewChallenge {
    ChallengeType type = ChallengeType::LapTime;
    int32_t value = 0;
    int32_t car = 0;                        // a car model id
    int32_t prize = 0;                      // +0x0C: Winnings
    int32_t routeSlot = 0;                  // +0x10: of the crew's circuit routes
    int32_t done = 0;                       // +0x14
    int32_t result = 0;                     // +0x18: float, the player's last best lap (-1: another result)
};

// A crew (career +0x08 + 4 n, 0x348 bytes); crew 0 is the player's.
struct Crew {
    int32_t number = 0;                     // +0x000
    std::array<float, 10> respectFactor{};  // +0x004 (RespectFactor x 0.01)
    std::array<float, 10> respectGain{};    // +0x02C
    std::array<int32_t, 4> stage{};         // +0x054: the respect steps reached
    // The marks the respect by the garage's value (+0x064), its best car (+0x068) and the money (+0x06C, 0x004BBB00)
    // is measured from.
    int32_t collectionMark = 0;             // float
    int32_t bestCarMark = 0;                // float
    float moneyMark = 0.0f;
    int32_t respect = 0;                    // +0x070
    int32_t respectNow = 0;                 // +0x074
    int16_t id = 0;                         // +0x078
    std::array<std::array<char, 25>, 4> memberNames{};       // +0x07A
    std::array<std::array<char, 25>, 4> memberShortNames{};  // +0x0DE
    std::array<uint8_t, 2> pad142{};
    std::array<CrewMemberDef, 4> members{};  // +0x144
    uint32_t baseColour = 0;                // +0x1A4: 0x80RRGGBB
    uint32_t metallicColour = 0;            // +0x1A8
    uint32_t pearlColour = 0;               // +0x1AC
    float metallicAmount = 0.0f;            // +0x1B0
    float pearlAmount = 0.0f;               // +0x1B4
    int32_t wheelColour = 0;                // +0x1B8
    int32_t wheels = 0;                     // +0x1BC
    int32_t neonColour = 0;                 // +0x1C0
    std::array<int32_t, 8> circuitRoute{};  // +0x1C4: an index of the routes table
    std::array<float, 8> circuitKm{};       // +0x1E4
    std::array<int32_t, 8> circuitDifficulty{};  // +0x204
    int32_t racesWonHosted = 0;             // +0x224: the player's wins at events the crew holds
    int32_t circuitCount = 0;               // +0x228
    std::array<int32_t, 2> p2pRoute{};      // +0x22C
    std::array<float, 2> p2pKm{};           // +0x234
    int32_t p2pCount = 0;                   // +0x23C
    int32_t cruiseRoute = 0;                // +0x240
    float cruiseKm = 0.0f;                  // +0x244
    int32_t cruiseCount = 0;                // +0x248
    int32_t sprintRoute = 0;                // +0x24C
    float sprintKm = 0.0f;                  // +0x250
    int32_t sprintCount = 0;                // +0x254
    int32_t illegalRoute = 0;               // +0x258
    float illegalKm = 0.0f;                 // +0x25C
    int32_t illegalCount = 0;               // +0x260
    std::array<int32_t, 9> willAttend{};    // +0x264: per crew
    int32_t entranceFee = 0;                // +0x288
    std::array<int32_t, 3> prizeMoney{};    // +0x28C
    float probMix = 0.0f;                   // +0x298 (x 0.01)
    float probCircuit = 0.0f;               // +0x29C
    float probSprint = 0.0f;                // +0x2A0
    float probP2P = 0.0f;                   // +0x2A4
    float probCruising = 0.0f;              // +0x2A8
    float probTerritory = 0.0f;             // +0x2AC
    float probMake = 0.0f;                  // +0x2B0
    float probModel = 0.0f;                 // +0x2B4
    int32_t preferredRegion = 0;            // +0x2B8 (CarPref)
    int32_t preferredMake = 0;              // +0x2BC
    int32_t hostAccepted = 0;               // +0x2C0: the player agreed to host the crew's challenge
    int32_t hostWon = 0;                    // +0x2C4: the player won it
    int32_t hostLost = 0;                   // +0x2C8: the player lost it
    int32_t racesSinceCall = 0;             // +0x2CC: the player's races since the crew's last call
    int32_t pinkSlipsWon = 0;               // +0x2D0: the crew's cars the player won
    int32_t unused = 0;                     // +0x2D4: only cleared
    int32_t meterBlinks = 0;                // +0x2D8: the respect meter blinks (only cleared in stock)
    std::array<CrewChallenge, 3> challenges{};  // +0x2DC
    float rams = 0.0f;                      // +0x330: the player's crew's hits on this crew's cars ahead (0x004BAD40)
    float money = 0.0f;                     // +0x334
    int32_t lastBetWon = 0;                 // +0x338: the crew won the last bet race against the player (-1: none)
    std::array<int32_t, 3> raceCars{};      // +0x33C: the opponents' cars the crew chose for the race (-1 none)
};
static_assert(sizeof(Crew) == 0x348);

// A crewmate (career +0x44 + 4 n, 0xC4 bytes, CMSCrewmates.txt); crewmate 0 is the player.
struct Crewmate {
    int32_t number = 0;                     // +0x00
    int32_t memberSlot = 0;                 // +0x04
    std::array<char, 64> name{};            // +0x08
    std::array<char, 64> shortName{};       // +0x48
    int32_t joinThreshold = 0;              // +0x88
    int32_t joinCrew = 0;                   // +0x8C
    int32_t phoneNumber = 0;                // +0x90
    float drivingAbility = 0.0f;            // +0x94
    float drivingLimit = 0.0f;              // +0x98
    float recover = 0.0f;                   // +0x9C
    float recoverLimit = 0.0f;              // +0xA0
    float learnRate = 0.0f;                 // +0xA4
    float forgetRate = 0.0f;                // +0xA8
    int32_t joined = 0;                     // +0xAC
    int32_t races = 0;                      // +0xB0
    int32_t wins = 0;                       // +0xB4
    int32_t joinDay = 0;                    // +0xB8
    int32_t daysIdle = 0;                   // +0xBC: days in the crew since its last race
    int32_t daysWanting = 0;                // +0xC0: days of wanting to join (0x004AA4C0)
};
static_assert(sizeof(Crewmate) == 0xC4);

// A round of a calendar event (0x38 bytes).
struct EventRound {
    uint8_t used = 0;                       // +0x00
    uint8_t ended = 0;                      // +0x01: the results left, the event quit or lost (0x004A3A60)
    uint8_t prototypePrize = 0;             // +0x02: the event's prototype part goes to the winner
    uint8_t pad03 = 0;
    EventKind type = EventKind::Challenge;  // +0x04
    int32_t teams = 0;                      // +0x08: a team size above 1
    int8_t mostCrews = 0;                   // +0x0C
    std::array<uint8_t, 9> crews{};         // +0x0D: the crews racing
    int8_t opponents = 0;                   // +0x16: the number of crews
    int8_t teamSize = 0;                    // +0x17
    int8_t drivers = 0;                     // +0x18
    std::array<uint8_t, 3> pad19{};
    int32_t route = 0;                      // +0x1C
    int32_t routeIndex = 0;                 // +0x20
    int8_t laps = 0;                        // +0x24
    std::array<uint8_t, 3> pad25{};
    float km = 0.0f;                        // +0x28
    std::array<int32_t, 3> prize{};         // +0x2C
};
static_assert(sizeof(EventRound) == 0x38);

// A calendar event (0x84 bytes).
struct CalendarEvent {
    uint8_t used = 0;                       // +0x00
    uint8_t done = 0;                       // +0x01
    uint8_t hostedByPlayer = 0;             // +0x02: the phone's "Host challenge?" answered yes
    uint8_t pinkSlip = 0;                   // +0x03: a pink slip race
    uint8_t extreme = 0;                    // +0x04: asks for a car with prototype parts; prizes and fee x 3
    uint8_t challenge = 0;                  // +0x05: a crew's challenge
    int8_t challengeIndex = 0;              // +0x06: the challenge of the crew (-1: none)
    int8_t dayOfMonth = 0;                  // +0x07
    int32_t month = 0;                      // +0x08: counted from the career's start
    int8_t crew = 0;                        // +0x0C
    int8_t location = 0;                    // +0x0D: the crew whose turf (its first circuit route's location)
    std::array<uint8_t, 2> pad0E{};
    EventKind type = EventKind::Challenge;  // +0x10
    int32_t entranceFee = 0;                // +0x14
    int32_t prizeTotal = 0;                 // +0x18
    EventRound round{};                     // +0x1C
    int8_t roundCount = 0;                  // +0x54
    uint8_t roundIndex = 0;                 // +0x55: the round raced
    std::array<uint8_t, 9> attend{};        // +0x56: per crew
    uint8_t pad5F = 0;
    int32_t region = 0;                     // +0x60: the cars asked for (0: any)
    int32_t make = 0;                       // +0x64
    int32_t model = 0;                      // +0x68
    int32_t carClass = 0;                   // +0x6C
    int32_t asksPrototype = 0;              // +0x70: asks for a car with a prototype part (0x004A2C70)
    int32_t prototypeKind = 0;              // +0x74: the part's kind
    int32_t prototypeSupplier = 0;          // +0x78
    int32_t timeOfDay = 0;                  // +0x7C
    int32_t weather = 0;                    // +0x80
};
static_assert(sizeof(CalendarEvent) == 0x84);

// The calendar (career +0x2C, 0xF94 bytes).
struct Calendar {
    int32_t dayOfMonth = 0;                 // +0x000: today
    int32_t month = 0;                      // +0x004
    int32_t daysPlayed = 0;                 // +0x008
    // +0x00C: the month again, written with +0x004 (0x004A6580); read by nothing.
    int32_t monthCopy = 0;
    std::array<CalendarEvent, 30> events{};  // +0x010
    int32_t newCareer = 0;                  // +0xF88 (not saved)
    int32_t chosenDayOfMonth = 0;           // +0xF8C: the day chosen in the calendar window (hosting a race)
    int32_t chosenMonth = 0;                // +0xF90
};
static_assert(sizeof(Calendar) == 0xF94);

// Who a phone message, a call or a contact is (their +0x04 / +0x00).
enum class PhoneCaller : int32_t { CrewLeader = 0, Crewmate = 1, Dealer = 2 };

// A phone call's type (the message's +0x88): what it asks and what its answers do (0x004B6B20).
enum class CallType : int32_t {
    LetJoin = 0,        // a crewmate: "Let driver join crew?"
    JoinMyCrew = 1,     // a crewmate called: "Join my crew?"
    HostChallenge = 2,  // a crew's leader: "Host challenge?"
    PinkSlip = 5,       // a crew's leader: "Pink slip race?"
    LeaderOffer = 8,    // a crew's leader called: a challenge, or a race for pinks
    Challenges = 9,     // the crew's challenges to take
    TextMessage = 10,   // Mr. Jeff's text message
    Reply = 11,         // a reply, END CALL only
    Unused12 = 12,      // 12 .. 15 idle when answered; nothing makes them
    Unused13 = 13,
    Unused14 = 14,
    Unused15 = 15,
    RespectFirst = 16,   // the crew's respect its first step less 20 (nothing makes 16 .. 19)
    RespectSecond = 17,  // its second step's
    RespectLast = 18,    // its last step's, then the third's
    RespectThird = 19,   // its third step's
};

// The phone's state (its +0x000).
enum class PhoneState : int32_t { Idle = 0, Opened = 1, Calling = 2, Ringing = 3, Talking = 4 };

// A phone contact (0x48 bytes).
struct PhoneContact {
    PhoneCaller kind = PhoneCaller::CrewLeader;  // +0x00
    std::array<char, 64> name{};            // +0x04: "<short name> (<crew name>)"
    int32_t crew = 0;                       // +0x44
};
static_assert(sizeof(PhoneContact) == 0x48);

// A message of the phone waiting for its call, or the call (0xBC bytes, 0x004B63B0). The stock game keeps the text as
// a wide string (+0x94) and the answers as pointers to the game's text lines (+0xA4 ..); here they are the lines'
// numbers (kPhoneNone for none, kPhoneChallenge - k for the k-th challenge text the phone made).
struct PhoneMessage {
    int32_t used = 0;                       // +0x00
    PhoneCaller kind = PhoneCaller::CrewLeader;  // +0x04
    std::array<char, 0x80> name{};          // +0x08
    CallType type = CallType::LetJoin;      // +0x88
    int32_t sample = 0;                     // +0x8C: the speech sample (played below 0x21; -1 none)
    int32_t param = 0;                      // +0x90: the sample's crew
    int32_t text = -1;                      // +0x94: the text's line
    int32_t textAfterLeader = 0;            // the leader's name of the player's crew before it (Mr. Jeff's texts)
    std::array<int32_t, 2> textRest{};      // +0x9C, +0xA0: the second half of the stock's text string
    std::array<int32_t, 4> answers{};       // +0xA4
    int32_t crew = 0;                       // +0xB4: the crew, or the crewmate for a crewmate's
    int32_t played = 0;                     // +0xB8: the call's sample played (+0x838)
};
static_assert(sizeof(PhoneMessage) == 0xBC);

constexpr int32_t kPhoneNone = -1;
constexpr int32_t kPhoneChallenge = -2;

// The phone (career +0x5C, 0xFA8 bytes): ten message slots (+0x28), the call (+0x780) and the contacts.
struct Phone {
    PhoneState state = PhoneState::Idle;    // +0x000
    float timer = 0.0f;                     // +0x004: the time before the next message's call
    int32_t model = 0;                      // +0x008: the phone model chosen
    int32_t goToEvent = 0;                  // +0x00C: the call made an event to go to when the phone closes
    int32_t eventMonth = 0;                 // +0x010
    int32_t eventDay = 0;                   // +0x014
    int32_t respectPending = 0;             // +0x018: the call changed a crew's respect
    float respectChange = 0.0f;             // +0x01C
    float respectAfter = 0.0f;              // +0x020
    int32_t respectCrew = 0;                // +0x024
    std::array<PhoneMessage, 10> messages{};  // +0x028
    PhoneMessage call;                      // +0x780
    std::array<PhoneContact, 26> contacts{};  // +0x83C
    int32_t contactCount = 0;               // +0xF8C
    int32_t dialled = 0;                    // +0xF90: the contact called
    float dialTimer = 0.0f;                 // +0xF94: the ringing before the answer
    int32_t answersAtOnce = 0;              // +0xF98: the call is answered at once
    int32_t challengesCrew = 0;             // +0xF9C: the crew whose challenges the call shows (number + 1; a pointer)
    int32_t shownCrewmate = 0;              // +0xFA0: the crewmate the call shows (number + 1; a pointer)
    int32_t hostNoteShown = 0;              // +0xFA4: the host rights note shown
};
static_assert(sizeof(Phone) == 0xFA8);

// A car offered by the dealer (0x40 bytes); the paint has the dealer's defaults (0x004B4280).
struct DealerOffer {
    int32_t index = 0;
    int32_t model = 0;                      // +0x04: car model id
    int32_t used = 0;                       // +0x08
    int32_t condition = 0;                  // +0x0C: 0, 1 above 20000 miles, 2 a special offer
    int32_t modelClass = 0;                 // +0x10
    int32_t mileage = 0;                    // +0x14
    int32_t rating = 0;                     // +0x18
    int32_t price = 0;                      // +0x1C
    int32_t modified = 0;                   // +0x20: automatically modified (only cleared)
    int32_t damage = 0;                     // +0x24: the damage of a special offer (1 .. 3) or a damaged car
    int32_t prototypeParts = 0;             // +0x28: with prototype parts (heavily tuned)
    uint32_t base = 0xFFFFFFFF;             // +0x2C
    uint32_t metallic = 0xFF000000;
    uint32_t pearl = 0xFFFFFFFF;
    float metallicAmount = 0.5f;
    float pearlAmount = 0.1f;
};
static_assert(sizeof(DealerOffer) == 0x40);

// What the dealer's windows show: the player's garage (selling) or the offers (buying).
enum class DealerMode : int32_t { Garage = 0, Offers = 1 };

// The cheats (CMSCheats' codes, 0x00718F88): the arcade's (its own object), money, every crew's respect, every
// car and part, every crewmate, the character test mode, every race won, all of them.
enum class Cheat : int32_t {
    Arcade = 0,
    Money = 1,
    Respect = 2,
    CarsAndParts = 3,
    Crewmates = 4,
    CharacterTest = 5,
    WinAllRaces = 6,
    Everything = 7,
};
// The career's cheat flags (its +0x64): the character test mode, every race won.
enum CheatFlag : size_t { kCheatCharacterTest = 0, kCheatWinAllRaces = 1 };

// The cars an event asks for (a hosted meeting's criteria; an online career session's, attribute 9): none, a
// territory's (region), a make's, a model's.
enum class CarCriteria : int32_t { None = 0, Region = 1, Make = 2, Model = 3 };

// A special offer's damage (the dealer entry's +0x24): none, at the front, at a side, at the back.
enum class ShowroomDamage : int32_t { None = 0, Front = 1, Side = 2, Back = 3 };

// The dealer (career +0x70, 0x4FB4 bytes).
struct Dealer {
    DealerMode mode = DealerMode::Garage;   // +0x0000
    uint32_t entry = 0;                     // +0x0004: the entry chosen (an address; Career::dealerEntry here)
    int32_t deal = 0;                       // +0x0008: a car bought or sold, done when the window is left
    int32_t fromStats = 0;                  // +0x000C: the garage opened from CMSStatsCarCollection
    int32_t afterRace = 0;                  // +0x0010: the garage opened after a race (BACK: the career menu)
    std::array<DealerOffer, 159> list{};    // +0x0014: the entries listed
    int32_t listCount = 0;                  // +0x27D4
    uint32_t listCars = 0;                  // +0x27D8: the cars of the list (an address: the showroom or the garage)
    int32_t chosen = 0;                     // +0x27DC: the chosen entry's index
    int32_t level = 0;                      // +0x27E0
    int32_t listClass = 0;                  // +0x27E4: the class listed (-1 every class)
    uint32_t showroom = 0;                  // +0x27E8: the showroom's cars (an address; Career::dealerShowroom here)
    std::array<DealerOffer, 159> offers{};  // +0x27EC
    int32_t offerCount = 0;                 // +0x4FAC
    uint32_t offerCars = 0;                 // +0x4FB0: a car for each offer (an address; Career::dealerCars here)
};
static_assert(sizeof(Dealer) == 0x4FB4);

// Money and respect over time (career +0x7C, 0x70C bytes): 9 series of 50 values.
struct History {
    std::array<std::array<float, 50>, 9> series{};
    int32_t position = 0;                   // +0x708
};
static_assert(sizeof(History) == 0x70C);

#pragma pack(pop)

// A racer of the race of an event (0x64 bytes from the session's +0x0C).
struct Racer {
    int32_t place = 0;           // +0x00: the grid place after the sort
    int32_t crew = 0;            // +0x04: the crew number
    int32_t member = 0;          // +0x08: the crew member (0 the leader)
    int32_t car = 0;             // +0x0C: an index into the car list
    const std::vector<Car>* cars = nullptr;  // +0x10: the garage for crew 0, else the opponents' cars
    std::wstring name;           // +0x14: the member's short name
    int32_t number = 0;          // +0x24: the racer's number in the order made
    int32_t finished = 0;        // +0x28: the racer finished (0x004B9CF0)
    // +0x2C: the race's car of the racer, the i-th car the race makes (0x00495DE0: 0x00496641, 0x00496D98, 0x0049713A;
    // on the network 0x004964BF); read by 0x004BDECF. Here found by the car's record.
    int32_t raceCar = 0;
    int32_t driver = 0;          // +0x30: the player's own racer (member slot 0)
    int32_t rammed = 0;          // +0x34: of the player's crew, hit a car ahead in the race: 2 hard, 1 lightly
    int32_t crashed = 0;         // +0x38: of the player's crew, hit what is not a car: 2 hard, 1 lightly
    float skill = 0.0f;          // +0x3C: the member's ability / 100 (a crewmate's driving ability)
    float composure = 0.0f;      // +0x40: the member's concentration / 100 (a crewmate's recover)
    float nerveGain = 0.0f;      // +0x44: 1.4 / concentration (a crewmate: 1.5 / recover, 0 for 0)
    float nerveLoss = 0.0f;      // +0x48: concentration x 0.0005 (a crewmate: recover x 0.001)
    int32_t level = 0;           // +0x4C: how hard the driver drives: 0 low, 1 medium, 2 high (0x004BEFD0)
    float panicTime = 0.0f;      // +0x50: the seconds the nerves have been above 1.9
    float nerves = 0.0f;         // +0x54: 0 .. 2
    int32_t order = 0;           // +0x58: the crew's order to the driver
    float orderTime = 0.0f;      // +0x5C
    float levelTimer = 0.0f;     // +0x60: until the level is chosen again; 1 .. 5 s at random at the start
};

// The odds of a crew (0x14 bytes from the session's +0x268): to win `against` for `stake`.
struct CrewOdds {
    float raceTime = 0.0f;       // +0x00: the crew's time of the race
    float against = 0.0f;        // +0x04
    float stake = 1.0f;          // +0x08
    int32_t staked = 0;          // +0x0C: the player's money on the crew; added up as whole numbers by 0x004BC2E0
    float winnings = 0.0f;       // +0x10: what it won when the crew came first
};

// The session (career +0x30, 0x37C bytes; made by 0x004B8450).
struct RaceSession {
    uint32_t count = 0;                       // +0x000: racers, at most 6
    const CalendarEvent* event = nullptr;     // +0x004
    const EventRound* round = nullptr;        // +0x008
    std::array<Racer, 6> racers{};            // +0x00C
    int32_t needsSetup = 1;                   // +0x264: set by the constructor and before a new race
    std::array<CrewOdds, 9> odds{};           // +0x268
    std::array<int32_t, 9> finished{};        // +0x31C: per crew, its racers finished (0x004B9CF0)
    std::array<int32_t, 6> order{};           // +0x340: the crews by place, as each crew's team has finished (-1)
    std::array<int32_t, 3> unused{};          // +0x358: read by nothing found
    int32_t placed = 0;                       // +0x364: the crews in the order
    int32_t winner = -1;                      // +0x368: the racer that finished first
    int32_t winnerCrew = -1;                  // +0x36C
    int32_t bet = 0;                          // +0x370: the bets' +0x00
    uint32_t betRacer = 0;                    // +0x374: the racer of the crew bet on
    int32_t location = 0;                     // +0x378: the location of the event crew's first circuit
};

// The AI drivers of the race (career +0x74, 0x68 bytes).
struct RaceDrivers {
    RaceSession* session = nullptr;  // +0x00
    int32_t withMembers = 0;         // +0x0C: the player's crew races with a member
    int32_t ordersShown = 0;         // +0x10: the crew orders shown in the race (as +0x0C)
    int32_t slotsTaken = 0;          // +0x14: the members' cars taken (0x004BDE90 once, then 1; 0x004BDB4B, 0x004BEAB3)
    // +0x18 the crew's members racing, +0x1C + 8 n their cars (the racer's +0x2C), each with a float 0 after
    // (0x004BDE90); a car none hides that line of the orders (0x004BDBC0). The race HUD keeps them (m_crewSlots).
    int32_t slotCount = 0;
    int32_t slotCar0 = 0;
    int32_t slotCar1 = 0;
    int32_t slotCar2 = 0;
    int32_t pad = 0;      // +0x60: the controller whose tactics keys are read (+0x78 .. +0x88), taken once in the
                          // frame ([[[[G+0x34] + 0x0C]]] + 0x08, 0x004BDF8E); here crewOrders' keys
    // +0x64: the racers' names' time: 2 s after a tactics key (0x004BDF00), down by the frame time (0x004BDF5A); read by
    // the network's overlay (0x00456BA0) and the labels over the racers (0x004BF320) (Game/Race/TrackView).
    float namesTime = 0.0f;
};

// A bet amount of the betting screen and its weight.
struct BetAmount {
    int32_t amount = 0;
    float weight = 0.0f;
};

// The bets on a race (career +0x6C, 0x84 bytes).
// What is bet with the rival crew (the bets' +0x10): money, a pink slip, money of an amount asked for.
enum class BetKind : int32_t { Money = 0, PinkSlip = 1, AmountAsked = 2 };

struct CareerBets {
    int32_t placed = 0;                       // +0x00: a bet with the rival crew is made
    int32_t won = 0;                          // +0x04: the player won the bet race
    int32_t crew = 0;                         // +0x08: the crew bet with
    float amount = 0.0f;                      // +0x0C: the money bet
    BetKind kind = BetKind::Money;            // +0x10
    int32_t firstStage = 0;                   // +0x14: where the betting window's talk starts
    int32_t pinkSlip = 0;                     // +0x18: a pink slip race
    float rivalMoney = 0.0f;                  // +0x1C: the rival crew's money
    std::array<BetAmount, 7> amounts{};       // +0x20
    int32_t amountCount = 0;                  // +0x58
    std::array<int32_t, 9> talkedTo{};        // +0x5C: per crew; 1 for a crew the betting window talked with
    int32_t rivalAsked = 0;                   // +0x80: a rival crew came for a money bet
};
static_assert(sizeof(CareerBets) == 0x84);

// Money counters of the career (career +0x58, 0x90 bytes).
struct CareerStats {
    int32_t moneyPaid = 0;                    // +0x00: bets and the looks under a bonnet
    int32_t moneyReceived = 0;                // +0x04: payments of a negative amount
    int32_t oddsStaked = 0;                   // +0x08: the money put on the crews' odds
    int32_t oddsWon = 0;                      // +0x0C: the money the odds won
    int32_t racesEntered = 0;                 // +0x10: races started with the profile's +0x618
    int32_t wins = 0;                         // +0x14: races the player's crew won (0x004B9E20)
    // +0x18 + 0x18 x a crewmate's number: [0] the driving ability and [2] the recovery it learnt in races (0x004AA3B0)
    std::array<std::array<float, 6>, 4> learnt{};
    std::array<int32_t, 6> order{};           // +0x78: the crews' order of the last race (0x004B9E20)
};
static_assert(sizeof(CareerStats) == 0x90);

// The crews' flyers (the object at the career's +0x00, CMSFlyers.txt, 0x004B2C20): the player's font colours (+0x00,
// 4 bytes each, 21 of them; a flyer writes the chosen logo over the last, +0x50, 0x004B2E20) and for each crew (+0x54 +
// 0x24 x crew) its font colour and its logo (+0x04); colours 0xFFRRGGBB.
struct Flyers {
    struct CrewFlyer {
        uint32_t colour = 0;
        std::string logo;
    };
    std::array<uint32_t, 21> playerColours{};
    size_t playerCount = 0;  // the PlayerFontColour lines read
    std::array<CrewFlyer, 9> crews{};
};

// A list of messages for the career's windows (0x324 bytes; career +0x38 and +0x3C): a kind and three numbers
// each, at most 50 (0x004B5C70 adds; a full list keeps what it has).
struct CareerNotes {
    std::array<int32_t, 50> kind{};           // +0x000
    std::array<float, 50> a{};                // +0x0C8
    std::array<float, 50> b{};                // +0x190
    std::array<float, 50> c{};                // +0x258
    int32_t count = 0;                        // +0x320

    void add(int32_t k, float va, float vb, float vc) {
        if (count >= 50) return;
        const auto i = static_cast<size_t>(count);
        kind[i] = k;
        a[i] = va;
        b[i] = vb;
        c[i] = vc;
        ++count;
    }
};

// What the race's news last told (career +0x34, 0x34 bytes, not saved; 0x004BD130), and the race's respect.
struct RaceNews {
    int32_t place = 0;                        // +0x00: the player's crew's place (-1 at the start)
    int32_t leadStep = 0;                     // +0x04: the lead / 100 (/ 20 in a round of type 5)
    int32_t unused = 0;                       // +0x08: only cleared
    std::array<int32_t, 8> rams{};            // +0x0C: crews 1 .. 8, their rams as whole numbers
    int32_t winningBet = 0;                   // +0x2C
    float respect = 0.0f;                     // +0x30: the race's respect last worked out
};
static_assert(sizeof(RaceNews) == 0x34);

// The career's advisor (its +0x78 object, 0x74 bytes; Core/Career/CareerAdvice.h): the advice CMSAdvice gives and
// what CMSMainMenu opens for it. +0x24 .. +0x59 and +0x60 are saved.
struct Advisor {
    int32_t chosen = 0;                     // +0x00: set once an advice is chosen
    // +0x04 .. +0x10: CMSMainMenu (and CMSCalendar) opens a window for the advice taken: the phone (advices 7, 9),
    // the calendar to host a meeting (12), the calendar (the others), the dealership (11).
    int32_t openPhone = 0;
    int32_t openHosting = 0;
    int32_t openCalendar = 0;
    int32_t openDealer = 0;
    int32_t challengeDue = 0;               // +0x14: a challenge's advice is due (0x004A57B0)
    int32_t lastAdvice = 0;                 // +0x18: the advice given last (13 none)
    int32_t advice = 0;                     // +0x1C: the advice to give (13 none)
    uint32_t event = 0;                     // +0x20: its event in the stock code (here Career::adviceEvent), cleared
    std::array<int16_t, 27> toldRatings{};  // +0x24: per crew x 3 challenges, the car rating told
    std::array<uint8_t, 2> padding{};       // +0x5A
    int32_t rating = 0;                     // +0x5C: the rating found by 0x004A6190
    int32_t hosted = 0;                     // +0x60: no advice to host events once set
    int32_t unused = 0;                     // +0x64: not used
    int32_t crew = 0;                       // +0x68: the crew of the challenge found
    int32_t challenge = 0;                  // +0x6C: the challenge found
    int32_t targetCrew = 0;                 // +0x70: the crew 0x004A6100 found

    // The rating told for crew x 3 + challenge: the stock code writes the u16 at +0x24 + 2 x index unchecked; here
    // only within the advisor's bytes.
    void tell(int32_t index, int16_t value) {
        const size_t at = offsetof(Advisor, toldRatings) + static_cast<size_t>(2 * index);
        if (at + 2 <= sizeof(Advisor)) std::memcpy(reinterpret_cast<uint8_t*>(this) + at, &value, 2);
    }
};
static_assert(sizeof(Advisor) == 0x74 && offsetof(Advisor, toldRatings) == 0x24 && offsetof(Advisor, rating) == 0x5C);

struct Career {
    uint32_t head = 0;                      // written by the save system before the career
    Flyers flyers;                          // the career's +0x00 object (not saved)
    CareerProfile profile;
    std::array<int32_t, 4> members{};       // the crewmate numbers of the member slots, as saved
    std::array<Crew, 9> crews{};
    Calendar calendar;
    RaceSession session;                    // +0x30 (not saved)
    RaceNews news;                          // +0x34 (not saved)
    CareerNotes meeting;                    // +0x38: CMSMeetingSummary's (not saved)
    CareerNotes respectChanges;             // +0x3C: CMSRespectChanges' (not saved)
    std::array<Crewmate, 4> crewmates{};
    CareerStats stats;                      // +0x58 (not saved)
    Phone phone;
    Cruise cruise;                          // +0x60: the cruise manager (not saved; made by 0x004AA5F0)
    CareerBets bets;                        // +0x6C (not saved)
    Dealer dealer;
    RaceDrivers drivers;                    // +0x74 (not saved)
    Advisor advisor;                        // +0x78
    const CalendarEvent* adviceEvent = nullptr;  // the advisor's +0x20: the advice's event (not saved)
    CalendarEvent* meetingEvent = nullptr;  // the profile's +0x610 ([0x00728C50]): the event gone to
    // [career + 0x64] +0x00 and +0x04: the cheats "character test mode" and "win all races" (0x004A6E50).
    std::array<int32_t, 2> cheatFlags{};
    const DealerOffer* dealerEntry = nullptr;  // the dealer's +0x04: the entry chosen, of its list or its offers
    std::vector<Car> dealerShowroom;        // the dealer's +0x27E8: the cars taken to the garage (not saved)
    std::vector<Car> dealerCars;            // the dealer's +0x4FB0: a car for each offer (not saved)
    History history;
    std::vector<Car> opponentCars;          // career +0x80
    std::vector<Car> garage;                // career +0x84
    uint32_t garageTail = 0;                // the garage list's +0x10
    // The online career's pink slip (career +0x88 .. +0xAC, not saved): its stakes (0x004A49A0) and their end
    // (0x004A4C60).
    std::vector<Car> onlineOwnStake;        // +0x88: a list holding a copy of our car
    std::vector<Car> onlineRivalStake;      // +0x8C: a list holding a copy of the rival's car
    int32_t onlinePinkSlip = 0;             // +0x90: the stakes set
    int32_t onlineSettled = 0;              // +0x94: the pink slip settled, its note to show
    int32_t onlineWon = 0;                  // +0x98
    int32_t onlineAutosave = 0;             // +0x9C: the autosave to ask for after the note
    int32_t valueAC = 0;                    // +0xAC
    int32_t onlineLeftPinkSlip = 0;         // +0xB0: an invitation taken at a pink slip event (0x0047C6D0)
    std::vector<Car> padding;               // default cars filling the garage up to 55, as saved
    uint32_t paddingTail = 0;

    bool read(const std::vector<uint8_t>& data, std::string* error);
    std::vector<uint8_t> write() const;
};

}  // namespace oj
