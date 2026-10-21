#pragma once
// Whether the sound card gives EAX through eax.dll, as the stock JuicedConfig.exe asks it (0x004058D0, 0x00405550).

namespace oj::config {

// 0x004058D0: eax.dll loaded (the current folder's, never freed) and its EAXDirectSoundCreate8 asked for a device (never
// released); with one, the probe (0x00405550) leaves the device's EAX flags, and the answer is their bit 0x10 (each
// voice's own EAX). Stock problem: each call loads the library and makes a device again; the window asks as it starts,
// as each tab comes in and as the check boxes are set.
bool eaxAvailable();

}  // namespace oj::config
