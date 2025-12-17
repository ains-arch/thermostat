# thermostat
![thermostat panel open to reveal wires coming out of the wall, which are connected to relays, which are connected to a raspberry pi, which is connected to a temperature sensor. the whole thing is pushpinned to the wall and dangling from the thermostat housing with safety pins.](image.jpg)

Building a networked smart thermostat using raw Linux kernel GPIO interfaces (libgpiod) and C++ to control 24V HVAC systems via relay modules and JSON configuration,

or,

have you ever wanted to scp a json file to a raspberry pi in order to turn your heat on?

Featuring:
* raspberrypi
    * the 3B+ refuses to boot no matter how much i beg
    * sshing into raspberry pi os lite on the 4B it is
    * i spent a normal amount of time flashing OSs i promise
* C++
    * toolchain! shared libraries!! g++!!!
    * language server in neovim!!
* breadboards and circuit design
    * guess and checking through the ancient art of google images
    * i made blinkie!!! using a breadboard and the GPIO pins!!
    * `gpioset -t500ms GPIO22=1`
* interacting with GPIO pins
    * what is a line? what is an offset? what is high and low?
    * GPIO pin libraries and how they're all bad except maybe the one in the Linux kernel (`libgpiod`)
    * i turned on an LED on the breadboard - `hello.cpp` - using the c++ bindings in the library instead of the CLI. progress!
* JSON and configuration settings
    * what do people want in a thermostat
    * that's right, a json file
    ```
    {
        "schedule": [
            {"hour": 0, "min": 65, "max": 72},
            {"hour": 1, "min": 65, "max": 72},
            ...
    }
    ```
    * `nlohmann/json` is my new best friend
* watching the config file with inotify
    * trying to understand this ugly c style boilerplate
    * it works, which is what matters. it'll update the state when the config changes now!
* HVAC standards
    * there's four wires sticking out of this wall
    * through the power of google I have deduced their usages and determined how to not fry my raspberry pi *or* freeze to death *or* burn my house down
    * the answer is a RPI 4 channel relay and being very careful
    * don't tell my landlord
* a relay board
    * it's not so bad. i just have to keep track of four pins instead of one
    * updated blinkie to cycle through all of the relays and turn them on and off (they helpfully come with an LED on the board for each) - `hello_relay.cpp`
    * hooked it up to a breadboard and LED circuits anyway
* two different temperature sensors
    * initally used a DHT11 from another project
    * no driver with C++ keybindings, so I'm calling out to Python like a chud - `temp.py`
    * didn't come with a pinout or even a name, so figuring out how to wire it up was a bit of a hassle
    * the sensor is flakey, so I have to wrap my library code in a while true try except block. luckily it's Python so I can pretend it's fine
    * aaaand I was careless while adjusting the wiring and fried it
    * try again with a DHT22 - more decimals of accuracy!
    * luckily the library is the same so `temp.py` only needs a two character update
* actual programming
    * need an algorithm to take a temperature reading and make the needed changes to the state of the thermostat
    * also made sure that it runs with a five minute hysteresis so I don't hurt the HVAC by cycling too quickly
    * wrapped the verbose object oriented GPIO init in a constructor and now my state changes aren't thirty lines!
* figuring out how to background it
    * lets try `setsid`
    * actually, I should run this on boot using `systemd`
    * that was less difficult than I expected
* refactoring the code base so it's spread across `functions.h`, `functions.cpp`, and `main.cpp` instead of having one gigantic file
    * this is annoying but reveals some bugs and is admittedly much cleaner. standards win again
    * also, now I can grab functions my main control flow and use them for `close_relays.cpp`
* a one line `Makefile`
    * got sick of ctrl-r finding the same g++ command, so I made one. now we're cooking
* c++23!
    * I misread the requirements on the GPIO library
    * time to add a `.clangd` so my language server stops complaining, and change all these `std::cout`s into `println`s.
    * crazy that it took untill 2023 for c++ to invent the idea of a print statement.
    * also, `std::chrono` is so much prettier than c time. time management is hard! thank god for the STL.
    * `std::ifstream`? of course. wouldn't want to `fgets` to `pclose` a file and have a memory leak or other certified C moments. thank god for the STL.
    * realized I can use an array of size 24 instead of a vector and a comment that it should be 24 hours. I love types.
* attempting error handling by destructor
    * this was a bad idea
    * don't do manual memory management, kids!
    * why am I wrapping the entire program in a try catch. god save me
* and I did all of this while going cold turkey on a lily58 keyboard
    * [I forgot to bind the curly brace](https://github.com/ains-arch/keyboard-config)

It works! My house is the correct temperature. Mission accomplished. Now, to put a screen on it...

## Dependencies
- libgpiod
- nlohmann/json
- requirements.txt
