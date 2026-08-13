# BYD / Chery (Omoda, iCAUR, JAECOO) / MG car port

**Status:** lives on branch `port/upstream-bump-byd-chery-jaecoo`
([PR #1](https://github.com/exo-electronics/opendbc/pull/1)), not merged to
`master`. The `dev/EOP10` submodule pin (`opendbc_repo@2cde2462`) is
unchanged — bumping it is a separate, deliberate step (see "What this is
not" below). PR #1 currently shows a merge conflict against `master`
(unrelated upstream movement, e.g. `Update CARS.md`) — not yet resolved,
resolve before merging.

## Where this came from

- BYD + Chery/Omoda/iCAUR/JAECOO: ported from
  [kommuai/opendbc](https://github.com/kommuai/opendbc) (MIT licensed, same
  as this fork). JAECOO is Chery's sub-brand, so JAECOO support here is the
  Chery module (`opendbc/car/chery/`) — there is no separate "JAECOO" car
  directory. Also pulled in was BYD's own multi-DBC, `cam_lka`/`mpc_lka`-split
  port (`opendbc/car/byd/`), more complete than the single-DBC BYD Atto 3
  port `dev/EDP10` vendors inline.
- MG (ZS EV + non-EV/ICE): ported from
  [dragonpilot](https://github.com/dragonpilot/dragonpilot)'s `opendbc_repo`.
  **License note:** dragonpilot's own original contributions (not the
  inherited comma.ai-derived base) carry a separate `LICENSE.md`
  (Copyright (c) 2019, Rick Lan) restricting commercial use without explicit
  written permission — this includes MG support and the generic
  `radar_interface.py`/`u_radar.dbc` universal-radar-retrofit code (which
  has the same restriction embedded as a file-level header, confirmed by
  direct inspection, and was for that reason **not** ported — see "Known
  gaps" below). The MG port was authorized directly by exo-electronics
  (verbal arrangement with dragonpilot's author, not a written license grant
  — worth formalizing).
- Toyota DSU-disconnect, VW PQ `HCA_Status` patch, GM Bolt EUV neural
  feedforward: ported from `dev/EDP10`'s own hand-authored patches (itself
  forked from dragonpilot some time ago), identified via a full 205-file
  audit of `dev/EDP10`'s `opendbc_repo` against its real parent
  (`dragonpilot`, not this fork — diffing directly against this fork is
  noise, the lineages diverged ~1.5 years ago).

## Platforms

- **Chery** (`opendbc/car/chery/`, `chery_general_pt.dbc`,
  `opendbc/safety/modes/chery.h`): `CHERY_JAECOO_J7_PHEV` (2024-26, marked
  "under validation" upstream), `CHERY_TIGGO_8_PRO` (forced selection only,
  see "Known gaps"), `CHERY_OMODA_5`, `CHERY_JAECOO_6T` (renamed from
  `CHERY_ICAUR_03` — iCAUR isn't a market badge sold in Thailand; this
  model is badged Jaecoo 6T there). Lateral-only (angle control), no
  longitudinal control, no radar.
- **BYD** (`opendbc/car/byd/`, `byd_general_pt.dbc` +
  `byd_han_dmev_2020.dbc` + `byd_radar_fd.dbc`/`byd_radar_seal6_fd.dbc`,
  `opendbc/safety/modes/byd.h`): `BYD_ATTO3`, `BYD_M6`, `BYD_SEAL`,
  `BYD_SEALION7`, `BYD_SEAL6`, `BYD_SHARK` (angle control, `cam_lka`),
  `BYD_SONG_PLUS_DMI_21` (torque control, `mpc_lka`).
- **MG** (`opendbc/car/mg/`, `mg.dbc`, `opendbc/safety/modes/mg.h`):
  `MG_ZS` (EV + non-EV/ICE variants, `MgSafetyFlags.NON_EV` param bit).
  Torque control. No FW/CAN fingerprint yet — forced selection only
  (`opendbc/car/mg/fingerprints.py` documents this explicitly).

## `car.capnp` `SafetyModel` enum numbering

`byd @35` deliberately matches `dev/EDP10`'s own
`opendbc_repo/opendbc/car/car.capnp` numbering (EDP10 already carries a BYD
Atto 3 port) — a mismatched number would decode a recorded route's safety
model to the wrong brand if the route is ever opened on the other branch.
`chery @36` is new; EDP10 has no Chery port to match against. `mg @37` is
new — dragonpilot's own numbering uses `mg @35`, which would collide with
`byd` here, so this fork uses its own next-free slot instead of matching
dragonpilot's number.

## The EOP10/NGP10 compatibility contract (read this before touching `torque_from_lateral_accel`)

This fork's `opendbc/car/interfaces.py` is a **shared submodule dependency**
consumed by `dev/EOP10`, `dev/NGP10`, and (eventually) `dev/EDP10`. GM's
Bolt EUV neural feedforward needed a richer calling convention than the
existing `torque_from_lateral_accel()` API provides
(`roll_compensation`/`vego`/`aego`, not derivable from `CarState` alone),
but `dev/EOP10` and `dev/NGP10`'s own `selfdrive/controls/lib/
latcontrol_torque.py` still call the *old* 2-arg convention today:

```python
self.torque_from_lateral_accel = CI.torque_from_lateral_accel()
self.lateral_accel_from_torque = CI.lateral_accel_from_torque()
...
output_torque = self.torque_from_lateral_accel(output_lataccel, self.torque_params)
```

So the base API is **frozen as-is**:

- `torque_from_lateral_accel()` returns a callable with signature
  `(lateral_acceleration: float, torque_params) -> float`. Do not change
  this signature or what it returns for any existing brand — every existing
  consumer of this fork depends on it exactly as-is.
- `lateral_accel_from_torque()` (the reverse mapping) **must stay present**.
  `dev/EOP10`/`dev/NGP10`'s `latcontrol_torque.py` call it for
  `pid.set_limits()`. It has no other purpose but removing it breaks both
  branches' `LatControlTorque.__init__`.
- The neural-FF path is **strictly additive**: a new
  `torque_from_lateral_accel_neural_fn()` on `CarInterfaceBase` returns
  `None` by default; GM overrides it to return a callable for
  `CHEVROLET_BOLT_EUV` only, taking the new `LatControlInputs(
  lateral_acceleration, roll_compensation, vego, aego)` NamedTuple. Callers
  that don't know about it (every current consumer) are completely
  unaffected — they never call it.

**`dev/EDP10` does NOT currently satisfy this contract either way** — its
own rewritten `latcontrol_torque.py` calls `CI.torque_from_lateral_accel()`
and then calls the result with the *new* 3-arg `LatControlInputs`-based
signature directly (not via the opt-in accessor), and its `interfaces.py`
long ago dropped `lateral_accel_from_torque` entirely. Submoduling this
fork into EDP10 as-is will break EDP10's controller with a `TypeError`
(too many positional args) the first time `torque_from_lateral_accel` is
called on any car. This is real, un-fixed EDP10-side work — see "What this
is not" and `docs/upstream-audit/DELTA_AUDIT.md`.

**The rule for future changes to this API:** if you need a richer calling
convention for some brand's feedforward, add a new opt-in accessor that
defaults to `None`/absent, exactly like
`torque_from_lateral_accel_neural_fn()`. Never change what
`torque_from_lateral_accel()`/`lateral_accel_from_torque()` already return
for a signature that existing consumers depend on.

**A second opt-in accessor, same pattern, for a different reason:**
`gm/interface.py`'s `torque_from_lateral_accel_legacy_siglin_fn()`
reproduces `GMC_ACADIA`/`CHEVROLET_SILVERADO`'s torque formula *without*
the constant offset term (`d`) this fork's normal formula includes.
`dev/EDP10` carries comma.ai's own PR #2528 ("Torque controller: refactor
calculations to be in accel space", `74bfaa2c`, 2025-08-15) without its
revert three days later (`4e50498a`, 2025-08-18, no stated reason) — and
confirmed it currently drives real Acadia/Silverado vehicles on that
formula. This fork's normal behavior (with `d`) is comma.ai's own
considered, currently-supported design and is very likely the *more
correct* one — but real deployed vehicles shouldn't have their steering
formula change as a side effect of an unrelated submodule conversion.
This accessor exists so a migrating consumer can explicitly opt into
preserving its exact current behavior instead, verified bit-for-bit
identical to `dev/EDP10`'s live output. It should not be reached for by
default, and it should not exist for any brand/car beyond this one
documented migration case.

## What changed versus a straight copy

This fork's core was bumped from v0.2.1 (2025-02-10, predates the
`opendbc/safety/` migration entirely) to current upstream first — none of
the ported brands import against that old core at all. On top of the bump,
real API drift between the source forks and this one's newer core required
fixes, not just a file copy:

- `car.CarParams`/`car.CarState` (pre-migration `cereal` import) →
  `opendbc.car.structs`, across all three brands.
- `_get_params()` signature: this fork uses 7 positional args (no
  `dp_params`, no dragonpilot-heritage params bitmask); dragonpilot/EDP10
  use 8. Every ported `interface.py` had its signature adjusted.
- Fields moved into `car.capnp`'s `deprecated :group` in this fork's core
  (`.brake`, `.startingState`, `.startAccel`, `.stoppingDecelRate`,
  `.vEgoStopping`, `.enableDsu`) → accessed as `ret.deprecated.X` /
  `self.CP.deprecated.X` everywhere. **This bit a real port** — an early
  attempt at the Toyota DSU-disconnect port used `ret.enableDsu` directly
  and raised `AttributeError` at runtime; caught by empirical testing, not
  by inspection. `dev/EDP10`'s own `car.capnp` has no `deprecated` group at
  all — these fields stay top-level there, which is the mirror image of
  this same hazard for the eventual submodule conversion.
- `AngleSteeringLimits` (byd.h): dropped fields this fork's C struct
  doesn't have; verified every one was a no-op in the source config
  already, or (for `inactive_angle_is_zero`) that this fork's fixed
  behavior is the *stricter* of the two options — not a safety regression.
- `personality`/`lkaDisabled` (CarState): source-fork-only schema fields
  absent from this fork's `car.capnp`. Dropped the assignments.
- `chery/values.py`'s `CarControllerParams` didn't dispatch to
  `MpcLkaCarControllerParams` for BYD's torque-controlled MPC_LKA platform;
  `test_lateral_limits.py` needs it to. Added `__new__` dispatch.
- `dbc_dict()` / `CUSTOM_CAR_PARTS`: small source-fork-only helpers both
  BYD and Chery depend on, added to `opendbc/car/__init__.py` /
  `docs_definitions.py`.
- `opendbc/car/torque_data/override.toml` and `opendbc/car/tests/routes.py`
  needed BYD/Chery/MG entries (generic infra every registered car must have
  an entry in).

## Real safety bug found and fixed: `byd_zone_interp` missing upper clamp

`byd.h`'s speed-zone interpolation for the steering angle/rate backstop
extrapolated past the top breakpoint at high speed instead of clamping,
inverting the backstop and rejecting all valid commands above ~130 km/h.
Found via `test_angle_violation`'s speed=50 case, traced empirically with
`libsafety_py`. The same bug, independently, was present in **three
separate places** and fixed in all three:

1. This fork's `opendbc/safety/modes/byd.h`.
2. `dev/EDP10`'s own `opendbc/safety/modes/byd.h`.
3. `~/panda/TC275_BrownPanda`'s deployed firmware, `Safety_ZoneInterp()`
   (`DBC/safety.c`) — the actual gateway hardware, a completely separate
   codebase that happened to share the same interpolation bug pattern.

The generic `interpolate()`/`safety_interpolate()` helpers (not the
BYD-specific zone function) were verified to **not** have this bug —
correctly clamped at both ends — in both `dev/EDP10` and this fork.

## Chery speed-dependent max-angle backstop (`CHERY_ZONE_ANGLE_DEG`)

The source (kommuai) only tapers steering *rate* with speed, not max angle,
unlike BYD's zone table. Added a from-scratch max-angle backstop derived
from real per-model physics: `max_angle = accel * wheelbase * steerRatio /
v^2` (ISO ~1.3g lateral-accel margin, matching BYD's own zone-table
precedent), using Omoda 5's wheelbase (2.63m, smallest of the Chery
platforms here) for a conservative bound shared across all of them,
steerRatio=16.0. Result: 3-point table `{0., 16., 36.} → {120., 120.,
23.7}` (deg). Applied identically to this fork and `dev/EDP10`, each
verified independently via `libsafety_py`.

## Known gaps and deliberate non-ports — do not assume complete

- **`CHERY_TIGGO_8_PRO` has no real CAN capture.** The original port
  reused `CHERY_JAECOO_J7_PHEV`'s capture byte-for-byte, which fails
  `test_can_fingerprint.py`'s round-trip assertion for both platforms (two
  byte-identical fingerprint entries can't both resolve correctly), and is
  wrong anyway — confirmed via web search these are genuinely distinct
  vehicles (different size/seating), not a rebadge. Removed from the
  CAN-fingerprint dict entirely; forced/manual selection only until a real
  capture exists (matching `MG_ZS`'s existing pattern). Do not "fix" this
  by fabricating distinguishing signal data.
- **`opendbc/car/radar_interface.py` (generic) / `u_radar.dbc` — not
  ported, deliberately.** `dev/EDP10` has a from-scratch, hand-tuned
  ~175-message universal aftermarket-radar-retrofit rewrite of these, used
  generically across Toyota/GM/Chrysler/Rivian/Ford/Hyundai/Honda (not
  Chinese-EV-brand-specific — nothing to do with BYD/Chery/MG). Two
  independent reasons it wasn't ported: (1) it carries dragonpilot's
  non-commercial `LICENSE.md` restriction as a file-level header, confirmed
  by direct inspection; (2) even setting licensing aside, it's unrelated to
  this port's actual scope (Chinese EV brand support) and would be scope
  creep onto brands whose radar this fork's own comma.ai-derived,
  presumably-proper native support already covers. Not to be confused with
  `docs/BROWNPANDA_RADAR.md`'s Tesla-protocol radar adapter, which is
  unrelated exo-electronics hardware, already merged, and untouched by this
  decision.
- **Toyota ALKA / `zss.py` (alternate steering-angle-source path) — not
  ported, deliberately.** `dev/EDP10` has this live; dragonpilot's current
  upstream HEAD keeps the code present but commented out (deliberately
  disabled, reason not confirmed). This fork has zero trace of it, and
  neither `dev/EOP10` nor `dev/NGP10` reference it anywhere. Reintroducing
  a second steering-angle-input path is safety-relevant complexity with no
  current consumer and no confirmed reason dragonpilot itself disabled it
  — skipped, same reasoning as the radar item above.
- **Toyota `lock_ctrl`/`long_filter` param-bit numbering — not applicable.**
  `dev/EDP10` and dragonpilot's current HEAD use opposite bit assignments
  for this; this fork has no `LOCK_CTRL` concept in `toyota.h`/`values.py`
  at all (confirmed via grep), so there's no mismatch to reconcile here.
- **No BrownPanda/TC275 firmware wiring for BYD/Chery/MG.** `byd.h`/
  `chery.h`/`mg.h` exist as C safety-mode source here, but
  `~/panda/TC275_BrownPanda` (the actual gateway firmware) vendors its own
  separate copy of the safety framework and wasn't touched for these
  brands (only the pre-existing `byd_zone_interp` bug fix landed there —
  see above). Nothing here makes BYD/Chery/MG drivable on real hardware.
- **No generic `car_helpers.get_car()` dispatch wired into `dev/EOP10` at
  all.** EOP10 currently has a Tesla-only custom path
  (`system/socketd/vehicle/tesla/`), not the standard fingerprint-and-dispatch
  flow this port (like every other brand) relies on.

## What this is not

Not a bump of the `dev/EOP10` submodule pin, not a merge to this fork's
`master`, not a submodule conversion for `dev/EDP10`. All three are
separate, deliberate follow-ups. In particular, the `dev/EDP10` submodule
conversion is **blocked**, not just "not yet done" — see the compatibility
contract section above and `docs/upstream-audit/DELTA_AUDIT.md` in the
main `openpilot` repo for the concrete list of what EDP10-side code needs
to change first (its `latcontrol_torque.py`'s calling convention, every
brand's `_get_params` arg count, and every `ret.deprecated.X` access
pattern EDP10's own `car.capnp` doesn't have).

## Validation

Full `opendbc/` test suite via this fork's actual CI runner
(`unittest-parallel -j4`, per `lefthook.yml`/`test.sh` — not `pytest`,
which silently resolves to a mismatched system Python install for parts of
this suite and should not be trusted for a full-suite verdict here): 4158
tests, 0 failures, 725 skipped. `ruff check .` and `cpplint` (on the
touched C files) clean. GitHub Actions "PR review" workflow green on the
branch as of the last push.
