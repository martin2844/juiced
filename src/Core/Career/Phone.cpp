#include "Core/Career/Phone.h"

#include "Core/Career/CareerAdvice.h"
#include "Core/Race/RaceSounds.h"

#include <algorithm>
#include <cstring>
#include <format>

namespace oj {

const char* const kPhoneCrewNames[9] = {"Player's crew", "Urban Maulerz", "A.W.B",     "Vixens", "The Wild Cats",
                                        "Omega Tau",     "Public Chaos",  "The Lordz", "Legion"};

namespace {

void copyName(std::array<char, 0x80>& to, const std::string& name) {
    to.fill('\0');
    std::memcpy(to.data(), name.data(), std::min(name.size(), to.size() - 1));
}

std::string nameOf(const std::array<char, 25>& text) {
    return std::string(text.data(), strnlen(text.data(), text.size()));
}

// The call takes the dialled contact's kind, crew and name; the phone calls (2).
void callContact(Phone& phone, const PhoneContact& contact) {
    phone.call.kind = contact.kind;
    phone.call.crew = contact.crew;
    phone.state = PhoneState::Calling;
    copyName(phone.call.name, std::string(contact.name.data(), strnlen(contact.name.data(), contact.name.size())));
}

}  // namespace

void queuePhoneMessage(Phone& phone, PhoneCaller kind, const std::string& name, int32_t crew, int32_t sample,
                       int32_t param, CallType type, int32_t text, int32_t answer0, int32_t answer1, int32_t answer2,
                       int32_t answer3) {
    for (const PhoneMessage& m : phone.messages) {
        if (m.used != 0 && m.type == type && m.kind == kind && _stricmp(name.c_str(), m.name.data()) == 0) return;
    }
    for (PhoneMessage& m : phone.messages) {
        if (m.used != 0) continue;
        m.answers = {answer0, answer1, answer2, answer3};
        m.used = 1;
        m.type = type;
        m.text = text;
        m.textAfterLeader = 0;
        m.sample = sample;
        m.crew = crew;
        m.kind = kind;
        m.param = param;
        copyName(m.name, name);
        return;
    }
}

void queueDealerMessage(Career& career, CrtRandom& random) {
    const auto line = static_cast<int32_t>(static_cast<uint32_t>(random.next()) & 3);
    for (PhoneMessage& m : career.phone.messages) {
        if (m.used != 0) continue;
        m.used = 1;
        m.answers = {phonetext::kDeleteMessage, kPhoneNone, kPhoneNone, kPhoneNone};
        m.type = CallType::TextMessage;
        m.text = phonetext::kDealerTexts + line;
        m.textAfterLeader = 1;
        m.kind = PhoneCaller::Dealer;
        m.sample = -1;
        m.param = 0;
        m.crew = 0;
        copyName(m.name, "Mr. Jeffs Motors");
        return;
    }
}

void setPhoneCall(Phone& phone, CallType type, int32_t sample, int32_t param, int32_t text, int32_t answer0,
                  int32_t answer1, int32_t answer2, int32_t answer3) {
    PhoneMessage& c = phone.call;
    c.type = type;
    c.param = param;
    c.sample = sample;
    c.text = text;
    c.textAfterLeader = 0;
    c.answers = {answer0, answer1, answer2, answer3};
    c.played = 0;
}

void setPhoneState(Phone& phone, PhoneState state, CrtRandom& random) {
    phone.state = state;
    if (state != PhoneState::Idle) return;
    phone.timer = static_cast<float>(random.next()) * 0.00030518509f + 5.0f;
    if (void (*stop)() = speechStop()) stop();  // with the speech object, 0x0040B600
    phone.challengesCrew = 0;
    phone.shownCrewmate = 0;
}

void crewPhoneCalls(Career& career) {
    for (Crew& crew : career.crews) {
        const size_t garage = career.garage.size();
        if (crew.stage[2] == 0 || career.profile.money <= 40000 || garage <= 1 || garage >= 55) continue;
        if (crew.racesSinceCall <= 17 - crew.id || crew.pinkSlipsWon >= 3) continue;
        crew.racesSinceCall = 0;
        const auto id = static_cast<size_t>(std::clamp<int>(crew.id, 0, 8));
        const std::string name = std::format("{} ({})", nameOf(crew.memberShortNames[0]), kPhoneCrewNames[id]);
        queuePhoneMessage(career.phone, PhoneCaller::CrewLeader, name, crew.id, 4, crew.id, CallType::PinkSlip,
                          phonetext::kPinkSlipRace, phonetext::kYes, phonetext::kNo, kPhoneNone, kPhoneNone);
    }
}

void dialContact(Career& career, int32_t contact, CrtRandom& random) {
    Phone& phone = career.phone;
    phone.dialled = contact;
    phone.answersAtOnce = 0;
    phone.dialTimer = static_cast<float>(random.next()) * 9.155553e-05f + 4.0f;
    if (contact < 0 || contact >= static_cast<int32_t>(phone.contacts.size())) return;
    const PhoneContact& c = phone.contacts[static_cast<size_t>(contact)];
    if (c.kind == PhoneCaller::Crewmate) {
        // Stock problem: a contact of crewmate -1 reads through a null pointer; here it is one who has not joined.
        const bool known = c.crew >= 0 && c.crew < static_cast<int32_t>(career.crewmates.size());
        const Crewmate* mate = known ? &career.crewmates[static_cast<size_t>(c.crew)] : nullptr;
        if (mate != nullptr && mate->joined != 0) {
            phone.answersAtOnce = 1;
            setPhoneCall(phone, CallType::Reply, -1, 0, kPhoneNone, phonetext::kEndCall, kPhoneNone, kPhoneNone,
                         kPhoneNone);
            callContact(phone, c);
            return;
        }
        setPhoneCall(phone, CallType::JoinMyCrew, 0x21, c.crew, 318, phonetext::kJoinMyCrew, phonetext::kEndCall,
                     kPhoneNone, kPhoneNone);
        callContact(phone, c);
        phone.shownCrewmate = known ? c.crew + 1 : 0;
        return;
    }
    if (c.kind != PhoneCaller::CrewLeader) return;
    const int32_t advice = career.advisor.advice;
    if (advice == 9) {
        setPhoneCall(phone, CallType::LeaderOffer, 0, c.crew, 330, phonetext::kTakeChallenge, phonetext::kEndCall,
                     kPhoneNone, kPhoneNone);
        // The challenge's rating told: the advisor's table (+0x24, u16 per crew x 3 challenges).
        const int32_t told = career.advisor.crew * 3 + career.advisor.challenge;
        if (told >= 0) career.advisor.tell(told, static_cast<int16_t>(career.advisor.rating));
    } else if (advice == 7) {
        setPhoneCall(phone, CallType::LeaderOffer, 0, c.crew, 330, phonetext::kRaceForPinks, phonetext::kEndCall,
                     kPhoneNone, kPhoneNone);
    } else if (!career.garage.empty()) {
        setPhoneCall(phone, CallType::LeaderOffer, 0, c.crew, 330, phonetext::kTakeChallenge, phonetext::kRaceForPinks,
                     phonetext::kEndCall, kPhoneNone);
    } else {
        setPhoneCall(phone, CallType::LeaderOffer, 0, c.crew, 330, phonetext::kTakeChallenge, phonetext::kEndCall,
                     kPhoneNone, kPhoneNone);
    }
    callContact(phone, c);
}

}  // namespace oj
