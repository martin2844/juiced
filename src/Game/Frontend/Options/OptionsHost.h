#pragma once
// What the options windows (opts_menu, opts_game_pc, opts_audio, opts_songlist, opts_graphics, opts_wheel_dx9 and
// their in-race copies) ask of the front end besides what every window does.

#include "Core/Saves/GraphicsConfig.h"
#include "Game/Frontend/WindowHost.h"
#include "Render/RenderDevice.h"

#include <cstdint>

namespace oj::frontend {

class OptionsHost : public virtual WindowHost {
public:
    // The options' menu the windows go back to: opts_ingame_menu from a race's pause, else opts_menu.
    virtual const char* optionsMenu() const = 0;
    // The options run from a race's pause ([game + 0x3C]).
    virtual bool racePause() const = 0;
    // The second player's pause of a race of more players, and that player's event this frame (the window's +0x8C).
    virtual bool secondPlayerPaused() const = 0;
    virtual uint32_t secondPlayerEvent() const = 0;
    // The interface's effects (0x005A89F0).
    virtual void setEffectsVolume(float volume) = 0;
    // 0x004080D0: the controls' set up (DirectInput's mapping window).
    virtual void controlsSetUp() = 0;
    // The interface's 3D renderer's detail factor ([0x0079E670]), when there is one.
    virtual void setDetailFactor(float lodBias) = 0;
    // opts_playlist's chosen playlist back to its own ([0x007688F8], [0x007688FC] from +0x24, +0x28).
    virtual void restorePlaylistChoice() = 0;
    // The fixed build's effects (GraphicsConfig::effects) changed in the graphics options' ADVANCED page: kept as the
    // registry's values for the next start.
    virtual void keepEffects(const FixedEffects& effects) { (void)effects; }
    // The same in effect now: the heat haze's strength, the anti-aliasing and the texture filter; with `programs` the
    // programs made again for the car paint and the shaders the device replaces (the world light, the headlights).
    virtual void applyEffects(const FixedEffects& effects, bool programs) {
        (void)effects;
        (void)programs;
    }
    // The anti-aliasing and texture filter the adapter offers (the device's).
    virtual render::QualityCaps qualityCaps() const { return {}; }
    // The fixed build's EAX (the audio options' row): on now (the sound system's flags), switched while the game runs
    // (audio::System::enableEax), kept in the registry's audioflags (0x20 off).
    virtual bool eax() const { return false; }
    virtual void setEax(bool on) { (void)on; }
    virtual void keepEax(bool on) { (void)on; }
};

}  // namespace oj::frontend
