# MediBox Embedded System

## Overview
MediBox is an embedded system for smart medicine management. It monitors environmental conditions, controls a servo-driven dispenser, and provides real-time feedback and control via an IoT dashboard (Node-RED). The system is built on ESP32 and integrates sensors, actuators, and MQTT-based communication.

## Features
- **Temperature & Humidity Monitoring:** Uses a DHT22 sensor to monitor and alert on environmental conditions.
- **Light Intensity Sensing:** Reads ambient light using an LDR and normalizes the value.
- **Servo Control:** Adjusts a servo motor based on light, temperature, and configurable parameters.
- **Alarms & Scheduling:** Supports multiple medicine alarms with snooze and delete options.
- **OLED Display:** Shows time, date, and system status on a 128x64 OLED.
- **MQTT Integration:** Publishes sensor data and receives configuration updates via MQTT.
- **Node-RED Dashboard:** Visualizes data and allows remote parameter adjustment.

## Hardware Requirements
- ESP32 Dev Board
- DHT22 Temperature & Humidity Sensor
- LDR (Light Dependent Resistor)
- Servo Motor (SG90 or similar)
- 0.96" 128x64 OLED Display (I2C)
- Buzzer
- Push Buttons (for menu navigation and alarm control)
- LEDs (for status indication)

## Software Requirements
- Arduino IDE or PlatformIO
- Node-RED (for dashboard)
- MQTT Broker (e.g., HiveMQ, Mosquitto)
- Required Arduino libraries (see `libraries.txt`)

## File Structure
```
MediBox/
├── src/
│   └── sketch.ino         # Main firmware source code
├── Node-RED/
│   └── flows.json         # Node-RED dashboard and logic
├── libraries.txt          # List of required Arduino libraries
├── platformio.ini         # PlatformIO project config (if used)
├── README.md              # This file
└── ...
```

## How It Works
1. **Startup:**
   - Connects to WiFi and MQTT broker.
   - Initializes sensors, display, and servo.
   - Publishes initial configuration parameters to MQTT.
2. **Sensing & Control:**
   - Periodically samples temperature, humidity, and light.
   - Computes servo angle based on configurable parameters and sensor data.
   - Triggers alarms and displays messages as scheduled.
3. **MQTT Communication:**
   - Publishes sensor data to `medibox/sensor_data`.
   - Publishes initial and updated parameters to `/Medibox/ts`, `/Medibox/tu`, `/Medibox/gammaValue`, `/Medibox/thetaOfset`, `/Medibox/Tmed`, or as a JSON object to `/Medibox/params`.
   - Receives parameter updates from the dashboard and applies them in real time.
4. **Node-RED Dashboard:**
   - Visualizes sensor data (gauges, charts).
   - Allows remote adjustment of parameters (sliders, text fields).
   - Displays current configuration values as text.

## Node-RED Dashboard Setup
1. Import `Node-RED/flows.json` into your Node-RED instance.
2. Ensure your MQTT broker settings match those in the flow and firmware.
3. The dashboard will show:
   - Live temperature, humidity, and light intensity
   - Current values of `ts`, `tu`, `gammaValue`, `thetaOfset`, `Tmed`
   - Controls to adjust these parameters remotely

## Customization
- **Alarms:**
  - Set, enable/disable, view, or delete alarms using the device buttons and menu.
- **Parameters:**
  - Adjust sampling/sending intervals, servo control factors, and temperature setpoint via Node-RED or MQTT.

## Libraries Used
See `libraries.txt` for a full list. Key libraries include:
- Adafruit_SSD1306
- Adafruit_GFX
- DHTesp
- PubSubClient
- ArduinoJson
- ESP32Servo

## MQTT Topics
- **Sensor Data:** `medibox/sensor_data` (JSON)
- **Parameter Updates:** `/Medibox/ts`, `/Medibox/tu`, `/Medibox/gammaValue`, `/Medibox/thetaOfset`, `/Medibox/Tmed` (individual) or `/Medibox/params` (JSON object)

## Example MQTT Parameter JSON
```json
{
  "ts": 5,
  "tu": 10,
  "gammaValue": 0.75,
  "thetaOfset": 30,
  "Tmed": 30.0
}
```

## Troubleshooting
- Ensure all hardware is connected as per your schematic.
- Check serial output for debug messages.
- Verify MQTT broker connectivity and topic subscriptions.
- Use Node-RED debug nodes to inspect incoming messages.

## License
This project is for educational use. See LICENSE if present.
