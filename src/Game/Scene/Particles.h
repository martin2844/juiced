#pragma once
// The particles of a race (the effects manager [0x0075F0C8], docs/spec/20-effects.md, "The particles"): the emitters
// made from the race's effect definitions, their particles stepped and drawn in the world's main pass after the lens
// flares (0x00505030), the wind the cars stir (0x00505740) and the particles' own generator (0x00509B70).

#include "Game/Scene/Particles/Emitter.h"
#include "Game/Scene/Particles/SmokeEmitter.h"
#include "Game/Scene/Particles/SparksEmitter.h"
#include "Game/Scene/Particles/TracksEmitter.h"
#include "Game/Scene/Particles/CameraEmitter.h"
#include "Game/Scene/Particles/SmearEmitter.h"
#include "Game/Scene/Particles/LeavesEmitter.h"
#include "Game/Scene/Particles/ParticleSystem.h"
