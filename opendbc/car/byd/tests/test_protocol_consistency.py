#!/usr/bin/env python3
"""Regression tests for the 2026-09-21 cross-repo protocol consistency pass.

Each test pins one thing that was inconsistent between byd_general_pt.dbc,
opendbc/car/byd, opendbc/safety/modes/byd.h and the two reference decodes
(BYD_Atto3/DBC/byd_atto3.dbc and qzwf/opendbc). See
docs/BYD_ATTO3_QZWF_REFERENCE_PORT.md for the evidence behind each one.
"""
import unittest

from opendbc.can.packer import CANPacker
from opendbc.can.parser import CANParser
from opendbc.car.byd.cam_lka.bydcan import (
  ACCEL_CMD_MAX,
  ACCEL_CMD_MIN,
  create_accel_command,
  create_can_steer_command,
)
from opendbc.car.byd.values import CAR, DBC, CarControllerParams
from opendbc.car.structs import CarParams
from opendbc.safety.tests.libsafety import libsafety_py

# byd.h's LongitudinalLimits for ACC_CMD, in raw ACCEL_CMD counts.
SAFETY_ACCEL_RAW_MAX = 135
SAFETY_ACCEL_RAW_MIN = 20
SAFETY_ACCEL_RAW_INACTIVE = 100

# byd.h's AngleSteeringLimits.max_angle, in CAN units (0.1 deg each).
SAFETY_MAX_ANGLE_CAN = 1200

PCM_BUTTONS = 0x3B0
STEERING_MODULE_ADAS = 0x1E2


def _pack(packer, name, values, bus=0):
  return packer.make_can_msg(name, bus, values)


class TestAccelCommandScale(unittest.TestCase):
  """ACCEL_CMD carries m/s^2 directly now; ACCEL_MULT is gone."""

  def setUp(self):
    self.packer = CANPacker(DBC[CAR.BYD_ATTO3]["pt"])
    self.parser = CANParser(DBC[CAR.BYD_ATTO3]["pt"], [("ACC_CMD", 0)], 0)

  def _round_trip(self, accel):
    addr, dat, bus = create_accel_command(self.packer, accel, True, False)
    self.parser.update([(0, [(addr, dat, bus)])])
    return dat[0], self.parser.vl["ACC_CMD"]["ACCEL_CMD"]

  def test_requested_accel_reaches_the_wire_unscaled(self):
    # The bug this replaces: ACCEL_MULT=26 against a raw (1, -100) DBC put 1.30x
    # the requested value on the wire. Inside the clip it must now be 1:1.
    for accel in (-3.5, -2.0, -1.0, -0.25, 0.0, 0.5, 1.0, 1.5):
      with self.subTest(accel=accel):
        _, decoded = self._round_trip(accel)
        self.assertAlmostEqual(accel, decoded, places=6,
                               msg=f"requested {accel} m/s2, car decodes {decoded}")

  def test_clip_lands_on_the_captured_envelope(self):
    # +1.50 m/s2 is the ceiling BYD_Atto3's COMMA-device captures recorded and
    # -4.00 the floor; both are exact raw counts under the 0.05/-5 DBC scale.
    self.assertEqual((130, 1.5), self._round_trip(ACCEL_CMD_MAX))
    self.assertEqual((20, -4.0), self._round_trip(ACCEL_CMD_MIN))
    self.assertEqual((130, 1.5), self._round_trip(99.0))
    self.assertEqual((20, -4.0), self._round_trip(-99.0))

  def test_zero_accel_is_the_safety_inactive_value(self):
    raw, decoded = self._round_trip(0.0)
    self.assertEqual(SAFETY_ACCEL_RAW_INACTIVE, raw)
    self.assertEqual(0.0, decoded)

  def test_every_command_stays_inside_the_byd_h_envelope(self):
    # byd.h checks raw counts, so it is scale-agnostic - this is what proves the
    # scale change did not widen longitudinal authority.
    for accel in (-99.0, -4.0, -1.0, 0.0, 1.0, 1.5, 99.0):
      with self.subTest(accel=accel):
        raw, _ = self._round_trip(accel)
        self.assertGreaterEqual(raw, SAFETY_ACCEL_RAW_MIN)
        self.assertLessEqual(raw, SAFETY_ACCEL_RAW_MAX)


