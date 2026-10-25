# OpenJuice native ports

Native macOS, Linux/NixOS and Android ports of **[OpenJuice](https://gitlab.com/sidecr3w/openjuice), created by Sidewinder (sidecr3w)**. This repository contains platform adaptations and integration work by martin2844 and contributors. The engine reconstruction is upstream OpenJuice's work.

Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/openjuice)

## Downloads

Development builds. Compatibility and performance vary by device; no frame-rate target is guaranteed. Supply your own Juiced game data; game data is not bundled.

| Platform | Build | Download / requirements |
| --- | --- | --- |
| Android ARM64 | OpenJuice 1.02.5 · android.4 | [APK and optional mods](https://github.com/martin2844/juiced/releases/tag/android-v1.02.5-4) · Android 8+ |
| Linux x86_64 / NixOS | OpenJuice 1.02.5 · mods1 | [Nix binary package](https://github.com/martin2844/juiced/releases/tag/linux-v1.02.5-mods1) · Nix and OpenGL drivers required |
| macOS Apple Silicon | Port 0.3.3 · OpenJuice 1.01.4 | Build pending: [release workflow](https://github.com/martin2844/juiced/actions/workflows/macos-release.yml) · Homebrew FFmpeg required |

Android: install the APK and import your own `Juiced.zip` in the launcher. Import optional mods separately. Physical controllers and customizable touch controls are available; touch controls can be disabled.

Linux: follow the included instructions. The download is a Nix closure, not a standalone AppImage.

macOS: install FFmpeg with `brew install ffmpeg`, extract the app and select your game folder. The app is ad-hoc signed, not notarized. This build uses a different upstream base from Linux and Android.

## Source and documentation

- [macOS source](https://github.com/martin2844/juiced/tree/main), [build instructions](https://github.com/martin2844/juiced/blob/main/docs/macos-build-and-run.md), [port status](https://github.com/martin2844/juiced/blob/main/docs/macos-port.md).
- [Linux and Android source](https://github.com/martin2844/juiced/tree/linux-nixos), [Linux guide](https://github.com/martin2844/juiced/blob/linux-nixos/docs/linux-port.md), [Android guide](https://github.com/martin2844/juiced/blob/linux-nixos/docs/android-port.md).
- [Upstream source](https://gitlab.com/sidecr3w/openjuice) and [upstream releases](https://gitlab.com/sidecr3w/openjuice/-/releases).

The older `android` branch and rain-fix release remain as history. Current Android development is on `linux-nixos`.

## License and credits

OpenJuice uses the [PolyForm Noncommercial License 1.0.0](LICENSE.md). Preserve the upstream copyright notice when redistributing. Dependencies retain their own licenses. Juiced and its game assets belong to their respective rights holders.
