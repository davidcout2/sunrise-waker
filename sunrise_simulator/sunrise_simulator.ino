//compiled with Arduino IDE 2.0.4

//questions:
//todo: split project into multiple files
//todo: add a switch in order to activate/deactivate screen?
//todo: add a button to activate/deactivate the waker
//todo: find a better way to display multiple pages
//todo: use a Real Time Clock RTC

#include <WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include "arduino_secrets.h"

//WIFI connection credential
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

//Server adress to get the time
const char* ntpServer = "ch.pool.ntp.org";  

//MQTT connection data
const uint8_t broker[] = {195, 15, 232, 178};
int port = 1883;
const char esp32_time_topic[] = "physicsforiot/data/esp32/time";
const char flutter_time_topic[] = "physicsforiot/data/flutter/time";
const char brocker_user[] = SECRET_MQTT_SSID;
const char brocker_pw[] = SECRET_MQTT_PASS;

//Settings for the mqtt messages to publish
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite clock_sprite = TFT_eSprite(&tft);

//Settings for the clock gui
#define SCREEN_HEIGHT 320
#define SCREEN_ROTATION 1  //90° rotation
#define CLOCK_MARGING_LEFT 30
#define CLOCK_MARGING_UP 50
#define CLOCK_R 110.0f
#define CLOCK_W CLOCK_R * 2 + 1
#define HOUR_HAND_LENGTH CLOCK_R * 0.5
#define MIN_HAND_LENGTH CLOCK_R * 0.7
#define SEC_HAND_LENGTH CLOCK_R * 0.8

//Settings for the calibration
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

//Buttons
ButtonWidget set_waker_btn = ButtonWidget(&tft);
ButtonWidget cancel_btn = ButtonWidget(&tft);
ButtonWidget hour_inc_btn = ButtonWidget(&tft);  // Button to increment the waking hour in the waker setting
ButtonWidget hour_dec_btn = ButtonWidget(&tft);  // Button to decrement the waking hour in the waker setting
ButtonWidget min_inc_btn = ButtonWidget(&tft);  // Button to increment the waking minute in the waker setting
ButtonWidget min_dec_btn = ButtonWidget(&tft);  // Button to decrement the waking minute in the waker setting
ButtonWidget save_btn = ButtonWidget(&tft);
ButtonWidget* btns[] = { &set_waker_btn };//todo: test with no initialisation
uint8_t buttonCount = sizeof(btns) / sizeof(btns[0]);//todo: test with no initialisation

bool WakerSettingShown = false;  // No solution were found to display two different page. This boolean is used to manage the displayed page

//the time hh:mm is stored as min (time = hours * 60 + minutes)
uint16_t waking_time = 360;
uint16_t waking_time_temp = 0;
struct tm timeinfo;

bool isSunRising = false;

//***************variables and methods for the led******************//
const int bluePin = 26;
const int redPin = 25;
const int greenpin = 33;

//settings for the pwm modulation
const int freq = 5000;
const int resolution = 8;
const int redChannel = 1;
const int greenChannel = 2;
const int blueChannel = 3;

//Sunrise constant depending on atmospheric conditions
//a between 0.3 and 0.5
//b between 0.02 and 0.04
//c between 0.01 and 0.02
//d between 0.01 and 0.02
const float a_r = 0.5;
const float b_r = 0.02;
const float c_r = 0.01;
const float d_r = 0.01;
const float a_g = 0.3;
const float b_g = 0.03;
const float c_g = 0.01;
const float d_g = 0.01;
const float a_b = 0.6;
const float b_b = 0.01;

long int sunriseStart = 0;
long int sunriseDeltaT = 0;
const long int sunriseDuration = 30 * 60; //Sunrise duration in seconds
//const long int timeCorrection = 600; //600 seconds are added to correct to shift the color function time in order to get accurate color at the sunrise start

float getSunriseRed(long int seconds) {
  float red = -1.0718 * pow(10, -15) * pow(seconds,6) + 4.3215 * pow(10, -12) * pow(seconds,5) -5.0382 * pow(10, -9) * pow(seconds,4) + 2.7783 * pow(10, -7) * pow(seconds, 3) + 2.1792 * pow(10, -3) * pow(seconds,2) - 0.41005 * seconds;
  if(seconds > 1800 | red > 255){
    red = 255;
  }  
  return int(red);
} 

float getSunriseGreen(long int seconds){
  float green = -1.2130 * pow(10, -15) * pow(seconds,6) + 5.9530 * pow(10, -12) * pow(seconds,5) - 1.0607 * pow(10, -8) * pow(seconds,4) + 8.1934 * pow(10, -6) * pow(seconds, 3) - 2.4324 * pow(10, -3) * pow(seconds,2) + 0.26394 * seconds;
  if(seconds > 1800 | green > 255){
    green = 255;
  }  
  return int(green);
} 

