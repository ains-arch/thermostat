#include "fire.h"

#include <print> // println

#include "functions.h"

constexpr int GPIO_BUTTON = 3;
constexpr int GPIO_LIGHT = 2;

bool check_fire() {
    return true;
}

void on_fire(ThermostatState &state) {
    std::println("→ Forced fan ON");
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::ACTIVE);
}
