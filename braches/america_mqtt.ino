// esp32_fires_subscriber.ino — super tiny subscriber (prints only)
#include <WiFi.h>
#include <PubSubClient.h>

#include <FastLED.h>
#define NUM_LEDS 50
#define DATA_PIN 18


CRGB leds[NUM_LEDS];

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

// TODO: set these
const char* WIFI_SSID = "Healthy_Design_&_Sick_Machines";
const char* WIFI_PSK  = "Bakterien_und_Viren";
const char* MQTT_HOST = "192.168.178.74"; // Pi's IP

int led_nums = 22;

//int ledsNums[16] = {2, 5, 8, 11, 15, 18, 22, 25, 27, 29, 31, 35, 39, 42, 47, 49};

WiFiClient net;
PubSubClient mqtt(net);

const char* hostname = "america";
int scale = 100;

void onMsg(char* topic, byte* payload, unsigned int len) {
  String msg; msg.reserve(len);
  for (unsigned int i = 0; i < len; ++i) msg += (char)payload[i];
  //Serial.print("[MSG] "); Serial.print(topic); Serial.print(" => "); Serial.println(msg);

  int fires = msg.toInt();
  int scaledFires = fires / scale;
  Serial.print("fires: ");
  Serial.print(fires);

  
  for(int i = led_nums - 1; i >= led_nums - scaledFires; i--){
    leds[i] =  CRGB::Red;
  }
  FastLED.show();
  delay(50);



}

void waitForWiFi() {
  //Serial.print("[WiFi] Connecting to "); Serial.print(WIFI_SSID); Serial.print(" ");
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  //Serial.println();
  //Serial.print("[WiFi] Connected. IP: "); Serial.println(WiFi.localIP());
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    //Serial.print("[MQTT] Connecting to "); Serial.print(MQTT_HOST); Serial.print(" ... ");
    if (mqtt.connect("esp32-fires")) {
      //Serial.println("OK");
      mqtt.subscribe("fires/north_america");
      //Serial.println("[MQTT] Subscribed to fires/#");
    } else {
      //Serial.print("FAIL (state="); Serial.print(mqtt.state()); Serial.println(") retrying...");
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  waitForWiFi();
  mqtt.setServer(MQTT_HOST, 1883);
  mqtt.setCallback(onMsg);
  ensureMqtt();

  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();

  pinMode(DATA_PIN,OUTPUT);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
}

void loop() {

  ArduinoOTA.handle();
  // Re-report if WiFi drops
  if (WiFi.status() != WL_CONNECTED) {
    //Serial.println("[WiFi] Lost connection. Reconnecting...");
    waitForWiFi();
  }
  if (!mqtt.connected()) {
    //Serial.println("[MQTT] Disconnected. Reconnecting...");
    ensureMqtt();
  }
  mqtt.loop();
  
}