float getSunriseBlue(long int seconds){
  float blue = 1.1553 * pow(10, -15) * pow(seconds,6) - 5.3772 * pow(10, -12) * pow(seconds,5) + 8.7685 * pow(10, -9) * pow(seconds,4) - 5.7576 * pow(10, -6) * pow(seconds, 3) + 1.2249 * pow(10, -3) * pow(seconds,2) + 0.071288 * seconds;
  if(seconds > 1800 | blue > 255){
    blue = 255;
  }  
  return int(blue);
} 

//***********************************************************//

//callback method when a mqtt message arrives
void on_mqtt_msg_arrived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received a message");
  int32_t time = 0;
  //convert the ascii bytes to int
  for (unsigned int i = 0; i < length; i++) {
    time += (payload[i] - 48) * pow(10, length - i - 1);   
  }
  waking_time = time;
}

void setup() {
  Serial.begin(115200);
  //LED settings
  ledcSetup(redChannel, freq, resolution);
  ledcSetup(greenChannel, freq, resolution);
  ledcSetup(blueChannel, freq, resolution);
  ledcAttachPin(redPin, redChannel);
  ledcAttachPin(greenpin, greenChannel);
  ledcAttachPin(bluePin, blueChannel);  
  
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(TFT_BLACK);
  touch_calibrate();
    
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    //todo: display error
  }
  mqttClient.setServer(broker, port);
  mqttClient.setCallback(on_mqtt_msg_arrived);

  // get the time and add 2 hours (timezone correction)
  configTime(7200, 0, ntpServer);
  getLocalTime();

  setupView();
}

void setupView() {
  tft.fillScreen(TFT_BLACK);
  clock_sprite.createSprite(CLOCK_W, CLOCK_W);
  memset(btns, 0, sizeof(btns)); //Empty the button list to remove the buttons when switching view
  // load the home screen
  if (!WakerSettingShown) {
    btns[0] = &set_waker_btn;
    buttonCount = 1;
    initHomeBtns();
    renderClockView();
  //load the waking time setting screen    
  } else {  
    waking_time_temp = waking_time;
    btns[0] = &cancel_btn;
    btns[1] = &hour_inc_btn;
    btns[2] = &hour_dec_btn;
    btns[3] = &min_inc_btn;
    btns[4] = &min_dec_btn;
    btns[5] = &save_btn;
    buttonCount = 6;
    initSettingbtns();
    renderWakerClockView();
  }
}

//Manage the mqtt client connection
void reconnect()
{
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("esp32Client", brocker_user, brocker_pw)) {
      Serial.println("connected");
      mqttClient.subscribe(flutter_time_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    } 
  }     
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
  
  getLocalTime();
  
  if(!isSunRising){ 
    isSunRising = checkWakingTime();
    if(isSunRising){
      sunriseStart = millis()/1000;
    }           
  } else {
    setSunriseColor(); 
    isSunriseEnded();
  }
  
  if (!WakerSettingShown) {
    renderClockView();
    renderWakingTime();        
  } else {
    renderWakerClockView();
    renderSettingText();
  }
  configureButtons();
}

boolean checkWakingTime() {
  if(getHour(waking_time) == timeinfo.tm_hour && getMinute(waking_time) == timeinfo.tm_min){
    Serial.print("time to wake up");
    return true;
  }  
  return false;
}

void setSunriseColor() {
    long int now = millis()/1000;
    sunriseDeltaT = now - sunriseStart;

    long int red = getSunriseRed(sunriseDeltaT);
    long int green = getSunriseGreen(sunriseDeltaT);
    long int blue = getSunriseBlue(sunriseDeltaT);
    printf("sunriseDeltaT=%d\n", sunriseDeltaT); 
    printf("red=%d\n", red);
    printf("green=%d\n", green);
    printf("blue=%d\n", blue);

    ledcWrite(redChannel, red);
    ledcWrite(greenChannel, green);
    ledcWrite(blueChannel, blue-50);
    //delay(1500);
}

void isSunriseEnded() {
  if(sunriseDeltaT > sunriseDuration) {
    isSunRising = false;
  } 
}

void stopLEDS() {
  //todo: to stop the leds when the user waked up
}

//can get stuck when starting the ESP32
void getLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");  // todo: alert on screen
    return;
  }
  timeinfo.tm_hour = timeinfo.tm_hour;
}

