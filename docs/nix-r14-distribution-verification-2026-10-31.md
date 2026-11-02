# Nix r14 runtime distribution — 2026-10-31

The new native Linux/Nix runtime is packaged locally, including the corrected
stereo pan. This completes the export/import packaging that was pending in the
[audio checkpoint](native-audio-pan-verification-2026-10-31.md). No engine,
Android, car or save code changed during this packaging pass. No public upload,
push or commit was made, and the RP5 was not accessed.

## Artifact and exact identity

`out/releases/OpenJuice-20261031-r14-linux-x86_64-nix.tar.xz`

- 319696360 bytes (approximately 305 MiB).
- SHA256: `fbf995354b74049d0e4329b93d6f43a067656ccf9db8b9ff1b9588180d5ecca6`.
- 23 archive payloads independently read back and hashed, including the checksum file.
- The user-facing `sha256sum -c CONTENTS.sha256` command actually runs and checks 22 files.

Actual Nix output:
`/nix/store/1zhs7n4wcryzdchqq9ricwp9ig2wk6z8-openjuice-1.02.5-linux-mods1`.
Game ELF SHA256:
`1f4e53e33dc93b8a17ca73a5a6599ac3a367b3483507eb88f48dce733cf37950`.
Wrapper SHA256:
`4e9652ddcb6b990e52a445ac9224a541d5208b5770840107ab50512d8bb6077a`.

The stored wrapper/ELF identities match the actual previously observed r14
six-car normal-entry race and menu/race PCM capture. That evidence is reused
by exact binary identity; this packaging pass does not claim another race or
execution inside the private import store. Runtime scope remains private Xvfb,
llvmpipe, virtual pad, autopilot and dummy audio. Native readbacks of its main
menu and six-car race were inspected. Transitional results captures are not
represented as a settled full-results gallery.

## Import, installer and archive checks

The 1103830000-byte raw store export contains 307 paths. It is imported into a
fresh exclusively owned `closure-import-root`. Querying that store reproduces
the complete exact dependency graph. Native Nix verification checks path
existence, link hashes and store contents; it exits0 without reported errors.
The imported game ELF separately matches the actual built/tested executable.

The unchanged installer is exercised with `OPENJUICE_PROFILE` set to the owned
`linux-package-r14/install-profile`. Its installed wrapper resolves to the exact
tested Nix store wrapper. That profile's `juiced --help` exits0. Its support
directory is also owned by the validation run. Neither the normal user Nix
profile nor the habitual development launcher is selected for replacement.
The usual launcher still executes the current development ELF.

The archive reader verifies all 23 payload hashes against the pre-packaging
files. Independently, the documented contents-check command reports all 22
listed files OK. All non-NAR package metadata is retained with matching hashes
after temporary export/import trees are removed. Instructions, source references
and the new README identify r14/18 CTest and APK26, replacing the earlier copied
guide's old validation counts. Nix remains required; this is not a standalone
runtime for systems without Nix or a clean-machine physical gameplay test.

## Source, media and provenance

The package retains the required upstream notice and GitLab link for Sidewinder/
OpenJuice, the port repository link, the original engine license and component
notices. 31 preceding source/evidence checkpoints are hash-verified and bound in
`SOURCE-CHECKPOINTS.json`. The exact engine source, exact Linux FFmpeg source,
optional car bundle r8 and signed APK26 have separate names/hashes/sizes in
`LOCAL-ARTIFACTS.json`; their bytes are not embedded in this runtime.

The actual r14 build matches 908 critical current inputs and passes 18/18 CTest.
The 1345-file engine-source companion was independently restored to the same NAR
and derivation. Its SHA256 remains
`be3d28ccfbdbf84e77d6c63d85686fb8346bdcc8f38f985815480a2e413f52f0`.
The unchanged r9 media-source companion remains applicable: the same dynamically
linked LGPL FFmpeg build and 306 dependency paths are retained. These checks do
not constitute final licensing approval for every component of the distribution.
Original game assets, mod assets, recorded game audio, credentials and user saves
are excluded from the runtime archive.

## Preservation, cleanup and remaining work

All six personal Linux save/creative files and every installed Wave2 file match
their prior hashes. Development ELF/APK26, the usual launcher and source/asset
state remain unchanged. RP5 remains APK19/car packr5; Linux stays bundle r8 /
Wave2revision7. Packaging creates only an isolated installer profile and owned
validation artifacts. It does not install another APK or modify user careers.

After complete archive/source/import verification, the owned raw export stage
and private import-store tree are removed: 2210840681 regular-file bytes in total.
This is a logical file-byte count, not a physical free-space guarantee. Actual
Nix store outputs, the source GC root, isolated installed test profile, final
runtime/source archives, metadata and receipts are retained. Imported read-only
directories were made writable only beneath these owned cleanup roots; source
store files and user folders were not chmodded or removed.

Replayable helpers/receipts are under
`out/validation/linux-current-20261029/linux-package-r14`. Final verification
binds the package, import, installed profile, current 908 inputs, source/media/
car/APK companions, runtime identity, cleanup and personal-data preservation.
The broader goal remains active: the strict119-D8M inventory failure, remaining
gameplay/route/art coverage, measured race saturation/reference listening,
physical Android/input/audio/network and final source/license/publication review
are not closed by this archive.
