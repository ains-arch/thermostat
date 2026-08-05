# Smart Thermostat

<img src="image.jpg" alt="thermostat panel open to reveal wires coming out of the wall, which are connected to relays, which are connected to a raspberry pi, which is connected to a temperature sensor. the whole thing is pushpinned to the wall and dangling from the thermostat housing with safety pins" width="20%">

A DIY networked smart thermostat that controls 24V HVAC systems using raw Linux kernel GPIO interfaces (libgpiod), C++23, and JSON configuration files.

**Or: have you ever wanted to `scp` a JSON file to a Raspberry Pi in order to turn your heat on?**

## Features

- **Raspberry Pi 4B** running Raspberry Pi OS Lite
- **C++23** with `std::println`, `std::chrono`, and `std::ifstream`
- **Direct GPIO control** via libgpiod (Linux kernel interface)
- **4-channel relay module** for safe 24V HVAC control
- **DHT22 temperature sensor** for accurate readings
- **JSON-based scheduling** with 24-hour granular control
- **Hot-reload configuration** using inotify file watching
- **Systemd integration** for automatic startup and monitoring
- **5-minute hysteresis** to prevent HVAC short-cycling
- **Home Assistant integration** via MQTT discovery (climate entity with live temp/target range/action, and availability)

## Hardware Setup

### Components
- Raspberry Pi 4B
- 4-channel relay module (5V trigger)
- DHT22 temperature/humidity sensor
- 24V HVAC system (standard R, G, Y, W wiring)
- Breadboard and jumper wires
- Safety pins and thumbtacks (for... mounting solutions)

### GPIO Pin Assignments
- GPIO 22 (white) - Heating relay
- GPIO 6 (yellow) - Cooling relay  
- GPIO 26 (green) - Fan relay
- GPIO 4 - DHT22 data pin

### Wiring
The relay module acts as a safe interface between the Pi's 3.3V GPIO and the HVAC's 24V system.
Each relay channel switches one HVAC wire (R/common connects to all).

## Software Architecture

### Core Components

**`main.cpp`**: Entry point, initializes thermostat state and starts main control loop

**`functions.h/cpp`**: Core thermostat logic
- GPIO initialization and relay control
- Temperature reading (via Python bridge to Adafruit DHT library)
- HVAC state management based on temperature ranges (schedule, or an active HA hold)
- JSON config parsing and hot-reloading
- inotify-based file watching

**`mqtt.h/cpp`**: Home Assistant integration
- Links against `libmosquitto` directly (async connect, background network thread via
  `mosquitto_loop_start`) rather than shelling out — the mosquitto-clients CLI tools on
  Debian don't support reading the password from a file, only `-P` on the command line,
  which would leak it in `ps aux` for the life of the process
- Publishes an MQTT Discovery `climate` entity config (retained) on startup
- Publishes current temp/target range/action on every HVAC update
- Subscribes to HA command topics; incoming messages are queued off the mosquitto thread
  and drained on the main loop, so relay control stays single-threaded
- Sets a Will (LWT) so HA shows the device as unavailable if the service dies

**`temp.py`**: Temperature sensor interface
- Reads DHT22 sensor via Adafruit library
- Retries on transient sensor errors (DHT sensors are finicky)
- Returns temperature in Celsius to stdout

### Configuration

The schedule lives in `/etc/thermostat/config.json` (not in the repo/working directory),
with 24 hourly temperature ranges:

```json
{
  "schedule": [
    {"hour": 0, "min": 65, "max": 72},
    {"hour": 1, "min": 65, "max": 72},
    {"hour": 2, "min": 65, "max": 72},
    ...
    {"hour": 23, "min": 68, "max": 75}
  ]
}
```

The thermostat will:
- Turn on **heating** if temp < min
- Turn on **cooling** if temp > max
- Turn off everything if min ≤ temp ≤ max

Changes to `/etc/thermostat/config.json` are automatically detected and applied immediately,
unless a Home Assistant hold (see below) is currently active for the hour.

## Home Assistant Integration

