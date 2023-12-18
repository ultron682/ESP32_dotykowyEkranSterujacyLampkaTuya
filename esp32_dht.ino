#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "DHT.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <FS.h>
#include "Free_Fonts.h"  // Include the header file attached to this sketch

#include <TFT_eSPI.h>     // Hardware-specific library
#include <TFT_eWidget.h>  // Widget library

#include "esp32_dht_bmp.h"

#include <ESPping.h>

// const String ssid = "AP-1";
// const String password = "152216100723";

const String ssid = "Inez 2,4";
const String password = "56964592";

// const String ssid = "PIETRO 1";
// const String password = "lublinaleksandrajaworowskiego14";

const char *serverName = "http://api.thingspeak.com/update";
unsigned long myChannelNumber = 2058504;
String apiKey = "3E55L1C2B988LHQJ";

uint8_t DHTPin = 14;
#define DHTTYPE DHT22
DHT dht(DHTPin, DHTTYPE);

float Temperature;
float Humidity;
float TemperatureOutdoor;

TFT_eSPI tft = TFT_eSPI();
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

ButtonWidget btnL = ButtonWidget(&tft);
ButtonWidget btnR = ButtonWidget(&tft);

#define BUTTON_W 50
#define BUTTON_H 116

ButtonWidget *btn[] = { &btnL, &btnR };
uint8_t buttonCount = sizeof(btn) / sizeof(btn[0]);

uint8_t oneWireBus = 15;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

unsigned long previousMillis = 0;

short currentImage = 2;
short lastReadStateUSB = 1;

IPAddress ipComputer(192, 168, 10, 153);  // The remote ip to ping

#define LED_BUILTIN 2/////////////////////////


void setup() {
  Serial.begin(115200);
  Serial.print("MOSI: ");
  Serial.println(MOSI);
  Serial.print("MISO: ");
  Serial.println(MISO);
  Serial.print("SCK: ");
  Serial.println(SCK);
  Serial.print("SS: ");
  Serial.println(SS);  


  pinMode(14, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  sensors.begin();
  pinMode(DHTPin, INPUT);
  dht.begin();

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(FF18);
  tft.setTextWrap(true);
  tft.setTextSize(1);

  WiFi.onEvent(onWiFiEvent);
  connectWifi(false);
  sendDataToThingSpeak("api_key=" + apiKey + "&field5=" + String(millis()));  // Wake up data

  delay(500);

  loopTask();

  //Calibrate the touch screen and retrieve the scaling factors
  touch_calibrate();
  initButtons();
}

unsigned long previousMillisPing = 0;
bool lastPing = false;

void loop() {
  if (millis() - previousMillisPing >= 2000) {
    bool currentPing = Ping.ping(ipComputer, 1);
    previousMillisPing = millis();
    tft.fillRect(10, 10, 10, 10, (currentPing == true ? TFT_GREEN : TFT_RED));

    if (currentPing == true && lastPing == true && lastReadStateUSB == 0) {
      turnOnLEDS();
      lastReadStateUSB = 1;
    } else if (currentPing == false && lastPing == false && lastReadStateUSB == 1) {
      turnOffLEDS();
      lastReadStateUSB = 0;
    }


    if (currentPing == false) {
      previousMillisPing += 4000;
    }

    lastPing = currentPing;
  }

  if (millis() - previousMillis >= 60000) {
    setCpuFrequencyMhz(240);
    loopTask();
    setCpuFrequencyMhz(80);
  }

  // static uint32_t scanTime = millis();
  // uint16_t t_x = 9999, t_y = 9999;  // To store the touch coordinates

  // // Scan keys every 50ms at most
  // if (millis() - scanTime >= 50) {
  //   // Pressed will be set true if there is a valid touch on the screen
  //   bool pressed = tft.getTouch(&t_x, &t_y);
  //   scanTime = millis();
  //   for (uint8_t b = 0; b < buttonCount; b++) {
  //     if (pressed) {
  //       if (btn[b]->contains(t_x, t_y)) {
  //         btn[b]->press(true);
  //         btn[b]->pressAction();
  //       }
  //     } else {
  //       btn[b]->press(false);
  //       btn[b]->releaseAction();
  //     }
  //   }
  // }
}

void btnL_pressAction(void) {
  if (btnL.justPressed()) {
    Serial.println("Left button just pressed");
    btnL.drawSmoothButton(true);

    turnOnLEDS();
  }
}

void btnL_releaseAction(void) {
  static uint32_t waitTime = 1000;
  if (btnL.justReleased()) {
    Serial.println("Left button just released");
    btnL.drawSmoothButton(false);
    btnL.setReleaseTime(millis());
    waitTime = 10000;
  } else {
    if (millis() - btnL.getReleaseTime() >= waitTime) {
      waitTime = 1000;
      btnL.setReleaseTime(millis());
      btnL.drawSmoothButton(btnL.getState());
    }
  }
}

void btnR_pressAction(void) {
  if (btnR.justPressed()) {
    Serial.println("Left button just pressed");
    btnR.drawSmoothButton(true);

    turnOffLEDS();
  }
}

void btnR_releaseAction(void) {
  static uint32_t waitTime = 1000;
  if (btnR.justReleased()) {
    Serial.println("Right button just released");
    btnR.drawSmoothButton(false);
    btnR.setReleaseTime(millis());
    waitTime = 10000;
  } else {
    if (millis() - btnR.getReleaseTime() >= waitTime) {
      waitTime = 1000;
      btnR.setReleaseTime(millis());
      btnR.drawSmoothButton(btnR.getState());
    }
  }
}

void initButtons() {
  uint16_t x = (tft.width() - BUTTON_W) - 3;
  uint16_t y = 3;
  btnL.initButtonUL(x, y, BUTTON_W, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_WHITE, "", 1);
  btnL.setPressAction(btnL_pressAction);
  btnL.setReleaseAction(btnL_releaseAction);
  btnL.drawSmoothButton(false, 2, TFT_BLACK);  // 3 is outline width, TFT_BLACK is the surrounding background colour for anti-aliasing

  y = BUTTON_H + 6;
  btnR.initButtonUL(x, y, BUTTON_W, BUTTON_H, TFT_WHITE, TFT_RED, TFT_WHITE, "", 1);
  btnR.setPressAction(btnR_pressAction);
  btnR.setReleaseAction(btnR_releaseAction);
  btnR.drawSmoothButton(false, 2, TFT_BLACK);  // 3 is outline width, TFT_BLACK is the surrounding background colour for anti-aliasing
}

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
        if (f.readBytes((char *)calData, 14) == 14)
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
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}

