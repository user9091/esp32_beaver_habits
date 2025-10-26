# ESP32 Beaver Habits Display

This project displays habit tracking data from a self-hosted Beaver application on an ESP32 with a display. A joystick is used to navigate through the habits and months.

## Features

- Connects to a self-hosted Beaver application.
- Displays habit tracking data in a calendar view.
- Navigate through months and habits using a joystick.
- Toggle habit completion for the current day.

## Hardware Requirements

- ESP32 development board
- ST7789 TFT display
- Joystick module

## Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software)
- [ESP32 Board Support Package](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- A running instance of the [Beaver](https://github.com/daya0576/beaverhabits) application.
- The following Arduino libraries:
  - `Adafruit GFX Library`
  - `Adafruit ST7789 Library`
  - `ArduinoJson`
  - `TFT_eSPI`

## Setup

1. **Clone the repository:**
   ```bash
   git clone https://github.com/user9091/esp32_beaver_habits.git
   ```
2. **Open in Arduino IDE:** Open the `esp32_beaver_habits/esp32.ino` file in the Arduino IDE.
3. **Configure the settings:** In the `esp32.ino` file, update the following variables with your credentials:
   - `ssid`: Your WiFi network name.
   - `password`: Your WiFi password.
   - `beaverHost`: The URL of your Beaver application.
   - `username`: Your Beaver username.
   - `password_api`: Your Beaver API password.
4. **Hardware Connections:** Connect the ST7789 display and joystick module to your ESP32 as follows:
   - **TFT Display:**
     - `TFT_CS`: Pin 5
     - `TFT_RST`: Pin 16
     - `TFT_DC`: Pin 17
     - `TFT_SCK`: Pin 18
     - `TFT_MOSI`: Pin 23
   - **Joystick:**
     - `joyX`: Pin 34
     - `joyY`: Pin 35
     - `joyBtn`: Pin 32
5. **Upload the code:** Select your ESP32 board from the Tools menu and upload the sketch.

## Usage

Use the joystick to navigate the interface:

- **Up/Down:** Switch between different habits.
- **Left/Right:** Change the displayed month.
- **Button Press:** Toggle the completion status of the selected habit for the current day.
