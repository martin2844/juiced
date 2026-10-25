#include "Core/Race/Racer.h"
#include "Core/Race/RaceSounds.h"

#include "Core/Race/Arcade.h"
#include "Core/Physics/CarBody.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Career/Career.h"
#include "Core/Base/Log.h"
#include "Core/Race/RaceSession.h"
#include "Core/Saves/SaveEntries.h"
#include "Core/Base/X87.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace oj {

uint32_t RaceRules::aiDifficulty() const {
    return settings != nullptr && settings->has_value() ? (*settings)->u32(save::Settings::kDifficulty) : 0;
}

namespace {

float clampTo(float v, float lo, float hi) {  // 0x00407EC0 and the inline clamps: NaN stays
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float sign(float v) {  // 0x00407890
    if (v < 0.0f) return -1.0f;
    if (v > 0.0f) return 1.0f;
    return 0.0f;
}

// The driver's record in the race session ([[ctx+0x80]+0x30] + 0x0C + record x 0x64).
Racer* recordOf(const RaceRules& rules, const RaceCar& car) {
    if (rules.session == nullptr || car.record < 0 || static_cast<size_t>(car.record) >= rules.session->racers.size()) {
        return nullptr;
    }
    return &rules.session->racers[static_cast<size_t>(car.record)];
}

// The first player's competitor ([[[ctx+0x34]+0x0C]]+4 → +0x494); none without a player.
Competitor* playerOf(const RaceRules& rules) {
    return rules.player != nullptr ? rules.player->competitor : nullptr;
}

}  // namespace

// --- The grid's throttle pattern ------------------------------------------------------------------------------

void RevPattern::make(CrtRandom& random, float frameTime) {
    const auto r1 = static_cast<uint32_t>(random.next());
    const auto r2 = static_cast<uint32_t>(random.next());
    const auto r3 = static_cast<uint32_t>(random.next());
    const auto r4 = static_cast<uint32_t>(random.next());
    entries[0] = static_cast<float>(r1 % 10) * 0.05f + 0.5f;
    entries[1] = static_cast<float>(r2 % 10) * 0.025f + 0.1f;
    entries[2] = static_cast<float>(r3 % 10) * 0.04f;
    entries[3] = static_cast<float>(r4 % 10) * 0.025f + 0.1f;
    index = 1;
    timer = entries[3];
    chance = 0.0f;
    step(random, frameTime);
}

void RevPattern::step(CrtRandom& random, float frameTime) {
    if (timer > 0.0f) {
        timer = timer - frameTime;
        return;
    }
    const auto r = static_cast<uint32_t>(random.next());
    if (static_cast<float>(r % 10) * 0.09090909f <= chance) {
        index = 1;
    } else {
        index = 1 - index;
    }
    timer = entries[static_cast<size_t>(index) * 2 + 1];
}

// --- A competitor ---------------------------------------------------------------------------------------------

float Competitor::distance() const {
    if (route->pointToPoint && (next == 0 || next > route->lastPoint || crossings == 0)) return 0.0f;
    return lapDistance;
}

namespace {

// What a competitor's frame works with.
struct Frame {
    RaceState& race;
    RaceRules& rules;
    CrtRandom& random;
    float dt;         // the race's step (the physics manager's +0x74)
    float frameTime;  // the frame's own time ([[ctx+4]+0x1C])
};

// 0x0049B5D0
void reset(Competitor& c, const RaceRules& rules, CrtRandom& random, float frameTime) {
    c.unusedResets = {1.0f, 0.0f, 30.0f};
    c.throttle = 0.0f;
    c.steering = 0.0f;
    c.brake = 0.0f;
    c.handbrake = 0.0f;
    c.nitroTimer = 0.0f;
    c.nitro = false;
    c.throttleFactor = 1.0f;
    c.line = 0;
    c.next = 0;
    c.along = 0.0f;
    c.fraction = 0.0f;
    c.offset = 0.0f;
    c.lapDistance = 0.0f;
    c.lastLapDistance = 0.0f;
    c.raceDistance = 0.0f;
    c.trapPassed = false;
    c.trapNow = false;
    c.bend = 0.0f;
    c.corner = 0;
    c.cornerWarning = 0;
    c.topSpeed = 0.0f;
    c.raceTime = 0.0f;
    c.lapTime = 0.0f;
    if (rules.type != RaceType::Sprint) c.bestLap = 0.0f;
    c.lastPlace = 6;
    c.crossings = 0;
    c.lap = 0;
    c.place = 1;
    c.playerGap = 0.0f;
    c.finishPlace = 10;
    c.previousPlace = 1;
    c.newLap = false;
    c.newBestLap = false;
    c.newRecord = false;
    c.newSector = false;
    c.finished = false;
    c.rollingStart = 1500.0f;
    c.finishRoll = 0.0f;
    c.rollingDelay = 0.0f;
    c.gridSkid = true;
    c.progressTimer = 10.0f;
    c.progress = 0.0f;
    c.stuckTime = 0.0f;
    c.respawnAsked = false;
    c.reverseTime = 0.0f;
    c.lineHold = 0.0f;
    c.magnet = 0.0f;
    c.sector = 0;
    c.mistakeTimer = 0.0f;
    c.mistakeInterval = 240.0f;
    c.trafficTime = 0.0f;
    c.behindTime = 0.0f;
    c.swerveTimer = 0.0f;
    c.swerveSide = 0.0f;
    c.revTimer = 0.0f;
    c.revThrottle = 1.0f;
    c.facingAway = false;
    c.awayTime = 0.0f;
    c.wrongWay = false;
    c.frontContact = false;
    c.sectorBest = {};
    c.sectorDelta = 0.0f;
    c.hornTimer = 0.0f;
    c.car->revs.make(random, frameTime);
}

// 0x0049B880: the race's clocks, the facing away, the timers, the reverse after a contact ahead, the respawn when the
// car makes no progress or has fallen far below its line; a challenge's lap time and speed.
void timers(Competitor& c, Frame& f) {
    const float count = f.race.count;
    if (!c.finished && count == 0.0f) {
        c.raceTime = f.dt + c.raceTime;
        c.lapTime = f.dt + c.lapTime;
        c.totalTime = f.dt + c.totalTime;
    }
    if (c.rollingDelay > 0.0f) {
        c.rollingDelay = c.rollingDelay - f.dt;
        c.rollingDelay = clampTo(c.rollingDelay, 0.0f, 10.0f);
    }
    if (count == 0.0f) {
        if (!c.facingAway) {
            c.awayTime = 0.0f;
            c.wrongWay = false;
        } else {
            c.awayTime = f.dt + c.awayTime;
            if (c.awayTime > 1.5f) c.wrongWay = true;
        }
        if (!f.rules.replay) {
            if (c.lineHold != 0.0f) {
                c.lineHold = c.lineHold - f.dt;
                if (c.lineHold < 0.0f) c.lineHold = 0.0f;
            }
            c.progressTimer = c.progressTimer - f.dt;
            if (c.progressTimer < 0.0f) {
                if (c.progress < 1000.0f) {
                    if (f.rules.type == RaceType::Cruise) {
                        c.respawnAsked = false;
                    } else if (c.route->pointToPoint && c.finished) {
                        c.respawnAsked = false;
                    } else if (f.race.respawnTimer <= 0.0f) {
                        c.respawnAsked = true;
                        f.race.respawnTimer = 5.0f;
                    } else {
                        c.respawnAsked = false;
                    }
                }
                c.progressTimer = 10.0f;
                c.progress = 0.0f;
            }
            c.trafficTime = clampTo(c.trafficTime - f.dt, 0.0f, 10.0f);
            c.behindTime = clampTo(c.behindTime - f.dt, 0.0f, 10.0f);
            c.swerveTimer = clampTo(c.swerveTimer - f.dt, 0.0f, 2.0f);
            if (c.reverseTime == 0.0f) {
                // Contacts ahead add up twice their time (the game object's +0x94 is never set here); past 0.6 the
                // car backs off for 2.7 s.
                if (c.frontContact) {
                    c.stuckTime = f.dt + f.dt + c.stuckTime;
                    if (c.stuckTime > 0.3f + 0.3f) {
                        c.reverseTime = 2.7f;
                        c.stuckTime = 0.0f;
                    }
                    c.frontContact = false;
                } else {
                    c.stuckTime = 0.0f;
                }
            } else if (c.car->contactSide == 2) {
                // Backing into something for 0.3 s ends the reverse.
                c.stuckTime = f.dt + c.stuckTime;
                if (c.stuckTime > 0.3f) {
                    c.reverseTime = 0.0f;
                    c.stuckTime = 0.0f;
                }
            } else {
                c.stuckTime = 0.0f;
            }
            const RoutePoint& at = c.route->line(c.line)[static_cast<size_t>(c.next)];
            if (c.car->node[7] < at.y - 1000.0f && !c.finished) {
                if (f.race.respawnTimer <= 0.0f) {
                    c.respawnAsked = true;
                    f.race.respawnTimer = 5.0f;
                } else {
                    c.respawnAsked = false;
                }
            }
        }
    }
    // A challenge's lap time (types 0 and 1) and speed (type 2): its outcome for the end of the race.
    if ((f.rules.type == RaceType::PerfectLap || f.rules.type == RaceType::TimedLap) && !c.finished &&
        f.rules.lapTime < c.lapTime) {
        f.rules.challenge = ChallengeOutcome::OutOfTime;
        f.race.finish(c);
    }
    if (f.rules.type == RaceType::Speed && !c.finished) {
        const float mph = c.car->body->localVelocity[2] * 2.235f;
        c.trapNow = false;
        if (!c.trapPassed) {
            if (f.rules.speed < mph) {
                c.trapPassed = true;
                c.trapNow = true;
            }
            if (c.raceTime > 10.0f) {
                f.rules.challenge = ChallengeOutcome::SpeedNotReached;
                f.race.finish(c);
            }
        } else if (mph < f.rules.speed) {
            f.rules.challenge = ChallengeOutcome::TooSlow;
            f.race.finish(c);
        }
    }
}

// 0x0049AC50: what `me` knows of a neighbour: when it is reached at the speed the two close at, where it is in this
// car's frame, the gap along the lap (wrapped by half a lap) and how fast that closes.
float record(Neighbour& n, Competitor& me, float dt) {
    const RaceCar& mine = *me.car;
    const RaceCar& theirs = *n.other->car;
    const float dxc = theirs.node[3] - mine.node[3];
    const float dyc = theirs.node[7] - mine.node[7];
    const float dzc = theirs.node[11] - mine.node[11];
    const Vec4& ov = theirs.body->velocity;
    const Vec4& mv = mine.body->velocity;
    const float rvx = mv[0] - ov[0];
    const float rvy = mv[1] - ov[1];
    const float rvz = mv[2] - ov[2];
    const float dx = dxc * 0.01f;
    const float dy = dyc * 0.01f;
    const float dz = dzc * 0.01f;
    const float closing = (rvz * dz + rvy * dy) + rvx * dx;
    n.contactTime = closing > 0.0f ? ((dz * dz + dy * dy) + dx * dx) / closing : 99999.0f;
    n.local = transformPoint(inverseRigid(mine.node), {theirs.node[3], theirs.node[7], theirs.node[11], 0.0f});

    const float old = n.gap;
    const float theirsAlong = n.other->distance();
    const float mineAlong = me.distance();
    n.gap = theirsAlong - mineAlong;
    const float half = me.route->lengths[0] * 0.5f;
    if (half < std::fabs(theirsAlong - mineAlong)) {
        if (me.distance() <= half) {
            n.gap = n.other->distance() - (me.distance() + me.route->lengths[0]);
        } else {
            n.gap = (me.route->lengths[0] + n.other->distance()) - me.distance();
        }
    }
    n.closing = (old - n.gap) / dt;
    n.order = n.gap;
    if (n.gap < -1000.0f) n.order = n.gap + 999999.0f;
    n.sameCrew = theirs.crew == mine.crew;
    return n.gap;
}

// 0x0049B2C0: every neighbour recorded; the nearest ahead and behind; a car of another crew close behind adds to the
// time it presses; the list sorted by order (0x0049F690 with 0x0049B2A0, std::sort: its quicksort past 16, 0x0049F700,
// is not reached by a race's cars; the insertion sort 0x00504A20 keeps equals in their order).
void neighbours(Competitor& c, Frame& f) {
    c.ahead = nullptr;
    c.behind = nullptr;
    float nearest = 1e9f;
    float farthest = -1e9f;
    for (const std::unique_ptr<Neighbour>& n : c.neighbours) {
        const float g = record(*n, c, f.dt);
        if (g > 0.0f) {
            if (g < nearest) {
                nearest = g;
                c.ahead = n.get();
            }
        } else if (g < 0.0f && farthest < g) {
            farthest = g;
            c.behind = n.get();
        }
    }
    if (c.behind != nullptr && !c.behind->sameCrew && -1000.0f < farthest) {
        c.behindTime = f.dt + f.dt + c.behindTime;
    }
    std::stable_sort(c.neighbours.begin(), c.neighbours.end(),
                     [](const std::unique_ptr<Neighbour>& a, const std::unique_ptr<Neighbour>& b) {
                         return a->order < b->order;
                     });
}

// The car's offset across a line at its point (positive to the line's +x side): (p - c) x d in the ground plane.
float across(const Vec4& p, const Vec4& c, const Vec4& d) {
    return (p[0] - c[0]) * d[2] - (p[2] - c[2]) * d[0];
}

// 0x0049B3F0: the line the car follows. For the first 4 s (and for a player's car always) the nearer of FAST and
// ATTACK; then the other line than the first neighbour's (the furthest behind within 10 m, or the nearest ahead)
// when it is ahead within 20 m, held for 3 s; a change is dropped when that car is within 8 m along the lap.
void chooseLine(Competitor& c) {
    if (kHoldCars) {
        c.line = Route::kFast;
        return;
    }
    if (c.raceTime < 4.0f || c.car->kind == CarKind::Player) {
        const int32_t i = c.route->before(c.next);
        const Vec4 p{c.car->node[3], 0.0f, c.car->node[11], 0.0f};
        Vec4 at{}, dir{};
        c.route->curve(Route::kFast, i, c.fraction, at, dir);
        const float fast = across(p, at, dir);
        c.route->curve(Route::kAttack, i, c.fraction, at, dir);
        const float attack = across(p, at, dir);
        c.line = std::fabs(fast) > std::fabs(attack) ? Route::kAttack : Route::kFast;
        return;
    }
    if (c.neighbours.empty()) {
        c.line = 0;  // [0x0079E808], never set
        return;
    }
    const Neighbour& n = *c.neighbours.front();
    const int32_t old = c.line;
    if (n.gap > -1000.0f && n.gap < 2000.0f && c.distance() < n.other->distance()) {
        if (c.lineHold == 0.0f) c.line = n.other->line == 0 ? 1 : 0;
    } else {
        c.line = 0;
        c.lineHold = 0.0f;
    }
    if (old != c.line) {
        if (std::fabs(n.gap) < 800.0f) {
            c.line = old;
        } else {
            c.magnet = 0.0f;
            c.lineHold = 3.0f;
        }
    }
}

// 0x0049BD30 (a player's car): the next corner of the route and the kind of one entered within 2.5 s at this speed.
void cornerWarning(Competitor& c) {
    if (c.car->kind != CarKind::Player) return;
    c.cornerWarning = 0;
    const std::vector<RouteCorner>& corners = c.route->corners;
    if (corners.empty()) return;
    const RouteCorner* k = &corners[static_cast<size_t>(c.corner)];
    float toGo = 0.0f;
    if (c.corner == 0 && k->first < c.next) {
        toGo = (c.route->lengths[0] - c.lapDistance) + k->distance;
    } else {
        toGo = k->distance - c.lapDistance;
    }
    if (toGo > 0.0f) {
        const float time = toGo / (c.car->body->localVelocity[2] * 100.0f);
        if (time > 0.0f && time < 2.5f) c.cornerWarning = k->kind;
    }
    if (c.next > k->middle) {
        c.corner = c.corner + 1;
        if (c.corner >= static_cast<int32_t>(corners.size())) c.corner = 0;
    }
    if (c.next > k->first && c.next < k->middle) c.cornerWarning = k->kind;
    const int32_t back = c.corner - 1;
    if (back > -1 && c.next < corners[static_cast<size_t>(back)].first) c.corner = back;
}

// 0x0049BEF0: the point the car heads for (back one when it is nearer the point before than the next), how far it
// is between them, the sectors (the gaps to the car ahead for a player), the laps, the best lap and the finish, the
// distances along the lap and the race.
void follow(Competitor& c, Frame& f) {
    c.newLap = false;
    c.newBestLap = false;
    c.newRecord = false;
    c.newSector = false;
    const Route& route = *c.route;
    const float px = c.car->node[3];
    const float py = c.car->node[7];
    const float pz = c.car->node[11];
    const bool back = !(route.pointToPoint && c.next == 0);
    const int32_t before = route.before(c.next);
    if (back) {
        const std::vector<RoutePoint>& l = route.line(c.line);
        const RoutePoint& b = l[static_cast<size_t>(before)];
        const float d0x = px - b.x;
        const float d0z = pz - b.z;
        const float behind = (d0z * b.direction[2] + b.direction[1] * 0.0f) + d0x * b.direction[0];
        const RoutePoint& n = l[static_cast<size_t>(c.next)];
        const float ax = px - n.x;
        const float ay = py - n.y;
        const float az = pz - n.z;
        if (behind < 0.0f) {
            const float toNext = std::sqrt((ax * ax + az * az) + ay * ay);
            const float toBefore = std::sqrt(d0x * d0x + d0z * d0z);
            if (toBefore < toNext) {
                c.next = before;
                if (before == 0 && c.crossings > 0 && c.lap == c.crossings) c.crossings = c.crossings - 1;
            }
        }
    }
    {
        const RoutePoint& n = route.line(c.line)[static_cast<size_t>(c.next)];
        c.along = ((pz - n.z) * n.direction[2] + n.direction[1] * 0.0f) + (px - n.x) * n.direction[0];
        c.fraction = (-1.0f / n.length) * c.along;
        c.fraction = clampTo(c.fraction, 0.0f, 1.0f);
        c.fraction = 1.0f - c.fraction;
    }
    if (c.along > 0.0f) {
        c.next = c.next + 1;
        c.fraction = 0.0f;
        if (c.next >= route.count()) c.next = 0;
        RaceState& race = f.race;
        const int32_t s = c.sector;
        if (s < 5 && c.next >= route.sectors[static_cast<size_t>(s)].first &&
            (s == 4 || c.next < route.sectors[static_cast<size_t>(s) + 1].first)) {
            RouteSector& sector = const_cast<RouteSector&>(route.sectors[static_cast<size_t>(s)]);
            std::vector<std::unique_ptr<Competitor>>& all = race.competitors();
            const auto valid = [&](int32_t r) { return r > -1 && r < static_cast<int32_t>(all.size()); };
            if (c.car->kind == CarKind::Player) {
                // A player's car: the gap to the car that entered the sector last when that one is ahead.
                if (valid(sector.racer) && all[static_cast<size_t>(sector.racer)]->place < c.place &&
                    sector.time != 0.0f) {
                    c.gapAhead = c.raceTime - sector.time;
                }
                sector.racer = c.index;
                sector.time = c.raceTime;
            } else {
                // The AI's: the gap behind of a player's car that entered the sector before it, ahead of it.
                if (valid(sector.racer)) {
                    Competitor& earlier = *all[static_cast<size_t>(sector.racer)];
                    if (earlier.car->kind == CarKind::Player && earlier.place < c.place && sector.time != 0.0f) {
                        earlier.gapBehind = c.raceTime - sector.time;
                    }
                }
                sector.time = 0.0f;
                sector.racer = -1;
                const Competitor* player = playerOf(f.rules);
                if (player != nullptr && c.place < player->place) {
                    sector.racer = c.index;
                    sector.time = c.raceTime;
                }
            }
            if (s != 0) {
                c.newSector = true;
                float& best = c.sectorBest[static_cast<size_t>(s)];
                if (best == 0.0f) {
                    best = c.lapTime;
                    c.sectorDelta = 0.0f;
                } else {
                    c.sectorDelta = c.lapTime - best;
                    if (best > c.lapTime) best = c.lapTime;
                }
            }
            c.sector = c.sector + 1;
        }
        if (c.next == 1 || (route.pointToPoint && c.next == route.lastPoint + 1)) {
            if (c.bestLap == 0.0f && c.crossings != 0) {
                c.sectorDelta = 0.0f;
            } else {
                c.sectorDelta = c.lapTime - c.bestLap;
            }
            if (c.crossings == 0) c.sectorDelta = 0.0f;
            c.crossings = c.crossings + 1;
            if (route.pointToPoint || c.sector >= 5 || c.lap == 0) {
                c.lap = c.crossings;
                c.lastLapDistance = 0.0f;
                if (!c.finished && c.crossings > 1) {
                    if (c.lapTime < c.bestLap || c.bestLap == 0.0f) {
                        c.bestLap = c.lapTime;
                        c.newBestLap = true;
                    }
                    if (race.betterLap(c.lapTime)) {
                        c.bestLap = c.lapTime;
                        c.newRecord = true;
                    }
                }
                if (c.crossings > 1) {
                    c.newLap = true;
                    if (c.crossings - 1 < race.laps) c.lapTime = 0.0f;
                }
                c.sector = 1;
            }
            if (c.crossings - 1 == race.laps && !c.finished) race.finish(c);
        }
    }
    c.along = -c.along;
    {
        const std::vector<RoutePoint>& fast = route.lines[0];
        c.lapDistance = fast[static_cast<size_t>(c.next)].length * c.fraction +
                        fast[static_cast<size_t>(route.before(c.next))].distance;
    }
    c.goingBack = c.lapDistance < c.lastLapDistance;
    const float old = c.raceDistance;
    c.raceDistance = c.lapDistance;
    c.lastLapDistance = c.lapDistance;
    if (c.crossings > 0) {
        c.raceDistance = static_cast<float>(c.crossings) * route.lengths[0] + c.lapDistance;
    }
    c.progress = (c.raceDistance - old) + c.progress;
    cornerWarning(c);
}

// 0x0049C590: the point of the car's line `look` centimetres ahead along it and the line's direction there.
void lookAhead(const Competitor& c, float look, Vec4& point, Vec4& dir) {
    const Route& route = *c.route;
    const std::vector<RoutePoint>& l = route.line(c.line);
    int32_t i = route.before(c.next);
    float length = l[static_cast<size_t>(i)].length;
    float gone = (1.0f - c.fraction) * length;
    float t = 1.0f - (gone - look) / length;
    while (gone < look) {
        i = route.after(i);
        length = l[static_cast<size_t>(i)].length;
        t = (look - gone) / length;
        gone = gone + length;
    }
    route.curve(c.line, i, t, point, dir);
}

// 0x0049C720: the steering. Toward the line `look` ahead (pow(speed, 1.8) cm, 50 .. 1750; on a rolling start the
// grid's heading); back onto the line by the offset and how fast it changes; the car moved onto the line while it is
// close and straight; away from cars beside it; against a sliding rear; full lock away from a reverse; times the
// car's SteeringFactor, within ±0.45.
void steer(Competitor& c, Frame& f) {
    const Mat34& m = c.car->node;
    CarBody& body = *c.car->body;
    const float fx = (m[1] * 0.0f + m[0] * 0.0f) + m[2];
    const float fz = (m[9] * 0.0f + m[8] * 0.0f) + m[10];
    const float s = 1.0f / (std::sqrt(fz * fz + fx * fx) + 1.1920929e-7f);
    const float hx = fx * s;
    const float hy = 0.0f * s;
    const float hz = s * fz;
    float look = 50.0f;
    if (body.localVelocity[2] > 0.0f) {
        look = clampTo(x87::pow(body.localVelocity[2], 1.8f) * 1.0f, 50.0f, 1750.0f);
    }
    Vec4 point{}, dir{};
    if (c.rollingStart > 0.0f) {
        const double heading = static_cast<double>(c.route->grid.empty() ? 0.0f : c.route->grid[0].heading);
        const auto cs = static_cast<float>(std::cos(heading));
        const auto sn = static_cast<float>(std::sin(heading));
        dir = {cs * 0.0f + sn, 0.0f, cs - sn * 0.0f, 0.0f};
    } else {
        lookAhead(c, look, point, dir);
    }
    float dot = (hz * dir[2] + dir[1] * hy) + hx * dir[0];
    c.facingAway = dot < 0.3f;
    dot = clampTo(dot, -1.0f, 1.0f);
    float steering = x87::acos(dot);
    c.angle = steering;
    if (hz * dir[0] - hx * dir[2] < 0.0f) steering = -steering;

    if (std::fabs(body.localVelocity[0]) < std::fabs(body.localVelocity[2]) * 0.25f && c.rollingStart == 0.0f) {
        const Vec4 p{m[3], m[7], m[11], 0.0f};
        c.route->curve(c.line, c.route->before(c.next), c.fraction, point, dir);
        const float offset = (p[0] - point[0]) * dir[2] - (p[2] - point[2]) * dir[0];
        // The magnet works for the AI's cars while the players' manager +0x20 is clear (never set here).
        if (c.car->kind == CarKind::Ai) {
            if (body.localVelocity[2] > 10.0f && std::fabs(offset) < 1200.0f && std::fabs(steering) < 1.0f) {
                // The magnet: the car's origin moved toward the line's point by at most 80 cm/s at full speed
                // factor (the speed x 0.025 within 0 .. 1), times the magnet's share.
                const float k = clampTo(body.localVelocity[2] * 0.025f, 0.0f, 1.0f);
                const float most = ((k * 80.0f) * f.dt) * c.magnet;
                Vec4 shift{p[0] - point[0], 0.0f, p[2] - point[2], 0.0f};
                if (length3(shift) > most) {
                    normalize3(shift);
                    shift[0] = shift[0] * most;
                    shift[1] = shift[1] * most;
                    shift[2] = shift[2] * most;
                }
                body.placeAt({(p[0] - shift[0]) * 0.01f, (p[1] - shift[1]) * 0.01f, (p[2] - shift[2]) * 0.01f,
                              0.0f});
                // The node follows at once (0x004F01E0): the competitors after this one see the car moved.
                c.car->node = body.modelMatrix();
                c.magnet = f.dt + c.magnet;
                if (c.magnet > 1.0f) c.magnet = 1.0f;
            } else {
                c.magnet = 0.0f;
            }
        }
        const float previous = c.offset;
        c.offset = offset;
        const float a = clampTo(offset / 1200.0f, -1.0f, 1.0f);
        const float b = clampTo((previous - offset) / 70.0f, -1.0f, 1.0f);
        const float sa = sign(a);
        float correction = -(((a * a) * sa) * 0.7853982f);
        if (sa == sign(b)) correction = ((b * b) * sa) * 0.7853982f + correction;
        const float bendShare = clampTo(2.0f * c.bend, 0.1f, 1.0f);
        const float timeShare = clampTo(c.raceTime * 0.1f, 0.0f, 1.0f);
        steering = timeShare * (correction * bendShare) + steering;

        if (c.reverseTime == 0.0f && !c.neighbours.empty()) {
            // Away from a car beside (±2.5 m across, ±3 m along) that is ahead in the lap; a swerve on top; in a
            // race of type 6 away from one close ahead that is reached in under 2.5 s. Stock problem: the swerve is
            // added for each neighbour (three cars near, three swerves); the fixed build adds it once, where the
            // first neighbour's turn takes it.
            bool swerved = false;
            for (const std::unique_ptr<Neighbour>& np : c.neighbours) {
                const Neighbour& n = *np;
                const float lx = n.local[0];
                const float lz = n.local[2];
                if (-250.0f < lx && lx < 250.0f && 600.0f * -0.5f < lz && lz < 600.0f * 0.5f &&
                    c.distance() < n.other->distance()) {
                    steering = lx < 0.0f ? 0.25f + steering : steering - 0.25f;
                }
                if (c.swerveTimer != 0.0f && !swerved) {
                    steering = c.swerveSide * c.swerveTimer + steering;
                    swerved = true;
                }
                if (f.rules.type == RaceType::Sprint && lz > 600.0f && n.contactTime < 2.5f) {
                    float push = 0.0f;
                    const float k = (1.0f - n.contactTime * 0.5f) * 0.3926991f;
                    const float w = 250.0f + 250.0f;
                    if (-w < lx && lx < 0.0f) {
                        push = (1.0f - -(lx / w)) * k;
                    } else if (lx < w && lx > 0.0f) {
                        push = -((1.0f - lx / w) * k);
                    }
                    if (sign(push) == sign(steering)) {
                        steering = push + steering;
                    } else if (std::fabs(steering) < std::fabs(push) || lz < 600.0f * 4.0f) {
                        steering = push;
                    }
                }
            }
        }
    }
    const float out = ((steering * steering) * sign(steering) + steering) * 0.5f;
    c.steering = out;
    if (c.reverseTime == 0.0f) {
        const RaceCar& car = *c.car;
        const float front = car.wheels[0].slide > car.wheels[1].slide ? car.wheels[0].slide : car.wheels[1].slide;
        const float rear = car.wheels[2].slide > car.wheels[3].slide ? car.wheels[2].slide : car.wheels[3].slide;
        const float d = rear - front;
        if (d > 0.0f) {
            const float e = d * d + d * d;
            c.steering = body.localAngularVelocity[1] > 0.0f ? out - e : out + e;
            c.steering = clampTo(c.steering, -1.0f, 1.0f);
        }
    } else {
        c.reverseTime = c.reverseTime - f.dt;
        if (c.reverseTime < 0.0f) c.reverseTime = 0.0f;
        if (std::fabs(out) < 0.2f) c.reverseTime = 0.0f;
    }
    if (c.reverseTime != 0.0f) {
        c.steering = -c.steering;
        c.steering = c.steering > 0.0f ? 1.0f : -1.0f;
    }
    c.steering = body.dynamics().steeringFactor * c.steering;
    c.steering = clampTo(c.steering, -0.45f, 0.45f);
}

// 0x0049D320: how much the car's line turns within the distance covered in `horizon` seconds: about 10 samples of
// the curve, each turn from the last weighted less with the distance (1 - (d/D)²); √(sum/3), at most 1.
float bendAhead(const Competitor& c, float horizon) {
    const RigidBody& body = *c.car->body;
    const Vec4& lv = body.localVelocity;
    const float speed = std::sqrt((lv[2] * lv[2] + lv[1] * lv[1]) + lv[0] * lv[0]);
    const float reach = speed * horizon * 100.0f;
    const float stepLength = reach / 10.0f;
    const float wx = body.velocity[0];
    const float wz = body.velocity[2];
    const float s = 1.0f / (std::sqrt(wz * wz + wx * wx) + 1.1920929e-7f);
    Vec4 reference{wx * s, 0.0f * s, wz * s, 0.0f};
    const Route& route = *c.route;
    const std::vector<RoutePoint>& l = route.line(c.line);
    int32_t i = route.before(c.next);
    float t = c.fraction;
    float sum = 0.0f;
    float gone = 0.0f;
    int32_t samples = 0;
    if (!(0.0f < reach)) return std::sqrt(sum / 3.0f);
    for (;;) {
        float k = clampTo(stepLength / l[static_cast<size_t>(i)].length, 0.1f, 1.0f);
        if (t < 1.0f) {
            const float inverse = 1.0f / reach;
            for (;;) {
                Vec4 point{}, dir{};
                // 0x004A0FD0 and 0x004A11D0 as 0x004A1370 calls them.
                route.curve(c.line, i, t, point, dir);
                float dot = (dir[2] * reference[2] + dir[1] * reference[1]) + dir[0] * reference[0];
                dot = clampTo(dot, -1.0f, 1.0f);
                const float q = inverse * gone;
                sum = (1.0f - q * q) * x87::acos(dot) + sum;
                gone = k * l[static_cast<size_t>(i)].length + gone;
                if (gone > reach) break;
                t = k + t;
                reference = dir;
                if (!(t < 1.0f)) break;
            }
        }
        t = t - 1.0f;
        i = i + 1;
        if (i > route.count() - 1) i = 0;
        if (route.pointToPoint && i > route.lastPoint) break;
        samples = samples + 1;
        if (static_cast<float>(samples) > 10.0f) break;
        if (!(gone < reach)) break;
    }
    if (sum < 0.0f) return std::sqrt(0.0f / 3.0f);
    if (sum > 3.0f) return std::sqrt(3.0f / 3.0f);
    return std::sqrt(sum / 3.0f);
}

// 0x0049D6F0: the nitro and the bend ahead (kept in +0xA4). A rival on a high level fires it on a straight at full
// throttle beside a car ahead on the attack line, or by chance (0.75 / laps); it keeps enough for the laps left.
float nitroAndBend(Competitor& c, Frame& f, float horizon) {
    const float b = bendAhead(c, horizon);
    RaceCar& car = *c.car;
    CarBody& body = *car.body;
    if (c.nitroTimer > 0.0f) {
        c.nitroTimer = c.nitroTimer - f.dt;
        if (c.nitroTimer < 0.0f) {
            c.nitroTimer = 0.0f;
            if (c.nitro && b < 0.32f && f.rules.type != RaceType::Sprint && car.crew != 0) {
                c.nitroTimer = 2.0f;
            } else {
                c.nitro = false;
            }
        }
    }
    if (!c.nitro && c.nitroTimer == 0.0f) {
        const RaceType type = f.rules.type;
        const float fast = type == RaceType::Sprint ? 5.0f : 10.0f;
        const auto straight = [&]() {
            return b < 0.32f && body.localVelocity[2] > fast && !(c.throttle < 0.9f) &&
                   std::fabs(body.localAngularVelocity[1]) < 0.1f && car.wheelSmoke < 0.1f;
        };
        if (car.crew == 0) {
            if (type == RaceType::Sprint) {
                if (c.level == 2 && straight()) {
                    c.nitroTimer = 0.5f;
                    c.nitro = true;
                } else {
                    c.nitro = false;
                    c.nitroTimer = x87::unit(f.random.next()) * 0.25f + 0.1f;
                }
            } else if (c.level == 2 && straight()) {
                c.nitroTimer = 0.5f;
                c.nitro = true;
            } else {
                c.nitroTimer = 1.0f;
                c.nitro = false;
            }
        } else if ((c.level == 2 || type == RaceType::Sprint) && straight()) {
            if (type == RaceType::Sprint) {
                c.nitroTimer = 0.5f;
                c.nitro = true;
            } else {
                if (!c.neighbours.empty()) {
                    const Neighbour& n = *c.neighbours.front();
                    if (-600.0f < n.gap && n.gap < 600.0f * 3.0f && 250.0f < std::fabs(n.local[0]) &&
                        c.line == 1) {
                        c.nitroTimer = 2.0f;
                        c.nitro = true;
                    }
                }
                const int32_t laps = c.race->laps;
                if (x87::unit(f.random.next()) < 0.75f / static_cast<float>(laps)) {
                    c.nitroTimer = 2.0f;
                    c.nitro = true;
                }
                if (!c.nitro) c.nitroTimer = x87::unit(f.random.next()) * 7.0f;
            }
        } else if (type == RaceType::Sprint) {
            c.nitro = false;
            c.nitroTimer = x87::unit(f.random.next()) * 0.25f + 0.1f;
        } else {
            c.nitroTimer = 5.0f;
            c.nitro = false;
        }
    }
    if (c.nitro && car.crew != 0) {
        const int32_t laps = c.race->laps;
        if ((15.0f / static_cast<float>(laps)) * static_cast<float>(laps - c.crossings) > body.nitroLeft) {
            c.nitro = false;
            c.nitroTimer = 0.0f;
        }
    }
    if (f.rules.mode == RaceMode::Arcade && f.rules.arcadeNitro) {
        body.nitroLeft = 15.0f;
        c.nitro = c.throttle > 0.95f;
    }
    if (c.finished) {
        c.nitro = false;
        c.nitroTimer = 0.0f;
    }
    if (body.nitroLeft == 0.0f) {
        c.nitro = false;
        c.nitroTimer = 10.0f;
    }
    c.bend = b;
    return b;
}

// 0x0049F450: the grid's throttle: the pattern stepped; in the last 8 s of the count the first throttle and its time
// grow and the second throttle's chance falls from 0.7 to 0.
void gridThrottle(Competitor& c, Frame& f) {
    RevPattern& revs = c.car->revs;
    revs.step(f.random, f.frameTime);
    float left = f.race.count;
    if (left < 0.0f) left = 0.0f;
    float chance = 0.7f;
    if (left < 8.0f) {
        const float near = (8.0f - left) / 8.0f;
        chance = 0.7f - near;
        if (chance < 0.0f) chance = 0.0f;
        revs.entries[1] = near / 100.0f + revs.entries[1];
        revs.entries[0] = near / 75.0f + revs.entries[0];
    }
    revs.chance = chance;
    c.throttle = revs.throttle();
}

// [0x00725578]: which throttle a car on the grid takes, the pattern's (0x0049F450) when set; it is set (1) in the
// stock game and nothing changes it, so the older revving below is never run.
constexpr int32_t kGridPattern = 1;

// 0x0049D1C0 (not run in the stock game, see kGridPattern): the grid's revving. A draw each frame; when the timer is
// out, a throttle of 0.85 (up to 1 in the count's last second) with the chance ((1 - left x 0.1) + 0.1)² falling as
// the count runs down from 10 s, else none, held for a drawn time of up to 0.3 s.
void gridRevs(Competitor& c, Frame& f) {
    float left = f.race.count;
    const float u = x87::unit(f.random.next());
    if (left < 0.0f) {
        left = 0.0f;
    } else if (left > 10.0f) {
        left = 10.0f;
    }
    const float k = (1.0f - left * 0.1f) + 0.1f;
    if (c.revTimer != 0.0f) {
        c.revTimer = c.revTimer - f.dt;
        if (c.revTimer < 0.0f) {
            c.revTimer = 0.0f;
        } else if (c.revTimer > 10.0f) {
            c.revTimer = 10.0f;
        }
        c.throttle = c.revThrottle;
        return;
    }
    if (!(k * k > u)) {
        c.revThrottle = 0.0f;
    } else {
        c.revThrottle = 0.85f;
        if (left < 1.0f) c.revThrottle = (1.0f - left) * 0.15f + 0.85f;
    }
    const float time = static_cast<float>(f.random.next()) * std::bit_cast<float>(0x37199ACDu);  // 0.3 / 32767
    c.throttle = c.revThrottle;
    c.revTimer = time;
}

// 0x0049DBB0: the speed. A target from the bend ahead (500 on a straight; else from (1 - (b - 0.2) x 1.25)² x
// 109.515 + MINCORNERSPEED, nearer the full value the better the driver), less for the rear-wheel drive, more for the
// four; throttle below it, brake above; traffic ahead brakes and swerves; before the start the grid's throttle with
// the car held, after a point-to-point finish a roll to a stop.
void speed(Competitor& c, Frame& f) {
    RaceCar& car = *c.car;
    CarBody& body = *car.body;
    const float vx = body.localVelocity[0];
    const float vy = body.localVelocity[1];
    const float vz = body.localVelocity[2];
    c.handbrake = 0.0f;
    if (f.race.count == 0.0f) {
        const float b = nitroAndBend(c, f, (1.0f - c.skill) + 2.0f);
        float target = 500.0f;
        if (!(b < 0.2f)) {
            const float x = 1.0f - (b - 0.2f) * 1.25f;
            target = x87::pow(x, 2.0f) * 109.515f + c.route->minCornerSpeed;
            const float capped = target < 71.52f ? target : 71.52f;
            const float p = x87::pow(capped / 71.52f, 3.0f);
            target = ((1.0f - p) * c.skill + p) * target;
            if (car.drive == 1) {
                target = target * 0.95f;
            } else if (car.drive == 2) {
                target = target * 1.1f;
            }
        }
        float corner = (1.0f - bendAhead(c, 6.0f)) * 105.045f;
        c.cornerSpeed = corner;
        if (!(corner > c.route->minCornerSpeed)) corner = c.route->minCornerSpeed;
        c.cornerSpeed = corner;
        if (c.finished) target = clampTo(target, 0.0f, 50.0f);
        const float squared = (vx * vx + vy * vy) + vz * vz;
        if (c.topSpeed < std::sqrt(squared) * 2.235f) c.topSpeed = std::sqrt(squared) * 2.235f;
        const float d = vz - target;
        if (d < -1.0f) {
            c.throttle = clampTo(d / -15.0f, 0.0f, 1.0f);
            c.brake = 0.0f;
            if (vz < -1.0f && c.reverseTime == 0.0f) {
                c.brake = 1.0f;
                c.throttle = 0.0f;
            }
        } else {
            c.brake = d > 1.0f ? clampTo(d / 10.0f, 0.0f, 0.95f) : 0.0f;
            c.throttle = 0.0f;
        }
    }
    if (car.tipped) {
        c.brake = 0.8f;
        c.throttle = 0.0f;
    }
    const float front = car.wheels[0].slide > car.wheels[1].slide ? car.wheels[0].slide : car.wheels[1].slide;
    const float rear = car.wheels[2].slide > car.wheels[3].slide ? car.wheels[2].slide : car.wheels[3].slide;
    if (rear - front > 0.02f) c.brake = 0.0f;
    if (f.race.count == 0.0f && vz > 2.0f) {
        bool stopped = false;
        for (const std::unique_ptr<Neighbour>& np : c.neighbours) {
            const Neighbour& n = *np;
            const float lz = n.local[2];
            const float lx = n.local[0];
            if (600.0f * 0.25f < lz && n.other->car->body->localVelocity[2] < vz) {
                if (n.contactTime < 2.5f && -250.0f < lx && lx < 250.0f && c.bend > 0.3f) {
                    c.brake = clampTo(1.0f - n.contactTime / 2.5f, 0.0f, 1.0f);
                    c.throttle = 0.0f;
                }
                if (lz < 600.0f * 1.5f && -250.0f < lx && lx < 250.0f) {
                    c.throttle = 0.0f;
                    c.brake = 1.0f;
                    if (vz < 1.788f && c.raceTime > 1.25f) {
                        stopped = true;
                        if (c.swerveTimer == 0.0f) {
                            c.swerveTimer = 1.5f;
                            c.swerveSide = c.offset < 0.0f ? 1.0f : -1.0f;
                        }
                    }
                    if (!n.sameCrew) c.trafficTime = f.dt + f.dt + c.trafficTime;
                }
            }
            if (-250.0f < lx && lx < 250.0f && 600.0f * -0.5f < lz && lz < 600.0f * 0.5f &&
                c.distance() < n.other->distance()) {
                c.throttle = 0.0f;
                if (car.bodyContact && f.rules.type != RaceType::Sprint) {
                    c.brake = 1.0f;
                    c.trafficTime = f.dt + f.dt + c.trafficTime;
                }
                if (!n.sameCrew) c.trafficTime = f.dt + f.dt + c.trafficTime;
            }
        }
        if (c.swerveTimer > 0.0f) {
            c.throttle = 0.9f;
            c.brake = 0.0f;
            if (!stopped && c.swerveTimer < 0.5f) c.swerveTimer = 0.0f;
        }
    }
    if (f.race.count <= 0.0f) {
        if (c.raceTime > 5.0f || f.rules.type == RaceType::Sprint) {
            const float rate = f.rules.aiDifficulty() == 1 ? 6.0f : 2.0f;
            const Racer* r = recordOf(f.rules, car);
            float bonus = (r != nullptr ? r->skill : 0.0f) - 0.5f;
            bonus = bonus + bonus;
            if (!(0.0f <= bonus)) bonus = 0.0f;
            const float growth = bonus * 10.0f + rate;
            if (!body.asrActive || !(c.throttleFactor > 0.8f)) {
                c.throttleFactor = growth * f.dt + c.throttleFactor;
            }
            // Stock problem: with the traction control cutting, the factor drops to 0 when the body's +0x1A4 is over
            // 22.35, a value of the stack the transform left there; 0 here, so it never drops.
        }
        c.throttleFactor = clampTo(c.throttleFactor, 0.3f, 1.0f);
        c.throttle = c.throttleFactor * c.throttle;
        if (c.reverseTime != 0.0f) c.throttle = 0.75f;
        if ((c.route->pointToPoint || f.rules.type == RaceType::Cruise) && c.finished) {
            c.brake = 0.4f;
            c.throttle = 0.0f;
            c.finishRoll = vz * f.dt + c.finishRoll;
            const auto left = static_cast<int32_t>(f.race.competitors().size()) - c.finishPlace + 1;
            if (static_cast<float>(left) * 17.0f < c.finishRoll) {
                c.brake = 0.5f;
                body.setLocalVelocity({body.localVelocity[0] * 0.97f, body.localVelocity[1],
                                       body.localVelocity[2] * 0.97f, body.localVelocity[3]});
            }
        }
        return;
    }
    if (c.rollingStart > 0.0f && c.rollingDelay == 0.0f) {
        if (!body.engineOn) {
            body.engineOn = true;
            body.clutch = 0.0f;
            body.clutchIn = false;
        }
        c.throttle = 0.4f;
        c.brake = 0.0f;
        if (vz > 3.0f) body.setLocalVelocity({0.0f, 0.0f, 1.0f, 0.0f});
        c.rollingStart = c.rollingStart - (vz * f.dt) * 100.0f;
        return;
    }
    // The grid skid's sound (0x0049E334): with +0x70, no rolling delay and under 0.5 forward, gridskid at the car's
    // place; +0x70 cleared only when it was made (with the sound off it is tried each frame).
    if (c.gridSkid && c.rollingDelay == 0.0f && vz < 0.5f) {
        if (RaceSounds* sounds = raceSounds(); sounds != nullptr && sounds->gridSkid(car, f.random)) c.gridSkid = false;
    }
    if (kGridPattern != 0) {
        gridThrottle(c, f);
    } else {
        gridRevs(c, f);
    }
    c.handbrake = 1.0f;
    body.engineOn = false;
    body.clutch = 0.0f;
    body.clutchIn = false;
    body.setLocalVelocity({0.0f, 0.001f, 0.0f, 0.0f});
    if (vz > 0.01f) c.brake = 1.0f;
}

// 0x0049E6A0: the driver's skill for the frame: the record's (Hard: x 0.75 + 0.25), the level's change after 5 s and
// the throttle it costs, the pull toward the player (every ~20 s the gap is taken: behind the player more skill,
// far ahead less), the arcade's and the career's changes.
void skill(Competitor& c, Frame& f) {
    RaceCar& car = *c.car;
    Competitor* player = playerOf(f.rules);
    const bool noPlayer = player == nullptr;
    const int32_t playerPlace = noPlayer ? 4 : player->place;
    c.skill = 0.0f;
    if (player == &c) {
        c.skill = 1.0f;
    } else {
        const Racer* r = recordOf(f.rules, car);
        const float ability = r != nullptr ? r->skill : 0.0f;
        c.skill = f.rules.aiDifficulty() != 0 ? ability * 0.75f + 0.25f : ability;
        if (noPlayer && car.crew == 0) c.skill = c.skill - 0.1f;
        if (c.raceTime > 5.0f) {
            if (c.level == 0) {
                c.skill = c.skill - 0.2f;
                if (car.crew == 0) {
                    c.throttle = c.throttle * 0.96f;
                } else if (f.rules.aiDifficulty() == 0) {
                    c.throttle = c.throttle * 0.98f;
                }
            } else if (c.level == 1) {
                if (car.crew == 0 || f.rules.aiDifficulty() == 0) c.throttle = c.throttle * 0.99f;
            } else if (c.level == 2) {
                c.skill = car.crew == 0 ? c.skill + 0.28f : c.skill + 0.2f;
            }
        }
        if (!noPlayer) {
            if (x87::unit(f.random.next()) < f.dt * 0.05f) {
                c.lastPlace = c.place;
                for (const std::unique_ptr<Neighbour>& n : c.neighbours) {
                    if (n->other == player) c.playerGap = n->gap * 0.01f;
                }
            }
            if (playerPlace < c.lastPlace) {
                float pull = f.rules.aiDifficulty() == 1 ? 0.5f : 0.35f;
                if (c.playerGap <= 100.0f) pull = (c.playerGap * 0.01f) * pull;
                c.skill = pull + c.skill;
            } else {
                const float ahead = -c.playerGap;
                float most = f.rules.aiDifficulty() == 1 ? 0.1f : 0.3f;
                float penalty = 0.0f;
                if (ahead > 150.0f) {
                    penalty = most;
                    c.throttle = (1.0f - penalty * 0.2f) * c.throttle;
                } else if (ahead > 10.0f) {
                    most = ((ahead - 10.0f) / (150.0f - 10.0f)) * most;
                    penalty = most;
                    c.throttle = (1.0f - penalty * 0.2f) * c.throttle;
                }
                c.skill = c.skill - penalty;
            }
            if (f.rules.mode == RaceMode::Arcade) {
                c.skill = f.rules.arcadeSkill + c.skill;
                if (f.rules.arcadeSkill < 0.0f) c.throttle = (f.rules.arcadeSkill * 2.5f * 0.1f + 1.0f) * c.throttle;
            } else if (f.rules.mode == RaceMode::Career && f.rules.career != nullptr) {
                const Career& career = *f.rules.career;
                if (f.rules.aiDifficulty() != 1) {
                    if (career.profile.introduction != 0) {
                        c.skill = c.skill - 0.2f;
                        c.throttle = c.throttle * 0.98f;
                    } else {
                        c.skill = career.profile.rivalsSkill + c.skill;
                        c.throttle = (career.profile.rivalsSkill * 0.25f + 1.0f) * c.throttle;
                    }
                }
                // The crew bet with drives better: a pink slip 0.25, a bet under 3/4 of the crew's money 0.066, over
                // 5/4 of it 0.2, else 0.125 (0x004B1F20, 0x004B1EE0).
                const CareerBets& bets = career.bets;
                if (bets.placed != 0 && car.crew == bets.crew && bets.crew >= 0 &&
                    static_cast<size_t>(bets.crew) < career.crews.size()) {
                    const float money = career.crews[static_cast<size_t>(bets.crew)].money;
                    if (bets.pinkSlip != 0) {
                        c.skill = c.skill + 0.25f;
                    } else if (money * 0.75f > bets.amount) {
                        c.skill = c.skill + 0.066f;
                    } else if (money * 1.25f < bets.amount) {
                        c.skill = c.skill + 0.2f;
                    } else {
                        c.skill = c.skill + 0.125f;
                    }
                }
            }
        }
    }
    c.skill = clampTo(c.skill, 0.0f, 1.0f);
    c.throttle = clampTo(c.throttle, 0.0f, 1.0f);
}

// 0x0049EB90: the mistakes. While steering over 0.1 above 10 m/s, with the chance dt / (the interval: x 10 when the
// player is ahead or for the player's crew, x 5 on Hard), 9 in 100 a spin of 1.2 s, the rest a wobble of 0.3 s; then
// 5 s without. A player's own car makes none.
void mistakes(Competitor& c, Frame& f) {
    float interval = c.mistakeInterval;
    c.spun = false;
    RaceCar& car = *c.car;
    Competitor* player = playerOf(f.rules);
    if (player == &c) return;
    const int32_t playerPlace = player != nullptr ? player->place : 100;
    bool now = false;
    Racer* r = recordOf(f.rules, car);
    if (car.crew == 0) {
        if (r != nullptr && r->nerves > 1.9f && r->level == 2 && r->panicTime > 5.0f) {
            now = true;
            interval = 0.0f;
        } else {
            interval = interval * 10.0f;
        }
    } else {
        if (playerPlace < c.place) interval = interval * 10.0f;
        if (f.rules.aiDifficulty() == 1) interval = interval * 5.0f;
    }
    if (std::fabs(c.steering) > 0.1f) {
        const auto value = static_cast<float>(f.random.next());
        if ((value * interval) * 3.0518509e-5f < f.dt && c.mistakeTimer == 0.0f &&
            length3(car.body->localVelocity) > 10.0f) {
            const auto roll = static_cast<uint32_t>(f.random.next());
            log::info("race: car {} makes a mistake", c.index);
            if (r != nullptr) {
                r->levelTimer = 0.0f;
                if (r->nerves > 1.9f && car.crew != 0) {
                    r->level = 0;
                    r->levelTimer = 5.0f;
                    r->nerves = r->nerves - 0.5f;
                }
            }
            if (roll % 100 > 90 || now) {
                c.mistakeKind = 0;
                c.mistakeTimer = 1.2f + 5.0f;
                if (r != nullptr) {
                    r->levelTimer = 0.0f;
                    if (car.crew == 0) r->nerves = 0.0f;
                }
            } else {
                c.mistakeKind = 1;
                c.mistakeTimer = 0.3f + 5.0f;
            }
            c.mistakeRight = c.steering > 0.0f;
        }
    }
    c.mistakeTimer = c.mistakeTimer - f.dt;
    if (c.mistakeTimer < 0.0f) c.mistakeTimer = 0.0f;
    if (c.mistakeTimer > 5.0f) {
        if (c.mistakeKind == 0) {
            c.steering = c.mistakeRight ? 1.0f : -1.0f;
            c.handbrake = 1.0f;
            c.brake = 0.0f;
            if (car.body->localVelocity[2] < 5.0f) {
                c.mistakeTimer = 5.0f;
                c.spun = true;
            }
        } else if (c.mistakeKind == 1) {
            c.steering = c.mistakeRight ? -0.2f : 0.2f;
            c.handbrake = 0.7f;
            c.throttle = 0.7f;
            c.brake = 0.5f;
        }
    }
    if (c.angle > 2.1991148f) c.spun = true;
}

// 0x0049EF00: a competitor's frame.
void frame(Competitor& c, Frame& f) {
    if (!f.rules.replay) timers(c, f);
    neighbours(c, f);
    chooseLine(c);
    follow(c, f);
    if (!f.rules.replay) {
        steer(c, f);
        speed(c, f);
        skill(c, f);
        mistakes(c, f);
    }
}

}  // namespace

bool hornSounds(Competitor& c, float frameTime, CrtRandom& random) {
    if (c.hornTimer < 0.0f && (0.0f < c.swerveTimer || 0.0f < c.trafficTime)) {
        c.hornTimer = -5.0f;
        return true;
    }
    c.hornTimer = c.hornTimer - frameTime;
    if (c.hornTimer < -5.0f) c.hornTimer = static_cast<float>(random.next() % 10);
    return false;
}

// --- The race -------------------------------------------------------------------------------------------------

RaceState::RaceState(const Route& route, int32_t laps_, const RaceRules& rules)
    : laps(route.pointToPoint ? 1 : laps_), count(14.0f), m_route(&route), m_rules(rules) {}

Competitor& RaceState::add(RaceCar& car, CrtRandom& random, float frameTime) {
    auto c = std::make_unique<Competitor>();
    c->index = static_cast<int32_t>(m_competitors.size());
    c->race = this;
    c->car = &car;
    c->route = m_route;
    c->skill = 1.0f;
    car.competitor = c.get();
    oj::reset(*c, m_rules, random, frameTime);
    m_competitors.push_back(std::move(c));
    return *m_competitors.back();
}

void RaceState::linkNeighbours() {
    for (std::unique_ptr<Competitor>& a : m_competitors) {
        for (std::unique_ptr<Competitor>& b : m_competitors) {
            if (a == b) continue;
            auto n = std::make_unique<Neighbour>();
            n->other = b.get();
            a->neighbours.push_back(std::move(n));
        }
    }
}

float RaceState::startCount() const {
    if (m_rules.replayRace) return 4.005f;
    switch (m_rules.type) {
    case RaceType::TimedLap:
    case RaceType::PerfectLap:
    case RaceType::Speed:
    case RaceType::Cruise:
    case RaceType::Circuit:
    case RaceType::Sprint:
    case RaceType::PointToPoint:
    case RaceType::Unused8:
    case RaceType::TestTrack:
        // 0x0049FDF0 on the network: 0.005 for a spectator, else 4.005 (Stock problem: a spectator of another
        // type counts 4 s).
        if (m_rules.network)
            return m_rules.spectator ? std::bit_cast<float>(0x3BA3D70Au) : std::bit_cast<float>(0x408028F6u);
        return m_rules.type == RaceType::Cruise ? 4.005f : 14.0f;
    default: return 4.0f;
    }
}

void RaceState::reset(CrtRandom& random, float frameTime) {
    if (m_rules.mode == RaceMode::Career && m_rules.career != nullptr) buildChatter();
    const_cast<Route*>(m_route)->resetSectors();
    bestLap = 0.0f;
    for (std::unique_ptr<Competitor>& c : m_competitors) oj::reset(*c, m_rules, random, frameTime);
    finishers.clear();
    order.clear();
    count = startCount();
    const bool listed =
        m_rules.type >= RaceType::TimedLap && m_rules.type <= RaceType::TestTrack && m_rules.type != RaceType::Solo;
    second = !m_rules.replayRace && listed && m_rules.network && m_rules.spectator ? 0 : 3;
    leader = nullptr;
    spinner = nullptr;
    leaderPending = false;
    leaderChanged = false;
    leaderTimer = 0.0f;
    spinEvent = false;
    finishersShown = 0;  // 0x0049FF02
    eventHold = 5.0f;
}

namespace {

// 0x00424230: a float made a whole number cut toward zero (x87 chop, or SSE with the game's MXCSR); out of range the
// x87's indefinite value.
int32_t chop(float v) {
    if (!(v > -2147483648.0f && v < 2147483648.0f)) return INT32_MIN;
    return static_cast<int32_t>(v);
}

// A whole number read back as an unsigned one (fild, plus 2^32 when negative), rounded to a float.
float asUnsigned(int32_t i) {
    return i < 0 ? static_cast<float>(static_cast<double>(i) + 4294967296.0) : static_cast<float>(i);
}

}  // namespace

void RaceState::driverLevels(float frameTime) {
    RaceSession* session = m_rules.session;
    if (session == nullptr) return;
    std::vector<RaceCar*> cars(session->racers.size(), nullptr);
    for (std::unique_ptr<Competitor>& c : m_competitors) {
        const int32_t r = c->car->record;
        if (r >= 0 && static_cast<size_t>(r) < cars.size()) cars[static_cast<size_t>(r)] = c->car;
    }
    const Competitor* player = playerOf(m_rules);
    const int32_t playerPlace = player != nullptr ? player->place : 8;
    for (uint32_t i = 0; i < session->count && i < session->racers.size(); ++i) {
        Racer& rec = session->racers[i];
        RaceCar* car = cars[i];
        if (car == nullptr) continue;
        Competitor& c = *car->competitor;
        if (rec.crew != 0) {
            // 0x004BEFD0: a rival chooses its level every 0.2 s by the gaps to the cars ahead and behind (metres).
            rec.levelTimer = rec.levelTimer - frameTime;
            if (!(rec.levelTimer > 0.0f)) {
                int32_t mode = 1;
                const float a = c.ahead != nullptr ? std::fabs(c.ahead->gap * 0.01f) : 1000.0f;
                const float b = c.behind != nullptr ? std::fabs(c.behind->gap * 0.01f) : 1000.0f;
                if (a > 90.0f) {
                    if (b > 90.0f && c.ahead == nullptr && b > 100.0f) mode = 0;
                    if (b < 90.0f && b < 50.0f) mode = 2;
                }
                if (a < 90.0f) {
                    if (b < 90.0f && (a < 50.0f || b < 50.0f)) mode = 2;
                    if (b > 90.0f && a < 50.0f) mode = 2;
                }
                if (b < 10.0f && c.behind != nullptr && c.behind->sameCrew) {
                    rec.order = 2;  // 0x004BF2F0
                    rec.orderTime = 0.0f;
                    rec.levelTimer = 10.0f;
                } else {
                    if (m_rules.type == RaceType::Sprint) mode = 2;
                    int32_t level = 0;
                    if (mode == 2) {
                        if (rec.nerves < 1.75f) {
                            level = 2;
                        } else if (rec.nerves <= 1.99f || rec.panicTime <= 5.0f) {
                            level = rec.level;
                        } else {
                            level = 1;
                        }
                    } else if (mode == 1) {
                        if (rec.nerves < 0.5f) {
                            level = 2;
                        } else if (rec.nerves <= 1.5f) {
                            level = 1;
                        }
                    }
                    rec.levelTimer = 0.2f;
                    if (level != rec.level) {
                        rec.level = level;
                        rec.levelTimer = -1.0f;
                    }
                }
            }
        } else if (c.behind != nullptr && std::fabs(c.behind->gap * 0.01f) < 10.0f && c.behind->sameCrew &&
                   rec.level == 0) {
            // 0x004BF210: a driver of the player's crew on the low level with a crewmate within 10 m behind lets it
            // by (the order 2 for 10 s).
            rec.order = 2;
            rec.orderTime = 0.0f;
            rec.levelTimer = 10.0f;
        }
        // 0x004BF280: under the orders 1 and 2 the driver stays on the low level for 10 s.
        rec.orderTime = frameTime + rec.orderTime;
        if (rec.order == 1 || rec.order == 2) {
            if (rec.orderTime < 10.0f) {
                rec.level = 0;
            } else {
                rec.order = 0;
                rec.orderTime = 0.0f;
            }
        }
        if (rec.nerves > 1.9f) {
            rec.panicTime = frameTime + rec.panicTime;
        } else {
            rec.panicTime = 0.0f;
        }
        // The pressure: the nearer of twice the gap ahead and the gap behind, each made a whole unsigned number (so
        // a car behind counts from 2^32); a car of the same crew not at all.
        float ahead = 10000.0f;
        float behind = 10000.0f;
        if (c.ahead != nullptr && c.ahead->other->car->crew != rec.crew) {
            ahead = asUnsigned(chop(c.ahead->gap * 0.01f));
            ahead = ahead + ahead;
        }
        if (c.behind != nullptr && c.behind->other->car->crew != rec.crew) {
            behind = asUnsigned(chop(c.behind->gap * 0.01f));
        }
        float nearest = std::fabs(ahead);
        const float b = std::fabs(behind);
        if (!(nearest < b)) nearest = b;
        float pressure = nearest <= 50.0f ? (1.0f - nearest * 0.02f) + 1.0f : 1.0f / static_cast<float>(c.place);
        if (static_cast<uint32_t>(c.place) < static_cast<uint32_t>(playerPlace)) {
            const Vec4& v = car->body->localVelocity;
            if (std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]) > 20.0f && c.behind != nullptr &&
                rec.level == 2 && c.behind->other->car->crew != rec.crew && c.behind->gap * 0.01f > -15.0f) {
                pressure = 2.5f;
            }
        }
        // With a bet on the race (the session's +0x370) the crew bet with feels 0.3 of it.
        if (session->bet != 0 && m_rules.career != nullptr && rec.crew == m_rules.career->bets.crew) {
            pressure = pressure * 0.3f;
        }
        if (rec.crew != 0 && player != nullptr &&
            static_cast<uint32_t>(c.place) > static_cast<uint32_t>(playerPlace)) {
            pressure = pressure * 0.3f;
        }
        if (rec.level == 1) {
            rec.nerves = rec.nerves - frameTime * rec.nerveLoss;
        } else if (rec.level == 2) {
            const float before = rec.nerves;
            rec.nerves = pressure * rec.nerveGain * frameTime + rec.nerves;
            // 0x004BEDFF: crossing 1.9 the orders asked for as the chatter asks (0x004BEA60) for no racer (-1): the
            // orders opened, no line flashed.
            if (before < 1.9f && rec.nerves >= 1.9f) ordersFlash(-1, frameTime);
        } else {
            rec.nerves = rec.nerves - frameTime * rec.nerveLoss * 3.0f;
        }
        rec.nerves = clampTo(rec.nerves, 0.0f, 2.0f);
        if (count > 0.0f || c.finished) rec.nerves = 0.0f;
        c.level = rec.level;
        if (rec.level == 1) {
            c.mistakeInterval = rec.nerves <= 1.9f ? (2.0f - rec.nerves) * 100.0f + 12.0f : 0.5f;
        } else if (rec.level == 2) {
            c.mistakeInterval = rec.nerves <= 1.9f ? (2.0f - rec.nerves) * 20.0f + 0.5f : 0.5f;
        } else {
            c.mistakeInterval = 200.0f;
        }
    }
}

