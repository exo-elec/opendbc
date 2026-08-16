# BYD Atto 3 / GWM Haval H6 — community port crosscheck

**Date:** 2026-08-17. **Scope:** cross-check the claim that `opendbc-data` and
`openpilot` community forks "successfully ported" BYD Atto 3 and GWM Haval H6,
against this fork's own BYD Atto 3 work (`opendbc/car/byd/`,
[`BYD_Atto3`](https://github.com/EXO-ELEC/BYD_Atto3),
[`TC275_BrownPanda`](https://github.com/EXO-ELEC/TC275_BrownPanda)). Companion
to [`BYD_CHERY_JAECOO_PORT.md`](BYD_CHERY_JAECOO_PORT.md), which covers where
this fork's `car/byd/` code itself came from.

## TL;DR

- The premise is half right. `commaai/opendbc-data` makes **no BYD Atto 3
  claim at all** (zero BYD files) and its one GWM Haval H6 artifact is a
  longitudinal-tuning report, not a port claim — see below.
- There is no single "BYD Atto 3 port." At least **four independent
  lineages** exist, split by control philosophy: two torque-control
  (this fork + its direct source), two angle-control (unrelated to each
  other and to this fork).
- GWM Haval H6 has **no full car port anywhere checked** — DBC file only,
  in `commaai/opendbc` and `kommuai/opendbc`. Nobody has shipped a
  `car/gwm` (or similarly named) interface directory.

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

## GWM Haval H6: no full port found anywhere checked

- `commaai/opendbc` (upstream, `master`): `opendbc/dbc/gwm_haval_h6_phev_2024.dbc`
  exists. No `opendbc/car/gwm` (or any brand-folder match for
  `gwm`/`haval`/`greatwall`) interface directory — DBC only, not wired to a
  `CarInterface`.
- `kommuai/opendbc` (`~/pilot/opendbc`): same DBC file present, same
  situation — no car interface directory.
- `qzwf/opendbc`, `shemps/byd-atto3-openpilot-port`: no GWM/Haval file of any
  kind.
- This fork (`EXO-ELEC/opendbc`): no GWM/Haval file of any kind. (The only
  `gwm` string hits in this fork's DBCs are Ford's `GWM` — "Global Wireless
  Module" — CAN node in `ford_lincoln_base_pt.dbc`/`FORD_CADS*.dbc`; false
  positive, unrelated to Great Wall Motors.)

So there is nothing to cross-check on our side for GWM Haval H6 — no repo
examined has a working port, only a shared DBC.

## `opendbc-data`: the "port claim" doesn't exist

`commaai/opendbc-data` (`~/panda/opendbc-data`, upstream remote, read-only —
**not** an EXO-ELEC fork) is comma's community longitudinal-tuning-report
archive, not a car-port repo. Its BYD/GWM footprint:

- BYD: **zero files** — no Atto 3 report, no BYD content of any kind.
- GWM Haval H6: **one file**,
  `longitudinal_reports/GWM_HAVAL_H6_075b133b6181e058_00000162--df9a818bf7.html`,
  added by commit `542af76` ("New report after tune from Gwm haval h6 (#20)"),
  listed in the repo's README as a `master`-branch longitudinal maneuver
  report. This is evidence that *someone* ran openpilot longitudinal on a
  Haval H6 well enough to generate a tuning report — it is not a "BYD Atto 3
  and GWM Haval H6 ported successfully" claim, and the repo makes no such
  claim in its own README.

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
