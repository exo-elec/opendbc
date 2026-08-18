# BYD Atto 3 / GWM Haval H6 — community port crosscheck

**Date:** 2026-08-17. **Scope:** cross-check the claim that `opendbc-data` and
`openpilot` community forks "successfully ported" BYD Atto 3 and GWM Haval H6,
against this fork's own BYD Atto 3 work (`opendbc/car/byd/`,
[`BYD_Atto3`](https://github.com/EXO-ELEC/BYD_Atto3),
[`TC275_BrownPanda`](https://github.com/EXO-ELEC/TC275_BrownPanda)). Companion
to [`BYD_CHERY_JAECOO_PORT.md`](BYD_CHERY_JAECOO_PORT.md), which covers where
this fork's `car/byd/` code itself came from.

## Update (same day): per-signal "best proven" grading added

The per-message decision tables this doc points to —
[`BYD_Atto3/DOC/community_port_comparison.md`](https://github.com/EXO-ELEC/BYD_Atto3/blob/com/BYD_ATTO3/DOC/community_port_comparison.md)
and
[`TC275_BrownPanda/docs/community_port_status.md`](https://github.com/EXO-ELEC/TC275_BrownPanda/blob/com/BYD_ATTO3/docs/community_port_status.md) —
now grade each qzwf-overlapping signal by strength of real-vehicle evidence
(a dated, quantified on-car finding outranks an untested claim; two
independent lineages agreeing outranks either alone). Headline result:
**`0x242` bit 37 (`DRIVE_STATE` brake bit)** is an unresolved conflict — this
fork and the `byd-atto3-openpilot-port` reference both treat it as
authoritative `brakePressed`, but qzwf/opendbc measured it dead (stuck at 0)
across 3.6 h of driving on their car and uses `0x342`'s pedal signal
instead. Neither side is provably wrong about their own vehicle — most
likely a market/firmware variant difference — but it's unreconciled and
safety-relevant (feeds disengage/override logic), so treat it as needing a
fresh local capture, not as settled. See those two docs for the full
per-signal table, including the strongest confirmed item (`0x1FC`
`MAIN_TORQUE` = EPS output, not driver input — independently reached by two
unrelated lineages).

## Update 2: GWM Haval H6 finding corrected — a real port exists, unmerged

The original pass below ("no full car port anywhere") only checked default
branches and named forks; it missed GitHub's PR history. A full,
CI-passing, comma-maintainer-reviewed GWM Haval H6 car interface has
existed as an open, unmerged PR (`commaai/opendbc#3263`) since 2026-03-29,
carried by a single lineage (same two authors) going back to mid-2024. Its
longitudinal control is explicitly gated behind a debug-only build flag —
comma's own maintainer says they haven't done final validation yet. Full
writeup replaces the old "GWM Haval H6" section below. This also resolves
where the `opendbc-data` Haval H6 tuning report came from: its fingerprint
(`075b133b6181e058`) matches the exact dongle the PR author used for their
test routes — the "report" is this same unmerged PR's own validation data,
not evidence of an independent, comma-supported port.

## TL;DR

- The premise is half right, in different ways for each car. `opendbc-data`
  makes **no BYD Atto 3 claim at all** (zero BYD files); its one Haval H6
  artifact is real on-car validation data, but it belongs to the same
  open/unmerged PR below, not to a shipped, comma-supported port.
- There is no single "BYD Atto 3 port." At least **four independent
  lineages** exist. **Correction (2026-08-17, caught while researching
  BYD Dolphin — see below):** this doc originally said the four split
  cleanly into two torque-control and two angle-control lineages. That was
  wrong. All four actually converge on **angle control** for Atto 3
  specifically — this fork's/kommuai's `car/byd/` does carry a
  torque-controlled `mpc_lka` code path, but `BYD_ATTO3` doesn't dispatch
  to it; only a different model in the same multi-platform port
  (`BYD_SONG_PLUS_DMI_21`) does. The real discriminator between the four
  lineages is hardware topology (this fork's TC275 bridge MCU vs. everyone
  else's direct-panda), not control mode. See the corrected table below.
- GWM Haval H6 has exactly **one** lineage (not four, unlike BYD), and it's
  further along than first thought: a complete, CI-green `opendbc/car/gwm/`
  interface sits in open PR `commaai/opendbc#3263`, reviewed by a comma
  maintainer, longitudinal control explicitly withheld pending comma's own
  validation. Not merged, not "successfully ported" in the shipped sense —
  but not vaporware either.

## BYD Atto 3: four lineages, not one

| Lineage | Repo | Control mode | Safety architecture | `0x1E2` / steering frame | Structure |
| --- | --- | --- | --- | --- | --- |
| **This fork** | `EXO-ELEC/opendbc` (`opendbc/car/byd/`), branch `port/upstream-bump-byd-chery-jaecoo` | **Angle** (`BYD_ATTO3` dispatches to `cam_lka`, `apply_std_steer_angle_limits`) | `panda/opendbc/safety/modes/byd.h`, zone-based angle/rate backstop table, **plus** a second, hardware-level backstop: `TC275_BrownPanda` (Infineon TC275 gateway MCU sitting between vehicle and comma device, own `Safety_ZoneInterp` 8-point speed grid) | `cam_lka`'s `STEER_ANGLE`-style command frame | `interface.py`/`fingerprints.py`/`values.py` + `cam_lka/` (used by `BYD_ATTO3`), `mpc_lka/` (torque-controlled, but only `BYD_SONG_PLUS_DMI_21` — a different model bundled in the same multi-platform port — dispatches to it) subpackages |
| **kommuai/opendbc** (`~/pilot/opendbc`, origin) | Same repo family, `master` | **Angle** (same `cam_lka` dispatch for `BYD_ATTO3`) | Same `byd.h` family (older cereal/`car.capnp` API — pre `structs` migration) | Same layout — `bydcan.py` is **byte-identical** to this fork's | Same `cam_lka`/`mpc_lka` split, same dispatch |
| **shemps/byd-atto3-openpilot-port** (`EXO-ELEC/byd-atto3-openpilot-port`, vendored copy under `port/opendbc/`) | Independent RE, own repo | **Angle** (absolute `STEER_ANGLE` position-servo target) | Own `safety_byd.h` variant, own accept test | `STEERING_MODULE_ADAS` (0x1E2) with `E2E_ALIVE_1/2`, different bit layout than this fork's `cam_lka` frame despite both being angle-controlled | Flat (`interface.py`/`carstate.py`/`carcontroller.py`, no `cam_lka`/`mpc_lka` split); adds `veoneer_tracks.py` (radar), references upstream [commaai/opendbc PR #3337](https://github.com/commaai/opendbc/pull/3337) — open, unmerged as of 2026-07-30 |
| **qzwf/opendbc** (`byd-atto3-stable` branch, vendored into `qzwf/openpilot`) | Independent RE, own repo | **Angle** (`apply_std_steer_angle_limits`, EPS treated as position servo) | Stock upstream `opendbc`/panda safety model (no hardware bridge) | `STEERING_MODULE_ADAS` (0x1E2) — **292-line diff against shemps's version of the same frame**: independently reverse-engineered, not a fork of shemps's work | Flat, same file set as upstream `car/<brand>/` convention; active, still shipping fixes as of 2026-08-11 (last commit: "fix the walking 0x1E2 frame that killed the car's ADAS") |

Key discriminators, in order of how cleanly they separate the four:

1. **Control mode does not split the four — all four use angle control for
   Atto 3.** This fork's/kommuai's `car/byd/` also contains a
   torque-controlled `mpc_lka` code path, but `BYD_ATTO3` doesn't use it —
   only `BYD_SONG_PLUS_DMI_21` (a different model in the same
   multi-platform port) dispatches there. shemps and qzwf independently
   arrived at the same angle-control approach this fork/kommuai already
   used. (Previous version of this doc claimed a clean torque-vs-angle
   split; that was an error, caught 2026-08-17.)
2. **Hardware topology** is this fork's real outlier, and the axis that
   actually separates the four: `TC275_BrownPanda`
   inserts a physical Infineon TC275 gateway MCU between the vehicle and the
   comma device (a Tesla-Model-3-party-protocol bridge with its own
   zone-interpolated backstop LUT). None of the other three lineages use a
   bridge MCU — they run the stock panda safety model directly against the
   car's own buses.
3. **Provenance**: this fork's `car/byd/` was explicitly ported from
   kommuai/opendbc (commit `2236aeb5`, documented in
   `BYD_CHERY_JAECOO_PORT.md`) and adapted for API drift (cereal→`structs`
   migration, etc.) — `bydcan.py` is still byte-identical between the two.
   shemps and qzwf are not related to that lineage or to each other; their
   `0x1E2` (`STEERING_MODULE_ADAS`) encodings differ from each other by 292
   lines despite both being angle-control.
4. Neither shemps's nor qzwf's line is upstream-merged. shemps's own
   upstream submission, `commaai/opendbc#3337` ("BYD: Atto 3 / Initial
   Platform Support"), has been open since 2026-04-19 and was last updated
   2026-07-30 — still unmerged.

None of this makes one lineage "correct" and the others "wrong" — they are
different, incompatible engineering approaches (bridge-MCU vs. direct-panda,
independently reverse-engineered bit layouts even where the control mode
agrees) validated against different vehicles/model years. Treat cross-lineage
signal names as reference material only, per the
existing per-message tables in
[`BYD_Atto3/DOC/community_port_comparison.md`](https://github.com/EXO-ELEC/BYD_Atto3/blob/com/BYD_ATTO3/DOC/community_port_comparison.md)
and
[`TC275_BrownPanda/docs/community_port_status.md`](https://github.com/EXO-ELEC/TC275_BrownPanda/blob/com/BYD_ATTO3/docs/community_port_status.md) —
do not merge wire-format assumptions across lineages without a local capture.

## GWM Haval H6 — correction: a full port exists, open upstream, not yet merged

**This section previously said "no full port found anywhere" — that was
wrong.** The initial pass only checked `master` branches and a handful of
named forks. A GitHub PR search on `commaai/opendbc` and `commaai/openpilot`
turns up a two-year, single-lineage community effort that has produced a
complete, CI-passing, actively-reviewed car interface — it just hasn't
landed on `master` yet, which is why it didn't show up in a branch/dir scan.

### The lineage — one continuous effort, not competing forks

Unlike BYD Atto 3 (four independent lineages), GWM Haval H6 has **one**
lineage, carried by two community members (`AlexandreSato`, `celobusana`),
iterating in public since mid-2024:

| PR | Repo | Date | State | What it did |
| --- | --- | --- | --- | --- |
| [openpilot#32880](https://github.com/commaai/openpilot/pull/32880) | openpilot | 2024-06-30 | closed, unmerged | First car-interface attempt, `AlexandreSato` (pre-dates opendbc/openpilot code split) |
| [openpilot#32877](https://github.com/commaai/openpilot/pull/32877) | openpilot | 2024-06-30 | closed, unmerged | Companion CRC fix, `celobusana` |
| [opendbc#1038](https://github.com/commaai/opendbc/pull/1038) | opendbc | 2024-05-04 | **merged** | `gwm_haval_h6_phev_2024.dbc` — DBC only. This is the file this doc previously found and stopped at. |
| [opendbc#1086](https://github.com/commaai/opendbc/pull/1086) | opendbc | 2024-08-18 | closed, unmerged | "Brand Port" attempt, `AlexandreSato` |
| [opendbc#3093](https://github.com/commaai/opendbc/pull/3093) | opendbc | 2026-01-30 | **merged** | Small DBC fix (`STEERING_DIRECTION` bit) |
| [opendbc#3117](https://github.com/commaai/opendbc/pull/3117) | opendbc | 2026-02-08 | closed, unmerged | Fuller car-interface attempt (19 files, +1171) |
| [opendbc#3521](https://github.com/commaai/opendbc/pull/3521) | opendbc | 2026-07-05 | **merged** | Typo/label fixes to the DBC |
| **[opendbc#3263](https://github.com/commaai/opendbc/pull/3263)** | opendbc | 2026-03-29 | **open**, last updated 2026-07-03 | **"GWM Haval H6: Initial Platform Support" — the current, live attempt** |

### What #3263 actually contains

20 files, +1019/−43: a full `opendbc/car/gwm/` interface
(`carstate.py`, `carcontroller.py`, `gwmcan.py`, `fingerprints.py`,
`values.py`, `interface.py`), `opendbc/safety/modes/gwm.h` with real
RX/TX hooks, `opendbc/safety/tests/test_gwm.py`, and a renamed/expanded DBC
(`opendbc/dbc/generator/gwm/gwm_haval_h6_mk3.dbc`, +91/−40 over the merged
baseline). This is comparable in scope to this fork's own `car/byd/` port.

Concretely, from the code:

- **Steering: torque control** (`apply_meas_steer_torque_limits`,
  `STEER_MAX = 253`) — same family as this fork's/kommuai's BYD lineage, not
  qzwf's/shemps's angle control.
- **Vehicle specs declared:** mass 2040 kg, wheelbase 2.738 m, steer ratio
  17.416, model years "Haval H6 2019-26".
- **Safety limits (`gwm.h`):** torque max 253, rate up/down 4/6,
  max torque error 80, max realtime delta 100; longitudinal max gas 4577,
  min gas −10, max brake 107 (units as coded, not independently verified
  here).
- **Longitudinal control is explicitly gated behind `ALLOW_DEBUG`** — not a
  documentation caveat, a build flag. comma's own reviewer
  (`adeebshihadeh`, a maintainer) commented directly on this: *"this should
  go under `ALLOW_DEBUG`. once we're able to do final validation ourselves,
  we'll move it out."* I.e. even comma has not yet signed off on the
  longitudinal safety limits as ready for general use.
- **Fingerprint coverage is thin by the maintainer's own question**
  ("do we get any other firmware?") — the author's answer: only one
  firmware responds, and only over OBD, not on the main bus. A second
  variant, the "Haval Jolion" (per a Discord contributor in the PR body),
  is flagged as needing a **separate port** because its EPS-to-camera
  feedback works differently — i.e. "Haval H6" is not necessarily one
  wire-compatible vehicle family.
- **CI passes** on the current head (`test models`, `car diff`, full test
  suite all green) and a comma maintainer has left substantive review
  comments (code organization concern about EPS-fault-detection logic
  living in `interface.py` instead of `carstate.py`/`carcontroller.py` —
  acknowledged by the author but not obviously resolved). `mergeable_state`
  is currently `dirty` (needs a rebase onto `master`), not blocked on
  unresolved review threads.

### Net assessment

"Successfully ported" overstates it and "no port exists" (this doc's
original claim) understates it. The accurate statement: **a complete,
CI-green, maintainer-reviewed GWM Haval H6 port exists, publicly, as an
open PR — but it is not merged, its longitudinal control is explicitly
withheld pending comma's own validation, and it's unclear the DBC/safety
model generalizes past the one tested firmware/trim.** None of the
*opendbc-level* forks checked (this fork's `opendbc`, `kommuai/opendbc`,
`qzwf/opendbc`, `shemps/byd-atto3-openpilot-port`) touch GWM/Haval — but
see the correction directly below: `TC275_BrownPanda`, a *different* repo
in this same project, does have its own independent GWM Haval H6 firmware
work, missed in the original pass.

### Correction (2026-08-17, found while researching BYD Dolphin): TC275_BrownPanda has its own dormant per-vehicle branches

The checks above only looked at `TC275_BrownPanda`'s checked-out branch
(`com/BYD_ATTO3`). `git branch -a` on that repo turns up remote branches
never examined in this crosscheck:

| Branch | Last commit | Status |
| --- | --- | --- |
| `dev/BYD_DOLPHIN` | 2025-12-14 | Real work: `DBC/byd_dolphin.c/h`, `DBC/BYD_DOLPHIN.dbc` (76 messages), full safety API (fingerprint, TxHook, `TorqueSteeringLimits`) — **torque-controlled**, CAN-FD 2 Mbps. See the BYD Dolphin section below. |
| `dev/HAVAL_H6` | — | Real work: `DBC/HAVAL_H6.dbc`, CAN-FD 2M with BRS, message renaming passes (`A_0x12B_MPC_Lateral_Cmd`, DEEPAL-style descriptive names). **This directly contradicts the "nothing in this fork touches GWM/Haval" line above** — that line is only true of the `opendbc` repo; `TC275_BrownPanda` has independent, unrelated Haval H6 firmware work that was missed because it sits on an unchecked branch, not because it doesn't exist. |
| `dev/DEEPAL_S05` | — | Real work: same firmware pattern as Dolphin/Haval (`DEEPAL_S05.dbc`, CANdb++-compatible naming pass). Changan Deepal S05, not a BYD model. |
| `dev/MG_5EV` | — | Real work: DBC structure/attribute fixes, CM_-before-Attributes ordering pass. MG 5 EV, not a BYD model. |
| `dev/CHERY_OMODA5`, `dev/CHERY_TIGGO7`, `dev/JAECOO_J5`, `dev/JAECOO_J7` | — | Explicitly self-documented as **scaffolds** ("Mark as scaffold awaiting protocol implementation") — not real ports, unlike the four above. |

None of these branches are merged into `com/BYD_ATTO3` (the current
single-vehicle firmware) or referenced from any doc this crosscheck
touched before now. Whether they're worth reviving is a separate decision
from this crosscheck — flagging their existence and rough maturity here so
they don't stay invisible to future doc passes the way they were to this
one.

### DBC files, checked and imported

Signal-level DBC comparison across the BYD Atto 3 lineages, and a GWM Haval
H6 DBC import into this fork, both done 2026-08-17:

- **BYD Atto 3:** `BYD_Atto3/DBC/byd_atto3.dbc` (local CANape source) and
  `TC275_BrownPanda/DBC/BYD_ATTO3.dbc` (firmware copy) are byte-identical
  for every message checked against the community findings above
  (`0x11F`, `0x1E2`, `0x1FC`, `0x242`, `0x32D`). Both now carry `CM_ SG_`
  comment annotations on the specific signals this crosscheck flagged
  (`VCU_BrakePressed`, `EPS_MainTorque`, `EPS_DriverTorque`, the `0x1E2`
  `CONST_0x*` fields, `VCU_ACCState`) — informational only, no signal
  definitions, bit positions, or scaling changed. Both files still parse
  cleanly (`cantools.database.load_file`) after the edit.
- **GWM Haval H6:** imported `opendbc/dbc/generator/gwm/gwm_haval_h6_mk3.dbc`
  into this fork, byte-identical to
  [PR #3263's copy](https://github.com/AlexandreSato/opendbc/blob/dde1d1eeb2329ae60bb181797242046ad8984f8c/opendbc/dbc/generator/gwm/gwm_haval_h6_mk3.dbc)
  (commit `dde1d1e`), at the same path the PR uses, for reference only —
  **not** referenced from `opendbc/car/values.py`, `docs/CARS.md`, or any
  `car_helpers.py` dispatch, so it changes no runtime behavior. This is the
  newer, semantically-named DBC the PR's actual `carstate.py`/
  `carcontroller.py` code uses (`RX_STEER_RELATED`, `ACC_CMD`,
  `STEER_CMD`, ...) — meaningfully different from (and supersedes, for
  reference purposes) the already-present `opendbc/dbc/gwm_haval_h6_phev_2024.dbc`
  baseline, whose message names are still placeholder/generic
  (`NEW_MSG_147`, `AUTOPILOT`, `SPEED2`, ...) from the earlier, DBC-only
  merge (PR #1038). **Known issue, carried over as-is from the PR (not
  something introduced by importing it):** `cantools` fails to parse this
  file — `Error: The signals DRIVE_MODE_SIGNAL3 and DRIVE_MODE are
  overlapping in message CAR_OVERALL_SIGNALS`. Consistent with the PR's
  `mergeable_state: dirty` — this is upstream's own in-progress state, left
  untouched here rather than "fixed" speculatively on someone else's open
  PR.

## `opendbc-data`: the "port claim" doesn't exist

`commaai/opendbc-data` (`~/panda/opendbc-data`, upstream remote, read-only —
**not** an EXO-ELEC fork) is comma's community longitudinal-tuning-report
archive, not a car-port repo. Its BYD/GWM footprint:

- BYD: **zero files** — no Atto 3 report, no BYD content of any kind.
- GWM Haval H6: **one file**,
  `longitudinal_reports/GWM_HAVAL_H6_075b133b6181e058_00000162--df9a818bf7.html`,
  added by commit `542af76` ("New report after tune from Gwm haval h6 (#20)"),
  listed in the repo's README as a `master`-branch longitudinal maneuver
  report. **The fingerprint hash in the filename, `075b133b6181e058`,
  matches the exact dongle used for the test routes listed in
  `commaai/opendbc#3263`'s own PR description** (see the GWM Haval H6
  section above) — almost certainly the same tester/vehicle, not an
  independent report. So this file is not third-party proof of a shipped
  port; it's the still-open PR's own on-car validation data, submitted to
  the shared report archive. The README's "master" label for this report
  most likely refers to the longitudinal-tuning report *format/tooling*
  (which is generated the same way regardless of which branch a car's
  interface lives on), not a claim that GWM Haval H6 support is on
  `commaai/opendbc`'s `master` — it demonstrably is not (see above). Either
  way, this is not a "BYD Atto 3 and GWM Haval H6 ported successfully"
  claim, and the repo makes no such claim in its own README.

## How `qzwf/openpilot.git` clone failures were resolved

Direct `git clone https://github.com/qzwf/openpilot.git` times out because
the repo is a full openpilot fork (~1.4 GB, plus Git-LFS camera/model
blobs served from a separate GitLab LFS store) — the LFS smudge filter alone
pulls tens of megabytes of unrelated binary model files before the clone
even finishes. Two fixes made this tractable:

1. Clone the **submodule repo directly** instead of the umbrella repo — the
   actual car port lives in `qzwf/opendbc` (referenced via `.gitmodules` as
   `opendbc_repo`), which has no LFS content and is far smaller.
2. `GIT_LFS_SKIP_SMUDGE=1 git clone --filter=blob:none --single-branch
   --branch <branch> --depth 1 <url>` — single-branch shallow clone with LFS
   smudging disabled. Branch used: `byd-atto3-stable`.

```bash
GIT_LFS_SKIP_SMUDGE=1 git clone --single-branch --branch byd-atto3-stable \
  --depth 1 https://github.com/qzwf/opendbc.git /tmp/qzwf-opendbc
```

This completed in seconds and gave direct access to
`opendbc/car/byd/{interface,carstate,carcontroller,bydcan,values,fingerprints}.py`.

## BYD Dolphin — checked 2026-08-17

Scope: does this project's BYD Dolphin work (`~/panda/BYD_Dolphin`,
`TC275_BrownPanda`'s `dev/BYD_DOLPHIN` branch) benefit from porting
anything, and is there relevant "online" (upstream/community) data?

### What exists on our side

- **`~/panda/BYD_Dolphin`** (CANape workspace, `main` branch): local capture
  work, dated 2025-10-14 (`DOC/dbc_completion_summary.md`). Two DBCs
  independently validated with `cantools`: `byd_dolphin.dbc` (84 messages;
  steering command `A_0x1BA_MPC_LateralCommand`) and `deepal_s05.dbc` (84
  messages, Changan Deepal S05, a different manufacturer sharing enough of
  the same body/ADAS platform to be captured in the same project). Also
  holds `DBC/reference/0ADASACAN_C857_V1.4_20241105_REEV&EV_BDC(LAS).dbc`
  — an OEM reference DBC for this shared platform (node names `ACC`/`GW`/
  `LAS`/`BDC` match `deepal_s05.dbc`'s own node list), renamed 83/88
  messages into this project's `{A|B|C|U}_0xNNN_Function` convention. (This
  is the same file this doc initially mis-flagged as "unclear provenance,
  possibly a GWM false positive" while researching Haval H6 — it's
  genuinely this platform's own reference file, unrelated to Great Wall
  Motors; that was a wrong tangent, corrected here.)
- **`TC275_BrownPanda`, `dev/BYD_DOLPHIN` branch** (dormant since
  2025-12-14 — see the branch table above): a complete firmware module,
  `DBC/byd_dolphin.c/h`, mirroring `byd_atto3.c`'s API surface exactly
  (message check/decode, fingerprinting, TX hook, safety limits). **Key
  fact: it's torque-controlled** (`BYD_DOLPHIN_STEERING_LIMITS` is a
  `TorqueSteeringLimits` struct, `TorqueMotorLimited` type, `max_steer:
  300`), and the bus is **CAN-FD at 2 Mbps data phase** — both genuinely
  different from Atto 3's classic-CAN, angle-controlled setup. `DBC/
  BYD_DOLPHIN.dbc` has 76 messages.

### What the community/"online" side claims

- **This fork's `opendbc/car/byd/values.py`** (ported from kommuai, same as
  the rest of the BYD work in this doc) lists **"BYD Dolphin 2023-26"** as
  a supported car doc entry — but bundled *inside* the `BYD_SEAL`
  `CamLkaPlatformConfig` (angle control, `byd_general_pt.dbc`), sharing
  Seal's fingerprint entry outright. There is no Dolphin-specific
  fingerprint, DBC, or validation anywhere in this chain.
- Traced the claim back further: `commaai/opendbc` closed PR
  [#2429](https://github.com/commaai/opendbc/pull/2429) ("Non-Chinese BYDs
  Brand port," `dotslashofficial`, 2025-07-02, closed unmerged
  2025-12-24) is explicitly scoped to **export markets — Thailand,
  Singapore, Malaysia, Australia** — and its description says, verbatim,
  "Primarily only for Atto 3 only. Will eventually be expandable to Seal,
  Sealion, Dolphin etc." That PR's file list (flat `opendbc/car/byd/*.py`,
  `byd_general_pt.dbc`, `safety/modes/byd.h`, no `cam_lka`/`mpc_lka` split)
  matches the DBC filename and general shape of what later became
  kommuai's/this fork's BYD support — i.e. the "Dolphin" entry this fork
  carries is very likely inherited from a **stated future intent** in an
  unmerged, export-market-focused PR, not from anyone validating it
  against an actual Dolphin.

### The gap this surfaces

Two independent signals point the same direction: **the community
"Dolphin support" (angle control, via `BYD_SEAL`) and this project's own
captured Dolphin (torque control, CAN-FD, per `TC275_BrownPanda`'s
`dev/BYD_DOLPHIN` branch) may not be the same vehicle generation/market
variant.** This is the same class of risk as the Atto 3 `0x242` conflict
above (community claim vs. this project's own evidence disagreeing on a
control-relevant fact) — except here it's the control *mode* itself
(torque vs. angle), not one signal, and there's no third-party measurement
to arbitrate. **Do not assume `opendbc`'s `BYD_SEAL`/Dolphin car
definition applies to this project's captured Dolphin** without checking
whether the fingerprint/FW versions actually match — they very plausibly
don't (India/SE-Asia-market BYDs have shown signal-scale and even
control-architecture differences from other markets throughout this whole
crosscheck, e.g. the Atto 3 wheel-speed correction factor and the `0x242`
brake-bit split already documented above).

### Net assessment: is there a benefit to porting anything?

- **No ready-to-port code exists for a torque-controlled, CAN-FD BYD
  Dolphin anywhere checked** (commaai/opendbc, kommuai/opendbc,
  qzwf/opendbc, shemps's port, PR #2429) — all BYD Atto 3/Dolphin/Seal
  community work found in this whole crosscheck is angle-control,
  classic-CAN. If this project's own `dev/BYD_DOLPHIN` firmware and local
  DBC captures are accurate, there is currently **nothing external to
  port in** for Dolphin — this project's own dormant branch and CANape
  captures are the most complete Dolphin-specific work found, full stop.
- The actionable finding is the reverse of "port something in": **don't
  let `opendbc`'s `BYD_SEAL` car definition claim Dolphin support in this
  project's own eventual `car/byd/` sync** without gating it behind a
  fingerprint/FW check against this project's own captured Dolphin data —
  otherwise a real Dolphin could silently get matched to Seal's
  angle-control interface when this project's own evidence says it needs
  `dev/BYD_DOLPHIN`'s torque-control path instead.
- Same discovery applies to Haval H6 and Deepal S05: this project already
  has real, independent, unmerged-anywhere-upstream firmware work for
  both (see the branch table above) — the "online data" search for those
  two should start from reconciling this project's own dormant branches
  against the upstream PR history documented earlier in this file, not
  from assuming a green-field port is needed.

### Correction (same day): the local Dolphin capture is Atto 3's sibling — Atto 3 work transfers directly

The "net assessment" above was wrong to treat Dolphin as architecturally
disconnected from Atto 3. It relied on `TC275_BrownPanda`'s dormant
`dev/BYD_DOLPHIN` **firmware** branch (torque control, CAN-FD 2 Mbps)
without checking it against this project's own **captured** Dolphin DBC.
Doing that check reverses the conclusion:

`~/panda/BYD_Dolphin/DBC/byd_dolphin.dbc` (76 messages, real captured
data, `cantools`-validated) shares **55 of its 76 message IDs — 72% —
with `byd_atto3.dbc`, same numeric ID and same message name**, including
every message this whole crosscheck has been discussing:

| ID | Message | Note |
| --- | --- | --- |
| `0x1E2` | `A_0x1E2_MPC_Lateral_Cmd_L8_20ms` | Same ID, same name, same checksum/counter bit positions as Atto 3's steering command. Signal contents are partially `FAKE_`-prefixed (unconfirmed placeholders) — consistent with this file having been seeded from Atto 3's structure as a decode starting point, not independently invented. |
| `0x1FC` | `B_0x1FC_EPS_MotorState_L8_20ms` | Same ID/name; `EPS_SteeringAngle` is a real (non-`FAKE_`) decoded field here, `FAKE_EPS_MotorAssist`/`FAKE_EPS_DriverTorque`/etc. are still placeholders. |
| `0x242` | `B_0x242_VCU_DriveState_L8_20ms` | Same ID/name, and **`VCU_BrakePressed` at the same bit position (37)** as the signal at the center of the unresolved qzwf conflict above — this is now a *third* independent local capture (on a related but different vehicle) confirming a brake-pressed bit exists at this position by platform design, which raises confidence this project's own reliance on it is architecturally sound rather than a fluke. |
| `0x11F`, `0x1FC`, `0x316`, `0x32D`, `0x342`, `0x3B0`, `0x418` | (driver torque, EPS state, LKAS HUD, ACC HUD, pedals, buttons, BSD) | All present at identical IDs/names — the entire per-message decision table in `BYD_Atto3/DOC/community_port_comparison.md` is a direct starting hypothesis for Dolphin, not just generally "related" work. |

This means `TC275_BrownPanda`'s `dev/BYD_DOLPHIN` firmware branch's
"torque-controlled, CAN-FD" characterization is now the **outlier** that
needs explaining, not the captured DBC's near-Atto-3-identical structure
(predominantly classic 8-byte CAN, 72 of 76 messages — not CAN-FD). Given
that branch went through repeated "cross-contamination cleanup" commits
around the same time as the also-dormant `dev/DEEPAL_S05` and
`dev/HAVAL_H6` branches (both plausible CAN-FD/torque vehicles), the most
likely explanation is that the firmware branch's bus/control-mode
configuration leaked in from that parallel work rather than reflecting a
real Dolphin measurement — but this needs an actual reconciliation pass
against the captured `.dbc`, not another guess. **Revised net assessment:
this project's own Atto 3 decode work is the single most useful "port" for
completing Dolphin's remaining `FAKE_`/`UNKNOWN_` signals** — reconcile
`dev/BYD_DOLPHIN`'s firmware assumptions against `byd_dolphin.dbc`'s real
captured bytes before trusting either on control mode or bus type.

## Dormant TC275_BrownPanda branches — DBC files updated (2026-08-18)

Applied the same crosscheck annotations to each dormant vehicle branch's
own DBC/firmware files (not just `com/BYD_ATTO3`), so the findings are
visible wherever an engineer actually opens that branch, not only in this
doc:

- **`dev/BYD_DOLPHIN`**: `DBC/BYD_DOLPHIN.dbc` got the same Atto3-informed
  `CM_ BO_` hypothesis comments as `~/panda/BYD_Dolphin/DBC/byd_dolphin.dbc`
  (0x1E2/0x1FC). `DBC/byd_dolphin.h` got an inline flag on the CAN-FD/
  torque-control `#define`s noting they contradict the captured DBC and
  are suspected to be contamination from the parallel Deepal/Haval work
  (this branch's own `CLAUDE.md` already documents TX hook safety
  validation as 0% complete, so none of this is production-relevant yet
  regardless).
- **`dev/HAVAL_H6`**: cross-checked `DBC/HAVAL_H6.dbc` against
  `commaai/opendbc` open PR #3263 directly (not just the BYD lineage) and
  found real, concrete matches: `0x12B`'s `AP_STATE` bit (125) matches the
  PR's `steer_req` bit exactly; `0x12B`'s `AP_STEERING_UNDEFINED_SIGNAL1`
  (10-bit signed) lines up with the PR's `desired_torque` field; `0x13B`'s
  wheel-speed scale (`0.05924739`) is an **exact, independently-derived
  match** with the PR's constant — the strongest confirmation in this
  whole crosscheck. `0x147` is completely undecoded locally
  (`NEW_MSG_147`) where the PR has a real, reviewed torque-measurement
  formula — a concrete next capture target. Also flagged: `haval_h6.h`
  declares `FD_ENABLED=FALSE` while `HAVAL_H6.dbc`'s own `0x13B` message
  needs bytes past the classic-CAN 8-byte limit — an internal
  contradiction, unresolved. Also noted (not fixed, pre-existing, unrelated
  to these edits): `HAVAL_H6.dbc` already fails to parse with `cantools`
  due to an overlapping-signal bug in `B_0x12F_VCU_DriveMode` — confirmed
  by testing the pre-edit `HEAD`.
- **`dev/DEEPAL_S05`**: `DEEPAL_S05.dbc` parses cleanly and is genuinely
  CAN-FD — added a one-line cross-reference noting this branch is the
  likely real source of the CAN-FD config that leaked into
  `dev/BYD_DOLPHIN`.
- **`dev/CHERY_OMODA5`, `dev/CHERY_TIGGO7`, `dev/JAECOO_J5`, `dev/JAECOO_J7`**:
  left untouched — these are explicit scaffolds with no DBC content to
  crosscheck yet (see the branch table earlier in this doc).

All four branches' changes are informational comments/flags only — no
signal definitions, bit positions, scales, or C logic changed. Each was
committed and pushed on its own branch (not merged into `com/BYD_ATTO3`,
which remains the single-vehicle Atto 3 firmware).

## Branch inventory completed (2026-08-18)

Confirmed via `git ls-remote --heads origin` directly against
`TC275_BrownPanda` (bypasses any local cache) that the 12-branch list in
the table earlier in this doc is the **complete** set — there is no
fork/upstream remote with additional branches, and no hidden/deleted
branches surfaced. Completed the two checks left open from the previous
pass:

- **`dev/MG_5EV`**: checked its DBC content against `commaai/opendbc`'s
  `mg.dbc` (ported there from dragonpilot) directly, the same way
  `dev/HAVAL_H6` was checked against PR #3263. Result is the **reverse**
  of the Dolphin/Haval pattern — this local 74-message capture is *more*
  complete than opendbc's 20-message `mg.dbc`: all 20 of opendbc's
  messages are present here with matching transmitter nodes (19/20), and
  this file has functional names (`B_0x1EC_EPS_SteeringTorque_L8`,
  `A_0x1FD_LBSS_LKA_Steer_Cmd_L8`, ...) where opendbc only has generic
  `FrPxx` placeholders. Found one concrete bug, not a hypothesis: `0x1FD`'s
  `LKAReqToqHSC2` (steering torque request) uses scale `(0.01,-10.24)` →
  ±10.24 Nm here, consistent with the analogous signal in both files'
  `0x1EC`; opendbc's `mg.dbc` has the same bit position with scale
  `(1,-1024)` → ±1024 Nm, two orders of magnitude off and physically
  implausible for an EPS torque request. Flagged inline so it isn't
  propagated in if this project ever syncs against opendbc's `mg.dbc`.
- **`dev/openarm`, `dev/litekit`**: confirmed out of scope — neither has
  any `.dbc` file (checked via `git ls-tree`). `openarm` is a generic
  OS/CAN ring-buffer infrastructure port, `litekit` is bootloader/linker/
  Infineon-driver work. Not vehicle ports, nothing to crosscheck.
- **`dev/BYD_ATTO3`**: confirmed stale/superseded by `com/BYD_ATTO3` (57
  files differ, `com/` is strictly ahead) — not a separate vehicle, no
  additional crosscheck needed.

## Correction (2026-08-19): the Chery/JAECOO branches weren't scaffolds — they're an unvalidated Atto3-cloned template

The prior pass dismissed `dev/CHERY_OMODA5`, `dev/CHERY_TIGGO7`,
`dev/JAECOO_J5`, `dev/JAECOO_J7` as "scaffolds" based only on their commit
messages ("Mark as scaffold awaiting protocol implementation") — the same
mistake flagged earlier in this doc (verify actual content, not
descriptions). Checking the actual files:

- Each branch has a real, populated `.dbc` (14 messages, 63 signals) and a
  full `.c`/`.h` module with fingerprint/decode/TX-hook functions — not an
  empty stub.
- All four branches' `.dbc` files are **byte-for-byte identical** to each
  other.
- That shared file's 14 message IDs and DLCs are an **exact match to
  `byd_atto3.dbc`'s 14 core ADAS messages** (`0x1E2`, `0x316`, `0x32E`,
  `0x11F`, `0x121`, `0x133`, `0x1F0`, `0x1FC`, `0x242`, `0x294`, `0x32D`,
  `0x342`, `0x3B0`, `0x418`) — confirming this is a single template cloned
  from the Atto 3 message structure, stamped across four different
  vehicles, not real captured data from any of them.

Cross-checked against `commaai/opendbc`'s real, community-validated Chery/
JAECOO support (`opendbc/car/chery/`, `chery_general_pt.dbc`, which
directly covers `CHERY_OMODA_5` and `CHERY_JAECOO_J7_PHEV` — real
`CarSpecs`, real fingerprints). Its actual CAN IDs are **completely
different** from the template: the steering command lives at `0x345`
(`LANE_KEEP.STEER_CMD_ANGLE`, angle control, 0.1 deg/count — not this
template's `0x1E2`), steering feedback at `0x1D3`/`0xC4`
(`EPS`/`STEER_RELATED`, not `0x11F`). Where the template happens to share
a numeric ID with a real opendbc Chery message (`0x316`=790, vs opendbc's
unrelated `WHEELSPEED_1` at the same ID), it's a coincidental collision,
not a real match — worth being careful not to assume otherwise.

**Net assessment, same shape as the Dolphin/Haval findings above:** don't
build on this template as-is. `opendbc/dbc/chery_general_pt.dbc`'s real
message list is a far better starting point for an actual Chery/JAECOO
local capture than the current Atto3-derived placeholder. Flagged inline
on all four branches (`CM_ BO_` comment on the steering command message,
no signal changes) rather than silently replaced, since none of this is
backed by a local capture yet either way.
