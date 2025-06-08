#include <Arduino.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>

#include "battery.h"
#include "DateTimeUtil.h"
#include "Weather.h"
#include "images/background.h"
#include "images/cloudy.h"
#include "images/rainy.h"
#include "images/rainyandcloudy.h"
#include "images/snow.h"
#include "images/sunny.h"
#include "images/sunnyandcloudy.h"
#include "images/lowbattery.h"
#include "time.h"

#define SHOW_LAST_UPDATED true // 天気を更新した時刻を表示するかどうか
#define SHOW_BATTERY_CAPACITY true // 電池残量を表示するかどうか
#define LOW_BATTERY_THRETHOLD 20 // バッテリー残量低下と判断するバッテリー残量 (0 - 100)

// この時刻より前は今日の時刻を表示し、この時刻以降は明日の時刻を表示する
const int8_t boundaryOfDate = 18;

// WiFi接続情報を指定する場合は有効にする 無効の場合は前回接続したWiFiの情報を使用
// #define WIFI_SSID "****"
// #define WIFI_PASS "****"

// NTPによる時刻補正を常に行う場合は有効にする 無効の場合でもEXTボタンを押しながら起動することで時刻補正を行う
#define ADJUST_RTC_NTP

// 天気を取得する間隔(秒) 1 - 10800 の間で指定
// 気象庁から取得できるJSONの仕様を鑑みて、午前6時と午後6時に取得する方針に変更
// #define UPDATE_INTERVAL 10800

const char* endpoint = "https://www.jma.go.jp/bosai/forecast/data/forecast/130000.json";
const char* region = "東京地方";

const char* NTP_SERVER = "192.168.1.198";
const char* NTP_TIMEZONE = "JST-9";

/*
// --------------
*/

M5Canvas backgroundSprite,dateSprite,weatherSprite,temperatureSprite,rainfallChanceSprite;
JsonDocument weatherInfo;

void getJson() {
    if ((WiFi.status() == WL_CONNECTED)) {
        HTTPClient http;
        http.begin(endpoint);
        int httpCode = http.GET();
        if (httpCode > 0) {
            String jsonString = http.getString();
            //jsonオブジェクトの作成
            deserializeJson(weatherInfo, jsonString);
        } else {
            Serial.println("Error on HTTP request");
        }
        http.end(); //リソースを解放
    }
}

Weather getWeatherOfDay(m5::rtc_date_t date) {
    Weather weather = {"",255,-255,255,-255};
    JsonArray data = weatherInfo[0]["timeSeries"];
    String dateString = dateToString(date);

    // areasのindexを取得
    uint8_t areaIndex = 0;
    for(int i = 0; i < data[0]["areas"].size();i++) {
        if(data[0]["areas"][i]["area"]["name"].as<String>() == region) {
            areaIndex = i;
            break;
        }
    }

    Serial.printf("areaIndex : %d\n", areaIndex);

    // 天気を取得
    JsonObject weatherData = data[0];
    for(int i = 0;i < weatherData["timeDefines"].size();i++) {
        JsonVariant v = weatherData["timeDefines"][i];
        if(v.as<String>().startsWith(dateString)) {
            // weathersのi番目がこの日の天気
            weather.weather = weatherData["areas"][areaIndex]["weathers"][i].as<String>();
            break;
        }
    }

   // 降水確率を取得
   JsonObject rainfallData = data[1];
   for(int i = 0;i < rainfallData["timeDefines"].size();i++) {
       JsonVariant v = rainfallData["timeDefines"][i];
       if(v.as<String>().startsWith(dateString)) {
           // popsのi番目がこの日の気温の一つ
           int pop = rainfallData["areas"][areaIndex]["pops"][i].as<int>();
           if(weather.minRainfallChance > pop) weather.minRainfallChance = pop;
           if(weather.maxRainfallChance < pop) weather.maxRainfallChance = pop;
       } 
   }

   // 気温を取得
   JsonObject temperatureData = data[2];
   for(int i = 0;i < temperatureData["timeDefines"].size();i++) {
       JsonVariant v = temperatureData["timeDefines"][i];
       if(v.as<String>().startsWith(dateString)) {
           // tempsのi番目がこの日の気温の一つ
           int temp = temperatureData["areas"][areaIndex]["temps"][i].as<int>();
           if(weather.minTemperature > temp) weather.minTemperature = temp;
           if(weather.maxTemperature < temp) weather.maxTemperature = temp;
       } 
   }

    return weather;
}

