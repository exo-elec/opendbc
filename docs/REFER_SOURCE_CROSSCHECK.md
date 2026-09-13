# `~/panda/REFER` crosscheck — does this material beat our own `car/byd/`?

**Date:** 2026-09-13. **Scope:** `~/panda/REFER` (`byd2/`, `BYD_Files/`,
`byd源码/`, `吉利源码/` (Geely), `比亚迪3/`, `高阶Python源码_v2(1)/`, plus a
stray `byd大全.dbc`) is a batch of unattributed, Chinese-language source/DBC
dumps dropped next to this checkout, not a named fork or PR like the ones
`BYD_ATTO3_COMMUNITY_LINEAGE.md`/`BYD_CHERY_JAECOO_PORT.md` already track.
This doc applies the same standard those two set: every claim below comes
from a hash, a `cantools`/`grep` check, or a direct file read — not from a
filename, a comment, or a README's own description. Companion to those two
docs; cross-link both ways.

## TL;DR

- **No license file anywhere in `REFER`.** Nothing here is copied into this
  repo. Findings only — verdicts on whether each payload is our own
  lineage, behind us, or ahead of us.
- **Security-relevant finding, not a code-quality one:** one sub-bundle
  (`高阶Python源码_v2(1)`, duplicated twice) wires a `server_connect.py`
  into `carstate.py` that POSTs the device's serial number, hardware type,
  and exact software version/branch/commit to `http://39.108.82.40/api/check`
  — a bare IP, over plaintext HTTP, no cert pinning — and stores whatever
  `license_key` comes back into `Params`. This is a commercial license/DRM
  phone-home gate bolted onto what looks like a paid Chinese-market BYD
  tuning package, not community car-support code. **Do not adopt any file
  from this sub-bundle**, even the parts that look like plain car interface
  code, without stripping this first — flagging to the user directly as
  well as here.
- **Where we're already ahead:** `BYD_Files/opendbc/dbc/byd_han_dmev_2020.dbc`
  is a strict subset of the file we already ship (missing our
  `WHEEL_SPEED` and `METER_CLUSTER` messages entirely) — nothing to import.
- **Where REFER has something we don't:** `比亚迪3/唐DMP_based_on_han.dbc`
  (Tang DM-p, 88 messages, real semantic names) shares 17/18 of our Han
  DMEV message IDs verbatim and adds 71 more, with plausible real-vehicle
  signal decodes — the most complete Tang-family capture found here, and a
  genuine future-expansion candidate if this project ever wants Tang DM-p
  support. `byd2/tuning.py` carries real-reading EPS handsoff/accel-smoothing
  tuning knowledge for the Tang/D9 platform not present in our own tuning.
- **Not a real port, despite looking like one:** `吉利源码` (Geely) — no
  `.dbc` file anywhere in the directory, `FW_QUERY_CONFIG.requests=[]` is
  empty, and `DBC = CAR.create_dbc_map()` points at platform DBC filenames
  that don't exist in this dump. Comments claim "实车验证值" (real-vehicle
  verified) and cite a "Proton implementation" reference that doesn't exist
  in this fork or anywhere else checked. Same failure pattern this project
  already caught once, in `BYD_ATTO3_COMMUNITY_LINEAGE.md`'s Chery/JAECOO
  "scaffold" correction: plausible-looking code, zero real capture behind
  it. Do not use as a starting point for a Geely port.

## Inventory and verdicts

