#define DEBUG 1

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include <Basecamp.hpp>
#include "esp_system.h"
#include "apps/sntp/sntp.h"
#include "esp_wifi.h"

#include <GxEPD.h>

#include <GxGDEW042T2_FPU/GxGDEW042T2_FPU.cpp>      // 4.2" b/w
bool hasRed = false;
String displayType = "4.2";

static const char* TAG = "MyModule";

#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

GxIO_Class io(SPI, SS, 17, 16);
GxEPD_Class display(io, 16, 4);
bool connection = false;
const GFXfont* f = &FreeMonoBold9pt7b;

Basecamp iot { 
  Basecamp::SetupModeWifiEncryption::none,
  Basecamp::ConfigurationUI::always
};

void setup() {
  esp_log_level_set("*", ESP_LOG_WARN);        // set all components to ERROR level
  esp_log_level_set(TAG, ESP_LOG_INFO);

  // put your setup code here, to run once:
  iot.begin();
  display.init();
  //iot.configuration.set("OTAActive", "true");
  display.setTextColor(GxEPD_BLACK);

  // Initialize sntp
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org");

  esp_log_level_set("*", ESP_LOG_WARN);        // set all components to ERROR level
  esp_log_level_set(TAG, ESP_LOG_VERBOSE);
  DEBUG_PRINTLN(" * SETUP DONE *********");

  display.eraseDisplay();
  display.fillScreen(GxEPD_WHITE);
  display.setRotation(0);
  display.setFont(f);
  display.setCursor(10, 30);
  display.println(" Hallo Jani");
  display.update();

  uint8_t retry = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retry < 20))
  {
    Serial.print(".");
    retry++;
    delay(500);
  }
  Serial.println("");
  if (retry < 20)
  {
    connection = true;
    iot.mqtt.publish("wecker/state", 0, true, "Wifi Connected");
  }

  setenv("LANG", "de_DE",  1);

  display.eraseDisplay(true);
//  display.fillScreen(GxEPD_WHITE);
}



void loop()
{
  struct tm timeinfo; 
  static int count = 0;
  const GFXfont* f_big = &FreeMonoBold24pt7b;
  static uint16_t day = 380;
  static uint8_t minute = 0;
  static uint8_t sec = 0;

  Serial.println("Loop");

  if (connection)
  {
    while (sec == timeinfo.tm_sec)
    {
      if ( !getLocalTime(&timeinfo, 10000) )
      {
        DEBUG_PRINTLN("AAAAAAA Failed to get time +++++++++++++");
      }
      if (sec == timeinfo.tm_sec)
            delay(100);
    }
  }

  if (count == 0)
  {
    display.eraseDisplay();
    Serial.println("   erase");
    display.fillScreen(GxEPD_WHITE);
    display.update();
//    day = timeinfo.tm_yday;
  }

  count++;

  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 80);
  display.setFont(f_big);
  display.print(&timeinfo, "%d.%m.%Y\n%R:%S");

  {
    char strftime_buf[20];
    strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y - %R (%z) (%Z)", &timeinfo);
    iot.mqtt.publish("wecker/time", 0, true, strftime_buf);
  }

  if (timeinfo.tm_yday != day)
  {
    display.updateWindow(0, 50, 290, 45);
    day = timeinfo.tm_yday;
  }

  if (timeinfo.tm_min != minute)
  {
    display.updateWindow(0, 85, 140, 45);
    minute = timeinfo.tm_min;
  }

  if (sec != timeinfo.tm_sec)
  {
    display.updateWindow(140, 85, 80, 45);
    sec = timeinfo.tm_sec;
  }

  
  //WiFiGenericClass::mode(WIFI_OFF);
  
  
}

/*
TODO:
* After first time display. disable wifi etc. deep_sleep for remeining time of minute
* Also disable BaseCamp tasks
* Only at 23:00 enable wifi and update NTP. Then disable wifi again
* wake on touch

*/