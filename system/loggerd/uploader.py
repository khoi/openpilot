#!/usr/bin/env python3
import bz2
import io
import json
import os
import random
import threading
import time
import traceback
from pathlib import Path
from subprocess import STDOUT, check_output

from cereal import log
import cereal.messaging as messaging
from common.api import Api
from common.params import Params
from common.realtime import set_core_affinity
from system.hardware import TICI
from system.loggerd.xattr_cache import getxattr, setxattr
from system.loggerd.config import ROOT
from system.swaglog import cloudlog
from datetime import datetime

NAS_IP="10.10.10.90"
NetworkType = log.DeviceState.NetworkType
UPLOAD_ATTR_NAME = 'user.upload'
UPLOAD_ATTR_VALUE = b'1'

allow_sleep = bool(os.getenv("UPLOADER_SLEEP", "1"))
force_wifi = os.getenv("FORCEWIFI") is not None
fake_upload = os.getenv("FAKEUPLOAD") is not None

def get_directory_sort(d):
  return list(map(lambda s: s.rjust(10, '0'), d.rsplit('--', 1)))

def listdir_by_creation(d):
  try:
    paths = os.listdir(d)
    paths = sorted(paths, key=get_directory_sort)
    return paths
  except OSError:
    cloudlog.exception("listdir_by_creation failed")
    return list()

def clear_locks(root):
  for logname in os.listdir(root):
    path = os.path.join(root, logname)
    try:
      for fname in os.listdir(path):
        if fname.endswith(".lock"):
          os.unlink(os.path.join(path, fname))
    except OSError:
      cloudlog.exception("clear_locks failed")


class Uploader():
  def __init__(self, dongle_id, root):
    self.dongle_id = dongle_id
    self.api = Api(dongle_id)
    self.root = root

    self.upload_thread = None

    self.last_resp = None
    self.last_exc = None

    self.immediate_size = 0
    self.immediate_count = 0

    # stats for last successfully uploaded file
    self.last_time = 0.0
    self.last_speed = 0.0
    self.last_filename = ""

    self.immediate_folders = ["crash/", "boot/"]
    self.immediate_priority = {"qlog": 0, "qcamera.ts": 1}
    self.high_priority = {"rlog": 0}
    self.normal_priority = {"fcamera.hevc": 1, "dcamera.hevc": 2, "ecamera.hevc": 3}

  def get_upload_sort(self, name):
    if name in self.immediate_priority:
      return self.immediate_priority[name]
    if name in self.high_priority:
      return self.high_priority[name] + 100
    if name in self.normal_priority:
      return self.normal_priority[name] + 300
    return 1000

  def list_upload_files(self):
    if not os.path.isdir(self.root):
      return

    self.immediate_size = 0
    self.immediate_count = 0

    for logname in listdir_by_creation(self.root):
      path = os.path.join(self.root, logname)
      try:
        names = os.listdir(path)
      except OSError:
        continue

      if any(name.endswith(".lock") for name in names):
        continue

      for name in sorted(names, key=self.get_upload_sort):
        key = os.path.join(logname, name)
        fn = os.path.join(path, name)
        # skip files already uploaded
        try:
          is_uploaded = getxattr(fn, UPLOAD_ATTR_NAME)
        except OSError:
          cloudlog.event("uploader_getxattr_failed", exc=self.last_exc, key=key, fn=fn)
          is_uploaded = True  # deleter could have deleted
        if is_uploaded:
          continue

        try:
          if name in self.immediate_priority:
            self.immediate_count += 1
            self.immediate_size += os.path.getsize(fn)
        except OSError:
          pass

        yield (name, key, fn)

  def next_file_to_upload(self, with_raw):
    upload_files = list(self.list_upload_files())

    for name, key, fn in upload_files:
      if any(f in fn for f in self.immediate_folders):
        return (key, fn)

    for name, key, fn in upload_files:
      if name in self.immediate_priority:
        return (key, fn)

    if with_raw:
      # then upload the full log files, rear and front camera files
      for name, key, fn in upload_files:
        if name in self.high_priority:
          return (key, fn)

      # Could add a param here to disable full video uploads
      for name, key, fn in upload_files:
        if name in self.normal_priority:
          return (key, fn)

    return None

  def do_upload(self, key, fn):
    class FakeResponse():
          def __init__(self, status_code=200):
            self.status_code = status_code

    try:
      args = ["/usr/bin/rsync", "--append", "--relative", "-e", "ssh -oStrictHostKeyChecking=no -p 2222 -i /data/id_rsa", "-azvhP", fn, f"khoi@{NAS_IP}:/volumeUSB1/usbshare/comma"]
      check_output(args, stderr=STDOUT, timeout=300, shell=False)
      try:
        check_output(["killall rsync"], stderr=STDOUT) # ensure rsync is no longer running
      except Exception:
        pass

      self.last_resp = FakeResponse()
    except Exception as e:
      self.last_exc = (e, traceback.format_exc())
      print(f"Upload Exception {self.last_exc}")
      raise

  def normal_upload(self, key, fn):
    self.last_resp = None
    self.last_exc = None

    try:
      self.do_upload(key, fn)
    except Exception:
      pass

    return self.last_resp

  def get_now_str(self):
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

  def upload(self, key, fn, network_type, metered):
    try:
      sz = os.path.getsize(fn)
    except OSError:
      cloudlog.exception("upload: getsize failed")
      return False

    print(f"{self.get_now_str()} upload_start {fn}")

    if sz == 0:
      try:
        # tag files of 0 size as uploaded
        setxattr(fn, UPLOAD_ATTR_NAME, UPLOAD_ATTR_VALUE)
      except OSError:
        print(f"uploader_setxattr_failed 1 {fn}")
      success = True
    else:
      start_time = time.monotonic()
      stat = self.normal_upload(key, fn)
      if stat is not None and stat.status_code in (200, 201, 401, 403, 412):
        try:
          # tag file as uploaded
          setxattr(fn, UPLOAD_ATTR_NAME, UPLOAD_ATTR_VALUE)
        except OSError:
          print(f"uploader_setxattr_failed 2 {fn}")
        self.last_filename = fn
        self.last_time = time.monotonic() - start_time
        self.last_speed = (sz / 1e6) / self.last_time
        success = True
        print(f"{self.get_now_str()} upload_success {fn}" if stat.status_code != 412 else f"upload_ignored {fn}")
      else:
        success = False
        print(f"{self.get_now_str()} upload_failed {fn}")

    return success

  def get_msg(self):
    msg = messaging.new_message("uploaderState")
    us = msg.uploaderState
    us.immediateQueueSize = int(self.immediate_size / 1e6)
    us.immediateQueueCount = self.immediate_count
    us.lastTime = self.last_time
    us.lastSpeed = self.last_speed
    us.lastFilename = self.last_filename
    return msg


