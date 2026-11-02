# Peugeot405 refinement r20

This extends the user-accepted r19 front/hood styling and targets the remaining
rear bumper, spoiler and see-through upper grille defects. It also implements
the explicitly approved405-only preparation up to400BHP while keeping the
factory102BHP engine and every existing part/save identity.

`build.py` runs in Blender with `REPO BASELINE_R19 FRESH_OUT`. It reuses the
frozen native adapter/constrained silhouette helpers from
`tools/peugeot405-photo-kits/build.py` and records their digest. Three continuous
rear fascias incorporate plate recesses and a genuine left exhaust notch.
Original plate/UVs, lights and corner vinyl receivers remain. New spoilers are
a deck-following bonded lip, a period painted bridge and a smaller painted GT
foil with individually fitted shoes/pedestals. The upper grille's new backing
has inward-facing roof/floor/side returns. Native units are cm, X across/Y up/
Z forward; Blender metres `(x,-z,y)/100`. No source chassis faces are removed.

Run `configure_tuning.py CANDIDATE` once after the visual export. Its optional
TurboUpgradeScale curve changes only405 dynamics. Do not use a tilde preceded
by a space in a script comment: retail script preprocessing joins its following
line and would swallow the tuning instruction. The actual native power probe
caught that rejected configuration before installation; final runtime parsing,
power, classes and actual flat-ground driving were rechecked.

Engine changes are in Dynamics.h/.cpp, CarPhysics.cpp and FrontendSaves.cpp.
The optional curve defaults to unity for all other models. It multiplies the
installed turbo tier's boost in the same fit used by simulation and rating.
Profile loading refreshes derived ratings only for configured models (plus the
existing zero-rating recovery). Original saves/parts remain readable; no save
migration or automatic part purchases are involved. Old executables ignore the
field and keep the previous maximum power until their engine is rebuilt.

`verify_geometry.py` independently parses exports and checks native indices,
materials, source chassis/plate retention, other-car identity and UI/race parity.
`check_grille.py` traces exported meshes with backface rejection, with r19 as a
negative control. `PerformanceProbe.cpp` reads real installed definitions and
catalogue tiers and can step CarBody on flat ground with `--drive`. It does not
open or write personal profiles. Its flat-ground times are diagnostics, not
real-car certification or a race/FPS benchmark.

`capture.py` records64 actual native images using the preserved r19 private
CarView harness. A second private front camera records12 grille close-ups.
`studio.py` and `finish_studio.py` retain four complete editable Blender scenes
with source textures packed; `build.py` retains separate authoring objects.
Native captures govern appearance; Blender's lighting/materials approximate it.

`prepare_delivery.py` freezes Wave2 revision18 / runtime r20; `install_linux.py`
reversibly exchanges installed and repository packages with exact save/other-mod
guards. The native ELF was rebuilt for the new feature; its previous keyboard
fix remains and the controller test was repeated. Sources, evidence, model and
binary digests belong to the checkpoint. No Android install or public push is
part of this local refinement. Source interior and floor-reflection limitations
are inherited; user aesthetic acceptance remains open.
