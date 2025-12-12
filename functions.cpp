#include "functions.h"
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctime>
#include <sys/inotify.h>
#include <iostream>
#include <unistd.h>

// GPIO pin assignments
const int GPIO_HEAT = 22; // Relay 1, white
const int GPIO_COOL = 6;  // Relay 2, yellow
const int GPIO_FAN = 26;  // Relay 3, green

ThermostatState::ThermostatState()
    : chip("/dev/gpiochip0")
    , current_temp(read_temperature())
    , last_temp_read(time(NULL))
{
    auto config = gpiod::line_config();
    auto pinSettings = gpiod::line_settings();
    pinSettings.set_direction(gpiod::line::direction::OUTPUT);
    pinSettings.set_output_value(gpiod::line::value::INACTIVE);

    config.add_line_settings(gpiod::line::offset(GPIO_HEAT), pinSettings);
    config.add_line_settings(gpiod::line::offset(GPIO_COOL), pinSettings);
    config.add_line_settings(gpiod::line::offset(GPIO_FAN), pinSettings);

    request = chip.prepare_request()
                  .set_consumer("thermostat")
                  .set_line_config(config)
                  .do_request();

    std::cout << "GPIO initialized, all relays OFF" << std::endl;
}

// Read temperature from sensor
// TODO: dont use python u bitch
int read_temperature() {
    FILE* pipe = popen("python3 /home/ains/dev/temp.py", "r");
    // if (!pipe) return 70;  // Default on error
    
    char buffer[128];
    fgets(buffer, 128, pipe);
    pclose(pipe);
    
    int temp = atoi(buffer);
    
    // Convert C to F
    int temp_f = (temp * 9/5) + 32;

    // TODO: handle unplugged sensor better
    if (temp_f < 50) throw std::runtime_error("The temperature is suspiciously low. You should go check the sensor.");

    std::cout << "Temperature: " << temp_f << "°F" << std::endl;
    return temp_f;
}

void turn_on_cooling(ThermostatState &state) {
    std::cout << "→ Cooling mode: AC ON, Fan ON, Heat OFF" << std::endl;
    state.request.value().set_value(GPIO_HEAT, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_COOL, gpiod::line::value::ACTIVE);
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::ACTIVE);
}

void turn_on_heating(ThermostatState &state) {
    std::cout << "→ Heating mode: Heat ON, Fan ON, AC OFF" << std::endl;
    state.request.value().set_value(GPIO_COOL, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_HEAT, gpiod::line::value::ACTIVE);
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::ACTIVE);
}

void turn_off_all(ThermostatState &state) {
    std::cout << "→ All systems OFF" << std::endl;
    state.request.value().set_value(GPIO_HEAT, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_COOL, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::INACTIVE);
}

void update_hvac_state(ThermostatState &state) {
    int temp = state.current_temp;
    
    // Get current hour (0-23)
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int current_hour = tm_info->tm_hour;
    
    // Find the schedule entry for this hour
    HourlyRange *current_range = nullptr;
    for (auto &range : state.config.schedule) {
        if (range.hour == current_hour) {
            current_range = &range;
            break;
        }
    }
    
    if (!current_range) {
        std::cerr << "Error: No schedule entry for hour " << current_hour << std::endl;
        turn_off_all(state);
        return;
    }
    
    std::cout << "\n=== HVAC Update ===" << std::endl;
    std::cout << "Current time: " << current_hour << ":00" << std::endl;
    std::cout << "Current temp: " << temp << "°F" << std::endl;
    std::cout << "Target range: [" << current_range->min_temp 
              << ", " << current_range->max_temp << "]" << std::endl;
    
    // Apply range logic
    if (temp > current_range->max_temp) {
        turn_on_cooling(state);
    } else if (temp < current_range->min_temp) {
        turn_on_heating(state);
    } else {
        turn_off_all(state);
    }
    
    std::cout << "==================\n" << std::endl;
}

using json = nlohmann::json;
void parse_config(ThermostatState &state) {
    std::cout << "Parsing config.json..." << std::endl;
    try {
        std::ifstream f("config.json");
        if (!f.is_open()) {
            std::cerr << "Error: Cannot open config.json" << std::endl;
            return;
        }
        
        json data = json::parse(f);
        state.config.schedule.clear();
        
        for (const auto& entry : data["schedule"]) {
            HourlyRange range;
            range.hour = entry["hour"];
            range.min_temp = entry["min"];
            range.max_temp = entry["max"];
            state.config.schedule.push_back(range);
        }
        
        std::cout << "Config loaded: " << state.config.schedule.size() 
                  << " hourly entries" << std::endl;
        
    } catch (const std::exception &e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
    }
}

// Watch config.json for changes
void watch_and_run(ThermostatState &state) {
    int fd, wd;
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t size;
    time_t last_parse_time = 0;
    
    fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }
    
    wd = inotify_add_watch(fd, ".", IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    if (wd == -1) {
        fprintf(stderr, "Cannot watch directory: %s\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    std::cout << "Watching config.json for changes..." << std::endl;
    
    // Initial setup
    parse_config(state);
    update_hvac_state(state);
    
    while (1) {
        // Check if it's time to read temperature (every 5 minutes)
        // TODO: hysteresis should either be defined explicitly at the top of the file or in the config
        time_t now = time(NULL);
        if (now - state.last_temp_read >= 300) {  // 300 seconds = 5 minutes
            state.current_temp = read_temperature();
            state.last_temp_read = now;
            // TODO: should only update if the temp has changed
            update_hvac_state(state);
        };
        
        // Check for config file changes
        size = read(fd, buf, sizeof(buf));
        
        if (size == -1 && errno != EAGAIN) {
            perror("read");
            break;
        }
        
        if (size > 0) {
            bool should_parse = false;
            
            for (char *ptr = buf; ptr < buf + size;
                 ptr += sizeof(struct inotify_event) + event->len) {
                
                event = (const struct inotify_event *)ptr;
                
                if (event->len && strcmp(event->name, "config.json") == 0) {
                    if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE)) {
                        should_parse = true;
                    }
                }
            }
            
            if (should_parse) {
                time_t now = time(NULL);
                if (now - last_parse_time >= 1) {
                    parse_config(state);
                    update_hvac_state(state);
                    last_parse_time = now;
                }
            }
        }
        
        usleep(100000);  // Sleep 100ms between checks
    }
    
    close(fd);
}
