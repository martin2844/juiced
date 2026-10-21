// Unit tests: Script.

#include "Framework/Test.h"

#include "Core/Base/Script.h"

namespace {

using oj::test::check;

}  // namespace

// Text scripts: keywords, plain and quoted words, comments, a line going on after " ~".
OJ_UNIT_TEST(Script, Lines) {
    const auto script = oj::parseScript("; comment\r\n  [BeginCar] focus_2 \"2004 FOCUS ZTS\"\r\nVALUE 19995 ~ x\n 7\n\nSTATE NEW\n");
    check(script.size() == 3 && script[0].keyword == "[BeginCar]" && script[0].word(1) == "2004 FOCUS ZTS" &&
              script[1].keyword == "VALUE" && script[1].words.size() == 2 && script[1].word(1) == "7" &&
              script[2].word(0) == "NEW",
          "script lines: keyword, quoted words, comments, continuation");
}
