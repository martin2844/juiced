# Physics: the rigid body

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

The physics runs on the main thread, where the x87 rounds every operation to a float (`07-dynamics.md`); every sum
below is formed in the order written. Distances are metres: the world's centimetres times 0.01.

## The definition

The first 0x104 bytes of a car definition (`07-dynamics.md`) are those of a dynamic object (`0x004EEE20`): the mass
(`+0xAC`), the inertia per kilogram about x, y, z (`+0xB0`), the centre of gravity in the model (`+0xC0`), the
gravity factor `LocalGravity` (`+0xD0`, default 1.0), the drag coefficient (`+0xD4`, default 1.55), the sleep values
(`+0xD8` set, `+0xDC` on, `+0xE0`) and the airfoils (`+0xE4`, 0x24 bytes each).

An airfoil (`0x004EF2C0`) keeps its point less the centre of gravity of that moment, its force (the force at 1 m/s)
and its drag. The files have none; the parts add them (`07-dynamics.md`, fitting).

## The body

A body (vtable `0x006F6738`, made by `0x004F2060`) holds a state vector (`+0x304`) whose layout is given by an index
table (`+0x104 .. +0x134`): the position of the centre of gravity, the momentum P, the orientation quaternion (w, x,
y, z), the angular momentum L. A car appends its own values.

Made, the body is at rest; its quaternion is that of the angles `+0x218` (`0x004F35B0`: Qz (Qy Qx) of the half
angles, sine and cosine rounded to floats; the angles are 0) normalised, while the rotation stays the identity until
a state is set. Its position is the node's position (centimetres x 0.01) plus the rotation times the centre of
gravity (`0x004F2C10`), which also puts the node there (`0x004F01E0`: the quaternion's rotation, the origin x 100).
The inverse mass is `1 / Mass`, the inverse inertia of the body's frame the diagonal `1 / (Inertia x Mass)` (0 for
0; `0x004F00C0`). The integrator (`+0x100`) is 2 (RK4) and the drag factor (`+0x2F0`) 1.

The quaternion product (`0x004EC490`) is w = a.w b.w - a.v·b.v, v = a.w b.v + b.w a.v + b.v x a.v; normalising
(`0x00429DC0`) divides by the length plus 1.1920929e-7, so a unit quaternion settles at 0.99999988.

### Setting a state (`[5] 0x004F0320`)

The position, quaternion, P and L are taken; the quaternion is normalised and written back into the vector. Then:
the rotation R of the quaternion (`0x0041D790`), its inverse (`0x0049F860`), the angles -asin(R⁻¹[9]),
atan2(R⁻¹[8], R⁻¹[10]), atan2(R⁻¹[1], R⁻¹[5]) (full precision, rounded once), the inverse inertia of the world
(R I⁻¹) Rᵀ (`0x0041B520` twice, Rᵀ by `0x004F3890`), the velocity P / m, the local velocity R⁻¹ v, the angular
velocity ω = I⁻¹ L, the local angular velocity R⁻¹ ω, the local acceleration F / m of the last forces and the
acceleration R (F / m).

### The derivative (`[6] 0x004F0670`)

The force and the torque are cleared and the forces summed; the derivative is the velocity, R F, half the product
q (0, ω), and R τ. The forces are in the body's frame.

### The forces (`[7] 0x004F0800`)

A force at a point of the body's frame (`0x004F0270`) adds the force whole and the torque r x (F x weight). In order:

1. the external force and torque (`+0x278`, `+0x288`), set from outside between two integrations;
2. the gravity: R⁻¹ (0, LocalGravity x Mass x -10, 0) at the centre;
3. each contact of the step: its first force with weight 1, its second with weight 0.5 for a tyre (shape type 2),
   else 1 (`0x004F9090` computes them);
