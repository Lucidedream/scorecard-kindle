#!/bin/sh
# Name: Scorecard
# Author: Noah
# DontUseFBInk

APP_DIR="/mnt/us/scorecard"
LOG_FILE="$APP_DIR/launcher.log"

mkdir -p "$APP_DIR"
date > "$LOG_FILE"

if [ ! -x "$APP_DIR/scorecard" ]; then
  echo "Scorecard binary is missing." >> "$LOG_FILE"
  /mnt/us/libkh/bin/fbink -c -m -S 5 "Scorecard binary is missing"
  sleep 5
  exit 1
fi

"$APP_DIR/scorecard" >> "$LOG_FILE" 2>&1
STATUS=$?

if [ "$STATUS" -ne 0 ]; then
  /mnt/us/libkh/bin/fbink -c -m -S 5 "Scorecard stopped with error $STATUS"
  sleep 6
fi

exit "$STATUS"
