#include <Adafruit_BME680.h>
//#include <bhy.h>
#include <time.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include <GyverOLED.h>
#include <AdvancedOximeter.h>
#include <math.h>
//Icons converted from https://www.iconarchive.com/show/material-battery-icons-by-pictogrammers.html
#include <batteryicons.h>
#include <invertedbatteryicons.h>
#include <appicons.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;

#define battery_pin 0
#define divider_on false
#define BHI_INT 8
#define Button1 5
#define Button2 6
#define Haptic 2

Adafruit_BME680 bme;
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
AdvancedOximeter oximeter;

float batteryReads[20];
int listcountcurrent = 0;
int appSelect = 1;
int weatherAppselect = 1;
float heartrateread = 0;

int hour24,hour12,minute,second,day,month,year,weekday; //Make variables to write to when reading from the onboard rtc

long return_start; //globals for the long press state machine
long return_last_state;
int return_length;

bool isPM, lastButtonstate;
bool inHome, inMenu, inWeather, inStopwatch, inAlarm, inCompass, inGolf, inFind, inSettings; 
bool sw = true;
bool firstfetch = true;
int64_t lastfetch;
String payload;

const char* day1;
const char* day2;
const char* day3;
const char* day4;
const char* day5;
const char* day6;
const char* day7;
float day1min, day1max, day2min, day2max, day3min, day3max, day4min, day4max, day5min, day5max, day6min, day6max, day7min, day7max;
float day1p,day2p,day3p,day4p,day5p,day6p,day7p;

float today0,today1,today2,today3,today4,today5,today6,today7,today8,today9,today10,today11,today12,today13,today14,today15,today16,today17,today18,today19,today20,today21,today22,today23;
float* hours[24] = {
  &today0, &today1, &today2, &today3, &today4, &today5, &today6, &today7,
  &today8, &today9, &today10, &today11, &today12, &today13, &today14, &today15,
  &today16, &today17, &today18, &today19, &today20, &today21, &today22, &today23
};
int hours_to_show;
int pagestoshow;

void setup() {
  Serial.begin(9600);

  pinMode(battery_pin, INPUT);
  pinMode(Button1, INPUT_PULLUP);
  pinMode(Button2, INPUT_PULLUP);
  pinMode(Haptic, OUTPUT);

  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); //Steal time from ntp server with the correct config

  Wire.begin(8,9);

  oled.init();
  oximeter.begin();
  inHome=inMenu=inWeather=inStopwatch=inAlarm=inCompass=inGolf=inFind=inSettings=false; 
  inHome = true;
}

void printlocaltime(){
  struct tm timeinfo; //Have the time library create a usable struct to get time variables from
  if(!getLocalTime(&timeinfo)){ //Try to get teh local time from the rtc, return if that fails
    return;
  }
//takes from the struct from time.h...
  hour24   = timeinfo.tm_hour;        // the 24hr time
  hour12   = (timeinfo.tm_hour % 12 == 0) ? 12 : timeinfo.tm_hour % 12; //the 12 hour time, uses % to convert if nessesary from 24hr time
  minute   = timeinfo.tm_min; //the minute 
  second   = timeinfo.tm_sec; //the second
  day      = timeinfo.tm_mday; //the day of the month
  month    = timeinfo.tm_mon + 1;     // the month of the year (-1 for some reeason, this corrects that)
  year     = timeinfo.tm_year + 1900; // the years since 1900
  weekday  = timeinfo.tm_wday;        // the day of the week (starting at sunday with 0)
  isPM    = timeinfo.tm_hour >= 12; //a bool telling if its the afternoon
}

void drawtext(int scale=1, double x=0, double y=0, const char* text="Hello World!"){
  y = y/8;
  oled.setScale(scale);
  oled.setCursor(x, y);
  oled.print(text);
}

double rightalign(const char* Str, int offset, int scale){
  return 127 - ((strlen(Str) + offset) * 6 * scale);
}
double rightalign(float floaty, int offset, int scale){
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%.0f", floaty);
  return 127 - ((strlen(buffer) + offset) * 6 * scale);
}
double rightalignint(int inty, int offset, int scale){
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", inty);
  return 127 - ((strlen(buffer) + offset) * 6 * scale);
}