void onWiFiEvent(WiFiEvent_t event) {
  tft.setCursor(10, 20);

  switch (event) {
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi begin failed ");
      // try reconnect here (after delay???)
      drawText("WiFi begin failed ");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi begin succeeded ");
      // Connected successfully
      drawText("WiFi begin succeeded ");
      break;
    default:
      Serial.println("WiFi default");
      drawText("WiFi default ");
  }
}

uint8_t timeoutWifiConnect = 0;
bool connectWifi(bool quiet) {
  if (WiFi.status() == WL_CONNECTED)
    return true;

  timeoutWifiConnect = 0;
  tft.setCursor(0, 20);

  digitalWrite(LED_BUILTIN, HIGH);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);

    previousMillis = millis();

    if (!quiet)
      tft.print(".");

    if (timeoutWifiConnect >= 50) {
      return false;
    }
    timeoutWifiConnect++;
  }

  if (!quiet)
    tft.print(" OK");
  digitalWrite(LED_BUILTIN, LOW);
  return true;
}

void loopTask() {
  previousMillis = millis();

  readSensors();

  if (connectWifi(true))
    sendDataToThingSpeak("api_key=" + apiKey + "&field1=" + String(Temperature, 1) + "&field2=" + String(Humidity, 1) + "&field3=" + String(TemperatureOutdoor, 1));

  drawNextImage();

  tft.setCursor(10, 230);
  drawTextInLine(String(Temperature, 1) + "'C", (Temperature >= 19 && Temperature <= 23) ? TFT_WHITE : TFT_RED);
  drawTextInLine(" | ", TFT_WHITE);
  drawTextInLine(String(Humidity, 1) + "%", (Humidity >= 35 && Humidity <= 60) ? TFT_WHITE : TFT_RED);
  //tft.setCursor(10, 230);
  drawTextInLine(" | " + String(TemperatureOutdoor, 1) + "'C", (TemperatureOutdoor >= 10) ? TFT_WHITE : TFT_BLUE);

  previousMillis = millis();
}

void turnOnLEDS() {
  digitalWrite(14, HIGH);
  delay(70);
  digitalWrite(14, LOW);
  delay(350);
  digitalWrite(14, HIGH);
  delay(70);
  digitalWrite(14, LOW);
}

void turnOffLEDS() {
  digitalWrite(14, HIGH);
  delay(70);
  digitalWrite(14, LOW);
  delay(100);
  digitalWrite(14, HIGH);
  delay(70);
  digitalWrite(14, LOW);
}

void drawText(String text) {
  tft.setTextColor(TFT_WHITE);
  tft.println(text);
}

void drawText(String text, uint16_t color) {
  tft.setTextColor(color);
  tft.println(text);
}

void drawTextInLine(String text) {
  tft.setTextColor(TFT_WHITE);
  tft.print(text);
}

void drawTextInLine(String text, uint16_t color) {
  tft.setTextColor(color);
  tft.print(text);
}

void drawNextImage() {
  tft.setSwapBytes(true);
  // tft.pushImage(0, 0, 320, 240, currentImage == 0 ? image_bejb : currentImage == 1 ? image_witcher
  //                                                                                  : image_bejb2);
  tft.pushImage(0, 0, 320, 240, image_bejb2);
  //tft.pushImage(0, 0, 320, 240, currentImage == 0 ? image_bejb : image_witcher);
  currentImage++;
  currentImage = currentImage % 2;
}

void readSensors() {
  Temperature = dht.readTemperature();
  Humidity = dht.readHumidity();

  sensors.requestTemperatures();
  TemperatureOutdoor = sensors.getTempCByIndex(0);

  // Serial.println(String(Temperature) + " " + String(Humidity) + " " + String(TemperatureOutdoor));
}

bool sendDataToThingSpeak(String requestData) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverName);

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(requestData);

    http.end();

    return true;
  } else {
    return false;
  }
}
