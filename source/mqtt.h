#pragma once

#include "functions.h"
#include <string>

// connect to the broker (async, auto-retrying) and start libmosquitto's
// background network thread. Call once at startup.
void mqtt_connect();

// subscribe to HA command topics and publish the (retained) discovery config
// and availability. Call once after mqtt_connect().
void mqtt_start();

// publish current temperature/target range/action to their state topics
void mqtt_publish_state(const ThermostatState &state, const HourlyRange &active_range,
                         const std::string &action);

// apply any HA target-range commands received since the last call. Messages
// arrive on libmosquitto's background thread and are queued; this drains
// that queue and runs update_hvac_state() on the calling (main) thread, so
// relay control stays single-threaded.
void mqtt_apply_pending_commands(ThermostatState &state);

void mqtt_disconnect();
