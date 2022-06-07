#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Servo.h>
#include "secrets.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "weather.hpp"

// Pinout for nodemcu with Arduino framework: https://mechatronicsblog.com/esp8266-nodemcu-pinout-for-arduino-ide/
// | Pin  | Code     | Arduino alias |
// |======|==========|===============|
// | A0   | A0       | A0            |
// | D0   | GPIO 16  | 16            |
// | D1   | GPIO 5   | 5             |
// | D2   | GPIO 4   | 4             |
// | D3   | GPIO 0   | 0             |
// | D4   | GPIO 2   | 2             |
// | D5   | GPIO 14  | 14            |
// | D6   | GPIO 12  | 12            |
// | D7   | GPIO 13  | 13            |
// | D8   | GPIO 15  | 15            |
// | SD2  | GPIO 9   | 9             |
// | SD3  | GPIO 10  | 10            |
// | RX   | GPIO 3   | 3             |
// | TX   | GPIO 1   | 1             |

#define PIN_SERVO 2 // D4
#define PIN_LED 0 // D3

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(35, PIN_LED, NEO_GRB + NEO_KHZ400);

WiFiClient client;
int status = WL_IDLE_STATUS;
String text;
int jsonend = 0;
boolean startJson = false;
boolean has_connected = false;

void printWiFiStatus();
void parseJson(const char * jsonString);
void makehttpRequest();
void diffDataAction(String nowT, String later, String weatherType);
void colorWipe(uint32_t c, uint8_t wait);
void rainbow(uint8_t wait);
void rainbowCycle(uint8_t wait);
void rainbow(uint8_t wait);
void theaterChase(uint32_t c, uint8_t wait);
void theaterChaseRainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);

int rainLed = 2;  // Indicates rain
int clearLed = 3; // Indicates clear sky or sunny
int snowLed = 4;  // Indicates snow
int hailLed = 5;  // Indicates hail

// Open Weather Map API server name
const char server[] = "api.openweathermap.org";

Servo servo;
int pos = 0; // Servo position

enum weather_t {
  cloudy,
  rainy,
  thunder,
  clear,
  snowy,
  haily
};

void set_weather(weather_t weather, int sun_status_int) {
  if (weather == cloudy) {
    Serial.println("Cloudy");
    colorWipe(strip.Color(255, 255, 255), 50);
  } else if (weather == rainy) {
    Serial.println("Rainy");
    colorWipe(strip.Color(20, 20, 255), 50);
  } else if (weather == thunder) {
    Serial.println("Thunder");
    for (int t=0; t<3; t++) {
      colorWipe(strip.Color(20, 20, 255), 30);
      colorWipe(strip.Color(255, 255, 255), 0);
    }
  } else if (weather == clear) {
    Serial.println("Clear");
    colorWipe(strip.Color(127, 127, 0), 50);
  }
  // sun_status = max(0.0, min(sun_status, 1.0)) * 180;
  // int sun_status_int = sun_status;
  servo.write(sun_status_int);
  Serial.print("Sun status: ");
  Serial.println(sun_status_int);
}

// print Wifi status
void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

//to parse json data recieved from OWM
void parseJson(const char * jsonString) {
  //StaticJsonBuffer<4000> jsonBuffer;
  const size_t bufferSize = 2*JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(2) + 4*JSON_OBJECT_SIZE(1) + 3*JSON_OBJECT_SIZE(2) + 3*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 2*JSON_OBJECT_SIZE(7) + 2*JSON_OBJECT_SIZE(8) + 720;
  DynamicJsonDocument jsonBuffer(bufferSize);

  // FIND FIELDS IN JSON TREE
  auto error = deserializeJson(jsonBuffer, jsonString);
  if (error) {
    Serial.println("parseObject() failed");
    return;
  }
  JsonObject root = jsonBuffer.as<JsonObject>();

  JsonArray list = root["list"];
  JsonObject nowT = list[0];
  JsonObject later = list[1];

  // including temperature and humidity for those who may wish to hack it in
  
  String city = root["city"]["name"];
  
  float tempNow = nowT["main"]["temp"];
  float humidityNow = nowT["main"]["humidity"];
  String weatherNow = nowT["weather"][0]["description"];

  float tempLater = later["main"]["temp"];
  float humidityLater = later["main"]["humidity"];
  String weatherLater = later["weather"][0]["description"];

  // checking for four main weather possibilities
  diffDataAction(weatherNow, weatherLater, "clear");
  diffDataAction(weatherNow, weatherLater, "rain");
  diffDataAction(weatherNow, weatherLater, "snow");
  diffDataAction(weatherNow, weatherLater, "hail");
  
  Serial.println();
}

