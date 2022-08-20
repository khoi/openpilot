#!/usr/bin/env python
import argparse
import os
import sys
from common.basedir import BASEDIR
from tools.lib.route import Route
import subprocess

os.environ['BASEDIR'] = BASEDIR

def get_arg_parser():
  parser = argparse.ArgumentParser(
      description="Join route videos",
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)

  parser.add_argument("data_dir", nargs='?',
                              help="Path to directory in which log and camera files are located.")

  return parser


def main(argv):
  args = get_arg_parser().parse_args(sys.argv[1:])
  if not args.data_dir:
    print('Data directory invalid.')
    return

  print(f"🔎 Searching for routes in {args.data_dir}")

  # list folders in args.data_dir
  folders = [f for f in os.listdir(args.data_dir) if os.path.isdir(os.path.join(args.data_dir, f))]

  route_names = set([("--".join(path.split("--")[0:2])) for path in folders])

  idx = 0
  for route_name in route_names:
    idx += 1
    route = Route(route_name, args.data_dir)
    print(f"🚗 {idx}/{len(route_names)} Route {route.name} - {route.max_seg_number + 1} segments")
    
    f_camera_paths = [f"'{segment.camera_path}'" for segment in route.segments]
    f_camera_path_dest = f"{args.data_dir}/{route.name}_fcamera.hevc"
    print(f"\t📹 saving fcam to {f_camera_path_dest}")
    command = f"cat {' '.join(f_camera_paths)} > '{f_camera_path_dest}'"
    subprocess.call(command, shell=True)

    e_camera_paths = [f"'{segment.ecamera_path}'" for segment in route.segments]
    e_camera_path_dest = f"{args.data_dir}/{route.name}_ecamera.hevc"
    print(f"\t📹 saving ecam to {e_camera_path_dest}")
    command = f"cat {' '.join(e_camera_paths)} > '{e_camera_path_dest}'"
    subprocess.call(command, shell=True)

    # save d_camera_path
    

    print("")

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
