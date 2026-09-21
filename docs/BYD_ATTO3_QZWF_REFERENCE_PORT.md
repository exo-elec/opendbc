# BYD Atto 3 — crosscheck against qzwf's on-car reference port

**Date:** 2026-09-21
**Reference:** `qzwf/opendbc` @ `fbcbc579` (branch `master`, 2026-08-13), submitted upstream as
[commaai/opendbc#3677](https://github.com/commaai/opendbc/pull/3677) — "BYD ATTO3: new brand
port (lateral)", **draft**, opened 2026-08-11, last touched 2026-09-16. MIT, same licence as
this fork.

## Why this port and not another

The trigger for this check was a report that "Blink Drive" had released comma 4 support for
the Atto 3 in Thailand. **No Blink Drive code exists that I can find.** `blinkdrive555` is a
Thai installer/dealer whose post on the subject says, in Thai, that the Atto 3 *can* run
openpilot having "just seen an Australian" do it — it points at someone else's work rather
than announcing their own. There is no `blinkdrive` GitHub org doing openpilot work (the
`blnkd` org is a Swiss driving school), and **BYD does not appear anywhere in
`commaai/openpilot`'s `docs/CARS.md`**, so there is no official comma 4 BYD support to
inherit either.

What does exist, and is the strongest evidence available, is qzwf's port:

- **Driven daily on a comma 3X**, lateral only, stock ACC for longitudinal.
- Upstream PR with 34 safety tests and a test route
  (`b8b2d1a1df1b3aad|000000a0--7c51639c22`) that moves the car out of `non_tested_cars`.
- Dated on-car measurements in the source comments (a 2026-08-07 drive, a 2026-08-09 drive,
  3.6 h of driving, 1111 of 1121 frames), not inference.
- It is already this fork's `byd-atto3-stable` crosscheck lineage — see
  `BYD_ATTO3_COMMUNITY_LINEAGE.md`. This is the same people, further along and now upstream.

Two caveats before adopting anything: qzwf's car is **India-market**, ours is Thailand, and
their port is **angle-lateral only** — it has no validated longitudinal, no `mpc_lka` path, and
supports one platform where this fork supports seven.

## Where we already agree

Worth stating, because it bounds the risk of the rest:

- **Checksum.** Their `byd_checksum(byte_key, dat)` and our `cam_lka/bydcan.py::byd_checksum`
  are algebraically identical — same 0xAF key, same nibble sums, same `(-x + 9) & 0xF`, same
  `+ (5 - remainder)`. Theirs masks the assembled byte where ours masks the high nibble first;
  both give the same result for every input, including the negative-carry case. Ours excludes
  the last byte by slicing, theirs by zeroing it before the call.
- **`0x1E2` frame structure.** Identical field for field: `STEER_ANGLE 24|16@1-` ×0.1,
  `STEER_REQ` @21, `SET_ME_FF` @40–47, `SET_ME_F 48|4`, counter @52–55, checksum byte 7.
- **`0x3B0 PCM_BUTTONS`.** Byte-identical to ours: SET @3, RES @4, LKAS @6, DEC @15, INC @16,
  ACC_ON @19, counter @52–55, checksum byte 7.
- **`0x418 BSM`.** `RIGHT_APPROACH` @17, `LEFT_APPROACH` @23 — exactly ours.
- **`0x122`.** Four 16-bit wheel speeds ×0.1 — exactly our `WHEEL_SPEED_ALT`.
- **`0x32E` field layout.** `ACCEL_CMD 0|8`, `ACCEL_FACTOR 32|4`, `DECEL_FACTOR 24|4`,
  counter @48–51, checksum byte 7 — ours and `byd_atto3.dbc` agree too.
- **`0x342`.** Gas byte 0, brake byte 1, both ×0.01.

## What their measurements change for us

Cross-referenced against `BYD_Atto3/DOC/protocol_consistency_audit.md`, which found twelve
divergences between `byd_atto3.dbc`, this fork, and openpilot. qzwf is a third independent
lineage, so it breaks several of those ties.

### Resolved in this fork's favour

- **`0x418` BSM** (audit finding 9) and **`0x3B0`** — two lineages now agree with
  `byd_general_pt.dbc`. `byd_atto3.dbc`'s `VCU_BSDLeftApproach 8|2` / `VCU_BSDRightApproach
  10|2` is the outlier.
- **`0x11F` byte 4** is a **counter**, not a checksum (`byd_atto3.dbc` calls it
  `SAS_unknown_checksum_11F`). qzwf: `COUNTER 39|8@0+`; ours: `MSG_COUNTER 32|8` — same byte.
- **`0x342` gas is ×0.01**, not the ×1 "%" in `byd_atto3.dbc`.
- **`0x1FC` motor torque lives in bytes 0–1 at ×0.1** (audit finding 3). qzwf:
  `MAIN_TORQUE 0|16@1-` ×0.1; ours: `STATE 0|4` + `MAIN_TORQUE 4|12@1-` ×0.1. They differ only
  on whether the low nibble belongs to the value. `byd_atto3.dbc`'s `EPS_MainTorque 32|12` ×1
  is the outlier — and its own `CM_ BO_ 508` already concedes bits 2–31 are undecoded there.
  Our `CURVATURE 16|16` remains uncorroborated (qzwf defines nothing there).

### Resolved against this fork

**1. `steeringTorque` and `steeringTorqueEps` are swapped (audit finding 2). Confirmed.**

```python
# qzwf/opendbc  opendbc/car/byd/carstate.py
ret.steeringTorque    = cp.vl["STEER_MODULE_2"]["DRIVER_EPS_TORQUE"]   # column sensor
ret.steeringTorqueEps = cp.vl["STEERING_TORQUE"]["MAIN_TORQUE"]        # EPS motor output
```

We assign the reverse. Their comment — "MAIN_TORQUE (0x1FC) is total EPS motor output — NOT
driver input" — is the same conclusion `byd_atto3.dbc`'s `CM_ SG_ 508` records from two
lineages. Three sources now agree; our `cam_lka/carstate.py` is alone.

**2. Wheel speed: wrong frame, and ~32.5% high (audit finding 8).**

qzwf reads per-wheel speed from **`0x122`**, not `0x1F0`, and applies a measured correction:

```python
# DBC factor 0.1 gives km/h, but BYD ATTO3 India raw values read ~32.5% high
# vs odometer at steady-state. Correction: 40/53.
_SPD_CORR = 40.0 / 53.0          # 0.1 * 40/53 = 0.0754717 km/h per count
```

Their safety mode reads `0x122` too (`BYD_WHEEL_SPEED 0x122U  // 290 - per-wheel speeds`).
Their `0x1F0` is a **single 16-bit `WHEELSPEED_CLEAN` at `0|16@1+`** — structurally the same
call `byd_atto3.dbc` makes with `ESP_VehicleSpeed 0|16@1+`.

So on `0x1F0` our four-12-bit-wheels-plus-fault-bits layout is contradicted by both other
lineages, and `carstate.parse_wheel_speeds`, `radar_interface`, and `byd.h`'s
`UPDATE_VEHICLE_SPEED` all read it. On scale, qzwf's odometer-derived 0.0754717 and
`byd_atto3.dbc`'s capture-derived 1/14 (0.0714286) are 5.7% apart, while our uncorrected ×0.1
is ~32% above both — which is exactly the error qzwf measured and corrected for.

This matters beyond `vEgo`: it is the speed our `byd.h` interpolates the steering angle-rate
limits against.

**3. Bit 20 of `0x1E2` is held at 0, not driven.**

```python
# qzwf bydcan.py
"STEER_REQ": 1,
"STEER_REQ_ACTIVE_LOW": 0,
# Note STEER_REQ_ACTIVE_LOW is *not* the inverse of STEER_REQ despite its name — the
# camera holds it at 0 in both states.
```

We drive the same bit (`EPS_OK`, and `byd_atto3.dbc`'s `MPC_SteerRequestActiveLow`) as
`eps_ok = not steer_req`. While steering our bytes 0–2 come out `2B 55 0B`, which matches the
camera constant qzwf measured (`2b 55 eb` — the 0xE nibble is bits 20–23, and bit 20 is 0 in
both). **Idle is where we diverge: we emit byte 2 = `0xD0`, the camera emits `0xE0`.**

I could not verify this against this project's own captures — `BYD_Atto3/LOG/` is gitignored
and absent from the checkout. It is a cheap check against any existing factory-MPC BLF:
decode `0x1E2` bit 20 with the camera steering and idle.

**4. We transmit `0x1E2` when not steering; they never do (audit finding 7).**

Their carcontroller sends the steering frame and `0x316` **only inside `if CC.latActive:`**.
Their safety uses `disable_static_blocking` plus a 150 ms takeover timeout, so panda blocks
the camera's copy only while openpilot is actually transmitting, and hands the EPS back to the
stock LKAS the moment it stops. There is never a window with two sources, and never an
"inactive" frame to get the encoding right.

Ours transmits every other frame unconditionally, and `create_can_steer_command` zeroes
`STEER_ANGLE`, `UNKNOWN`, `SET_ME_X01` and `SET_ME_XE` when `steer_req` is false. That is both
the audit's finding 7 (our own `steer_angle_cmd_inactive_check` wants the inactive angle to
track `angle_meas`, not sit at 0) and a frame the camera is never seen to emit.

**5. The steering-frame constants should be latched from the camera, not hardcoded.**

qzwf reads `UNKNOWN`/`SET_ME_X01`/`SET_ME_XE` off the camera's own bus-2 frame, requires them
stable for 5 frames before adopting (the camera ramps in over a frame or two), and falls back
to the captured `{2773, 1, 0xB}` only if the camera has never steered. Their measurement:
**1111 of 1121 `STEER_REQ` frames carried exactly that triple, at every speed.**

We hardcode them, and `create_can_steer_command` swaps `SET_ME_XE` to `0xE` at standstill:

```python
set_me_xe = 0xE if is_standstill else 0xB
```

qzwf observed `0xB` at *every* speed, and documents that walking these fields is what made the
car "drop the entire stock ADAS — `ACC_HUD_ADAS` `ACC_ON1`/`ACC_ON2` and every `ACC_CMD`
engagement bit go to zero together". A standstill-only variant is not the same as walking
them, but it does put a triple on the wire that their capture never saw.

Decoded through `byd_atto3.dbc`'s layout for these bits, our two variants are:

```
SET_ME_XE = 0xB -> MPC_SteerAngleRateUpper = +299, MPC_SteerAngleRateLower = -299
SET_ME_XE = 0xE -> MPC_SteerAngleRateUpper = +299, MPC_SteerAngleRateLower = -107
```

i.e. the standstill variant narrows one side of what `byd_atto3.dbc` reads as a steering
angle-rate envelope. That is a coherent reading of the field, but it is our inference against
their measurement.

**6. Angle limits: three different sets, and ours are loose to cover a bug.**

| | max angle | rate up (bp / v) | rate down |
| --- | --- | --- | --- |
| qzwf `values.py` **and** `byd.h` | **90°** ("the EPS faults past this") | `[0,5,25] / [2.5,1.5,0.4]` | `[0,5,25] / [2.5,1.5,0.6]` |
| ours `values.py` | 120° | `[0,5,15] / [6,4,3]` | `[0,5,15] / [8,6,4]` |
| ours `safety/modes/byd.h` | 450 CAN = 45° | `[0,5,15] / [28,26,22]` | same |

qzwf's Python and C agree with each other; ours agree with neither. At road speed theirs is
**7.5× tighter** than our `values.py` and ~35× tighter than our C backstop.

The reason ours is inflated is in our own `byd.h` comment: a single violation resets
`desired_angle_last` to 0 while the next frame may carry a ~20°+ absolute command, so tight Y
values burst-block TX. **qzwf fixes the cause instead**, with a windup clamp in the
carcontroller:

```python
MAX_ANGLE_ERROR = 12.  # deg
apply_angle = np.clip(apply_angle,
                      CS.out.steeringAngleDeg - MAX_ANGLE_ERROR,
                      CS.out.steeringAngleDeg + MAX_ANGLE_ERROR)
```

With the command structurally unable to drift more than 12° from the measured angle, the
saturation-and-reset failure cannot happen, and the safety Y values can be the real ones.

**7. We disable every Rx counter and checksum check; they don't.**

Every `RxCheck` in our `byd.h` sets `.ignore_checksum = true, .ignore_counter = true`. qzwf
validates where the frame has them — `max_counter = 15U` with checksums live on `0x1FC`,
`0x342` and `0x32E` — and only ignores them on `0x11F`, `0x122` and `0x242`. Since both forks
implement the same checksum, this is available to us.

### Not resolved by qzwf — the longitudinal scale is unmeasured everywhere

Audit finding 1 stands, and qzwf makes it worse rather than better. Three lineages, three
different `ACCEL_CMD` scales, none of them measured on a car except ours-by-inference:

| Source | m/s² → raw | Status |
| --- | --- | --- |
| `byd_atto3.dbc` | ×20 (factor 0.05, offset −5) | derived from CANape captures; the +30 clip lands on exactly the +1.50 m/s² ceiling the COMMA captures recorded |
| this fork | ×26 (`ACCEL_MULT[BYD_ATTO3]`) | no stated derivation |
| qzwf | ×16.67 | **explicitly disclaimed**: "The ACCEL_CMD scale below is inferred, not measured — calibrate it on the car before enabling longitudinal" |

qzwf also keeps `ACC_CMD` **out of the TX allowlist** entirely, so their number never reaches
a car. We ship ours. The capture evidence in `BYD_Atto3` remains the only measurement anyone
has, and it says our commands are 30% high.

### Other measured findings worth importing

- **`0x242`'s `BRAKE_PRESSED` bit 37 is dead on their car** — "byte 4 is a constant 0x0C",
  verified over 3.6 h. We use it OR'd with `BRAKE_PEDAL > 0.01`, so we degrade gracefully;
  `byd_atto3.dbc`'s `CM_ SG_ 578` already logs this as an unresolved market/firmware conflict
  and this is a second data point for "dead".
- **`0x220 PEDAL_PRESSED` is the brake-light switch, not the driver's foot** — "86% of its
  assertions on the road happened while the camera's ACC was commanding decel". Do not adopt
  it as `brakePressed`.
- **`0x122`'s `WHEELSPEED_BR` is not 16-bit** — "byte 7 is a constant status byte (0x41), NOT
  the high byte of the wheel speed. DBC wrongly declares it as 16-bit." Our
  `WHEEL_SPEED_ALT.WHEELSPEED_BR 48|16` has the same error. They derive RR from the other
  three wheels.
- **Cruise engagement comes from `ACC_CMD`, not the HUD.** Theirs:
  `ACC_ON_1 && ACC_ON_2 && !CMD_REQ_ACTIVE_LOW`, with a 5-frame drop debounce, and
  `cruiseState.available` from the HUD's `ACC_ON1/ACC_ON2` main-switch bits. Their comment
  explains the trap (25 min of stop-and-go, driver braking 28% of the time, both HUD bits set
  throughout) — the same trap `byd_atto3.dbc`'s `CM_ SG_ 813` records. Our
  `cam_lka/carstate.py` reaches a similar place through a hand-rolled `is_cruise_latch` with
  standstill and low-speed special cases; theirs is two lines and measured.
- **Driver override threshold.** Ours: `abs(steeringTorqueEps) > 6`. Theirs:
  `steeringTorque > 80`, with "observed max ~52 during normal turns". `byd_atto3.dbc`'s
  `CM_ SG_ 287` notes a third value (>10) and warns the mapping is not proven portable. Ours is
  by far the most trigger-happy of the three.
- **Brake threshold in C.** Ours: `brake_pressed = data[1] > 0`. Theirs:
  `data[1] > BYD_BRAKE_THRESHOLD` (3), matching their Python `> 0.03`.
- **Vehicle specs.** Wheelbase agrees (2.72). Mass 2090 (ours) vs 1750 (theirs); steer ratio
  16.0 vs 14.8; they also set `tireStiffnessFactor = 0.7983`, we leave it default.

## Resolution status (2026-09-21)

Everything with enough evidence to act on has been applied on this branch. Everything
that needs a capture is annotated in the DBC where an engineer will hit it, rather than
guessed at.

### Fixed

| # | What | Where |
| --- | --- | --- |
| 1 | `steeringTorque` / `steeringTorqueEps` swapped | `cam_lka/carstate.py` |
| 2 | `create_steering_torque_spoof_camera` fed the wrong signal after (1) | `cam_lka/carcontroller.py` |
| 3 | `ACCEL_CMD` scale: DBC now carries 0.05/-5, `ACCEL_MULT` removed | `byd_general_pt.dbc`, `cam_lka/bydcan.py`, `values.py`, `cam_lka/carcontroller.py` |
| 4 | Inactive `0x1E2` transmitted angle 0 instead of the measured angle | `cam_lka/bydcan.py` |
| 5 | `acc_pressed` and `cancel_pressed` read the same bit | `safety/modes/byd.h` |
| 6 | `max_angle` 450 (45°) vs `STEER_ANGLE_MAX` 120° | `safety/modes/byd.h` |
| 7 | `bl_raw` actually reads `WHEELSPEED_FR` | `safety/modes/byd.h` |
| 8 | No BYD test coverage for any of the above | `car/byd/tests/test_protocol_consistency.py` (12 tests) |

Notes on the ones that aren't cosmetic:

- **(1)** `mpc_lka/carstate.py` already had it right, so this was an inconsistency inside
  this fork as well as against the other two lineages. `steeringPressed` already read the
  driver signal and keeps its threshold of 6, so override sensitivity is unchanged. The
  Seal 6 override block was reading the EPS motor value through `CS.out.steeringTorque`
  and now reads it through `steeringTorqueEps` — same signal, same thresholds, so Seal 6
  behaviour is unchanged. Moving it onto real driver torque needs those two thresholds
  re-tuned on a car.
- **(3)** The raw TX envelope is unchanged — raw 20..130 before and after — so the
  `byd.h` longitudinal limits and the authority they allow are untouched. What changed is
  the mapping inside it, from 1.30× to 1:1. `ACCEL_MULT` was a units conversion standing
  in for a DBC scale, with no stated derivation for its value; the DBC now describes
  itself. **Still needs bench validation before hardware use.**
- **(4)** `apply_std_steer_angle_limits()` in `car/lateral.py` sets the inactive angle to
  the measured angle "on all angle cars", and `steer_angle_cmd_inactive_check()` requires
  it — the carcontroller was already passing the measured angle in and `bydcan` was
  overwriting it with 0.
- **(5)** Behaviour-preserving: the grant ran first and the clear second on the same bit,
  so that bit could never leave `controls_allowed` set, and it still cannot. What it
  removes is a dead grant term that read as though ACC_ON_BTN engaged control.
- **(6)** `max_angle` bounds the inactive-angle window and the post-violation reset of
  `desired_angle_last`, not the active command. At 450 the reset landed up to 75° away
  from the angle being commanded whenever the wheel was past 45°, which is the
  burst-blocking `byd.h`'s own comment describes. The active command is bounded by the
  rate lookup and, in the controller, by `MAX_STEER_ANGLE_OFFSET_DEG = 10`.

**Correction to an earlier draft of this document:** it recommended adopting a
qzwf-style windup clamp. This fork already has one — `MAX_STEER_ANGLE_OFFSET_DEG = 10`
in `cam_lka/carcontroller.py`, clamping the command to ±10° of measured, against qzwf's
±12°. The burst-blocking cause was `max_angle`, fixed above, not a missing clamp.

### Deliberately not changed, annotated instead

Each of these is now a `CM_` comment in the DBC that owns the signal, so it surfaces in
cabana and in any tool that reads the file:

| What | Why not | Annotated in |
| --- | --- | --- |
| `0x1F0` structure and scale | Two lineages say one 16-bit field; two scales disagree by 30%. Needs a local GPS or odometer run. Affects `vEgo` and the speed the angle-rate lookup interpolates against. | `byd_general_pt.dbc` `CM_ BO_ 496` |
| `0x1E2` bit 20 (`EPS_OK`) | qzwf measured the camera holding it at 0 in both states; this fork drives it as `not steer_req`. BYD_Atto3 has the captures to settle it. | `byd_general_pt.dbc` `CM_ SG_ 482 EPS_OK` (pre-existing, still accurate), `byd_atto3.dbc` `CM_ SG_ 482 MPC_SteerRequestActiveLow` |
| `0x1E2` `SET_ME_XE = 0xE` at standstill | qzwf measured `0xB` at every speed over 1111 frames. Changing the steering template without a local capture is the exact failure mode they documented. | `byd_general_pt.dbc` `CM_ SG_ 482 UNKNOWN` |
| `0x316` `SET_ME_XFF` sitting on `LKAS_Output` | Two lineages place a signed 11-bit torque there; qzwf's own `0x316` defines nothing in that region, so nothing corroborates or refutes it. | `byd_general_pt.dbc` `CM_ SG_ 790 SET_ME_XFF` |
| `0x122` `WHEELSPEED_BR` width | qzwf measured byte 7 as a constant 0x41, but the real width is unknown and nothing in this fork reads the frame. | `byd_general_pt.dbc` `CM_ SG_ 290 WHEELSPEED_BR` |
| `0x1FC` torque position in `byd_atto3.dbc` | Per that repo's own policy, disputed fields are annotated and left in place until a local BLF/MF4 decode. | `byd_atto3.dbc` `CM_ SG_ 508 EPS_MainTorque` |
| Angle rate lookup (28/26/22 vs 6/4/3) | Tightening the C backstop toward `values.py` is the right direction but changes control behaviour under load. Wants a bench run, not an argument. | `byd.h` comment |
| `steeringPressed` threshold (6 / 10 / 80) | Raising it makes override detection *less* sensitive. Not a change to make from another market's car. | `cam_lka/carstate.py` `STEER_DRIVER_TORQUE_THRESHOLD` |

### Still open everywhere

The `0x32E` longitudinal scale has no on-car measurement in any of the three lineages.
This fork now ships the only value with evidence behind it (BYD_Atto3's captures), which
is a better position than an underived 26, but it is not a measurement made through this
port. **Bench-validate longitudinal before hardware use.**

### Verification

`opendbc`: 4197 tests pass (`unittest discover -s opendbc`), including the 12 new ones in
`car/byd/tests/test_protocol_consistency.py` and the 3372-test safety suite that compiles
`byd.h`. `ruff check .` reports one pre-existing error in `car/__init__.py`, unrelated and
present before these changes.

## Things that do not transfer

- Their port has **no `mpc_lka` path**, no Seal/Sealion/Shark/M6/Seal 6, no radar, no
  longitudinal, and no firmware fingerprints. This fork's multi-platform structure has no
  counterpart there.
- They use `HUD_MULTIPLIER = 0.718`; we use `1.12`. These are not comparable until the
  underlying speed scale is settled — the multiplier is downstream of it.
- Their `0x316` HUD field names (`STEER_ACTIVE_1_1/1_2/1_3`) do not match ours or
  `byd_atto3.dbc`'s. They do corroborate that `STEER_ACTIVE_ACTIVE_LOW` is **not** the inverse
  of the active bits — "sending the inverse is what made the cluster show LKAS engaged at all
  times". Worth checking our `create_lkas_hud` against on the bench.
- Nothing here corroborates or refutes `byd_general_pt.dbc`'s `SET_ME_XFF` sitting on
  `LKAS_Output` (audit finding 4) — qzwf's `0x316` doesn't define that region either.
