# ESP32 Smart Home Security System

A compact and intelligent home security system built with the ESP32-WROOM-32D/U microcontroller, featuring multi-sensor input, a web-based monitoring and control portal, and a robust alarm system.

## Features

* **PIR Motion Detection:** Detects movement within a monitored area.
* **Reflective IR Object Detection:** Utilizes a single 3-pin IR sensor module to detect objects in close proximity (e.g., as a precise tripwire or presence sensor). The system keeps a count of objects detected since the last reset.
* **Ultrasonic Proximity Alert:** Employs an HC-SR04 ultrasonic sensor to measure distance, triggering an alarm condition if an object approaches within 5 cm.
* **Smart Alarm Logic:** The alarm (audible buzzer and blinking external LED) activates **ONLY** if all three core conditions are met simultaneously:
    * The reflective IR sensor has detected at least one object since its count was last reset (`irObjectCount >= 1`).
    * PIR sensor detects active motion.
    * Ultrasonic sensor detects an object within 5 cm.
* **Latching Alarm:** Once triggered by the sensor conditions, the alarm remains active (latched) until it is manually silenced via the web interface.
* **Web Portal for Monitoring & Control:** A simple, responsive web interface hosted directly on the ESP32 allows for:
    * Real-time display of sensor statuses (Motion Detected, Reflective IR Object, Proximity distance).
    * Display of the total objects detected by the reflective IR sensor.
    * Manual activation/silencing of the alarm.
    * Resetting the reflective IR object count.
* **Robust Wi-Fi Connectivity:** Integrates the `WiFiManager` library for easy, persistent, and portable Wi-Fi credential configuration without hardcoding.

## Hardware Components and Connections

* **ESP32 Board:** ESP32-WROOM-32D/U (or any ESP32 DevKitC compatible board)
* **PIR Motion Sensor (HC-SR501):**
    * `VCC` to ESP32 `3V3` pin.
    * `GND` to ESP32 `GND` pin.
    * `OUT` (Signal) to ESP32 `GPIO16`.
* **Reflective IR Sensor Module (3-pin type):**
    * `VCC` to ESP32 `3V3` pin.
    * `GND` to ESP32 `GND` pin.
    * `OUT` (Signal) to ESP32 `GPIO17`.
        * *Note: Many 3-pin reflective IR modules output LOW when an object is detected. The code is designed assuming this behavior.*
* **HC-SR04 Ultrasonic Distance Sensor:**
    * `VCC` to ESP32 `5V` pin (supplied via USB).
    * `GND` to ESP32 `GND` pin.
    * `Trig` pin to ESP32 `GPIO13`.
    * `Echo` pin to a **Voltage Divider** then to ESP32 `GPIO12`.
        * **Voltage Divider Construction:** Connect the `Echo` pin to one end of a **470 Ohm resistor**. Connect the other end of the 470 Ohm resistor to one end of a **1k Ohm resistor**. Connect the other end of the 1k Ohm resistor to `GND`. The connection point *between* the 470 Ohm and 1k Ohm resistors goes to `GPIO12`. This is crucial to convert the 5V Echo signal to a safe 3.3V for the ESP32.
* **Buzzer (Passive):**
    * One buzzer pin to ESP32 `GPIO4`.
    * The other buzzer pin to ESP32 `GND`.
* **External LED (Standard 5mm LED):**
    * Longer leg (Anode) to one end of a **220 Ohm resistor**.
    * The other end of the **220 Ohm resistor** to ESP32 `GPIO23`.
    * Shorter leg (Cathode) to ESP32 `GND`.

## Software Setup

