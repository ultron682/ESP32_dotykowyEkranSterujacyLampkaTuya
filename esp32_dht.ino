#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "DHT.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ILI9341.h> // Hardware-specific library for ILI9341
#include <SPI.h>

#include "esp32_dht_bmp.h"

//
// #include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
// #include <SPI.h>

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

// #define TFT_CS 12 //SS/CS
#define TFT_RST 4
#define TFT_DC 5
// #define TFT_MOSI 11  // Data out
// #define TFT_SCL 7  // Clock out

// Adafruit_ST7735 tft = Adafruit_ST7735(SS,  TFT_DC, MOSI, SCK, TFT_RST);
Adafruit_ILI9341 tft = Adafruit_ILI9341(SS, TFT_DC, TFT_RST);
//Adafruit_ILI9341 tft = Adafruit_ILI9341(SS, TFT_DC, MOSI, SCK, TFT_RST, MISO);

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

  tft.begin(240, 320);
  tft.invertDisplay(false);
 // tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  //tft.setRotation(1);
  // tft.fillScreen(ILI9341_BLACK);
  //tft.setCursor(0, 0);
  // tft.cp437(true);
  tft.setTextWrap(true);
  tft.setTextSize(2);

  WiFi.onEvent(onWiFiEvent);
  connectWifi(false);
  sendWakeOnToThingSpeak();

  delay(500);

  //tft.setTextColor(ILI9341_BLUE);
  //tft.print("Hello world");
  Serial.println(String(SS) + "  " + String(SCK)+ "  " + String(MOSI)+ "  " + String(MISO));

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);

  loopTask();
}

void loop()
{
  if (digitalRead(33) == LOW && currentState == 0)
  {
    currentState = 1;
    turnOnLamp();
  }
  else if (digitalRead(33) == HIGH && currentState == 1)
  {
    currentState = 0;
    turnOffLamp();
  }

  if (millis() - previousMillis >= 5000 /*30000*/)
  {
    //setCpuFrequencyMhz(240); // CPU
    if (connectWifi(false))
    {
      loopTask();
      WiFi.mode(WIFI_OFF);
    }
    else
    {
      ESP.restart();
    }
    //setCpuFrequencyMhz(40); // CPU
  }

  delay(300);
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
  tft.setCursor(0, 0);

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

  drawText("Temp: " + String(Temperature) + "\367C", (Temperature >= 19 && Temperature <= 23) ? ILI9341_GREEN : ILI9341_RED);
  drawText("Wilg: " + String(Humidity) + " %", (Humidity >= 35 && Humidity <= 60) ? ILI9341_GREEN : ILI9341_RED);
  drawText("Zewn: " + String(TemperatureOutdoor) + "\367C");
}

void turnOnLamp()
{
  digitalWrite(34, HIGH);
  delay(70);
  digitalWrite(34, LOW);
  delay(400);
  digitalWrite(34, HIGH);
  delay(70);
  digitalWrite(34, LOW);
}

void turnOffLamp()
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
  tft.setTextColor(ILI9341_WHITE);
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

      //if (currentImage == 0)
        tft.drawPixel(col, row, evive_in_hand[buffidx]);
      //else if (currentImage == 1)
      //  tft.drawPixel(col, row, evive_in_hand2[buffidx]);
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
