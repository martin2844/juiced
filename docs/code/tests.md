# The tests (`tests`)

How the tests are written, what they need and how to run them. They build into `OpenJuice.Tests.exe` beside the
game's executables, linking the same libraries (`README.md`).

## Running

```
OpenJuice.Tests.exe [save folder] [--game <folder>] [--unit | --integration] [--suite <name>] [--list]
```

- `--game` names an installed Juiced (packed or not). Without it the tests of the game's data return at once and
  check nothing: the last line then counts fewer checks.
- The save folder is the player's (`%APPDATA%\THQ\Juiced`) unless one is given; the tests only read it.
- `--unit` or `--integration` runs one kind, `--suite` one suite, `--list` names the tests. The last line counts the
  checks passed and failed; the exit code is 1 when one failed.

Every configuration (Release and Debug, x64 and x86) runs them all; a change is done when the four pass. Each run
also checks the game's executable of its own configuration beside it (`Windows7.Executable`,
`tests/Integration/Platform/Windows7Tests.cpp`): its header and every function it imports must be Windows 7's. A new
import fails it with its name; one of Windows 7 gets its entry in the test's list (the compiler checks the name
against the SDK's headers for Windows 7), one of a later Windows a way round (`app.md`, "The platform"). The
`DirectSound` suite needs a sound device: it checks the values Windows' DirectSound refuses that the stock's 3D voices
ask for (`docs/spec/26-sound.md`). The `DirectInputKeyboard` suite checks that DirectInput's default
keyboard map gives the controls file's keys (no user's map on the machine).

## Writing one

```cpp
#include "Framework/Test.h"

#include "Core/Career/Money.h"

namespace {

using oj::test::check;

}  // namespace

// Money as the career's windows show it: thousands marked with "," ("." in French and Italian), "$" first
// ("<amount> $" in French).
OJ_UNIT_TEST(Money, Texts) {
    check(oj::groupThousands(40000, 7) == "40,000" && oj::moneyText(2000, 7) == "$2,000" &&
              oj::moneyText(2000, 1) == "2.000 $",
          "money texts: thousands marks and the dollar sign by language");
}
```

- **A unit test** (`OJ_UNIT_TEST`, `tests/Unit/<area>/`) checks one part with the data it makes itself: a format's
  reader on bytes it writes, the race's state on a route it builds, the physics on flat ground.
- **An integration test** (`OJ_INTEGRATION_TEST`, `tests/Integration/<area>/`) checks parts together: against the
  game's files, the save device in a folder, the network on the loopback with stand-ins of GameSpy's servers, a
  window over a fake host.
- `check(ok, what)` prints PASS or FAIL with the text and counts it; a test goes on after a failed check.
- A file is one area's tests, in the area's folder named as the source's (`tests/Unit/Race/RaceStateTests.cpp`).
  A new file goes in `tests/OpenJuice.Tests.vcxproj` and its `.filters`.

## Fixtures and fakes

- **`Framework/Fixtures.h`**: what the integration tests share, made once: the game's folder and its files
  (`gameFiles()`), `InGameFolder` (the current directory the game's while it lives, as the game runs), the game's
  data (`sharedGameData()`: the built-in tables, the car models, the parts, the dynamics), the save folder and the
  cars of its saved careers.
- **`Framework/Fakes/`**:
  - `NullRenderDevice.h`: a render device that draws nothing, for windows and scenes without a screen;
  - `FakeWindowHost.h`: the front end's `WindowHost`, keeping what a window asks (`frontend.md` shows a window's test);
  - `FakeGpcm.h`: a stand-in of GameSpy Presence's connection manager;
  - `FlatGround.h`: a collision of flat ground for a car's physics.

A test that needs the game starts with its fixture and returns without it:

```cpp
OJ_INTEGRATION_TEST(Tracks, EveryTrack) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;  // no game: left out
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    ...
}
```

## Against the stock game

The tests check what can be checked without the stock game. What only the stock game shows (a race's frames, a
window's look, the values `rand()` gives a career) is compared by running both: the debug build's scripted runs
(`--test --frames --seed --capture`, `app.md`) beside the stock game under a debugger, and the values a test expects
are the ones read there.
