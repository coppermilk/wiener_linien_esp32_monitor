# Wiener Linien ESP32-S3 Public Transport Departure Monitor

**Brief description:** The ESP32-S3 Public Transport Departure Monitor project is a small device based on the ESP32-S3 platform. It allows you to track public transport departures in real time and receive a countdown to the next departures. The project is based on the use of data from Wiener Linien, a public transport network in Vienna.

https://github.com/coppermilk/wiener_linien_esp32_monitor/assets/25866713/15eaa64c-565c-4828-9e0d-c0c5b81a090a

## Project components:
- 1x LILYGO T-Display-S3 ESP32-S3 1.9-inch board.
- 1x USB cable with magnetic connector.
- 1x M2.5 nut 
- 1x M2.5x18 bolt.
- Glue.
- [Details printed on a 3D printer.](https://www.thingiverse.com/thing:6166463)
- [Sketch for Arduino IDE.](https://github.com/coppermilk/wiener_linien_esp32_monitor/)

## Installation
- Clone or download this repository.
- Open the Arduino IDE and install the required libraries (TFT_eSPI, ArduinoJson, HttpClient, WiFi).
- Connect your ESP32 device to your computer.
- Change the ssid and password variables in the code to match your Wi-Fi credentials.
- Important: Update the URL variable in the code to select your specific RBL (reference bus stop). You can find your RBL [here](https://till.mabe.at/rbl/?line=102&station=4909).
- Flash the code to your ESP32.

## Opportunities:
- Displaying the countdown to the next public transport departures.
- Updating information every 30 seconds, ensuring that the data is up to date.
- Automatic startup when booting the ESP32-S3 device.
- Authentic color design, providing ease of use.
- Based on the open data of the City of Vienna provided by Wiener Linien.

![Wien Liner Monitor](img/minotor colage.png)

## Description:
The ESP32-S3 Public Transport Departure Monitor project is a homemade device designed to effectively track and manage the time of public transport departures. Inspired by the needs of citizens tired of endless waiting at bus stops, the project provides accurate data on the time of the next departures on selected routes.

The main goal of the project is to reduce the waiting time at bus, tram and metro stops, allowing users to know when they need to leave home to arrive at the time of departure of transport. The project is equipped with a user-friendly interface and is automatically updated every 30 seconds to provide up-to-date information about the departure time.

## Future plans:
- [ ] Add support for multiple urban transport lines to track different routes and flexible travel planning.
- [ ] Allow users to set preferences and favorite routes.
