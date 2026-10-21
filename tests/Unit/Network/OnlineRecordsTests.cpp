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
