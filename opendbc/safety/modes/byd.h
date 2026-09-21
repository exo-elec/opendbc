#pragma once

#include "opendbc/safety/declarations.h"

// BYD (Atto 3, Seal, Sealion, etc.) - ported from panda safety onto this fork's core, 2026-08.
// byd_zone_interp() below originally had a missing upper clamp, found and fixed this same port
// (extrapolated past the top breakpoint at high speed, inverting the backstop and rejecting all
// valid commands above ~130 km/h) - the same bug was independently present, and fixed, in
// dev/EDP10's own byd.h and in the TC275_BrownPanda firmware's Safety_ZoneInterp(). Verified via
// libsafety_py round-trips against real controller output, not by inspection alone.
#define BYD_STEERING_MODULE_ADAS 0x1E2U   // 482
#define BYD_ACC_MPC_STATE        0x316U   // 790 mpc_lka LKAS command
#define BYD_ACC_EPS_STATE        0x318U   // 792 mpc_lka EPS handshake
#define BYD_ACC_CMD              0x32EU   // 814
#define BYD_PCM_BUTTONS          0x3B0U   // 944
#define BYD_STEERING_TORQUE      0x1FCU   // 508

static bool byd_alt_engage = false;
static bool byd_steering_torque_spoof = false;
static bool byd_mpc_lka_engage = false;
static bool byd_relax_controls = false;
static bool byd_stock_long = false;

static void byd_rx_hook(const CANPacket_t *msg) {
  int bus = (int)msg->bus;
  int addr = (int)msg->addr;

  if (bus == 0) {
    if (addr == 287) {
      int angle_meas_new = (GET_BYTES(msg, 0, 2) & 0xFFFFU);
      angle_meas_new = to_signed(angle_meas_new, 16);
      update_sample(&angle_meas, angle_meas_new);
    }

    if (addr == 834) {
      gas_pressed = (msg->data[0] > 0U);
      brake_pressed = (msg->data[1] > 0U);
    }

    if (addr == 496) {
      // byd_general_pt.dbc WHEEL_SPEED: FL 0|12, FR 16|12 (BL is 28|12, BR 40|12).
      // The second field read here is FR, not BL - it was named bl_raw until 2026-09-21.
      // Averaging the two fronts is the same speed estimate either way; only the name
      // was wrong, and it hid the fact that no rear wheel is read.
      // NOTE: the 0.1 factor is byd_general_pt.dbc's and is disputed. byd_atto3.dbc decodes
      // this frame as one 16-bit value at 1/14 km/h per count from local captures, and
      // qzwf/opendbc corrects a 0.1 factor by 40/53 after measuring raw values ~32.5% high
      // against the odometer. Both land near 0.072-0.075. Unresolved pending a local GPS or
      // odometer run - see docs/BYD_ATTO3_QZWF_REFERENCE_PORT.md.
      uint16_t fl_raw = (uint16_t)(((msg->data[1] & 0x0FU) << 8U) | msg->data[0]);
      uint16_t fr_raw = (uint16_t)(((msg->data[3] & 0x0FU) << 8U) | msg->data[2]);
      float fl_ms = ((float)fl_raw) * 0.1f * (1.0f / 3.6f);
      float fr_ms = ((float)fr_raw) * 0.1f * (1.0f / 3.6f);
      float speed = (fl_ms + fr_ms) * 0.5f;
      vehicle_moving = SAFETY_ABS(speed) > 0.1f;
      UPDATE_VEHICLE_SPEED(speed);
    }

    if (byd_mpc_lka_engage && (addr == (int)BYD_ACC_EPS_STATE)) {
      int torque_motor = (int)(((msg->data[2] & 0x0FU) << 8U) | msg->data[1]);
      torque_motor = to_signed(torque_motor, 12);
      update_sample(&torque_meas, torque_motor);
    }

    if (addr == 944) {
      // PCM_BUTTONS bit numbering per byd_general_pt.dbc, matched field for field by
      // qzwf/opendbc: SET_BTN @3, RES_BTN @4, LKAS_ON_BTN @6, ACC_ON_BTN @19.
      bool set_pressed = (((msg->data[0] >> 3U) & 1U) != 0U);
      bool res_pressed = (((msg->data[0] >> 4U) & 1U) != 0U);
      bool icc_pressed = (((msg->data[0] >> 6U) & 1U) != 0U);
      // ACC_ON_BTN (bit 19) is this mode's cancel - send_buttons() transmits cancel through
      // it. Until 2026-09-21 this same bit was also read as acc_pressed into the grant
      // below, so setting it granted controls_allowed and then immediately cleared it on
      // the same frame, taking any simultaneous SET/RES/ICC press with it. Reading it once
      // as cancel is exactly what the two reads together already did: this bit could never
      // leave controls_allowed set, and it still cannot.
      bool cancel_pressed = (((msg->data[2] >> 3U) & 1U) != 0U);

      if (set_pressed || res_pressed || icc_pressed) {
        controls_allowed = true;
      }
      if (cancel_pressed) {
        controls_allowed = false;
      }
    }
  }

  if (byd_mpc_lka_engage) {
    if ((bus == 0) && (addr == 813)) {
      uint8_t acc_state = (msg->data[2] >> 3) & 0x7U;
      bool engaged = (acc_state == 3U) || (acc_state == 5U);
      pcm_cruise_check(engaged);
    }
  } else if (byd_alt_engage) {
    if (addr == 813) {
      uint8_t state = (msg->data[5] >> 4) & 0xFU;
      bool engaged = (state == 3U) || (state == 5U) || (state == 6U) || (state == 7U);
      pcm_cruise_check(engaged);
    }
  } else {
    if (addr == 814) {
      bool engaged = (msg->data[5] >> 4) & 1U;
      pcm_cruise_check(engaged);
    }
  }

  // SEAL (safetyParam 2) and Song Plus MPC LKA (safetyParam 4): pcm_cruise_check only
  // grants on ACC rising edge and safety_tick can clear controls_allowed while stock ACC
  // stays engaged, so OP lateral TX gets blocked without relax_controls.
  if (byd_relax_controls) {
    controls_allowed = true;
  }
}

