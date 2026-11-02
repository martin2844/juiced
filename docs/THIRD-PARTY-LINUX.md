# Linux component notices

Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/openjuice)
Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/juicedconfig-extended)

OpenJuice is distributed under the PolyForm Noncommercial License 1.0.0 in
LICENSE-OpenJuice.md. Changes in this port do not replace upstream authorship.
Juiced game assets and optional runtime mod assets are separate from the engine.

The Linux build uses these components, which retain their own licenses:

| Component | Notice / source |
| --- | --- |
| SDL 3.4.10 | zlib; licenses/SDL3.txt. Version/source are pinned through default.nix. |
| MojoShader | zlib; licenses/MojoShader.txt. Source commit ad5dff84830c2863c841f4b1f4e3df78c705b383. |
| JuicedConfig Extended shared controls | PolyForm Noncommercial 1.0.0; licenses/JuicedConfigExt.txt. Submodule commit ec4aaab62cfeb71561e1848517e9a09625f5f1a0; native builds use ControlsFile, PadActions and ForceFeedbackScale. |
| Lua 5.4.7 | MIT; licenses/Lua.txt, retaining the linked version's copyright notice. |
| FFmpeg 8.1.2 | LGPL 2.1 or later; licenses/FFmpeg-LGPL-2.1.txt. Dynamically linked; nix/ffmpeg.nix disables GPL, version-3 and nonfree options. |
| Speex 1.0.3 | BSD-style notice retained in licenses/Speex.txt for the vendored source; Linux currently omits the online voice backend. |

The distribution's media-source companion retains the exact FFmpeg upstream
source and Nix patch/configuration inputs used for its shared libraries, along
with instructions to reproduce them. The pinned Nix definitions remain in the
engine source companion. Other dynamic system dependencies retain their own
component licenses and upstream source references through that pinned package
set; this notice does not relabel their licenses as OpenJuice's.

The application does not prohibit replacing/relinking the LGPL libraries or
reverse engineering for debugging modifications to them. The source build uses
pkg-config discovery of shared FFmpeg libraries; its Nix configuration is a
reproducible default, not an application lock on replacement libraries.

Run `juiced_ffmpeg_license_check` from a development/test build to inspect the
actual linked FFmpeg license, forbidden configure flags and required audio
decoder/input support. A distribution receipt records the library hashes,
complete configure strings and source/patch identities separately.
