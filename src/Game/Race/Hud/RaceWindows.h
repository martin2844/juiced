#pragma once
// The race's HUD (docs/spec/22-race-hud.md): the window state of hud_race and the other hud_* windows (0x0044D9F0,
// vtable 0x006F1438) as it enters (0x004476D0) and as it follows the race each frame (0x0044B1C0), with the race
// front end's messages of each player (0x004560B0).

#include "Game/Race/Hud/HudRace.h"
#include "Game/Race/Hud/RaceHud.h"
#include "Game/Race/Hud/RaceComplete.h"
#include "Game/Race/Hud/RaceMessage.h"
#include "Game/Race/Hud/RaceScreenWindow.h"
#include "Game/Race/Hud/RaceWindowId.h"
#include "Game/Race/Hud/HudReplay.h"
#include "Game/Race/Hud/RacePause.h"
#include "Game/Race/Hud/RaceResults.h"
#include "Game/Race/Hud/ChallengeResults.h"
#include "Game/Race/Hud/CruiseResults.h"
#include "Game/Race/Hud/SprintResults.h"
