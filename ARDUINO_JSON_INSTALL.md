# ArduinoJson Library Installation Guide for ESP32

This guide will help you install the ArduinoJson library (version 6.x) for your ESP32 project.

## Method 1: Using Arduino Library Manager (Recommended)

1. Open the Arduino IDE
2. Go to **Tools > Board > Boards Manager**
3. Search for "ESP32" and install the latest version of the ESP32 Arduino Core
4. Go to **Sketch > Include Library > Manage Libraries...**
5. In the Library Manager window:
   - Search for "ArduinoJson"
   - Look for "ArduinoJson by Benoit Blanchon"
   - Select version **6.x** (e.g., 6.21.3 or later)
   - Click "Install"

## Method 2: Manual Installation

If you prefer to install manually or have issues with Library Manager:

1. Download the latest ArduinoJson 6.x release from GitHub:
   - Go to https://github.com/bblanchon/ArduinoJson/releases
   - Download the `.zip` file (e.g., ArduinoJson-6.21.3.zip)

2. In Arduino IDE:
   - Go to **Sketch > Include Library > Add .ZIP Library...**
   - Select the downloaded zip file
   - Wait for installation to complete

## Method 3: Using PlatformIO (if you're using PlatformIO)

Add this to your `platformio.ini` file:

```ini
lib_deps = 
    bblanchon/ArduinoJson@^6.21.3
```

## Verifying Installation

To verify the installation is working:

1. Open your ESP32 project in Arduino IDE
2. Select the correct board and port
3. Click **Sketch > Verify/Compile**
4. Check that there are no errors

## Troubleshooting Common Issues

### "WiFi.h: No such file or directory"

This means the ESP32 Arduino Core is not properly installed.

1. Go to **Tools > Board > Boards Manager**
2. Search for "ESP32"
3. Install or update the ESP32 Arduino Core to the latest version
4. Restart Arduino IDE

### "ArduinoJson.h: No such file or directory"

If you see this error:

1. Go to **Sketch > Include Library**
2. Make sure ArduinoJson 6.x is checked (not grayed out)
3. If not checked, check it or reinstall the library
4. If you manually installed, make sure you chose version 6.x

### Version Compatibility Issues

- Always use ArduinoJson version 6.x for ESP32
- Avoid version 7.x as it has major breaking changes
- The code in this project is specifically written for ArduinoJson 6.x

### Sketch Size Issues

If you get errors about sketch size being too large:

1. Go to **Tools > Partition Scheme**
2. Try "Huge App" or "Minimal SPIFFS" options
3. If using PlatformIO, adjust your `platformio.ini` file

## Using the Library

Once installed, you can use it like this:

```cpp
#include <ArduinoJson.h>

// For serialization (creating JSON)
StaticJsonDocument<200> doc;
doc["key"] = "value";
doc["number"] = 42;
doc["array"] = {1, 2, 3};
String output;
serializeJson(doc, output);

// For deserialization (parsing JSON)
StaticJsonDocument<200> doc;
DeserializationError error = deserializeJson(doc, input);
if (error) {
  Serial.print(F("JSON parsing error: "));
  Serial.println(error.c_str());
} else {
  const char* value = doc["key"];
  int number = doc["number"];
}
```

## Verify Your Project

Now open `esp32_bms_arduino_json.ino` in Arduino IDE and click **Sketch > Verify/Compile**. If you see "Done compiling" without errors, the installation was successful.

## Next Steps

1. Update the WiFi and API configuration in the sketch
2. Upload the sketch to your ESP32 board
3. Monitor the Serial output at 115200 baud