static bool byd_tx_hook(const CANPacket_t *msg) {
  bool violation = false;
  int addr = (int)msg->addr;
  int bus = (int)msg->bus;

  if (byd_mpc_lka_engage && (bus == 0) && (addr == (int)BYD_ACC_MPC_STATE)) {
    static const TorqueSteeringLimits BYD_MPC_LKA_STEERING_LIMITS = {
      .max_torque = 300,
      .max_rate_up = 9,
      .max_rate_down = 9,
      .max_torque_error = 80,
      .max_rt_delta = 113,
      .type = TorqueMotorLimited,
    };
    int desired_torque = (int)(((msg->data[3] & 0x07U) << 8U) | msg->data[2]);
    desired_torque = to_signed(desired_torque, 11);
    bool steer_req = (msg->data[3] >> 4) & 1U;
    if (steer_torque_cmd_checks(desired_torque, steer_req, BYD_MPC_LKA_STEERING_LIMITS)) {
      violation = true;
    }
    return !violation;
  }

  if (addr == (int)BYD_STEERING_MODULE_ADAS) {
    int desired_angle = (GET_BYTES(msg, 3, 2) & 0xFFFFU);
    bool lka_active = (msg->data[2] >> 5) & 1U;
    desired_angle = to_signed(desired_angle, 16);

    /*
     * Angle rate lookup Y values are **max delta in degrees per steering TX frame** (same numeric scale as
     * opendbc/car/byd/values.py CarControllerParams ANGLE_RATE_LIMIT_* angle_v with apply_std_steer_angle_limits).
     * lateral.h multiplies by angle_deg_to_can (+1); there is no extra Hz division in steer_angle_cmd_checks.
     *
     * If these Y match values.py (~6/3/1 up, ~8/6/4 down), a single violation can still burst-block TX:
     * steer_angle_cmd_checks resets desired_angle_last to 0 while the next frame may carry a large absolute
     * command (~20+ deg). Larger Y here tolerates that generic reset until last-angle handling is fixed.
     */
    // NOTE: upstream's AngleSteeringLimits here has no max_angle_error/enforce_angle_error/
    // angle_is_curvature/inactive_angle_is_zero fields (fork API drift vs. the source this was
    // ported from). Every one of those was set to a no-op value there anyway (enforce_angle_error
    // false, angle_is_curvature false), except inactive_angle_is_zero=true, which this fork's
    // shared steer_angle_cmd_inactive_check() doesn't offer a toggle for — it always requires the
    // inactive commanded angle to track angle_meas, which is the stricter of the two options, not
    // a weaker one. No behavior change versus the source config.
    static const AngleSteeringLimits BYD_STEERING_LIMITS = {
      // 1200 CAN units = 120.0 deg, matching values.py CarControllerParams.STEER_ANGLE_MAX
      // and the 120 deg advertised in the car docs. It read 450 (45 deg) until 2026-09-21,
      // agreeing with neither. max_angle does not bound the active command - it bounds the
      // inactive-angle window and the post-violation reset of desired_angle_last - so the
      // old value was itself a cause of the burst-blocking described above: with the wheel
      // past 45 deg the reset landed 55+ deg from the angle actually being commanded and
      // the following frame violated again. The active command is bounded by the rate
      // lookup below and, in the controller, by MAX_STEER_ANGLE_OFFSET_DEG, which clamps
      // it to 10 deg either side of the measured angle.
      // qzwf/opendbc uses 900 (90 deg) with "the EPS faults past this" on an India-market
      // car. If that holds here, this, values.py and the car docs should move together.
      .max_angle = 1200,
      .angle_deg_to_can = 10,
      .angle_rate_up_lookup = {{0., 5., 15.}, {28., 26., 22.}},
      .angle_rate_down_lookup = {{0., 5., 15.}, {28., 26., 22.}},
      .frequency = 50U,
    };
    if (steer_angle_cmd_checks(desired_angle, lka_active, BYD_STEERING_LIMITS)) {
      violation = true;
    }
  }

  if (addr == (int)BYD_ACC_CMD) {
    // Stock-long platforms forward camera ACC_CMD and must not TX it.
    if (byd_stock_long) {
      return false;
    }
    // ACCEL_CMD raw byte. byd_general_pt.dbc scales it 0.05 with a -5 offset, so these raw
    // counts are +1.75 / -4.00 m/s^2 with 0.00 inactive. Stock logs use down to raw 20.
    // The carcontroller clips to raw 130 / raw 20 (+1.50 / -4.00 m/s^2), inside this.
    // These are raw counts and so were unaffected by the 2026-09-21 scale correction.
    static const LongitudinalLimits BYD_LONG_LIMITS = {
      .max_accel = 135,
      .min_accel = 20,
      .inactive_accel = 100,
    };
    violation |= longitudinal_accel_checks((int)msg->data[0], BYD_LONG_LIMITS);
  }
  return !violation;
}

