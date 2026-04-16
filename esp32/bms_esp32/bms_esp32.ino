#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "DHT.h"

// ── WiFi Configuration ───────────────────────────────────────
const char* WIFI_SSID     = "DESKTOP-lol";
const char* WIFI_PASSWORD = "12345678";

// ── API Configuration ────────────────────────────────────────
const char* API_HOST = "bmsk-production.up.railway.app";
const int   API_PORT = 443;

// ── GPIO Pin Configuration ───────────────────────────────────
const int CHARGE_RELAY_PIN    = 23;
const int DISCHARGE_RELAY_PIN = 22;

// ── Sensor Pins ──────────────────────────────────────────────
const int VOLTAGE_SENSOR_PIN = 36;
const int CURRENT_SENSOR_PIN = 39;

// ✅ FIXED DHT PIN
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ── Timing Configuration ─────────────────────────────────────
const unsigned long HEARTBEAT_INTERVAL     = 100;
const unsigned long STATE_POLL_INTERVAL    = 200;
const unsigned long SENSOR_UPDATE_INTERVAL = 300;
const unsigned long DHT_INTERVAL           = 1000;
const unsigned long WIFI_RECONNECT_DELAY   = 5000;

unsigned long lastHeartbeat          = 0;
unsigned long lastStatePoll          = 0;
unsigned long lastSensorUpdate       = 0;
unsigned long lastWifiConnectAttempt = 0;
unsigned long lastDHT                = 0;

// ── Network ──────────────────────────────────────────────────
WiFiClientSecure wifiClient;

// ── Battery Data ─────────────────────────────────────────────
float voltage     = 0.0;
float current_a   = 0.0;
float temperature = 27.0;
String batteryState = "idle";

// ── Calibration ──────────────────────────────────────────────
const float ADC_REF = 3.3;

// Voltage module ratio (usually 5)
const float VOLTAGE_RATIO = 5.0;

// Fine tuning multiplier (adjust slightly if needed)
float VOLTAGE_CAL = 1.10;

// ACS712
float ACS_OFFSET = 0.0;
const float ACS_SENSITIVITY = 0.185;

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 BMS System Starting...");

  wifiClient.setInsecure();

  pinMode(CHARGE_RELAY_PIN,    OUTPUT);
  pinMode(DISCHARGE_RELAY_PIN, OUTPUT);

  digitalWrite(CHARGE_RELAY_PIN,    LOW);
  digitalWrite(DISCHARGE_RELAY_PIN, LOW);

  dht.begin();

  calibrateACS();   // 🔥 IMPORTANT

  connectToWiFi();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  checkWiFiConnection();

  if (millis() - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    updateSensorReadings();
    lastSensorUpdate = millis();
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      sendHeartbeat();
      lastHeartbeat = millis();
    }

    if (millis() - lastStatePoll >= STATE_POLL_INTERVAL) {
      fetchState();
      controlRelays();
      lastStatePoll = millis();
    }
  }
}

// ── SENSOR LOGIC ─────────────────────────────────────────────
void updateSensorReadings() {

  // ── VOLTAGE ──
  int vRaw = analogRead(VOLTAGE_SENSOR_PIN);
  float vADC = (vRaw / 4095.0) * ADC_REF;
  voltage = vADC * VOLTAGE_RATIO * VOLTAGE_CAL;

  // ── CURRENT ──
  int cRaw = analogRead(CURRENT_SENSOR_PIN);
  float cADC = (cRaw / 4095.0) * ADC_REF;
  current_a = (cADC - ACS_OFFSET) / ACS_SENSITIVITY;

  // ── TEMPERATURE ──
  if (millis() - lastDHT >= DHT_INTERVAL) {
    float t = dht.readTemperature();
    if (!isnan(t)) {
      temperature = t;
    } else {
      Serial.println("DHT read failed");
    }
    lastDHT = millis();
  }

  Serial.printf("Voltage: %.3fV  Current: %.3fA  Temp: %.2f°C\n",
                voltage, current_a, temperature);
}

// ── ACS AUTO CALIBRATION ─────────────────────────────────────
void calibrateACS() {
  Serial.println("Calibrating ACS712... KEEP NO LOAD");

  float sum = 0;
  int samples = 200;

  for (int i = 0; i < samples; i++) {
    int raw = analogRead(CURRENT_SENSOR_PIN);
    float v = (raw / 4095.0) * ADC_REF;
    sum += v;
    delay(5);
  }

  ACS_OFFSET = sum / samples;

  Serial.print("ACS Offset: ");
  Serial.println(ACS_OFFSET, 4);
}

// ── WiFi ─────────────────────────────────────────────────────
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
  } else {
    Serial.println("\nWiFi connection failed.");
  }

  lastWifiConnectAttempt = millis();
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWifiConnectAttempt >= WIFI_RECONNECT_DELAY) {
    connectToWiFi();
  }
}

// ── API ──────────────────────────────────────────────────────
void sendHeartbeat() {
  HTTPClient http;
  String url = String("https://") + API_HOST + "/update";

  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");

  String body = String("{") +
    "\"voltage\":"     + String(voltage, 4) + "," +
    "\"current\":"     + String(current_a, 4) + "," +
    "\"temperature\":" + String(temperature, 4) + "," +
    "\"time\":"        + String(millis() / 1000.0, 3) + "," +
    "\"hardware_connection\":true" +
    "}";

  int code = http.POST(body);

  if (code == HTTP_CODE_OK) {
    Serial.println("♥ Heartbeat OK");
  } else {
    Serial.printf("♥ Heartbeat failed — HTTP %d\n", code);
  }

  http.end();
}

// ── FETCH STATE ──────────────────────────────────────────────
void fetchState() {
  HTTPClient http;
  String url = String("https://") + API_HOST + "/state";

  http.begin(wifiClient, url);
  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    String response = http.getString();

    if      (response.indexOf("\"charging\"")    != -1) batteryState = "charging";
    else if (response.indexOf("\"discharging\"") != -1) batteryState = "discharging";
    else if (response.indexOf("\"stopped\"")     != -1) batteryState = "stopped";
    else batteryState = "idle";

  } else {
    batteryState = "idle";
  }

  http.end();
}

// ── RELAY CONTROL ────────────────────────────────────────────
void controlRelays() {

  if (batteryState == "charging") {
    digitalWrite(CHARGE_RELAY_PIN, HIGH);
    digitalWrite(DISCHARGE_RELAY_PIN, LOW);

  } else if (batteryState == "discharging") {
    digitalWrite(CHARGE_RELAY_PIN, LOW);
    digitalWrite(DISCHARGE_RELAY_PIN, HIGH);

  } else {
    digitalWrite(CHARGE_RELAY_PIN, LOW);
    digitalWrite(DISCHARGE_RELAY_PIN, LOW);
  }
}