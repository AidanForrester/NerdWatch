#include <time.h>
#include <esp_sntp.h>
#include <WiFi.h>

#include <GyverOLED.h>
#include <AdvancedOximeter.h>
#include <Deneyap_9EksenAtaletselOlcumBirimi.h>  
#include <Adafruit_BME680.h>
#include <Adafruit_NeoPixel.h>

#include <bhy.h>
//if WOKWI 
#include "Bosch_PCB_7183_di03_BMI160_BMM150-7183_di03.2.1.11696_170103.h"
//#include "firmware\Bosch_PCB_7183_di03_BMI160_BMM150-7183_di03.2.1.11696_170103.h"

#include <math.h>
#include <cmath>
#include <ArduinoJson.h>
#include <HTTPClient.h>
//Icons converted from https://www.iconarchive.com/show/material-battery-icons-by-pictogrammers.html
#include "batteryicons.h"
#include "invertedbatteryicons.h"
#include "nerdwatch_icons.h"

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;

#define battery_pin 0
#define divider_on false
#define Button1 5
#define Button2 6
#define Haptic 2
#define BHI_INT 4
#define fl 3

Adafruit_BME680 bme;
MAGNETOMETER Magne;    
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
AdvancedOximeter oximeter;
BHYSensor bhi;
Adafruit_NeoPixel light(1,fl,NEO_GRB + NEO_KHZ800);

volatile bool intrToggled = false;

float batteryReads[20];
int listcountcurrent = 0;
int appSelect = 1;
int weatherAppselect = 0;
float heartrateread = 0;

int hour24,hour12,minute,second,day,month,year,weekday; //Make variables to write to when reading from the onboard rtc

unsigned long return_start; //globals for the long press state machine
unsigned long return_last_state;
int return_length = 1500;

unsigned long sw_start; //globals for the long press state machine - stopwatch menus
unsigned long sw_last_state;
int sw_length = 1500;
char swBuffer[16] = "0:00:00:00";
uint64_t elapsedtimecurrent = 0;

unsigned long stop_start; //globals for the long press state machine - stopwatch start/stop
unsigned long stop_last_state;
int stop_length = 1500;

unsigned long weather_start; //globals for the long press state machine - weather app
unsigned long weather_last_state;
int weather_length = 1500;

unsigned long weather2_start; //globals for the long press state machine - weather app button 2
unsigned long weather2_last_state;
int weather2_length = 1500;

unsigned long s_start; //globals for the long press state machine - settings app button 1
unsigned long s_last_state;
int s_length = 1500;

unsigned long s2_start; //globals for the long press state machine - settings app button 2
unsigned long s2_last_state;
int s2_length = 1500;

unsigned long m_start; //globals for the long press state machine - menu button 1
unsigned long m_last_state;
int m_length = 1500;

unsigned long m2_start; //globals for the long press state machine - menu button 2
unsigned long m2_last_state;
int m2_length = 1500;

bool sw_on = false;

int hours_select = 0;
int minutes_select = 0;
int seconds_select = 0;
int* selection[3] = {&hours_select,&minutes_select,&seconds_select};
int current_selection = -1;
unsigned long previous = millis();
unsigned long now = 0;
bool on = false;

unsigned long alarm_start; //globals for the long press state machine - alarm selection
unsigned long alarm_last_state;
int alarm_length = 1500;
unsigned long alarm2_start; 
unsigned long alarm2_last_state;
int alarm2_length = 1500;

const char* amorpm = "AM";
uint64_t desired_wakeup = 0;
int Acurrent_selection = -1;
int Ahours_select = 0;
int Aminutes_select = 0;
int* selectionA[2] = {&Ahours_select,&Aminutes_select};

bool timer_on = false;
uint64_t useconds_left;
uint64_t desired_end;
bool aftertime = false;

bool isPM, lastButtonstate;
bool inHome, inMenu, inWeather, inStopwatch, inAlarm, inCompass, inGolf, inFind, inSettings; 
bool swAppselect = true;
int64_t lastfetch;
String payload;

