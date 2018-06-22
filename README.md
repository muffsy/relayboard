# Muffsy Relay Input Selector

Muffsy Relay Input Selector firmware is a simple FreeRTOS/ESP32 application for controlling the five relays of the Muffsy Relay Input Selector board via a web interface.

## Configuration, build and flash

Wireless configuration is done at compile time, so prior to this step you need to set up a working ESP-IDF environment. Please refer to http://www.github.com/espressif/esp-idf for setup instructions.

### Configuration

make menuconfig

Wireless configuration --->
  WiFi SSID
  WiFi password

### Build

make

### Flash

make flash

### Troubleshooting

make monitor

## Web interface

The web server serves an embedded index-page which exposes the five relays as buttons on a mobile compatible web page. Styling and scripting is done with embedded CSS and JavaScript.

## Web API

The server exposes a simple HTTP GET based API:

  /0 - Turns off all relays
  /1 - Switches relay 1 on
  /2 - Switches relay 2 on
  /3 - Switches relay 3 on
  /4 - Switches relay 4 on
  /5 - Switches relay 5 on
  /? - Returns current state (0-5)

## Examples

Using curl, assuming that the ESP32 was given 192.168.0.109 by DHCP:

  curl http://192.168.0.109/1

  curl http://192.168.0.109/?

