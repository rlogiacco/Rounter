#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <WebServer.h>

#define DEBUG(format, ...) Serial.printf(format, ##__VA_ARGS__); Serial.println();

/*** WiFi ***/
WebServer server(80);
#include <EEPROM.h>
#include "pages.h"
#define MAX_STRING_LENGTH 32
struct {
  char ssid[MAX_STRING_LENGTH] = "";
  char pwd[MAX_STRING_LENGTH] = "";
  uint16_t duration = 60;
} settings;
void handleNotFound();
void handleRoot();
void handleReset();
void handleCounter();
void handlePlus();
void handleMinus();
void handleMode();
void handleTimer();
void handleWiFi();
void handleInfo();

typedef struct {
  char name[MAX_STRING_LENGTH] = "";
  uint8_t round = 0;
  char type[MAX_STRING_LENGTH] = "";
} alarm_t;
alarm_t *alarms;

/*** Buttons ***/
#include <OneButton.h>
OneButton btnA(6);
OneButton btnB(7);
OneButton btnC(9);

/*** Battery ***/
#include <Battery.h>
Battery battery(3300, 4200, A0, 12);
#define BAT_PIN 0
#define BAT_VOLTAGE 4300
#define BAT_RES_UP 21800
#define BAT_RES_DW 21600


/*** FastLED ***/
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS SPI
#include <FastLED.h>

#define DATA_PIN 5
#define NUM_LEDS 28
#define REFRESH_MS 50
#define SLOW_REFRESH_MS 30000

CRGB leds[NUM_LEDS];
const uint8_t clockIdxs[8] = { 4, 5, 0, 1, 8, 9, 10, 11 };

void clear();
void display(uint16_t, CRGB);
void timer();

#define DISPLAY_MODE 0
#define TIMER_MODE 1
#define TIMER_MODE_PAUSE 2
#define SETTING_MODE 3
#define BATTERY_MODE 4

uint8_t mode = DISPLAY_MODE;
uint16_t ticking = 0;
uint16_t counter = 0;
unsigned long lastUpdate;
unsigned long lastRefresh;
unsigned long lastSlowRefresh;

bool connected = false;
bool checkWiFi() {
  if (WiFi.status() == WL_CONNECTED && !connected) {
    char buffer[15];
    WiFi.localIP().toString().toCharArray(buffer, 15);
    DEBUG("Connected to %s with IP %s", WiFi.SSID(), buffer);
    DEBUG("Starting mDNS responder");
    while (!MDNS.begin("rounder")) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
    MDNS.addService("_http", "_tcp", 80);
    connected = true;
  }
  return WiFi.status() == WL_CONNECTED;
}

void connect(unsigned long maxWait = 0) {
  if (settings.ssid[0] != 255 && settings.ssid[0] != 0) {
    DEBUG("Attempting connection to WiFi [ssid:%s, pwd:%s]", settings.ssid, settings.pwd);
    connected = false;
    WiFi.begin(settings.ssid, settings.pwd);
    maxWait += millis();
    while (!checkWiFi() && maxWait > millis()) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
  }
}

/*** Button Functions ***/
bool reset = false;
void plus(uint16_t* var, uint8_t val = 1) {
  if (*var < 99) *var = *var + val;
}

void minus(uint16_t* var, uint8_t val = 1) {
  if (*var > 0) *var = *var - val;
}

