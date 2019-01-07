#include <WiFi.h>
#include <HTTPClient.h>

#ifdef USE_DMA
extern "C"
{
#include "tftspi.h"
#include "tft.h"
}
#else
#include "Adafruit_GFX.h"
#include "WROVER_KIT_LCD.h"

WROVER_KIT_LCD tft;
#endif

/*Uncomment the following line to get the read and display timings*/
//#define TIMINGS

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
#ifndef USE_DMA
    tft.print(".");
#endif
    delay(1000);
  }
}

void setup()
{
  Serial.begin(115200);

#ifdef USE_DMA
  esp_err_t ret;

  tft_disp_type = DISP_TYPE_ILI9341;
  _width = DEFAULT_TFT_DISPLAY_WIDTH;  // smaller dimension
  _height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension
  TFT_PinsInit();

  spi_lobo_device_handle_t spi;

  spi_lobo_bus_config_t buscfg;
  buscfg.mosi_io_num = PIN_NUM_MOSI; // set SPI MOSI pin
  buscfg.miso_io_num = PIN_NUM_MISO; // set SPI MISO pin
  buscfg.sclk_io_num = PIN_NUM_CLK;  // set SPI CLK pin
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 6 * 1024;

  spi_lobo_device_interface_config_t devcfg;
  devcfg.mode = 0;                         // SPI mode 0
  devcfg.clock_speed_hz = 8000000;         // Initial clock out at 8 MHz
  devcfg.spics_io_num = -1;                // we will use external CS pin
  devcfg.spics_ext_io_num = PIN_NUM_CS;    // external CS pin
  devcfg.flags = LB_SPI_DEVICE_HALFDUPLEX; // ALWAYS SET  to HALF DUPLEX MODE!! for display spi

  vTaskDelay(500 / portTICK_RATE_MS);
  printf("\r\n==============================\r\n");
  printf("Pins used: miso=%d, mosi=%d, sck=%d, cs=%d\r\n", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
  printf("==============================\r\n\r\n");

  // ==================================================================
  // ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

  ret = spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
  assert(ret == ESP_OK);
  printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
  disp_spi = spi;

  // ==== Test select/deselect ====
  ret = spi_lobo_device_select(spi, 1);
  assert(ret == ESP_OK);
  ret = spi_lobo_device_deselect(spi);
  assert(ret == ESP_OK);

  printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
  printf("SPI: bus uses native pins: %s\r\n", spi_lobo_uses_native_pins(spi) ? "true" : "false");

  printf("SPI: display init...\r\n");
  TFT_display_init();
  printf("OK\r\n");

  // ---- Detect maximum read speed ----
  max_rdclock = find_rd_speed();
  printf("SPI: Max rd speed = %u\r\n", max_rdclock);

  // ==== Set SPI clock used for display operations ====
  spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
  printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

  TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
  TFT_setRotation(LANDSCAPE);
  TFT_setFont(DEFAULT_FONT, NULL);
  TFT_resetclipwin();
#else
  tft.begin();
  tft.fillScreen(0);
#ifdef MYDEV
  tft.setRotation(1);
#endif
#endif
  reconnect();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
#ifndef USE_DMA
    tft.fillScreen(0);
    tft.print("Reconnecting...");
#endif
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
#ifdef USE_DMA
          TFT_jpg_image(CENTER, CENTER, 0, NULL, buff, len);
#else
          tft.drawJpg(buff, len);
#endif
#ifdef TIMINGS
          d = millis();
          Serial.printf("display toke %d\n", (d - b));
#endif
          free(buff);
        }
      }
    }
  }
  http.end();
}