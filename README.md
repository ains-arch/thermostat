# thermostat
Building a networked smart thermostat using raw Linux kernel GPIO interfaces (libgpiod) and C++ to control 24V HVAC systems via relay modules and JSON configuration,

or,

have you ever wanted to scp a json file to a raspberry pi in order to turn your heat on?

Featuring:
* raspberrypi
    * the 3B+ refuses to boot no matter how much i beg
    * sshing into raspberry pi os lite on the 4B it is
    * i spent a normal amount of time flashing OSs i promise
* C++
    <!-- * what the hell is cross compilation -->
    <!-- * toolchain? shared libraries?? g++??? CMake???? -->
    * toolchain? shared libraries?? g++??? -->
    * ~~CLion~~ language server in neovim???
* breadboards and circuit design
    * guess and checking through the ancient art of google images
    * i made blinkie!!! using a breadboard and the GPIO pins!!
    * `gpioset -t500ms GPIO22=1`
* interacting with GPIO pins
    * what is a line? what is an offset? what is high and low?
    * GPIO pin libraries and how they're all bad except maybe the one in the Linux kernel (`libgpiod`)
    * except that one is aggressively object oriented and therefore also kind of bad
    * i turned on an LED on the breadboard - `hello.cpp` - using the c++ bindings in the library instead of the CLI
* JSON and configuration settings
    * what do people want in a thermostat
    * that's right, a json file
    ```
    {
      "mode": "heat" or "AC" or "auto",
      "target_temp": int,
      "temp_range": {"min": int, "max": int}
    }
    ```
    * `nlohmann/json` is my new best friend
* HVAC standards
    * there's four wires sticking out of this wall
    * through the power of google I have deduced their usages and determined how to not fry my raspberry pi *or* freeze to death *or* burn my house down
    * the answer is a RPI 4 channel relay and being very careful
    * don't tell my landlord
* and I'm doing all of this while going cold turkey onto a lily58 keyboard
    * [I forgot to bind the curly brace](https://github.com/ains-arch/keyboard-config)
    * help

## Dependencies
Make sure to remember the `--recursive` flag when cloning.
- libgpiod (install: `sudo apt install libgpiod-dev`)
- nlohmann/json v3.11.3 (download to `external/json.hpp` from https://github.com/nlohmann/json/releases)
