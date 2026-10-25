#include "Game/Frontend/Career/CharacterScreen.h"

#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Career/Money.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Cars/CarRepair.h"
#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Race/RaceSession.h"
#include "Game/Media/Movies.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <format>

namespace oj::frontend {

namespace {

// The talk's steps (+0xD0) that pick their clip with 0x00433750.
enum Stage {
    kTest = 0,  // the character test mode's first step (cheat 5)
    kIdle = 1,
    kGreet = 3,
    kCommentCar = 4,
    kSuggest = 7,
    kBetMenu = 8,
    kWait = 9,
    kAmounts = 10,
    kAmountWait = 11,
    kAmountAnswer = 12,
    kSuggestPinkSlip = 13,
    kSeeBonnet = 14,
    kCarStats = 15,
    kCarStatsDone = 16,
    kAccept = 17,
    kDecline = 18,
    kStartRace = 19,
    kAutosave = 20,
    kLeave = 21,
    // The leader on the race bet with, after it (0x004321B0, 0x00432360, 0x00432280, 0x004323B0): nothing sets
    // these steps (the talk starts at 3 or 13, the bets' +0x14), the window after the race (characterPostRace) plays
    // those clips; kept as the stock code has them.
    kWinsCash = 22,
    kWinsPinkSlip = 23,
    kLosesCash = 24,
    kLosesPinkSlip = 25,
    kThinkRaise = 26,
    kRaise = 27,
    kThinkLower = 28,
    kLower = 29,
};

// 0x004B1650: the movie folder of a crew's leader.
std::string leaderFolder(int crew) {
    static const char* const kFolders[] = {"", "TK", "Biggi", "Sue", "Maria", "Jack", "Melody", "Poppa", "Carlos"};
    // Another crew number leaves the folder as it was; empty here.
    return crew >= 1 && crew <= 8 ? kFolders[crew] : "";
}

// 0x00715AE0: the character test mode's clips (cheat 5) by its +0x0C, 0x0042F140; the six words after them are 0.
constexpr const char* kTestClips[] = {
    "_GREETING1", "_GREETING_WON_LAST1", "_GREETING_WON_LAST2", "_GREETING_WON_LAST3", "_GREETING_LOST_LAST1",
    "_GREETING_LOST_LAST2", "_GREETING_LOST_LAST3", "_DECLINE_CHALLENGE1", "_DECLINE_CHALLENGE2", "_DECLINE_CHALLENGE3",
    "_COMMENT_CAR_NEG1", "_COMMENT_CAR_NEG2", "_COMMENT_CAR_NEG3", "_COMMENT_CAR_POS1", "_COMMENT_CAR_POS2",
    "_COMMENT_CAR_POS3", "_COMMENT_CAR_NEUTRAL1", "_COMMENT_CAR_NEUTRAL2", "_COMMENT_CAR_NEUTRAL3", "_SUGGEST_BET1",
    "_SUGGEST_BET2", "_SUGGEST_BET3", "_HOW_MUCH1", "_HOW_MUCH2", "_HOW_MUCH3", "_ACCEPT_BET1", "_ACCEPT_BET2",
    "_ACCEPT_BET3", "_DECLINE_BET_LOW1", "_DECLINE_BET_LOW2", "_DECLINE_BET_LOW3", "_DECLINE_BET1", "_DECLINE_BET2",
    "_DECLINE_BET3", "_DECLINE_BET_HIGH1", "_DECLINE_BET_HIGH2", "_DECLINE_BET_HIGH3", "_RESPONCE_SEE_BONNET1",
    "_RESPONCE_SEE_BONNET2", "_RESPONCE_SEE_BONNET3", "_OKAY1", "_OKAY2", "_OKAY3", "_THINKING1", "_THINKING2",
    "_THINKING3", "_YES1", "_YES2", "_YES3", "_WAITCYCLE1", "_WAITCYCLE2", "_WAITCYCLE3", "_PLAYER_ACCEPT_CHALLENGE1",
    "_PLAYER_ACCEPT_CHALLENGE2", "_PLAYER_ACCEPT_CHALLENGE3", "_PLAYER_DECLINE_BET1", "_PLAYER_DECLINE_BET2",
    "_PLAYER_DECLINE_BET3", "_IMPATIENT1", "_IMPATIENT2", "_IMPATIENT3", "_YAWN1", "_COUGH1", "_SIGH1", "_WINS_CASH1",
    "_WINS_CASH2", "_WINS_CASH3", "_LOSES_CASH1", "_LOSES_CASH2", "_LOSES_CASH3", "_WIN_PINKSLIP1", "_LOSES_PINKSLIP1",
    "_SUGGEST_PINKSLIP1", "_SUGGEST_PINKSLIP2", "_SUGGEST_PINKSLIP3",
};

// One of three clips by rand() % 3: 1 the second, 2 the third, else the first.
const char* ofThree(uint32_t r, const char* first, const char* second, const char* third) {
    return r % 3 == 1 ? second : r % 3 == 2 ? third : first;
}

}  // namespace

std::wstring CharacterScreen::line(size_t index) const {
    return m_texts != nullptr ? m_texts->at(index) : std::wstring();
}

std::string CharacterScreen::clipPrefix() const {
    return m_leader + "\\" + m_leader;
}

void LeaderMovie::bind(ui::Screen& s) {
    screen = &s;
    element = s.find("MOVIE");
}

// The clip plays from the element's next draw (0x005AA3A0); a missing take is played as take 1 or 2 of the same clip
// (0x00559B10), and a player of no file at all (CHALLENGE_INTRO1 and _OUTRO1 of every leader but TK) counts as
// finished, so that clip ends at once.
void LeaderMovie::play(CareerHost& host, const std::string& name) {
    if (screen == nullptr || element < 0) return;
    screen->playMovie(element, name, false);
    // 0x00434762: MOVIE's +0x114 the speech volume ([G+0x64]+0x5C, the settings' +0x08).
    screen->setMovieVolume(element, std::bit_cast<float>(host.settingsValue(save::Settings::kSpeechVolume)));
    screen->holdMovie(element, false);
    log::info("character: clip {}", name);
}

bool LeaderMovie::finished() const {
    const movie::Player* p = screen != nullptr ? screen->elementMovie(element) : nullptr;
    return p != nullptr && p->finished();
}

void LeaderMovie::hold(bool on) {
    if (screen != nullptr) screen->holdMovie(element, on);
}

void CharacterScreen::leave(CareerHost& host) {
    host.setMusicVolume(std::bit_cast<float>(host.settingsValue(save::Settings::kMusicVolume)), false);
    m_movie.hold(true);
    if (m_movie.screen != nullptr) m_movie.screen->setMovieVolume(m_movie.element, 0.0f);
    m_views.free();
}

// BET_CASH: the amount of the bet (0x004A3A90).
void CharacterScreen::setCash(ui::Screen& screen, int value) {
    screen.setText("BET_CASH", toWide(moneyText(value, m_language)));
}

// The menu shown again with its transition from the start, and the arrows.
void CharacterScreen::showMenuAgain(ui::Screen& screen) {
    screen.setHidden("Menu", true);
    screen.show("Menu", true);
}

void CharacterScreen::cashTexts(ui::Screen& screen, const GameData& data) {
    if (m_texts != nullptr) careerCashTexts(screen, *data.career, m_language, *m_texts);
}

void careerCashTexts(ui::Screen& screen, const Career& career, int language, const TextTable& texts) {
    const auto line = [&](size_t index) { return texts.at(index); };
    const int m_language = language;
    screen.setText("Cash", toWide(moneyText(career.profile.money, m_language)));
    const int day = career.calendar.dayOfMonth;
    const int month = career.calendar.month;
    std::wstring date = toWide(std::format("{}", day + 1));
    if (m_language == 3) {
        if (day == 0) date += line(kTextOrdinals + 3);
    } else if (m_language == 1) {
        if (day == 0) date += L"er";
    } else if (m_language == 4) {
        date += L" de ";
    } else {
        // 1st, 2nd, 3rd, then th; 11th .. 14th.
        const int last = (day + 1) % 10;
        size_t suffix = kTextOrdinals + 3;
        if ((day < 10 || day > 13) && last >= 1 && last <= 3) {
            suffix = kTextOrdinals + static_cast<size_t>(last) - 1;
        }
        date += line(suffix);
    }
    // Stock problem: a negative month index reads before the table; the first month here.
    const int m = month % 12;
    date += L" " + line(kTextMonths + static_cast<size_t>(m < 0 ? 0 : m));
    screen.setText("Date", date);
}

void crewRespect(CareerHost& host, Career& career, int crew, RespectReason kind, int flag, int language,
                 const TextTable& texts) {
    const auto crewIndex = static_cast<size_t>(crew);
    if (crewIndex >= career.crews.size()) return;
    Crew& c = career.crews[crewIndex];
    const float respect = static_cast<float>(c.respect);
    std::wstring comment;
    if (kind == RespectReason::ChallengeCompleted) {
        comment = texts.at(kTextChallengeCompleted);
    } else {
        if (language != 4) {
            comment = widenSigned(std::string_view(c.memberNames[0].data(), c.memberNames[0].size()));
        }
        comment += texts.at(kTextRespectReasons + static_cast<size_t>(kind));
    }
    float change = 0.0f;
    switch (kind) {
    case RespectReason::HighBet:
    case RespectReason::BetAccepted: change = 20.0f; break;
    case RespectReason::LowBet:
    case RespectReason::LowerDeclined: change = -25.0f; break;
    // The value before the months' days (0x00718ED8, 1000) less 20.
    case RespectReason::BackedOut: change = flag != 0 ? -(respect - 980.0f) : -35.0f; break;
    case RespectReason::WaitedTooLong: change = -30.0f; break;
    case RespectReason::RaiseAccepted: change = 10.0f; break;
    case RespectReason::RaiseDeclined: change = 30.0f; break;
    case RespectReason::ChallengeCompleted: change = 50.0f; break;
    default: break;
    }
    comment += L" " + texts.at(kTextRespect) + L" ";
    if (change > 0.0f) comment += L"+";
    comment += toWide(std::format("{}", static_cast<int>(change)));
    host.respectMessage(crew, respect, change, comment);
    changeRespect(c, change);
}

// A message of the leader's crew of the bets on its respect (0x004BC7B0 with the flag 0).
void CharacterScreen::respect(CareerHost& host, Career& career, RespectReason kind) {
    if (m_texts == nullptr) return;
    crewRespect(host, career, career.bets.crew, kind, 0, m_language, *m_texts);
}

// The bet's crew leader joins the phone's contacts ("<short name> (<crew>)", 0x004B6640); a new one is told in a note.
void CharacterScreen::phoneContact(CareerHost& host, Career& career) {
    const auto crewIndex = static_cast<size_t>(career.bets.crew);
    if (crewIndex >= career.crews.size()) return;
    const Crew& crew = career.crews[crewIndex];
    static const char* const kCrewNames[] = {"Player's crew", "Urban Maulerz", "A.W.B",     "Vixens", "The Wild Cats",
                                             "Omega Tau",     "Public Chaos",  "The Lordz", "Legion"};
    const auto id = static_cast<size_t>(crew.id);
    const std::string shortName(crew.memberShortNames[0].data(),
                                strnlen(crew.memberShortNames[0].data(), crew.memberShortNames[0].size()));
    const std::string name = std::format("{} ({})", shortName, id < 9 ? kCrewNames[id] : "");
    Phone& phone = career.phone;
    for (int32_t i = 0; i < phone.contactCount && i < static_cast<int32_t>(phone.contacts.size()); ++i) {
        if (_stricmp(name.c_str(), phone.contacts[static_cast<size_t>(i)].name.data()) == 0) return;
    }
    if (phone.contactCount < 0 || phone.contactCount >= static_cast<int32_t>(phone.contacts.size())) {
        // Stock problem: past 26 contacts the stock code writes after the list.
        log::warning("character: no room for a phone contact");
        return;
    }
    PhoneContact& contact = phone.contacts[static_cast<size_t>(phone.contactCount)];
    contact.kind = PhoneCaller::CrewLeader;
    contact.name.fill('\0');
    std::memcpy(contact.name.data(), name.c_str(), std::min(name.size(), contact.name.size() - 1));
    contact.crew = crew.id;
    ++phone.contactCount;
    const std::wstring text =
        widenSigned(std::string_view(crew.memberNames[0].data(), crew.memberNames[0].size())) + L" (" +
        line(kTextCrewNames + (id < 9 ? id : 0)) + L") " + line(kTextContact);
    host.message(MessageType::Continue, text, false, nullptr);
    m_movie.hold(true);
}

void CharacterScreen::enter(CareerHost& host, ui::Screen& screen) {
    GameData& data = *host.gameData();
    const Language& language = host.language();
    const TextTable& texts = host.gameTexts();
    m_texts = &texts;
    m_language = language.index;
    Career& career = *data.career;
    CareerBets& bets = career.bets;
    m_lowered = 0;
    m_raised = 0;
    m_refusedHigher = 0;
    m_waits = 0;
    m_amountAnswer = 0;
    for (const char* name : {"Pref", "PrefVal", "YourCash", "YourCashVal"}) {
        if (bets.pinkSlip == 0) {
            screen.show(name, false);
        } else {
            screen.setHidden(name, true);
        }
    }
    m_kind = bets.kind;
    m_stage = bets.firstStage;
    m_playing = 0;
    m_skipFade = 1.0f;
    m_leader = leaderFolder(bets.crew);
    m_pinkSlip = bets.pinkSlip;
    m_amountIndex = 0;
    m_tries = 1;
    m_movie.bind(screen);
    screen.setHidden("Menu", true);
    m_shownCash = 0;
    m_cash = 0;
    screen.hideContaining("ARROW");
    screen.hideContaining("BET");
    if (bets.pinkSlip == 0) screen.setHidden("carbox", true);
    // The cars of the leader and the player in CAR_CHARACTER and CAR_PLAYER, both hidden for a money bet.
    m_views.make(screen, data, host, true);

    screen.setText("PrefVal", toWide(moneyText(static_cast<int>(bets.rivalMoney), m_language)));
    const auto crewIndex = static_cast<size_t>(bets.crew);
    const Crew* crew = crewIndex < career.crews.size() ? &career.crews[crewIndex] : nullptr;
    screen.setText("CharCrew", line(kTextCrewNames + (crewIndex < 9 ? crewIndex : 0)));
    if (crew != nullptr) {
        screen.setText("CharName",
                       widenSigned(std::string_view(crew->memberNames[0].data(), crew->memberNames[0].size())));
        screen.setMeter("SkillMeter", crew->members[0].ability * 0.01f);
        screen.setMeter("AggMeter", crew->members[0].concentration * 0.01f);
        screen.setMultiline("BossName", true);
        screen.setMultiline("BossCar", true);
        screen.setText("BossName", widenSigned(std::string_view(crew->memberShortNames[0].data(),
                                                                crew->memberShortNames[0].size())));
    }
    auto carName = [&](int32_t crewNumber) {
        const std::vector<Car>& cars = carsOf(career, crewNumber);
        const auto index = static_cast<size_t>(leaderCar(career, crewNumber));
        return index < cars.size() && cars[index].def != nullptr ? toWide(cars[index].def->displayName)
                                                                 : std::wstring();
    };
    screen.setText("BossCar", carName(bets.crew));
    screen.setHidden("BossName", bets.pinkSlip == 0);
    screen.setHidden("BossCar", bets.pinkSlip == 0);
    screen.setMultiline("YourName", true);
    screen.setMultiline("YourCar", true);
    screen.setText("YourName", widenSigned(std::string_view(career.crews[0].memberShortNames[0].data(),
                                                            career.crews[0].memberShortNames[0].size())));
    screen.setText("YourCar", carName(0));
    screen.setHidden("YourName", bets.pinkSlip == 0);
    screen.setHidden("YourCar", bets.pinkSlip == 0);
    m_lookPrice = (bets.crew + 2) * 500;
    cashTexts(screen, data);
    m_quitAsked = 0;
    m_noAsked = 0;
    m_pinkAsked = 0;
    m_yesAsked = 0;
    m_answer = -1;
    // 0x0042EFED: the character test mode (cheat 5, [career + 0x64] +0x00; its code cannot be typed on the PC, see
    // CMSCheats) starts the talk at step 0, +0x0C and +0x10 0.
    if (career.cheatFlags[kCheatCharacterTest] != 0) {
        m_stage = kTest;
        m_debugClip = 0;
    }
    m_inputOn = 1;
    // 0x0042EF3B: the front end's music at 0.
    host.setMusicVolume(0.0f, false);
    log::info("character: crew {} ({}), bet kind {}, first step {}", bets.crew, m_leader, static_cast<int>(m_kind),
              m_stage);
}

// The clip of the talk's step (0x00433750); some steps also move on, make bets or open windows.
void CharacterScreen::pickClip(ui::Screen& screen, GameData& data, CrtRandom& random, CareerHost& host,
                               std::string& clip) {
    Career& career = *data.career;
    CareerBets& bets = career.bets;
    CareerProfile& profile = career.profile;
    const auto crewIndex = static_cast<size_t>(bets.crew);
    Crew* crew = crewIndex < career.crews.size() ? &career.crews[crewIndex] : nullptr;
    auto draw = [&] { return static_cast<uint32_t>(random.next()); };
    switch (m_stage) {
    case kGreet: {
        // 0x0042F1D0: the last race against the crew: none, won (1) or lost.
        const int32_t last = crew != nullptr ? crew->lastBetWon : -1;
        if (last == -1) {
            if (profile.introduction == 0) {
                clip += "_GREETING1";
            } else {
                clip += "_CHALLENGE_INTRO1";
                career.session.betRacer = betRacer(career);
                career.session.bet = 1;
            }
        } else if (last == 1) {
            clip += ofThree(draw(), "_GREETING_WON_LAST1", "_GREETING_WON_LAST2", "_GREETING_WON_LAST3");
        } else {
            clip += ofThree(draw(), "_GREETING_LOST_LAST1", "_GREETING_LOST_LAST2", "_GREETING_LOST_LAST3");
        }
        m_stage = kCommentCar;
        break;
    }
    case kCommentCar: {
        // 0x0042F490: the leader on the player's car by the ratings of the two leaders' cars.
        for (const char* name : {"sel_but", "sel_text", "back_but", "back_text"}) screen.show(name, false);
        m_inputOn = 1;
        float player = 0.0f;
        float leader = 0.0f;
        const RaceSession& s = career.session;
        for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
            const Racer& r = s.racers[i];
            if (r.member != 0) continue;
            const std::vector<Car>& cars = carsOf(career, r.crew);
            const float rating = r.car >= 0 && static_cast<size_t>(r.car) < cars.size()
                                     ? Car::asFloat(cars[static_cast<size_t>(r.car)].rating)
                                     : 0.0f;
            if (r.crew == 0) player = rating;
            if (r.crew == bets.crew) leader = rating;
        }
        const float lead = player - leader;
        if (lead <= 20.0f) {
            if (-20.0f <= lead) {
                clip += ofThree(draw(), "_COMMENT_CAR_NEUTRAL1", "_COMMENT_CAR_NEUTRAL2", "_COMMENT_CAR_NEUTRAL3");
            } else {
                clip += ofThree(draw(), "_COMMENT_CAR_POS1", "_COMMENT_CAR_POS2", "_COMMENT_CAR_POS3");
            }
        } else {
            clip += ofThree(draw(), "_COMMENT_CAR_NEG1", "_COMMENT_CAR_NEG2", "_COMMENT_CAR_NEG3");
        }
        m_stage = kSuggest;
        for (const char* name : {"CAR_CHARACTER", "CAR_PLAYER", "carbox", "YourName", "BossName", "YourCar",
                                 "BossCar"}) {
            screen.setHidden(name, false);
        }
        break;
    }
    case kSuggest:
        // 0x0042F910
        if (m_kind == BetKind::PinkSlip) {
            m_stage = kBetMenu;
            clip += ofThree(draw(), "_SUGGEST_PINKSLIP1", "_SUGGEST_PINKSLIP2", "_SUGGEST_PINKSLIP3");
        } else if (m_kind == BetKind::AmountAsked) {
            m_stage = kAmounts;
            clip += ofThree(draw(), "_HOW_MUCH1", "_HOW_MUCH2", "_HOW_MUCH3");
        } else {
            m_stage = kBetMenu;
            // Stock problem: the pick is rand() & 1, so _SUGGEST_BET3 (for 2) never plays. The fixed build picks one of
            // three as the other lines (only TK has a third take: the others' movie open takes 1 or 2, 0x00559B10).
            clip += ofThree(draw(), "_SUGGEST_BET1", "_SUGGEST_BET2", "_SUGGEST_BET3");
        }
        break;
    case kBetMenu: {
        // 0x0042FBE0: the bet menu; CHECK CAR while the player has more money than its price.
        showMenuAgain(screen);
        screen.clearMenu("Menu");
        screen.addCommandItem("Menu", line(kTextAcceptBet), 0.0f, "yes");
        if (profile.introduction == 0) screen.addCommandItem("Menu", line(kTextDeclineBet), 0.0f, "no");
        screen.addCommandItem("Menu", line(kTextLowerBet), 0.0f, "lower");
        screen.addCommandItem("Menu", line(kTextRaiseBet), 0.0f, "raise");
        if (profile.money > m_lookPrice) {
            screen.addCommandItem("Menu",
                                  line(kTextCheckCar) + L" (" + toWide(moneyText(m_lookPrice, m_language)) + L")",
                                  0.0f, "hood");
        }
        screen.showContaining("ARROW", true);
        clip += "_WAITCYCLE1";
        m_stage = kWait;
        screen.showContaining("BET", true);
        if (m_kind == BetKind::Money) {
            m_cash = static_cast<int32_t>(bets.rivalMoney);
        } else if (m_kind == BetKind::PinkSlip) {
            screen.setText("BET_CASH", line(kTextPinkSlip));
        }
        break;
    }
    case kWait:
    case kAmountWait: {
        // 0x00431F90: a wait clip, every tenth an impatient one.
        if (++m_waits < 10) {
            const uint32_t r = draw() % 100;
            if (r < 20) {
                clip += "_WAITCYCLE1";
            } else if (r < 40) {
                clip += "_WAITCYCLE2";
            } else if (r < 60) {
                clip += "_WAITCYCLE3";
            } else if (r < 75) {
                clip += "_COUGH1";
            } else if (r < 85) {
                clip += "_YAWN1";
            } else {
                clip += profile.introduction == 0 ? "_SIGH1" : "_COUGH1";
            }
        } else {
            m_waits = 0;
            clip += m_impatience == 2 ? "_IMPATIENT1" : m_impatience == 3 ? "_IMPATIENT3" : "_IMPATIENT2";
            if (++m_impatience == 4) m_impatience = 0;
        }
        break;
    }
    case kAmounts: {
        // 0x0042FDE0: the amounts the player has the money for, after "Back out".
        showMenuAgain(screen);
        screen.showContaining("ARROW", true);
        screen.hideContaining("BET", true);
        screen.clearMenu("Menu");
        screen.addMenuItem("Menu", line(kTextBackOut), -1.0f);
        for (int32_t i = 0; i < bets.amountCount && i < static_cast<int32_t>(bets.amounts.size()); ++i) {
            const int32_t amount = bets.amounts[static_cast<size_t>(i)].amount;
            if (amount < profile.money) {
                screen.addMenuItem("Menu", toWide(moneyText(amount, m_language)), static_cast<float>(i));
            }
        }
        m_stage = kAmountWait;
        clip += "_WAITCYCLE1";
        break;
    }
    case kAmountAnswer: {
        // 0x0042FFB0: the leader takes the amount by its weight, else turns it down as too low or too high.
        const size_t index = static_cast<size_t>(std::clamp(m_amountIndex, 0, 6));
        const float weight = bets.amounts[index].weight;
        if (static_cast<float>(draw()) * 3.0518509e-05f < weight) {
            clip += ofThree(draw(), "_ACCEPT_BET1", "_ACCEPT_BET2", "_ACCEPT_BET3");
            m_stage = kAutosave;
            placeBet(career, bets.amounts[index].amount);
            career.session.betRacer = betRacer(career);
            career.session.bet = 1;
            phoneContact(host, career);
        } else if (m_amountIndex < bets.amountCount / 2) {
            keepHighBets(bets);
            respect(host, career, RespectReason::LowBet);
            if (m_tries == 0) {
                m_stage = kLeave;
                clip += ofThree(draw(), "_DECLINE_BET1", "_DECLINE_BET2", "_DECLINE_BET3");
            } else {
                clip += ofThree(draw(), "_DECLINE_BET_LOW1", "_DECLINE_BET_LOW2", "_DECLINE_BET_LOW3");
                --m_tries;
                m_stage = kAmounts;
            }
        } else {
            bets.amountCount = 4;
            if (m_tries == 0) {
                respect(host, career, RespectReason::WaitedTooLong);
                m_stage = kLeave;
                clip += ofThree(draw(), "_DECLINE_BET1", "_DECLINE_BET2", "_DECLINE_BET3");
            } else {
                respect(host, career, RespectReason::HighBet);
                clip += ofThree(draw(), "_DECLINE_BET_HIGH1", "_DECLINE_BET_HIGH2", "_DECLINE_BET_HIGH3");
                --m_tries;
                m_stage = kAmounts;
            }
        }
        break;
    }
    case kSuggestPinkSlip:
        // 0x00432440
        showMenuAgain(screen);
        screen.clearMenu("Menu");
        screen.addCommandItem("Menu", line(kTextYes), 0.0f, "yes");
        screen.addCommandItem("Menu", line(kTextBackOut), 0.0f, "no");
        screen.showContaining("ARROW", true);
        m_inputOn = 1;
        clip += ofThree(draw(), "_SUGGEST_PINKSLIP1", "_SUGGEST_PINKSLIP2", "_SUGGEST_PINKSLIP3");
        m_stage = kWait;
        break;
    case kSeeBonnet:
        // 0x00430650
        clip += ofThree(draw(), "_RESPONCE_SEE_BONNET1", "_RESPONCE_SEE_BONNET2", "_RESPONCE_SEE_BONNET3");
        m_stage = kCarStats;
        screen.removeMenuItem("Menu", "hood");
        screen.selectMenuItem("Menu", 0);
        break;
    case kCarStats:
        // 0x00430770: the car of the leader of the crew bet with (0x004B1730; of the garage for crew 0, else of the
        // opponents' cars), not of the garage, in car_stats2purple over this window.
        {
            std::vector<Car>& cars = career.bets.crew == 0 ? career.garage : career.opponentCars;
            const auto index = static_cast<size_t>(leaderCar(career, career.bets.crew));
            host.statsCar(index < cars.size() ? &cars[index] : nullptr, 0);
        }
        host.request("car_stats2purple", SwitchMode::Push);
        m_stage = kCarStatsDone;
        clip += "_WAITCYCLE1";
        break;
    case kCarStatsDone:
        // 0x00430830: the look is paid.
        clip += ofThree(draw(), "_OKAY1", "_OKAY2", "_OKAY3");
        showMenuAgain(screen);
        screen.showContaining("ARROW", true);
        m_stage = kWait;
        pay(career, m_lookPrice);
        cashTexts(screen, data);
        screen.setText("YourCashVal", toWide(moneyText(profile.money, m_language)));
        break;
    case kAccept: {
        // 0x00431760
        clip += ofThree(draw(), "_PLAYER_ACCEPT_CHALLENGE1", "_PLAYER_ACCEPT_CHALLENGE2", "_PLAYER_ACCEPT_CHALLENGE3");
        if (bets.pinkSlip == 0) {
            if (m_lowered == 0) respect(host, career, RespectReason::BetAccepted);
            const auto money = static_cast<int32_t>(bets.rivalMoney);
            placeBet(career, m_lowered != 0 ? money / 2 : m_raised != 0 ? money * 2 : money);
            phoneContact(host, career);
        } else {
            // The pink slip: the car views freed; a copy of the player's car (the profile's +0x600 of the garage,
            // 0x00434D90) kept as the stake and the car taken out of the garage (0x00489CD0); the event marked done
            // (+0x01) and the race set up again (0x004B8570). The autosave that follows keeps the garage without it.
            // Stock problem: the copy has a new grid, so the car's kept dents are gone (a repair for nothing); the
            // fixed build's stake keeps them.
            m_views.free();
            const auto index = static_cast<size_t>(career.profile.raceCars[0]);
            if (index < career.garage.size()) {
                m_stake = career.garage[index];
                career.garage.erase(career.garage.begin() + static_cast<std::ptrdiff_t>(index));
            }
            if (CalendarEvent* e = currentEvent(career.calendar)) e->done = 1;
            data.setupRace(random);
        }
        m_stage = kAutosave;
        career.session.betRacer = betRacer(career);
        career.session.bet = 1;
        break;
    }
    case kDecline:
        // 0x00431CC0
        clip += ofThree(draw(), "_PLAYER_DECLINE_BET1", "_PLAYER_DECLINE_BET2", "_PLAYER_DECLINE_BET3");
        if (m_refusedHigher == 0) respect(host, career, RespectReason::BackedOut);
        m_stage = kLeave;
        break;
    case kThinkRaise:
        // 0x004309F0
        clip += ofThree(draw(), "_THINKING1", "_THINKING2", "_THINKING3");
        m_stage = kRaise;
        break;
    case kThinkLower:
        // 0x00431190
        clip += ofThree(draw(), "_THINKING1", "_THINKING2", "_THINKING3");
        m_stage = kLower;
        break;
    case kRaise:
    case kLower: {
        // 0x00430AD0 / 0x00431270: the leader takes the higher bet when rand() % 18 < crew x 2 + 4, the lower one when
        // it is above that.
        const auto limit = static_cast<uint32_t>(bets.crew * 2 + 4);
        const uint32_t r = draw() % 18;
        const bool raising = m_stage == kRaise;
        if (raising) {
            if (r < limit) {
                clip += ofThree(draw(), "_ACCEPT_BET1", "_ACCEPT_BET2", "_ACCEPT_BET3");
                respect(host, career, RespectReason::RaiseAccepted);
                m_raised = 1;
            } else {
                clip += ofThree(draw(), "_DECLINE_BET_HIGH1", "_DECLINE_BET_HIGH2", "_DECLINE_BET_HIGH3");
                respect(host, career, RespectReason::RaiseDeclined);
                m_refusedHigher = 1;
            }
        } else {
            if (limit < r) {
                clip += ofThree(draw(), "_YES1", "_YES2", "_YES3");
                m_lowered = 1;
            } else {
                clip += ofThree(draw(), "_DECLINE_BET_LOW1", "_DECLINE_BET_LOW2", "_DECLINE_BET_LOW3");
                respect(host, career, RespectReason::LowerDeclined);
            }
        }
        m_stage = kWait;
        screen.removeMenuItem("Menu", "raise");
        screen.removeMenuItem("Menu", "lower");
        screen.selectMenuItem("Menu", 0);
        showMenuAgain(screen);
        screen.showContaining("ARROW", true);
        // The amount: twice (raise taken) or half (lower taken) the money cut to a whole number. Stock problem: else
        // the text shows the money cut, the amount keeps it rounded. The fixed build shows what is kept.
        const auto cut = static_cast<int32_t>(bets.rivalMoney);
        const bool taken = raising ? m_raised != 0 : m_lowered != 0;
        const int32_t changed = raising ? cut * 2 : cut / 2;
        const int32_t kept = taken ? changed : static_cast<int32_t>(std::nearbyint(bets.rivalMoney));
        m_cash = kept;
        m_shownCash = kept;
        setCash(screen, kept);
        break;
    }
    case kAutosave: {
        // 0x00432ECC: the autosave over the window when the save system saves; a pink slip's stake goes back
        // after it (+0x18).
        if (data.saving != 0) host.request("autosave", SwitchMode::Push);
        if (bets.pinkSlip != 0) m_stakeBack = 1;
        m_stage = kStartRace;
        m_playing = 0;
        m_skipFade = 1.0f;
        m_shownCash = m_cash;
        setCash(screen, m_shownCash);
        break;
    }
    case kWinsCash:
        // 0x004321B0: the count at 0x00715AD8 (1 .. 5, read by nothing) on.
        clip += ofThree(draw(), "_WINS_CASH1", "_WINS_CASH2", "_WINS_CASH3");
        if (++m_winsCount > 5) m_winsCount = 1;
        m_stage = m_kind == BetKind::PinkSlip ? kWinsPinkSlip : kLeave;
        break;
    case kWinsPinkSlip:
        // 0x00432360
        clip += "_WIN_PINKSLIP1";
        m_stage = kLeave;
        break;
    case kLosesCash:
        // 0x00432280: the count at 0x00715AD4 (1 .. 5, read by nothing) on.
        clip += ofThree(draw(), "_LOSES_CASH1", "_LOSES_CASH2", "_LOSES_CASH3");
        if (++m_losesCount > 5) m_losesCount = 1;
        m_stage = m_kind == BetKind::PinkSlip ? kLosesPinkSlip : kLeave;
        break;
    case kLosesPinkSlip:
        // 0x004323B0: rand() & 1.
        clip += (draw() & 1) != 0 ? "_LOSES_PINKSLIP1" : "_LOSES_PINKSLIP2";
        m_stage = kLeave;
        break;
    default:
        // 0x0042F140 (the step 0 of the character test mode, and 2, 5, 6 and past 29, which nothing sets): the test
        // mode's clip of +0x0C, BOSSCAR shown (+0x58 0) with the clip's name for its text (0x0049A850, its text
        // modifier started again as 0x005B19C0 does). Stock problem: +0x0C of 75 .. 80 names no clip (a null
        // pointer's length, the game stops); here the leader's folder alone.
        if (m_debugClip >= 0 && m_debugClip < static_cast<int32_t>(std::size(kTestClips))) {
            clip += kTestClips[static_cast<size_t>(m_debugClip)];
        }
        screen.setHidden("BOSSCAR", false);
        screen.setText("BOSSCAR", widenSigned(clip));
        break;
    }
}

