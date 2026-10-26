# Hosted pink slips — 2026-10-26

## Delivered

Optional `mods/host-pink-slips` API-1 script enables the native
`host_pink_slips` feature. Android 1.02.5-android.7 (versionCode 8) contains
the engine hook and launcher description. Source hook is shared with Linux/macOS;
their binaries have not been rebuilt for this change.

Career calendar → empty future date → host event → Meet Type, right past
Sprint → PINK SLIPS. Opponent selection contains only crews with respect >=600
and a car satisfying the selected class/make/region. The player also needs an
eligible car and room below the 55-car garage capacity. Race setup can change
opponent, physical race type, route and laps. One round, one player and one rival;
no cash entry fee, hosting cost or cash prize. Confirmation explicitly warns
that the selected car is at stake.

Eligibility is rechecked at commit. Empty/full-calendar conflicts cannot create
this event. This uses the original pinkSlip/bets/settlement path, not the
hostedByPlayer flag: that flag means a host-rights challenge in the engine.
Regular hosting requirements remain intact. Disabling the package removes new
creation while existing scheduled pink slips retain ordinary serialized semantics.
This script does not change the catalogue/save variant or grant respect.

## Validation

Host target `android_host_pink_check`, using real retail data/screens and an
isolated in-memory career:

- 599 excluded, exactly 600 included; cyclic eligible rival selection.
- No qualifying rival prevents advancing.
- Full garage and missing player car prevent the wager.
- Respect dropping after confirmation prevents commit without charging money.
- Two racers, the selected rival, no host-rights challenge or cash charge.
- Event serializes and reloads through the existing career format.
- Native setupRace creates two racers.
- Native pinkSlipCar win/loss settlement adds/removes a car.
- Regular location/team/prize captions restored after leaving pink mode.
- Disabling the package restores ordinary type choices.

Reproduce from repository root:

```sh
nix-shell shell.nix --run 'cmake --build out/android-host --target android_host_pink_check -j 8'
out/android-host/android_host_pink_check ../game-data mods
# Optional fourth argument: absolute output directory for screenshots.
```

Screenshots inspected at `out/validation/android/pink-ui/pink-{setup,race,summary}.png`.
Harness renders the actual retail screens with GLES, without a 3D map or controller
texture bindings. These harness limitations explain the empty map panel and
blank button-icon squares; the new labels fit their existing fields.
A first visual run used an inappropriate ES context for the desktop shader path;
using the desktop core context resolved that harness/driver failure.

Android debug and signed release builds passed. RP5 debug smoke launched the
native game and logged `runtime mod loaded: host-pink-slips/main.lua`; signed
release .7 then installed successfully. Package copied to both internal mods
and `/sdcard/Documents/OpenJuice/mods/host-pink-slips`, enabled. Launcher left open.
A complete manually driven pink-slip race has not been tested on the RP5;
settlement validation above exercises native code with the isolated fixture.

Actual RP5 ModSaves archives before/after testing are byte-identical:
`3c1c4241d4bfade99e8a08e1da05845c8ffcf82f37e9cf5e32573ac151cc4670`.
No real profile was used for the fixture or modified.

## Local deliverables

- `out/releases/android7/OpenJuice-1.02.5-android.7-arm64.apk`
  SHA256 `191f9572e00d3f3a7be29775e8ae2de6d566cc781d264330b12616db57306a87`
- `out/releases/android7/OpenJuice-Host-Pink-Slips.zip`
  SHA256 `11f77b7ece44ba01a67765ea4330211e32ec36aa8ca872a984f4637969716d34`
- Build/test/device logs in `out/validation/android/pink-*.log`.
- Save backups `out/validation/android/{pre,post}-pink-saves.tar`.

The APK requires the package as well as this native version. The small zip can
be imported from the launcher's Mods tab on other devices. Existing APK versions
do not implement the new script feature. No new public GitHub release created.