bool RaceState::betterLap(float time) {
    if (bestLap != 0.0f && bestLap <= time) return false;
    bestLap = time;
    return true;
}

void RaceState::finish(Competitor& c) {
    c.finished = true;
    c.progressTimer = 10.0f;
    // 0x0049BE50: the car's damage kept as it is (0x00556FB0).
    if (c.car != nullptr && c.car->damage != nullptr) c.car->damage->freeze();
    finishers.push_back(&c);
    c.finishPlace = static_cast<int32_t>(finishers.size());
    c.place = c.finishPlace;
    if (c.finishPlace == 1) c.points = c.points + 6.0f;
    if (c.finishPlace == 2) c.points = c.points + 4.0f;
    if (c.finishPlace == 3) c.points = c.points + 2.0f;
    // 0x0049FF30: in a career race other than a sprint the session takes the finish (0x004B9CF0); the first place's
    // racer and crew kept in the session (+0x368, +0x36C).
    if (m_rules.mode == RaceMode::Career && m_rules.type != RaceType::Sprint && m_rules.type != RaceType::TestTrack &&
        m_rules.career != nullptr) {
        racerFinished(*m_rules.career, c.car->record, c.raceTime, c.bestLap, false);
    }
    // 0x0049FFB1: on the network a finish of this machine's car (not of another's, kind 2) outside a cruise is sent.
    if (m_rules.network && m_rules.type != RaceType::Cruise && c.car != nullptr && c.car->kind != CarKind::Remote &&
        m_rules.netFinished) {
        m_rules.netFinished(c);
    }
    if (c.finishPlace == 1 && m_rules.session != nullptr) {
        m_rules.session->winner = c.car->record;
        m_rules.session->winnerCrew = c.car->crew;
    }
    log::info("race: car {} finishes in place {} at {:.2f} s", c.index, c.finishPlace, c.raceTime);
    arcadeResult(100);
}