void CharacterScreen::update(CareerHost& host, ui::Screen& screen, uint32_t event) {
    GameData& data = *host.gameData();
    CrtRandom& random = gameRandom();
    float dt = host.frameTime();
    Career& career = *data.career;
    CareerBets& bets = career.bets;
    if (m_cash != 0) {
        m_shownCash = m_cash;
        setCash(screen, m_shownCash);
    }
    // 0x00432696: a pink slip's stake back at the garage's end after the autosave: a copy of it (0x00434D90; the fixed
    // build's with its grid, as above), the event's +0x01 cleared, rated (0x0048E8A0), the profile's +0x600 the new
    // car, each racer of crew 0 racing it.
    if (m_stakeBack != 0) {
        if (m_stake) {
            Car car = *m_stake;
            if (CalendarEvent* e = currentEvent(career.calendar)) e->done = 0;
            if (car.def != nullptr) {
                rateCar(car, data.dynamics ? data.dynamics->of(*car.def) : nullptr, data.catalogue);
            }
            career.garage.push_back(std::move(car));
            career.profile.raceCars[0] = static_cast<int32_t>(career.garage.size()) - 1;
            RaceSession& session = career.session;
            for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
                if (session.racers[i].crew == 0) session.racers[i].car = career.profile.raceCars[0];
            }
        }
        m_stake.reset();
        m_stakeBack = 0;
    }
    // The answers of the questions asked.
    if (m_quitAsked != 0) {
        if (m_answer == 1) {
            host.request("menu", SwitchMode::Replace);
        } else {
            m_movie.hold(false);
        }
        m_quitAsked = 0;
        m_answer = 0;
    }
    if (m_noAsked != 0) {
        screen.hideContaining("ARROW");
        if (m_answer == 1) {
            m_stage = kDecline;
            m_inputOn = 0;
            screen.setHidden("Menu", true);
        } else {
            screen.setHidden("Menu", false);
        }
        m_playing = 0;
        m_noAsked = 0;
        m_answer = 0;
    }
    for (int32_t* asked : {&m_pinkAsked, &m_yesAsked}) {
        if (*asked == 0) continue;
        if (m_answer == 1) {
            m_stage = kAccept;
        } else {
            screen.hideContaining("ARROW");
            screen.setHidden("Menu", false);
        }
        m_playing = 0;
        *asked = 0;
        m_answer = 0;
    }
    m_views.frame(host, random);
    if (m_movie.finished()) {
        m_playing = 0;
    }
    if (m_skipFade != 0.0f) {
        m_skipFade = std::clamp(m_skipFade - dt, 0.0f, 1.0f);
    }
    const auto alpha = static_cast<uint32_t>(x87::chop(m_skipFade * 255.0f)) & 0xFF;
    screen.setBoxColour("SKIPFADE", alpha << 24);

    if (m_stage == kStartRace) {
        // 0x004A3CC0 starts the race and opens "loading".
        // The save system's +0x84 is the settings' +0x30.
        const auto automaticGearbox = static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
        if (bets.pinkSlip == 0 && career.profile.introduction != 0) {
            m_movie.hold(true);
            updateBetShare(career);
            host.sound("CLICK");
            data.startRace(random, automaticGearbox);
            host.request("loading", SwitchMode::Replace);
        } else if (bets.pinkSlip == 0) {
            host.sound("CLICK");
            data.startRace(random, automaticGearbox);
            host.request("loading", SwitchMode::Replace);
            updateBetShare(career);
            host.sound("CLICK");
        } else {
            host.sound("CLICK");
            host.request("loading", SwitchMode::Replace);
        }
        m_stage = kIdle;
        return;
    }

    if (m_playing == 0) {
        std::string clip = clipPrefix();
        if (m_stage == kIdle) {
            return;
        }
        if (m_stage == kLeave) {
            m_movie.hold(true);
            if (bets.pinkSlip != 0) {
                // 0x00432FA3: the event lost (0x004A3A60: its round's +0x01 by its +0x55 and its +0x01 set), its end
                // (0x004A6870), CLICK and the career's next window (0x004BAEE0). Stock problem: the round (+0x55, signed,
                // 0x004A3A73 movsx) other than 0 writes past the event's one round, into the calendar's other bytes;
                // here the same byte while it is in the calendar.
                if (CalendarEvent* e = currentEvent(career.calendar)) {
                    auto* calendar = reinterpret_cast<uint8_t*>(&career.calendar);
                    const std::ptrdiff_t at = (reinterpret_cast<uint8_t*>(e) - calendar) + 0x1D +
                                              static_cast<std::ptrdiff_t>(static_cast<int8_t>(e->roundIndex)) * 0x38;
                    if (at >= 0 && at < static_cast<std::ptrdiff_t>(sizeof(Calendar))) calendar[at] = 1;
                    e->done = 1;
                    endEvent(career, *e, data.tables);
                }
                host.sound("CLICK");
                host.afterRaceResults();
            } else {
                host.request("CMSGridWalk", SwitchMode::Replace);
            }
            m_stage = kIdle;
            return;
        }
        pickClip(screen, data, random, host, clip);
        m_movie.play(host, clip);
        m_playing = 1;
    }

    // 0x004330E7: the character test mode takes the events in place of the window: SELECT counts +0x0C up through 81
    // and BACK down (the clip of the step 0, 0x0042F140); event 6 bets on the next crew (1 .. 8 round): a pink slip
    // kind 1 at step 13, else kind 0 at step 3 with +0x80 set, the bets on the crew (0x004B1870), its +0x5C set, no
    // amount, the leader's movie folder (0x004B1650). After them a clip playing gives CLICK, +0xD4 0 and SKIPFADE 1.
    // Other events: nothing.
    if (career.cheatFlags[kCheatCharacterTest] != 0) {
        if (event == input::kSelect) {
            if (++m_debugClip == 81) m_debugClip = 0;
        } else if (event == input::kBack) {
            if (--m_debugClip < 0) m_debugClip += 81;
        } else if (event == input::kSelect2) {
            int32_t crew = bets.crew + 1;
            if (crew == 9) crew = 1;
            bets.crew = crew;
            if (bets.pinkSlip != 0) {
                bets.kind = BetKind::PinkSlip;
                bets.firstStage = 13;
            } else {
                bets.rivalAsked = 1;
                bets.kind = BetKind::Money;
                bets.firstStage = 3;
            }
            setupBets(career, crew);
            bets.talkedTo[static_cast<size_t>(crew)] = 1;
            bets.amount = 0.0f;
            m_leader = leaderFolder(crew);
        } else {
            return;
        }
        if (m_playing != 0) {
            host.sound("CLICK");
            m_playing = 0;
            m_skipFade = 1.0f;
        }
        return;
    }

    if (m_amountAnswer == 1) {
        m_amountAnswer = -1;
        m_stage = kAmountAnswer;
        m_amountIndex = static_cast<int32_t>(screen.menuItemValue("Menu", screen.selectedMenuItem("Menu")));
        m_cash = bets.amounts[static_cast<size_t>(std::clamp(m_amountIndex, 0, 6))].amount;
        screen.hideContaining("ARROW", true);
        screen.setHidden("Menu", true);
        screen.showContaining("BET", true);
        m_playing = 0;
        m_skipFade = 1.0f;
    }

    auto skip = [&] {
        if (m_playing != 0) {
            host.sound("CLICK");
            m_playing = 0;
            m_skipFade = 1.0f;
        }
    };
    if (event == input::kSelect) {
        if (m_inputOn == 0) return;
        m_shownCash = m_cash;
        const size_t selected = screen.selectedMenuItem("Menu");
        if (m_stage == kWait) {
            host.sound("CLICK");
            screen.hideContaining("ARROW", true);
            screen.setHidden("Menu", true);
            const std::string command = screen.menuItemCommand("Menu", selected);
            if (_stricmp(command.c_str(), "yes") == 0) {
                if (bets.pinkSlip == 0) {
                    m_yesAsked = 1;
                    host.message(MessageType::YesNo, line(kTextPlaceBet) + L"\n" + line(kTextAreYouSure), false,
                                 &m_answer, true);
                } else {
                    m_pinkAsked = 1;
                    host.message(MessageType::YesNo, line(kTextPinkWarning) + L"\n" + line(kTextPinkLoss), false,
                                 &m_answer, true);
                }
                m_movie.hold(true);
            } else if (_stricmp(command.c_str(), "no") == 0) {
                m_noAsked = 1;
                host.message(MessageType::YesNo, line(kTextAreYouSure), false, &m_answer, true);
                m_movie.hold(true);
            } else if (_stricmp(command.c_str(), "hood") == 0) {
                m_stage = kSeeBonnet;
            } else if (_stricmp(command.c_str(), "raise") == 0) {
                m_stage = kThinkRaise;
            } else if (_stricmp(command.c_str(), "lower") == 0) {
                m_stage = kThinkLower;
            }
        } else if (m_stage == kAmountWait) {
            host.sound("CLICK");
            m_amountIndex = static_cast<int32_t>(screen.menuItemValue("Menu", selected));
            screen.setText("BET_CASH", screen.menuItemText("Menu", selected));
            if (m_amountIndex == -1) {
                m_noAsked = 1;
                host.message(MessageType::YesNo, line(kTextAreYouSure), false, &m_answer, true);
            } else {
                m_movie.hold(true);
                host.message(MessageType::YesNo, line(kTextPlaceBet) + L"\n" + line(kTextAreYouSure), false,
                             &m_amountAnswer, true);
            }
            m_movie.hold(true);
        } else {
            skip();
        }
    } else if (event == input::kBack) {
        if (career.profile.introduction != 0 && m_inputOn != 0) {
            m_movie.hold(true);
            m_quitAsked = 1;
            host.message(MessageType::YesNo,
                         line(kTextQuitCareer) + L"\n" + line(kTextProgressLost) + L"\n" + line(kTextAreYouSure), false,
                         &m_answer, true);
            m_movie.hold(true);
        }
        if (career.profile.introduction == 0 && m_inputOn != 0) {
            m_noAsked = 1;
            host.message(MessageType::YesNo, line(kTextAreYouSure), false, &m_answer, true);
            m_movie.hold(true);
        }
    }
}

