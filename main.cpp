#include <gpiod.hpp>
#include <print>
#include "functions.h"

int main() {
    std::println("=== Smart Thermostat Starting ===");
    try {
        ThermostatState state;
        watch_and_run(state);
    } catch (const std::exception& e) {
        std::println(stderr, "Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}