int64_t starttime;

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

struct tm timeinfo; //Have the time library create a usable struct to get time variables from
struct tm alarminfo;

float heading = 0;
float nadjusted = 0;
float dheading = 0;
float x_offset = 0;
float y_offset = 0;

float xstart = 93;
float ystart = 32;
float xend = 0;
float yend = 0;

time_t timenow;
RTC_DATA_ATTR time_t alarm_now;
RTC_DATA_ATTR signed long long waittime;

bhySensorStatus sensorStatus;
bhySensorStatus sensorStatus2;
bhySensorStatus sensorStatus3;

int stepday = 0;
uint16_t steps = 0;
uint16_t yesterdaySteps = 0;


float currentXmax = 0;
float currentYmax = 0;
float currentXmin = 0;
float currentYmin = 0;

int SettingsappSelect = 1;

void tiltHandler(bhyVirtualSensor id){
  return;
}

void stepHandler(uint16_t data, bhyVirtualSensor id){
  if (stepday == day){
    steps = data - yesterdaySteps;
  }
  else{
    stepday = day;
    yesterdaySteps = data;
    steps = data - yesterdaySteps;
    }
  return;
}

uint64_t last_chop = 0;
bool isFlashOn = false;

void flashHandle(uint16_t data, bhyVirtualSensor id){
  if (esp_timer_get_time() - last_chop <= 500000){
    last_chop = esp_timer_get_time();
    isFlashOn = !isFlashOn;
    if (isFlashOn == true){
      light.setPixelColor(0,255,255,255);
      light.show();
    }
    else{
      light.setPixelColor(0,0,0,0);
      light.show();
    }
  }
  else{
    last_chop = esp_timer_get_time();
  }
}

uint64_t startwait = 0;
bool firstwait = true;

void api_pull(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED){
    delay(50);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); //Steal time from ntp server with the correct config

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

    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED){
      if (firstwait == true){
        startwait = esp_timer_get_time();
        firstwait = false;
      }
      if (esp_timer_get_time() - startwait >= 10000000){
        break;
      }
      else{
        delay(10);
      }
    }

    startwait = 0;
    firstwait = true;
    WiFi.disconnect(true);
  }

void setup() {
  Serial.begin(9600);

  pinMode(battery_pin, INPUT);
  pinMode(Button1, INPUT_PULLUP);
  pinMode(Button2, INPUT_PULLUP);
  pinMode(Haptic, OUTPUT);

  Wire.begin(8,9);

  api_pull();

  oled.init();
  oximeter.begin();
  bme.begin();
  Magne.begin(0x60); //Magne.readMagnetometerX()
  inHome=inMenu=inWeather=inStopwatch=inAlarm=inCompass=inGolf=inFind=inSettings=false; 
  inHome = true;

  bhi.begin(BHY_I2C_ADDR2);
  bhi.loadFirmware(bhy1_fw);

  bhi.configVirtualSensor((bhyVirtualSensor)BHY_VS_STEP_COUNTER, true, BHY_FLUSH_ALL, 1, 0, 0, 0);
  int8_t result = bhi.installSensorCallback(BHY_VS_STEP_COUNTER, false, stepHandler);

  bhi.configVirtualSensor((bhyVirtualSensor)BHY_VS_LINEAR_ACCELERATION, true, BHY_FLUSH_ALL,50,0,0,0);
  int8_t result3 = bhi.installSensorCallback(BHY_VS_LINEAR_ACCELERATION, false, flashHandle);

  bhi.configVirtualSensor((bhyVirtualSensor)BHY_VS_TILT, true, BHY_FLUSH_ALL, 0, 0, 0, 0);
  int8_t result2 = bhi.installSensorCallback(BHY_VS_TILT, true, tiltHandler);

  esp_deep_sleep_enable_gpio_wakeup(1ULL << BHI_INT, ESP_GPIO_WAKEUP_GPIO_HIGH);

  bhi.getSensorStatus((bhyVirtualSensor)BHY_VS_TILT, true, &sensorStatus);
  if (sensorStatus.power_mode == 7){
  }
  //else{
    //ESP.restart();
  //}

  bhi.getSensorStatus((bhyVirtualSensor)BHY_VS_LINEAR_ACCELERATION, true, &sensorStatus3);
  if(sensorStatus3.power_mode == 7){
  }
  //else{
    //ESP.restart();
  //}

  bhi.getSensorStatus((bhyVirtualSensor)BHY_VS_STEP_COUNTER, true, &sensorStatus2);
  if (sensorStatus2.power_mode == 7){
  }
  //else{
    //ESP.restart();
  //}

  bhi.run();
}