// Pot and YourCash shown; Outcome over several lines. Pink slip: "PINK SLIP WON" (the bets' +0x04) or "PINK SLIP LOST",
// Pot and YourCash hidden (+0x58), cash and PotVal emptied. Money won: twice the bet (cut) paid to the player (as
// 0x004B8200), "BET WON", and each race car's +0xAC and the profile's +0x4EC take the bet (cut); lost: "BET LOST".
// The clip "<leader>\<leader>" and by the bet: pink slip won _LOSES_PINKSLIP1, lost _WIN_PINKSLIP1; money won
// _LOSES_CASH1 / 2 / 3, lost _WINS_CASH1 / 2 / 3 (rand() % 3: 1 the second, 2 the third). +0x48 .. +0x50 3 s, +0x38 0.
void CharacterPostRace::enter(CareerHost& host, ui::Screen& screen) {
    GameData& data = *host.gameData();
    CrtRandom& random = gameRandom();
    const Language& language = host.language();
    const TextTable& texts = host.gameTexts();
    m_texts = &texts;
    m_language = language.index;
    Career& career = *data.career;
    CareerBets& bets = career.bets;
    CareerProfile& profile = career.profile;
    // 0x0042E6C8: the front end's music ([game + 0x38] with its manager and its handle) at 0 now and as its target.
    host.setMusicVolume(0.0f, false);
    m_playing = 0;
    m_skipFade = 1.0f;
    m_movie.bind(screen);
    m_leader = leaderFolder(bets.crew);
    for (const char* name : {"BossName", "BossCar", "YourName", "YourCar"}) screen.setMultiline(name, true);
    // The cars of the leader and the player in CAR_CHARACTER and CAR_PLAYER as in the betting window.
    m_views.make(screen, data, host, false);
    const auto crewIndex = static_cast<size_t>(bets.crew);
    auto carName = [&](int32_t crewNumber) {
        const std::vector<Car>& cars = carsOf(career, crewNumber);
        const auto index = static_cast<size_t>(leaderCar(career, crewNumber));
        return index < cars.size() && cars[index].def != nullptr ? toWide(cars[index].def->displayName)
                                                                 : std::wstring();
    };
    if (crewIndex < career.crews.size()) {
        const Crew& crew = career.crews[crewIndex];
        screen.setText("BossName",
                       widenSigned(std::string_view(crew.memberShortNames[0].data(), crew.memberShortNames[0].size())));
    }
    screen.setText("BossCar", carName(bets.crew));
    screen.setText("YourName", widenSigned(std::string_view(career.crews[0].memberShortNames[0].data(),
                                                            career.crews[0].memberShortNames[0].size())));
    screen.setText("YourCar", carName(0));
    screen.setText("cash", toWide(moneyText(profile.money, m_language)));
    m_shown = static_cast<float>(profile.money);
    const int32_t pot = x87::chop(bets.amount) * 2;
    screen.setText("PotVal", toWide(moneyText(pot, m_language)));
    m_pot = static_cast<float>(pot);
    m_step = m_pot * 0.05f;
    screen.show("Pot", false);
    screen.show("YourCash", false);
    screen.setMultiline("Outcome", true);
    std::wstring outcome;
    if (bets.pinkSlip != 0) {
        outcome = texts.at(kTextBetWon + (bets.won != 0 ? 2 : 3));
        screen.setHidden("Pot", true);
        screen.setHidden("YourCash", true);
        screen.setText("cash", L"");
        screen.setText("PotVal", L"");
    } else if (bets.won != 0) {
        pay(career, -pot);
        outcome = texts.at(kTextBetWon);
        const auto won = static_cast<float>(x87::chop(bets.amount));
        for (int32_t k = 0; k < profile.raceCarCount && k < 3; ++k) {
            const auto index = static_cast<size_t>(profile.raceCars[static_cast<size_t>(k)]);
            if (index >= career.garage.size()) continue;
            Car& car = career.garage[index];
            car.winnings = Car::bitsOf(Car::asFloat(car.winnings) + won);
            const auto prizes = static_cast<float>(static_cast<int32_t>(profile.prizesWon));
            profile.prizesWon = static_cast<uint32_t>(x87::chop(prizes + won));
        }
    } else {
        outcome = texts.at(kTextBetWon + 1);
    }
    screen.setText("Outcome", outcome);
    std::string clip = m_leader + "\\" + m_leader;
    if (bets.pinkSlip != 0) {
        clip += bets.won != 0 ? "_LOSES_PINKSLIP1" : "_WIN_PINKSLIP1";
    } else if (bets.won != 0) {
        clip += ofThree(static_cast<uint32_t>(random.next()), "_LOSES_CASH1", "_LOSES_CASH2", "_LOSES_CASH3");
    } else {
        clip += ofThree(static_cast<uint32_t>(random.next()), "_WINS_CASH1", "_WINS_CASH2", "_WINS_CASH3");
    }
    m_movie.play(host, clip);
    m_playing = 1;
    m_delay = 3.0f;
    m_pinkDelay1 = 3.0f;
    m_pinkDelay2 = 3.0f;
    m_outro = 0;
    log::info("character post race: crew {} ({}), {}, pot {}", bets.crew, m_leader,
              bets.pinkSlip != 0 ? "pink slip" : "money", pot);
}

