#include <gpiod.hpp>
#include <print>
#include "functions.h"

int main() {
    std::println("=== Smart Thermostat Starting ===");
    ThermostatState state;
    watch_and_run(state);
}