void printlocaltime(){
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
  oled.drawBitmap(13 + 8 + 2, 24, LABEL_WEATHER, LABEL_WEATHER_W, 8);

  oled.drawBitmap(13, 34, ICON_STOPWATCH, STOPWATCH_W, 8);
  oled.drawBitmap(13 + 8 + 2, 34, LABEL_STOPWATCH, LABEL_STOPWATCH_W, 8);

  oled.drawBitmap(13, 44, ICON_COMPASS, COMPASS_W, 8);
  oled.drawBitmap(13 + 8 + 2, 44, LABEL_COMPASS, LABEL_COMPASS_W, 8);

  oled.drawBitmap(13, 54, ICON_ALARM, ALARM_W, 8);
  oled.drawBitmap(13 + 8 + 2, 54, LABEL_ALARM, LABEL_ALARM_W, 8);

  oled.drawBitmap(73, 24, ICON_CRASH, CRASH_W, 8);
  oled.drawBitmap(73 + 8 + 2, 24, LABEL_CRASH, LABEL_CRASH_W, 8);

  oled.drawBitmap(73, 34, ICON_FINDPHONE, FINDPHONE_W, 8);
  oled.drawBitmap(73 + 8 + 2, 34, LABEL_FINDPHONE, LABEL_FINDPHONE_W, 8);

  oled.drawBitmap(73, 44, ICON_SETTINGS, SETTINGS_W, 8);
  oled.drawBitmap(73 + 8 + 2, 44, LABEL_SETTINGS, LABEL_SETTINGS_W, 8);

  int button_one_result = Buttonlogic(Button1,m_start,m_last_state,m_length);
  int button_two_result = Buttonlogic(Button2,m2_start,m2_last_state,m2_length);

  if (button_one_result == 1 && button_two_result == 1){
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
      crashAPP();
    }
    if (appSelect == 6){
      findphoneAPP(averageArray());
    }
    if (appSelect == 7){
      settingsAPP(averageArray());
    }
  }

  else if (button_two_result == 1){
    if (appSelect == 7){
      appSelect = 7;
    }
    else{
      appSelect++;
    }
  }
  else if (button_one_result == 1){
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

void crashAPP(){
  ESP.restart();
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
  
  int button_one_result = Buttonlogic(Button1,weather_start,weather_last_state,weather_length);
  int button_two_result = Buttonlogic(Button2,weather2_start,weather2_last_state,weather2_length);

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

  if (button_two_result == 1){
    if (weatherAppselect == (pagestoshow + 4)){
      weatherAppselect = 1;
    }
    else{
      weatherAppselect++;
    }
  }
  else if (button_one_result == 1){
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
  
  int button_one_result = Buttonlogic(Button1,stop_start,stop_last_state,stop_length);
  int button_two_result = Buttonlogic(Button2,sw_start,sw_last_state,sw_length);

  if (swAppselect == true){
    drawtext(1, rightalign("Stopwatch", 0, 1), 8, "Stopwatch");
    oled.invertText(false);
    drawtext(1, 5, 60, "1=Restart");
    drawtext(1, rightalign("2=Resume", 0, 1), 60, "2=Resume");
    oled.invertText(true);
  }
  
  else{
    drawtext(1, rightalign("Timer", 0, 1), 8, "Timer");
    oled.invertText(false);
    drawtext(1, 5, 60, "1/2=Up/Down");
    drawtext(1, rightalign("1+2=Start", 0, 1), 60, "1+2=Start");
    oled.invertText(true);
  }
  oled.invertText(false);

  if (button_two_result == 2){
    swAppselect  = !swAppselect;
    if (swAppselect == false){
      hours_select = 0;
      minutes_select = 0;
      seconds_select = 0;
    }
  }

  if (button_one_result == 1 || button_two_result == 1 && swAppselect == true){
    sw_on = !sw_on;
    if (sw_on == false){
      elapsedtimecurrent = esp_timer_get_time() - starttime;
    }
    if (sw_on == true && button_two_result == 1){
      starttime = esp_timer_get_time();
      starttime = starttime - elapsedtimecurrent;
    }
    if (sw_on == true && button_one_result == 1){
      starttime = esp_timer_get_time();
    }
  }

  if (sw_on == true && swAppselect == true){
    uint64_t current = esp_timer_get_time() - starttime;
    int s = ((int)current / 1000000) % 60;
    float fractional_s = (float)current / 1000000;
    int m = (int(current) / 60000000) % 60;
    int h = (int(current) / 3600000000) % 60;

    float ds;

    ds=std::modf(fractional_s, &ds);
    ds = ds * 100.0f;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%01d:%02d:%02d:%02d",(int)h,(int)m,(int)s,(int)ds);
    drawtext(2,5,35,buffer);
    memcpy(swBuffer, buffer, sizeof(buffer));}

  if (sw_on == false && swAppselect == true){
    drawtext(2,5,35,swBuffer);
  }

  if(swAppselect == false && timer_on == false && aftertime == false){
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",hours_select,minutes_select,seconds_select);
    drawtext(2,15,35,buffer);
    
    if (button_one_result == 1){
      *selection[current_selection] = *selection[current_selection] + 1;
      if (*selection[current_selection] > 59){
        *selection[current_selection] = 0;
      }
    }

    if (button_two_result == 1){
      *selection[current_selection] = *selection[current_selection] - 1;
      if (*selection[current_selection] < 0){
        *selection[current_selection] = 59;
      }
    }
  }

  if (button_one_result == 1 && button_two_result == 1 && current_selection != 2){
    current_selection = current_selection + 1;
  }
  else if (button_one_result == 1 && button_two_result == 1 && current_selection == 2){
    current_selection = -1;
    timer_on = !timer_on;
    if (timer_on == true){
      desired_end = (hours_select * 3600000000) + (minutes_select * 60000000) + (seconds_select * 1000000);
      starttime = esp_timer_get_time();
    }
  }

    if (swAppselect == false && timer_on == true){
      //TODO: Allow the timer to run, then haptic like nuts when it is over
      if ((esp_timer_get_time() - starttime) <= desired_end && timer_on == true){
        useconds_left = desired_end - (esp_timer_get_time() - starttime);
        int s = ((useconds_left) / 1000000) % 60;
        int m = ((useconds_left) / 60000000) % 60;
        int h = ((useconds_left) / 3600000000) % 60;
        
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",h,m,s);
        drawtext(2,15,35,buffer);
      }
      else{
        useconds_left = 0;
        aftertime = true;
        timer_on = false;
      }
    }

  if (aftertime == true){
    now = millis();
    if (now - previous >= 250){
      previous = now;
      on = !on;
    }
    if (on == true){
      drawtext(2,15,35,"00:00:00");
    }
    if (button_one_result == 1 && button_two_result == 1){
      useconds_left = 0;
      desired_end = 0;
      timer_on = false;
      sw_on = false;
      aftertime = false;}
  }


  if (button_one_result == 2){
    useconds_left = 0;
    desired_end = 0;
    timer_on = false;
    sw_on = 0;
    aftertime = false;
  }
}

void compassAPP(float batterylife){
  inMenu = false;
  inCompass = true;
  oled.fill(0);
  oled.rect(0,0,50,64,1);
  oled.rect(1,1,16,16,0);
  battery(batterylife, 1);
  oled.invertText(true);
  drawtext(1, 5, 56, "Compass");
  oled.invertText(false);

  heading = atan2((Magne.readMagnetometerY() - y_offset), (Magne.readMagnetometerX() - x_offset));
  dheading = atan2((Magne.readMagnetometerY() - y_offset), (Magne.readMagnetometerX() - x_offset))*(180/PI);

  tocharinverted(heading,"",1,5,32);

  nadjusted = heading + (PI/2); //Put North "UP"

  xend = (xstart + (20 * cos(nadjusted)));
  yend = (ystart - (20 * sin(nadjusted)));

  oled.line(xstart,ystart,xend,yend);

  oled.drawBitmap(63, 1, COMPASS_DIAL, COMPASS_DIAL_W, 62);
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

  int button_one_result = Buttonlogic(Button1,alarm_start,alarm_last_state,alarm_length);
  int button_two_result = Buttonlogic(Button2,alarm2_start,alarm2_last_state,alarm2_length);

  if (Acurrent_selection == 0){
    if (*selectionA[Acurrent_selection] >= 12){
      amorpm = "PM";
    }
    else{
      amorpm = "AM";
    }
    if (*selectionA[Acurrent_selection] > 23){
      *selectionA[Acurrent_selection] = 0;
      amorpm = "AM";
    }
    if (*selectionA[Acurrent_selection] < 0){
      *selectionA[Acurrent_selection] = 23;
      amorpm = "PM";
    }
    if (button_one_result == 1){
      *selectionA[Acurrent_selection] = *selectionA[Acurrent_selection] + 1;
    }
    if (button_two_result == 1){
      *selectionA[Acurrent_selection] = *selectionA[Acurrent_selection] - 1;
    }
  }
  if (Acurrent_selection == 1){
    if (*selectionA[Acurrent_selection] > 60){
      *selectionA[Acurrent_selection] = 0;
    }
    if (*selectionA[Acurrent_selection] < 0){
      *selectionA[Acurrent_selection] = 59;
    }
    if (button_one_result == 1){
      *selectionA[Acurrent_selection] = *selectionA[Acurrent_selection] + 1;
    }
    if (button_two_result == 1){
      *selectionA[Acurrent_selection] = *selectionA[Acurrent_selection] - 1;
    }
  }

  if (button_one_result == 1 && button_two_result == 1 && Acurrent_selection == 0){
    Acurrent_selection = Acurrent_selection + 1;
  }
  else if (button_one_result == 1 && button_two_result == 1 && Acurrent_selection <= 1){
    Acurrent_selection = 0;
    alarminfo = timeinfo;
    alarminfo.tm_hour = Ahours_select;
    if (alarminfo.tm_hour < hour24){
      alarminfo.tm_mday += 1;
    }
    alarminfo.tm_min = Aminutes_select;

    alarm_now = mktime(&alarminfo);

    waittime = (signed long long)(alarm_now - time(&timenow));
  }

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d:%02d %s",(Ahours_select % 12 == 0) ? 12 : Ahours_select % 12,(Aminutes_select),amorpm);
  drawtext(2,15,35,buffer);
}

/*
CANNED FOR V1 FIRMWARE PUSH, WILL COME BACK
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
}
*/

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

  int button_one_result = Buttonlogic(Button1,s_start,s_last_state,s_length);
  int button_two_result = Buttonlogic(Button2,s2_start,s2_last_state,s2_length);

  oled.drawBitmap(13, 24, ICON_FLASH, FLASH_W, 8);
  oled.drawBitmap(13 + 8 + 2, 24, LABEL_FLASHLIGHTSETTINGS, LABEL_FLASHLIGHTSETTINGS_W, 8);

  oled.drawBitmap(13, 34, ICON_COMPASS, COMPASS_W, 8);
  oled.drawBitmap(13 + 8 + 2, 34, LABEL_CALIBRATECOMPASS, LABEL_CALIBRATECOMPASS_W, 8);

  oled.drawBitmap(13, 44, ICON_VERSION, VERSION_W, 8);
  oled.drawBitmap(13 + 8 + 2, 44, LABEL_VERSION, LABEL_VERSION_W, 8);

  if (button_one_result == 1 && button_two_result == 1){
    if (SettingsappSelect == 1){
      weatherAppselect = 1;
      weatherAPP(averageArray());
    }
    if (SettingsappSelect == 2){
      stopwatchAPP(averageArray());
    }
    if (SettingsappSelect == 3){
      compassAPP(averageArray());
    }
  }

  else if (button_two_result == 1){
    if (SettingsappSelect == 3){
      SettingsappSelect = 3;
    }
    else{
      SettingsappSelect++;
    }
  }
  else if (button_one_result == 1){
    if (SettingsappSelect == 1){
      SettingsappSelect = 1;
    }
    else{
      SettingsappSelect--;
    }
  }

  if (SettingsappSelect == 1){
    oled.drawBitmap(5, 24, ICON_CURSOR, CURSOR_W, 8);
  }
  if (SettingsappSelect == 2){
    oled.drawBitmap(5, 34, ICON_CURSOR, CURSOR_W, 8);
  }
  if (SettingsappSelect == 3){
    oled.drawBitmap(5, 44, ICON_CURSOR, CURSOR_W, 8);
  }

/*
  if ((esp_timer_get_time() - calibrationStart) >= 150000000){
    float nowX = Magne.readMagnetometerX();
    float nowY = Magne.readMagnetometerY();
    if (nowX >= currentXmax){
      currentXmax = nowX;
    }
    if (nowX <= currentXmin){
      currentXmin = nowX;
    }

    if (nowY >= currentYmax){
      currentYmax = nowY;
    }
    if (nowX <= currentXmin){
      currentYmin = nowY;
    }
    x_offset = ((currentXmax + currentXmin)/2);
    y_offset = ((currentYmax + currentYmin)/2);
  }
*/

  //TODO: Import the menu interface again - the Icons

  //TODO: Add settings for things like flashlight brightness, compass calibration, seeing version number, maybe an easter egg, etc

  //TODO: Write to Preferences ROM to save these settings
}

bool appCheck(float batterylife){
  if (inHome == true){
    home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), steps);
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
  //if (inGolf == true){
    //golfAPP(averageArray());
    //return true;
  //}
  if (inFind == true){
    findphoneAPP(averageArray());
    return true;
  }
  if (inSettings == true){
    settingsAPP(averageArray());
    return true;
  }
  return false;
}

int Buttonlogic(int button, unsigned long &starting, unsigned long &laststate, int &length){ //Returns an int based off how a button is pressed. Long press = 1, Short Press = 2
  if (digitalRead(button) == false && laststate == false){
    starting = millis();
    laststate = true;
  }
  if (laststate == true && digitalRead(button) == true){
    laststate = false;
    if (millis() - starting >= length){
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

  if (Buttonlogic(Button1,return_start,return_last_state,return_length) == 2){
    if (inHome != true){
      home(averageArray(), bme.temperature, buildTime(), buildDate(), oximeter.getHeartRate(), 10000);
      inMenu = false;
      inWeather= false;
      inStopwatch = false; 
      inAlarm = false;
      inCompass = false; 
      inGolf = false; 
      inFind = false; 
      inSettings = false; 
      inHome = true;
    }
    else{
      menu(averageArray());
      inHome = false;
      inMenu = true;
    }
  }
  oled.update();

  if (esp_timer_get_time() - lastfetch >= 1800000000){
    lastfetch = esp_timer_get_time();
    api_pull();
  }
}