//Render the clock without the Hands
void renderClockFace() {
  clock_sprite.fillSprite(TFT_BLACK);
  clock_sprite.fillSmoothCircle(CLOCK_R, CLOCK_R, CLOCK_R, TFT_DARKGREY);
  double endX = 0.0, endY = 0.0;  //used to store the results of the computeCoordinates method
  for (uint8_t hour = 1; hour <= 12; hour++) {
    double angle = hour * 360.0 / 12;

    const int16_t numberR = CLOCK_R - 15;
    computeCoordinates(CLOCK_R, CLOCK_R, angle, numberR, &endX, &endY);
    clock_sprite.drawNumber(hour, endX - 2, endY - 2);
  }
}

//todo: adapt the hour hand with the minutes
void renderClockView() {
  renderClockFace();
  double endX = 0.0, endY = 0.0;  //used to store the results of the computeCoordinates method

  int hourAngle = timeinfo.tm_hour * 360 / 12;
  computeCoordinates(CLOCK_R, CLOCK_R, hourAngle, HOUR_HAND_LENGTH, &endX, &endY);
  clock_sprite.drawWideLine(CLOCK_R, CLOCK_R, endX, endY, 6.0f, TFT_ORANGE);

  int minAngle = timeinfo.tm_min * 360 / 60;
  computeCoordinates(CLOCK_R, CLOCK_R, minAngle, MIN_HAND_LENGTH, &endX, &endY);
  clock_sprite.drawWideLine(CLOCK_R, CLOCK_R, endX, endY, 6.0f, TFT_ORANGE);

  int secAngle = timeinfo.tm_sec * 360 / 60;
  computeCoordinates(CLOCK_R, CLOCK_R, secAngle, SEC_HAND_LENGTH, &endX, &endY);
  clock_sprite.drawWedgeLine(CLOCK_R, CLOCK_R, endX, endY, 2.5, 1.0, TFT_YELLOW);

  clock_sprite.pushSprite(CLOCK_MARGING_LEFT, CLOCK_MARGING_UP, TFT_TRANSPARENT);
}

void renderWakerClockView() {
  renderClockFace();
  double endX = 0.0, endY = 0.0;  //used to store the results of the computeCoordinates method

  int hourAngle = getHour(waking_time_temp) * 360 / 12;
  computeCoordinates(CLOCK_R, CLOCK_R, hourAngle, HOUR_HAND_LENGTH, &endX, &endY);
  clock_sprite.drawWideLine(CLOCK_R, CLOCK_R, endX, endY, 6.0f, TFT_ORANGE);

  int minAngle = getMinute(waking_time_temp) * 360 / 60;
  computeCoordinates(CLOCK_R, CLOCK_R, minAngle, MIN_HAND_LENGTH, &endX, &endY);
  clock_sprite.drawWideLine(CLOCK_R, CLOCK_R, endX, endY, 6.0f, TFT_ORANGE);

  clock_sprite.pushSprite(CLOCK_MARGING_LEFT, CLOCK_MARGING_UP, TFT_TRANSPARENT);
}

int getHour(uint32_t time) {
  int minute = getMinute(time);
  return (time - minute) / 60;
}

int getMinute(uint32_t time) {
  return time % 60;
}

//Render the waking time text on the home screen
void renderWakingTime() {
  int hour = getHour(waking_time);
  int minute = getMinute(waking_time);
  tft.fillRoundRect(350, 160, 80, 20, 0, TFT_BLACK); //to override old numbers
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawNumber(hour, 350, 160);
  
  tft.drawString(":", 380, 160);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawNumber(minute, 400, 160);
}

//Render the waking time text on the setting screen
void renderSettingText() {
  int hour_temp = getHour(waking_time_temp);
  int minute_temp = getMinute(waking_time_temp);  

  tft.fillRoundRect(350, 120, 40, 20, 0, TFT_BLACK); //to override old numbers
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawNumber(hour_temp, 350, 120);
  tft.drawString("h", 380, 120);

  tft.fillRoundRect(350, 190, 40, 20, 0, TFT_BLACK); //to override old numbers
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawNumber(minute_temp, 350, 190);
  tft.drawString("m", 380, 190);
}

void initHomeBtns() {
  set_waker_btn.initButtonUL(300, 30, 150, 50, TFT_RED, TFT_BLACK, TFT_WHITE, "Set Waker", 2);
  set_waker_btn.setPressAction(loadSettingScreen);
  set_waker_btn.drawSmoothButton(false, 3, TFT_BLACK);
}