// The clip finished: +0x94 0. SKIPFADE's +0x1C less the frame time, within 0 .. 1. With no clip playing the next:
// the first event's _CHALLENGE_OUTRO1 once (+0x38), else by rand() % 100 below 20 _WAITCYCLE1, 40 _WAITCYCLE2, 60
// _WAITCYCLE3, 75 _COUGH1, 85 _YAWN1, else _SIGH1 (the first event _COUGH1). SKIPFADE's colour black with the alpha
// +0x1C x 255. SELECT: in the first event T.K. pays the repair bill of the garage's first car (0x0048EBC0, cut, at
// most 5000) once (the profile's +0x04) when it is 10 or more: paid to the player (as 0x004B8200), cash the money,
// the money shown less it, "Repair bill for T.K.'s car " (553) and the bill (type 1), the movie paused (0x00435030);
// else CLICK, the movie stopped, the event's +0x01 set and the dealer (as 0x004BAEE0's first event). Not the first
// event: CLICK, the movie stopped, the next window (0x004BAEE0). Then 3 s (+0x48); a pink slip counts down +0x4C and
// +0x50; money: while the pot is over 0 the step moves from it (SWOOSH), to the money shown when the bet was won;
// cash the money shown (+ 0.5, cut), PotVal the pot (cut).
void CharacterPostRace::update(CareerHost& host, ui::Screen& screen, uint32_t event) {
    GameData& data = *host.gameData();
    CrtRandom& random = gameRandom();
    float dt = host.frameTime();
    Career& career = *data.career;
    CareerProfile& profile = career.profile;
    if (m_movie.finished()) m_playing = 0;
    if (m_skipFade != 0.0f) m_skipFade = std::clamp(m_skipFade - dt, 0.0f, 1.0f);
    if (m_playing == 0) {
        std::string clip = m_leader + "\\" + m_leader;
        if (profile.introduction != 0 && m_outro == 0) {
            clip += "_CHALLENGE_OUTRO1";
            m_outro = 1;
        } else {
            const uint32_t r = static_cast<uint32_t>(random.next()) % 100;
            if (r < 20) {
                clip += "_WAITCYCLE1";
            } else if (r < 40) {
                clip += "_WAITCYCLE2";
            } else if (r < 60) {
                clip += "_WAITCYCLE3";
            } else if (r < 75) {
                clip += "_COUGH1";
            } else if (r < 85) {
                clip += "_YAWN1";
            } else {
                clip += profile.introduction != 0 ? "_COUGH1" : "_SIGH1";
            }
        }
        m_movie.play(host, clip);
        m_playing = 1;
    }
    const auto alpha = static_cast<uint32_t>(x87::chop(m_skipFade * 255.0f)) & 0xFF;
    screen.setBoxColour("SKIPFADE", alpha << 24);

    if (event == input::kSelect) {
        if (profile.introduction != 0) {
            int32_t bill = 0;
            if (!career.garage.empty()) bill = x87::chop(repairCost(career.garage[0], data.catalogue));
            if (bill > 5000) bill = 5000;
            if (profile.introBillPaid == 0 && bill >= 10) {
                // The bill paid (0x004348A4 neg ecx; 0x004B8200 adds -bill to the money) and taken off the money
                // shown (+0x44).
                pay(career, bill);
                profile.introBillPaid = 1;
                screen.setText("cash", toWide(moneyText(profile.money, m_language)));
                m_shown = m_shown - static_cast<float>(bill);
                host.message(MessageType::Continue, m_texts->at(kTextRepairBill) + toWide(moneyText(bill, m_language)),
                             false, nullptr);
                m_movie.hold(true);
            } else {
                host.sound("CLICK");
                m_movie.hold(true);
                if (CalendarEvent* current = currentEvent(career.calendar)) current->done = 1;
                host.firstEventDealer();
                return;
            }
        } else {
            host.sound("CLICK");
            m_movie.hold(true);
            host.afterRaceResults();
        }
    }

    if (m_delay > 0.0f) {
        m_delay = m_delay - dt;
        return;
    }
    if (career.bets.pinkSlip != 0) {
        if (m_pinkDelay1 > 0.0f) {
            m_pinkDelay1 = m_pinkDelay1 - dt;
        } else if (m_pinkDelay2 > 0.0f) {
            m_pinkDelay2 = m_pinkDelay2 - dt;
        }
        return;
    }
    if (m_pot > 0.0f) {
        if (career.bets.won != 0) m_shown = m_step + m_shown;
        m_pot = m_pot - m_step;
        host.sound("SWOOSH");
    }
    screen.setText("cash", toWide(moneyText(x87::chop(m_shown + 0.5f), m_language)));
    screen.setText("PotVal", toWide(moneyText(x87::chop(m_pot), m_language)));
}

