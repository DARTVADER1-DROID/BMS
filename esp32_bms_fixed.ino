/*
ESP32 Battery Management System Communication Program
Connects to backend API and controls charge/discharge relays based on battery state

Hardware Connections:
- Charge Relay: GPIO 23
- Discharge Relay: GPIO 22
- Voltage Sensor: Analog input (ADC1_0 - GPIO 36)
- Current Sensor: Analog input (ADC1_3 - GPIO 39)
- Temperature Sensor: Analog input (ADC1_6 - GPIO 34)
*/

#include <WiFi.h>
#include <HTTPClient.h>

// WiFi Configuration
const char* WIFI_SSID = "Airtel_bala_1093";
const char* WIFI_PASSWORD = "Air@75272";

// API Configuration
const char* API_HOST = "127.0.0.1";  // Replace with your backend server IP address
const int API_PORT = 8000;

// GPIO Pin Configuration
const int CHARGE_RELAY_PIN = 23;
const int DISCHARGE_RELAY_PIN = 22;

// Sensor Configuration
const int VOLTAGE_SENSOR_PIN = 36;  // ADC1_0
const int CURRENT_SENSOR_PIN = 39;  // ADC1_3
const int TEMP_SENSOR_PIN = 34;     // ADC1_6

// Timing Configuration
const unsigned long API_UPDATE_INTERVAL = 3000;  // 3 seconds
const unsigned long SENSOR_UPDATE_INTERVAL = 1000;  // 1 second
const unsigned long WIFI_RECONNECT_DELAY = 5000;  // 5 seconds

unsigned long lastApiUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastWifiConnectAttempt = 0;

WiFiClient wifiClient;
HTTPClient http;

// Battery Parameters
float voltage = 3.7;
float current = 0.0;
float temperature = 27.0;

// Current State
String batteryState = "idle";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 BMS System Starting...");

  // Initialize relay pins
  pinMode(CHARGE_RELAY_PIN, OUTPUT);
  pinMode(DISCHARGE_RELAY_PIN, OUTPUT);
  
  // Initially turn off all relays
  digitalWrite(CHARGE_RELAY_PIN, LOW);
  digitalWrite(DISCHARGE_RELAY_PIN, LOW);

  // Connect to WiFi
  connectToWiFi();
}

void loop() {
  // Check and maintain WiFi connection
  checkWiFiConnection();

  // Update sensor readings periodically
  if (millis() - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    updateSensorReadings();
    lastSensorUpdate = millis();
  }

  // Update backend API periodically (only if WiFi is connected)
  if (WiFi.status() == WL_CONNECTED && millis() - lastApiUpdate >= API_UPDATE_INTERVAL) {
    updateBackendAPI();
    controlRelays();
    lastApiUpdate = millis();
  }
}

void connectToWiFi() {
  Serial.println("\nConnecting to WiFi: " + String(WIFI_SSID));
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("\nWiFi connection failed!");
  }

  lastWifiConnectAttempt = millis();
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiConnectAttempt >= WIFI_RECONNECT_DELAY) {
    Serial.println("\nWiFi connection lost. Reconnecting...");
    connectToWiFi();
  }
}

bool testBackendConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping backend test");
    return false;
  }

  String url = String("http://") + API_HOST + ":" + String(API_PORT) + "/";
  http.begin(wifiClient, url);

  int httpResponseCode = http.GET();
  bool success = false;

  if (httpResponseCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("Backend connection test: Success");
    Serial.println("Response: " + response);
    success = true;
  } else {
    Serial.print("Backend connection test failed. HTTP code: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode == -1) {
      Serial.println("Possible causes: Server down, incorrect IP, network issue, or CORS problem");
    }
  }

  http.end();
  return success;
}

void updateSensorReadings() {
  // Read sensor values (these are placeholders - replace with actual sensor calibration)
  int voltageRaw = analogRead(VOLTAGE_SENSOR_PIN);
  int currentRaw = analogRead(CURRENT_SENSOR_PIN);
  int tempRaw = analogRead(TEMP_SENSOR_PIN);

  // Convert raw ADC values to real units (calibration required for your sensors)
  voltage = map(voltageRaw, 0, 4095, 0, 50) / 10.0;  // 0-5V range, converted to 0-5V with 0.1V resolution
  current = map(currentRaw, 0, 4095, -20, 20) / 10.0;  // -2A to 2A range
  temperature = map(tempRaw, 0, 4095, -40, 85);  // -40°C to 85°C range

  Serial.print("Voltage: "); Serial.print(voltage); Serial.print("V, ");
  Serial.print("Current: "); Serial.print(current); Serial.print("A, ");
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println("°C");
}

void updateBackendAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping API update");
    batteryState = "idle";
    return;
  }

  Serial.println("\nUpdating backend API...");

  // Send sensor data to backend
  String url = String("http://") + API_HOST + ":" + String(API_PORT) + "/update";
  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");

  // Create JSON payload manually (without ArduinoJson)
  String requestBody = String("{") +
    "\"voltage\":" + String(voltage) + "," +
    "\"current\":" + String(current) + "," +
    "\"temperature\":" + String(temperature) + "," +
    "\"time\":" + String(millis() / 1000.0) + "," +
    "\"hardware_connection\":true" +
    "}";

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("Update response: " + response);
  } else {
    Serial.print("Update failed. HTTP code: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode == -1) {
      Serial.println("Connection refused - check server IP and port");
    }
  }

  http.end();

  // Get current battery state from backend
  url = String("http://") + API_HOST + ":" + String(API_PORT) + "/state";
  http.begin(wifiClient, url);

  httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("State response: " + response);
    
    // Extract state from response using string manipulation
    if (response.indexOf("\"state\":\"charging\"") != -1) {
      batteryState = "charging";
    } else if (response.indexOf("\"state\":\"discharging\"") != -1) {
      batteryState = "discharging";
    } else if (response.indexOf("\"state\":\"idle\"") != -1) {
      batteryState = "idle";
    } else {
      Serial.println("Unknown state received");
      batteryState = "idle";
    }

    // Extract warning message
    int warningStart = response.indexOf("\"warning\":\"") + 11;
    if (warningStart > 10) {
      int warningEnd = response.indexOf("\"", warningStart);
      if (warningEnd > warningStart) {
        String warning = response.substring(warningStart, warningEnd);
        Serial.print("Warning: "); Serial.println(warning);
      }
    }
  } else {
    Serial.print("Get state failed. HTTP code: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode == -1) {
      Serial.println("Connection refused - check server IP and port");
    }
    batteryState = "idle";  // Default to idle if we can't get state
  }

  http.end();
}

void controlRelays() {
  Serial.print("Current State: "); Serial.println(batteryState);

  if (batteryState == "charging") {
    digitalWrite(CHARGE_RELAY_PIN, HIGH);
    digitalWrite(DISCHARGE_RELAY_PIN, LOW);
    Serial.println("Charge relay ON, Discharge relay OFF");
  } else if (batteryState == "discharging") {
    digitalWrite(CHARGE_RELAY_PIN, LOW);
    digitalWrite(DISCHARGE_RELAY_PIN, HIGH);
    Serial.println("Charge relay OFF, Discharge relay ON");
  } else {  // idle or unknown state
    digitalWrite(CHARGE_RELAY_PIN, LOW);
    digitalWrite(DISCHARGE_RELAY_PIN, LOW);
    Serial.println("Both relays OFF - Idle state");
  }
}
