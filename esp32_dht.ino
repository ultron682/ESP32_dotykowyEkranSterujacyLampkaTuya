#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "DHT.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <FS.h>
#include "Free_Fonts.h" // Include the header file attached to this sketch

#include <TFT_eSPI.h>    // Hardware-specific library
#include <TFT_eWidget.h> // Widget library

#include "esp32_dht_bmp.h"

const String ssid = "Inez 2,4";
const String password = "56964592";

const char *serverName = "http://api.thingspeak.com/update";
unsigned long myChannelNumber = 2058504;
String apiKey = "3E55L1C2B988LHQJ";

uint8_t DHTPin = 6;
#define DHTTYPE DHT22
DHT dht(DHTPin, DHTTYPE);

float Temperature;
float Humidity;
float TemperatureOutdoor;

#define TFT_RST 4
#define TFT_DC 5

TFT_eSPI tft = TFT_eSPI();
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

ButtonWidget btnL = ButtonWidget(&tft);
ButtonWidget btnR = ButtonWidget(&tft);

#define BUTTON_W 100
#define BUTTON_H 50

ButtonWidget *btn[] = {&btnL, &btnR};
;
uint8_t buttonCount = sizeof(btn) / sizeof(btn[0]);

uint8_t oneWireBus = 10;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

unsigned long previousMillis = 0;

short currentImage = 2;
short currentState = 1;

void setup()
{
  pinMode(33, INPUT_PULLUP);
  pinMode(34, OUTPUT);
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
  sendWakeOnToThingSpeak();

  delay(500);

  loopTask();

  // Calibrate the touch screen and retrieve the scaling factors
  touch_calibrate();
  initButtons();
}

void loop()
{
  // if (digitalRead(33) == LOW && currentState == 0)
  // {
  //   currentState = 1;
  //   turnOnLEDS();
  // }
  // else if (digitalRead(33) == HIGH && currentState == 1)
  // {
  //   currentState = 0;
  //   turnOffLEDS();
  // }

  if (millis() - previousMillis >= 30000)
  {
    // // setCpuFrequencyMhz(240); // CPU
     if (connectWifi(false))
     {
      loopTask();
      WiFi.mode(WIFI_OFF);
     }
    // else
    // {
    //   ESP.restart();
    // }
    // setCpuFrequencyMhz(40); // CPU
  }

  static uint32_t scanTime = millis();
  uint16_t t_x = 9999, t_y = 9999; // To store the touch coordinates

  // Scan keys every 50ms at most
  if (millis() - scanTime >= 50)
  {
    // Pressed will be set true if there is a valid touch on the screen
    bool pressed = tft.getTouch(&t_x, &t_y);
    scanTime = millis();
    for (uint8_t b = 0; b < buttonCount; b++)
    {
      if (pressed)
      {
        if (btn[b]->contains(t_x, t_y))
        {
          btn[b]->press(true);
          btn[b]->pressAction();
        }
      }
      else
      {
        btn[b]->press(false);
        btn[b]->releaseAction();
      }
    }
  }
}

void btnL_pressAction(void)
{
  if (btnL.justPressed())
  {
    Serial.println("Left button just pressed");
    btnL.drawSmoothButton(true);

    turnOnLEDS();
  }
}

void btnL_releaseAction(void)
{
  static uint32_t waitTime = 1000;
  if (btnL.justReleased())
  {
    Serial.println("Left button just released");
    btnL.drawSmoothButton(false);
    btnL.setReleaseTime(millis());
    waitTime = 10000;
  }
  else
  {
    if (millis() - btnL.getReleaseTime() >= waitTime)
    {
      waitTime = 1000;
      btnL.setReleaseTime(millis());
      btnL.drawSmoothButton(btnL.getState());
    }
  }
}

void btnR_pressAction(void)
{
  if (btnR.justPressed())
  {
    Serial.println("Left button just pressed");
    btnR.drawSmoothButton(true);

    turnOffLEDS();
  }
}

void btnR_releaseAction(void)
{
  static uint32_t waitTime = 1000;
  if (btnR.justReleased())
  {
    Serial.println("Left button just released");
    btnR.drawSmoothButton(false);
    btnR.setReleaseTime(millis());
    waitTime = 10000;
  }
  else
  {
    if (millis() - btnR.getReleaseTime() >= waitTime)
    {
      waitTime = 1000;
      btnR.setReleaseTime(millis());
      btnR.drawSmoothButton(btnR.getState());
    }
  }
}

void initButtons()
{
  uint16_t x = (tft.width() - BUTTON_W) - 3;
  uint16_t y =  3;
  btnL.initButtonUL(x, y, BUTTON_W, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "ON", 1);
  btnL.setPressAction(btnL_pressAction);
  btnL.setReleaseAction(btnL_releaseAction);
  btnL.drawSmoothButton(false, 2, TFT_BLACK); // 3 is outline width, TFT_BLACK is the surrounding background colour for anti-aliasing

  y =  BUTTON_H + 6;
  btnR.initButtonUL(x, y, BUTTON_W, BUTTON_H, TFT_WHITE, TFT_RED, TFT_BLACK, "OFF", 1);
  btnR.setPressAction(btnR_pressAction);
  btnR.setReleaseAction(btnR_releaseAction);
  btnR.drawSmoothButton(false, 2, TFT_BLACK); // 3 is outline width, TFT_BLACK is the surrounding background colour for anti-aliasing
}