namespace {

// The model id of the race's first car (its record's CarDef), -1 without one.
int32_t firstCarModel(const std::vector<std::unique_ptr<Competitor>>& competitors) {
    if (competitors.empty() || competitors[0]->car == nullptr) return -1;
    const Car* record = competitors[0]->car->carRecord;
    return record != nullptr && record->def != nullptr ? record->def->id : -1;
}

}  // namespace

void RaceState::arcadeResult(int32_t place) {
    if (m_rules.mode != RaceMode::Arcade || m_rules.arcade == nullptr || m_competitors.empty() || m_rules.replayRace)
        return;
    const Competitor& first = *m_competitors[0];
    const bool winAll = m_rules.career != nullptr && m_rules.career->cheatFlags[kCheatWinAllRaces] != 0;
    if (!winAll) {
        switch (m_rules.type) {
        case RaceType::Cruise: {
            // The cruise's score over the arcade race's target (its +0x10, cut).
            if (m_rules.career == nullptr) return;
            const Arcade& arcade = *m_rules.arcade;
            const int32_t value = arcade.i32(arcade.current() + Arcade::kRaceChallengeValue);
            const auto target = x87::chop(std::bit_cast<float>(value));
            if (m_rules.career->cruise.score() <= target) return;
            place = 1;
            break;
        }
        case RaceType::Circuit:
        case RaceType::PointToPoint:
            if (!first.finished) return;
            place = first.finishPlace;
            break;
        case RaceType::Sprint: break;
        default:
            return;
        }
        if (place != 1) return;
    }
    m_rules.arcade->complete(firstCarModel(m_competitors));
    log::info("race: the arcade's race completed");
}

