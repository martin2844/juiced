// Unit tests: Route.

#include "Framework/Test.h"

#include "Core/Tracks/Track.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// A seventh GRIDPOINT (0x004A1410): the stock writes its x over the count (an x of 0 cuts the grid to one point);
// the fixed build keeps the six and passes the seventh and later over.
OJ_UNIT_TEST(Route, SeventhGridPoint) {
    const std::string six = "GRIDPOINT 1 2 3 4\nGRIDPOINT 5 6 7 8\nGRIDPOINT 9 10 11 12\nGRIDPOINT 13 14 15 16\n"
                            "GRIDPOINT 17 18 19 20\nGRIDPOINT 21 22 23 24\n";
    const oj::Spl zero = oj::parseSpl(six + "GRIDPOINT 0 30 31 32\nGRIDPOINT 40 41 42 43\n");
    const oj::Spl other = oj::parseSpl(six + "GRIDPOINT 25 26 27 28\n");
    check(zero.grid.size() == 6 && zero.grid[0].x == 1.0f && zero.grid[0].y == 2.0f && zero.grid[5].x == 21.0f &&
              other.grid.size() == 6 && other.grid[5].x == 21.0f,
          "spl (fixed): the six grid points kept, a seventh and later passed over");
}