void tochar(float value, const char* suffix, int scale, double x, double y){
  char Str[32];
  snprintf(Str, sizeof(Str), "%.1f%s", value, suffix);
  drawtext(scale,x,y,Str);
}
void tocharinverted(float value, const char* suffix, int scale, double x, double y){
  char Str[32];
  snprintf(Str, sizeof(Str), "%.1f%s", value, suffix);
  oled.invertText(true);
  drawtext(scale,x,y,Str);
  oled.invertText(false);
}
void tocharint(int value, const char* suffix, int scale, double x, double y){
  char Str[32];
  snprintf(Str, sizeof(Str), "%d%s", value, suffix);
  drawtext(scale,x,y,Str);
}

struct BattIcon {
  const uint8_t* bitmap;
  uint8_t width;
};
static const BattIcon BATT_ICONS[][11] = {
  { // Normal
    {BATT_0,   BATT_0_W},   {BATT_10,  BATT_10_W},
    {BATT_20,  BATT_20_W},  {BATT_30,  BATT_30_W},
    {BATT_40,  BATT_40_W},  {BATT_50,  BATT_50_W},
    {BATT_60,  BATT_60_W},  {BATT_70,  BATT_70_W},
    {BATT_80,  BATT_80_W},  {BATT_90,  BATT_90_W},
    {BATT_100, BATT_100_W}
  },
  { // Inverted
    {I_BATT_0,   I_BATT_0_W},   {I_BATT_10,  I_BATT_10_W},
    {I_BATT_20,  I_BATT_20_W},  {I_BATT_30,  I_BATT_30_W},
    {I_BATT_40,  I_BATT_40_W},  {I_BATT_50,  I_BATT_50_W},
    {I_BATT_60,  I_BATT_60_W},  {I_BATT_70,  I_BATT_70_W},
    {I_BATT_80,  I_BATT_80_W},  {I_BATT_90,  I_BATT_90_W},
    {I_BATT_100, I_BATT_100_W}
  }
};

void battery(float percent, int Inverted = 1) {
  uint8_t idx = constrain((int)percent, 0, 100) / 10;
  const BattIcon& icon = BATT_ICONS[Inverted][idx];
  oled.drawBitmap(1, 1, icon.bitmap, icon.width, 16);
  Inverted ? tocharinverted(percent, "%", 1, 18, 8)
          : tochar(percent, "%", 1, 18, 8);
}

float readBattery(){
  digitalWrite(divider_on, true);
  float voltage = (analogReadMilliVolts(battery_pin) / 1000.0f) * 2.0f;
  float percentage = 0.0000945484 * pow(voltage, 9.7559);
  digitalWrite(divider_on, false);
  return percentage;
}

float averageArray(){
  float rollingnumber = 0;
  for (int x = 0; x < listcountcurrent; x++) {
      rollingnumber = rollingnumber + batteryReads[x];
    }
  rollingnumber = rollingnumber / listcountcurrent;
  //Return the average of the data in the array
  return rollingnumber;
}

void manageInput(float newBattRead) {
  //If there are less than 20 readings, write to the newest available slot
  //Otherwise, move all values down 1 index, allowing 19 to be the newest and 0 to be the oldest, getting replaced
  if (listcountcurrent <= 19) {
    batteryReads[listcountcurrent] = readBattery();
    listcountcurrent++;
  }
  else {
    for (int i = 1; i <= 19; i++) {
      batteryReads[i - 1] = batteryReads[i];
    }
      listcountcurrent = 19;
      batteryReads[listcountcurrent] = readBattery();
  }
}

const char* buildTime(){
  static char buffer[32];
  sprintf(buffer, "%02d:%02d:%02d", hour12, minute, second);
  return buffer;
}

const char* buildDate(){
  static char buffer[32];
  sprintf(buffer, "%02d/%02d/%04d", month, day, year);
  return buffer;
}

void home(float batteryLife, float temp, const char* time, const char* date, float bpm, int steps) {
  oled.fill(0);
  drawtext(2,17,27,time);
  drawtext(1,35,43,date);

  tochar(bpm, " BPM", 1, 2, 60);
  tocharint(steps, " Stps", 1, rightalignint(steps, 5, 1), 60);
  tochar(temp, " F", 1, rightalign(temp, 4, 1), 8);
  battery(batteryLife, 0);
}