// A Menu element's highlighted item kept; the music's volume back; the movie stopped; the car views freed. A pink slip
// gives or takes the car (0x004B1A80, pinkSlipCar). The bets cleared (+0x00, +0x04, +0x0C, +0x18, +0x5C .. +0x80).
void BetCarViews::make(ui::Screen& screen, GameData& data, CareerHost& host, bool hide) {
    free();
    Career& career = *data.career;
    const CareerBets& bets = career.bets;
    scene::Engine* engine = host.engine();
    const auto make = [&](const char* element, int32_t crew, std::unique_ptr<scene::CarView>& view) {
        if (screen.find(element) < 0) return;
        if (engine != nullptr) {
            scene::CarView::Setup setup;
            setup.element = element;
            setup.zoom = 0.82f;
            setup.shadow = true;
            view = std::make_unique<scene::CarView>(*engine, screen, setup, data.catalogue, data.tables);
            std::vector<Car>& cars = crew == 0 ? career.garage : career.opponentCars;
            const auto index = static_cast<uint32_t>(leaderCar(career, crew));
            if (index < cars.size()) {
                view->show(cars[index], crew == 0 ? &career.garageTail : nullptr, index, 3.74f, 0.0f, true,
                           host.viewLoading());
            }
            view->setCameraPlace({0.0f, 110.0f, -560.0f, 0.0f});
        }
        if (hide) screen.setHidden(element, bets.pinkSlip == 0);
        if (view) view->finish(host.frameTime(), gameRandom());
    };
    if (screen.find("CAR_CHARACTER") >= 0) {
        const auto crewIndex = static_cast<size_t>(bets.crew);
        if (crewIndex < career.crews.size()) {
            applyCrewScheme(career, career.crews[crewIndex], data.defs, data.catalogue);
        }
    }
    make("CAR_CHARACTER", bets.crew, leader);
    make("CAR_PLAYER", 0, player);
}

void BetCarViews::frame(CareerHost& host, CrtRandom& random) {
    if (player) player->frame(host.frameTime(), random);
    if (leader) leader->frame(host.frameTime(), random);
}

void BetCarViews::free() {
    for (std::unique_ptr<scene::CarView>* view : {&player, &leader}) {
        if (*view) (*view)->clear();
        view->reset();
    }
}

void CharacterPostRace::leave(CareerHost& host) {
    GameData& data = *host.gameData();
    host.setMusicVolume(std::bit_cast<float>(host.settingsValue(save::Settings::kMusicVolume)), false);
    m_movie.hold(true);
    m_views.free();
    if (!data.career) return;
    if (data.career->bets.pinkSlip != 0) pinkSlipCar(*data.career, data.catalogue);
    clearBets(data.career->bets);
}

}  // namespace oj::frontend
