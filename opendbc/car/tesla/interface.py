#!/usr/bin/env python3
from opendbc.car import structs
from opendbc.car.interfaces import CarInterfaceBase
from opendbc.car.tesla.radar_interface import BROWNPANDA_RADAR_CARS, RadarInterface
from opendbc.car.tesla.values import CANBUS


class CarInterface(CarInterfaceBase):
  RadarInterface = RadarInterface

  @staticmethod
  def _get_params(ret: structs.CarParams, candidate, fingerprint, car_fw, experimental_long, docs) -> structs.CarParams:
    ret.brand = "tesla"

    # Needs safety validation and final testing before pulling out of dashcam
    ret.dashcamOnly = True

    # Not merged yet
    #ret.safetyConfigs = [get_safety_config(structs.CarParams.SafetyModel.tesla)]

    ret.steerLimitTimer = 1.0
    ret.steerActuatorDelay = 0.25

    ret.steerControlType = structs.CarParams.SteerControlType.angle
    party_fingerprint = fingerprint.get(CANBUS.party, {})
    radar_present = party_fingerprint.get(0x401) == 8 and party_fingerprint.get(0x45F) == 8
    ret.radarUnavailable = candidate not in BROWNPANDA_RADAR_CARS or not radar_present

    ret.experimentalLongitudinalAvailable = True
    if experimental_long:
      ret.openpilotLongitudinalControl = True
      # ret.safetyConfigs[0].safetyParam |= TeslaPandaFlags.FLAG_TESLA_LONG_CONTROL.value

    return ret
