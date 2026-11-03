# Native effects checkpoint — 3 November 2026

> **Superseded by [the Android/device validation follow-up](native-effects-device-validation-2026-11-03.md).**
> The black surface speckling described below was a screenshot interpretation
> error: game backbuffer alpha was exported as transparency. Inspecting RGB before
> resizing and taking a direct RP5 screen capture disproved the supposed scene
> defect. Preserve this document as investigation history, not the current verdict.

**Work in progress; not a release acceptance.** The native effects implementation
builds and its isolated GPU regressions pass on the Mac and desktop GL hosts.
Actual races expose a remaining visual defect. Do not replace the habitual Linux
executable or distribute/install this APK as a verified upgrade yet.

The user subsequently directed work to the Mac and deferred RP5 installation.
The Retroid was queried and screenshots taken, but no APK, mod or save was
installed, deleted or edited during this pass. It was locked. No further device
interaction is needed until the user makes it available again.

## Starting point and isolation

* Starting source: `a6664242da85b4be2e0a0ccb5a35c6d70742a105`, upstream 1.02.7.
* Safety branch: `backup/pre-native-effects-20261103`.
* Habitual Linux executable: `out/linux/juiced`, SHA256
  `295d980d1d0079c19a978774f94c619b739e073415a1f6e85eb172c34fc0bcdf`.
  This executable was not replaced.
* Development Linux build: `out/native-effects`.
* Exact Android backend on the desktop: `out/android-effects-host`.
* Mac source/build: `/Users/martin/Projects/juiced/openjuice-effects-20261103`,
  `out/native-effects/juiced` and `out/native-effects/Juiced.app`.
* Mac original assets: `/Users/martin/JuicedData`. Tests use separate
  `/tmp/oj-effects-*` support/save directories. The normal Mac support folder
  and installed application have not been replaced.
* Evidence on this Linux host:
  `/run/media/martin/Ediloco/OpenJuice-builds/20261103-native-effects`.

`JUICED_SUPPORT_DIR` now also controls the Mac settings and controls paths;
previously some Mac reads ignored isolation. Test careers do not use the owner's
normal profiles. No roster/save serialization or car geometry/physics changed.

## Implementation

`NativeRenderDevice` shares post-processing through the real renderer API and
the original compiled shader arrays from the pinned JuicedConfigExt submodule
`ec4aaab62cfeb71561e1848517e9a09625f5f1a0`. There is no runtime Wine/HLSL
compiler. MojoShader translates those arrays to GLSL ES or Metal source.

Implemented native paths include ambient occlusion, point/volumetric lights,
lamp shadows, road reflections, depth dilation, SMAA lookup/passes, procedural
cloud map/view/dome, heat-haze particles/composite and the upstream brightness
curve. GL and Metal provide actual float/HDR targets, volume noise uploads,
copies, captures, shader variants and state restoration. Floating-point depth
marks must survive unclamped; RGBA8 is not a substitute for R32F.

World/headlight/sun material replacement uses symbolic upstream fingerprints and
light-case variants. The native settings now apply the four graphics CLI
overrides; previously those switches parsed but did not reach the native
settings defaults. Texture filtering applies real anisotropy on both backends.

Native quality capabilities advertise SMAA and anisotropy. **MSAA is not
implemented/advertised**; it must not be described as working. Performance at
60 FPS, especially Android ULTRA, has not been established.

Concrete native defects fixed during validation:

* MojoShader's GLSL fog output mapping also needs the scalar component for
  numbered shader-model-3 outputs, not just `vs_oFog`.
* D3D LOG uses the absolute operand and a finite zero limit. Direct native
  `log2` on negative lighting data fails the new signed-logarithm fixture.