1.  **Arduino IDE:** Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
2.  **ESP32 Board Support:**
    * Open Arduino IDE and go to `File > Preferences`.
    * In "Additional Boards Manager URLs," add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`. Click OK.
    * Go to `Tools > Board > Boards Manager...`. Search for "esp32" and install "esp32 by Espressif Systems".
    * Select your board: `Tools > Board > ESP32 Arduino > ESP32 Dev Module`.
    * Select your COM Port: `Tools > Port > (Your ESP32's COM Port)`.
3.  **Install `WiFiManager` Library:**
    * In Arduino IDE, go to `Sketch > Include Library > Manage Libraries...`.
    * Search for "WiFiManager" and install "WiFiManager by tzapu".

## Code

*(The actual Arduino sketch code would be placed here in your GitHub repository, typically in a file named `ESP32_Security_System.ino` or similar.)*

## Uploading the Code

1.  Open the project sketch (`.ino` file) in the Arduino IDE.
2.  **Initial Wi-Fi Setup:**
    * For the **first upload** (or if you need to re-configure Wi-Fi), locate the line `// wm.resetSettings();` in the `setup()` function of the sketch. **Uncomment this line**.
    * Upload the code to your ESP32.
    * Once uploaded, open the Serial Monitor (Baud rate: 115200).
    * The ESP32 will create its own Wi-Fi Access Point (AP) named "ESP32_Security_AP".
    * Connect your computer or phone to this "ESP32_Security_AP" Wi-Fi network.
    * Open a web browser and navigate to `http://192.168.4.1`.
    * On the configuration portal, select your home Wi-Fi network, enter its password, and click "Save".
    * The ESP32 will then connect to your home Wi-Fi.
    * **After successful configuration, IMMEDIATELY comment out `wm.resetSettings();` again and re-upload the code.** This prevents the ESP32 from always going into AP mode on subsequent boots.
3.  **Regular Uploads:** For any further updates to the code after initial Wi-Fi configuration, ensure `wm.resetSettings();` is commented out. Upload the code as usual.

## Usage

1.  After the ESP32 successfully connects to your home Wi-Fi (its IP address will be printed in the Serial Monitor), ensure your computer/phone is also connected to the same home Wi-Fi network.
2.  Open a web browser and enter the ESP32's IP address (e.g., `http://192.168.1.XX`) in the address bar.
3.  The web portal will display the real-time status of your security system:
    * **Motion Detected:** YES/NO (from PIR sensor)
    * **Reflective IR:** OBJECT DETECTED!/CLEAR (indicating if an object is currently close to the reflective IR sensor)
    * **Objects Detected Count:** A cumulative count of how many times the reflective IR sensor has detected an object since the last reset.
    * **Proximity:** Displays the distance in centimeters and a SAFE/CLOSE! status.
4.  **Alarm Trigger Logic:** The alarm (buzzer and blinking external LED) will activate and latch **ONLY** if:
    * The `Objects Detected Count` is 1 or more (meaning the reflective IR sensor has detected at least one object since its count was last reset).
    * AND `Motion Detected` is YES.
    * AND `Proximity` status is CLOSE! (i.e., the ultrasonic sensor detects an object within 5 cm).
    * Once these conditions are met, the alarm will remain active (latched).
5.  **Alarm Control Buttons (on the Web Portal):**
    * **Activate Alarm:** Manually triggers the alarm (buzzer and blinking LED), overriding the sensor states. The alarm will latch ON.
    * **Silence Alarm:** Stops the alarm and clears its latched state (allowing for future triggers based on sensor conditions).
    * **Reset IR Object Count:** Resets the `Objects Detected Count` to 0.

### 🔧 Applications

* Personal safes
* Lockers or cabinets
* Small secure enclosures
* DIY home security experiments

## Troubleshooting

* **"Connecting for too long" / No Wi-Fi connection:**
    * Double-check SSID and password entered in the `WiFiManager` portal (they are case-sensitive).
    * Ensure your home Wi-Fi network is operating on the 2.4 GHz band. ESP32 modules do not support 5 GHz Wi-Fi.
    * Move the ESP32 physically closer to your Wi-Fi router for testing purposes.
    * Use a high-quality USB data cable. Some cables are "charge-only" and won't work for data transfer.
    * Ensure your USB power source provides sufficient and stable current (ESP32 typically requires at least 500mA).
* **Sensor not working or inaccurate readings:**
    * Verify all wiring connections (VCC, GND, and Signal pins).
    * For the Reflective IR sensor, confirm if its output logic is active LOW or active HIGH and adjust the code if necessary (currently assumes LOW on detect). Ensure its sensitivity knob (if present) is adjusted.
    * For the HC-SR04 ultrasonic sensor, ensure the voltage divider on the Echo pin is correctly wired to protect the ESP32 and provide accurate readings. Also, clear the path in front of the sensor for accurate distance measurement.
    * PIR sensors have a warm-up time (typically 30-60 seconds) after power-up during which they might trigger randomly.
* **LED not blinking or buzzer not sounding:**
    * Check the LED's polarity (longer leg is anode, connects to resistor).
    * Verify the 220 Ohm resistor is in series with the external LED.
    * Verify buzzer connections and its type (passive vs. active).
    * For automatic triggering, ensure all three alarm conditions (IR count >=1, PIR motion, Ultrasonic < 5cm) are simultaneously met.
    * Use the "Activate Alarm" button on the web portal to test the buzzer and LED functionality directly.
    * Monitor the Serial Monitor for debug output indicating which sensor conditions are being met or when the alarm state changes.

---
