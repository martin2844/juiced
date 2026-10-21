# The front end's code (`src/Game/Frontend`)

How the port's front end is put together, for someone reading the code for the first time. What the windows do is
in `docs/spec` (02-frontend, 03-saves, 14-career-windows, 28-network, ..); this page is about where that lives and how
the parts talk. The windows over a race are `race-frontend.md`.

## The pieces

- **`Frontend`** (`Frontend.h`) is one of the stock's front ends: the game's, the loading phase's, the race's options'.
  It owns the window manager, the current window (`m_screen`), the windows over it (`m_popups`), the message window,
  the switch between windows (`request`), the music and the interface's sounds.
- **A window** is a class in `oj::frontend`, one per stock window record, in a folder of its family:
  `Startup`, `Loading`, `Menus`, `Saves`, `Profile`, `Options`, `Garage` (`Customise`), `Career`, `CustomRace`,
  `Arcade`, `Network`, `Unused` (windows the PC game never opens). A few windows that read and write each other's
  records are one class: `HostMeeting` (the three hosting windows), `GridWalk` (the grid and the bet over it),
  `MobilePhone` (the phone's four windows).
- **A host** is the interface a window gets instead of the front end. Windows never include `Frontend.h`.

## A window

```cpp
class CareerMenu {
public:
    void enter(CareerHost& host, ui::Screen& s);                   // the record's enter
    void update(CareerHost& host, ui::Screen& s, uint32_t event);  // its frame, with the window's event
private:
    void topUp(CareerHost& host, Career& career);                  // its own helpers
    int32_t m_leaving = 0;  // +0x0C                                 the record's fields, by their offsets
    int32_t m_answer = -1;  // +0x10
};
```

- The fields are the stock record's, named, with the offset in the comment. Where the stock clears its record the
  class does `*this = CareerMenu{};`.
- `enter`, `update`, `leave`, `draw` are the record's methods; a parameter a method does not need is left out
  (`leave()`, `update(host, event)`).
- `s` is the window's own screen: the base window's, or a pop-up's. A pop-up that writes into the window under it
  uses `host.baseScreen()`.
- Each method's comment starts with the stock address it ports; what the stock gets wrong and the port keeps is
  marked "Stock problem".

## Hosts

`WindowHost` (`WindowHost.h`) is what every window asks of the front end: the switch (`request`, `switchAsked`), the
message window (`message`, `messageUp`, `closeMessage`, `messageNumber`), the sounds, the game's data and texts,
the language, the 3D engine, the frame times, the driver's settings, the music and the shared state.

A family adds only what it needs, in its folder:

| host | adds | file |
|---|---|---|
| `OptionsHost` | the options' menu, the race's pause and its second player, the effects' volume, the controls, the details, the playlist | `Options/OptionsHost.h` |
| `MainMenuHost` | quitting the game | `Menus/MainMenuHost.h` |
| `ProfileHost` | the name filter, the profile, the driver's name, the name window's setup | `Profile/ProfileHost.h` |
| `SaveHost` (+ Profile) | the save device's side and texts, the volumes, the network's delayed call | `Saves/SaveHost.h` |
| `GarageHost` | the garage's shared object (`Garage`) and the stats windows | `Garage/GarageHost.h` |
| `CareerHost` | the respect message, the phone's frame, the way to a meeting, the hosted meeting | `Career/CareerHost.h` |
| `NetworkHost` | the network game's waits, keys, hooks, joins | `Network/NetworkHost.h` |
| `OnlineHost` (+ Network, Profile) | the voice window | `Network/OnlineHost.h` |
| `CustomRaceHost` (+ Network) | the second player's device | `CustomRace/CustomRaceHost.h` |
| `StartupHost` | the window's countdown, the keyboard let go | `Startup/StartupHost.h` |

`Frontend` implements them all (privately). The hosts derive from their bases virtually, so a method two hosts
declare with the same signature is one override in `Frontend`.

A window that needs something new gets a method on the narrowest host, implemented by `Frontend`; the comment says
what the stock does there.

## Where the windows are wired

- **`Frontend.h`** holds the window objects. `static inline` ones stand for the stock's records, which are globals and
  outlive the front ends; plain members (`m_splash`, `m_skippable`, `m_character`, `m_postRace`, `m_cutScene`,
  `m_rolling`) are state each front end has its own of. Keep the lifetime of the stock's: it shows when a front end is
  made again (the loading phase, the race's options).
- **`Frontend.cpp`**: `enterState`, `updateState`, `leaveState` call the window of the current state by its name;
  `releaseRecords` frees what the records hold as the game ends.
- **`Popups.cpp`**: the windows over a window (mode 5). The online windows over `online_bg` run with the front end's
  current screen swapped to theirs (`onPopupScreen`), as the stock code reads them.
- **`FrontendShared.cpp`**: the windows that run either as the base window or over one (`opts_voice`, the cut ones).
- **`FrontendDraw.cpp`**: the windows' draws, the notes, the screen's fade.

## What stays the front end's

Code that is the front end's own and not a window's: `FrontendAudio.cpp` (music, sounds, speech),
`FrontendCareer.cpp` (help pages, the career's next window after a race, the way to a meeting, the phone's frame),
`FrontendNetwork.cpp` (the network game's hooks, waits, keys, the session's loss), `FrontendOnline.cpp` (sessions
hosted and joined), `FrontendSaves.cpp` (the save device's side), `Messages.cpp`, `Controllers.cpp`, `WindowBase.cpp`
(what most windows do on entering and leaving).

## Shared state

- `SharedState` (`host.shared()`): the stock's globals several windows read and write (the tutorial's way back, the
  autosave asked, the online command line and forms).
- Window to window goes through the host (`host.garage()`, `host.carStatsPurple()`, `host.hostMeeting()`,
  `host.onlineVoice()`), never into another window's fields.
- `WindowTools.h`: helpers on a screen (`hideAll`, `hideNamed`, `routeNameLine`, ..) and the few globals of the stock
  the windows share (`g_selectedDay`, `g_selectedMonth`, `g_statsCar`).

## Adding a window

1. A class in its family's folder: the record's fields, `enter` / `update` / `leave` as the stock has them.
2. What it needs of the front end: the narrowest host (a new method there, implemented in `Frontend`).
3. Its object in `Frontend.h`, `static inline` unless the stock keeps it per front end.
4. Its calls in `enterState` / `updateState` / `leaveState` (`Popups.cpp` for a window over a window).
5. Its files in `src/Game/OpenJuice.Game.vcxproj` and `.filters`.
6. A test of what it does (below).

## Testing a window

`tests/Integration/Frontend` runs windows without the game's front end, over the game's own `.ui` files and texts:

```cpp
class FakeStartupHost : public oj::test::FakeWindowHost, public oj::frontend::StartupHost {
public:
    float timer = 0.0f;
    float windowTimer() const override { return timer; }
    void setWindowTimer(float seconds) override { timer = seconds; }
    // .. the rest of StartupHost
};

OJ_INTEGRATION_TEST(Startup, DisclaimerWaits) {
    oj::test::InGameFolder folder;        // the game's folder the current directory, as the game runs
    if (!folder.valid()) return;          // no game: the test is left out
    FakeStartupHost host;
    const auto s = host.open("disclaimer");  // ui\windows\disclaimer.ui and its texts, over a NullRenderDevice
    if (!s) return;
    oj::frontend::Disclaimer window;
    window.enter(host, *s);
    host.timer = -0.01f;
    window.update(host);
    check(host.states() == std::vector<std::string>{"legals"}, "the disclaimer goes on to legals");
}
```

- `FakeWindowHost` (`tests/Framework/Fakes`) implements `WindowHost`: what the window asks is kept (`requests`,
  `messages`, `sounds`, `closedMessages`), what it reads is set by the test (`data`, `texts`, `dt`, `sharedState`,
  `messageOpen`, `switchPending`). A message's answer is given through its pointer (`*host.messages[0].answer = 1`).
- A family's fake adds its host's methods, as `Frontend` implements them all.
- The tests link `OpenJuice.Game.lib`; a window's test needs nothing of `Frontend`.

## Names

- A class in `oj::frontend` must not take the name of a type of `oj`: inside `oj::frontend` it would hide the game's
  type (`Calendar`, `RaceSetup`, `Crew` and `Phone` are the career's, hence `CalendarWindow`, `RaceSetupWindow`,
  `CrewWindow`, `MobilePhone`). The same goes for a helper in a file's anonymous namespace: it hides `oj`'s function of
  that name (the network files' `widenSigned(const std::string&)` over `oj::widenSigned(std::string_view)`).
- A method is not named as a local of its bodies (`view3d`, `fillList`, `fillRows`).