void RaceState::challengeResult() {
    if (m_rules.challenge != ChallengeOutcome::None) return;
    m_rules.challenge = ChallengeOutcome::Met;
    if (m_rules.mode == RaceMode::Arcade && m_rules.arcade != nullptr)
        m_rules.arcade->complete(firstCarModel(m_competitors));
}

void RaceState::start() {
    for (std::unique_ptr<Competitor>& c : m_competitors) {
        CarBody& body = *c->car->body;
        body.automatic = true;
        if (!body.engineOn) {
            body.engineOn = true;
            body.clutch = 0.0f;
            body.clutchIn = false;
        }
        c->handbrake = 0.0f;
    }
}

void RaceState::events(float frameTime) {
    leaderChanged = false;
    spinEvent = false;
    if (eventHold != 0.0f) eventHold = clampTo(eventHold - 0.02f, 0.0f, 5.0f);
    if (leaderPending) {
        leaderTimer = leaderTimer - frameTime;
        if (leaderTimer < 0.0f) {
            leaderChanged = true;
            eventHold = 2.0f;
            leaderPending = false;
        }
    }
    int32_t place = 1;
    for (Competitor* c : order) {
        if (eventHold == 0.0f) {
            if (place == 1 && leader != c) {
                leader = c;
                leaderTimer = 2.0f;
                leaderPending = true;
            }
            if (c->spun && c != spinner) {
                spinner = c;
                spinEvent = true;
                eventHold = 2.0f;
            }
        }
        ++place;
    }
}

