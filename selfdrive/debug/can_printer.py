#!/usr/bin/env python3
import argparse
import binascii
from collections import defaultdict

import cereal.messaging as messaging
from common.realtime import sec_since_boot
from opendbc.can.parser import CANParser


signals = [
  ("CF_Gway_DrvKeyLockSw", "CGW1", 0),
  ("CF_Gway_DrvKeyUnlockSw", "CGW1", 1),  # 1 is unlocked
  ("CF_Gway_PassiveAccessUnlock", "CGW1", 0),
  ("CF_Gway_PassiveAccessLock", "CGW1", 0),
  ("CF_Gway_DrvDrSw", "CGW1", 0),
  ("CF_Gway_ParkBrakeSw", "CGW1", 0),
]


def can_printer(bus, max_msg, addr, ascii_decode):
  cp = CANParser("hyundai_kia_generic", signals, bus=0, enforce_checks=False)
  sm = messaging.SubMaster(['deviceState', 'sensorEvents'], poll=['sensorEvents'])

  can_sock = messaging.sub_sock('can', addr=addr)

  while 1:
    sm.update()
    can_strs = messaging.drain_sock_raw(can_sock, wait_for_one=True)
    cp.update_strings(can_strs)
    if sm.frame % (100 / 20.) == 0:
      print("_____________________")
      print(f"{cp.vl['CGW1']['CF_Gway_DrvKeyLockSw']}")
      print(f"{cp.vl['CGW1']['CF_Gway_DrvKeyUnlockSw']}")
      print(f"{cp.vl['CGW1']['CF_Gway_PassiveAccessUnlock']}")
      print(f"{cp.vl['CGW1']['CF_Gway_PassiveAccessLock']}")
      print(f"{cp.vl['CGW1']['CF_Gway_DrvDrSw']}")
      print(f"{cp.vl['CGW1']['CF_Gway_ParkBrakeSw']}")


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="simple CAN data viewer",
                                   formatter_class=argparse.ArgumentDefaultsHelpFormatter)

  parser.add_argument("--bus", type=int, help="CAN bus to print out", default=0)
  parser.add_argument("--max_msg", type=int, help="max addr")
  parser.add_argument("--ascii", action='store_true', help="decode as ascii")
  parser.add_argument("--addr", default="127.0.0.1")

  args = parser.parse_args()
  can_printer(args.bus, args.max_msg, args.addr, args.ascii)
