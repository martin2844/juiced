// Unit tests: Money.

#include "Framework/Test.h"

#include "Core/Career/Money.h"

namespace {

using oj::test::check;

}  // namespace

// Money as the career's windows show it: thousands marked with "," ("." in French and Italian), "$" first
// ("<amount> $" in French).
OJ_UNIT_TEST(Money, Texts) {
    check(oj::groupThousands(40000, 7) == "40,000" && oj::groupThousands(999, 0) == "999" &&
              oj::groupThousands(1234567, 3) == "1.234.567" && oj::groupThousands(-1500, 0) == "-1,500" &&
              oj::groupThousands(0, 0) == "0" && oj::moneyText(2000, 7) == "$2,000" &&
              oj::moneyText(2000, 1) == "2.000 $",
          "money texts: thousands marks and the dollar sign by language");
}
