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
  lineages** exist, split by control philosophy: two torque-control
  (this fork + its direct source), two angle-control (unrelated to each
  other and to this fork).
- GWM Haval H6 has exactly **one** lineage (not four, unlike BYD), and it's
  further along than first thought: a complete, CI-green `opendbc/car/gwm/`
  interface sits in open PR `commaai/opendbc#3263`, reviewed by a comma
  maintainer, longitudinal control explicitly withheld pending comma's own
  validation. Not merged, not "successfully ported" in the shipped sense —
  but not vaporware either.

## BYD Atto 3: four lineages, not one

| Lineage | Repo | Control mode | Safety architecture | `0x1E2` / steering frame | Structure |
| --- | --- | --- | --- | --- | --- |
| **This fork** | `EXO-ELEC/opendbc` (`opendbc/car/byd/`), branch `port/upstream-bump-byd-chery-jaecoo` | Torque (`MPC_LKA`) | `panda/opendbc/safety/modes/byd.h`, zone-based angle/rate backstop table, **plus** a second, hardware-level backstop: `TC275_BrownPanda` (Infineon TC275 gateway MCU sitting between vehicle and comma device, own `Safety_ZoneInterp` 8-point speed grid) | Local torque-controlled `MPC_LKA` layout, `cam_lka`/`mpc_lka` split | `interface.py`/`fingerprints.py`/`values.py` + `mpc_lka/`, `cam_lka/` subpackages |
| **kommuai/opendbc** (`~/pilot/opendbc`, origin) | Same repo family, `master` | Torque (`MPC_LKA`) | Same `byd.h` family (older cereal/`car.capnp` API — pre `structs` migration) | Same layout — `bydcan.py` is **byte-identical** to this fork's | Same `cam_lka`/`mpc_lka` split |
| **shemps/byd-atto3-openpilot-port** (`EXO-ELEC/byd-atto3-openpilot-port`, vendored copy under `port/opendbc/`) | Independent RE, own repo | **Angle** (absolute `STEER_ANGLE` position-servo target) | Own `safety_byd.h` variant, own accept test | `STEERING_MODULE_ADAS` (0x1E2) with `E2E_ALIVE_1/2`, different bit layout than the torque lineage | Flat (`interface.py`/`carstate.py`/`carcontroller.py`, no `cam_lka`/`mpc_lka` split); adds `veoneer_tracks.py` (radar), references upstream [commaai/opendbc PR #3337](https://github.com/commaai/opendbc/pull/3337) — open, unmerged as of 2026-07-30 |
| **qzwf/opendbc** (`byd-atto3-stable` branch, vendored into `qzwf/openpilot`) | Independent RE, own repo | **Angle** (`apply_std_steer_angle_limits`, EPS treated as position servo) | Stock upstream `opendbc`/panda safety model (no hardware bridge) | `STEERING_MODULE_ADAS` (0x1E2) — **292-line diff against shemps's version of the same frame**: independently reverse-engineered, not a fork of shemps's work | Flat, same file set as upstream `car/<brand>/` convention; active, still shipping fixes as of 2026-08-11 (last commit: "fix the walking 0x1E2 frame that killed the car's ADAS") |

Key discriminators, in order of how cleanly they separate the four:

1. **Torque vs. angle control** splits the four into two pairs cleanly: this
   fork + kommuai control via `MPC_LKA` torque commands; shemps + qzwf both
   moved to absolute-angle EPS control (treating the EPS as a position
   servo), independently of each other.
2. **Hardware topology** is this fork's real outlier: `TC275_BrownPanda`
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
different, incompatible engineering approaches (torque vs. angle command,
bridge-MCU vs. direct-panda) validated against different vehicles/model
years. Treat cross-lineage signal names as reference material only, per the
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
model generalizes past the one tested firmware/trim.** Nothing in this
fork, `kommuai/opendbc`, `qzwf/opendbc`, or `shemps/byd-atto3-openpilot-port`
touches GWM/Haval at all — this lineage is entirely separate from the BYD
Atto 3 work above, same author overlap or not.

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
