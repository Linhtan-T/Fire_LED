/*
 * esp32_fires_subscriber.ino — Subscribe to MQTT fire count for ONE continent
 *
 * Libraries (Arduino IDE):
 *   - PubSubClient by Nick O'Leary
 *   - FastLED by Daniel Garcia
 * Board: "ESP32 Dev Module"
 *
 * Each ESP32 sets CONTINENT to a different one of:
 *   "asia", "africa", "north_america", "europe", "australia", "south_america"
 *
 * The Raspberry Pi publisher (fires_mqtt.py) publishes retained messages to:
 *   fires/<continent>  -> payload is an integer (e.g., "1234")
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>

// ======== USER CONFIG ========
const char* WIFI_SSID     = "YOUR_WIFI";
const char* WIFI_PASSWORD = "YOUR_PASS";

const char* MQTT_HOST = "192.168.1.70";  // IP of your broker (e.g., Pi)
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = nullptr; // or "user"
const char* MQTT_PASS = nullptr; // or "pass"

const char* BASE_TOPIC = "fires";
const char* CONTINENT  = "asia";   // <--- change per device

#define DATA_PIN 18
#define NUM_LEDS 30
#define BRIGHTNESS  48

// Expected scale for counts -> LEDs
// You can tune this per continent based on typical volumes.
const long EXPECTED_MAX = 4000;  // counts mapped to full strip

// LED color for "on" pixels
const CRGB ON_COLOR  = CRGB::Red;
const CRGB OFF_COLOR = CRGB::Black;
// ==============================

CRGB leds[NUM_LEDS];
WiFiClient espClient;
PubSubClient mqtt(espClient);

volatile long latestCount = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastWifiCheck = 0;

// Forward decls
void drawFromCount(long count);
void ensureWifi();
void ensureMqtt();
String chipId();

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Parse integer payload
  static char buf[32];
  length = min(length, (unsigned int)(sizeof(buf) - 1));
  memcpy(buf, payload, length);
  buf[length] = 0;
  long val = atol(buf);
  latestCount = val;
  drawFromCount(latestCount);
}

void setup() {
  delay(100);
  Serial.begin(115200);
  Serial.println("\n[ESP32 fires subscriber starting]");

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  ensureWifi();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  ensureMqtt();
}

void loop() {
  // Keep connections alive
  if ((millis() - lastWifiCheck) > 5000) {
    ensureWifi();
    lastWifiCheck = millis();
  }
  if (!mqtt.connected()) {
    ensureMqtt();
  }
  mqtt.loop();

  // Optional: small heartbeat animation when connected but no messages yet
  static uint8_t hue = 0;
  if (latestCount == 0 && mqtt.connected()) {
    leds[0] = CHSV(hue++, 200, 40);
    FastLED.show();
    delay(30);
    leds[0] = OFF_COLOR;
    FastLED.show();
  } else {
    delay(20);
  }
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  Serial.printf("[WiFi] Connecting to %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] Failed to connect (timeout).");
  }
}

void ensureMqtt() {
  if (mqtt.connected()) return;
  if (millis() - lastMqttAttempt < 2000) return;
  lastMqttAttempt = millis();

  String clientId = "fires-sub-" + chipId();
  const char* willTopic = "fires/devices/status";
  const char* willMsg   = "offline";
  bool ok;
  if (MQTT_USER) {
    ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS, willTopic, 1, true, willMsg);
  } else {
    ok = mqtt.connect(clientId.c_str(), nullptr, nullptr, willTopic, 1, true, willMsg);
  }

  if (ok) {
    Serial.println("[MQTT] Connected");
    mqtt.publish("fires/devices/status", "online", true);
    String topic = String(BASE_TOPIC) + "/" + CONTINENT;
    mqtt.subscribe(topic.c_str(), 1);
    Serial.print("[MQTT] Subscribed to: "); Serial.println(topic);
  } else {
    Serial.print("[MQTT] Connect failed, rc="); Serial.println(mqtt.state());
  }
}

void drawFromCount(long count) {
  // Map count to number of lit LEDs (0..NUM_LEDS)
  long ledCount = map(count, 0, EXPECTED_MAX, 0, NUM_LEDS);
  if (ledCount < 0) ledCount = 0;
  if (ledCount > NUM_LEDS) ledCount = NUM_LEDS;

  // Simple fill from start
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = (i < ledCount) ? ON_COLOR : OFF_COLOR;
  }
  FastLED.show();

  Serial.print("[LED] count="); Serial.print(count);
  Serial.print(" -> leds="); Serial.println(ledCount);
}

String chipId() {
  uint64_t chipid = ESP.getEfuseMac();
  char buf[17];
  snprintf(buf, sizeof(buf), "%04X%08X",
           (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(buf);
}
