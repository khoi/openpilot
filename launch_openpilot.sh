#!/usr/bin/bash

export PASSIVE="0"
export MAPBOX_TOKEN="$(echo 'cGsuZXlKMUlqb2lhMmh2YVhKaFkyeGxJaXdpWVNJNkltTnNOMkUyTTJ0cE5EQTRjM2t6ZG13NE9UVnVaMlZ0Y1hRaWZRLmpNWl96cDFFdjRTRmI2WXpfY2F3UGcK' | openssl enc -d -base64)"
exec ./launch_chffrplus.sh
