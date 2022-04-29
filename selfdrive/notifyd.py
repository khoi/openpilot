#!/usr/bin/env python3

import requests
import time
import os
from cereal import messaging
from common.realtime import Ratekeeper
from selfdrive.camerad.snapshot.snapshot import jpeg_write, snapshot


BOT_TOKEN = "5317619693:AAGJhwotJDDAHkpq1Qo8xyJNIB88xS2ePeU"
FRONT_PIC_TMP_PATH = "/tmp/front_pic.jpg"
REAR_PIC_TMP_PATH = "/tmp/rear_pic.jpg"

DEBUG = True

def debug_print(*args, **kwargs):
  if not DEBUG:
    return
  print(*args, **kwargs)

class NotifyD():
    def __init__(self):
        self.last_processed_update_id = 0

    def process(self, sm):
      try:
        updates = get_updates()
        last_update = updates["result"][-1]
        chat_id = last_update["message"]["chat"]["id"]

        if last_update["message"]["from"]["username"] != "khoiracle":
          return

        if self.last_processed_update_id == 0: # skip first update
          self.last_processed_update_id = last_update["update_id"]
          return

        if self.last_processed_update_id != last_update["update_id"]:
          debug_print(f"processing {last_update}")
          self.last_processed_update_id = last_update["update_id"]

          debug_print(send_text(chat_id, "Getting your photos"))

          takeSnapShot()

          for p in [FRONT_PIC_TMP_PATH, REAR_PIC_TMP_PATH]:
            if os.path.exists(p):
              debug_print(send_photo(chat_id, p))
              os.remove(p)
          
      except Exception as e:
        debug_print("Oops!", e, "occurred.")
        try:
            send_text(chat_id, f"{e}")
        except:
            pass
        

def notifyd_thread(sm=None):
    notifyd = NotifyD()
    rk = Ratekeeper(1 / 5, print_delay_threshold=None)

    if sm is None:
        sm = messaging.SubMaster(['deviceState', 'carState'])

    while True:
        sm.update(0)
        notifyd.process(sm)
        rk.keep_time()

def takeSnapShot():
  pic, fpic = snapshot()
  if pic is not None:
    jpeg_write(FRONT_PIC_TMP_PATH, pic)
    if fpic is not None:
      jpeg_write(REAR_PIC_TMP_PATH, fpic)
  else:
    raise Exception("not available while camerad is started")

def get_updates():
  return requests.get(f"https://api.telegram.org/bot{BOT_TOKEN}/getUpdates").json()

def send_text(chat_id, txt):
  # send text to telegram
  return requests.post(f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage", data={
    "chat_id": chat_id,
    "text": txt
  }).json()

def send_photo(chat_id, file_path):
  # send photo to telegram

  multipart_form_data = {
    'photo': ('1.jpeg', open(file_path, "rb")),
    'chat_id': (None, chat_id),
}

  return requests.post(f"https://api.telegram.org/bot{BOT_TOKEN}/sendPhoto", files=multipart_form_data).json()

def main(sm=None):
  time.sleep(5) # waiting for camerad to start
  notifyd_thread(sm)
  
if __name__ == "__main__":
  main()