* Legacy pixel COLOR inputs are saturated before pixel calculations, consistent
  with [Microsoft's shader documentation](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/direct3dhlsl/dx-graphics-hlsl-writing-shaders-9.md).
* Metal attachment CLEAR ignores a restricted viewport. Restricted color,
  depth and stencil clears now draw the requested rectangle instead of clearing
  the entire atlas. The depth-dilation fixture failed before this correction.
* Metal now applies material depth bias, independent constant RGBA blending,
  inverse constant blending, blend operations and separate alpha blend states.
* SMAA's area texture emulates upstream A8L8, including its alpha channel.
* Half conversion preserves finite overflow as infinity and round-trips all
  non-NaN half values.
* Mac mod-linked builds now include the mod sources/image importer and link
  iconv/libavutil/Lua correctly. Mac code-page unit failures were corrected by
  requesting UTF-32LE instead of the unsupported WCHAR_T iconv target.

The pinned SDL Metal source has an idempotent, checked CMake patch enabling
conservative compilation and `preserveInvariance`; generated vertex positions
are marked invariant. [Apple documents](https://developer.apple.com/documentation/metal/mtlcompileoptions/preserveinvariance)
why both are needed for invariant multipass positions. This change **did not
resolve the remaining race artifact** and is not presented as its solution.

## Verification and its limits

`juiced_native_effects_check OUTPUT_DIRECTORY` exercises real offscreen rendering
and reads the pixels back. It does not need game data or a save. It has cases for
signed logarithms, UNORM blend controls, depth bias, legacy color saturation,
RGBA constant/inverse blending, AO, point light, volumetric light, lamp shadow,
reflections, depth dilation, SMAA, clouds, heat haze, brightness and half formats.

Cloud coverage changes are checked numerically. Volume-view/temporal shader
execution is exercised, but this fixture does not establish the appearance of
every sky/dome/weather combination. Effects return values and compilation alone
are insufficient for release acceptance. The RGB assertions are deliberately
separate from scene-load and frame-count checks.

On the actual arm64 Mac, the complete shader/scene build succeeds, the unit suite
reports **730 passed, 0 failed**, and the 16 GPU cases pass. The desktop Radeon
also runs the Linux and exact Android renderer implementations. Desktop GLES
results do not establish Adreno renderability, performance or visual correctness.
The physical RP5 GPU still needs this fixture and real race testing.

The ARM64 release APK builds with the existing signing configuration, version
code 28 / `1.02.7-android.28-native-effects`. It remains a candidate. The debug
build has a guarded `effects-check` mode for a later physical GPU run.

The Mac selector opened with the current mod collection. The portable gallery
tool now accepts a mod collection directly and assembles a temporary overlay.
It generated **384 captures**: a Focus reference plus the eleven added cars,
four styles and eight directions. Four sample images were viewed on this host;
this is not a visual approval of all 384 images. Original assets are still
required; this is not a game-data distribution.

The current car packages copied into the isolated Mac checkout include Street
Cars revision 8, Wave 2 revision 21 (the latest accepted 405 work) and AI Hero
Liveries revision 2. Their catalogue was used in the selector/gallery and a
three-car 405 night/wet diagnostic race. No new car models were authored here.

## Remaining blocker: rendered surfaces

Real race captures show black speckling/streaks on the road and car paint, and
in some runs black portions of world geometry. A stock Focus exhibits this too;
it is not isolated to the imported 405 mesh. The same symptom was reproduced
with the unchanged Linux production ELF, so the initial native-effects patch
alone is not a sufficient explanation.

Useful preserved evidence:

* `baseline-test.bmp`, `baseline-race.bmp`: unchanged Linux production binary.
* `race-405-valid.bmp`, `log-fixed-race.bmp`: native development races.
* `mac-race.bmp`, `mac-normal.bmp`, `mac-405-night.bmp`: real Metal scenes.
* `mac-gallery-samples/`: selector captures and the full 384-row manifest.
* `before-post.bmp`, `after-post.bmp`, `post-compare.png`: the symptom already
  exists in the scene target before final post-processing. Final glow/copy alone
  is not the origin.
* `blend-check.bmp`: disabling blending experimentally restores much of the
  world but does not eliminate road speckles. This is diagnostic evidence;
  disabling blending is not an acceptable fix and is not retained in source.
* `blend-trace.log`: combinations of enable/source/destination/op/constants
  observed during the failing scene.
* `nan-check.bmp`, `output-clamp.bmp`, `pixel-clamp.bmp`,
  `mac-invariant-race.bmp`, `mac-bias-race.bmp`, `mac-blend-race.bmp`: diagnostic
  attempts, not proof of visual repair. NaN marking, generic output clamping,
  invariance and depth bias did not resolve the complete symptom.

Temporary shader marking, blend disabling, blend tracing and pre-post capture
instrumentation were removed from production source. Keep the evidence instead
of repeating these unsuccessful hypotheses. The next investigation should
identify the actual material/pass and its source alpha/depth/texture inputs,
using this reproducible scene; do not hide the issue by disabling effects or
loosening the image acceptance criterion.

## Resume and deployment

1. Resolve and visually approve the material artifact in Metal with stock cars
   and the imported cars, including day/dry and night/wet scenes. Keep isolated
   support paths and actual GPU readbacks.
2. Rebuild the CLI, app bundle and signed APK from the same checkpoint. Repeat
   only affected regressions after fixes, then check normal menu/race flow.
3. When the user makes RP5 available, export saves through the launcher's backup
   UI **before** any update. Installed APK19 is not debuggable; do not assume
   `run-as` can extract its files. Do not uninstall or clear application data.
4. Install the signed update with `adb install -r`, import current car/livery
   packs, compare exported save contents before/after, and test physical GLES,
   controls, sound and actual races. Do not promise 60 FPS without measurements.

No public push/release, habitual launcher replacement, Mac installation or RP5
update is included in this checkpoint.