4. each airfoil: its force times the speed² (the local velocity's, z² + y² + x²) at its point; its drag joins the
   body's;
5. the drag at the centre: the unit local velocity (divided by its length plus 1.1920929e-7) times
   -(drag x the drag factor x speed²).

### Integrating (`[8] 0x004F0B90`)

With s the state and h the step:

- 0, Euler: D = derivative at h 0; s = h D + s; hold, set.
- 1, midpoint: D₁ at h 0; t = h D₁ + s; hold, set; D₂ at h (written into t); s = (D₂ + D₁) (h x 0.5) + s; hold,
  set.
- 2, RK4: k₁ = h D(s); t = k₁ x 0.5 + s; hold, set; k₂ = h D; t = k₂ x 0.5 + s; hold, set; k₃ = h D; t = k₃ + s;
  hold, set; s = ((((k₃ + k₂) + (k₃ + k₂)) + h D) + k₁) x 0.16666667 + s; hold, set. The derivatives take h.

Each "hold" is the limits below at h; each "set" sets the state just made. Then, when |h| > 1e-4, the change of the
local velocity times 1 / h goes to `+0x2B8` (else 0), and the external force and torque are cleared.

### The limits (`[9] 0x004F1E10`)

P is held within Mass x (-150, 150) in x and z and Mass x (-100, 20) in y. The angular velocity of the state's L by
the inverse inertia of the state set last is held within ±4π about x and z and ±1.5π about y, and L becomes the
inverse of that matrix (`0x004F36C0`, by cofactors) times it: L makes this round trip every time.

### A step (`[3] 0x004F2810`)

The world's contacts are gathered (`0x0053EC30`; a pressing one gets LocalGravity x Mass x 10 / count times its
`+0x14`), the impacts applied (`0x004F1770`), the body integrated, then pushed out (`0x004F1C70`; "Impacts" below). The displacement of the
step goes to `+0x2C8`. A body with sleep values (`+0xD8`) counts the time it moves less than 2 cm (the displacement's
square over 0.0004) and turns slower than 2 rad/s (the angular velocity's square over 4) in a step; past 3 s it goes
to sleep: when it is in a world (`+0x0C`) and active (`+0x74`) it leaves the active list (the last of the list takes
its place), `+0x74` = 0; the time starts again and the step ends there.

Every other step goes on: a body whose definition is named `wheel` (without regard to case) sets `+0x7C` = 1, `+0x80`
= 0 while |R[1][0]| (`+0x1C8`) is under 0.8, else the other way; then the slow body's brake (`0x004F2AFE`): of the
velocity in the body's frame, s = √((z² + y²) + x²), its direction each axis x 1 / (1.1920929e-7 + s); under 0.3 m/s
the velocity becomes the direction x (s - 2 dt) (`0x004F2F80`; 2 dt taken as 0 when below 0). A car has no sleep
values: every car step ends so. Under 2 dt the speed turns the other way (`stock-problems.md`): a car at rest keeps
moving by up to 2 dt each step.

## The car

A car's body (vtable `0x006F6A68`, made by `0x004F6B50`) keeps its own copy of the car definition. It has a wheel
record (0x24 bytes) for each wheel of the definition: the wheel, its index in the state, its spin, the brake torque,
the tyre torque, their sum, a value cleared before each step, the brake and the handbrake applied. The state grows by
the engine's spin (index 13) and the wheels' spins. The inverse of EngineInertia, the sum of the driven wheels'
WheelInertia and its inverse (`0x004F5A70`), and TorqueCurveY's last value x MaxTorque x 2.8 (`0x004F5B60`) are kept.

The gear (`0x004F7610`) is 0 for reverse (ReverseGearRatio), 1 for neutral (ratio 0), 2 .. 7 for the forward gears;
the ratio's inverse is kept (0 for 0). Another gear (`0x004F7710`) opens the clutch again while the engine is on.

### Setting a state (`0x004F5E60`)

After the body's: the engine's and the wheels' spins; with the clutch in and a gear other than neutral, each driven
wheel's spin is (1 / FinalDrive x the gear's inverse) x the engine's, also in the vector.

### The derivative (`0x004F5F30`)

The engine's spin changes by its torque x the inverse EngineInertia while the clutch is out or in neutral; else by its
torque divided by EngineInertia plus, for each driven wheel in order, |the gear's inverse| x WheelInertia x
1 / FinalDrive (0 when that is 0). A wheel's spin changes by its torque x its inverse WheelInertia.

### The forces (`0x004F6050`)

First each tyre contact of the step, from the last, takes its wheel's spin, radius and angle (MaxSteerAngle x the
steering), LocalGravity and its inverse, and the throttle when the wheel at the contact's place in the list is
driven. Then the body's forces (the tyres' among them). Then, each from the last:

1. the engine torque from 0 (`0x004F5C60`): while the engine is off, the clutch in or in neutral, the torque curve's
   factor at its spin x MaxTorque x the throttle x 2.8, times ((Turbo - 1) x the boost + 1), times Nitro while the
   nitro burns; then less its spin / the curve's last speed x 2 (1 - throttle) MaxTorque. The boost (`0x004F7550`,
   0 for a Turbo of 1) rises from PowerRangeMin to a tenth of the way to PowerRangeMax, times the rpm's share of
   PowerRangeMax;
2. each wheel's brake torque (`0x004F5D80`): less BrakeMu x the brake, HandbrakeMu x the handbrake and AxleMu, each
   by the sign of the spin;