void clickA() {
  DEBUG("Button A click");
  if (mode == DISPLAY_MODE) plus(&counter);
  if (mode == SETTING_MODE) plus(&settings.duration, 10);
}
void clickB() {
  DEBUG("Button B click");
  if (mode == DISPLAY_MODE) {
    mode = TIMER_MODE;
    ticking = settings.duration;
  } else if (mode == TIMER_MODE) {
    mode = TIMER_MODE_PAUSE;
  } else if (mode == TIMER_MODE_PAUSE) {
    mode = TIMER_MODE;
  } else {
    mode = DISPLAY_MODE;
  }
}
void doubleClickB() {
  DEBUG("Button B double click");
  if (mode == TIMER_MODE || mode == TIMER_MODE_PAUSE) {
    mode = DISPLAY_MODE;
  }
}
void longClickB() {
  DEBUG("Button B long click");
  if (mode >= BATTERY_MODE) mode -= BATTERY_MODE;
  if (mode == DISPLAY_MODE) {
    mode = SETTING_MODE;
  } else if (mode == SETTING_MODE) {
    DEBUG("Storing timer duration at %u", settings.duration);
    EEPROM.put(0, settings);
    EEPROM.commit();
    mode = DISPLAY_MODE;
  }
}
void longHoldB() {
  DEBUG("Button B long hold");
  mode += BATTERY_MODE;
}
void clickC() {
  DEBUG("Button C click");
  if (mode == DISPLAY_MODE) minus(&counter);
  if (mode == SETTING_MODE) minus(&settings.duration, 10);
}
void longHoldAC() {
  DEBUG("Button A or C long hold");
  if (reset) {
    counter = 0;
  } else { 
    reset = true; 
  }
}
void longReleaseAC() {
  DEBUG("Button A or C long release");
  reset = false;
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);

  DEBUG("Reading settings from EEPROM");
  EEPROM.begin(sizeof(settings));
  EEPROM.get(0, settings);
  DEBUG("Starting WiFi");
  WiFi.setSleep(false);
  WiFi.softAP("Round Counter");
  
  if (settings.duration == 0 || settings.duration == 255) {
    DEBUG("No timer duration found in EEPROM, setting to default of 60 seconds");
    settings.duration = 60;
  }

  server.on("/", handleRoot);
  server.on("/timer", HTTP_GET, handleTimer);
  server.on("/timer", HTTP_POST, handleTimer);
  server.on("/mode", HTTP_GET, handleMode);
  server.on("/counter", HTTP_POST, handleCounter);
  server.on("/plus", HTTP_GET, handlePlus);
  server.on("/plus", HTTP_POST, handlePlus);
  server.on("/minus", HTTP_GET, handleMinus);
  server.on("/minus", HTTP_POST, handleMinus);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/reset", HTTP_DELETE, handleReset);
  server.on("/wifi", HTTP_GET, handleWiFi);
  server.on("/info", HTTP_GET, handleInfo);
  server.onNotFound(handleNotFound);
  server.begin();

  btnA.attachClick(clickA);
  btnA.attachLongPressStart(longHoldAC);
  btnA.attachLongPressStop(longReleaseAC);
  btnB.attachClick(clickB);
  btnB.attachLongPressStart(longHoldB);
  btnB.attachLongPressStop(longClickB);
  btnB.attachDoubleClick(doubleClickB);
  btnC.attachClick(clickC);
  btnC.attachLongPressStart(longHoldAC);
  btnC.attachLongPressStop(longReleaseAC);

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(128);
  
  analogSetAttenuation(ADC_11db); 
  analogReadResolution(12); 
	battery.begin(3300, 2.0, &sigmoidal);

  connect(1000);
  CRGB color = CRGB::RoyalBlue;
  for (uint8_t i = 0; i < 10; i++) {
    if (checkWiFi()) {
      color = CRGB::Green;
    }
    display(i * 11, color);
    delay(250);
  }
  
  lastSlowRefresh = millis();
  DEBUG("Setup complete!");
}

void loop() {
  server.handleClient();
  btnA.tick();
  btnB.tick();
  btnC.tick();

  if (millis() - lastRefresh >= REFRESH_MS) {
    Serial.print('.');

    lastRefresh = millis();
    switch (mode) {
      case DISPLAY_MODE:
        display(counter, CRGB::Green);
        break;
      case TIMER_MODE:
      case TIMER_MODE_PAUSE:
        timer();
        break;
      case SETTING_MODE:
        display(settings.duration, CRGB::Blue);
        break;
    }
  }
  if (millis() - lastSlowRefresh >= SLOW_REFRESH_MS) {
    DEBUG("Connected clients: %d\n", WiFi.softAPgetStationNum());
    lastSlowRefresh = millis();
    if (!checkWiFi()) {
      connect();
    }
    DEBUG("Battery voltage is %umV, level is %u%%", battery.voltage(), battery.level());
    if (mode == BATTERY_MODE) {
      display(battery.level(), CRGB::Yellow);
    }
  }
}

void handleNotFound() {
  server.send(404, "text/html", "Not found");
}
void handleRoot() {
  server.send(200, "text/html", adminPage);
}
void handleReset() {
  DEBUG("Reset requested");
  mode = DISPLAY_MODE;
  counter = 0;
  server.send(200, "text/html", "DONE!");
}
void handleCounter() {
  if (server.hasArg("val")) {
    counter = atoi(server.arg("val").c_str());
    DEBUG("Setting counter to %u", counter);
  }
  char buffer[10];
  itoa(counter, buffer, 10);
  server.send(200, "text/html", buffer);
}
void handlePlus() {
  char buffer[10];
  itoa(++counter, buffer, 10);
  server.send(200, "text/html", buffer);
}
void handleMinus() {
  char buffer[10];
  itoa(--counter, buffer, 10);
  server.send(200, "text/html", buffer);
}
void handleMode() {
  if (server.hasArg("val")) {
    mode = atoi(server.arg("val").c_str());
    DEBUG("Setting mode to %u", mode);
  }
  char buffer[10];
  itoa(mode, buffer, 10);
  server.send(200, "text/html", buffer);
}

