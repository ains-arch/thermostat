#include <optional>
#include <vector>
#include <gpiod.hpp>

struct HourlyRange
{
    int hour;
    int min_temp;
    int max_temp;
};

struct ThermostatConfig
{
    std::array<HourlyRange, 24> schedule;
};

int read_temperature(); // fahrenheit

struct ThermostatState
{
    gpiod::chip chip;
    std::optional<gpiod::line_request> request;
    ThermostatConfig config;
    int current_temp;
    time_t last_temp_read;
    
    ThermostatState();
};

void watch_and_run(ThermostatState &state);