void menu(float batterylife){
  oled.fill(0);
  oled.rect(0,0,128,20,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.drawBitmap(13, 24, ICON_WEATHER, WEATHER_W, 8);

  oled.drawBitmap(13, 34, ICON_STOPWATCH, STOPWATCH_W, 8);

  oled.drawBitmap(13, 44, ICON_COMPASS, COMPASS_W, 8);

  oled.drawBitmap(13, 54, ICON_ALARM, ALARM_W, 8);

  oled.drawBitmap(73, 24, ICON_GOLF, GOLF_W, 8);

  oled.drawBitmap(73, 34, ICON_FINDPHONE, FINDPHONE_W, 8);

  oled.drawBitmap(73, 44, ICON_SETTINGS, SETTINGS_W, 8);

  if (digitalRead(Button1) == false && digitalRead(Button2) == false){
    if (appSelect == 1){
      weatherAppselect = 1;
      weatherAPP(averageArray());
    }
    if (appSelect == 2){
      stopwatchAPP(averageArray());
    }
    if (appSelect == 3){
      compassAPP(averageArray());
    }
    if (appSelect == 4){
      alarmAPP(averageArray());
    }
    if (appSelect == 5){
      golfAPP(averageArray());
    }
    if (appSelect == 6){
      findphoneAPP(averageArray());
    }
    if (appSelect == 7){
      settingsAPP(averageArray());
    }
  }

  else if (digitalRead(Button2) == false){
    if (appSelect == 7){
      appSelect = 7;
    }
    else{
      appSelect++;
    }
  }
  else if (digitalRead(Button1) == false){
    if (appSelect == 1){
      appSelect = 1;
    }
    else{
      appSelect--;
    }
  }

  if (appSelect == 1){
    oled.drawBitmap(5, 24, ICON_CURSOR, CURSOR_W, 8);
  }
  if (appSelect == 2){
    oled.drawBitmap(5, 34, ICON_CURSOR, CURSOR_W, 8);
  }
  if (appSelect == 3){
    oled.drawBitmap(5, 44, ICON_CURSOR, CURSOR_W, 8);
  }
  if (appSelect == 4){
    oled.drawBitmap(5, 54, ICON_CURSOR, CURSOR_W, 8);
  }
  if (appSelect == 5){
    oled.drawBitmap(65, 24, ICON_CURSOR, CURSOR_W, 8);
  }
  if (appSelect == 6){
    oled.drawBitmap(65, 34, ICON_CURSOR, CURSOR_W, 8);
  }
  if (appSelect == 7){
    oled.drawBitmap(65, 44, ICON_CURSOR, CURSOR_W, 8);
  }
}

const char* weekdayConverter(int dayint){
  int daycheck = dayint % 7;
  if (daycheck == 0){
    return "Sun";
  }
  if (daycheck == 1){
    return "Mon";
  }
  if (daycheck == 2){
    return "Tue";
  }
  if (daycheck == 3){
    return "Wed";
  }
  if (daycheck == 4){
    return "Thurs";
  }
  if (daycheck == 5){
    return "Fri";
  }
  if (daycheck == 6){
    return "Sat";
  }
  else{
    return "";
  }
}

void drawBars(int startindex, float lowest, float highest, int hours_left)  {
  int barsCandraw;
  if (hours_left < 4){
    barsCandraw = 4;
  }
  else{
    barsCandraw = hours_left;
  } 
  if (highest == lowest){
    highest++;
  }
    for (int i = 0; i < barsCandraw; i++){
      int id = startindex + i;
      if (id >= 24){
        return;
      }
      float currenthour = *hours[id];
      float scaledTemp = (((35 - 0)*(currenthour - (lowest)))/(highest-(lowest)));
      float x = 4 + (i * 30);
      oled.rect(x,55,(x+24),(55-scaledTemp),1);
      if (id<24) tocharinverted(currenthour, "", 1, x, 55);
      if (id<24){
        oled.invertText(true);
        tocharint((id % 12), "", 1, (x+8), 24);
        oled.invertText(false);
      }
    }
}

void weatherAPP(float batterylife){
  inMenu = false;
  inWeather = true;
  oled.fill(0);
  oled.rect(0,0,128,17,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  drawtext(1, rightalign("Weather", 0, 1), 8, "Weather");
  oled.invertText(false);

  if (esp_timer_get_time() - lastfetch >= 120000000 || firstfetch == true){
    firstfetch = false;
    lastfetch = esp_timer_get_time();
    HTTPClient client;
    client.begin("https://api.open-meteo.com/v1/forecast?latitude=42.0104798&longitude=-71.301051&hourly=temperature_2m,precipitation&daily=temperature_2m_max,temperature_2m_min,precipitation_sum&temperature_unit=fahrenheit&forecast_days=8&timezone=auto");
    int error = client.GET();
    if (error == HTTP_CODE_OK){
      payload = client.getString();
    }
    client.end();

    JsonDocument doc;
    DeserializationError dse = deserializeJson(doc, payload);

    if (dse){
      inWeather = false;
      inMenu = true;
      oled.clear();
      oled.fill(1);
      return;
    }

    day1max = doc["daily"]["temperature_2m_max"][1];
    day1min = doc["daily"]["temperature_2m_min"][1];
    day2max = doc["daily"]["temperature_2m_max"][2];
    day2min = doc["daily"]["temperature_2m_min"][2];
    day3max = doc["daily"]["temperature_2m_max"][3];
    day3min = doc["daily"]["temperature_2m_min"][3];
    day4max = doc["daily"]["temperature_2m_max"][4];
    day4min = doc["daily"]["temperature_2m_min"][4];
    day5max = doc["daily"]["temperature_2m_max"][5];
    day5min = doc["daily"]["temperature_2m_min"][5];
    day6max = doc["daily"]["temperature_2m_max"][6];
    day6min = doc["daily"]["temperature_2m_min"][6];
    day7max = doc["daily"]["temperature_2m_max"][7];
    day7min = doc["daily"]["temperature_2m_min"][7];

    day1p = doc["daily"]["precipitation_sum"][1];
    day2p = doc["daily"]["precipitation_sum"][2];
    day3p = doc["daily"]["precipitation_sum"][3];
    day4p = doc["daily"]["precipitation_sum"][4];
    day5p = doc["daily"]["precipitation_sum"][5];
    day6p = doc["daily"]["precipitation_sum"][6];
    day7p = doc["daily"]["precipitation_sum"][7];

    today0 = doc["hourly"]["temperature_2m"][0];
    today1  = doc["hourly"]["temperature_2m"][1];
    today2  = doc["hourly"]["temperature_2m"][2];
    today3  = doc["hourly"]["temperature_2m"][3];
    today4  = doc["hourly"]["temperature_2m"][4];
    today5  = doc["hourly"]["temperature_2m"][5];
    today6  = doc["hourly"]["temperature_2m"][6];
    today7  = doc["hourly"]["temperature_2m"][7];
    today8  = doc["hourly"]["temperature_2m"][8];
    today9  = doc["hourly"]["temperature_2m"][9];
    today10 = doc["hourly"]["temperature_2m"][10];
    today11 = doc["hourly"]["temperature_2m"][11];
    today12 = doc["hourly"]["temperature_2m"][12];
    today13 = doc["hourly"]["temperature_2m"][13];
    today14 = doc["hourly"]["temperature_2m"][14];
    today15 = doc["hourly"]["temperature_2m"][15];
    today16 = doc["hourly"]["temperature_2m"][16];
    today17 = doc["hourly"]["temperature_2m"][17];
    today18 = doc["hourly"]["temperature_2m"][18];
    today19 = doc["hourly"]["temperature_2m"][19];
    today20 = doc["hourly"]["temperature_2m"][20];
    today21 = doc["hourly"]["temperature_2m"][21];
    today22 = doc["hourly"]["temperature_2m"][22];
    today23 = doc["hourly"]["temperature_2m"][23];

    day1 = weekdayConverter(weekday + 1);
    day2 = weekdayConverter(weekday + 2);
    day3 = weekdayConverter(weekday + 3);
    day4 = weekdayConverter(weekday + 4);
    day5 = weekdayConverter(weekday + 5);
    day6 = weekdayConverter(weekday + 6);
    day7 = weekdayConverter(weekday + 7);
  }

  float highest = 0;
  float lowest = 100;
  for(int i = 0; i < 24; i++){
    if (*hours[i] >= highest){
      highest = *hours[i];
    }
    if (*hours[i] <= lowest){
      lowest = *hours[i];
    }
  }

  hours_to_show = 24 - hour24;
  pagestoshow = ceil(hours_to_show / 4.0);

  if (digitalRead(Button2) == false){
    if (weatherAppselect == (pagestoshow + 4)){
      weatherAppselect = 1;
    }
    else{
      weatherAppselect++;
    }
  }
  else if (digitalRead(Button1) == false){
    if (weatherAppselect == 1){
      weatherAppselect = (pagestoshow + 5);
    }
    else{
      weatherAppselect--;
    }
  }

  if(weatherAppselect == 1){
    oled.rect(0,55,42,64,1);
    oled.invertText(true);
    drawtext(1, 1, 56, "Current");
    oled.invertText(false);

    tochar(bme.temperature, "F", 1, 5, 24);
    //bme.temperature
    drawtext(1,0,40," Temp");
    //bme.humidity
    tochar(bme.humidity, "%", 1, 50, 24);
    drawtext(1,39,40,"Humidity");
    //(bme.pressure * 0.0145038)
    tochar((bme.pressure * 0.0145038), "PSI", 1, rightalign((bme.pressure * 0.0145038), 5, 1), 24);
    drawtext(1,98,40,"BMP");
  }
  else{
    oled.rect(0,55,42,64,OLED_STROKE);
    drawtext(1, 1, 56, "Current");
  }

  if (weatherAppselect >= 2 && weatherAppselect <= (1 + pagestoshow)){
      oled.invertText(true);
      oled.rect(42,55,78,64,1);
      drawtext(1,46,56,"Today");
      oled.invertText(false);

  int startIndex = hour24;
  int todayPageNum = weatherAppselect - 1;

  for(int i = 0; i < pagestoshow; i++){
    if (todayPageNum == (i + 1)){
      drawBars(startIndex + (i * 4),lowest,highest, hours_to_show);
    }
  }
  }
  else{
    oled.rect(42,55,78,64,OLED_STROKE);
    drawtext(1,46,56,"Today");
  }
  if (weatherAppselect == (2 + pagestoshow)|| weatherAppselect == (3 + pagestoshow) || weatherAppselect == (4 + pagestoshow) || weatherAppselect == (5 + pagestoshow)){
    oled.invertText(true);
    oled.rect(78,55,128,64,1);
    drawtext(1,rightalign("SevenDay", 0, 1),56,"SevenDay");
    oled.invertText(false);
    drawtext(1, 50, 24, "High");
    drawtext(1, 90, 24, "Low");
  }

  else{
    oled.rect(78,55,128,64,OLED_STROKE);
    drawtext(1,rightalign("SevenDay", 0, 1),56,"SevenDay");
  }
  
  if (weatherAppselect == ((2 + pagestoshow))){
    drawtext(1, 2, 32, day1);
    tochar(day1max, "F", 1, 50, 32);
    tochar(day1min, "F", 1, 90, 32);

    drawtext(1, 2, 40, day2);
    tochar(day2max, "F", 1, 50, 40);
    tochar(day2min, "F", 1, 90, 40);
  }
  if (weatherAppselect == (pagestoshow + 3)){
    drawtext(1, 2, 32, day3);
    tochar(day3max, "F", 1, 50, 32);
    tochar(day3min, "F", 1, 90, 32);

    drawtext(1, 2, 40, day4);
    tochar(day4max, "F", 1, 50, 40);
    tochar(day4min, "F", 1, 90, 40);
  }

  if (weatherAppselect == (pagestoshow + 4)){
    drawtext(1, 2, 32, day5);
    tochar(day5max, "F", 1, 50, 32);
    tochar(day5min, "F", 1, 90, 32);

    drawtext(1, 2, 40, day6);
    tochar(day6max, "F", 1, 50, 40);
    tochar(day6min, "F", 1, 90, 40);
  }

  if (weatherAppselect == (pagestoshow + 5)){
    drawtext(1, 2, 32, day7);
    tochar(day7max, "F", 1, 50, 32);
    tochar(day7min, "F", 1, 90, 32);
  }
  
  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inWeather = false;
      inHome = true;
  }
  oled.rect(120,18,128,54,0);
}

void stopwatchAPP(float batterylife){
  inMenu = false;
  inStopwatch = true;
  oled.fill(0);
  oled.rect(0,0,128,20,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  if (sw == true){
    drawtext(1, rightalign("Stopwatch", 0, 1), 8, "Stopwatch");
  }
  else{
    drawtext(1, rightalign("Timer", 0, 1), 8, "Timer");
  }
  oled.invertText(false);

  //TODO: Take the esp_timer_get_time() and store it until the start stop is pressed

  //TODO: Subtract the initial from the current esp_timer_get_time() And display that, overflowing to minutes if needed

  //TODO: Add menu tabs to switch to timer

  //TODO: Use Buttoms to set a slider for the timer duration

  //TODO: Allow the timer to run, then haptic like nuts when it is over

  //TODO: Clear all the new variables
  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inStopwatch = false;
      inHome = true;
  }
}

void alarmAPP(float batterylife){
  inMenu = false;
  inAlarm = true;
  oled.fill(0);
  oled.rect(0,0,128,20,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  drawtext(1, rightalign("Alarms", 0, 1), 8, "Alarms");
  oled.invertText(false);
  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inAlarm = false;
      inHome = true;
  }
}

void compassAPP(float batterylife){
  inMenu = false;
  inCompass = true;
  oled.fill(0);
  oled.rect(0,0,128,20,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  drawtext(1, rightalign("Compass", 0, 1), 8, "Compass");
  oled.invertText(false);
  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inCompass = false;
      inHome = true;
  }

  //TODO: Set up/steal a compass interface

  //TODO: Read from the magnometer and rotate the compass dial as expected
}

void golfAPP(float batterylife){
  inMenu = false;
  inGolf = true;
  oled.fill(0);
  oled.rect(0,0,128,20,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  drawtext(1, rightalign("Golf", 0, 1), 8, "Golf");
  oled.invertText(false);
  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inGolf = false;
      inHome = true;
  }
}

void findphoneAPP(float batterylife){
  inMenu = false;
  inFind = true;
  oled.fill(0);
  oled.rect(0,0,128,20,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  drawtext(1, rightalign("Find Phone", 0, 1), 8, "Find Phone");
  oled.invertText(false);

  //TODO: Setup an HTTP client call to send an emergency priority ring to my phone
  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inFind = false;
      inHome = true;
  }
}

void settingsAPP(float batterylife){
  inMenu = false;
  inSettings = true;
  oled.fill(0);
  oled.rect(0,0,128,20,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  drawtext(1, rightalign("Settings", 0, 1), 8, "Settings");
  oled.invertText(false);

  //TODO: Import the menu interface again - the Icons

  //TODO: Add settings for things like wifi networks, flashlight brightness, etc

  //TODO: Write to Preferences ROM to save these settings
  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inSettings = false;
      inHome = true;
  }
}

bool appCheck(float batterylife){
  if (inHome == true){
    home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
    return true;
  }
  if (inMenu == true){
    menu(averageArray());
    return true;
  }
  if (inWeather == true){
    weatherAPP(averageArray());
    return true;
  }
  if (inStopwatch == true){
    stopwatchAPP(averageArray());
    return true;
  }
  if (inAlarm == true){
    alarmAPP(averageArray());
    return true;
  }
  if (inCompass == true){
    compassAPP(averageArray());
    return true;
  }
  if (inGolf == true){
    golfAPP(averageArray());
    return true;
  }
  if (inFind == true){
    findphoneAPP(averageArray());
    return true;
  }
  if (inSettings == true){
    settingsAPP(averageArray());
    return true;
  }
}

int returnButtonlogic(int button, long starting, long laststate, int length){ //Returns an int based off how a button is pressed. Long press = 1, Short Press = 2
  if (digitalRead(button) == false && lastButtonstate == false){
    start = millis();
    lastButtonstate = true;
  }
  if (lastButtonstate == true && digitalRead(button) == true){
    lastButtonstate = false;
    if (millis() - start <= length){
      return 2;
    }
    else{
      return 1;
    }
  }
  return 0;
}

void loop() {
  oled.fill(0);
  printlocaltime();
  manageInput(readBattery());

  appCheck(averageArray());

  if (returnButtonlogic(Button1,return_start,return_last_state,return_length) == 1){
    if (inMenu == true){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inMenu = false;
      inHome = true;
    }
    else{
      menu(averageArray());
      inHome = false;
      inMenu = true;
    }
  }

  oled.update();
}