RaceState::Events RaceState::update(float dt, float frameTime, CrtRandom& random) {
    Events e;
    respawnTimer = respawnTimer - dt;
    if (respawnTimer < 0.0f) respawnTimer = 0.0f;
    if (count > 0.0f) {
        // On the network the count holds until the session's stage 12 (the race).
        if (!m_rules.network || (m_rules.netStage && m_rules.netStage() == net::SessionStage::Racing))
            count = count - dt;
        // Stock problem: a count that lands on exactly 0 never starts the race (the cars are let go, nothing else
        // of the start happens). The fixed build starts it at 0 too.
        if (count <= 0.0f) {
            count = 0.0f;
            start();
            e.go = true;
        }
        if (count < static_cast<float>(second)) {
            e.beep = true;
            second = second - 1;
        }
    }
    Frame f{*this, m_rules, random, dt, frameTime};
    order.clear();
    for (std::unique_ptr<Competitor>& c : m_competitors) {
        frame(*c, f);
        order.push_back(c.get());
    }
    // 0x004A0480 with 0x004A00B0 (std::sort: its quicksort past 16, 0x004A04F0, is not reached by a race's cars): by
    // the race's distance, the further first; the insertion sort of up to 16 keeps equals in their order.
    std::stable_sort(order.begin(), order.end(),
                     [](const Competitor* a, const Competitor* b) { return a->raceDistance > b->raceDistance; });
    int32_t place = 1;
    for (Competitor* c : order) {
        if (!c->finished) {
            c->previousPlace = c->place;
            c->place = place;
        }
        ++place;
    }
    events(frameTime);
    // 0x004A0220's end: the career race's chatter once the count is over.
    if (m_rules.mode == RaceMode::Career && m_rules.career != nullptr && count <= 0.0f) chatterFrame(frameTime, random);
    return e;
}

