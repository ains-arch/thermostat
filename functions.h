#include <gpiod.hpp>

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
    
    ThermostatState();
    ~ThermostatState();
};

int read_temperature(); // fahrenheit
void turn_off_all(ThermostatState &state);
void watch_and_run(ThermostatState &state);