class TestInactiveSteeringAngle(unittest.TestCase):
  """The inactive steering command tracks the measured angle, not zero."""

  def setUp(self):
    self.packer = CANPacker(DBC[CAR.BYD_ATTO3]["pt"])
    self.parser = CANParser(DBC[CAR.BYD_ATTO3]["pt"], [("STEERING_MODULE_ADAS", 0)], 0)

  def _steer_angle(self, angle, steer_req):
    addr, dat, bus = create_can_steer_command(self.packer, angle, steer_req, False, True, False)
    self.parser.update([(0, [(addr, dat, bus)])])
    return self.parser.vl["STEERING_MODULE_ADAS"]["STEER_ANGLE"]

  def test_inactive_command_passes_through_the_measured_angle(self):
    # carcontroller hands in CS.out.steeringAngleDeg whenever steer_req is false;
    # this used to be forced to 0 here, which violates
    # steer_angle_cmd_inactive_check() on any off-centre wheel.
    for angle in (-100.0, -12.3, 0.0, 12.3, 100.0):
      with self.subTest(angle=angle):
        self.assertAlmostEqual(angle, self._steer_angle(angle, False), places=1)

  def test_active_command_is_unchanged(self):
    for angle in (-45.0, 0.0, 45.0):
      with self.subTest(angle=angle):
        self.assertAlmostEqual(angle, self._steer_angle(angle, True), places=1)


class TestSafetyLimitsAgreeWithController(unittest.TestCase):
  def test_max_angle_matches_car_controller_params(self):
    # byd.h read 450 (45 deg) against values.py's 120 deg. max_angle bounds the
    # inactive-angle window and the post-violation reset, so a value below the
    # controller's own range made both track something the car was never at.
    self.assertEqual(SAFETY_MAX_ANGLE_CAN,
                     int(CarControllerParams.STEER_ANGLE_MAX * 10))


class TestPcmButtons(unittest.TestCase):
  """ACC_ON_BTN (bit 19) is cancel only; it must never leave controls allowed."""

  def setUp(self):
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.byd, 1)
    self.packer = CANPacker(DBC[CAR.BYD_ATTO3]["pt"])

  def _rx_buttons(self, **values):
    addr, dat, bus = _pack(self.packer, "PCM_BUTTONS", values)
    msg = libsafety_py.make_CANPacket(addr, bus, dat)
    self.safety.safety_rx_hook(msg)

  def test_set_and_res_grant_controls(self):
    for btn in ("SET_BTN", "RES_BTN", "LKAS_ON_BTN"):
      with self.subTest(btn=btn):
        self.safety.set_controls_allowed(False)
        self._rx_buttons(**{btn: 1})
        self.assertTrue(self.safety.get_controls_allowed(), f"{btn} should grant")

  def test_acc_on_btn_cancels(self):
    self.safety.set_controls_allowed(True)
    self._rx_buttons(ACC_ON_BTN=1)
    self.assertFalse(self.safety.get_controls_allowed())

  def test_acc_on_btn_still_wins_over_a_simultaneous_grant(self):
    # This is the pre-fix behaviour being pinned, not changed: the duplicated
    # read meant a SET press on the same frame as ACC_ON_BTN was cancelled.
    # Reading the bit once as cancel keeps that, without the dead grant term.
    self.safety.set_controls_allowed(False)
    self._rx_buttons(SET_BTN=1, ACC_ON_BTN=1)
    self.assertFalse(self.safety.get_controls_allowed())

  def test_no_button_pressed_changes_nothing(self):
    self.safety.set_controls_allowed(True)
    self._rx_buttons()
    self.assertTrue(self.safety.get_controls_allowed())


class TestWheelSpeedDecode(unittest.TestCase):
  """byd.h reads WHEEL_SPEED's two front wheels; it named the second one BL."""

  def setUp(self):
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.byd, 1)
    self.packer = CANPacker(DBC[CAR.BYD_ATTO3]["pt"])

  def _rx_speed(self, fl_kph, fr_kph, bl_kph):
    addr, dat, bus = _pack(self.packer, "WHEEL_SPEED", {
      "WHEELSPEED_FL": fl_kph, "WHEELSPEED_FR": fr_kph,
      "WHEELSPEED_BL": bl_kph, "WHEELSPEED_BR": bl_kph,
    })
    msg = libsafety_py.make_CANPacket(addr, bus, dat)
    self.safety.safety_rx_hook(msg)

  def test_speed_comes_from_the_two_front_wheels(self):
    # Rear wheels moved, fronts stopped -> still standstill. If the second read
    # were really BL (28|12) this would report motion.
    self._rx_speed(0.0, 0.0, 100.0)
    self.assertFalse(self.safety.get_vehicle_moving())

    self._rx_speed(50.0, 50.0, 0.0)
    self.assertTrue(self.safety.get_vehicle_moving())


if __name__ == "__main__":
  unittest.main()
