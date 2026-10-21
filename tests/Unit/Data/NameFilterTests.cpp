// Unit tests: NameFilter.

#include "Framework/Test.h"

#include "Core/Data/NameFilter.h"

namespace {

using oj::test::check;

}  // namespace

// The name filter: a word of the list refuses a name unless an exception is in it too.
OJ_UNIT_TEST(NameFilter, RefusesListedWords) {
    oj::NameFilter filter;
    filter.load("Anal\r\nLordz\r\n", "Canal\r\n");
    check(filter.refuses(L"Anal1") && filter.refuses(L"x-LORDZ") && !filter.refuses(L"Canal") &&
              !filter.refuses(L"Player"),
          "name filter: words, exceptions, letters and digits only, any case");
}
