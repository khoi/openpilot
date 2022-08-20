#!/usr/bin/env python
import argparse
import os
import sys
from common.basedir import BASEDIR
from tools.lib.logreader import MultiLogIterator
from tools.lib.route import Route
import datetime
import json 

os.environ['BASEDIR'] = BASEDIR

LOG_HERTZ = 10 # 10 hz = 0.1 sec, higher for higher accuracy, 10hz seems fine
LOG_LENGTH = 10 # mins, higher means it keeps more data in the memory, will take more time to write into a file too.
LOST_SIGNAL_COUNT_LENGTH = 30 # secs, output log file if we lost signal for this long

# do not change
LOST_SIGNAL_COUNT_MAX = LOST_SIGNAL_COUNT_LENGTH * LOG_HERTZ # secs,
LOGS_PER_FILE = LOG_LENGTH * 60 * LOG_HERTZ # e.g. 10 * 60 * 10 = 6000 points per file

INTERESTING_LOG_KEYS = {
  "carState",
  "gpsLocationExternal"
}
class GpxD():
  def __init__(self):
    self.log_count = 0
    self.logs = list()
    self.lost_signal_count = 0
    self.started_time = None

  def log(self, gps):
    # do not log when no fix or accuracy is too low, add lost_signal_count
    if gps.flags % 2 == 0 or gps.accuracy > 5.:
      if self.log_count > 0:
        self.lost_signal_count += 1
    else:
      self.logs.append([datetime.datetime.utcfromtimestamp(gps.unixTimestampMillis*0.001).isoformat(), str(gps.latitude), str(gps.longitude), str(gps.altitude)])
      self.log_count += 1
      self.lost_signal_count = 0
      if not self.started_time:
        self.started_time = datetime.datetime.utcfromtimestamp(gps.unixTimestampMillis*0.001).isoformat()

  def write_log(self, path, force = False):
    if self.log_count == 0:
      return

    if force or (self.log_count >= LOGS_PER_FILE or self.lost_signal_count >= LOST_SIGNAL_COUNT_MAX):
      self._write_gpx(path)
      self.lost_signal_count = 0
      self.log_count = 0
      self.logs.clear()

  def _write_gpx(self, path):
    if len(self.logs) > 1:
      if not os.path.exists(path):
        os.makedirs(path)
      filename = self.started_time.replace(':','-')
      str = ''
      str += "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\n"
      str += "<gpx version=\"1.1\" creator=\"openpilot https://github.com/khoi/openpilot\" xmlns=\"http://www.topografix.com/GPX/1/1\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">\n"
      str += "<trk>\n"
      str += "  <name>" + self.started_time + "</name>"
      str += "  <trkseg>\n"
      for trkpt in self.logs:
        str += self._trkpt_template(trkpt[1], trkpt[2], trkpt[3], trkpt[0])
      str += "  </trkseg>\n"
      str += "</trk>\n"
      str += "</gpx>\n"
      try:
        f = open('%s%sZ.gpx' % (path, filename), 'w')
        f.write(str)
        f.close()
      except:
        pass

  def _trkpt_template(self, lat, lon, ele, time):
    str = ""
    str += "    <trkpt lat=\"" + lat + "\" lon=\"" + lon + "\">\n"
    str += "      <ele>" + ele + "</ele>\n"
    str += "      <time>" + time + "</time>\n"
    str += "    </trkpt>\n"
    return str

def get_arg_parser():
  parser = argparse.ArgumentParser(
      description="Extract GPX from route",
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)

  parser.add_argument("data_dir", nargs='?',
                              help="Path to directory in which log and camera files are located.")
  parser.add_argument("route_name", type=(lambda x: x.replace("#", "|")), nargs="?",
                      help="The route whose messages will be published.")
  return parser


def main(argv):
  args = get_arg_parser().parse_args(sys.argv[1:])
  if not args.data_dir:
    print('Data directory invalid.')
    return

  if not args.route_name:
    # Extract route name from path
    args.route_name = os.path.basename(args.data_dir)
    args.data_dir = os.path.dirname(args.data_dir)


  route = Route(args.route_name, args.data_dir)

  lr = MultiLogIterator(route.log_paths(), sort_by_time=True)
  
  gpxD = GpxD()

  start_log_mono_time = None
  metadatas = []

  for msg in lr:
    if not start_log_mono_time: 
      start_log_mono_time = msg.logMonoTime

    relative_mono_time = msg.logMonoTime - start_log_mono_time
    print(relative_mono_time / 1e9)
    if not msg or not msg.valid:
      continue
    if msg.which() == 'gpsLocationExternal':
      gpxD.log(msg.gpsLocationExternal)
    if msg.which() in INTERESTING_LOG_KEYS:
      data = dict()
      data["type"] = msg.which()
      data["relativeMonoTime"] = relative_mono_time
      data["payload"] = msg.to_dict()
      metadatas.append(data)
      
  with open("sample.json", "w") as f:
    json.dump(metadatas, f)
  gpxD.write_log("./", True)

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