void drawTemperature(String maxTemperature, String minTemperature) {
    Serial.printf("Temperature: max: %s, min: %s\n", maxTemperature.c_str(), minTemperature.c_str());
    temperatureSprite.createSprite(200, 200);
    temperatureSprite.fillSprite(TFT_TRANSPARENT);
    temperatureSprite.setTextColor(TFT_BLACK, TFT_WHITE);
    temperatureSprite.setFont(&fonts::AsciiFont8x16);;
    temperatureSprite.drawString(maxTemperature, 63, 145);
    temperatureSprite.drawString(minTemperature, 63, 168);
    temperatureSprite.pushSprite(&M5.Display, 0, 0, TFT_TRANSPARENT);
    temperatureSprite.deleteSprite();
}

void drawRainfallChance(String maxRainfallChance,String minRainfallChance) {
    Serial.printf("Rainfall Chance: max: %s, min: %s\n", maxRainfallChance.c_str(), minRainfallChance.c_str());
    rainfallChanceSprite.createSprite(200, 200);
    rainfallChanceSprite.fillSprite(TFT_TRANSPARENT);
    rainfallChanceSprite.setTextColor(TFT_BLACK, TFT_WHITE);
    rainfallChanceSprite.setFont(&fonts::AsciiFont8x16);;
    rainfallChanceSprite.drawString(maxRainfallChance, 142, 145);
    rainfallChanceSprite.drawString(minRainfallChance, 142, 168);
    rainfallChanceSprite.pushSprite(&M5.Display, 0, 0, TFT_TRANSPARENT);
    rainfallChanceSprite.deleteSprite();
}

void drawDate(String date) {
    Serial1.printf("Date: %s\n", date.c_str());
    dateSprite.createSprite(200, 200);
    dateSprite.fillSprite(TFT_TRANSPARENT);
    dateSprite.setTextColor(TFT_BLACK, TFT_WHITE);
    dateSprite.setFont(&fonts::AsciiFont8x16);;
    dateSprite.drawString(date, 60, 16);
    // Draw last updated time
    if(SHOW_LAST_UPDATED){
        m5::rtc_date_t RtcDate;
        m5::rtc_time_t RtcTime;
        M5.Rtc.getTime(&RtcTime);
        M5.Rtc.getDate(&RtcDate);
        String nowString = String("Updated: ") + dateTimeToString(RtcDate,RtcTime);
        dateSprite.drawString(nowString, 0, 184);
    }
    
    //Draw battery capacity
    if(SHOW_BATTERY_CAPACITY) 
            dateSprite.drawString(String(getBatCapacity()) + String("%"), 176, 184);
    dateSprite.pushSprite(&M5.Display, 0, 0, TFT_TRANSPARENT);
    dateSprite.deleteSprite();
}

void drawLowbattery(){
    backgroundSprite.createFromBmp((const uint8_t*)image_background, image_background_len);
    backgroundSprite.setTextColor(TFT_BLACK, TFT_WHITE);
    backgroundSprite.setFont(&fonts::AsciiFont8x16);;
    backgroundSprite.drawString("Low Battery", 56, 16);
    backgroundSprite.pushSprite(&M5.Display, 0, 0);
    backgroundSprite.deleteSprite();
    weatherSprite.createFromBmp((const uint8_t*)image_lowbattery, image_lowbattery_len);
    weatherSprite.pushSprite(&M5.Display, 46, 36);
    weatherSprite.deleteSprite();
}

