// Unit tests: Route.

#include "Framework/Test.h"

#include "Core/Tracks/Track.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// A seventh GRIDPOINT (0x004A1410): its x over the count, y, z and heading into the slot of x's bits, the
// count x's bits + 1; an x of 0 makes the first point's y, z, heading its own and leaves one point, another x
// is dropped here.
OJ_UNIT_TEST(Route, SeventhGridPoint) {
    const std::string six = "GRIDPOINT 1 2 3 4\nGRIDPOINT 5 6 7 8\nGRIDPOINT 9 10 11 12\nGRIDPOINT 13 14 15 16\n"
                            "GRIDPOINT 17 18 19 20\nGRIDPOINT 21 22 23 24\n";
    const oj::Spl zero = oj::parseSpl(six + "GRIDPOINT 0 30 31 32\nGRIDPOINT 40 41 42 43\n");
    const oj::Spl other = oj::parseSpl(six + "GRIDPOINT 25 26 27 28\n");
    check(zero.grid.size() == 2 && zero.grid[0].x == 1.0f && zero.grid[0].y == 30.0f &&
              zero.grid[0].z == 31.0f && zero.grid[0].heading == 32.0f && zero.grid[1].x == 40.0f &&
              other.grid.size() == 6 && other.grid[5].x == 21.0f,
          "spl: a seventh grid point of x 0 goes to the first slot and leaves one point, another is dropped");
}
