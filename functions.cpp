#include "functions.h"
#include <nlohmann/json.hpp>
#include <sys/inotify.h> // file watching, linux kernel
#include <print> // println
#include <fstream> // opening file

// GPIO pin assignments
// TODO: rewire this so im actually using relays 1-3
// TODO: buy standalone relays instead of a GPIO hat to save space
constexpr int GPIO_HEAT = 22; // white
constexpr int GPIO_COOL = 6;  // yellow
constexpr int GPIO_FAN = 26;  // green

ThermostatState::ThermostatState() // type::ConstructorFunction()
    : chip("/dev/gpiochip0") // set the location of the chip
    , current_temp(read_temperature()) // read the current temperature at construction
    , last_temp_read(std::chrono::system_clock::now())
{
    // object-oriented library crimes to init GPIO to sane state
    auto config = gpiod::line_config();
    auto pinSettings = gpiod::line_settings();
    pinSettings.set_direction(gpiod::line::direction::OUTPUT); // driving
    pinSettings.set_output_value(gpiod::line::value::INACTIVE); // low/off

    config.add_line_settings(gpiod::line::offset(GPIO_HEAT), pinSettings);
    config.add_line_settings(gpiod::line::offset(GPIO_COOL), pinSettings);
    config.add_line_settings(gpiod::line::offset(GPIO_FAN), pinSettings);

    request = chip.prepare_request()
                  .set_consumer("thermostat")
                  .set_line_config(config)
                  .do_request();

    std::println("GPIO initialized, all relays OFF");
}

ThermostatState::~ThermostatState() { //type::DestructorFunction()
    // emergency shutdown: turn off all relays on destruction
    turn_off_all(*this);
}

int read_temperature() {
    // get the temperature by running a python file because drivers are hard
    // TODO: write a driver for the temperature sensor
    FILE* pipe = popen("python3 /home/ains/dev/temp.py", "r");
    char buffer[128];
    fgets(buffer, 128, pipe);
    pclose(pipe);
    int temp_c = atoi(buffer);
    
    int temp_f = (temp_c * 9.0/5.0) + 32;

    // TODO: handle unplugged sensor directly
    if (temp_f < 50) throw std::runtime_error("The temperature is suspiciously low. You should go check the sensor.");

    std::println("Temperature: {} °F", temp_f);
    return temp_f;
}

void turn_on_cooling(ThermostatState &state) {
    std::println("→ Cooling mode: AC ON, Fan ON, Heat OFF");
    state.request.value().set_value(GPIO_HEAT, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_COOL, gpiod::line::value::ACTIVE);
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::ACTIVE);
}

void turn_on_heating(ThermostatState &state) {
    std::println("→ Heating mode: Heat ON, Fan ON, AC OFF");
    state.request.value().set_value(GPIO_COOL, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_HEAT, gpiod::line::value::ACTIVE);
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::ACTIVE);
}

void turn_off_all(ThermostatState &state) {
    std::println("→ All systems OFF");
    state.request.value().set_value(GPIO_HEAT, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_COOL, gpiod::line::value::INACTIVE);
    state.request.value().set_value(GPIO_FAN, gpiod::line::value::INACTIVE);
}

void update_hvac_state(ThermostatState &state) {
    int const temp = state.current_temp;
    
    using namespace std::chrono;

    const auto now_utc = system_clock::now(); // UTC
    const auto now = zoned_time{current_zone(), now_utc}.get_local_time();
    const auto today = floor<days>(now);
    const uint32_t current_hour = duration_cast<hours>(now - today).count();

    // find the schedule entry for this hour
    const auto current_range = std::ranges::find_if(
        state.config.schedule, [current_hour](const HourlyRange& range) {
            return range.hour == current_hour;
        });
    
    std::println("\n=== HVAC Update ===");
    std::println("Current time: {}:00", current_hour);
    std::println("Current temp: {}°F", temp);
    std::println("Target range: [{}, {}]", current_range->min_temp, current_range->max_temp);
    
    // apply range logic
    if (temp > current_range->max_temp) {
        turn_on_cooling(state);
    } else if (temp < current_range->min_temp) {
        turn_on_heating(state);
    } else {
        turn_off_all(state);
    }
    
    std::println("==================\n");
}

using json = nlohmann::json;
std::array<HourlyRange, 24> parse_config(ThermostatState &state)
{
    std::println("Parsing config.json...");
    std::ifstream file("config.json");
    json data = json::parse(file);

    // empty array of hours
    std::array<HourlyRange, 24> schedule;

    // fill array with data from json parse
    for (const auto& [i, entry] : std::views::enumerate(data["schedule"]))
    {
        // build range object
        HourlyRange range;
        range.hour = entry["hour"];
        range.min_temp = entry["min"];
        range.max_temp = entry["max"];

        // put the range object in the relevant hour in the array
        schedule[i] = range;
    }

    // apply config change
    state.config.schedule = schedule;

    std::println("Config loaded: {} hourly entries", state.config.schedule.size());

    return schedule;
}

// watch config.json for changes
// TODO: const and constexpr as much as possible
// TODO: only pass the needed parts of the state
// TODO: make function inputs and outputs explicit
void watch_and_run(ThermostatState &state) {
    // very ugly boilerplate from inotify
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

    std::println("Watching config.json for changes...");
    
    parse_config(state); // initial parse
    update_hvac_state(state); // change relay state based on parse and current temp from state
    
    // TODO: consider using for (auto element : range) instead of while
    while (1) {
        using namespace std::chrono;
        // check if it's time to read temperature (every 5 minutes)
        // TODO: hysteresis rule should either be defined explicitly at the top of the file or in the config
        const auto now = system_clock::now();

        if (duration_cast<seconds>(now - state.last_temp_read).count() >= 300)
        {  // 300 seconds = 5 minutes
            state.current_temp = read_temperature();
            state.last_temp_read = now;
            // TODO: should only update if the temp has changed
            update_hvac_state(state);
        };
        
        // more inotify boilerplate
        // check for config file changes
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
        usleep(100000);  // sleep 100ms between checks
    }
    // TODO: consider std::ofstream, if it's possible w/ Linux kernel interfaces)
    close(fd);
}
