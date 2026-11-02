// Unit tests: the fixed build's replays of Acclaim's Xbox builds made the PC's (Core/Race/Replay.h, Replay::fromXbox).

#include "Framework/Test.h"

#include "Core/Race/Replay.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using oj::test::check;

void put32(std::vector<uint8_t>& out, size_t at, uint32_t v) { std::memcpy(out.data() + at, &v, 4); }
uint32_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}

// An Xbox build's car record as its replay.hdr has it: the May build's 196 bytes or the August build's 464 (the damage
// grid's 256 bytes there, one cell of 0x80).
std::vector<uint8_t> xboxRecord(const char* model, bool august) {
    std::vector<uint8_t> r(august ? 464 : 196, 0);
    std::memcpy(r.data(), model, std::strlen(model));
    const uint32_t colours[10] = {0xFFEFBD0Eu, 0xFF101010u, bits(0.5f), 0xFFEFBD0Eu, bits(0.5f),
                                  0xFF00FFFFu, 0xC8C8C8C8u, 0xFF202020u, 0xFF808080u, 0};
    for (size_t i = 0; i < 10; ++i) put32(r, 0x40 + i * 4, colours[i]);
    const uint8_t body[10] = {3, 1, 2, 1, 1, 1, 1, 1, 0, 1};
    const uint8_t mods[10] = {9, 0x11, 6, 8, 0x38, 9, 9, 7, 8, 2};
    std::memcpy(r.data() + 0x68, body, 10);
    std::memcpy(r.data() + 0x72, mods, 10);
    put32(r, 0x7C, bits(0.8f));    // lowering
    put32(r, 0x80, bits(0.5f));    // gearing
    put32(r, 0x84, 50);            // spent
    put32(r, 0x90, bits(0.02f));   // the brakes' wear
    put32(r, 0x94, bits(4750.0f)); // brake
    put32(r, 0x9C, bits(2.4f));    // grip
    put32(r, 0xA0, bits(15.0f));   // nitro
    put32(r, 0xA4, bits(660.0f));  // rating
    if (august) {
        for (size_t i = 0; i < 3; ++i) put32(r, 0xA8 + i * 4, bits(-1.0f));
        put32(r, 0xB4, 1);  // the grid's flag
        r[0xB8 + 17] = 0x80;
        put32(r, 0x1B8, bits(1.0f));  // torqueFactor
        put32(r, 0x1BC, 1);           // steeringPulls
        put32(r, 0x1CC, 1);           // sponsorDecals
    } else {
        put32(r, 0xA8, 1);
        put32(r, 0xB0, bits(1.0f));  // torqueFactor
        put32(r, 0xB4, 1);           // steeringPulls
    }
    return r;
}

// A frame of the replay: car 1 at a place, turned, in third gear.
oj::ReplayFrame frameOf(float time, int32_t car) {
    oj::ReplayFrame f;
    f.time = time;
    f.car = car;
    f.orientation = {0.0f, 0.0f, -1.0f, 0.0f};
    f.position = {-475.5f, 0.4f, -191.7f, 0.0f};
    f.wheels = {10.0f, 10.0f, 12.0f, 12.0f};
    f.gear = 3;
    f.steering = 0.2f;
    f.rpm = 5000.0f;
    f.throttle = 1.0f;
    return f;
}

struct XboxFiles {
    oj::ReplayHeader header;
    std::vector<uint8_t> cars;
    std::vector<uint8_t> dat;
};

// Two cars and one block of the Xbox builds (0x1E0C bytes: the head, 240 frames of 32 bytes), its first two frames
// written.
XboxFiles xboxFiles(bool august) {
    XboxFiles x;
    x.header.lastBlock = 0;
    x.header.lastFrames = 2;
    x.header.length = 0.5f;
    x.header.bytes = 0x1E0C;
    x.header.track = "SOUTHSIDE BEACH";
    x.header.route = "SOUTHSIDE_R2";
    x.header.cars = 2;
    x.header.mode = 1;
    x.header.type = 5;
    x.header.laps = 1;
    for (const char* model : {"EVO8_STOCK", "TRANS_STOCK"}) {
        const std::vector<uint8_t> r = xboxRecord(model, august);
        x.cars.insert(x.cars.end(), r.begin(), r.end());
    }
    x.header.carsBytes = static_cast<uint32_t>(x.cars.size());
    x.dat.assign(0x1E0C, 0);
    put32(x.dat, 4, 1);  // the next block
    for (int32_t i = 0; i < 2; ++i) {
        const oj::PackedFrame p = oj::packFrame(frameOf(0.04f, i));
        std::memcpy(x.dat.data() + 0x0C + static_cast<size_t>(i) * 32, p.data(), 32);
    }
    return x;
}

}  // namespace

