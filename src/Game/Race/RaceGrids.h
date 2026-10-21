#pragma once
// The race's grids as the game's load makes them (0x00495DE0 by the race's mode) and the window that follows a race.

#include "Core/Data/GameData.h"
#include "Core/Network/NetSession.h"
#include "Core/Race/RaceSession.h"
#include "Core/Race/Replay.h"
#include "Game/Frontend/Frontend.h"
#include "Game/Race/Hud/RaceWindows.h"
#include "Game/Race/TrackView/TrackView.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <string>

namespace app {

// 0x00495DE0 for a custom race (modes 1 and 3; off line, not a replay): the session of the players and opponents
// (0x004B9150, then the drivers 0x004BDCC0); the opponents automatically modified when the player's car has a catalogue
// part (the first player's). Each player's car is its arcade car (its save system object's +0x50): its grid made
// (0x005562A0), rated (0x0048E8A0), on grid place i as a player's car of crew 0, its competitor named by the driver.
// [0x0075EC98] (only cleared, by start's enter) would take the group by the place: 0 3, 1 4, 2 6. Each opponent k: a
// new car (0x0048DB40) of a car id drawn from the chosen group (rand() % 60 until not empty) with the CarDef of the
// first arcade car of that model (the first arcade car's when none); with the modification: as a prototype (0x0048DAE0:
// the player's car has a prototype part) of any level, else of level rand() % 3 (0x0048F2C0); a paint of the CarDefs'
// list (rand() % its size); rated, and drawn again (20 times at most in all) while its rating passes the last
// player's by more than 1; on the place after the players as an AI car of crew and number players + k, its competitor
// named by the short name of the first member of crew k + 1 ([career + 0x0C + 4 k] + 0xDE). The loading thread's rand(). The stock
// car keeps its name (+0x14); here the session's racer does (the HUD reads it there).
void customGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine);

// 0x004269A0, the race's part: the race a replay's (the replay's play has set it), the header's type (a sprint's heat
// 0 of 3, of 1 in a network game), laps, track, route, time of day and weather; the race's mode is left as it is.
void setReplayRace(oj::RaceSetup& race, const oj::ReplayHeader& h);

// 0x00461390 for a replay's race (mode 4): the replay reads its files (0x00429720: <name>.hdr and <name>.dat) and plays
// from the start; its header gives the race (0x004269A0). False when a file does not read.
bool loadReplayRace(TrackView& view, oj::GameData& data);

// 0x00495DE0 on the network (in place of every mode's): the session of the players in the race and not gone
// (0x004B8C90), the race's state with the session's laps ([0x0075EFE0]); each such player a new car (0x0048DB40) of
// the CarDef of its config's name ("_STOCK" added, the first CarDef when none, 0x0048CC30) with its config applied
// (0x0048E7D0), its grid made, on its grid place (+0xC4) as ours of index 0 (kind 0), ours of another index (the AI,
// 1) or another machine's (2: Stock problem, kind 0 for a player before the race's player count, 1 here, whoever's it
// is), crew 0, the k-th car made; the player's car (+0xF0) it. The session's hooks on the race's cars.
void netGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine, oj::net::NetSession& session);

// 0x00495DE0 for a replay's race: each car the replay read (0x00426A50; its grid object made when it has none,
// 0x005562A0) on the grid place of its number, all driven by the replay; the first player's camera on the first car.
void replayGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine);

// 0x00495DE0 for an arcade race (mode 0, off line, not a replay): the session of the race's entries (0x004B93C0), the
// challenge's outcome (+0x7C) 0, the race's cars made (0x0042B000, the loading thread's rand()); each entry's car on
// grid place k as the player's (kind 0) or an AI's, of crew its number, its competitor named by the short name of the
// first member of that crew ([career + 0x08 + 4 x number] + 0xDE). The AI drivers' skill and the nitro by the race's
// +0x24 and +0x04 (the race's rules). The stock car keeps its name; here the session's racer does.
void arcadeGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine);

// 0x00461A10: the front end's first window after a race, by the race's mode: an arcade race Arcade_Race (Arcade_Results
// while any of the arcade's news +0x53C8 .. +0x53D4 is set), a custom race instant_manu, a split screen
// race multi, mode 4 start; a career race: the dealer's +0x10 set; type 9 garageload (the dealer's +0x0C and the
// career's +0xA4 cleared); else the session's end (0x004A3A40: 0x004BA8A0, 0x004B9E20) and
// by the current event: a cruise (type 2) the event's end (0x004A6870) and CMSMeetingSummary while the career's +0x38
// list has entries, CMSRespectChanges while the +0x3C list has, else the history's step (0x004BD8A0) and CMSMainMenu;
// a challenge (+0x05) CMSChallengeResults, else CMSRaceResults. In a network game (0x00461E71, 0x00462028) an arcade or
// a custom race goes to link_lobby while the session has players (0x004EA000), else link_search (online_main
// online). Before all that a window named by [0x0075ED18] (a saved replay's read sets "menu"; the network's quits
// set it in the stock game, not here), emptied.
std::string windowAfterRace(oj::GameData& data);

}  // namespace app
