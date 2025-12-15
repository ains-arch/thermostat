#!/bin/bash
cd /home/ains/dev
source env/bin/activate
exec stdbuf -oL -eL ./thermostat  # Force line buffering
