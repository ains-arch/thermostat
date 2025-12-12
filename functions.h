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
    std::vector<HourlyRange> schedule;  // 24 entries, one per hour
};

int read_temperature(); // Farenheit

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