void drawWeather(Weather weather) {
    Serial.printf("Weather: %s, minTemp: %d, maxTemp: %d, minRain: %d, maxRain: %d\n",weather.weather,weather.minTemperature,weather.maxTemperature,weather.minRainfallChance,weather.maxRainfallChance);
    backgroundSprite.createFromBmp((const uint8_t*)image_background, image_background_len);
    backgroundSprite.pushSprite(&M5.Display, 0, 0);
    backgroundSprite.deleteSprite();
    String weatherString = weather.weather;
    if (weatherString.indexOf("雨") != -1) {
        if (weatherString.indexOf("くもり") != -1) {
            weatherSprite.createFromBmp((const uint8_t*)image_rainyandcloudy,image_rainyandcloudy_len);
        } else {
            weatherSprite.createFromBmp((const uint8_t*)image_rainy,image_rainy_len);
        }
    } else if (weatherString.indexOf("晴") != -1) {
        if (weatherString.indexOf("くもり") != -1) {
            weatherSprite.createFromBmp((const uint8_t*)image_sunnyandcloudy,image_sunnyandcloudy_len);
        } else {
            weatherSprite.createFromBmp((const uint8_t*)image_sunny,image_sunny_len);
        }
    } else if (weatherString.indexOf("雪") != -1) {
            weatherSprite.createFromBmp((const uint8_t*)image_snow,image_snow_len);
    } else if (weatherString.indexOf("くもり") != -1) {
            weatherSprite.createFromBmp((const uint8_t*)image_cloudy,image_cloudy_len);
    }
    weatherSprite.pushSprite(&M5.Display, 46, 36);
    weatherSprite.deleteSprite();
    drawTemperature(String(weather.maxTemperature), String(weather.minTemperature));
    drawRainfallChance(String(weather.maxRainfallChance),String(weather.minRainfallChance));
}

void adjustRTC() {
    Serial.print("RTC adjusting...");
    tm timeinfo;
    time_t now;
    m5::rtc_time_t RtcTime;
    m5::rtc_date_t RtcDate;
    configTzTime(NTP_TIMEZONE, NTP_SERVER);
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
      Serial.print('.');
      delay(500);
    }
    Serial.println("done.");
    time(&now);
    localtime_r(&now, &timeinfo);
    char time_output[30];
    strftime(time_output, 30, "NTP: %a  %d-%m-%y %T", localtime(&now));
    Serial.println(time_output);
    convertTimeToRTC(&RtcTime, timeinfo);
    convertDateToRTC(&RtcDate, timeinfo);
    M5.Rtc.setTime(&RtcTime);
    M5.Rtc.setDate(&RtcDate);
}

void setup() {
    auto config = M5.config();
    config.serial_baudrate = 115200;
    M5.begin(config);
    M5.Power.begin();
    M5.Display.begin();
    M5.Display.setAutoDisplay(false);
    M5.Display.setRotation(0);
    
    // バッテリー残量が低下している場合はそのことを表示し、自動更新はしない
    if(getBatCapacity() < LOW_BATTERY_THRETHOLD) {
        drawLowbattery();
        delay(1000);
        M5.Power.powerOff();
        return;
    }

#ifdef WIFI_SSID
    WiFi.begin(WIFI_SSID,WIFI_PASS);
#else
    WiFi.begin();
#endif

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the WiFi network");
    
    // EXTが押されている時は時刻合わせをする
    if(M5.BtnEXT.isPressed()) adjustRTC();
    else {
#ifdef ADJUST_RTC_NTP
        adjustRTC();
#endif
    }

    getJson();
    WiFi.disconnect();

    m5::rtc_date_t RtcDate;
    m5::rtc_time_t RtcTime;
    M5.Rtc.getTime(&RtcTime);
    M5.Rtc.getDate(&RtcDate);
    if(RtcTime.hours >= boundaryOfDate) offsetDate(&RtcDate,1); // RtcDateを翌日に設定

    drawWeather(getWeatherOfDay(RtcDate));
    drawDate(dateToString(RtcDate));
    M5.Display.display();

    // 今が午前なら次は午後6時に、今が午後なら次は午前6時に起動
    m5::rtc_date_t RtcDateToWake;
    m5::rtc_time_t RtcTimeToWake;
    M5.Rtc.getTime(&RtcTimeToWake);
    M5.Rtc.getDate(&RtcDateToWake);
    if(RtcTime.hours < 12) {
        RtcTimeToWake.hours = 18;
    }
    else {
        offsetDate(&RtcDateToWake, 1);
        RtcTimeToWake.hours = 6;
    }
    M5.Power.timerSleep(RtcDateToWake, RtcTimeToWake);
    M5.Power.powerOff();
}

void loop() {
    // USBによる電源供給が行われている場合はShutdownしても電源が切れないが、
    // RTCの設定は保持されている
    delay(1000);
}


