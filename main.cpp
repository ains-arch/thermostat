#include <gpiod.hpp>
#include "functions.h"

int main() {
    std::cout << "=== Smart Thermostat Starting ===" << std::endl;
    ThermostatState state; // run the constructor, init gpio
    
    // Run main loop
    watch_and_run(state);
    return 0;
}
