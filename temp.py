import time
import board
import adafruit_dht

# Initial the dht device, with data pin connected to:
dhtDevice = adafruit_dht.DHT11(board.D4, use_pulseio=False)

while True:
    try:
        # Print the values to the serial port
        temperature_c = dhtDevice.temperature
        print(temperature_c)
        break

    except RuntimeError as error:
        # Errors happen fairly often, DHT's are hard to read, just keep going
        time.sleep(2.0)
        continue
    except Exception as error:
        dhtDevice.exit()
        raise error