The thermostat publishes itself to Home Assistant as an MQTT Discovery `climate` entity
(`heat_cool` mode, using HA's low/high target range) — current temperature, target range,
heating/cooling/idle action, and availability all show up automatically, no HA-side YAML
required beyond having the MQTT integration configured against your broker.

Dragging the target range on the HA thermostat card applies immediately and holds until the
top of the next hour, then reverts to whatever `/etc/thermostat/config.json` specifies for
that hour — a temporary hold, like a normal thermostat, not a permanent schedule edit.

### Setup

**System packages** (`libmosquitto-dev` for the build; `mosquitto-clients` is optional,
handy for manually poking topics with `mosquitto_sub`/`mosquitto_pub` while debugging):
```bash
sudo apt install libmosquitto-dev mosquitto-clients
```

**Broker connection** — `/etc/thermostat/mqtt.env` (loaded via `EnvironmentFile=` in the
systemd unit):
```
MQTT_HOST=homeassistant.local
MQTT_PORT=1883
MQTT_USERNAME=thermostat
```

**Broker password** — `/etc/thermostat/mqtt_password` (mode 600, contains only the
password, no trailing content besides the one line). Read once at startup and handed to
libmosquitto in-process; never appears on a command line or in `ps`.

## Building

### Dependencies

**System packages:**
```bash
sudo apt update
sudo apt install libgpiod-dev nlohmann-json3-dev libmosquitto-dev g++ make python3-pip mosquitto-clients
```

**Python packages:**
```bash
pip3 install -r requirements.txt
# Contents: adafruit-circuitpython-dht
```

### Compilation

```bash
make
```

Or manually:
```bash
g++ source/main.cpp source/functions.cpp source/fire.cpp source/mqtt.cpp --std=c++26 -lgpiodcxx -lmosquitto -o thermostat
```

## Running

### Manual Testing
```bash
./thermostat
# Ctrl+C to stop
# If relays stay on: ./close_relays
```

### Production (Systemd Service)

1. Copy `thermostat.service` to `/etc/systemd/system/thermostat.service` (it loads
   `/etc/thermostat/mqtt.env` via `EnvironmentFile=` — see Home Assistant Integration above):


2. Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable thermostat
sudo systemctl start thermostat
```

3. Monitor:

```bash
sudo systemctl status thermostat
journalctl -u thermostat -f
tail -f log.txt
```

## Project Journey

- **Hardware interfacing**: From blinkie to controlling my heater 
- **C++**: Moved from C-style code to C++23 features (`std::println`, `std::chrono`, `std::ranges`)
- **Linux kernel APIs**: Direct GPIO control via libgpiod, inotify file watching, trying and thinking better of writing bare syscalls
- **Circuit design**: Relay modules, breadboards, voltage isolation
- **Error handling**: Destructors, exception safety, and why manual memory management is a mistake
- **Build tooling**: Makefiles, language servers, clangd configuration

### Lessons Learned

- My Raspberry Pi 3B+ doesn't boot
- libgpiod is a maze of OOP and undefined jargon
- DHT sensors are unreliable; it's not my fault that sometimes it takes five times as long to get a reading
- It is never the last time I'm going to rewire the screw terminal
- I still can't believe it took until 2023 for C++ to invent the idea of a print statement
- Destructors are great except for when you need them to work
- JSON reigns supreme
- Systemd is just text files, and that's beautiful
- Safety pins are a valid mounting solution. Duct tape is not.

### Known Issues

- Relays will stay in the state they were in if the program stops (investigating watchdog pattern)
- Temperature sensor driver is in Python (might write my own so I can get C++ bindings)
- Relay wiring should be reorganized to use channels 1-3 and take wires directly from the wall rather than through a screw terminal

### Future Improvements

- [ ] Write native C++ driver for DHT22
- [ ] Implement watchdog pattern for safe shutdown
- [ ] Add web interface for remote control
- [ ] Add screen to display state and temperature
- [ ] Rewrite project for a Pi Pico to reduce space and computation
- [ ] Actually mount it properly (sorry, safety pins)

## Development Notes

Built while learning a Lily58 split keyboard. [I still haven't bound the curly brace](https://github.com/ains-arch/keyboard-config).

## License

MIT - Use at your own risk. Not responsible for freezing, overheating, or angry landlords.
Also, this project involves controlling residential HVAC systems, which are slightly more complicated than an LED.
Don't do anything stupid.
