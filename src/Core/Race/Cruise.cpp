#include "Core/Race/Cruise.h"
#include "Core/Race/RaceSounds.h"

#include "Core/Data/GameTexts.h"
#include "Core/Physics/CarBody.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Data/Language.h"
#include "Core/Career/Money.h"
#include "Core/Race/Racer.h"
#include "Core/Race/Replay.h"
#include "Core/Tracks/SceneObjects.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <format>

namespace oj {

namespace {

// The tricks' points factors (0x007187E0 + 4 x trick).
constexpr std::array<float, 18> kFactor = {0.0f,   1.0f,   0.25f,  0.5f,   0.5f,   0.5f,   0.5f, 75.0f, 500.0f,
                                           500.0f, 200.0f, 200.0f, 300.0f, 150.0f, 150.0f, 5.0f, 25.0f, 0.0f};
// The tricks' groups the special moves name (0x00718828) and their crowd kinds (0x00718870).
constexpr std::array<int32_t, 18> kGroup = {0, 1, 2, 4, 4, 6, 6, 7, 8, 8, 10, 10, 12, 13, 13, 15, 16, 17};
constexpr std::array<int32_t, 18> kCrowd = {0,           kHighSpeed,    kHighSpeedReverse, kRightDonut, kRightDonut,
                                            kRightDonut, kRightDonut,   kBootlegTurn,      kRight360,   kRight360,
                                            kRight180,   kRight180,     kBoomerang,        kRightJTurn, kRightJTurn,
                                            kDrift,      kNitrousDrift, kWipeout};  // by the trick

// The special moves (0x007188D8, 0x20 bytes each): the groups of the last four tricks (0 any; the oldest first), the
// points, the window of each step back, what the multiplier gains and the done flag (0x007188F4 + 0x20 x move).
struct SpecialMove {
    std::array<int32_t, 4> groups;
    int32_t points;
    int32_t window;
    int32_t add;
};
constexpr std::array<SpecialMove, 9> kMoves = {{
    {{0, 7, 4, 7}, 250, 30, 2},    {{0, 10, 6, 13}, 250, 30, 2}, {{7, 7, 7, 7}, 1000, 20, 6},
    {{0, 0, 8, 8}, 1000, 8, 4},    {{0, 0, 10, 13}, 250, 10, 2}, {{7, 10, 13, 7}, 1500, 10, 4},
    {{15, 15, 15, 15}, 500, 10, 2}, {{0, 0, 1, 8}, 2500, 4, 4},  {{0, 0, 1, 10}, 1500, 3, 4},
}};
std::array<int32_t, 9> g_moveDone{};  // 0x007188F4 + 0x20 x move
// The zones' names by CRUISE_MESSAGE ([0x00765C60 + 16 n], 1562 + n): MaxSpeed "HIGH SPEED", Doughnut "DONUT", UTurn
// "BOOTLEG TURN", m360 "360", m180 "180", Boomerang "BOOMERANG", JTurn "J-TURN", Drift "DRIFT".
constexpr std::array<const char*, 8> kZoneMessages = {"MaxSpeed", "Doughnut", "UTurn", "m360",
                                                      "m180",     "Boomerang", "JTurn", "Drift"};

// The cruise car's origin of the frame before ([0x00768A34], kept from race to race): the zones' test.
Vec4 g_carOrigin{};

bool sameText(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](char ch) { return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

// 0x004B1440: the crowd kind of a CRUISE_MOVE, else 0.
int32_t moveKind(std::string_view move) {
    static constexpr std::array<std::pair<const char*, int32_t>, 8> kKinds = {{
        {"MaxSpeed", kHighSpeed},
        {"Doughnut", kRightDonut},
        {"UTurn", kBootlegTurn},
        {"m360", kRight360},
        {"m180", kRight180},
        {"JTurn", kRightJTurn},
        {"Drift", kDrift},
        {"Boomerang", kBoomerang},
    }};
    for (const auto& [name, kind] : kKinds) {
        if (sameText(move, name)) return kind;
    }
    return 0;
}

// 0x00407B20: a car's model origin in centimetres (w as it is).
Vec4 originCm(const RaceCar& car) {
    const Vec4 o = car.body->origin();
    return {o[0] * 100.0f, o[1] * 100.0f, o[2] * 100.0f, o[3]};
}

float speedOf(const Vec4& v) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// 0x00407C00: the vector over its length plus 2^-23.
Vec4 normalised(Vec4 v) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    const double inverse = 1.0 / (static_cast<double>(len) + 1.1920929e-7);
    v[0] = static_cast<float>(inverse * v[0]);
    v[1] = static_cast<float>(inverse * v[1]);
    v[2] = static_cast<float>(inverse * v[2]);
    return v;
}

int32_t chopTimes(float v, double by) {
    return x87::chopDouble(static_cast<double>(v) * by);
}

}  // namespace

// 0x004AA730: the time 60 x minutes (the career's 3) for a cruise, else 60000; the warning at 10 s; the bank, combo,
// score and the rest cleared; the slots numbered, the special moves not done; the list's lines emptied; the crowd
// zones calmed (0x004B1510).
void Cruise::setup(int32_t minutes, RaceMode mode, RaceType type) {
    if (mode == RaceMode::Career) minutes = 3;
    started = 1;
    time = type == RaceType::Cruise ? static_cast<float>(minutes * 60) : 60000.0f;
    warning = 10.0f;
    comboTime = 0.0f;
    bank = 0.0f;
    combo = 0.0f;
    comboShown = 0.0f;
    total = 0.0f;
    scoreShown = 0.0f;
    soundTime = -1.0f;
    car = nullptr;
    multiplier = 1;
    multiplierShown = 1;
    idle = 0.0f;
    donutTime = 0.0f;
    speedTime = 0.0f;
    flash = 0.0f;
    crashed = 0;
    lastZone = nullptr;
    zone = nullptr;
    pause = 0.0f;
    dt = std::bit_cast<float>(0x3C88CE70u);
    carFactor = 1.0f;
    volumeTarget = 1.0f;
    volume = 1.0f;
    history = {};
    ages = {};
    g_moveDone = {};
    moves = {};
    for (size_t i = 0; i < tricks.size(); ++i) {
        tricks[i] = CruiseTrick{};
        tricks[i].index = static_cast<int32_t>(i);
    }
    for (CruiseMessage& m : messages) {
        m.bonus = 0;
        m.trick = 0;
        m.move = 0;
        m.name.clear();
        m.points.clear();
    }
    // 0x004AA88B on the network: six times (clearing +0x43C .. +0x450, read by nothing found) our score sent (Stock
    // problem: the same message six times).
    if (net.score) {
        for (int i = 0; i < 6; ++i) net.score(x87::nearest(total));
    }
    for (CruiseZone& z : zones) resetZone(z);
}

int32_t Cruise::score() const {
    return x87::chop(total);
}

// 0x00553E50's and 0x00553F10's slot 0 (0x00554570, 0x00554360).
bool CruiseZone::contains(const Vec4& point) const {
    return helperContains(matrix, shape, boxMin, boxMax, centre, radius, point);
}

// 0x004B0F40: the helper's node, its properties: CRUISE_MOVE the crowd kind, CRUISE_NAME, CRUISE_DIFFICULTY (5 without
// it, at least 5), CRUISE_CROWD; with CRUISE_MESSAGE and a CRUISE_NAME of text, the zone's name by the message (none
// for another message). Stock problem: the message is taken only when CRUISE_NAME has a text.
void Cruise::addZone(const SceneMoveZone& helper, const TextTable& texts) {
    CruiseZone z;
    for (size_t i = 0; i < 12; ++i) z.matrix[i] = helper.matrix[i];
    z.shape = helper.shape;
    z.boxMin = helper.boxMin;
    z.boxMax = helper.boxMax;
    z.centre = helper.centre;
    z.radius = helper.radius;
    if (!helper.properties.empty()) {
        const ObjectProperties props(helper.properties);
        if (props.has(ObjectProperties::CruiseMove)) {
            const std::string* move = props.text(ObjectProperties::CruiseMove);
            z.kind = move != nullptr ? moveKind(*move) : 0;
        }
        const ObjectProperties::Entry* name = props.entry(ObjectProperties::CruiseName);
        const bool named = name != nullptr && name->isText;
        if (props.has(ObjectProperties::CruiseName)) z.crowdName = named ? name->text : std::string();
        z.difficulty = props.has(ObjectProperties::CruiseDifficulty)
                           ? props.integer(ObjectProperties::CruiseDifficulty)
                           : 5;
        if (z.difficulty < 5) z.difficulty = 5;
        if (props.has(ObjectProperties::CruiseCrowd)) z.pleased = props.integer(ObjectProperties::CruiseCrowd);
        if (props.has(ObjectProperties::CruiseMessage) && named) {
            const std::string* message = props.text(ObjectProperties::CruiseMessage);
            for (size_t i = 0; message != nullptr && i < kZoneMessages.size(); ++i) {
                if (sameText(*message, kZoneMessages[i])) {
                    z.name = texts.at(kTextZones + i);
                    break;
                }
            }
        }
    }
    zones.push_back(std::move(z));
}

// 0x004B1510: active, not yet done nor pleased, calm. (It also asks the zone of each scene object's place and keeps
// nothing of it.)
void Cruise::resetZone(CruiseZone& z) {
    z.active = 1;
    z.done = 0;
    z.pleasedOnce = 0;
    z.state = 0;
    z.time = 0.0f;
    z.stateTime = 0.0f;
}

// 0x004B0440: the first zone holding the cruise car's origin.
CruiseZone* Cruise::carZone() {
    const Vec4 at = originCm(*car);
    for (CruiseZone& z : zones) {
        if (z.contains(at)) return &z;
    }
    return nullptr;
}

// 0x004B04A0, a zone's crowd by the race's count (the first car's competitor's race): over 3.5 s calm, over 2 state 1,
// over 0.1 state 3 (each for 0.1 s), over 0 state 4 for 4 s. Then in a cruise with its time run out state 4 for 4 s;
// in another race a finished car in the zone makes it 4 for the winner, else 3, for 5 s. The state's time runs; when
// its hold runs out each car in the zone raises it (never lowering it; a new state from 0 s, held 0.5 s): outside a
// cruise to 3 for a car in its top gear past 95% of its torque curve's end or over 200 km/h, with the nitro asked over
// 100 km/h, or first; in any race to 3 for a car whose nearest ahead is within 8000 along the lap, to 4 within 3000.
// None raising it: calm for 0.5 s.
void Cruise::crowd(CruiseZone& z) {
    const std::vector<RaceCar*>& cars = m_race->cars;
    if (cars.empty() || cars[0] == nullptr) return;
    const RaceCar& first = *cars[0];
    const float count = first.competitor != nullptr && first.competitor->race != nullptr ? first.competitor->race->count
                                                                                         : 0.0f;
    const bool cruise = m_race->type == RaceType::Cruise;
    const auto set = [&](int32_t state, float seconds) {
        if (z.state != state) z.time = 0.0f;
        z.state = state;
        z.stateTime = seconds;
    };
    if (count > 3.5f) return set(0, std::bit_cast<float>(0x3DCCCCCDu));
    if (count > 2.0f) return set(1, std::bit_cast<float>(0x3DCCCCCDu));
    if (count > std::bit_cast<float>(0x3DCCCCCDu)) return set(3, std::bit_cast<float>(0x3DCCCCCDu));
    if (count > 0.0f) return set(4, 4.0f);
    if (cruise) {
        if (time < 0.0f) return set(4, 4.0f);
    } else {
        for (RaceCar* c : cars) {
            if (z.contains(originCm(*c)) && c->competitor->finished) {
                return set(c->competitor->place == 1 ? 4 : 3, 5.0f);
            }
        }
    }
    z.time = m_race->frameTime + z.time;
    z.stateTime = z.stateTime - m_race->frameTime;
    if (z.stateTime > 0.0f) return;
    int32_t level = 0;
    const auto raise = [&](int32_t to) {
        if (z.state <= to) {
            if (z.state != to) z.time = 0.0f;
            z.state = to;
            z.stateTime = 0.5f;
        }
        level = to;
    };
    for (RaceCar* c : cars) {
        if (c == nullptr || !z.contains(originCm(*c))) continue;
        const CarBody& body = *c->body;
        if (!cruise) {
            const float kmh = body.localVelocity[2] * 3.6f;
            const std::vector<float>& curve = body.dynamics().torqueSpeeds;
            const float top = curve.empty() ? 0.0f : curve.back();
            const auto rpm = static_cast<float>(static_cast<double>(body.engineSpeed) * 9.549296379089355);
            const double most = static_cast<double>(top) * 9.549296379089355 * 0.949999988079071;
            if ((most < rpm && body.gear == body.dynamics().forwardRatios.size() + 1) || kmh > 200.0f) raise(3);
            if (c->nitroAsked && kmh > 100.0f) raise(3);
            if (c->competitor->place == 1) raise(3);
        }
        if (const Neighbour* ahead = c->competitor->ahead) {
            if (std::fabs(ahead->gap) < 8000.0f) raise(3);
            if (std::fabs(ahead->gap) < 3000.0f) raise(4);
        }
    }
    if (level == 0) set(0, 0.5f);
}

void Cruise::excite(int32_t level, float seconds) {
    CruiseZone* z = carZone();
    if (z == nullptr || z->state >= level + 1) return;
    if (z->state != level) z->time = 0.0f;
    z->state = level;
    z->stateTime = seconds;
}

// 0x004ABD10 / 0x004ABDC0: the lines move up one place (the first goes); the last a new line of the trick (or of the
// special move), its value 0, going on, shown 2.5 s; the list changed.
void Cruise::shift() {
    for (size_t i = 0; i + 1 < messages.size(); ++i) {
        CruiseMessage& to = messages[i];
        const CruiseMessage& from = messages[i + 1];
        to.bonus = from.bonus;
        to.trick = from.trick;
        to.move = from.move;
        to.name = from.name;
        to.points = from.points;
        to.value = from.value;
        to.active = from.active;
        to.time = from.time;
    }
}

CruiseMessage& Cruise::newTrick(int32_t trick) {
    shift();
    CruiseMessage& m = messages.back();
    m.bonus = 0;
    m.trick = trick;
    m.move = 0;
    m.value = 0.0f;
    m.active = 1;
    m.time = 2.5f;
    changed = 1;
    return m;
}

CruiseMessage& Cruise::newMove(int32_t move) {
    shift();
    CruiseMessage& m = messages.back();
    m.bonus = 0;
    m.trick = 0;
    m.move = move;
    m.value = 0.0f;
    m.active = 1;
    m.time = 2.5f;
    changed = 1;
    return m;
}

// 0x004AC240 (the trick in EBX): outside a cruise the multiplier back to 1. A trick of more than 9 points: the combo
// held 3 s, the trick the newest of the last four (its age 0), the replay's mark (its interest 1, its +0xCB4 the
// trick), the special moves (0x004AC0A0), the crowd's bonus unless the trick's crowd kind is 1, 4 or 15, the
// multiplier up one (to 25) unless the trick is the newest's again (+0x4C 2 s), the idle time 0.
void Cruise::trickDone(int32_t trick, int32_t points) {
    if (m_type != RaceType::Cruise) {
        multiplier = 1;
        return;
    }
    if (points <= 9) return;
    if (net.trick) net.trick(trick);  // 0x004AC25F: 0x1A to all
    const int32_t newest = history[3];
    comboTime = 3.0f;
    for (size_t i = 0; i < 3; ++i) {
        history[i] = history[i + 1];
        ages[i] = ages[i + 1];
    }
    history[3] = trick;
    ages[3] = 0.0f;
    mark(0, trick, 1.0f);
    specialMoves();
    const int32_t kind = kCrowd[static_cast<size_t>(trick)];
    if (kind != kHighSpeed && kind != kRightDonut && kind != kDrift) crowdBonus(trick);
    if (multiplier < 25 && trick != newest) {
        ++multiplier;
        flash = 2.0f;
    }
    idle = 0.0f;
}

// 0x004AC0A0: each special move whose groups the last four tricks match (0 any; the oldest against the window x 4
// .. the newest against it x 1) and that is not the one done last: every move's flag cleared and its set; a new line
// of the move worth its points x the multiplier x the car's factor, over the times it was done (from the ninth, a
// hundredth); the combo held 3 s and gains it, the line shown 2 s and over; the multiplier gains the move's (to 25);
// the replay's mark (its interest 5, its +0xCB5 the move).
void Cruise::specialMoves() {
    for (size_t k = 0; k < kMoves.size(); ++k) {
        const SpecialMove& move = kMoves[k];
        bool fits = true;
        for (int32_t step = 4, i = 0; step > 0; --step, ++i) {
            const int32_t group = move.groups[static_cast<size_t>(i)];
            if (group == 0) continue;
            if (group != kGroup[static_cast<size_t>(history[static_cast<size_t>(i)])] ||
                static_cast<float>(move.window * step) < ages[static_cast<size_t>(i)]) {
                fits = false;
                break;
            }
        }
        if (!fits || g_moveDone[k] != 0) continue;
        g_moveDone = {};
        g_moveDone[k] = 1;
        const auto number = static_cast<int32_t>(k) + 1;
        CruiseMessage& m = newMove(number);
        m.value = static_cast<float>(move.points * multiplier) * carFactor;
        const int32_t times = ++moves[static_cast<size_t>(number)];
        m.value = times < 9 ? m.value / static_cast<float>(times) : m.value * 0.01f;
        comboTime = 3.0f;
        combo = combo + m.value;
        m.time = 2.0f;
        m.active = 0;
        multiplier = std::min(multiplier + move.add, 25);
        mark(1, number, 5.0f);
        if (net.trick) net.trick(number + 100);  // 0x004AC1F3: the move + 100
    }
}

void Cruise::mark(size_t slot, int32_t value, float weight) {
    if (m_replay == nullptr) return;
    m_replay->interest = m_replay->interest + weight;
    m_replay->interestLines = m_replay->interestLines + 1 == 25 ? 24 : m_replay->interestLines + 1;
    m_replay->marks[slot] = static_cast<uint8_t>(value);
}

// 0x004AC560: the zone the car was in (+0x434) of the trick's crowd kind and not yet pleased: done; a bonus line of
// the trick: "BONUS MOVE!" worth the multiplier x 5000 x 0.2 (its points shown), the combo held 3 s, the multiplier up
// 5; a zone that pleases once (+0x118) and is not yet: when every zone is, a bonus line "!!! Crowd Pleaser Super Bonus
// !!!" worth 100000, the multiplier up 25. The multiplier at most 25; the zone forgotten. The replay's interest 10 for
// the bonus.
void Cruise::crowdBonus(int32_t trick) {
    CruiseZone* z = zone;
    if (z == nullptr || z->kind != kCrowd[static_cast<size_t>(trick)] || z->done != 0) return;
    const TextTable& texts = *m_race->texts;
    z->done = 1;
    {
        CruiseMessage& m = newTrick(trick);
        m.bonus = 1;
        m.name = texts.at(kTextBonusMove);
        m.points.clear();
        m.value = static_cast<float>(multiplier * 5000) * 0.2f;
        comboTime = 3.0f;
        m.points = toWide(groupThousands(x87::chop(m.value), m_race->language));
        multiplier = multiplier + 5;
        if (m_replay != nullptr) {
            m_replay->interest = m_replay->interest + 10.0f;
            m_replay->interestLines = m_replay->interestLines + 1 == 25 ? 24 : m_replay->interestLines + 1;
        }
    }
    if (z->pleased != 0 && z->pleasedOnce == 0) {
        z->pleasedOnce = 1;
        bool all = true;
        for (const CruiseZone& other : zones) all = all && other.pleasedOnce != 0;
        if (all) {
            CruiseMessage& m = newTrick(trick);
            m.bonus = 1;
            m.name = texts.at(kTextCrowdBonus);
            m.points.clear();
            m.value = 100000.0f;
            comboTime = 3.0f;
            m.points = toWide(groupThousands(x87::chop(m.value), m_race->language));
            multiplier = multiplier + 25;
        }
    }
    if (multiplier > 25) multiplier = 25;
    zone = nullptr;
}

// 0x004ABA30: each line not a bonus: a special move's name (359 + move), " (n)" after it once done more than once, and
// its points; a trick's name (339 + trick) and, but for a crash, its points: donuts (3 .. 6) after 30 s of them and
// high speed (1) after 30 s of it read "Boring!!" (high speed's points emptied first); a line of neither emptied.
void Cruise::listTexts() {
    const TextTable& texts = *m_race->texts;
    const int language = m_race->language;
    for (CruiseMessage& m : messages) {
        if (m.bonus != 0) continue;
        if (m.trick == 0 && m.move == 0) {
            m.name.clear();
            m.points.clear();
            continue;
        }
        if (m.move != 0) {
            m.name = texts.at(kTextMoves + static_cast<size_t>(m.move));
            m.points.clear();
            const int32_t times = moves[static_cast<size_t>(m.move)];
            if (times > 1) m.name += toWide(std::format("  ({})", times));
            m.points += toWide(groupThousands(x87::chop(m.value), language));
            continue;
        }
        m.name = texts.at(kTextTricks + static_cast<size_t>(m.trick));
        m.points.clear();
        if (m.trick == 17) continue;
        if (m.trick >= 3 && m.trick <= 6 && donutTime > 30.0f) {
            m.name = texts.at(kTextBoring);
            m.points.clear();
        } else if (m.trick == 1 && speedTime > 30.0f) {
            m.name = texts.at(kTextBoring);
            m.points.clear();
        }
        m.points += toWide(groupThousands(x87::chop(m.value), language));
    }
}

// 0x004AB740: the multiplier over 1 falls by one after 20 s without a trick and each 2 s more (+0x4C 1 s); at least 1.
void Cruise::decayMultiplier() {
    if (multiplier == 1) return;
    idle = dt + idle;
    if (idle > 20.0f) {
        decay = dt + decay;
        if (decay <= 2.0f) {
            if (multiplier < 1) multiplier = 1;
            return;
        }
        --multiplier;
        flash = 1.0f;
    }
    decay = 0.0f;
    if (multiplier < 1) multiplier = 1;
}

// 0x004AB7C0: after the quiet time (+0x14, else less the game's frame time) a hit of 1000 or more on the cruise's car:
// over 3000 "Crash 'n Burn!" (the multiplier less 15), over 2000 "Mega Wipeout!" (10), else "Wipeout!" (5), at least 1;
// the blink, the combo, the lines' kinds, the last tricks and every trick's slot cleared, the zone forgotten; the last
// line a crash (17) shown 2.5 s; crashed (+0x430); the zone the car is in excited (state 5, 6 s). The replay's
// interest 5 over 3000 and 2 over 2000; its mark the crash (17 + the kind).
void Cruise::crash(float impulse, const RaceCar* hit, float frameTime) {
    if (pause > 0.0f) {
        pause = pause - frameTime;
        return;
    }
    if (hit != car || impulse < 1000.0f) return;
    int32_t kind = 0;
    if (impulse > 3000.0f) {
        if (m_replay != nullptr) {
            m_replay->interest = m_replay->interest + 5.0f;
            m_replay->interestLines = m_replay->interestLines + 1 == 25 ? 24 : m_replay->interestLines + 1;
        }
        multiplier -= 15;
        kind = 2;
    } else if (impulse > 2000.0f) {
        if (m_replay != nullptr) {
            m_replay->interest = m_replay->interest + 2.0f;
            m_replay->interestLines = m_replay->interestLines + 1 == 25 ? 24 : m_replay->interestLines + 1;
        }
        multiplier -= 10;
        kind = 1;
    } else {
        multiplier -= 5;
    }
    if (multiplier < 1) multiplier = 1;
    flash = 0.0f;
    combo = 0.0f;
    for (CruiseMessage& m : messages) {
        m.bonus = 0;
        m.trick = 0;
        m.move = 0;
    }
    history = {};
    for (CruiseTrick& t : tricks) {
        t.time = 0.0f;
        t.state = TurnState::Idle;
        t.message = nullptr;
    }
    zone = nullptr;
    CruiseMessage& last = messages.back();
    last.value = 0.0f;
    last.time = 2.5f;
    last.move = 0;
    last.trick = 17;
    last.bonus = 1;
    last.name = m_texts != nullptr ? m_texts->at(kTextCrash + static_cast<size_t>(kind)) : std::wstring();
    last.points.clear();
    if (m_replay != nullptr) m_replay->marks[0] = static_cast<uint8_t>(17 + kind);
    crashed = 1;
    if (hit != nullptr) excite(5, 6.0f);
}

void Cruise::timeUp() {
    if (car == nullptr || car->competitor == nullptr) return;
    car->competitor->finished = true;
    car->competitor->progressTimer = 10.0f;
    if (car->damage != nullptr) car->damage->freeze();
}

// 0x004AC780, high speed (1, a cruise only): in the top gear with the engine past 80% of the torque curve's end and
// over 10 m/s ahead, or over 55.6 m/s. Held 0.2 s by the game's frame time it starts a line; while it holds the time
// grows and so do the line and the combo by the multiplier x the car's factor x 1 x the speed x 0.01 (after 30 s of
// high speed they fall instead); after 2.5 s the crowd's bonus; when it stops the trick is done.
void Cruise::highSpeed() {
    if (m_type != RaceType::Cruise) return;
    const CarBody& body = *car->body;
    CruiseTrick& t = tricks[kHighSpeed];
    const float speed = body.localVelocity[2];
    const std::vector<float>& curve = body.dynamics().torqueSpeeds;
    const float top = curve.empty() ? 0.0f : curve.back();
    bool on = static_cast<double>(top) * 7.639437 < static_cast<double>(body.engineSpeed) * 9.549296 &&
              speed > 10.0f && body.gear == body.dynamics().forwardRatios.size() + 1;
    if (speed > 55.555557f) on = true;
    if (t.time <= 0.0f) {
        if (!on) {
            m_speedHold = 0.0f;
            return;
        }
        m_speedHold = m_speedHold + m_race->frameTime;
        if (m_speedHold > 0.2f) {
            t.time = dt + t.time;
            t.message = &newTrick(kHighSpeed);
        }
        return;
    }
    if (!on) {
        t.time = 0.0f;
        t.time2 = 0.0f;
        trickDone(kHighSpeed, x87::chop(t.message->value));
        t.message->active = 0;
        return;
    }
    speedTime = dt + speedTime;
    t.time = t.time + dt;
    const auto add = static_cast<float>(static_cast<double>(static_cast<float>(multiplier)) * carFactor *
                                        kFactor[1] * speed * 0.01);
    if (speedTime > 30.0f) {
        t.message->value = t.message->value - add;
        combo = combo - add;
        return;
    }
    t.message->value = add + t.message->value;
    combo = add + combo;
    if (t.time > 2.5f) crowdBonus(kHighSpeed);
}

// 0x004AC9F0 .. 0x004AD200, the donuts: the wheels down lately (+0x3C8), turning (3 left: under -0.5 rad/s, 4 right
// over 0.5; 5 and 6 the same reversed, the turn's sign the other way), slower than 10 m/s (20 reversed) and in a gear
// (reverse for 5 and 6). Held 2 s it starts a line worth 1; while it holds the donuts' time and the trick's grow and
// the line and the combo by the multiplier x the frame x the trick's time x the car's factor x the trick's factor x 10
// (after 30 s of donuts they fall instead), the zone cheers (3, 2 s); past 3 s the crowd's bonus (of trick 3); when it
// stops the trick is done unless 30 s of donuts are up.
void Cruise::donut(int32_t trick) {
    const CarBody& body = *car->body;
    CruiseTrick& t = tricks[static_cast<size_t>(trick)];
    const float yaw = body.localAngularVelocity[1];
    const bool reversed = trick >= 5;
    const bool left = trick == 3 || trick == 6;
    const bool turning = left ? yaw < -0.5f : yaw > 0.5f;
    const float limit = reversed ? 20.0f : 10.0f;
    const bool gearOk = reversed ? body.gear == 0 : body.gear != 0;
    bool on = false;
    if (ground > 0.0f && turning && speedOf(body.localVelocity) < limit && gearOk) {
        t.hold = dt + t.hold;
        on = t.hold > 2.0f;
    } else {
        t.hold = 0.0f;
    }
    if (t.time <= 0.0f) {
        if (!on) return;
        t.time = dt + t.time;
        t.message = &newTrick(trick);
        t.message->value = 1.0f;
        return;
    }
    if (!on) {
        t.time = 0.0f;
        t.time2 = 0.0f;
        if (donutTime < 30.0f) trickDone(trick, x87::chop(t.message->value));
        t.message->active = 0;
        return;
    }
    donutTime = donutTime + dt;
    t.time = dt + t.time;
    const auto add = static_cast<float>(static_cast<double>(static_cast<float>(multiplier)) * dt * t.time *
                                        carFactor * kFactor[static_cast<size_t>(trick)] * 10.0);
    if (donutTime > 30.0f) {
        t.message->value = t.message->value - add;
        combo = combo - add;
        return;
    }
    excite(3, 2.0f);
    t.message->value = add + t.message->value;
    combo = add + combo;
    if (t.time > 3.0f) crowdBonus(kLeftDonut);
}

// A turn done: its line worth the multiplier x the car's factor x the trick's factor, the trick done, the combo gains
// it, the line not going on, the zone cheers (3, 2 s).
void Cruise::turnDone(int32_t trick) {
    CruiseTrick& t = tricks[static_cast<size_t>(trick)];
    t.state = TurnState::Idle;
    t.hold = 0.0f;
    CruiseMessage& m = newTrick(trick);
    t.message = &m;
    m.value = static_cast<float>(multiplier) * carFactor * kFactor[static_cast<size_t>(trick)];
    trickDone(trick, x87::chop(m.value));
    combo = m.value + combo;
    m.active = 0;
    excite(3, 2.0f);
}

// 0x004AD4B0, the bootleg turn (7): from going straight (under 0.5 rad/s) with the wheel turned (over 0.2) it starts
// with the direction of the move in the world. Under way its time grows; backing over 4 m/s, nearly stopped (under 1
// m/s) without the throttle (under 0.5), or straight again before 2.8 rad/s were reached, end it; the handbrake is
// noted and the fastest turn kept; the zone cheers (1, 1 s). After 3 s it ends (the zone 2, 2 s); else, with the
// handbrake and 2.8 rad/s reached, moving the other way (the direction x 5 and x -5 cut equal in x and z) it is done.
void Cruise::bootleg() {
    const CarBody& body = *car->body;
    CruiseTrick& t = tricks[kBootlegTurn];
    Turn& s = m_turns[7];
    if (t.state == TurnState::Idle) {
        if (std::fabs(body.localAngularVelocity[1]) < 0.5f && std::fabs(m_race->steering) > 0.2f) {
            t.state = TurnState::UnderWay;
            s.start = normalised(body.velocity);
            t.hold = 0.0f;
            s.handbrake = false;
            s.most = 0.0f;
        }
        return;
    }
    if (t.state != TurnState::UnderWay) return;
    t.hold = dt + t.hold;
    if (body.localVelocity[2] < -4.0f) t.state = TurnState::Idle;
    if (speedOf(body.localVelocity) < 1.0f && body.throttle < 0.5f) t.state = TurnState::Idle;
    const float yaw = std::fabs(body.localAngularVelocity[1]);
    if (yaw < 0.2f && s.most < 2.8f) t.state = TurnState::Idle;
    if (body.handbrake != 0.0f) s.handbrake = true;
    if (s.most < yaw) s.most = yaw;
    const Vec4 now = normalised(body.velocity);
    excite(1, 1.0f);
    if (3.0f <= t.hold) {
        t.state = TurnState::Idle;
        excite(2, 2.0f);
        return;
    }
    if (s.handbrake && s.most > 2.8f && chopTimes(s.start[0], 5.0) == chopTimes(now[0], -5.0) &&
        chopTimes(s.start[2], 5.0) == chopTimes(now[2], -5.0)) {
        turnDone(kBootlegTurn);
    }
}

// 0x004AD960 / 0x004ADE70 / 0x004AECE0, the 360s (8 right, 9 left) and the boomerang (12): from going straight with
// the wheel turned over 20 m/s ahead, with the direction in the world. Under way: under 2 m/s it ends (the boomerang
// also out of reverse... in reverse gear); backing over 2 m/s noted, the handbrake noted, the turn's highest and lowest
// kept; not having backed, straight after 0.5 s ends it; the zone cheers (1, 1 s). After 4 s it ends (the zone 2, 2 s);
// else after 1 s, turning under 1.5 rad/s, over 10 m/s ahead, having backed with the handbrake and turned the right way
// (8: never under -1; 9: never over 1; 12: under -2 and over 2), going the same way again (x 2 cut equal in x and z) it
// is done.
void Cruise::spin(int32_t trick) {
    const CarBody& body = *car->body;
    CruiseTrick& t = tricks[static_cast<size_t>(trick)];
    Turn& s = m_turns[static_cast<size_t>(trick)];
    if (t.state == TurnState::Idle) {
        if (std::fabs(body.localAngularVelocity[1]) < 0.5f && std::fabs(m_race->steering) > 0.2f &&
            body.localVelocity[2] > 20.0f) {
            t.state = TurnState::UnderWay;
            s.start = normalised(body.velocity);
            t.hold = 0.0f;
            s.handbrake = false;
            s.reversed = false;
            s.most = -10.0f;
            s.least = 10.0f;
        }
        return;
    }
    if (t.state != TurnState::UnderWay) return;
    t.hold = dt + t.hold;
    if (speedOf(body.localVelocity) < 2.0f) t.state = TurnState::Idle;
    if (trick == 12 && body.gear == 0) t.state = TurnState::Idle;
    if (body.localVelocity[2] < -2.0f) s.reversed = true;
    if (body.handbrake != 0.0f) s.handbrake = true;
    const float yaw = body.localAngularVelocity[1];
    if (s.most < yaw) s.most = yaw;
    if (yaw < s.least) s.least = yaw;
    if (!s.reversed && std::fabs(yaw) < 0.5f && t.hold > 0.5f) t.state = TurnState::Idle;
    const Vec4 now = normalised(body.velocity);
    excite(1, 1.0f);
    if (4.0f <= t.hold) {
        t.state = TurnState::Idle;
        excite(2, 2.0f);
        return;
    }
    if (!(t.hold > 1.0f) || !(std::fabs(body.localAngularVelocity[1]) < 1.5f)) return;
    if (!(body.localVelocity[2] > 10.0f) || !s.reversed || !s.handbrake) return;
    const bool way = trick == 8 ? s.least > -1.0f : trick == 9 ? s.most < 1.0f : s.least < -2.0f && s.most > 2.0f;
    if (way && chopTimes(s.start[0], 2.0) == chopTimes(now[0], 2.0) &&
        chopTimes(s.start[2], 2.0) == chopTimes(now[2], 2.0)) {
        turnDone(trick);
    }
}

// 0x004AE380 / 0x004AE830, the 180s (10 right, 11 left): from going straight with the wheel turned over 10 m/s ahead,
// with the direction in the car's frame. Under way: under 1 m/s it ends; turning less than 0.2 the right way before
// 1.8 rad/s were reached ends it; the handbrake noted, the fastest turn kept (10: its size over the highest; 11: the
// lowest); the zone cheers (1, 1 s). After 2.5 s it ends (the zone 2, 2 s); else, straight (under 0.5) in reverse gear,
// backing over 10 m/s with the handbrake and 1.8 rad/s reached, moving the other way (x 10 and x -10 cut equal) it is
// done.
void Cruise::half(int32_t trick) {
    const CarBody& body = *car->body;
    CruiseTrick& t = tricks[static_cast<size_t>(trick)];
    Turn& s = m_turns[static_cast<size_t>(trick)];
    const bool right = trick == 10;
    if (t.state == TurnState::Idle) {
        if (std::fabs(body.localAngularVelocity[1]) < 0.5f && std::fabs(m_race->steering) > 0.2f &&
            body.localVelocity[2] > 10.0f) {
            t.state = TurnState::UnderWay;
            s.start = normalised(body.localVelocity);
            t.hold = 0.0f;
            s.handbrake = false;
            s.most = 0.0f;
        }
        return;
    }
    if (t.state != TurnState::UnderWay) return;
    t.hold = dt + t.hold;
    if (speedOf(body.localVelocity) < 1.0f) t.state = TurnState::Idle;
    const float yaw = body.localAngularVelocity[1];
    if (right ? (yaw < 0.2f && s.most < 1.8f) : (yaw > -0.2f && s.most > -1.8f)) t.state = TurnState::Idle;
    if (body.handbrake != 0.0f) s.handbrake = true;
    if (right) {
        if (s.most < yaw) s.most = std::fabs(yaw);
    } else if (yaw < s.most) {
        s.most = yaw;
    }
    const Vec4 now = normalised(body.localVelocity);
    excite(1, 1.0f);
    if (2.5f <= t.hold) {
        t.state = TurnState::Idle;
        excite(2, 2.0f);
        return;
    }
    if (!(std::fabs(body.localAngularVelocity[1]) < 0.5f) || body.gear != 0) return;
    const bool reached = right ? 1.8f < s.most : s.most < -1.8f;
    if (body.localVelocity[2] < -10.0f && s.handbrake && reached &&
        chopTimes(s.start[0], 10.0) == chopTimes(now[0], -10.0) &&
        chopTimes(s.start[2], 10.0) == chopTimes(now[2], -10.0)) {
        turnDone(trick);
    }
}

// 0x004AF220 / 0x004AF690, the J-turns (13 right, 14 left): with the boomerang not under way, from going straight with
// the wheel turned backing over 10 m/s, with the direction in the car's frame. Under way: under 1 m/s it ends; turning
// less than 0.2 the right way before 1 rad/s (13) or 1.8 (14) were reached ends it; the fastest turn kept; the zone
// cheers (1, 1 s). After 4.5 s it ends (the zone 2, 2 s); else, straight over 10 m/s ahead with 1 rad/s reached the
// right way, moving the other way (x 10 and x -10 cut equal) it is done.
void Cruise::jTurn(int32_t trick) {
    const CarBody& body = *car->body;
    CruiseTrick& t = tricks[static_cast<size_t>(trick)];
    Turn& s = m_turns[static_cast<size_t>(trick)];
    const bool right = trick == 13;
    if (t.state == TurnState::Idle) {
        if (std::fabs(body.localAngularVelocity[1]) < 0.5f && std::fabs(m_race->steering) > 0.2f &&
            body.localVelocity[2] < -10.0f && tricks[kBoomerang].state == TurnState::Idle) {
            t.state = TurnState::UnderWay;
            s.start = normalised(body.localVelocity);
            t.hold = 0.0f;
            s.most = 0.0f;
        }
        return;
    }
    if (t.state != TurnState::UnderWay) return;
    t.hold = dt + t.hold;
    if (speedOf(body.localVelocity) < 1.0f) t.state = TurnState::Idle;
    const float yaw = body.localAngularVelocity[1];
    if (right ? (yaw < 0.2f && s.most < 1.0f) : (yaw > -0.2f && s.most > -1.8f)) t.state = TurnState::Idle;
    if (right) {
        if (s.most < yaw) s.most = std::fabs(yaw);
    } else if (yaw < s.most) {
        s.most = yaw;
    }
    const Vec4 now = normalised(body.localVelocity);
    excite(1, 1.0f);
    if (4.5f <= t.hold) {
        t.state = TurnState::Idle;
        excite(2, 2.0f);
        return;
    }
    const bool reached = right ? 1.0f < s.most : s.most < -1.0f;
    if (std::fabs(body.localAngularVelocity[1]) < 0.5f && body.localVelocity[2] > 10.0f && reached &&
        chopTimes(s.start[0], 10.0) == chopTimes(now[0], -10.0) &&
        chopTimes(s.start[2], 10.0) == chopTimes(now[2], -10.0)) {
        turnDone(trick);
    }
}

// 0x004AFB00 / 0x004AFE30, the drift (15, a cruise only, not during high speed) and the nitrous drift (16): over 20 m/s
// in the world, turning under 2 rad/s, without the handbrake, moving ahead with the sideways speed over a tenth of the
// ahead speed (the slide) and less than it, the nitro off (15) or asked (16). Not sliding after a slide, under 15 m/s a
// line going on is worth nothing. A slide held 0.5 s starts a line (the drift's worth 10); while sliding the line and
// the combo gain the multiplier x the car's factor x the trick's factor x the slide, the zone cheers (3, 2 s) and (15)
// the crowd's bonus; 2 s without sliding the trick is done.
void Cruise::drift(bool nitro) {
    const int32_t trick = nitro ? 16 : 15;
    if (!nitro && (m_type != RaceType::Cruise || tricks[kHighSpeed].time > 0.0f)) return;
    const CarBody& body = *car->body;
    CruiseTrick& t = tricks[static_cast<size_t>(trick)];
    bool& was = nitro ? m_nitroDrift : m_drift;
    const float side = body.localVelocity[0];
    const float ahead = body.localVelocity[2];
    const float yaw = body.localAngularVelocity[1];
    const float speed = speedOf(body.velocity);
    bool on = false;
    float slide = 0.0f;
    if (speed > 20.0f && std::fabs(yaw) < 2.0f && body.handbrake == 0.0f && ahead > 0.0f &&
        std::fabs(ahead) != 0.0f) {
        slide = std::fabs(side) / std::fabs(ahead);
        on = slide > 0.1f && car->nitroAsked == nitro && std::fabs(side) < ahead;
    }
    if (!on && was && speed < 15.0f && t.message != nullptr) t.message->value = 0.0f;
    if (t.time <= 0.0f) {
        if (on) {
            if (t.hold > 0.5f) {
                t.hold = 0.0f;
                t.time = dt + t.time;
                t.message = &newTrick(trick);
                was = on;
                if (!nitro) t.message->value = 10.0f;
                return;
            }
            t.hold = dt + t.hold;
        }
        was = on;
        return;
    }
    if (on) {
        t.hold = 0.0f;
        t.time = dt + t.time;
        const auto add = static_cast<float>(static_cast<double>(static_cast<float>(multiplier)) * carFactor *
                                            kFactor[static_cast<size_t>(trick)] * slide);
        t.message->value = add + t.message->value;
        combo = add + combo;
        excite(3, 2.0f);
        if (!nitro) crowdBonus(kDrift);
        was = on;
        return;
    }
    t.hold = dt + t.hold;
    if (t.hold > 2.0f) {
        t.time = 0.0f;
        t.time2 = 0.0f;
        t.hold = 0.0f;
        trickDone(trick, x87::chop(t.message->value));
        t.message->active = 0;
    }
    was = on;
}

// 0x004AAAB0. While the race is paused (off the network) the quiet time 0.25 and nothing more. The zones' crowds
// (0x004B04A0) and the zone (+0x434, +0x438): the last active zone not yet done holding the car's origin of the frame
// before. Outside a cruise the time stays 10 s; with no time left nothing more. The first frame takes the player's car
// (else the race's first) and the car's factor (its value x 0.00005 within 1 .. 3, times 1 less its damage, at least
// 0.3; 1 outside a cruise) and in a cruise of the arcade or the career tells the player "TARGET SCORE <target>" (8 s /
// 5 s), then "CAR MULTIPLIER x<factor>" (5 s). Before the start (the race's count over 0) the time stands (the
// network's lines go on); else the frame's time, the time less it and, run out, the bank and the combo go into the
// score. The car's origin kept; the wheels' touch time and the tricks' ages run; the music's volume follows the zone
// the car is in; the tricks (15), the list's texts, the multiplier's fall; a combo going on pays the bank into the
// score at once; the bank pays 5% of itself into the score; the sounds' time runs down; the score at least 0. In a
// cruise the warnings each second of the last 10, the count and swoosh sounds (raceSounds); the combo's hold runs out
// into the bank; at the multiplier's 25 once "MAX COMBO MULTIPLIER" (1915, 2.5 s).
void Cruise::frame(const CruiseRace& race, std::vector<std::pair<std::wstring, float>>& out) {
    m_race = &race;
    m_texts = race.texts;
    m_replay = race.replay;
    m_type = race.type;
    if (race.paused && !race.networked) {
        pause = 0.25f;
        return;
    }
    zone = nullptr;
    lastZone = nullptr;
    for (CruiseZone& z : zones) {
        crowd(z);
        if (z.active != 0 && z.done == 0 && z.contains(g_carOrigin)) {
            lastZone = &z;
            zone = &z;
        }
    }
    if (race.type != RaceType::Cruise) time = 10.0f;
    if (time <= 0.0f) return;
    const TextTable& texts = *race.texts;
    if (car == nullptr) {
        player = race.player;
        car = race.car;
        if (car == nullptr) return;
        carFactor = static_cast<float>(static_cast<float>(race.carValue) * 5e-05);
        if (carFactor < 1.0f) carFactor = 1.0f;
        if (3.0f < carFactor) carFactor = 3.0f;
        carFactor = static_cast<float>((1.0 - race.carDamage) * carFactor);
        if (carFactor < 0.3f) carFactor = 0.3f;
        if (race.type == RaceType::Cruise) {
            if (race.mode == RaceMode::Arcade || race.mode == RaceMode::Career) {
                // "TARGET SCORE" (777), " " and the target with its thousands marked ([0x00768A44]).
                out.emplace_back(texts.at(kTextTargetScore) + L" " + toWide(groupThousands(race.target, race.language)),
                                 race.mode == RaceMode::Arcade ? 8.0f : 5.0f);
            }
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), " x%.1f", static_cast<double>(carFactor));
            out.emplace_back(texts.at(kTextCarMultiplier) + toWide(buffer), 5.0f);
        } else {
            carFactor = 1.0f;
        }
    }
    changed = 0;
    const float count = car->competitor != nullptr && car->competitor->race != nullptr ? car->competitor->race->count
                                                                                       : 0.0f;
    if (count <= 0.0f) {
        dt = race.frameTime;
        time = time - race.frameTime;
        if (time < 0.0f) {
            const float banked = bank;
            comboShown = 0.0f;
            const float held = combo;
            combo = 0.0f;
            bank = banked + held;
            total = banked + held + total;
        }
        // 0x004AAC0E: a network cruise run out ends (Stock problem: again each frame after, as nothing stops it).
        if (race.networked && race.type == RaceType::Cruise && time < 0.0f && net.end) net.end();
    } else if (race.networked && net.board) {
        net.board();
    }
    g_carOrigin = originCm(*car);
    ground = ground - dt;
    for (float& age : ages) age = dt + age;
    {
        const CruiseZone* z = carZone();
        float factor = 1.0f;
        if (z != nullptr && z->time < 4.0f && z->state >= 3 && z->state <= 5) factor = 0.9f;
        volume = (factor * volumeTarget - volume) * 0.2f + volume;
        volume = std::clamp(volume, 0.0f, 1.0f);
    }
    if (race.type == RaceType::Cruise) {
        if (RaceSounds* sounds = raceSounds()) sounds->cruiseMusic(volume);
    }
    highSpeed();
    donut(kLeftDonut);
    donut(kRightDonut);
    donut(kLeftDonutReverse);
    donut(kRightDonutReverse);
    bootleg();
    spin(kRight360);
    spin(kLeft360);
    half(kRight180);
    half(kLeft180);
    spin(kBoomerang);
    jTurn(kRightJTurn);
    jTurn(kLeftJTurn);
    drift(false);
    drift(true);
    listTexts();
    decayMultiplier();
    if (0.0f < combo && 0.0f < bank) {
        const float b = bank;
        bank = 0.0f;
        total = total + b;
    }
    total = bank * 0.05f + total;
    bank = bank - bank * 0.05f;
    soundTime = soundTime - race.frameTime;
    if (total < 0.0f) total = 0.0f;
    if (race.type == RaceType::Cruise) {
        // The race front end's sounds while the race is not paused (0x005A8860): warning, count, swoosh.
        RaceSounds* sounds = raceSounds();
        if (time < warning) {
            if (sounds != nullptr) sounds->raceInterface("warning");
            warning = warning - 1.0f;
        }
        if (x87::chop(scoreShown) != x87::chop(total) && soundTime < 0.0f) {
            scoreShown = total;
            if (sounds != nullptr) sounds->raceInterface("count");
            soundTime = 0.05f;
        }
        if (x87::chop(combo) <= x87::chop(comboShown) || 0.0f <= soundTime || comboShown == 0.0f) {
            comboShown = combo;
        } else {
            comboShown = combo;
            if (sounds != nullptr) sounds->raceInterface("swoosh");
            soundTime = 0.05f;
        }
    }
    // 0x004AAEE7 on the network: the frame's time (within 0 .. 0.04) counted; past 2 s, or under 0.25 s of time
    // left, our score sent and the board again.
    if (race.networked) {
        m_netTimer = std::clamp(dt, 0.0f, 0.04f) + m_netTimer;
        if (2.0f < m_netTimer || time < 0.25f) {
            m_netTimer = 0.0f;
            if (net.score) net.score(x87::nearest(total));
            if (net.board) net.board();
        }
    }
    if (0.0f < comboTime) {
        comboTime = comboTime - dt;
        if (comboTime <= 0.0f) {
            const float held = combo;
            combo = 0.0f;
            bank = bank + held;
        }
    }
    if (race.type == RaceType::Cruise) {
        if (multiplier == 25) {
            if (m_maxComboShown) {
                out.emplace_back(texts.at(kTextMaxCombo), 2.5f);
                m_maxComboShown = false;
            }
        } else {
            m_maxComboShown = true;
        }
    }
}

}  // namespace oj
