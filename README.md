[![Build Arduino Sketch](https://github.com/coppermilk/wiener_linien_esp32_monitor/actions/workflows/main.yml/badge.svg)](https://github.com/coppermilk/wiener_linien_esp32_monitor/actions/workflows/main.yml)
# Wiener Linien ESP32-S3 Public Transport Departure Monitor

**Brief description:** The ESP32-S3 Public Transport Departure Monitor project is a small device based on the ESP32-S3 platform. It allows you to track public transport departures in real time and receive a countdown to the next departures. The project is based on the use of open data from the City of Vienna provided by Wiener Linien. (Data source: City of Vienna - https://data.wien.gv.at)

https://github.com/coppermilk/wiener_linien_esp32_monitor/assets/25866713/15eaa64c-565c-4828-9e0d-c0c5b81a090a

## Project components:
- 1x LILYGO T-Display-S3 ESP32-S3 1.9-inch board. Without pins! [[AliExpress]](https://de.aliexpress.com/item/1005004756588137.html?af=208&cv=0&cn=42s1eefrilp8reljxqmxkbskcmmy336v&dp=v5_42s1eefrilp8reljxqmxkbskcmmy336v&af=208&cv=0&cn=42s1eefrilp8reljxqmxkbskcmmy336v&dp=v5_42s1eefrilp8reljxqmxkbskcmmy336v&utm_source=epn&utm_medium=cpa&utm_campaign=208&utm_content=0&product_id=1005004756588137&afref=https%3A%2F%2Fbackit.me&aff_fcid=c8c377a14da845c7838fa62c18307494-1695404439822-03623-_9G57Xi&aff_fsk=_9G57Xi&aff_platform=portals-hotproduct&sk=_9G57Xi&aff_trace_key=c8c377a14da845c7838fa62c18307494-1695404439822-03623-_9G57Xi&terminal_id=4559c76026b7443083747fca08307839&afSmartRedirect=y)
- 1x USB cable with magnetic connector. (Little cut borders.) [[AliExpress]](https://de.aliexpress.com/item/4001058884201.html?af=208&cv=0&cn=42s1eeqlg1t78izxx05nefdc9mbc0xmk&dp=v5_42s1eeqlg1t78izxx05nefdc9mbc0xmk&af=208&cv=0&cn=42s1eeqlg1t78izxx05nefdc9mbc0xmk&dp=v5_42s1eeqlg1t78izxx05nefdc9mbc0xmk&utm_source=epn&utm_medium=cpa&utm_campaign=208&utm_content=0&product_id=4001058884201&afref=https%3A%2F%2Fbackit.me&aff_fcid=e96e9205401748eb9aa8edd56caa1a1b-1695404829888-03654-_9G57Xi&aff_fsk=_9G57Xi&aff_platform=portals-hotproduct&sk=_9G57Xi&aff_trace_key=e96e9205401748eb9aa8edd56caa1a1b-1695404829888-03654-_9G57Xi&terminal_id=4559c76026b7443083747fca08307839&afSmartRedirect=y)
- 2x M2.5 nut [[AliExpress]](https://de.aliexpress.com/item/32868834536.html?af=208&cv=0&cn=42s1eekk33drkxs90p3wbwdm24t1uubz&dp=v5_42s1eekk33drkxs90p3wbwdm24t1uubz&utm_source=epn&utm_medium=cpa&utm_campaign=208&utm_content=0&product_id=32868834536&afref=https%3A%2F%2Fbackit.me&aff_fcid=0557ee873c784873aeaec6d3a895b90d-1695404612484-05078-_vPQBRQ&tt=API&aff_fsk=_vPQBRQ&aff_platform=api-new-link-generate&sk=_vPQBRQ&aff_trace_key=0557ee873c784873aeaec6d3a895b90d-1695404612484-05078-_vPQBRQ&terminal_id=4559c76026b7443083747fca08307839&afSmartRedirect=y)
- 2x M2.5x18 bolt. [[AliExpress]](https://de.aliexpress.com/item/1005003853856791.html?af=208&cv=0&cn=42s1eenn5zlx19t4h9xmxpal5wqfbf41&dp=v5_42s1eenn5zlx19t4h9xmxpal5wqfbf41&af=208&cv=0&cn=42s1eenn5zlx19t4h9xmxpal5wqfbf41&dp=v5_42s1eenn5zlx19t4h9xmxpal5wqfbf41&utm_source=epn&utm_medium=cpa&utm_campaign=208&utm_content=0&product_id=1005003853856791&afref=https%3A%2F%2Fbackit.me&aff_fcid=791ee3d4ae5c448eb8498230bbe23f74-1695404723693-04391-_9G57Xi&aff_fsk=_9G57Xi&aff_platform=portals-hotproduct&sk=_9G57Xi&aff_trace_key=791ee3d4ae5c448eb8498230bbe23f74-1695404723693-04391-_9G57Xi&terminal_id=4559c76026b7443083747fca08307839&afSmartRedirect=y)
- Glue.
- [Details printed on a 3D printer.](https://www.thingiverse.com/thing:6166463)
- [Sketch for Arduino IDE.](https://github.com/coppermilk/wiener_linien_esp32_monitor/)

## Key Features
- **Real-time Countdown**: Stay informed about the next public transport departure times.
- **Timely Updates**: Data is automatically refreshed every 30 seconds to ensure accuracy.
- **Easy Startup**: The device starts up automatically when the ESP32-S3 is booted.
- **User-Friendly Interface**: A thoughtfully designed, intuitive interface for ease of use.
- **Data Source**: Utilizes open data from the City of Vienna provided by Wiener Linien.

## Installation
To set up the ESP32-S3 Public Transport Departure Monitor, follow these steps:

1. **Hardware Installation**:
   - Begin by installing T-Display S3 in your Arduino. For a step-by-step guide, refer to this [YouTube tutorial](https://www.youtube.com/watch?v=gpyeMjM9cOU&ab_channel=VolosProjects).
   
2. **Repository Download**:
   - Clone or download this repository to your local machine.

3. **Board configuration**:
   - In Arduino IDE, open the *Tools* menu, choose *Board*, and open the *Boards manager*
   - Install *esp32* which includes files for the ESP32-S3 too. (Note: this downloads a few 100 MB, and installs about 2.3 GB)
   - Open the *Tools* menu once more, choose *Board*, pick *ESP32 Arduino* and choose *LilyGo T-Display S3*

4. **Library Installation**:
   - Open the Arduino IDE and install the required libraries (TFT_eSPI, ArduinoJson, HttpClient, WiFiManager).

5. **Device Connection**:
   - Connect your ESP32 device to your computer.

6. **Flash the Code**:
   - Flash the code to your ESP32.

## Configuration
Once the installation is complete, follow these steps to configure your device:
[![Everything Is AWESOME](https://img.youtube.com/vi/vSUY8oJgrUI/0.jpg)](https://www.youtube.com/watch?v=vSUY8oJgrUI "Everything Is AWESOME")
- **Connection**:
  - Connect to the device's Wi-Fi network.

- **Settings**:
  - Press the button "Configure WIFI".
  - Enter your Wi-Fi SSID and password.
  - Specify your RBL, which you can find [here](https://till.mabe.at/rbl/?line=102&station=4909).
  - Optionally, select the number of lines to display.
  - Optionally, define your filter criteria.
  - Press the save button to save your settings.

## Resetting
![Reset Button](img/resset_button.png)
In case you need to reset your device, we offer two options:

- **Factory Reset**:
  - Perform a factory reset by keeping the reset button pressed for more than 30 seconds. This will erase all data on your device, including Wi-Fi settings, StopID/RBL, stop filters, and the count of lines displayed on the screen.

- **Soft Reset**:
  - A soft reset involves keeping the reset button pressed for 5 to 10 seconds and then releasing it. This will only erase your Wi-Fi settings.

### When to Use Each Reset?

**Use a Factory Reset if:**
- You plan to give away your device.
- Your device is malfunctioning, and other troubleshooting steps have failed.

**Use a Soft Reset if:**
- You encounter Wi-Fi connectivity issues.
- You want to restore your Wi-Fi settings to their default values.

With the ESP32-S3 Public Transport Departure Monitor, we aim to make your daily commute more predictable and efficient, reducing the time spent waiting for public transport. Stay up-to-date with real-time departure information and plan your journeys with confidence.

![Vienna Liner Monitor](img/monitor.jpeg)

![Line notification](img/notification.gif)