void RaceState::buildChatter() {
    Chatter& c = chatter;
    c.player = -1;
    c.crew.clear();
    c.hasBet = false;
    const RaceSession& session = m_rules.career->session;
    for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
        const Racer& r = session.racers[i];
        ChatterEntry e;
        e.racer = static_cast<int32_t>(i);
        e.lastLevel = r.level;
        if (r.crew == 0) {
            if (r.member == 0) {
                c.player = static_cast<int32_t>(i);
            } else {
                c.crew.push_back(e);
            }
        } else if (session.bet != 0 && session.betRacer == i) {
            c.hasBet = true;
            c.bet = e;
        }
    }
}

Competitor* RaceState::competitorOf(int32_t racer) const {
    for (const std::unique_ptr<Competitor>& c : m_competitors) {
        if (c->car != nullptr && c->car->record == racer) return c.get();
    }
    return nullptr;
}

Competitor* RaceState::atPlace(int32_t place) const {
    const int32_t i = place - 1;
    return i >= 0 && static_cast<size_t>(i) < order.size() ? order[static_cast<size_t>(i)] : nullptr;
}

void RaceState::ordersFlash(int32_t racer, float frameTime) {
    if (m_rules.ordersFlash) m_rules.ordersFlash(racer, frameTime);
}

void RaceState::chatterFrame(float frameTime, CrtRandom& random) {
    if (m_rules.replayRace) return;
    Chatter& c = chatter;
    RaceSounds* sounds = raceSounds();
    const bool speech = sounds != nullptr && sounds->hasSpeech();
    if (!(sounds != nullptr && sounds->speaking())) {
        c.speakerMember = -1;
        c.speakerPlace = -1;
        c.queued = 0;
    }
    if (c.queued != 0) return;
    RaceSession& session = m_rules.career->session;
    if (c.hasBet) {
        const int32_t s = betLine(c.bet, frameTime, random);
        if (s != -1 && speech) {
            sounds->say(session.racers[static_cast<size_t>(c.bet.racer)].crew, s);
            c.queued = 1;
        }
    }
    if (c.queued != 0) return;
    for (ChatterEntry& p : c.crew) {
        const int32_t s = crewLine(p, frameTime, random);
        if (s == -1 || !speech) continue;
        const Racer& r = session.racers[static_cast<size_t>(p.racer)];
        // The crewmate's number ([[career+4]+0x500 + member x 4]'s +0x00): 2 and 3 its own lines, else 0.
        const int32_t k = r.member >= 0 && r.member < 4 ? m_rules.career->members[static_cast<size_t>(r.member)] : 0;
        sounds->say2(k == 2 ? 1 : k == 3 ? 2 : 0, s);
        c.speakerMember = r.member;
        const Competitor* comp = competitorOf(p.racer);
        c.speakerPlace = comp != nullptr ? comp->place : -1;
        c.queued = 1;
        return;
    }
}