| Path | What it is | Checked against | Verdict |
| --- | --- | --- | --- |
| `byd2/byd_generic_pt.dbc` | 19-message DBC, every signal/message name randomized/obfuscated (`CID_MZNOSW`, ...) | Our `opendbc/dbc/byd_general_pt.dbc` (39 messages) | 17/19 IDs match ours exactly (`0x11f`, `0x1e2`, `0x1fc`, `0x242`, `0x316`, `0x32d/e/f`, `0x342`, `0x3b0`, `0x418`, etc.). **2 IDs we don't carry: `0x2b6`, `0x318`** — real gap, worth a capture, but the obfuscated names give no hint what they decode to. |
| `byd2/byd_tangdm_radar.dbc` | Radar DBC for BYD Tang DM | No Tang DM radar support exists in this fork | Reference-only; no current platform to attach it to. |
| `byd2/carcontroller.py`, `fingerprints.py`, `interface.py` | Flat BYD car module | See `byd源码/` below — same file shapes, not independently checked twice | Treated as the same lineage as `byd源码/`. |
| `byd2/tuning.py` | Standalone tuning-parameter module: lat non-linear steer table, EPS handsoff-angle/period workaround "某些D9或者唐车型" (some D9/Tang models), 4-tier (1-4 bar regen) accel/decel smoothing curves by distance-to-lead | No direct equivalent in our `opendbc/car/byd/` (this fork has no `tuning.py` at all — tuning lives in openpilot-side `latcontrol`/`longcontrol`, out of `opendbc`'s scope) | Real-reading, real-vehicle-flavored tuning knowledge, but it's openpilot-side and Tang/D9-specific — nothing to port into `opendbc` itself; noted for awareness only. |
| `BYD_Files/opendbc/dbc/byd_han_dmev_2020.dbc` | Han DMEV DBC | Byte-diffed against our shipped `opendbc/dbc/byd_han_dmev_2020.dbc` | **We're ahead.** Identical except our copy has two extra messages theirs lacks entirely: `BO_ 496 WHEEL_SPEED` (4-wheel speed + fault bits) and `BO_ 660 METER_CLUSTER` (doors/seatbelt). Nothing to import. |
| `BYD_Files/opendbc/car/byd/*`, `BYD_Files/opendbc/car/values.py`, `fingerprints.py` | Full car module tree, same shape as `dev/EDP10`'s BYD Atto 3 vendoring described in `BYD_CHERY_JAECOO_PORT.md` | Not independently re-hashed (time-boxed); shape matches the already-documented EDP10/kommuai lineage | Same lineage already covered by the existing docs; no new information. |
| `BYD_Files/carcontroller(2).py`, `long_mpc.py`, `params.toml` | openpilot-side control/config files, not `opendbc` car-interface code | N/A | Out of this repo's scope; not checked further. No phone-home code found in these three specifically (checked via `grep`). |
| `byd源码/` (`bydcan.py`, `carstate.py`, `carcontroller.py`, `interface.py`, `values.py`, `fingerprints.py`, `radar_interface.py`, `README.md`) | A full rewrite/"optimization" of the `cam_lka` BYD control stack — the README (`README.md`, read in full) states its own scope: refactor + comfort/longitudinal tuning, not a new vehicle | `md5sum` against every file we ship for BYD (`cam_lka/`, `mpc_lka/`, flat `byd/*.py`) | **No hash matches anything we ship** — this is a genuine, independent rewrite, not a copy of our code or of kommuai's. Contains real control-logic features absent from our `cam_lka/carcontroller.py` (confirmed via `grep`, zero hits on our side): TTC-based predictive braking (`_predictive_braking`, `LONG_TTC_HORIZON = 3.0`), steering anti-oscillation/"防画龙" correction scaled by speed, and an explicit "0-frame launch" (`0帧起步`) responsiveness tweak. Potentially useful *comfort-tuning ideas* for a future BYD controller revision — but it's a full rewrite of unknown vehicle/firmware validation, no license header, and structurally flat (no `cam_lka`/`mpc_lka` split), so it cannot be dropped in as-is. Treat as design reference only. **Checked against `~/pilot/openpilot` (2026-09-13) and rejected on both counts, not just deferred:** the TTC predictive-braking idea is redundant with `dev/EOP10`'s already-live Adaptive Gap block in `selfdrive/controls/lib/longitudinal_planner.py` (modulates `t_follow`/jerk off real radar `leadOne.dRel`/`vLead` via a proper MPC solver) — `cam_lka/carcontroller.py:222-225` only passes `actuators.accel` through to `create_accel_command` for `BYD_OP_LONG_PLATFORMS`, so accel shaping happens upstream of opendbc entirely; inserting a second heuristic at the CAN-packing layer would fight the planner, not improve it. The anti-oscillation idea isn't an addition to a blank slate either — `cam_lka/carcontroller.py` already runs a 2 Hz one-pole lowpass (`lowpass_1pole`) plus `apply_std_steer_angle_limits` and a ±10° offset clamp; stacking REFER's different mechanism (10-sample angle-history trend counter-correction) on top, with no on-car data from either side, is more likely to reintroduce the oscillation it's meant to fix than remove it. Also confirmed `~/pilot/openpilot`'s `claude/ngp10-opendbc-byd` branch pins `opendbc_repo` to this fork's `master` (commit `ac750092`, this doc's parent commit) — `cam_lka/carcontroller.py` is a live steering path for that integration effort, a second reason not to touch it on a hypothesis. Net: nothing in `byd源码/` cleared the bar; our design is the more mature one here too. |
| `吉利源码/` (Geely: `interface.py`, `geelycan.py`, `fingerprints.py`, `values.py`, `radar_interface.py`, `carstate.py`, `carcontroller.py`) | Would-be Geely (Binyue) car module | This fork has **zero** Geely code anywhere (`grep -ril geely opendbc/` — no hits) | **Not a real port.** `values.py`'s `FW_QUERY_CONFIG = FwQueryConfig(requests=[], ...)` — empty ECU query list. `DBC = CAR.create_dbc_map()` resolves to per-platform `.dbc` filenames that **do not exist anywhere in this directory or the rest of `REFER`** — the module is non-functional as shipped; `carstate.py`/`radar_interface.py` index `DBC[...]` against a mapping with no backing file. Comments assert "实车验证值" (real-vehicle-verified torque max) and cite tuning "参考Proton实现" (reference: Proton implementation) — no Proton brand exists anywhere in this fork's `opendbc/car/`. Same shape as the Chery/JAECOO "scaffold" trap this project already documented once: plausible structure, zero real capture underneath. **Do not use as a starting point for a Geely port** without an actual local CAN capture. |
| `比亚迪3/Bydsong(1).dbc`, `generated(1).dbc` | 89-message DBCs, generic `MSG_NNN` names, no semantic decode | `diff`/message-count only (time-boxed — no decoded signals to compare meaningfully) | Low value as-is: an un-decoded raw capture. Message-ID range is consistent with `BYD_SONG_PLUS_DMI_21`, a platform this fork already supports via `mpc_lka` — if this project ever needs a second opinion on Song Plus's bus layout, this is a candidate starting point for *decoding*, not a drop-in DBC. |
| `比亚迪3/唐DMP_based_on_han.dbc` | 88-message DBC for BYD Tang DM-p ("based on Han"), real semantic names (`EPS`, `CARSPEED`, `SteeringAngle` at 0.1°/count, `CarDisplaySpeed`) | Message-ID overlap against our `opendbc/dbc/byd_han_dmev_2020.dbc` (Python, exact-match on frame IDs) | **17 of our 18 Han DMEV message IDs appear here verbatim** (`0x11f`, `0x121`, `0x12d`, `0x133`, `0x1f0`, `0x242`, `0x294`, `0x2b6`, `0x316`, `0x318`, `0x32d/e/f`, `0x342`, `0x374`, `0x3b0`, `0x418` — only our `0x55` is missing here), plus **71 additional messages** we don't carry at all. Signal decodes read as genuine (plausible scale/offset/range, not placeholder). **This is the single most complete Tang-family capture found in this whole crosscheck** — a real candidate if Tang DM-p support is ever wanted, the same way `BYD_ATTO3_COMMUNITY_LINEAGE.md` flagged `~/panda/BYD_Dolphin`'s own capture as the best Dolphin starting point. No code exists anywhere in `REFER` or elsewhere checked to pair with this DBC (no Tang-specific `carstate.py`/`carcontroller.py`) — DBC only. |
| `比亚迪3/高阶Python源码_v2(1).zip`, `高阶Python源码_v2(1)/` (top-level, duplicate) | Two copies (one still zipped) of the same bundle: `opendbc/car/byd/{interface,carstate,carcontroller,bydcan,fingerprints,values,radar_interface,tuning,server_connect}.py`, plus `opendbc_repo/opendbc/dbc/{byd_generic_pt,byd_tangdm_radar}.dbc` (same two DBCs as `byd2/`) | `server_connect.py` read in full; import wired into `carstate.py` (`from opendbc.car.byd.server_connect import ServerConnect`, called at `carstate.py:85` as `ServerConnect.check_auth(serial)`) | **Security-relevant, see TL;DR.** This bundle is a commercial/DRM-gated BYD package — real vehicle CAN plumbing (same `byd_generic_pt.dbc`/`byd_tangdm_radar.dbc` as `byd2/`) shipped alongside a license-check call-home to a bare IP over plain HTTP, sending device serial + hardware model + exact software version/branch/commit and persisting an opaque `license_key` via `openpilot.common.params`. Confirmed present identically in **both** copies of this bundle (top-level and nested under `比亚迪3/`) and in the extracted `opendbc_repo/` copy too — three occurrences, one source. Not adopted; not to be adopted even piecemeal. |
| `byd大全.dbc` (top-level `REFER/`, "BYD comprehensive") | Single large DBC, not yet decode-compared | Not checked (time-boxed) | Unexamined — flagging its existence for a future pass rather than guessing at its content. |

## What this means for this fork's own `car/byd/`

- No code or DBC changes made to `opendbc/car/byd/` or the shipped BYD DBCs
  as a result of this crosscheck — nothing here cleared the bar (either
  it's a strict subset of what we ship, unverifiable/non-functional, or
  carries a DRM phone-home that disqualifies it outright).
- Two concrete, narrow follow-ups worth a real local capture rather than
  guesswork, in priority order:
  1. `byd_general_pt.dbc` message IDs `0x2b6` and `0x318` — present in
     `byd2/byd_generic_pt.dbc` under obfuscated names, absent from our DBC.
  2. Tang DM-p (`唐DMP_based_on_han.dbc`) as a from-scratch platform, if
     this project ever wants to support it — it would need real
     `carstate.py`/`carcontroller.py`/safety work from zero; only the DBC
     exists anywhere in what was checked.
- The `byd源码/` comfort-tuning ideas (TTC predictive braking, anti-wobble
  correction, 0-frame launch) are plausible *design* references for a
  future controller revision, but porting them means re-implementing the
  ideas against this fork's own `cam_lka` structure and safety envelope,
  not copying the files — they're a different lineage's flat lay-out and
  carry no license.

## What wasn't checked (time-boxed, flagging rather than guessing)

- `byd大全.dbc` content.
- Full byte-level signal comparison for `Bydsong(1).dbc`/`generated(1).dbc`
  (only message-ID-level, since neither has semantic names to compare).
- `BYD_Files/opendbc/car/byd/*` was not independently re-hashed file-by-file
  against this fork's tree (its shape matches the already-documented
  EDP10/kommuai lineage closely enough that a full hash sweep looked
  low-value against the time cost; flagging instead of asserting exact
  byte-equality).
