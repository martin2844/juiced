# Volvo 240 transmission correction — 2026-10-31

Delivered in local Linux car bundle **r11**, Wave2 **revision 9**. First-wave
revision 6 and Ferrari revision 4 retain their r10 payloads. This is another
specific handling correction within the ten-port task, not full acceptance.
RP5 was not accessed, updated or controlled.

## Evidence and change

Original Volvo Swedish model-year-1984 owners manual TP2401/1, printed page 100
(PDF page 102), identifies M46 ratios **4.03 / 2.16 / 1.37 / 1.00**, overdrive
rounded to .79 and reverse **3.68**. The table was rendered and visually inspected.
Original service manual TP30425/4, printed page 80, gives precise overdrive .797.

Sources: [1984 Swedish owners manual](https://www.240.se/litteratur/tp2401.pdf),
[factory service specifications](https://www.240.se/litteratur/tp30425_4.pdf).
Original PDFs, extracted text, rendered tables and SHA256 receipts are retained
under `out/validation/linux-current-20261029/ten-car-factory/references`.

The Volvo 240 script used first gear 3.71 and the MX5 donor's reverse 3.758.
R11 changes only these two dynamics values to 4.03 and 3.68. Existing .797
overdrive, 3.73 axle, torque/power curve, mass, suspension, geometry, sounds and
stable car/part/save IDs are retained. The authoring script and handling/spec
metadata now emit and describe the same values. Seven authoring outputs were
regenerated privately and compared numerically with measured candidate scripts.

**Remaining estimates:** the precise axle allocation and curb mass for the
chosen European Turbo. The 1984 manual gives a broad four-door Swedish
`Tjänstevikt` range of 1330–1405 kg; it does not establish an exact Turbo curb
mass or authorize silently replacing it with the range's minimum. The existing
1330-kg candidate remains explicitly estimated.

## Native driving results

The existing six-case native handling study was run for 22 stock cars at both
60/120-Hz steps: **264 new candidate cases**. Its prior r10 baseline remains in
the CP50 evidence. At each step only Volvo 240's six rows change; **126 cases
for the other 21 cars are identical**. 0–100 is unchanged at the recorded tick
resolution (5.667 / 5.542 s respectively); 90-second terminal speed remains
206.685 km/h within .02 km/h. These are game observations, not factory timings.

A separate native `TransmissionStudy.cpp` holds first or reverse for five
seconds after settling, with automatic shifting disabled. It covers 23 cars
(the same set plus MX5) before/after at both steps: **184 manual-gear case runs**.
Every car moves in the intended direction with finite state and retains its
selected gear. Only the two Volvo 240 cases change per step; the other **44
manual cases are identical**.

At 60 Hz, holding Volvo's first for five seconds changes its end speed from
54.812 to 50.532 km/h, consistent with the shorter ratio. Native parsed first
changes from 3.710 to 4.030. The loader scales **every car's raw reverse ratio
by .6** (`Dynamics.cpp`); native reverse therefore changes from -2.2548 to
-2.2080, while the authored factory ratio is -3.680. This common game behavior
was preserved, not confused with the real gearbox specification. The existing
2.8 torque multiplier, synthetic ground and timestep-dependent braking still
prevent a claim of physical road-car certification.

The first reverse-study attempt supplied throttle only once; native ASR mutates
it, so the pedal remained cut and a retail 350Z failed the motion assertion.
The harness was corrected to supply the held pedal every tick, as normal player
input does. The failed partial output is retained. All 184 corrected cases
passed; no production engine fix was made for that harness error.

## Package and preservation

The r11 ZIP is read back with every payload hash, CRC and extracted manifest
verified before installation. Its full-roster native checker passes **1238/1238**.
Its Volvo script matches the measured candidate byte-for-byte. Ferrari and
first-wave payloads match r10 exactly; their previous 482 first-wave passes are
retained evidence, not represented as a newly rerun checker.

The Wave2 folder is installed through an atomic exchange with the old revision
retained as `ten-car-factory/wave2-r8-rollback`. Repository runtime files match
the installed package. All nine installed packages assemble together. Six
personal hashes, the production ELF and the other eight packages are unchanged.
No save restore/edit, Android deployment or public publication occurred.

The archive hash, sizes, exact changed-file set and installed composition are in
`ten-car-factory/r11-delivery-receipt.json`; measurements and comparisons are
`forward-comparison.json` and `transmission-comparison.json` in that directory.

## Other variant research

The original August-2002 European V70 technical sheet was rendered and inspected.
It distinguishes B5244S2 M56L/4.00 (first 3.39, second 1.91) from B5244S M56H/4.25
(first 3.07, second 1.77); their reverse ratios also differ. This exposes a
variant-assignment question in the selected US2004 candidate. It does not prove
which European row applies to that US car, so no speculative V70 change ships.
Source: https://www.volvoclub.org.uk/tech/V70_August_2002.pdf

The 1993 Dutch 405 brochure download returned HTTP403. Its search-index text is
not visually reviewed proof for the selected 1994 UK GLX. Manufacturer evidence
for that variant remains open; no arbitrary different-market numbers were used.
These findings and source limitations are recorded in `factory-findings.json`.

The previous goal turn made progress (r10 installed and CP50 verified). This
turn adds independently inspected manufacturer evidence, one tested car fix and
a package delivery. The full ten-car objective remains active; remaining work
is listed in `ten-car-acceptance-2026-10-31.md`.