int32_t RaceState::betLine(ChatterEntry& p, float t, CrtRandom& random) {
    int32_t s = -1;
    const Competitor* comp = competitorOf(p.racer);
    const Competitor* rival = chatter.player >= 0 ? competitorOf(chatter.player) : nullptr;
    if (comp != nullptr && comp->crossings != 0) {
        const int32_t place = comp->place;
        if (place != p.lastPlace && p.delay < 0.0f) {
            p.lastPlace = place;
            if (p.skip == 0) {
                if (chatter.player >= 0) {
                    const Competitor* behind = atPlace(place + 1);
                    if (behind != nullptr && rival != nullptr && behind->car == rival->car) {
                        static const int32_t kBehind[4] = {1, 3, 7, 9};
                        s = kBehind[random.next() & 3];
                        p.delay = 2.0f;  // [0x00718A14]
                    }
                    const Competitor* ahead = atPlace(place - 1);
                    if (ahead != nullptr && rival != nullptr && ahead->car == rival->car) {
                        const int32_t r = random.next() & 3;
                        s = r < 2 ? 0 : 6;
                        p.delay = 2.0f;
                    }
                }
            } else {
                p.skip = 0;
            }
        }
        if (comp->finished && p.done == 0) {
            const int32_t rivalPlace = rival != nullptr ? rival->place : 0;
            if (place < rivalPlace) {
                s = (random.next() & 1) != 0 ? 4 : 10;
            } else {
                s = (random.next() & 1) != 0 ? 5 : 11;
            }
            p.done = 1;
        }
    }
    p.delay = p.delay - t;
    return s;
}

