#pragma once

#include <gpiod.hpp>

constexpr const char* CONFIG_DIR = "/etc/thermostat";
constexpr const char* CONFIG_PATH = "/etc/thermostat/config.json";

struct HourlyRange {
    int hour;
    int min_temp;
    int max_temp;
};

struct ThermostatConfig {
    std::array<HourlyRange, 24> schedule;
};

struct ThermostatState {
    gpiod::chip chip;
    std::optional<gpiod::line_request> request;
    ThermostatConfig config;
    int current_temp;
    std::chrono::system_clock::time_point last_temp_read;

    bool fire;

    // HA can hold a target range that overrides the scheduled hour until the
    // next hour boundary, at which point watch_and_run() clears it
    std::optional<HourlyRange> manual_override;
    int last_seen_hour = -1;

    ThermostatState();
};

int read_temperature(); // fahrenheit
void turn_off_all(ThermostatState &state);
void watch_and_run(ThermostatState &state);
void update_hvac_state(ThermostatState &state);

int current_local_hour();
// the range currently in force: the HA hold if one is set, otherwise the
// schedule entry for the current hour
HourlyRange active_range(const ThermostatState &state);

constexpr int GPIO_FAN = 26;  // green
