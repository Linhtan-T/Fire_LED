#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ====== EDIT THESE ======
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

// Your Mac's IP shown by Flask when it runs on 0.0.0.0 (or `ipconfig getifaddr en0` on macOS)
const char* LAPTOP_IP = "192.168.1.70";
const uint16_t LAPTOP_PORT = 5055;
const int DAYS = 1;                // 1..7
const uint32_t POLL_MS = 10000;    // pull every 10s

unsigned long lastPoll = 0;

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
}

void loop() {
  ensureWifi();
  unsigned long now = millis();
  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    fetchAndPrint();
  }
}


