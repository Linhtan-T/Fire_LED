#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <FastLED.h>
#define NUM_LEDS 100 //Set your number of LEDs here
#define DATA_PIN 18 //Set your Data-Pin here

CRGB leds[NUM_LEDS];

// ====== EDIT THESE ======
const char* WIFI_SSID = "_Free_Wifi_Berlin";
const char* WIFI_PASS = "";

// Your Mac's IP shown by Flask when it runs on 0.0.0.0 (or `ipconfig getifaddr en0` on macOS)
const char* LAPTOP_IP = "172.16.40.132";
const uint16_t LAPTOP_PORT = 5055;
const int DAYS = 1;                // 1..7
const uint32_t POLL_MS = 10000;    // pull every 10s

unsigned long lastPoll = 0;

//led Numbers
int asia;
int africa;
int europe;
int sa;
int australia;


void fetchAndPrint() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  // Build URL: http://<ip>:5055/fires?days=N
  String url = "http://" + String(LAPTOP_IP) + ":" + String(LAPTOP_PORT) + "/fires?days=" + String(DAYS);
  Serial.println("GET " + url);

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  if (!http.begin(url)) {
    Serial.println("http.begin() failed");
    return;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("HTTP error: %d\n", code);
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  // Parse JSON: {"counts":{"asia":...},"ts":..., "days":...}
  // Use a DynamicJsonDocument sized a bit larger than expected
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    Serial.println(body); // debug raw
    return;
  }

  JsonObject counts = doc["counts"];
  int asia         = counts["asia"]        | 0;
  int africa       = counts["africa"]      | 0;
  int na           = counts["north_america"] | 0;
  int europe       = counts["europe"]      | 0;
  int australia    = counts["australia"]   | 0;
  int sa           = counts["south_america"] | 0;
  long ts          = doc["ts"]             | 0;
  int days         = doc["days"]           | 0;

  Serial.println("=== FIRMS counts ===");
  Serial.printf("days=%d  ts=%ld\n", days, ts);
  Serial.printf("asia=%d, africa=%d, north_america=%d, europe=%d, australia=%d, south_america=%d\n",
                asia, africa, na, europe, australia, sa);
  Serial.println("====================");
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\nWiFi OK  IP=" + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  ensureWifi();
  // first fetch immediately
  fetchAndPrint();

  //LED Setting
  pinMode(DATA_PIN,OUTPUT);
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  ensureWifi();
  unsigned long now = millis();
  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    fetchAndPrint();
  }

  
  int ledAsia = asia / 100;
  int ledAfrica = africa / 100;
  int ledEuro = europe / 10;
  int ledAus = australia / 100;
  int ledSA = sa / 100;

  for (int i = 0; i < ledAsia; i++){
    leds[i] = (0,0,255);
  }
  for (int i = 20; i < 20 + ledAfrica; i++){
    leds[i] = (0,0,255);
  }
  for (int i = 40; i < 40 + ledEuro; i++){
    leds[i] = (0,0,255);
  }
  for (int i = 60; i < 60 + ledAus; i++){
    leds[i] = (0,0,255);
  }
  for (int i = 80; i < 80 + ledSA; i++){
    leds[i] = (0,0,255);
  }
  
   FastLED.show();
   delay(50);

}
