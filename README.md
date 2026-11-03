# OpenJuice native ports

Native macOS, Linux/NixOS and Android ports of **[OpenJuice](https://gitlab.com/sidecr3w/openjuice), created by Sidewinder (sidecr3w)**. This repository contains platform adaptations and integration work by martin2844 and contributors. The engine reconstruction is upstream OpenJuice's work.

Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/openjuice)

The current source integrates upstream **1.02.7** (`b23976cf`) across macOS,
Linux/NixOS and Android, including native Metal/GL effects, our car packs,
liveries, career mods and SDL controls. The main branch contains the shared
native-port code; the older platform branches remain available as history.

The [3 November validation checkpoint](docs/native-effects-device-validation-2026-11-03.md)
records 18 GPU checks on the physical Retroid Pocket 5 and the Apple Silicon Mac,
day/night races, a signed Android release build, installed Mac applications and
save-preservation checks. See the [upstream integration notes](docs/upstream-sync-2026-11-02.md)
for the source import. This source update does not replace existing release assets.

## Downloads

Development builds. Compatibility and performance vary by device; no frame-rate target is guaranteed. Supply your own Juiced game data; game data is not bundled.

The Android and Linux downloads below are older published builds. They do not
contain all changes in the current 1.02.7 source. Build the current source using
the platform guides below.

| Platform | Build | Download / requirements |
| --- | --- | --- |
| Android ARM64 | OpenJuice 1.02.5 · android.4 | [APK and optional mods](https://github.com/martin2844/juiced/releases/tag/android-v1.02.5-4) · Android 8+ |
| Linux x86_64 / NixOS | OpenJuice 1.02.5 · mods1 | [Nix binary package](https://github.com/martin2844/juiced/releases/tag/linux-v1.02.5-mods1) · Nix and OpenGL drivers required |
| macOS Apple Silicon | Current source: OpenJuice 1.02.7 · app 1.2.7 | [Build and package locally](docs/macos-build-and-run.md) · validated locally; current app download not published |

Android: install the APK and import your own `Juiced.zip` in the launcher. Import optional mods separately. Physical controllers and customizable touch controls are available; touch controls can be disabled.

Linux: follow the included instructions. The download is a Nix closure, not a standalone AppImage.

macOS: compile locally on a Mac. The packaging helper bundles non-system libraries
and applies an ad-hoc signature; it does not notarize the app. The regular app
runs the base game, and **Juiced Mod Garage** loads the editable mod collection.

## Source and documentation

- [macOS build instructions](docs/macos-build-and-run.md) and [port status](docs/macos-port.md).
- [Linux/NixOS guide](docs/linux-port.md) and [Android guide](docs/android-port.md).
- [Current device validation and checkpoints](docs/native-effects-device-validation-2026-11-03.md).
- [Upstream source](https://gitlab.com/sidecr3w/openjuice) and [upstream releases](https://gitlab.com/sidecr3w/openjuice/-/releases).

The older `android` branch and rain-fix release remain as history. Current
macOS, Linux and Android sources are consolidated on `main`.

## License and credits

OpenJuice uses the [PolyForm Noncommercial License 1.0.0](LICENSE.md). Preserve the upstream copyright notice when redistributing. Dependencies retain their own licenses. Juiced and its game assets belong to their respective rights holders.
