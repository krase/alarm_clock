#define DEBUG 1


#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#undef ARDUHAL_LOG_LEVEL
#define ARDUHAL_LOG_LEVEL ARDUHAL_LOG_LEVEL_DEBUG
#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

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

void getWeatherData();
void show_weather_data();

String CityID = "6548487"; //Sparta, Greece
String APIKEY = "deb68260a4f9589a20607767a38a012c";
const char weather_servername[] ="api.openweathermap.org";  // remote server we will connect to

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

  esp_log_level_set("*", ESP_LOG_WARN);        // set all components to ERROR level
  esp_log_level_set(TAG, ESP_LOG_VERBOSE);

  uint8_t retry = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retry < 20))
  {
    Serial.print(".");
    retry++;
    delay(500);
  }
  Serial.println(" ");
  if (retry < 20)
  {
    connection = true;
    //Serial.println(" ** Wifi connected");
    iot.mqtt.publish("wecker/state", 0, true, "Wifi Connected");
  }
  else
  {
    //Serial.println(" ** Wifi ERROR");
    iot.mqtt.publish("wecker/state", 0, true, "Wifi Not connected");    
  }

  setenv("LANG", "de_DE",  1);
  // Initialize sntp:
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org");

  display.eraseDisplay(true); // Erase with partial update

  struct tm timeinfo; 
  getLocalTime(&timeinfo, 10000);

  DEBUG_PRINTLN(" * SETUP DONE *********");
}



void loop()
{
  static uint16_t day = 380;
  static uint8_t hour = 70;
  static uint8_t minute = 70;
  static uint8_t sec = 0;  
  struct tm timeinfo; 
  static int count = 0;
  const GFXfont* f_big = &FreeMonoBold24pt7b;

  Serial.print("Loop "); Serial.println(count);


  if (count == 0)
  {
      Serial.println("   erase");
      //display.eraseDisplay(true); // Erase with partial update
      display.fillScreen(GxEPD_WHITE);
      display.update();
  }


  // TODO: 
  // Check when last wheather update was done
  // Old parsed data should be stored in nvram
  //getWeatherData();
  show_weather_data();

  count++;

  if ( !getLocalTime(&timeinfo, 10000) )
  {
    DEBUG_PRINTLN("AAAAAAA Failed to get time +++++++++++++");
  }

  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 80);
  display.setFont(f_big);
  display.print(&timeinfo, "%d.%m.%Y\n%R"/*:%S"*/);


  {
    char strftime_buf[20];
    int len = strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y - %R (%z) (%Z)", &timeinfo);
    strftime_buf[len] = 0;
    iot.mqtt.publish("wecker/time", 0, true, strftime_buf);
  }

  if (timeinfo.tm_yday != day)
  {
    display.updateWindow(0, 50, 290, 45);
    day = timeinfo.tm_yday;
  }

  if (timeinfo.tm_hour != hour)
  {
    display.updateWindow(0, 95, 80, 45);
    hour = timeinfo.tm_hour;
  }
  if (timeinfo.tm_min != minute)
  {
    display.updateWindow(80, 95, 80, 50);
    minute = timeinfo.tm_min;
  }
/*  if (sec != timeinfo.tm_sec)
  {
    display.updateWindow(160, 95, 80, 50);
    sec = timeinfo.tm_sec;
  }*/

    const int sleep_sec = 30;
    ESP_LOGI(TAG, "Entering delay for %d seconds", sleep_sec);
    delay(850LL * sleep_sec);
}

/*
void getWeatherData() //client function to send/receive GET request data.
{
  String result ="";
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(weather_servername, httpPort)) 
  {
    return;
  }
  // We now create a URI for the request
  String url = "/data/2.5/forecast?id="+CityID+"&units=metric&cnt=1&APPID="+APIKEY;

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + weather_servername + "\r\n" +
                 "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) 
  {
    if (millis() - timeout > 5000)
    {
      client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server
  while(client.available()) {
    result = client.readStringUntil('\r');
  }

  char jsonArray [result.length()+1];
  result.toCharArray(jsonArray,sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';

  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success())
  {
    Serial.println("parseObject() failed");
  }

  String location = root["city"]["name"];
  String temperature = root["list"]["main"]["temp"];
  String weather = root["list"]["weather"]["main"];
  String description = root["list"]["weather"]["description"];
  String idString = root["list"]["weather"]["id"];
  String timeS = root["list"]["dt_txt"];

  Serial.println(location +": "+temperature+"; "+weather+"; "+description+"; "+timeS);

  long weatherID = idString.toInt();
  Serial.print("  WeatherID: ");
  Serial.println(weatherID);
}
*/

void show_weather_data()
{
  //time_t time_sec;
  struct tm time_info, time_info2;
  #include "weather_data.h"

  DynamicJsonBuffer json_buf;
  JsonObject &root = json_buf.parseObject(json_array);
  if (!root.success())
  {
    Serial.println("parseObject() failed");
  }
  else
  {
    Serial.println("parseObject() OK");
  }

  String location = root["city"]["name"];
  JsonArray& list = root["list"];
  JsonObject& list0 = list[0];

  String temperature = list0["main"]["temp"];  
  String weather = list0["weather"][0]["main"];
  String description = list0["weather"][0]["description"];
  String idString = list0["weather"][0]["id"];
  String timeS = list0["dt_txt"];
  long list0_dt = list0["dt"];

  // Find next day noon
  localtime_r(&list0_dt, &time_info);
  for (int i = 0; i < 10; i++)
  {
    long time_sec = list[i]["dt"];
    localtime_r(&time_sec, &time_info2);
    if (time_info2.tm_yday != time_info.tm_yday && time_info2.tm_hour > 12)
    {
      char strftime_buf[20];
      int len = strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y - %R", &time_info2);
      strftime_buf[len] = 0;
      Serial.println(strftime_buf);      
      break;
    }
  }

  Serial.println(location +": "+temperature+"; "+weather+"; "+description+"; "+timeS);

  long weatherID = idString.toInt();
  Serial.print("  WeatherID: ");
  Serial.println(weatherID);

}

/*
TODO:
* After first time display. disable wifi etc. deep_sleep for remeining time of minute
* Also disable BaseCamp tasks
* Only at 23:00 enable wifi and update NTP. Then disable wifi again
* wake on touch

*/

// Weather:  http://api.openweathermap.org/data/2.5/weather?id=6548487&&APPID=deb68260a4f9589a20607767a38a012c&units=metric
// Forecast: http://api.openweathermap.org/data/2.5/forecast?id=6548487&&APPID=deb68260a4f9589a20607767a38a012c&units=metric