def uploader_fn(exit_event):
  try:
    set_core_affinity([0, 1, 2, 3])
  except Exception:
    cloudlog.exception("failed to set core affinity")

  clear_locks(ROOT)

  params = Params()
  dongle_id = params.get("DongleId", encoding='utf8')

  if dongle_id is None:
    cloudlog.info("uploader missing dongle_id")
    raise Exception("uploader can't start without dongle id")

  if TICI and not Path("/data/media").is_mount():
    cloudlog.warning("NVME not mounted")

  sm = messaging.SubMaster(['deviceState'])
  pm = messaging.PubMaster(['uploaderState'])
  uploader = Uploader(dongle_id, ROOT)

  backoff = 0.1
  while not exit_event.is_set():
    sm.update(0)
    offroad = params.get_bool("IsOffroad")
    network_type = sm['deviceState'].networkType if not force_wifi else NetworkType.wifi
    if network_type == NetworkType.none:
      if allow_sleep:
        time.sleep(60 if offroad else 5)
      continue

    allow_raw_upload = True
    d = uploader.next_file_to_upload(with_raw=allow_raw_upload)
    if d is None:  # Nothing to upload
      if allow_sleep:
        time.sleep(60 if offroad else 5)
      continue

    key, fn = d

    success = uploader.upload(key, fn, sm['deviceState'].networkType.raw, sm['deviceState'].networkMetered)
    if success:
      backoff = 0.1
    elif allow_sleep:
      cloudlog.info("upload backoff %r", backoff)
      time.sleep(backoff + random.uniform(0, backoff))
      backoff = min(backoff*2, 120)

    pm.send("uploaderState", uploader.get_msg())


def main():
  uploader_fn(threading.Event())


if __name__ == "__main__":
  main()
