#include "Adafruit_GFX.h"
#include "WROVER_KIT_LCD.h"
#include <WiFi.h>
#include <HTTPClient.h>

/*Uncomment the following line to get the read and display timings*/
#define TIMINGS

//Objects
WROVER_KIT_LCD tft;
HTTPClient http;

//Variables
const char *ssid = "M5Cam";
const char *password = "";
String url = "http://192.168.4.1/capture";
int httpCode, len;

//Variables for measurements
#ifdef TIMINGS
int a, b, d;
#endif

void reconnect()
{
  WiFi.disconnect();
  delay(500);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    tft.print(".");
    delay(1000);
  }
}

void setup()
{
  Serial.begin(115200);
  tft.begin();
  #ifdef MYDEV
  tft.setRotation(1);
  #endif
  tft.fillScreen(0);
  reconnect();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    tft.fillScreen(0);
    tft.print("Reconnecting...");
    delay(1000);
    reconnect();
  }
  else
  {
#ifdef TIMINGS
    a = millis();
#endif
    http.begin(url);
    httpCode = http.GET();
    // HTTP header has been send and Server response header has been handled
    if (httpCode <= 0)
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    else
    {
      if (httpCode != HTTP_CODE_OK)
      {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      }
      else
      {
        // get lenght of document (is -1 when Server sends no Content-Length header)
        len = http.getSize();

        if (len <= 0)
        {
          Serial.printf("[HTTP] Unknow content size: %d\n", len);
        }
        else
        {
          // get tcp stream
          WiFiClient *stream = http.getStreamPtr();
          //Allocate buffer for reading
          uint8_t *buff = (uint8_t *)malloc(len * sizeof(uint8_t));
          int chunks = len;
          // read all data from server
          while (http.connected() && (chunks > 0 || len == -1))
          {
            // get available data size
            size_t size = stream->available();

            if (size)
            {
              int chunk_size = ((size > (len * sizeof(uint8_t))) ? (len * sizeof(uint8_t)) : size);
              int indexer = stream->readBytes(buff, chunk_size);
              buff += indexer;

              if (chunks > 0)
              {
                chunks -= indexer;
              }
            }
          }
          buff -= len;
#ifdef TIMINGS
          b = millis();
          Serial.printf("read toke %d\n", (b - a));
          b = millis();
#endif
          tft.drawJpg(buff, len);
#ifdef TIMINGS
          d = millis();
          Serial.printf("display toke %d\n", (d - b));
#endif
          free(buff);
        }
      }
    }
    http.end();
  }
}