void initSettingbtns() {
  cancel_btn.initButtonUL(300, 30, 150, 50, TFT_WHITE, TFT_BLACK, TFT_WHITE, "Cancel", 2);
  cancel_btn.setPressAction(loadHomeScreen);
  cancel_btn.drawSmoothButton(false, 3, TFT_BLACK);

  hour_dec_btn.initButtonUL(290, 100, 50, 50, TFT_RED, TFT_BLACK, TFT_WHITE, "-", 2);
  hour_dec_btn.setPressAction(decreaseWakingHour);
  hour_dec_btn.drawSmoothButton(false, 3, TFT_BLACK); 

  hour_inc_btn.initButtonUL(410, 100, 50, 50, TFT_GREEN, TFT_BLACK, TFT_WHITE, "+", 2);
  hour_inc_btn.setPressAction(increaseWakingHour);
  hour_inc_btn.drawSmoothButton(false, 3, TFT_BLACK);

  min_dec_btn.initButtonUL(290, 170, 50, 50, TFT_RED, TFT_BLACK, TFT_WHITE, "-", 2);
  min_dec_btn.setPressAction(decreaseWakingMin);
  min_dec_btn.drawSmoothButton(false, 3, TFT_BLACK); 

  min_inc_btn.initButtonUL(410, 170, 50, 50, TFT_GREEN, TFT_BLACK, TFT_WHITE, "+", 2);
  min_inc_btn.setPressAction(increaseWakingMin);
  min_inc_btn.drawSmoothButton(false, 3, TFT_BLACK);
  
  save_btn.initButtonUL(300, 240, 150, 50, TFT_GREEN, TFT_GREEN, TFT_BLACK, "Save", 2);
  save_btn.setPressAction(setWakingSettings);
  save_btn.drawSmoothButton(false, 3, TFT_BLACK);
}

//Configure the buttons actions on both screens
void configureButtons() {
  static uint32_t scanTime = millis();
  uint16_t t_x = 9999, t_y = 9999;  // To store the touch coordinates

  // Scan keys every 50ms at most
  if (millis() - scanTime >= 50) {
    // Pressed will be set true if there is a valid touch on the screen
    bool pressed = tft.getTouch(&t_x, &t_y);
    scanTime = millis();
    for (uint8_t b = 0; b < buttonCount; b++) {   
      if (pressed) {   
        Serial.println(b);        
        if (btns[b]->contains(t_x, t_y)) {
          btns[b]->press(true);
          btns[b]->pressAction();
        }
      } else {
        btns[b]->press(false);
      }
    }
  }
}

void loadSettingScreen(void) {
  WakerSettingShown = true; //when set to true, the waking time setting screen will be opened by view setup
  setupView();
}

void loadHomeScreen(void) {
  WakerSettingShown = false;
  setupView();
}


//todo: remove the inc and dec hour and add the functionality in the minute methods
void increaseWakingHour(void) {
  waking_time_temp += 60;  
  if(getHour(waking_time_temp) > 23){
    waking_time_temp = getMinute(waking_time_temp);
  }
}

void decreaseWakingHour(void) {
  if(getHour(waking_time_temp) < 1){
    waking_time_temp = 23 * 60 + getMinute(waking_time_temp);
    return;
  }
  waking_time_temp -= 60;
}

//todo: try to extract minutes and hour with bit shift
void increaseWakingMin(void) {
  waking_time_temp += 5;  
  if(getHour(waking_time_temp) > 23){
    waking_time_temp = getMinute(waking_time_temp);
  }
}

void decreaseWakingMin(void) {    
  if(getHour(waking_time_temp) < 1 && getMinute(waking_time_temp) < 1){
    waking_time_temp = 23 * 60 + 55;
    return;
  }
  waking_time_temp -= 5;
}

//Set the waking time and publish it with a MQTT message
void setWakingSettings(void) {
  WakerSettingShown = false;
  waking_time = waking_time_temp;
  snprintf (msg, MSG_BUFFER_SIZE, "%u", waking_time);
  mqttClient.publish(esp32_time_topic, msg);

  setupView();
}

void btn_releaseAction(void) {
  //no action
}

void computeCoordinates(int16_t startX, int16_t startY, double angle, int16_t distance, double* endX, double* endY) {
  double x = cos((angle - 90) * PI / 180);
  double y = sin((angle - 90) * PI / 180);
  *endX = x * distance + startX;
  *endY = y * distance + startY;
}

//source: https://github.com/Bodmer/TFT_eWidget/blob/main/examples/Buttons/Button_demo/Button_demo.ino
void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!LittleFS.begin()) {
    Serial.println("Formating file system");
    LittleFS.format();
    LittleFS.begin();
  }

  // check if calibration file exists and size is correct
  if (LittleFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      // Delete if we want to re-calibrate
      LittleFS.remove(CALIBRATION_FILE);
    } else {
      File f = LittleFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char*)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(SCREEN_ROTATION);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char*)calData, 14);
      f.close();
    }
  }
}