void handleTimer() {
  if (server.hasArg("val")) {
    settings.duration = atoi(server.arg("val").c_str());
    DEBUG("Setting timer duration to %u", settings.duration);
    if (server.hasArg("save")) {
      EEPROM.put(0, settings);
      EEPROM.commit();
    }
  } else {
    DEBUG("Starting %u seconds timer", settings.duration);
    mode = TIMER_MODE;
    ticking = settings.duration;
  }
  server.send(200, "text/html", "OK");
}

void handleWiFi() {
  if (server.hasArg("ssid")) {
    server.arg("ssid").toCharArray(settings.ssid, MAX_STRING_LENGTH);
    if (server.hasArg("pwd")) {
      server.arg("pwd").toCharArray(settings.pwd, MAX_STRING_LENGTH);
    }
    DEBUG("Storing WiFi [ssid:%s, pwd:%s]", settings.ssid, settings.pwd);
    EEPROM.put(0, settings);
    EEPROM.commit();
  }
  server.send(200, "text/html", "Attempting connection");
  connect(10000);
}


void handleInfo() {
  String json = "{\"counter\":";
  json += counter;
  json += ",\"timer\":";
  json += settings.duration;
  json += "}";
  server.send(200, "application/json", json);
}
void clear() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
}
void set(uint8_t idx, uint8_t pos, CRGB color) {
  leds[(idx * 2) + (14 * pos)] = color;
  leds[(idx * 2) + (14 * pos)+ 1] = color;
}
void num(uint8_t val, uint8_t pos, CRGB color) {
  switch (val) {
    case 0:
      if (pos == 0) {
        set(0, pos, color); set(1, pos, color); set(2, pos, color); set(3, pos, color); set(4, pos, color); set(5, pos, color);
      }
      break;
    case 1:
      set(0, pos, color); set(5, pos, color);
      break;
    case 2:
      set(5, pos, color); set(4, pos, color); set(6, pos, color); set(1, pos, color); set(2, pos, color);
      break;
    case 3:
      set(1, pos, color); set(4, pos, color); set(5, pos, color); set(0, pos, color); set(6, pos, color);
      break;
    case 4:
      set(3, pos, color); set(5, pos, color); set(0, pos, color); set(6, pos, color);
      break;
    case 5:
      set(4, pos, color); set(3, pos, color); set(6, pos, color); set(0, pos, color); set(1, pos, color);
      break;
    case 6:
      set(4, pos, color); set(3, pos, color); set(2, pos, color); set(1, pos, color); set(0, pos, color); set(6, pos, color);
      break;
    case 7:
      set(4, pos, color); set(5, pos, color); set(0, pos, color);
      break;
    case 8:
      set(0, pos, color); set(1, pos, color); set(2, pos, color); set(3, pos, color); set(4, pos, color); set(5, pos, color); set(6, pos, color);
      break;
    case 9:
      set(3, pos, color); set(4, pos, color); set(5, pos, color); set(0, pos, color); set(6, pos, color); set(1, pos, color);
      break;
  }
}

void showTimer() {
  CRGB color = CRGB::Red;
  if (ticking > 10) {
    display(ticking < 100 ? ticking : (ticking / 60) + 1, CRGB::Blue);
  } else {
    display(ticking, CRGB::Red);
  } 
}

void timer() {
  if (mode == TIMER_MODE_PAUSE) {
    showTimer();
    FastLED.show();
    return;
  }
  if (millis() - lastUpdate >= 1000) {
    ticking--;
    lastUpdate = millis();
    showTimer();
    DEBUG("Timer tick %u", ticking);
  }
  if (ticking == 0) {
    // pulse and exit timer mode
    for (uint8_t i = 0; i < 10; i++) {
      if (i % 2 == 0) {
        display(0, CRGB::Red);
      } else {
        display(0, CRGB::Black);
      }
      FastLED.show();
      delay(250);
    }
    mode = DISPLAY_MODE;
  } else {
    fadeToBlackBy(leds, NUM_LEDS, 50);
    FastLED.show();
  }
}

void display(uint16_t number, CRGB color) {
  int units = number % 10;
  int tens = int(number / 10) % 10;
  clear();
  num(units, 0, color);
  num(tens, 1, color);
  FastLED.show();
}

void set(uint8_t idx, CRGB color) {
  leds[(idx * 2) ] = color;
  leds[(idx * 2) + 1] = color;
}

uint8_t clockPos = 0;
void clock(uint8_t pos, CRGB color) {
  if (pos == 8) {
    pos = 0;
    clear();
  } else {
    leds[(clockIdxs[pos] * 2) ] = color;
    leds[(clockIdxs[pos] * 2) + 1] = color;
  }
  FastLED.show();
}