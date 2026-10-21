#pragma once
// The career's phone (the career's +0x5C object; docs/spec/08-career.md "The phone"): the messages waiting for a call,
// the call and the contacts called.

#include "Core/Career/Career.h"
#include "Core/Base/Random.h"

#include <cstdint>
#include <string>

namespace oj {

// The crews' names of the phone's contacts and calls (0x00718738).
extern const char* const kPhoneCrewNames[9];

// Game text lines of the phone's calls.
namespace phonetext {
constexpr int32_t kYes = 105;              // "YES"
constexpr int32_t kNo = 106;               // "NO"
constexpr int32_t kEndCall = 313;          // "END CALL"
constexpr int32_t kContacts = 314;         // "CONTACTS"
constexpr int32_t kLetJoin = 315;          // "Let driver join crew?"
constexpr int32_t kJoinMyCrew = 319;       // "Join my crew?"
constexpr int32_t kHostChallenge = 321;    // "Host challenge?"
constexpr int32_t kPinkSlipRace = 324;     // "Pink slip race?"
constexpr int32_t kRaceForPinks = 327;     // "RACE FOR PINKS."
constexpr int32_t kTakeChallenge = 331;    // "TAKE A CHALLENGE."
constexpr int32_t kDeleteMessage = 335;    // "DELETE MESSAGE"
constexpr int32_t kNewMessage = 336;       // "NEW MESSAGE"
constexpr int32_t kDealerTexts = 487;      // ", I have a beauty of a car if you're interested." .. (4)
}  // namespace phonetext

// 0x004B63B0: a message into the first free slot unless one of that type, kind and name (case aside) waits already.
void queuePhoneMessage(Phone& phone, PhoneCaller kind, const std::string& name, int32_t crew, int32_t sample,
                       int32_t param, CallType type, int32_t text, int32_t answer0, int32_t answer1, int32_t answer2,
                       int32_t answer3);

// 0x004B64C0 from the dealer's new stock (0x004B4BE0): Mr. Jeff's text message (type 10, kind 2, "Mr. Jeffs Motors",
// no sample): the player's leader's name and one of four lines (487 + rand() & 3), answered "DELETE MESSAGE".
void queueDealerMessage(Career& career, CrtRandom& random);

// 0x004B6580: the call's type, sample, its crew, the text and the four answers; its sample not played yet.
void setPhoneCall(Phone& phone, CallType type, int32_t sample, int32_t param, int32_t text, int32_t answer0,
                  int32_t answer1, int32_t answer2, int32_t answer3);

// 0x004B69B0: the phone's state; idle (0) waits 5 .. 15 s (rand()) for the next message's call and forgets the crew
// and crewmate shown; the speech's line stopped (0x0040B600) between the two.
void setPhoneState(Phone& phone, PhoneState state, CrtRandom& random);

// 0x004B6A10: each crew 1 .. 8 past its second step (+0x5C) with the player's money over 40000, two to 54 garage cars,
// more days since its last call (+0x2CC) than 17 less its leader's id and fewer than 3 pink slip races (+0x2D0):
// +0x2CC 0 and a pink slip call (type 5) queued.
void crewPhoneCalls(Career& career);

// 0x004B6710: the contact called: ringing 4 .. 7 s (rand()); a crewmate (kind 1) who joined answers at once (type 11,
// no sample, END CALL), else asks "Join my crew?" (type 1); a crew's leader (kind 0) offers a challenge (type 8) by the
// advisor: told to take a challenge (+0x1C 9: the challenge's rating written to the advisor's table), told to race for
// pinks (7), else both while the player has cars; the call's kind, crew and name, the phone calling (2).
void dialContact(Career& career, int32_t contact, CrtRandom& random);

}  // namespace oj
