#pragma once
// The crew files of the career (docs/spec/08-career.md): CMSCrews.txt, CMSCrewmates.txt and CMSCrewChallenges.txt,
// read into the career's objects as the stock game reads them.

#include "Core/Career/Career.h"
#include "Core/Data/ExeTables.h"

#include <array>
#include <string_view>

namespace oj {

// CMSCrews.txt for one crew (after its clearing, 0x004A7240): the lines between [BeginCrew] <the crew's id> and
// [EndCrew]; repeated keys fill the next slot.
void readCrewFile(std::string_view text, Crew& crew, const ExeTables& tables);

// CMSCrewmates.txt for one crewmate (0x004A9CE0). currentMate is the crewmate number the reading starts with (the
// stock game keeps it between readings: 0 before the first, -1 after an [EndCrewmate]).
void readCrewmateFile(std::string_view text, Crewmate& mate, int& currentMate);

// CMSFlyers.txt (0x004B2C20): Crew takes the next crew (from 0), FontColour r g b its colour, PlayerFontColour r g b
// the player's next colour, Logo <texture> its logo.
Flyers readFlyersFile(std::string_view text);

// CMSCrewChallenges.txt (0x004A7650) into the crews.
void readChallengeFile(std::string_view text, std::array<Crew, 9>& crews, const ExeTables& tables);

}  // namespace oj
