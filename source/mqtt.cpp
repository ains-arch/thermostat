#include "mqtt.h"
#include <cstdlib>
#include <deque>
#include <fstream>
#include <mosquitto.h>
#include <mutex>
#include <print>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// broker host/port/username come from EnvironmentFile=/etc/thermostat/mqtt.env
// (see thermostat.service). The password lives in its own file instead, read
// once here and handed to libmosquitto in-process via mosquitto_username_pw_set()
// -- it's never put on a command line or in a child process's environment.
constexpr const char* MQTT_PW_FILE = "/etc/thermostat/mqtt_password";

constexpr const char* DISCOVERY_TOPIC = "homeassistant/climate/thermostat/config";
constexpr const char* AVAILABILITY_TOPIC = "thermostat/availability";
constexpr const char* MODE_STATE_TOPIC = "thermostat/mode/state";
constexpr const char* TEMP_STATE_TOPIC = "thermostat/temperature/state";
constexpr const char* LOW_STATE_TOPIC = "thermostat/target/low/state";
constexpr const char* HIGH_STATE_TOPIC = "thermostat/target/high/state";
constexpr const char* LOW_CMD_TOPIC = "thermostat/target/low/set";
constexpr const char* HIGH_CMD_TOPIC = "thermostat/target/high/set";
constexpr const char* ACTION_TOPIC = "thermostat/action/state";
constexpr const char* CMD_TOPIC_FILTER = "thermostat/target/+/set";

namespace {

mosquitto *client = nullptr;

struct Command {
    std::string topic;
    int value;
};
std::mutex pending_mutex;
std::deque<Command> pending_commands;

std::string env_or(const char *name, const std::string &fallback) {
    const char *value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

std::string read_password() {
    std::ifstream f(MQTT_PW_FILE);
    std::string pw;
    std::getline(f, pw);
    return pw;
}

void publish(const char *topic, const std::string &payload, int qos, bool retain) {
    if (!client) return;
    int rc = mosquitto_publish(client, nullptr, topic, static_cast<int>(payload.size()),
                                payload.data(), qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::println("MQTT publish to {} failed: {}", topic, mosquitto_strerror(rc));
    }
}

// runs on libmosquitto's background network thread -- just queue the
// command, don't touch relay/state here
void on_message(mosquitto *, void *, const mosquitto_message *msg) {
    if (!msg->payload || msg->payloadlen <= 0) return;
    std::string topic(msg->topic);
    std::string payload_str(static_cast<char *>(msg->payload), msg->payloadlen);

    try {
        int value = std::stoi(payload_str);
        std::lock_guard<std::mutex> lock(pending_mutex);
        pending_commands.push_back({std::move(topic), value});
    } catch (const std::exception &) {
        std::println("Ignoring malformed MQTT command on {}: {}", topic, payload_str);
    }
}

} // namespace

void mqtt_connect() {
    mosquitto_lib_init();
    client = mosquitto_new("thermostat", /*clean_session=*/true, nullptr);

    const std::string username = env_or("MQTT_USERNAME", "thermostat");
    const std::string password = read_password();
    mosquitto_username_pw_set(client, username.c_str(), password.c_str());

    mosquitto_will_set(client, AVAILABILITY_TOPIC, 7, "offline", /*qos=*/1, /*retain=*/true);
    mosquitto_message_callback_set(client, on_message);

    const std::string host = env_or("MQTT_HOST", "homeassistant.local");
    const int port = std::stoi(env_or("MQTT_PORT", "1883"));

    // async + loop_start retries in the background if the broker is
    // unreachable at startup, instead of blocking or giving up
    int rc = mosquitto_connect_async(client, host.c_str(), port, /*keepalive=*/60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::println("MQTT: initial connect to {}:{} failed: {}", host, port, mosquitto_strerror(rc));
    }
    mosquitto_loop_start(client);
}

void mqtt_start() {
    mosquitto_subscribe(client, nullptr, CMD_TOPIC_FILTER, /*qos=*/1);

    json payload = {
        {"name", "Thermostat"},
        {"unique_id", "thermostat_pi"},
        {"device", {
            {"identifiers", json::array({"thermostat_pi"})},
            {"name", "Smart Thermostat"},
            {"manufacturer", "DIY"}
        }},
        {"modes", json::array({"heat_cool"})},
        {"mode_state_topic", MODE_STATE_TOPIC},
        {"temperature_unit", "F"},
        {"precision", 1.0},
        {"min_temp", 50},
        {"max_temp", 90},
        {"current_temperature_topic", TEMP_STATE_TOPIC},
        {"temperature_low_command_topic", LOW_CMD_TOPIC},
        {"temperature_low_state_topic", LOW_STATE_TOPIC},
        {"temperature_high_command_topic", HIGH_CMD_TOPIC},
        {"temperature_high_state_topic", HIGH_STATE_TOPIC},
        {"action_topic", ACTION_TOPIC},
        {"availability_topic", AVAILABILITY_TOPIC},
    };

    publish(DISCOVERY_TOPIC, payload.dump(), /*qos=*/1, /*retain=*/true);
    // only mode we support; no command topic wired to it, so it's not user-settable
    publish(MODE_STATE_TOPIC, "heat_cool", /*qos=*/1, /*retain=*/true);
    publish(AVAILABILITY_TOPIC, "online", /*qos=*/1, /*retain=*/true);

    std::println("MQTT: discovery published, subscribed for HA commands");
}

void mqtt_publish_state(const ThermostatState &state, const HourlyRange &active,
                         const std::string &action) {
    publish(TEMP_STATE_TOPIC, std::to_string(state.current_temp), /*qos=*/0, /*retain=*/false);
    publish(LOW_STATE_TOPIC, std::to_string(active.min_temp), /*qos=*/0, /*retain=*/false);
    publish(HIGH_STATE_TOPIC, std::to_string(active.max_temp), /*qos=*/0, /*retain=*/false);
    publish(ACTION_TOPIC, action, /*qos=*/0, /*retain=*/false);
}

void mqtt_apply_pending_commands(ThermostatState &state) {
    std::deque<Command> commands;
    {
        std::lock_guard<std::mutex> lock(pending_mutex);
        std::swap(commands, pending_commands);
    }

    for (const auto &cmd : commands) {
        HourlyRange range = active_range(state);
        if (cmd.topic == LOW_CMD_TOPIC) {
            range.min_temp = cmd.value;
        } else if (cmd.topic == HIGH_CMD_TOPIC) {
            range.max_temp = cmd.value;
        } else {
            continue;
        }
        range.hour = current_local_hour();
        state.manual_override = range;

        std::println("HA hold set: [{}, {}] until next hour", range.min_temp, range.max_temp);
        update_hvac_state(state);
    }
}

void mqtt_disconnect() {
    if (!client) return;
    mosquitto_loop_stop(client, /*force=*/true);
    mosquitto_disconnect(client);
    mosquitto_destroy(client);
    mosquitto_lib_cleanup();
    client = nullptr;
}
