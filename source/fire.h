#pragma once

#include "functions.h"

bool check_fire(ThermostatState &state);

void on_fire(ThermostatState &state);

constexpr int GPIO_FIRE_BUTTON = 20; // 3 volt power
constexpr int GPIO_FIRE_LED = 21;
