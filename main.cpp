#include <gpiod.hpp>
#include <print>
#include "functions.h"

int main() {
    std::println("=== Smart Thermostat Starting ===");
    ThermostatState state; // run the constructor, init gpio
    watch_and_run(state); // run the main loop
    return 0;
}
