#include "fire.h"

#include <print> // println

#include "functions.h"

void on_fire(ThermostatState &state) {
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::ACTIVE);
    std::println("→ Forced fan ON");
}
