/*
ESP32 Battery Management System Communication Program
Connects to backend API and controls charge/discharge relays based on battery state

Hardware Connections:
- Charge Relay:       GPIO 23
- Discharge Relay:    GPIO 22
- Voltage Sensor:     Analog input (ADC1_0 - GPIO 36)
- Current Sensor:     Analog input (ADC1_3 - GPIO 39)
- Temperature Sensor: Analog input (ADC1_6 - GPIO 34)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ── WiFi Configuration ───────────────────────────────────────
const char* WIFI_SSID     = "Airtel_bala_1093";
const char* WIFI_PASSWORD = "Air@75272";

// ── API Configuration ────────────────────────────────────────
const char* API_HOST = "bms-production-2a2c.up.railway.app";
const int   API_PORT = 443;  // HTTPS

// ── GPIO Pin Configuration ───────────────────────────────────
const int CHARGE_RELAY_PIN    = 23;
const int DISCHARGE_RELAY_PIN = 22;

// ── Sensor Pin Configuration ─────────────────────────────────
const int VOLTAGE_SENSOR_PIN = 36;  // ADC1_0
const int CURRENT_SENSOR_PIN = 39;  // ADC1_3
const int TEMP_SENSOR_PIN    = 34;  // ADC1_6

// ── Timing Configuration ─────────────────────────────────────
const unsigned long API_UPDATE_INTERVAL    = 3000;  // 3 seconds
const unsigned long SENSOR_UPDATE_INTERVAL = 1000;  // 1 second
const unsigned long WIFI_RECONNECT_DELAY   = 5000;  // 5 seconds

unsigned long lastApiUpdate          = 0;
unsigned long lastSensorUpdate       = 0;
unsigned long lastWifiConnectAttempt = 0;

// Use WiFiClientSecure for HTTPS
WiFiClientSecure wifiClient;

// ── Battery Data ─────────────────────────────────────────────
float  voltage     = 3.7;
float  current_a   = 0.0;   // renamed from 'current' — reserved word in some compilers
float  temperature = 27.0;
String batteryState = "idle";

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 BMS System Starting...");

  // Skip SSL certificate verification (fine for dev/production without pinning)
  wifiClient.setInsecure();

  // Initialize relay pins
  pinMode(CHARGE_RELAY_PIN,    OUTPUT);
  pinMode(DISCHARGE_RELAY_PIN, OUTPUT);

  // Safe default — both relays off
  digitalWrite(CHARGE_RELAY_PIN,    LOW);
  digitalWrite(DISCHARGE_RELAY_PIN, LOW);

  connectToWiFi();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  checkWiFiConnection();

  // Update sensor readings every second
  if (millis() - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    updateSensorReadings();
    lastSensorUpdate = millis();
  }

  // Push to backend and sync relay state every 3 seconds
  if (WiFi.status() == WL_CONNECTED && millis() - lastApiUpdate >= API_UPDATE_INTERVAL) {
    updateBackendAPI();
    controlRelays();
    lastApiUpdate = millis();
  }
}

// ── WiFi ──────────────────────────────────────────────────────
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
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");       Serial.println(WiFi.RSSI());
  } else {
    Serial.println("\nWiFi connection failed — will retry.");
  }

  lastWifiConnectAttempt = millis();
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWifiConnectAttempt >= WIFI_RECONNECT_DELAY) {
    Serial.println("\nWiFi lost. Reconnecting...");
    connectToWiFi();
  }
}

// ── Sensors ───────────────────────────────────────────────────
void updateSensorReadings() {
  int voltageRaw = analogRead(VOLTAGE_SENSOR_PIN);
  int currentRaw = analogRead(CURRENT_SENSOR_PIN);
  int tempRaw    = analogRead(TEMP_SENSOR_PIN);

  // Float math — avoids integer truncation from map()
  // Adjust these calibration formulas to match your actual sensors
  voltage     = (voltageRaw / 4095.0) * 5.0;           // 0 – 5 V
  current_a   = (currentRaw / 4095.0) * 4.0 - 2.0;    // -2 – +2 A
  temperature = (tempRaw    / 4095.0) * 125.0 - 40.0;  // -40 – +85 °C

  Serial.printf("Voltage: %.3fV  Current: %.3fA  Temp: %.2f°C\n",
                voltage, current_a, temperature);
}

// ── API ───────────────────────────────────────────────────────
void updateBackendAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected — skipping API update.");
    batteryState = "idle";
    return;
  }

  Serial.println("\n── Updating backend ──");

  // ── POST /update ─────────────────────────────────────────
  {
    HTTPClient http;
    String url = String("https://") + API_HOST + "/update";
    http.begin(wifiClient, url);
    http.addHeader("Content-Type", "application/json");

    String body = String("{") +
      "\"voltage\":"     + String(voltage,     4) + "," +
      "\"current\":"     + String(current_a,   4) + "," +
      "\"temperature\":" + String(temperature, 4) + "," +
      "\"time\":"        + String(millis() / 1000.0, 3) + "," +
      "\"hardware_connection\":true" +
      "}";

    int code = http.POST(body);
    if (code == HTTP_CODE_OK) {
      Serial.println("POST /update OK: " + http.getString());
    } else {
      Serial.printf("POST /update failed — HTTP %d\n", code);
    }
    http.end();
  }

  // ── GET /state ────────────────────────────────────────────
  {
    HTTPClient http;
    String url = String("https://") + API_HOST + "/state";
    http.begin(wifiClient, url);

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
      String response = http.getString();
      Serial.println("GET /state OK: " + response);

      // Parse state from JSON response
      if      (response.indexOf("\"charging\"")    != -1) { batteryState = "charging"; }
      else if (response.indexOf("\"discharging\"") != -1) { batteryState = "discharging"; }
      else if (response.indexOf("\"stopped\"")     != -1) { batteryState = "stopped"; }
      else if (response.indexOf("\"idle\"")        != -1) { batteryState = "idle"; }
      else {
        Serial.println("Unknown state — defaulting to idle.");
        batteryState = "idle";
      }

    } else {
      Serial.printf("GET /state failed — HTTP %d\n", code);
      batteryState = "idle";
    }
    http.end();
  }
}

// ── Relay Control ─────────────────────────────────────────────
void controlRelays() {
  Serial.println("State → " + batteryState);

  if (batteryState == "charging") {
    digitalWrite(CHARGE_RELAY_PIN,    HIGH);
    digitalWrite(DISCHARGE_RELAY_PIN, LOW);
    Serial.println("Charge relay ON  | Discharge relay OFF");

  } else if (batteryState == "discharging") {
    digitalWrite(CHARGE_RELAY_PIN,    LOW);
    digitalWrite(DISCHARGE_RELAY_PIN, HIGH);
    Serial.println("Charge relay OFF | Discharge relay ON");

  } else {
    // idle, stopped, or any unknown state — safe default
    digitalWrite(CHARGE_RELAY_PIN,    LOW);
    digitalWrite(DISCHARGE_RELAY_PIN, LOW);
    Serial.println("Both relays OFF  | " + batteryState);
  }
}