int32_t RaceState::crewLine(ChatterEntry& p, float t, CrtRandom& random) {
    const int32_t saved = p.lastPlace;
    if (p.done != 0) return -1;
    RaceSession& session = m_rules.career->session;
    Racer& racer = session.racers[static_cast<size_t>(p.racer)];
    int32_t s = -1;
    // 1. The driving level.
    const int32_t lv = racer.level;
    const int32_t old = p.lastLevel;
    if (lv != old && p.delay < 0.0f) {
        p.lastLevel = lv;
        if (p.skip == 0) {
            if (lv == 1) {
                s = (random.next() & 1) == 0 ? 0x12 : (lv > old ? 0x22 : 0x21);
            } else if (lv == 2) {
                s = 0x11;
            } else {
                s = 0x13;
            }
            ordersFlash(p.racer, t);
            p.delay = 2.0f;
        }
        p.skip = 0;
    }
    Competitor* comp = competitorOf(p.racer);
    if (comp == nullptr) {
        p.delay = p.delay - t;
        return s;
    }
    const Competitor* rival = chatter.player >= 0 ? competitorOf(chatter.player) : nullptr;
    // 2. The place.
    bool placeDone = false;
    if (comp->crossings != 0) {
        const int32_t place = comp->place;
        const auto last = static_cast<int32_t>(session.count);
        if (place != p.lastPlace && p.delay < 0.0f) {
            ordersFlash(p.racer, t);
            if (p.lastPlace == -1 || place == 1 || place < p.lastPlace) {
                static const int32_t kGained[5] = {0, 3, 5, 7, 9};
                s = place >= 1 && place <= 5 ? kGained[place - 1] : 1;
                const Competitor* behind = atPlace(place + 1);
                if (chatter.player >= 0 && p.rivalArmed != 0 && behind != nullptr && rival != nullptr &&
                    behind->car == rival->car) {
                    // Stock problem: the place and the delay are not kept: next frame it changes again.
                    s = 0x20;
                    p.rivalArmed = 0;
                    placeDone = true;
                }
            } else {
                static const int32_t kLost[5] = {0, 2, 4, 6, 8};
                s = place >= 1 && place <= 5 ? kLost[place - 1] : 1;
            }
            if (!placeDone) {
                if (place == last) s = 1;
                p.lastPlace = place;
                p.rivalArmed = 1;
                p.delay = 2.0f;
            }
        }
    }
    // 3. The start line (the place's own change undone).
    const int32_t v = comp->crossings;
    if (v != p.lastCrossings && p.delay < 0.0f) {
        if (v > 1) {
            if (comp->finished) {
                s = comp->place == 1 ? 0x18 : 0x17;
                p.done = 1;
            } else {
                s = 0x16;
            }
        }
        p.lastCrossings = v;
        p.lastPlace = saved;
        p.delay = 2.0f;
    }
    const RaceCar& car = *comp->car;
    // 4. The nitro asked.
    p.nitroTimer = p.nitroTimer - t;
    if (car.nitroAsked && p.nitroTimer < 0.0f && p.delay < 0.0f) {
        p.nitroTimer = 5.0f;  // [0x00718A18]
        s = 0x0F;
        p.lastPlace = saved;
        p.delay = 2.0f;
    }
    // 5. The nitro gone (the delay not set again).
    if (p.nitroOutSaid == 0 && p.delay < 0.0f && car.hasNitro && car.body != nullptr && car.body->nitroLeft == 0.0f) {
        p.nitroOutSaid = 1;
        s = 0x10;
    }
    // 6. Spun: the crewmate's order 1 (0x004BF2F0).
    p.spinTimer = p.spinTimer - t;
    if (comp->spun && p.spinTimer < 0.0f && p.delay < 0.0f) {
        ordersFlash(p.racer, t);
        p.spinTimer = 20.0f;  // [0x00718A1C]
        p.delay = 2.0f;
        s = 0x0A;
        racer.order = 1;
        racer.orderTime = 0.0f;
    }
    // 7. In traffic.
    p.trafficTimer = p.trafficTimer - t;
    if (comp->trafficTime > 0.5f && p.trafficTimer < 0.0f && p.delay < 0.0f) {
        ordersFlash(p.racer, t);
        s = 0x1E;
        p.trafficTimer = 20.0f;
        p.delay = 2.0f;
    }
    // 8. Pressed.
    p.behindTimer = p.behindTimer - t;
    if ((comp->behindTime > 2.0f || racer.nerves > 1.9f) && p.behindTimer < 0.0f && p.delay < 0.0f) {
        ordersFlash(p.racer, t);
        s = 0x1D;
        p.behindTimer = 20.0f;
        p.delay = 2.0f;
    }
    // 9. The contacts, each flag cleared.
    if (racer.rammed == 1 && p.delay < 0.0f) {
        ordersFlash(p.racer, t);
        p.delay = 2.0f;
        s = 0x19;
        racer.rammed = 0;
    }
    if (racer.rammed == 2 && p.delay < 0.0f) {
        ordersFlash(p.racer, t);
        p.delay = 2.0f;
        s = 0x1A;
        racer.rammed = 0;
    }
    if (racer.crashed == 1 && p.delay < 0.0f) {
        ordersFlash(p.racer, t);
        p.delay = 2.0f;
        s = 0x0E;
        racer.crashed = 0;
    }
    if (racer.crashed == 2 && p.delay < 0.0f) {
        ordersFlash(p.racer, t);
        p.delay = 2.0f;
        s = 0x0C;
        racer.crashed = 0;
    }
    // 10. The player spun.
    p.playerSpinTimer = p.playerSpinTimer - t;
    if (chatter.player >= 0 && p.playerSpinTimer < 0.0f && p.delay < 0.0f && rival != nullptr && rival->spun) {
        s = 0x1F;
        p.playerSpinTimer = 20.0f;
        p.delay = 2.0f;
    }
    // 11. The steering pulls (no delay asked).
    if (p.pullsSaid == 0 && car.car != nullptr && car.car->steeringPulls != 0) {
        s = 0x0D;
        p.delay = 2.0f;
        p.pullsSaid = 1;
    }
    p.delay = p.delay - t;
    return s;
}

RaceSounds*& raceSounds() {
    static RaceSounds* sounds = nullptr;
    return sounds;
}

void (*&speechStop())() {
    static void (*stop)() = nullptr;
    return stop;
}

}  // namespace oj