// A replay of the Xbox builds: its blocks made the PC's (each frame its 32 bytes and the PC's last byte, no horn and
// the engine sound 0.5), the header's sizes with them; its cars the PC's records of their model, colours, parts and
// race values (May: no grid; August: its grid and the dyno's runs, the sponsors' decals as recorded).
OJ_UNIT_TEST(ReplayXbox, BlocksAndCars) {
    for (const bool august : {false, true}) {
        XboxFiles x = xboxFiles(august);
        const bool made = oj::Replay::fromXbox(x.header, x.cars, x.dat);
        const std::string build = august ? "the August build" : "the May build";
        check(made && x.header.bytes == oj::Replay::kBlockSize && x.dat.size() == oj::Replay::kBlockSize &&
                  x.header.carsBytes == x.cars.size(),
              build + ": one PC block (0x1EFC), the header's sizes with them");
        if (!made) continue;
        bool frames = true;
        for (int32_t i = 0; i < 2; ++i) {
            const oj::ReplayFrame f = oj::unpackFrame(x.dat.data() + 0x0C + static_cast<size_t>(i) * 33);
            const oj::ReplayFrame want = frameOf(0.04f, i);
            frames = frames && f.time == want.time && f.car == i && f.position[0] == want.position[0] &&
                     f.position[2] == want.position[2] && f.gear == 3 && f.horn == 0 && f.engineSound > 0.49f &&
                     f.engineSound < 0.5f && f.orientation[2] < -0.99f;
        }
        check(frames, build + ": the frames read as the PC's (the place, the turn, the gear), no horn, engine 0.5");
        oj::Replay replay;
        replay.header = x.header;
        replay.carsStream = x.cars;
        const bool read = replay.readCars() && replay.savedCars.size() == 2;
        check(read, build + ": the PC's car records of both cars read");
        if (!read) continue;
        const oj::Car& c = replay.savedCars[0];
        const bool fields = c.modelName() == "EVO8_STOCK" && replay.savedCars[1].modelName() == "TRANS_STOCK" &&
                            c.base == 0xFFEFBD0Eu && c.neon == 0xFF00FFFFu && c.decalColour == 0xFF808080u &&
                            c.sponsorColour == oj::Car().sponsorColour && c.body[0] == 3 && c.mods[4] == 0x38 &&
                            c.lowering == bits(0.8f) && c.gearing == bits(0.5f) && c.nitro == bits(15.0f) &&
                            c.rating == bits(660.0f) && c.brakeWear == bits(0.02f) && c.torqueFactor == bits(1.0f) &&
                            c.steeringPulls == 1;
        check(fields, build + ": model, colours (the sponsors' the PC's), parts, lowering, gearing and race values");
        if (august) {
            check(c.grid.has_value() && c.grid->made[17] && c.dynoRun[0] == bits(-1.0f) && c.sponsorDecals == 1,
                  "the August build: its damage grid, the dyno's runs, the sponsors' decals");
        } else {
            check(!c.grid.has_value() && c.sponsorDecals == oj::Car().sponsorDecals,
                  "the May build: no grid, the sponsors' decals the PC's");
        }
    }
}

// A replay of the PC (its blocks of 0x1EFC, its cars' records) is left as it is.
OJ_UNIT_TEST(ReplayXbox, PcReplayLeft) {
    oj::ReplayHeader h;
    h.bytes = oj::Replay::kBlockSize;
    h.cars = 1;
    std::vector<uint8_t> cars;
    oj::Car car;
    car.setModel("EVO8_STOCK");
    car.write(cars);
    h.carsBytes = static_cast<uint32_t>(cars.size());
    std::vector<uint8_t> dat(oj::Replay::kBlockSize, 0x11);
    const std::vector<uint8_t> carsBefore = cars, datBefore = dat;
    const bool made = oj::Replay::fromXbox(h, cars, dat);
    check(!made && cars == carsBefore && dat == datBefore && h.bytes == oj::Replay::kBlockSize,
          "a PC replay: not the Xbox builds', nothing changed");
}
