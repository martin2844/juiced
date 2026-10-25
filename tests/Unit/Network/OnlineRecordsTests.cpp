// Unit tests: OnlineRecords.

#include "Framework/Test.h"

#include "Core/Network/OnlineRecords.h"

namespace {

using oj::test::check;

}  // namespace

// The online ratings' steps (0x004EC980 over 0x00725630).
OJ_UNIT_TEST(OnlineRecords, RatingSteps) {
    using oj::net::stats::ratingAfter;
    check(ratingAfter(1, 10000, 10000) == 9995 && ratingAfter(-1, 10000, 10000) == 10005 &&
              ratingAfter(0, 10000, 6000) == 10010 && ratingAfter(1, 3, 20000) == 3 && ratingAfter(1, 5, 0) == 0 &&
              ratingAfter(-1, 10000, 11000) == 10008,
          "online ratings: a race's steps by the difference");
}

// The fixed build: a table whose fields ran out early (OpenSpy's answer of the leaderboards, "1 0 0 0 CHC-Juiced": a
// rank, a name, a score and no car for top showoff scores) has no row; the stock read the car's column past its end,
// at address 0 (online_stats closed the game).
OJ_UNIT_TEST(OnlineRecords, ShortTableHasNoRow) {
    oj::net::stats::TableReader reader;
    static const char16_t name[] = u"0";
    reader.done = 1;
    reader.ranks = {0};
    reader.names = {name};
    reader.columns = {{0}, {}, {}};
    int32_t rank = 0, points = 0, car = 0, total = 0;
    const char16_t* got = nullptr;
    const bool none = !reader.showoffRow(0, rank, got, points, car, total);
    reader.columns[1] = {7};
    const bool whole = reader.showoffRow(0, rank, got, points, car, total) && car == 7 && got == name;
    check(none && whole, "online records: a table's row only where every column read has it");
}
