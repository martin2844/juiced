#include "Core/Network/OnlineSessions.h"

#include "Core/Network/GameSpy/GameSpyUser.h"

#include "Core/Cars/CarDefs.h"
#include "Core/Career/Career.h"
#include "Core/Career/CareerEvents.h"
#include "Core/Saves/SaveEntries.h"

#include <cstring>
#include <format>

namespace oj::net {

namespace {

constexpr uint32_t kArcadeSession = 0x21C;  // the profile's +0x270
constexpr uint32_t kArcadeSearch = 0x254;   // +0x2A8
constexpr uint32_t kCareerSession = 0x268;  // +0x2BC
constexpr uint32_t kCareerSearch = 0x2B0;   // +0x304

int32_t get(const save::Settings& s, uint32_t offset) {
    return static_cast<int32_t>(s.u32(offset));
}

void put(save::Settings& s, uint32_t offset, int32_t value) {
    s.setU32(offset, static_cast<uint32_t>(value));
}

std::array<char16_t, 16> getName(const save::Settings& s, uint32_t offset) {
    std::array<char16_t, 16> name{};
    std::memcpy(name.data(), s.object.data() + offset, sizeof name);
    return name;
}

void putName(save::Settings& s, uint32_t offset, const std::array<char16_t, 16>& name) {
    std::memcpy(s.object.data() + offset, name.data(), sizeof name);
}

MatchAttribute intAttribute(int32_t id, int32_t value) {
    MatchAttribute a;
    a.id = id;
    a.type = 1;
    a.value = value;
    return a;
}

// The name's record ({0, 2, 0x10, 1, the name to its 0}, 0x32 bytes).
MatchAttribute nameAttribute(const std::array<char16_t, 16>& name) {
    MatchAttribute a;
    a.id = 0;
    a.type = 2;
    a.length = 0x10;
    a.text = nameText(name);
    return a;
}

SearchAttribute searchAttribute(int32_t id, int32_t type, int32_t length) {
    return {id, type, length, 0};
}

// Host with the attributes' records (0x005A3EB0 each, freed after the call, 0x005A40A0).
bool hostWith(Matchmaking& mm, uint32_t privateFlag, uint32_t players, uint32_t open,
              const std::vector<MatchAttribute>& attributes) {
    std::vector<const MatchAttribute*> records;
    for (const MatchAttribute& a : attributes) records.push_back(&a);
    return mm.host(privateFlag, players, open, static_cast<uint32_t>(records.size()), records.data());
}

void startSearch(const std::string& filter, const std::vector<SearchAttribute>& attributes, bool quick,
                 int32_t kind) {
    Matchmaking* mm = matchmaking();
    // The stock windows always have the matchmaking (made by online_main's enter); none here: no search.
    if (mm == nullptr) return;
    mm->setFilter(filter);
    mm->setSearchAttributes(quick ? 5 : 25, static_cast<uint32_t>(attributes.size()), attributes.data());
    // The values of the search (0x14-byte records, a 0x10 one for an Any) are not used by the GameSpy matchmaking.
    mm->startSearch(kind, static_cast<int32_t>(attributes.size()), 0);
}

const SearchResult* resultAt(int32_t position) {
    const Matchmaking* mm = matchmaking();
    if (mm == nullptr || position < 0) return nullptr;
    return mm->sortedResult(static_cast<uint32_t>(position));
}

// 0x00564060 then the value at +0x10 into the field, which is kept without the attribute.
void readInt(const SearchResult& r, int32_t id, int32_t& field) {
    if (const MatchAttribute* a = findAttribute(r, id)) field = a->value;
}

const std::vector<Car>& crewCars(const Career& career) {
    // The player's crew ([career + 8], its +0x78 0): the garage, else the opponents' cars.
    return career.crews[0].id == 0 ? career.garage : career.opponentCars;
}

int32_t teamCount(int32_t team) {
    return team > 100 ? 1 : team;
}

}  // namespace

// ---- The settings.

ArcadeSession ArcadeSession::read(const save::Settings& s) {
    ArcadeSession a;
    a.access = get(s, kArcadeSession + 0x00);
    a.name = getName(s, kArcadeSession + 0x04);
    a.playing = get(s, kArcadeSession + 0x24);
    a.career = get(s, kArcadeSession + 0x28);
    a.level = get(s, kArcadeSession + 0x2C);
    a.mods = get(s, kArcadeSession + 0x30);
    a.collision = get(s, kArcadeSession + 0x34);
    return a;
}

void ArcadeSession::write(save::Settings& s) const {
    put(s, kArcadeSession + 0x00, access);
    putName(s, kArcadeSession + 0x04, name);
    put(s, kArcadeSession + 0x24, playing);
    put(s, kArcadeSession + 0x28, career);
    put(s, kArcadeSession + 0x2C, level);
    put(s, kArcadeSession + 0x30, mods);
    put(s, kArcadeSession + 0x34, collision);
}

CareerSession CareerSession::read(const save::Settings& s) {
    CareerSession c;
    c.access = get(s, kCareerSession + 0x00);
    c.name = getName(s, kCareerSession + 0x04);
    c.playing = get(s, kCareerSession + 0x24);
    c.career = get(s, kCareerSession + 0x28);
    c.team = get(s, kCareerSession + 0x2C);
    c.lower = get(s, kCareerSession + 0x30);
    c.upper = get(s, kCareerSession + 0x34);
    c.criteriaType = get(s, kCareerSession + 0x38);
    c.criteria = get(s, kCareerSession + 0x3C);
    c.pinkslip = get(s, kCareerSession + 0x40);
    c.collision = get(s, kCareerSession + 0x44);
    return c;
}

void CareerSession::write(save::Settings& s) const {
    put(s, kCareerSession + 0x00, access);
    putName(s, kCareerSession + 0x04, name);
    put(s, kCareerSession + 0x24, playing);
    put(s, kCareerSession + 0x28, career);
    put(s, kCareerSession + 0x2C, team);
    put(s, kCareerSession + 0x30, lower);
    put(s, kCareerSession + 0x34, upper);
    put(s, kCareerSession + 0x38, criteriaType);
    put(s, kCareerSession + 0x3C, criteria);
    put(s, kCareerSession + 0x40, pinkslip);
    put(s, kCareerSession + 0x44, collision);
}

ArcadeSearch ArcadeSearch::read(const save::Settings& s) {
    ArcadeSearch a;
    a.minPlayers = get(s, kArcadeSearch + 0x00);
    a.status = get(s, kArcadeSearch + 0x04);
    a.level = get(s, kArcadeSearch + 0x08);
    a.mods = get(s, kArcadeSearch + 0x0C);
    a.collision = get(s, kArcadeSearch + 0x10);
    return a;
}

void ArcadeSearch::write(save::Settings& s) const {
    put(s, kArcadeSearch + 0x00, minPlayers);
    put(s, kArcadeSearch + 0x04, status);
    put(s, kArcadeSearch + 0x08, level);
    put(s, kArcadeSearch + 0x0C, mods);
    put(s, kArcadeSearch + 0x10, collision);
}

CareerSearch CareerSearch::read(const save::Settings& s) {
    CareerSearch c;
    c.minPlayers = get(s, kCareerSearch + 0x00);
    c.status = get(s, kCareerSearch + 0x04);
    c.team = get(s, kCareerSearch + 0x08);
    c.lower = get(s, kCareerSearch + 0x0C);
    c.upper = get(s, kCareerSearch + 0x10);
    c.criteriaType = get(s, kCareerSearch + 0x14);
    c.criteria = get(s, kCareerSearch + 0x18);
    c.pinkslip = get(s, kCareerSearch + 0x1C);
    c.collision = get(s, kCareerSearch + 0x20);
    return c;
}

void CareerSearch::write(save::Settings& s) const {
    put(s, kCareerSearch + 0x00, minPlayers);
    put(s, kCareerSearch + 0x04, status);
    put(s, kCareerSearch + 0x08, team);
    put(s, kCareerSearch + 0x0C, lower);
    put(s, kCareerSearch + 0x10, upper);
    put(s, kCareerSearch + 0x14, criteriaType);
    put(s, kCareerSearch + 0x18, criteria);
    put(s, kCareerSearch + 0x1C, pinkslip);
    put(s, kCareerSearch + 0x20, collision);
}

OnlineHosted& onlineHosted() {
    static OnlineHosted hosted;
    return hosted;
}

bool onlineUsersPresent() {
    return onlineUser() != nullptr;
}

std::vector<int32_t> onlineUserIds() {
    const OnlineUser* user = onlineUser();
    return user != nullptr ? user->localPlayers : std::vector<int32_t>{};
}

std::u16string onlineNick(int32_t id) {
    const OnlineUser* user = onlineUser();
    return user != nullptr ? user->playerNick(id) : std::u16string{};
}

void setOnlineUsersState(int32_t state, int32_t isPublic) {
    OnlineUser* user = onlineUser();
    if (user == nullptr) return;
    const std::vector<int32_t> players = user->localPlayers;
    for (int32_t player : players) user->setPlaying(player, state, isPublic);
}

std::u16string nameText(const std::array<char16_t, 16>& name) {
    std::u16string text;
    for (char16_t c : name) {
        if (c == 0) break;
        text.push_back(c);
    }
    return text;
}

std::array<char16_t, 16> nameOf(std::u16string_view text) {
    std::array<char16_t, 16> name{};
    for (size_t i = 0; i < text.size() && i < 15; ++i) name[i] = text[i];
    return name;
}

void classBhp(int32_t index, int32_t& lo, int32_t& hi) {
    switch (index) {
    case 1: lo = 700; hi = 799; break;
    case 2: lo = 600; hi = 699; break;
    case 3: lo = 500; hi = 599; break;
    case 4: lo = 400; hi = 499; break;
    case 5: lo = 300; hi = 399; break;
    case 6: lo = 200; hi = 299; break;
    case 7: lo = 0; hi = 199; break;
    case 8: lo = 0; hi = 1000; break;
    default: lo = 800; hi = 1200; break;
    }
}

// ---- The matchmaking's calls.

// 0x004E27D0's pieces (0x0059C160 / 0x0059C210 into a narrow text).
std::string arcadeFilter(const ArcadeSearch& s, bool quick) {
    std::string f = std::format("key52 = {}", 0);
    f += std::format(" and key56 = {}", 0);
    f += std::format(" and key50 >= {}", s.minPlayers);
    if (quick) f += " and key51 > 0";
    if (s.status != kAny) f += std::format(" and key55 = {}", s.status);
    if (s.level != kAny) f += std::format(" and key58 = {}", s.level);
    if (s.mods != kAny) f += std::format(" and key57 = {}", s.mods);
    if (s.collision != kAny) f += std::format(" and key65 = {}", s.collision);
    return f;
}

// 0x004E3300's: the classes always (the ranges overlapping), the criteria only with its type.
std::string careerFilter(const CareerSearch& s, bool quick) {
    std::string f = std::format("key52 = {}", 0);
    f += std::format(" and key56 = {}", 1);
    f += std::format(" and key50 >= {}", s.minPlayers);
    if (quick) f += " and key51 > 0";
    if (s.status != kAny) f += std::format(" and key55 = {}", s.status);
    f += std::format(" and key62 <= {}", s.lower);
    f += std::format(" and key61 >= {}", s.upper);
    if (s.criteriaType != kAny) {
        f += std::format(" and key63 = {}", s.criteriaType);
        if (s.criteria != kAny) f += std::format(" and key64 = {}", s.criteria);
    }
    if (s.pinkslip != kAny) f += std::format(" and key59 = {}", s.pinkslip);
    if (s.team != kAny) f += std::format(" and key60 = {}", s.team);
    if (s.collision != kAny) f += std::format(" and key65 = {}", s.collision);
    return f;
}

std::vector<MatchAttribute> arcadeAttributes(const ArcadeSession& s) {
    std::vector<MatchAttribute> a = {nameAttribute(s.name)};
    a.push_back(intAttribute(1, 0));
    a.push_back(intAttribute(2, 0));
    a.push_back(intAttribute(3, s.mods));
    a.push_back(intAttribute(4, s.level));
    for (int32_t id = 5; id <= 10; ++id) a.push_back(intAttribute(id, 0));
    a.push_back(intAttribute(11, s.collision));
    return a;
}

std::vector<MatchAttribute> careerAttributes(const CareerSession& s) {
    std::vector<MatchAttribute> a = {nameAttribute(s.name)};
    a.push_back(intAttribute(1, 0));
    a.push_back(intAttribute(2, 1));
    a.push_back(intAttribute(3, 0));
    a.push_back(intAttribute(4, 0));
    a.push_back(intAttribute(5, s.pinkslip));
    a.push_back(intAttribute(6, s.team));
    a.push_back(intAttribute(7, s.lower));
    a.push_back(intAttribute(8, s.upper));
    a.push_back(intAttribute(9, s.criteriaType));
    a.push_back(intAttribute(10, s.criteria));
    a.push_back(intAttribute(11, s.collision));
    return a;
}

std::vector<SearchAttribute> arcadeSearchAttributes() {
    return {searchAttribute(0, 2, 0x10), searchAttribute(1, 1, 0), searchAttribute(2, 1, 0),
            searchAttribute(4, 1, 0),    searchAttribute(3, 1, 0), searchAttribute(11, 1, 0)};
}

std::vector<SearchAttribute> careerSearchAttributes() {
    std::vector<SearchAttribute> a = {searchAttribute(0, 2, 0x10)};
    for (int32_t id : {1, 2, 6, 7, 8, 9, 10, 5, 11}) a.push_back(searchAttribute(id, 1, 0));
    return a;
}

std::vector<SearchAttribute> requeryAttributes() {
    std::vector<SearchAttribute> a = {searchAttribute(0, 2, 0x10)};
    for (int32_t id = 1; id <= 11; ++id) a.push_back(searchAttribute(id, 1, 0));
    return a;
}

bool hostArcade(const ArcadeSession& s) {
    bool hosted = false;
    // The stock uses the matchmaking unchecked (online_main made it); none here: not hosted.
    if (Matchmaking* mm = matchmaking()) {
        const std::vector<MatchAttribute> attributes = arcadeAttributes(s);
        mm->setInfo(nameText(s.name), 0, u"Arcade");
        hosted = hostWith(*mm, static_cast<uint32_t>(s.access), 1, 5, attributes);
    }
    onlineHosted().arcade = s;
    return hosted;
}

bool hostCareer(const CareerSession& s) {
    bool hosted = false;
    if (Matchmaking* mm = matchmaking()) {
        const std::vector<MatchAttribute> attributes = careerAttributes(s);
        mm->setInfo(nameText(s.name), 0, u"Career");
        if (s.pinkslip != 0) {
            hosted = hostWith(*mm, static_cast<uint32_t>(s.access), 1, 1, attributes);
        } else {
            hosted = hostWith(*mm, static_cast<uint32_t>(s.access), static_cast<uint32_t>(s.team),
                              static_cast<uint32_t>(6 - s.team), attributes);
        }
    }
    onlineHosted().career = s;
    return hosted;
}

void searchArcade(const ArcadeSearch& search, bool quick) {
    startSearch(arcadeFilter(search, quick), arcadeSearchAttributes(), quick, 2);
}

void searchCareer(const CareerSearch& search, bool quick) {
    startSearch(careerFilter(search, quick), careerSearchAttributes(), quick, 3);
}

void requery(int32_t position) {
    Matchmaking* mm = matchmaking();
    if (mm == nullptr) return;
    const std::vector<SearchAttribute> attributes = requeryAttributes();
    mm->setSearchAttributes(1, static_cast<uint32_t>(attributes.size()), attributes.data());
    mm->refreshResult(1, static_cast<uint32_t>(position));
}

void searchFriendSession(int32_t entry) {
    Matchmaking* mm = matchmaking();
    if (mm == nullptr) return;
    const std::vector<SearchAttribute> attributes = requeryAttributes();
    mm->setSearchAttributes(1, static_cast<uint32_t>(attributes.size()), attributes.data());
    mm->searchFriend(1, entry);
}

bool readSession(int32_t position, int32_t& access, std::array<char16_t, 16>& name, int32_t& playing,
                 int32_t& career) {
    const SearchResult* r = resultAt(position);
    if (r == nullptr) return false;
    access = static_cast<int32_t>(r->closed);
    if (const MatchAttribute* a = findAttribute(*r, 0)) {
        // At most 16 characters and a 0 after them (Stock problem: after 16 the 0 is the low half of +0x24, the
        // playing flag), then +0x22 0.
        size_t n = 0;
        for (; n < a->text.size() && n < 16 && a->text[n] != 0; ++n) name[n] = a->text[n];
        if (n < 16) {
            name[n] = 0;
        } else {
            playing = static_cast<int32_t>(static_cast<uint32_t>(playing) & 0xFFFF0000u);
        }
        name[15] = 0;
    }
    readInt(*r, 1, playing);
    readInt(*r, 2, career);
    return true;
}

bool readArcade(int32_t position, ArcadeSession& s) {
    readSession(position, s.access, s.name, s.playing, s.career);
    const SearchResult* r = resultAt(position);
    if (r == nullptr) return false;
    readInt(*r, 4, s.level);
    readInt(*r, 3, s.mods);
    readInt(*r, 11, s.collision);
    return true;
}

bool readCareer(int32_t position, CareerSession& s) {
    readSession(position, s.access, s.name, s.playing, s.career);
    const SearchResult* r = resultAt(position);
    if (r == nullptr) return false;
    readInt(*r, 6, s.team);
    readInt(*r, 7, s.lower);
    readInt(*r, 8, s.upper);
    readInt(*r, 9, s.criteriaType);
    readInt(*r, 10, s.criteria);
    readInt(*r, 5, s.pinkslip);
    readInt(*r, 11, s.collision);
    return true;
}

// ---- The options' steps.

int32_t stepLevel(int32_t value, int32_t dir) {
    if (value == kAny) return dir > 0 ? 0 : 7;
    value += dir;
    return value < 0 || value > 7 ? kAny : value;
}

int32_t stepCreateMods(int32_t value, int32_t dir) {
    value += dir;
    if (value == 3) value = 0;
    if (value < 0) value = 2;
    return value;
}

int32_t stepAnyOfTwo(int32_t value, int32_t dir) {
    if (dir < 0) {
        if (value == 0) return kAny;
        if (value == 1) return 0;
        if (value == kAny) return 1;
        return value;
    }
    if (value == 0) return 1;
    if (value == 1) return kAny;
    if (value == kAny) return 0;
    return value;
}

int32_t stepSearchMods(int32_t value, int32_t dir) {
    if (dir < 0) {
        if (value == kAny) return 2;
        if (value == 2) return 1;
        if (value == 1) return 0;
        if (value == 0) return kAny;
        return value;
    }
    if (value == kAny) return 0;
    if (value == 2) return kAny;
    if (value == 0) return 1;
    if (value == 1) return 2;
    return value;
}

int32_t stepCreateTeam(int32_t value, int32_t dir) {
    value += dir;
    if (value > 3) return 1;
    if (value < 1) return 3;
    return value;
}

int32_t stepCreateCriteria(int32_t type, int32_t value, int32_t dir) {
    switch (type) {
    case 1:
        value += dir;
        if (value > 3) return 1;
        if (value < 1) return 3;
        return value;
    case 2:
        value += dir;
        if (value == 18) return 1;
        if (value == 0) return 17;
        return value;
    case 3:
        value += dir;
        if (value == 53) return 1;
        if (value == 0) return 52;
        return value;
    default:
        return value;
    }
}

int32_t stepSearchTeam(int32_t value, int32_t dir) {
    if (dir < 0) {
        if (value == kAny) return 3;
        if (value == 3) return 2;
        if (value == 2) return 1;
        if (value == 1) return kAny;
        return value;
    }
    if (value == kAny) return 1;
    if (value == 3) return kAny;
    if (value == 1) return 2;
    if (value == 2) return 3;
    return value;
}

int32_t stepSearchCriteriaType(int32_t value, int32_t dir) {
    if (dir < 0) {
        if (value == 3 || value == kAny) return 3;  // Stock problem: 3 stays 3
        if (value == 2) return 1;
        if (value == 1) return 0;
        if (value == 0) return kAny;
        return value;
    }
    if (value == 3) return kAny;
    if (value == kAny) return 0;
    if (value == 2) return 3;
    if (value == 0) return 1;
    if (value == 1) return 2;
    return value;
}

int32_t stepSearchCriteria(int32_t type, int32_t value, int32_t dir) {
    if (type == 0) return value;
    int32_t least = 0;
    int32_t most = 0;
    if (type == 1) {
        least = 1;
        most = 3;
    } else if (type == 2) {
        least = 1;
        most = 17;
    } else if (type == 3) {
        least = 1;
        most = 52;
    }
    if (value == kAny) return dir < 0 ? most : least;
    value += dir;
    return value < least || value > most ? kAny : value;
}

// ---- The garage's checks.

bool garageHasRange(const Career& career, int32_t team, int32_t lower, int32_t upper, int32_t type,
                    int32_t criteria) {
    int32_t n = 0;
    for (const Car& car : career.garage) {
        const int cls = carClass(car);
        if (cls < upper || lower < cls) continue;
        if (type == 1 || type == 2 || type == 3) {
            if (car.def == nullptr) continue;
            const int32_t value = type == 1 ? car.def->region : type == 2 ? car.def->make : car.def->id;
            if (value != criteria) continue;
        }
        ++n;
    }
    return teamCount(team) <= n;
}

bool garageHasCriteria(const Career& career, int32_t team, int32_t criteria, int32_t type) {
    if (type != 1 && type != 2 && type != 3) return true;
    int32_t n = 0;
    for (const Car& car : career.garage) {
        if (car.def == nullptr) continue;
        const int32_t value = type == 1 ? car.def->region : type == 2 ? car.def->make : car.def->id;
        if (value == criteria) ++n;
    }
    return teamCount(team) <= n;
}

int32_t weakestClass(const Career& career) {
    float least = 10000.0f;
    for (const Car& car : crewCars(career)) {
        if (Car::asFloat(car.rating) < least) least = Car::asFloat(car.rating);
    }
    if (least == 10000.0f) least = 10.0f;
    return ratingClass(least);
}

int32_t strongestClass(const Career& career) {
    float most = 0.0f;
    for (const Car& car : crewCars(career)) {
        if (most < Car::asFloat(car.rating)) most = Car::asFloat(car.rating);
    }
    return ratingClass(most);
}

bool careerCheck(const Career& career, int32_t team, int32_t lower, int32_t upper, int32_t type, int32_t criteria,
                 int32_t pinkslip, bool notes, size_t* text) {
    auto fail = [&](size_t line) {
        if (text != nullptr) *text = notes ? line : 0;
        return false;
    };
    if (text != nullptr) *text = 0;
    if (career.garage.empty()) return fail(665);
    if (career.profile.memberCount < teamCount(team)) return fail(661);
    if (!garageHasRange(career, team, lower, upper, type, criteria)) return fail(662);
    if (!garageHasCriteria(career, team, criteria, type)) return fail(type == 1 ? 663 : 664);
    if (pinkslip == 0 || career.garage.size() != 55 || !notes) return true;
    return fail(1614);
}

}  // namespace oj::net
