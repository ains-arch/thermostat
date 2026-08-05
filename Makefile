default:
	g++ source/main.cpp source/functions.cpp source/fire.cpp source/mqtt.cpp --std=c++26 -lgpiodcxx -lmosquitto -o thermostat