static bool byd_fwd_hook(int bus_num, int addr) {
  bool block = false;

  if (byd_mpc_lka_engage) {
    if ((bus_num == 0) && (addr == (int)BYD_ACC_EPS_STATE)) {
      block = true;
    } else if ((bus_num == 2) && ((addr == (int)BYD_ACC_MPC_STATE) || (addr == (int)BYD_ACC_CMD))) {
      block = true;
    }
    return block;
  }

  if (bus_num == 0) {
    bool is_torque_msg = (addr == (int)BYD_STEERING_TORQUE);
    block = is_torque_msg && byd_steering_torque_spoof;
  } else if (bus_num == 2) {
    bool is_lkas_msg = (addr == 0x1E2) || (addr == 0x316);
    bool is_acc_msg = (addr == (int)BYD_ACC_CMD);
    // Stock long: forward camera ACC_CMD. OP long: block it so we can TX our own.
    block = is_lkas_msg || (is_acc_msg && !byd_stock_long);
  } else {
    /* no action */
  }
  return block;
}

static safety_config byd_init(uint16_t param) {
  // Reset mode flags every init — these are sticky statics and must not leak across params.
  byd_alt_engage = false;
  byd_steering_torque_spoof = false;
  byd_mpc_lka_engage = false;
  byd_relax_controls = false;
  byd_stock_long = false;

  safety_config cfg;
  static const CanMsg BYD_TX_MSGS[] = {
    {0x1E2, 0, 8, .check_relay = true},   // STEERING_MODULE_ADAS
    {0x316, 0, 8, .check_relay = true},   // LKAS_HUD_ADAS
    {0x32E, 0, 8, .check_relay = false},   // ACC_CMD
    {0x3B0, 0, 8, .check_relay = false},  // PCM_BUTTONS
    {0x3B0, 2, 8, .check_relay = false},
    {0x1FC, 2, 8, .check_relay = false},  // STEERING_TORQUE spoof TX (cam bus)
  };

  static RxCheck byd_rx_checks[] = {
    {.msg = {{287, 0, 5, 100U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
    {.msg = {{496, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
    {.msg = {{508, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
    {.msg = {{834, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
    {.msg = {{944, 0, 8, 20U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
    {.msg = {{814, 2, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
  };

  cfg = BUILD_SAFETY_CFG(byd_rx_checks, BYD_TX_MSGS);
  if (param == 1U) {
    // Atto 3: ACC_CMD(814) engage + block PT→cam 0x1FC; cam_lka TX's spoof on bus 2.
    byd_steering_torque_spoof = true;
  } else if (param == 2U) {
    static RxCheck byd_rx_checks_alt[] = {
      {.msg = {{287, 0, 5, 100U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{496, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{508, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{834, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{944, 0, 8, 20U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{813, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
    };
    byd_alt_engage = true;
    byd_steering_torque_spoof = true;
    byd_relax_controls = true;
    cfg = BUILD_SAFETY_CFG(byd_rx_checks_alt, BYD_TX_MSGS);
  } else if (param == 3U) {
    byd_alt_engage = true;
    byd_steering_torque_spoof = true;
  } else if (param == 5U) {
    // Seal 6 stock longitudinal: same lateral/engage path as param 3, but forward
    // camera ACC_CMD and omit it from the TX allow-list so OP cannot conflict.
    static const CanMsg BYD_TX_MSGS_STOCK_LONG[] = {
      {0x1E2, 0, 8, .check_relay = true},   // STEERING_MODULE_ADAS
      {0x316, 0, 8, .check_relay = true},   // LKAS_HUD_ADAS
      {0x3B0, 0, 8, .check_relay = false},  // PCM_BUTTONS
      {0x3B0, 2, 8, .check_relay = false},
      {0x1FC, 2, 8, .check_relay = false},  // STEERING_TORQUE
    };
    byd_alt_engage = true;
    byd_steering_torque_spoof = true;
    byd_stock_long = true;
    cfg = BUILD_SAFETY_CFG(byd_rx_checks, BYD_TX_MSGS_STOCK_LONG);
  } else if (param == 4U) {
    static const CanMsg BYD_MPC_LKA_TX_MSGS[] = {
      {0x316, 0, 8, .check_relay = true},   // ACC_MPC_STATE -> EPS
      {0x318, 2, 8, .check_relay = false},  // ACC_EPS_STATE fake -> MPC
    };
    static RxCheck byd_rx_checks_mpc_lka[] = {
      {.msg = {{287, 0, 5, 100U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{496, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{834, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{944, 0, 8, 20U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{792, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{813, 0, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
      {.msg = {{790, 2, 8, 50U, .ignore_checksum = true, .ignore_counter = true, .ignore_quality_flag = true}, {0}, {0}}},
    };
    byd_mpc_lka_engage = true;
    byd_relax_controls = true;
    cfg = BUILD_SAFETY_CFG(byd_rx_checks_mpc_lka, BYD_MPC_LKA_TX_MSGS);
  } else {
    /* keep default cfg */
  }
  return cfg;
}

const safety_hooks byd_hooks = {
  .init = byd_init,
  .rx = byd_rx_hook,
  .tx = byd_tx_hook,
  .fwd = byd_fwd_hook,
};
