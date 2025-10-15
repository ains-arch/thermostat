# thermostat
Building a networked smart thermostat using raw Linux kernel GPIO interfaces (libgpiod) and C++ to control 24V HVAC systems via relay modules and JSON configuration,

or,

have you ever wanted to scp a json file to a raspberry pi in order to turn your heat on?

Featuring:
* raspberrypi
    * the 3B+ refuses to boot no matter how much i beg
    * sshing into raspberry pi os lite on the 4B it is
    * I spent a normal amount of time flashing OSs I promise
* C++
    * what the hell is cross compilation
    * toolchain? shared libraries?? g++??? CMake????
    * ~~CLion~~ language server in neovim???
* interacting with GPIO pins
    * what is a line? what is an offset? what is high and low?
    * GPIO pin libraries and how they're all bad except maybe the one in the Linux kernel (libgpiod)
    * except that one is aggressively object oriented and therefore also kind of bad
* breadboards and circuit design
    * guess and checking through the ancient art of google images
    * i made blinky!!! using a breadboard and the GPIO pins!! there's hope yet!
    * `gpioset -t500ms GPIO22=1`
* HVAC standards
    * there's four wires sticking out of this wall
    * through the power of google I have deduced their usages and determined how to not fry my raspberry pi *or* freeze to death *or* burn my house down
    * the answer is a RPI 4 channel relay and being very careful
    * don't tell my landlord
* and I'm doing all of this while going cold turkey onto a lily58 keyboard
    * [I forgot to bind the curly brace](https://github.com/ains-arch/keyboard-config)
    * help