void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!LittleFS.begin())
  {
    Serial.println("Formating file system");
    LittleFS.format();
    LittleFS.begin();
  }

  // check if calibration file exists and size is correct
  if (LittleFS.exists(CALIBRATION_FILE))
  {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      LittleFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = LittleFS.open(CALIBRATION_FILE, "r");
      if (f)
      {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL)
  {
    // calibration data valid
    tft.setTouch(calData);
  }
  else
  {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL)
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f)
    {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}

void onWiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi begin failed ");
    // try reconnect here (after delay???)
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WiFi begin succeeded ");
    // Connected successfully
    break;
  default:
    Serial.println("WiFi default");
  }
}

uint8_t timeoutWifiConnect = 0;
bool connectWifi(bool quiet)
{
  if (WiFi.status() == WL_CONNECTED)
    return true;

  timeoutWifiConnect = 0;
  tft.setCursor(0, 0);

  digitalWrite(LED_BUILTIN, HIGH);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    previousMillis = millis();

    delay(200);
    if (!quiet)
      tft.print(".");

    if (timeoutWifiConnect >= 50)
    {
      return false;
    }
    timeoutWifiConnect++;
  }

  digitalWrite(LED_BUILTIN, LOW);
  return true;
}

void loopTask()
{
  getSensorReadings();
  sendTemperatureToThingSpeak();
  tft.setCursor(0, 185);

  drawImage();
  if (currentImage == 0)
  {
    currentImage = 1;
  }
  else if (currentImage == 1)
  {
    currentImage = 2;
  }
  else
  {
    currentImage = 0;
  }

  drawText("Temp: " + String(Temperature) + "\367C", (Temperature >= 19 && Temperature <= 23) ? TFT_GREEN : TFT_RED);
  drawText("Wilg: " + String(Humidity) + " %", (Humidity >= 35 && Humidity <= 60) ? TFT_GREEN : TFT_RED);
  drawText("Zewn: " + String(TemperatureOutdoor) + "\367C");
}

void turnOnLEDS()
{
  digitalWrite(34, HIGH);
  delay(70);
  digitalWrite(34, LOW);
  delay(400);
  digitalWrite(34, HIGH);
  delay(70);
  digitalWrite(34, LOW);
}

void turnOffLEDS()
{
  digitalWrite(34, HIGH);
  delay(70);
  digitalWrite(34, LOW);
  delay(100);
  digitalWrite(34, HIGH);
  delay(70);
  digitalWrite(34, LOW);
}

void drawText(String text)
{
  // tft.setTextColor(ILI9341_WHITE, 0x0000);
  tft.setTextColor(TFT_WHITE);
  tft.println(text);
}

void drawText(String text, uint16_t color)
{
  // tft.setTextColor(color, 0x0000);
  tft.setTextColor(color);
  tft.println(text);
}

void drawImage()
{
  int h = 240, w = 320, row = 0, col = 0, buffidx = 0;
  for (row = 0; row < h; row++)
  { // For each scanline...
    for (col = 0; col < w; col++)
    { // For each pixel...
      // To read from Flash Memory, pgm_read_XXX is required.
      // Since image is stored as uint16_t, pgm_read_word is used as it uses 16bit address

      // if (currentImage == 0)
      tft.drawPixel(col, row, evive_in_hand[buffidx]);
      // else if (currentImage == 1)
      //   tft.drawPixel(col, row, evive_in_hand2[buffidx]);
      // else
      //  tft.drawPixel(col, row, bejba[buffidx]);
      buffidx++;
    } // end pixel
  }
}

void getSensorReadings()
{
  Temperature = dht.readTemperature();
  Humidity = dht.readHumidity();

  sensors.requestTemperatures();
  TemperatureOutdoor = sensors.getTempCByIndex(0);

  // Serial.println(String(Temperature) + " " + String(Humidity) + " " + String(TemperatureOutdoor));
}

bool sendWakeOnToThingSpeak()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String httpRequestData = "api_key=" + apiKey + "&field5=" + String(millis());
    int httpResponseCode = http.POST(httpRequestData);

    http.end();
    return true;
  }
  else
  {
    return false;
  }
}

bool sendTemperatureToThingSpeak()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverName);

    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // Data to send with HTTP POST
    String httpRequestData = "api_key=" + apiKey + "&field1=" + String(Temperature) + "&field2=" + String(Humidity) + "&field3=" + String(TemperatureOutdoor);
    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);

    // Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);

    // Free resources
    http.end();

    return true;
  }
  else
  {
    return false;
  }
}