3. each tyre's force along its wheel (cos of the wheel's angle x its z, plus its y x 0, plus -sin x its x; the sine
   and cosine at the x87's full precision) times the radius, off the wheel's tyre torque;
4. each wheel's sum;
5. with the clutch in and a gear: each driven wheel's sum x 1 / FinalDrive x the gear's inverse joins the engine
   torque;
6. with the engine on, the clutch out and a gear: the spin difference d = engine - |ratio| x FinalDrive x the wheel's
   spin passes sign(d) x 500 (reverse) or sign(d) x ratio x 160, times |d| / 10 under 10 rad/s: off the engine, onto
   the wheel by the sign of the ratio;
7. a wheel with a tyre contact is held from passing, within the step, the spin its tyre rolls at (the contact's
   `+0xB0` / radius, or 0 when the brake torque is the larger): the share (target - spin) / (h x inverse inertia x
   torque) clears the torque under 1e-7 and scales it under 1.

### The limits (`0x004F6A80`)

The engine's spin within TorqueCurveX's first and last speeds; then the last speed becomes the definition's top speed
(`+0x180`), x 1.45 while the nitro burns, and the body's limits follow.

### A step (`0x004F6F30`)

The step's time is multiplied by TimeFactor; the nitro burns. The ABS (an ABS car over sqrt(200) m/s with the brake
on): a wheel whose tyre slips under -0.15 lets go, the others follow the pedal at 15 a second within 0, 1, never under
0.05; else every wheel takes the pedal. Each wheel takes the handbrake; a driven wheel whose tyre slips over 0.05 is
braked at least by 1 - throttle, and every driven wheel's brake is eased by (1 - throttle / 2). The traction control
(an ASR car over 1 m/s on the throttle, steering under 0.75 or faster than 8.94 m/s forward, ASRFactor over 0) takes
the largest slip of the driven wheels (by their places in the list): over (1 - ASRFactor) x 3 + 0.2 the throttle is
held to 1 - the slip (at most 1). The throttle is passed on; under a slip of 0.8 the automatic gearbox works. The
contacts of the step start anew, the body steps, the clutch follows (`0x004F6730`).

The clutch: on the throttle an engine that was off starts with the clutch out; off the throttle under 1.01 x the
curve's first speed the engine stops. While the engine runs and the clutch is out it is let in at 5 a second (first
gear, after a shift down) or 2.5, and is in over 0.99.

The automatic gearbox (`0x004F6870`, when set): its wait runs down. On the throttle, in a gear with the clutch in:
over PowerRangeMax x 1.02 rpm and below the top gear it shifts up (not from first while steering over 0.5 under
12 m/s; from the second gear on only faster than 0.894 m/s forward) and waits 1.5 s; else above 0.6 throttle under
0.75 x PowerRangeMax rpm above second gear with no wait it shifts down. Off the throttle (under 0.1) under 0.6 x
PowerRangeMax rpm above second gear, with the clutch in and no wait, it shifts down and waits 0.5 s.

## Contacts

The collision gives a body touches (the body's `[2]`): the collider's ID, the surface zone with its friction and
bounce, the point and the surface's normal in the model's frame (centimetres), the depth, the other body. A touch
finds its contact definition (`0x004F3220`): the first of its ID whose clipping plane, if it has one, the point lies
beyond (((z - z0) nz + (y - y0) ny) + (x - x0) nx > 0). Rigid and spring contacts take a record of their pool
(`0x004F2710`; 150 each, at most 35 a body a step); a car's tyre also one of the tyre pool (`0x004F6590`; 64, at most
8 a car), and a tyre touched again is taken anew only when deeper; its wheel keeps the deepest compression, at most
MaxCompression (`+0x18`). The pools start again before every step of the world.

