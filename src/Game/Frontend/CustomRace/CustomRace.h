#pragma once
// The custom race's windows (docs/spec/02-frontend.md, "The custom race"): instant_manu, instant_selcar,
// instant_seltype, instant_setup and split_selcar, each in its own files.

#include "Core/Race/Racer.h"
#include "Game/Cars/CarSound.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/Career/CityMap.h"
#include "Game/Frontend/CustomRace/CustomRaceHost.h"
#include "Game/UI/UiScreen.h"
#include "Game/Frontend/CustomRace/InstantManu.h"
#include "Game/Frontend/CustomRace/InstantSelCar.h"
#include "Game/Frontend/CustomRace/InstantSelType.h"
#include "Game/Frontend/CustomRace/InstantSetup.h"
#include "Game/Frontend/CustomRace/SplitSelCar.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
