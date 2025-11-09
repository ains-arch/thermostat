#include <gpiod.hpp>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/inotify.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "external/json.hpp"
#include <optional>

using json = nlohmann::json;

// GPIO pin assignments
const int GPIO_HEAT = 22;   // Relay 1, white
const int GPIO_COOL = 6;  // Relay 2, yellow
const int GPIO_FAN = 26;    // Relay 3, green

// Global state
struct ThermostatConfig {
    // TODO: set this up so if i just give it a target or a range its in that mode automatically
    // ie i should only need the mode if i give it both
    std::string mode;  // "range" or "target"
    std::optional<int> target_temp;  // Only used in "target" mode
    std::optional<int> min_temp;     // Only used in "range" mode
    std::optional<int> max_temp;     // Only used in "range" mode
};

int read_temperature();
struct ThermostatState {
    gpiod::chip chip;
    std::optional<gpiod::line_request> request;  // Make it optional to avoid unneccessary init
    ThermostatConfig config;
    int current_temp;
    time_t last_temp_read;
    
        ThermostatState() : chip("/dev/gpiochip0"), current_temp(read_temperature()), last_temp_read(time(NULL)) {}
};

// Read temperature from sensor
// TODO: dont use python u bitch
int read_temperature() {
    FILE* pipe = popen("python3 /home/ains/dev/temp.py", "r");
    // if (!pipe) return 70;  // Default on error
    
    char buffer[128];
    fgets(buffer, 128, pipe);
    pclose(pipe);
    
    int temp = atoi(buffer);
    // if (temp == -999) return 70;  // Sensor error, use default
    
    // Convert C to F
    int temp_f = (temp * 9/5) + 32;
    std::cout << "Temperature: " << temp_f << "°F" << std::endl;
    return temp_f;
}

void init_gpio(ThermostatState &state) {
    auto config = gpiod::line_config();
    auto pinSettings = gpiod::line_settings();
    pinSettings.set_direction(gpiod::line::direction::OUTPUT);
    pinSettings.set_output_value(gpiod::line::value::INACTIVE);
    
    config.add_line_settings(gpiod::line::offset(GPIO_HEAT), pinSettings);
    config.add_line_settings(gpiod::line::offset(GPIO_COOL), pinSettings);
    config.add_line_settings(gpiod::line::offset(GPIO_FAN), pinSettings);
    
    state.request = state.chip.prepare_request()
        .set_consumer("thermostat")
        .set_line_config(config)
        .do_request();
    
    std::cout << "GPIO initialized, all relays OFF" << std::endl;
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

// Main control logic
void update_hvac_state(ThermostatState &state) {
    int temp = state.current_temp;
    
    std::cout << "\n=== HVAC Update ===" << std::endl;
    std::cout << "Current temp: " << temp << "°F" << std::endl;
    std::cout << "Mode: " << state.config.mode << std::endl;
    
    if (state.config.mode == "range") {
        // Range mode: keep between min and max
        if (!state.config.min_temp.has_value() || !state.config.max_temp.has_value()) {
            std::cerr << "Error: Range mode requires min and max temps!" << std::endl;
            turn_off_all(state);
            return;
        }
        
        int min = state.config.min_temp.value();
        int max = state.config.max_temp.value();
        
        if (temp > max) {
            turn_on_cooling(state);
        } else if (temp < min) {
            turn_on_heating(state);
        } else {
            turn_off_all(state);
        }
        
    } else if (state.config.mode == "target") {
        // Target mode: maintain specific temperature
        if (!state.config.target_temp.has_value()) {
            std::cerr << "Error: Target mode requires target_temp!" << std::endl;
            turn_off_all(state);
            return;
        }
        
        int target = state.config.target_temp.value();
        
        if (temp > target) {
            turn_on_cooling(state);
        } else if (temp < target) {
            turn_on_heating(state);
        } else {
            turn_off_all(state);
        }
        
    } else {
        std::cerr << "Unknown mode: " << state.config.mode << std::endl;
        turn_off_all(state);
    }
    std::cout << "==================\n" << std::endl;
}

// Parse config.json
void parse_config(ThermostatState &state) {
    std::cout << "Parsing config.json..." << std::endl;
    try {
        std::ifstream f("config.json");
        if (!f.is_open()) {
            std::cerr << "Error: Cannot open config.json" << std::endl;
            return;
        }
        
        json data = json::parse(f);
        state.config.mode = data["mode"];
        
        // Parse optional fields based on mode
        // TODO: make this not need the mode explicitly
        if (data.contains("target_temp")) {
            state.config.target_temp = data["target_temp"];
        } else {
            state.config.target_temp = std::nullopt;
        }
        
        if (data.contains("temp_range")) {
            state.config.min_temp = data["temp_range"]["min"];
            state.config.max_temp = data["temp_range"]["max"];
        } else {
            state.config.min_temp = std::nullopt;
            state.config.max_temp = std::nullopt;
        }
        
        std::cout << "Config loaded: mode=" << state.config.mode;
        if (state.config.target_temp.has_value()) {
            std::cout << ", target=" << state.config.target_temp.value();
        }
        if (state.config.min_temp.has_value() && state.config.max_temp.has_value()) {
            std::cout << ", range=[" << state.config.min_temp.value() 
                      << ", " << state.config.max_temp.value() << "]";
        }
        std::cout << std::endl;
        
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

int main() {
    std::cout << "=== Smart Thermostat Starting ===" << std::endl;
    
    ThermostatState state; // run the constructor
    
    // Initialize GPIO
    init_gpio(state);
    
    // Run main loop
    watch_and_run(state);
    
    return 0;
}