// to request data from OWM
void makehttpRequest() {
  // close any connection before send a new request to allow client make connection to server
  client.stop();

  // if there's a successful connection:
  if (client.connect(server, 80)) {
    // Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET /data/2.5/forecast?q=" + nameOfCity + "&APPID=" + open_weather_map_api_key + "&mode=json&units=metric&cnt=2 HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
    
    char c = 0;
    while (client.available()) {
      c = client.read();
      // since json contains equal number of open and close curly brackets, this means we can determine when a json is completely received  by counting
      // the open and close occurences,
      //Serial.print(c);
      if (c == '{') {
        startJson = true;         // set startJson true to indicate json message has started
        jsonend++;
      }
      if (c == '}') {
        jsonend--;
      }
      if (startJson == true) {
        text += c;
      }
      // if jsonend = 0 then we have have received equal number of curly braces 
      if (jsonend == 0 && startJson == true) {
        parseJson(text.c_str());  // parse c string text in parseJson function
        text = "";                // clear text string for the next time
        startJson = false;        // set startJson to false to indicate that a new message has not yet started
        has_connected = true;
      }
    }
  }
  else {
    // if no connction was made:
    Serial.println("connection failed");
    return;
  }
}

//representing the data
void diffDataAction(String nowT, String later, String weatherType) {
  int indexNow = nowT.indexOf(weatherType);
  int indexLater = later.indexOf(weatherType);
  // if weather type = rain, if the current weather does not contain the weather type and the later message does, send notification
  if (weatherType == "rain") { 
    if (indexNow == -1 && indexLater != -1) {
      set_weather(rainy, 0);
      Serial.println("Oh no! It is going to " + weatherType + " later! Predicted " + later);
    }
  }
  // for snow
  else if (weatherType == "snow") {
    if (indexNow == -1 && indexLater != -1) {
      set_weather(snowy, 0);
      Serial.println("Oh no! It is going to " + weatherType + " later! Predicted " + later);
    }
    
  }
  // can't remember last time I saw hail anywhere but just in case
  else if (weatherType == "hail") { 
   if (indexNow == -1 && indexLater != -1) {
      set_weather(haily, 0);
      Serial.println("Oh no! It is going to " + weatherType + " later! Predicted " + later);
   }

  }
  // for clear sky, if the current weather does not contain the word clear and the later message does, send notification that it will be sunny later
  else {
    if (indexNow == -1 && indexLater != -1) {
      Serial.println("It is going to be sunny later! Predicted " + later);
      set_weather(clear, 0);
    }
  }
}

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

void setup() {
  Serial.begin(115200);
  Serial.println("Starting!");
  strip.begin();
  strip.setBrightness(255);
  strip.show(); // Initialize all pixels to 'off'
  servo.attach(PIN_SERVO, 800, 2600); // attaches the servo on pin 18 to the servo object

  text.reserve(JSON_BUFF_DIMENSION);
  
  WiFi.begin(ssid,pass);
  Serial.println("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected");
  printWiFiStatus();
}


void loop() {
  // Serial.println("Wipe");
  // colorWipe(strip.Color(255, 0, 0), 50);
  // delay(1000);
  // colorWipe(strip.Color(0, 255, 0), 50);
  // delay(1000);
  // colorWipe(strip.Color(0, 0, 255), 50);
  // delay(1000);

  //OWM requires 10mins between request intervals
  //check if 10mins has passed then conect again and pull
  if (millis() - lastConnectionTime > postInterval || ! has_connected) {
    Serial.println("Requesting");
    lastConnectionTime = millis();
    lastConnectionTime = millis();
    makehttpRequest();
  }

  // set_weather(rainy, random(0, 180));
  // set_weather(thunder, random(0, 180));
  // set_weather(clear, random(0, 180));
  // Serial.println("Rainbow");
  rainbow(20);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}