# OpenAL Soft 1.25.2

The fixed build's sound output (`src/Audio/OpenAL`): OpenAL Soft, a software implementation of the OpenAL 3D audio
API with EFX (its reverb plays the game's EAX). Linked statically into OpenJuice; its licence is the GNU Library
General Public License 2 or later (`COPYING`; parts under `LICENSE-pffft` and `BSD-3Clause`, the bundled Guidelines
Support Library and {fmt} under their own `LICENSE`s). OpenJuice's whole source is published, so the library can be
changed and OpenJuice built again with it, as the licence asks of a work linked with it.

The files are those of the release as its author published it, unchanged:
`https://github.com/kcat/openal-soft/archive/refs/tags/1.25.2.zip`
(SHA-256 `47e22c066dffa2f65a1747272978344db845dcef9c7ee118449ec560a44adc8f`), only the folders the library is built of: `al`, `alc`, `common`, `core`, `include`, the headers
of `gsl`, those of `fmt-11.2.0` and its `src/format.cc` and `src/os.cc` (the examples, utilities, tests, router, presets and HRTF data left out).

`openjuice-config/` holds the headers CMake writes for that build (the same for x64 and x86):

    cmake -S openal-soft-1.25.2 -B <build> -G "Visual Studio 18 2026" -A x64 -DLIBTYPE=STATIC
          -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF -DALSOFT_INSTALL=OFF
          -DALSOFT_ENABLE_MODULES=OFF -DALSOFT_UPDATE_BUILD_VERSION=OFF -DALSOFT_EMBED_HRTF_DATA=OFF
          -DALSOFT_BACKEND_DSOUND=OFF

(the backends WASAPI, WinMM, WaveFile and Null; legacy EAX on). `OpenJuice.OpenAL.vcxproj` compiles the sources of
CMake's `OpenAL`, `alsoft.common` and `alsoft.fmt` targets with its definitions and options.
