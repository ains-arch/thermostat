#include <gpiod.hpp>
#include <iostream>
#include <ostream>
#include <unistd.h>

int main() {
    std::cout << "Hello, relay!" << std::endl;

    // the doc says BCM pins are
    // relay 1: GPIO 4
    // relay 2: GPIO 22
    // relay 3: GPIO 6
    // relay 4: GPIO 26
    const int RELAY_PINS[] = {4, 22, 6, 26};
    const int NUM_RELAYS = 4;

    // instantiate a new chip object by opening the gpiochip0 device file
    auto chip = gpiod::chip("/dev/gpiochip0");
    
    // object-oriented crimes
    auto config = gpiod::line_config();
    auto pinSettings = gpiod::line_settings();
    pinSettings.set_direction(gpiod::line::direction::OUTPUT); // driving
    auto request = chip.prepare_request();

    // set all pins to output 
    for (int i = 0; i < NUM_RELAYS; i++) {
        config.add_line_settings(gpiod::line::offset(RELAY_PINS[i]), pinSettings);
        request.set_line_config(config);
        request.do_request();
    }

    // for four cycles of all the relays
    for (int w = 0; w < 4; w++) {
        // turn on each relay in sequence
        for (int i = 0; i < NUM_RELAYS; i++) {
            std::cout << "relay " << i << " / " << "GPIO" << RELAY_PINS[i] << " on" << std::endl;

            pinSettings.set_output_value(gpiod::line::value::ACTIVE); // high
            config.add_line_settings(gpiod::line::offset(RELAY_PINS[i]), pinSettings);
            request.set_line_config(config);
            request.do_request();

            usleep(100000);
            
            std::cout << "relay " << i << " / " << "GPIO" << RELAY_PINS[i] << " off" << std::endl;

            pinSettings.set_output_value(gpiod::line::value::INACTIVE); // low 
            config.add_line_settings(gpiod::line::offset(RELAY_PINS[i]), pinSettings);
            request.set_line_config(config);
            request.do_request();

            usleep(100000);
        }
        usleep(100000);
    }
}