A record (`0x004F9170`) keeps: Mu x the zone's friction, Restitution x its bounce, the point in the world (the model
point x 0.01 turned and added to the model's origin), the arm (the point less the centre, in the body's frame), the
normal in the body's frame and in the world, the depth x 0.01 as the compression, the other body. Each time the
forces are summed (`0x004F9090`) the normal is taken back into the body's frame, the point follows the body, the
compression loses the point's way along the world normal, and the velocity of the point in the body's frame
(`0x004F8E50`: v + w x r, less the other body's) and its part along the normal are found. Then:

- a spring or a tyre (`0x004F77B0`): the compression within 0 and MaxCompression x the stiffness, less the damping x
  the normal speed, within 0 and MaxCompression x stiffness, along the normal;
- a rigid contact (`0x004F9440`): (the compression's share of MaxCompression x 1.1 - the normal speed x 0.01) x its
  weight, within 0 and the weight x 1.1. Before the integration of a step each rigid contact facing up without
  another body gets LocalGravity x Mass x 10 / their number, times the normal's y, as its weight;
- the friction of rigid and spring contacts (`0x004F9360`): against the velocity along the surface, the load x Mu
  plus (1 - |the world normal's y|) x the compression x Mu x 1000, times the sliding speed under 1 m/s;
- a tyre (`0x004F7C40`), when the normal points up in the body's frame: the wheel's lateral and forward axes (turned
  by its angle and laid on the surface), the velocity on the surface, the slip angle (the forward axis's heading
  less the velocity's; backwards the velocity's reversed) and the slip ratio (`0x004F7A60`: rim speed / (speed x
  cos of the angle held within pi/2 + 1.1920929e-7) - 1). From 8 to 10 m/s the Magic Formula fades in, less over
  slip angles 0.3 .. 1, slip ratios 0.7 .. 1 and -0.6 .. -0.9, and on surfaces flatter than a world normal y of 1
  ((y - 0.8) x 5): the combined formula (`0x004F7AC0`) at the load / LocalGravity gives the lateral and the
  longitudinal force along the axes. The rest slides: against the sliding velocity (the velocity less the rim's)
  by the load / LocalGravity x Mu, x 0.4 locked under -0.9 or under 8 m/s, x 0.1 spinning over 1 on more than half
  throttle, times the sliding speed under 1. All times LocalGravity; none on a world normal y under 0.1.

The Magic Formula: sigma_a = tan(alpha) x LateralStiffness / (load x LateralMu), sigma_k = kappa x
LongitudinalStiffness / (load x LongitudinalMu), sigma their length; k = (r + 1) / 2 - cos(sigma / 2) (1 - r) / 2
up to 2 pi, then 1, with r the Mu ratio; F = D sin(C atan(B ((1 - E) sigma + E/B atan(B sigma)))) /
(sqrt(k² tan² alpha + kappa²) + 1.1920929e-7); the lateral force k F tan(alpha) LateralMu x load, the longitudinal
F kappa LongitudinalMu x load. The tangent, the arctangents and the cosines are the x87's at full precision until
their products round.

## The collision with the track

A body's touches with the route's collision (`.cld`, `15-tracks.md`) in its step (`0x0053EC30`, the static part
`0x0053B8D0`) are found in the model's frame, in centimetres: the inverse of the model's matrix (the node's: the
quaternion's rotation, the origin x 100) takes the world there.

The body's sphere is found once (`0x005371A0`): the middle of the box of its colliders (in this order the sphere
colliders' centres less and plus their radii, the ends of the wheel lines, the sphere collidees' centres less and plus
their radii, the vertices of the two meshes; a value below the box's least is not compared with its greatest) and the
farthest of those from it (sums (dz² + dy²) + dx², a sphere's distance plus its radius, the collidee mesh's
(dx² + dz²) + dy²). Its centre, taken back into the
world, and its radius make the box of the quadtree query. Each face found is taken into the model's frame (the corners
x: ((z m2 + y m1) + x m0) + m3, y: ((y m5 + z m6) + x m4) + m7, z: ((z m10 + y m9) + x m8) + m11, the first corner's
z as ((x m8 + y m9) + z m10) + m11; the normal without the position). The zone's offset (`rangeMin`, 0 below -100)
moves the surface along its normal.

A wheel line touches a face (`0x005390E0`) when it runs against the normal (d . n < 0), reaches the face's plane
within its length (t = ((v0 - p) . n) / (d . n)), and the point there lies inside (each edge's cross product with the
point turns along the normal); the depth is the length less t, plus the offset, and the point moves by the offset
along the normal. A touch deeper than 0 goes to the body. After the lines the face goes against the mesh collider
(below).

The body's `+0x7C` and `+0x80` (1 for every body but a lying or standing `wheel`) choose the tests of a face: with
`+0x7C` the face is cut by the definition's clipping planes, then tried against each sphere collider, then against
the lines; with `+0x80` against the mesh collider. The cut (`0x00538310`) takes the face's three corners through each
plane in turn, the first buffer into the second and back; the sphere colliders take the first buffer and its count,
so after an odd number of planes the last cut is lost (one plane: the face uncut) — a stock problem. A sphere touch
(`0x005388C0`, below) takes the offset as a line's does: the point moves by it along the normal, the depth grows by
it, deeper than 0 it goes to the body with the sphere's ID, the face's zone and the face's normal.

### A sphere against a face (`0x005388C0`)

For a polygon of at least 3 corners with the face's unit normal n and a sphere (centre c, radius r): the centre's
distance in front of the face's plane d = ((c - v0).x n.x + (c - v0).z n.z) + (c - v0).y n.y; the depth is r - d
(set also when it misses). When |d| is over r there is no touch. The centre's foot c - n d (w of the centre) is the
point when it lies inside every edge: for each corner v and the edge e to the next (the last to the first, w of the
corner it goes to), with q = foot - v, ((q.z e.y - q.y e.z) n.x + (q.y e.x - q.x e.y) n.z) + (q.x e.z - q.z e.x)
n.y not below 0. Otherwise each edge as a segment of length 1 (`0x00538760`): the roots t0, t1 of its line's meetings
with the sphere (`0x00538620`), not both below 0 and not both over 1, give t = (t0 + t1) x 0.5 held to 0 .. 1 and the
point v + e t. The first edge met gives the point; a later one only when its point's squared distance from the centre
is below the least so far, which starts as the count less 1 read as a float and is never set by the first — a stock
problem: the first edge met keeps its point. No edge met, no touch.

A clipping plane (point o, unit normal n) keeps what lies behind it: with k = (n.z o.z + n.y o.y) + o.x n.x, a corner
v is kept when (n.z v.z + n.x v.x) + n.y v.y is below k (the first corner (n.z v.z + n.y v.y) + v.x n.x). Each
corner kept is written before the next is looked at; an edge whose ends lie on two sides (strictly: a kept corner
followed by one over k, a lost one by one under k; the last back to the first) adds the point where it crosses
(`0x005394E0`: d . n not 0, t = (((o - v).z n.z + (o - v).y n.y) + (o - v).x n.x) / ((n.z d.z + n.y d.y) + n.x d.x)
within 0 .. 1, the point v + d t with w of v). A crossing not found still counts a corner, left as the buffer held it
— a stock problem.

Before the faces come the world's planes (its list `+0x34`, 0x20 bytes each: a point and a unit normal; a race's world
has none, the garage's test track scene one, `0x004C7DF0`). Each (`0x0053AB00`) is taken into the model's frame
(`0x00538250`: the point as x ((z m2 + y m1) + x m0) + m3, y ((x m4 + z m6) + y m5) + m7, z ((x m8 + z m10) + y m9)
+ m11, the normal without the position); when the body's sphere reaches it (|((cy - py) ny + nz (cz - pz)) + nx (cx -
px)| at most the radius) each line entering it from its front (`0x00539410`: d . n < 0, t = ((p - l) . n) / (d . n)
within 0 .. its length) touches it at l + d t, the depth the length less t, with the plane's normal, Mu 0.8,
restitution 0 and the zone of the world's `+0x8E0` (none without a route). The sphere colliders come first, the same
way (`0x00539330`): a centre within its radius of the plane on either side (|((c - p).z n.z + (c - p).y n.y) + (c -
p).x n.x| = |d| below the radius) touches at its foot c - n d (w of the centre), the depth the radius less d. A
world without a route (`0x0053DEA0`, vtable `0x006F9CC0`) takes only its planes and the bodies near; its grid has no
bounds, every body in the last cell.

The walls follow the faces: each wall segment found joins a marker to the next, with the first marker's zone. It is
two triangles facing the first marker's direction: (first marker at the zone's `rangeMin` above it, second marker at
`rangeMin`, first marker at `rangeMax`) and (first marker at `rangeMax`, second at `rangeMin`, second at `rangeMax`),
taken into the model's frame. With `+0x7C` each sphere collider is tried against the first triangle, then the second
(uncut, `0x005388C0`), then each line likewise; a wall's touch takes no offset and goes to the body at any depth.
Then, with `+0x80` and a mesh collider with a sphere tree, the mesh collider against both triangles, with no zone in
its touches and the offset of the last face of the pass (0 without faces) — a stock problem.

### The mesh collider against a triangle (`0x0053A660`)

The collider mesh (`[EndMeshCollider]`'s `.ocd`) is tried when one of the triangle's corners lies no lower than the
mesh's lowest vertex and the mesh has a sphere tree. From the root (`0x0053A3B0`) each child whose sphere reaches the
triangle is walked first, then the node's faces; a face is tried once per triangle (marked, the marks cleared after).

A sphere reaches the triangle (`0x00539FB0`) when its centre lies no farther than the radius in front of the plane
(behind is not limited) and the centre's foot on the plane lies inside, or one of the edges passes through the
sphere: the roots of |a + t e - c|² = r² (`0x00538620`, a = e . e, b = 2 (a - c) . e, disc = b² - 4 a q with
q = |c|² + |a|² - 2 a . c - r²) when the discriminant is over 0, not both below 0 and not both over 1.

A face of the mesh meets the triangle (`0x005396D0`) when neither lies wholly on one side of the other's plane (all
distances ≥ 0, or all ≤ 0). Along the line of the two planes (their normals' cross product, made unit) each gives the
span between the crossings of its two edges from its lone corner; the first crossing of the mesh's face is also kept
as a point. A lone corner level with another makes the test fail (for the track's second crossing at once). When the
spans overlap, the middle of the track's span, held within the face's, is the touch point on the line through the
face's crossing; the depth is the distance of the face's deepest corner behind the triangle. The point moves by the
offset along the triangle's normal and the depth takes the offset; deeper than 0 it touches with the mesh collider's
ID, the face's zone, the triangle's normal.

The meshes of a car (`.ocd`, the add-on's `OCD`) are read from the car's folder: `cars\<name>\<File>`.

## Bodies against bodies

After the route, a body's step tries each body the world found near it, the last of the list first (`0x0053EC30` →
`0x0053D0C0`). The other body's frame is taken into this one's: the inverse of this node's matrix times the other's
node (the nodes as the last sync left them). When the spheres overlap (`0x00539F50`: the other's centre taken over,
(r_this + r_other)² over (dx² + dy²) + dz²), each face of the other's collidee mesh (`[EndMeshCollidee]`'s `.ocd`) is
taken over (the normal ((z r2 + y r1) + x r0, (x r4 + y r5) + z r6, (z r10 + y r9) + x r8); the third and second
corners ((z r2 + y r1) + x r0) + r3, ((x r4 + y r5) + z r6) + r7, ((z r10 + y r9) + x r8) + r11; the first ((x r0
+ z r2) + y r1) + r3, ((y r5 + x r4) + z r6) + r7, ((z r10 + y r9) + x r8) + r11) and tried when this body's centre
of gravity (centimetres) lies in front of it or on it: ((a.x - c.x) n.x + (a.z - c.z) n.z) + (a.y - c.y) n.y ≤ 0.
Such a face goes, with `+0x7C`, cut by the clipping planes against the sphere colliders and then against the wheel
lines, with `+0x80` against the mesh collider, as a face of the route without an offset; the touches carry no zone,
the collidee's Mu and Restitution as the friction and the bounce, and the other body.

Before the faces, with `+0x7C`, each sphere collider meets each of the other's sphere collidees (`0x005395A0`), the
collidee's centre taken over (((x r0 + z r2) + y r1) + r3, ((x r4 + y r5) + z r6) + r7, ((y r9 + z r10) + x r8) +
r11, its w the other body's radius): the normal the collider's centre less the collidee's made unit (`0x00489910`,
w of the collider's), and when that distance is below the two radii added, the touch midway between the centres
(each axis (a + b) x 0.5, w of the collider's) with half the overlap as its depth, the sphere's ID, no zone, the
collidee's Mu and Restitution. After the faces, when this body has a mesh collider and `+0x80`, each of the other's
sphere collidees (taken over the same way) goes against it (`0x0053A890`): when the mesh has a sphere tree, from the
root (`0x0053A750`) each child whose sphere reaches it ((r_node + r)² over (dz² + dx²) + dy²) is walked first, then
the node's faces not yet met are listed (marked); each listed face (unmarked in turn) against the sphere
(`0x005388C0`) touches with the mesh collider's ID, no zone, the collidee's Mu and Restitution, the face's normal
turned round (w a value of the stack) and the other body.

These spheres and planes are in the data only for bodies no race makes (`07-dynamics.md`): the car parts' bodies
(`0x004191C0`) never join a world, and no track names `dynamic_tyrewall`.

A contact with another body measures its velocity against the other's at the point (`0x004F8E50`): the other's
v + w x r in its frame (r = the point less its centre, in its frame), turned into the world and then into this body's
frame. It carries no share of the weight.

## Impacts

Before the integration (`0x004F1770`) first each contact with another body, then each contact with the world, pressed
past its MaxCompression, with a
restitution of 0 or more, takes an impact (`0x004F0F90`) when its point closes along the normal (normal speed
vn < 0): j = -(1 + restitution x ramp(2, -vn, 5)) vn, divided by 1/m + n . ((I⁻¹ (r x n)) x r) (r from the centre),
along the world normal; the momentum takes it, the angular momentum r x (the impulse x 0.1; x 0.05 under -6 m/s).
A rigid contact adds its friction x h, turned into the world, to the impulse, and n x clamp(-w . n, -1, 1) x h to
the angular momentum. A contact with another body that moves apart under 1 m/s takes as its approach the excess
over MaxCompression (negative, at least -0.3) with a restitution of 1. Its impact is shared: j is divided by both
bodies' 1/m + n . ((I⁻¹ (r x n)) x r) summed, (mine + theirs) + 1/m_other + 1/m; this body takes j n plus the
friction part, the other -j n less it; over 30000 both take 4500 more upward, over 10000 both 0.15 j; each turns by
r x (its impulse x 0.1 / 0.05), this one plus the angular part, the other less it. After the integration (`0x004F1C70`)
the body moves out along the normal of the contact with the world pressed deepest past MaxCompression by half of the
excess. The momenta are set with their velocities (`0x004F2DC0`, `0x004F2EC0`).

## The world

Each frame of a race (`0x00425D40`) the physics takes the frame's time smoothed (`0x00425C00`): the time, held within
0 .. 0.2 s, joins a ring of the last 16; their mean (summed in order) less a tenth of the error kept so far is the
step time, the error grows by the step time less the frame's, and the step time is held within 0 .. 0.2 s; for the
first 50 frames the frame's own time is taken, at most 0.066 s. The step time is split in 1 step, 2 over 0.05 s, 3
over 0.1 s; before each step the contact pools are emptied and the world (`0x0053F110`) steps. The smoothing runs
every frame, the race paused or not; the steps only while it goes on. A restart of the race (`0x004261F0`) or of a
replay (`0x00426300`) clears the frames' count (`+0x78`): the next 50 frames take their own time again.

The world (the scene objects' manager's `+0x58`, which is also the collision `[0x0075F0C8]`) keeps three lists of
bodies: the primary ones (`+0x44`, their definition's Primary), the others (`+0x54`) and the active ones (`+0x64`). A
body joins (`0x0053E450`, refused when it is in a world already) the first or the second list; once the grid is made
it also goes in the grid and is woken. A body is active (`+0x74`) from its making when it is primary
(`0x00536F90`), else not. Waking (`0x0053EDF0`) puts a body that is not active on the active list, finds its near
bodies at once and makes it active; putting it to sleep takes it off (the last of the list in its place).

It keeps a grid of 16 x 16 cells over the route (made once, `0x0053E8D0`): the bounds in x and z of the
collision's vertices; each cell knows the cells either side (an edge cell the one inside) and keeps a list of its
bodies, the newest first. Making it puts every body of the first, then of the second list in its cell, and wakes each
that is not active (the second list's: a primary body is active). A step (`0x0053F110`) does nothing without a
primary body. Every primary body goes to the cell of its node (`0x0053E7E0`: (x - min) / (max - min) x
16 as a float, cut toward zero; 16 or more as an unsigned number is 15), leaving the old cell's list for the head of
the new one (`0x0053E770`). Then the next primary body in turn (the counter `[0x0079E894]` goes up before it is used,
back to 0 at the end) finds the bodies near it (`0x0053EE90`; none while it does not collide, `+0x78`): in its cell and those
around, each other body that collides and is primary, or within (r + r_other + 1000)² in x and z ((dx² + dz²) of
the nodes), up to MaxCollidees; when full a new one takes the place of the first kept one it outranks (primary over
not, or the same rank and nearer). Then the next active body in turn (`[0x0079E890]`, the same way) goes to its cell
and finds its near bodies; the other active bodies keep their cells. Both counters belong to the process and are
never reset: a race goes on from where the one before left them. The primary bodies step, then the active ones, each
list walked by index with its length read anew (a body woken in the step steps in it; one going to sleep has the
last of the list take its place, which is skipped); then the nodes of the primary bodies, then of the active ones,
follow their bodies (`[4] 0x004F2C00` → `0x004F01E0`): a body put to sleep keeps its node of the step before. A
primary body whose spheres overlap a body that is not primary wakes it (`0x0053D0C0`). A body is also synced when it is placed (`0x00429D40`, `0x004F2C10` and `0x004F3150` with their flag
set) and after each step of the grid's settling and a respawn. A player's car does not collide (`+0x78` = 0) while
its ghost time after a respawn lasts (`0x0041E200`).

## The race start

At the start (`0x004975A0`, after the race's state is reset) each car, in the grid's order, burns its nitro at 2.5 a
second (`+0x380`) in a race of type 6, is reset (`0x00486E90`: no velocity, unturned, first gear, the engine on and
still, no throttle, the wheels still, the frame's damage events `+0x4F4` .. `+0x504` cleared), gets its nitro (not in
a replay's race), loses its automatic gearbox flag, with a nitro part counts the low nitro message as given when its
car has none (`+0x198`), and its
centre is put on its grid place (centimetres x 0.01; `0x00429D40`) facing the place's heading (angles 0, heading, 0;
`0x004F3150`). It then settles in neutral, without brake or throttle, in steps of 0.02 s with the pools emptied:
first 75 steps for the first car of the game and 50 for every later one, during which, until it happens, a tyre
touching a surface whose world normal's y is over 0.5 stops the car and turns it back to the heading keeping its
pitch and roll; then the same stop and turn, 50 more steps, and a last stop. Before it settles its competitor's
rolling start fields (`+0x68`, `+0x6C`, `+0x70`) are cleared and the world's grid is made anew (`0x0053E8D0`).

## The driver's control

A player's car is driven (`0x0041E200`, after the frame's physics, with the step time) from the DRIVER1 actions of
the player's controller (`02-frontend.md`, "Input"): the analogue STEERING, THROTTLE, BRAKE, HANDBRAKE, REVERSE and
DRIVER_CAMERA_PAN, the digital PAUSE, SELECT, REAR_VIEW, GEAR_DOWN, GEAR_UP, CHANGE_CAMERA, RESET, NITRO, DRIVER_HORN,
NEXTCAR and the six DRIVER_TACTICS ones (the slots `+0x4C ..` and `+0xD4 ..` of `0x004080D0`).

- The throttle is the action squared above 0.2, else 0; the brake cubed above 0.2.
- The steering input s (the speed: the body's local velocity's length, m/s; mph x 2.235):
  - Without the steering wheel (the save system's `+0x78`, STEERING WHEEL of `opts_wheel_dx9`, 0 for a new
    driver): s is shaped by the profile's sensitivity (the save system's `+0xC4` per player, 0.5 for a new
    driver): sign(s)
    |s|^((1.1 - 1.1 x sensitivity) + 0.07, 1.01's float; 1.00 added 0.05, the last sum in double), held within ±1. The target is s x ((1 -
    min(speed x 2.235, 140) / 140) x 0.75 + 0.25).
  - With the steering wheel and the registry's MultiTurn (`[0x0075F78C]`): the target is (sensitivity + 1) x s,
    unshaped, held within ±1.
  - With the steering wheel, not speed sensitive (the save system's `+0xAC` per player, the opposite of
    `opts_wheel_dx9`'s SPEED SENSITIVE tick): s shaped with the power (1.65 - 1.65 x sensitivity) + 0.8 (`[0x007252F0]`,
    `[0x007252EC]`), held within ±1, is the target (held again), not lessened at speed.
  - With the steering wheel, speed sensitive: shaped with the same power (`[0x007252E8]`, `[0x007252E4]`) and
    lessened at speed as without the wheel.
  The wheel's turn (`+0x2C`) moves towards the target at most 0.61 a second (`+0x54`; x 3 when the input does not
  pull the same way as the difference; 3000 with the steering wheel, at once), and drops to 0 when it lies on the
  other side of the target. The car's steering is SteeringFactor x 0.83 (`+0x4C`) x the turn, held within ±1
  (`0x004203E0`).
- With the automatic gearbox (the body's `+0x364`, set each frame: 0 in a drag race, type 6, else the settings'
  `+0x30` per player, 1 for a new driver; `0x0041F244`): REVERSE over 0.2 takes reverse and is the throttle,
  squared; else the throttle over 0.2 takes neutral when standing (|forward speed| under 0.1 m/s) on a brake over
  0.9, or first gear from reverse or neutral. By hand GEAR_DOWN takes the gear below (not below reverse), GEAR_UP
  the gear above (not past the top).
- A car whose steering pulls (the car's `+0x64`) turns 0.05 more to its side (`+0x4F8`: 0 the left) after that,
  held within ±1.
- The handbrake is 1 above 0.2, and 1 while the race's count before the start is not 0. From a count of 10 up
  (the fly-by) the actions are not read at all.
- The nitro (NITRO, held; `0x00488FA0` asks it, on only while the body has some left (`+0x37C`) or the race is a
  replay's, a new start flashing the HUD's gauge for 3 frames): in an arcade race whose record gives endless nitro
  (its `+0x04`, `0x0042BA40`), once the count is 0, the body's nitro is set to 15 every frame and the gate below
  decides; else NITRO held at a count of 0 goes to the gate, and otherwise a car with a nitro part asks it off. The
  gate: on with a nitro part (`+0x184`), the throttle over 0.9, no brake and a forward gear (2 up), else off. Then
  a nitro on goes off with the throttle below 0.8 or the brake over 0.35.
- RESET held at a count of 0 ends the controls there; else PAUSE is looked at (`22-race-hud.md`) and the body's
  `+0x38C` (its traction control's mode) is 0 in a cruise, 1 otherwise.

The settings are read from the profile's settings object (`+0x84` and `+0xC4` of the save system, the settings'
`+0x30` and `+0x70`).
