#include <PCA9557.h>
#include <lvgl.h>
#include <Crowbits_DHT20.h>
#include <SPI.h>

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <climits>
#include <esp_heap_caps.h>
#include "ui.h"

// Backlight control pin on the 5.0-inch RGB panel.
#define TFT_BL 2

/*---------------------------------------------------------------
 * RGB display configuration
 * Configure the 16-bit RGB bus and the 800x480 panel timings.
 *--------------------------------------------------------------*/
class LGFX : public lgfx::LGFX_Device
{
public:

  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;

  /**
   * @brief Configure the RGB bus and fixed 800x480 panel geometry.
   *
   * The bus timing matches the display carrier board so LVGL full-frame
   * buffers are presented at the physical panel resolution.
   *
   * @note Constructed before setup() initializes the display hardware.
   */
  LGFX(void)
  {
    {
      auto busConfig = _bus_instance.config();
      busConfig.panel = &_panel_instance;
      const int8_t dataPins[16] = {8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14};
      memcpy(busConfig.pin_data, dataPins, sizeof(dataPins));
      busConfig.pin_henable = 40;
      busConfig.pin_vsync = 41;
      busConfig.pin_hsync = 39;
      busConfig.pin_pclk = 0;
      busConfig.freq_write = 12000000;
      busConfig.hsync_polarity = 0;
      busConfig.hsync_front_porch = 8;
      busConfig.hsync_pulse_width = 4;
      busConfig.hsync_back_porch = 43;
      busConfig.vsync_polarity = 0;
      busConfig.vsync_front_porch = 8;
      busConfig.vsync_pulse_width = 4;
      busConfig.vsync_back_porch = 12;
      busConfig.pclk_active_neg = 1;
      busConfig.de_idle_high = 0;
      busConfig.pclk_idle_high = 0;
      _bus_instance.config(busConfig);
    }
    {
      // Keep LVGL's drawing area aligned with the panel's physical resolution.
      auto panelConfig = _panel_instance.config();
      panelConfig.memory_width = 800;
      panelConfig.memory_height = 480;
      panelConfig.panel_width = 800;
      panelConfig.panel_height = 480;
      _panel_instance.config(panelConfig);
    }
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);

  }
};


LGFX lcd;
// LVGL uses these dimensions when it creates the display and frame buffers.
static constexpr uint32_t screenWidth = 800;
static constexpr uint32_t screenHeight = 480;

// Current LED state is changed by the generated LVGL button callbacks.

int led;
/* DHT20 object that provides the temperature and humidity samples. */
Crowbits_DHT20 dht20;
SPIClass& spi = SPI;

/* The touch controller and coordinate mapping are selected in touch.h. */
#include "touch.h"

/**
 * @brief Present LVGL's full frame buffer on the RGB panel.
 *
 * @param display LVGL display that requested the flush.
 * @param area Area metadata supplied by LVGL (unused in full-frame mode).
 * @param pixelMap Frame buffer selected by the RGB bus.
 * @return None.
 * @note Called by LVGL whenever a rendered frame is ready.
 */
void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap)
{
  // Switch the RGB controller to the complete frame selected by LVGL.
  if (!lcd._bus_instance.presentFrameBuffer(pixelMap)) {
    Serial.println("LovyanGFX VSYNC frame switch timeout");
  }
  // Always release LVGL's renderer after the panel has accepted the frame.
  lv_display_flush_ready(display);
}

/**
 * @brief Translate GT911 state into LVGL pointer events.
 *
 * @param indev LVGL input device that requested the read.
 * @param data Output state and coordinates consumed by LVGL.
 * @return None.
 * @note Called periodically by LVGL's input driver.
 */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  // Only report a press when the touch driver has a fresh, valid sample.
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      // Forward the mapped panel coordinates to LVGL's pointer device.
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      Serial.print( "Data x :" );
      Serial.println( touch_last_x );

      Serial.print( "Data y :" );
      Serial.println( touch_last_y );
    }
    else if (touch_released())
    {
      // Explicitly release the pointer so widgets do not remain pressed.
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    // A missing sample is treated as released rather than reusing stale data.
    data->state = LV_INDEV_STATE_REL;
  }
  delay(15);
}

/*---------------------------------------------------------------
 * Hardware and LVGL startup
 * Reset the I/O expander, initialize sensors, display, touch, and UI.
 *--------------------------------------------------------------*/
/**
 * @brief Initialize all peripherals and create the LVGL display/input devices.
 *
 * Called once after reset. The function returns early if LCD initialization
 * fails, leaving the serial error message as the diagnostic clue.
 */
PCA9557 Out;
void setup()
{
  Serial.begin(115200);
  // Serial.println("LVGL Widgets Demo");
  Wire.begin(19, 20);
  Out.reset();
  Out.setMode(IO_OUTPUT);  

  Out.setState(IO0, IO_LOW);
  Out.setState(IO1, IO_LOW);
  delay(20);
  Out.setState(IO0, IO_HIGH);
  delay(100);
  Out.setMode(IO1, IO_INPUT);
  dht20.begin();

  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  
  // Display init
  if (!lcd.begin()) {
    Serial.println("lcd.begin() failed!");
    Serial.println("Check Arduino Tools > PSRAM is set to OPI PSRAM.");
    return;
  } else {
    Serial.println("LovyanGFX lcd.begin() OK");
  }
  delay(200);

  lv_init();
  lv_tick_set_cb(millis);
  delay(100);
  
  touch_init();

  // Reuse LovyanGFX's two full-screen RGB buffers for tear-free VSYNC swaps.
  lv_color_t *frameBuffer0 = (lv_color_t *)lcd._bus_instance.getFrameBuffer(0);
  lv_color_t *frameBuffer1 = (lv_color_t *)lcd._bus_instance.getFrameBuffer(1);
  lv_display_t *display = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, my_disp_flush);
  lv_display_set_buffers(display, frameBuffer0, frameBuffer1,
                         screenWidth * screenHeight * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_FULL);

  // Register GT911 as LVGL's pointer source for the generated UI widgets.
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
  ui_init();

  lv_timer_handler();

  Serial.println( "Setup done" );
}

/**
 * @brief Refresh sensor labels, apply the requested LED state, and run LVGL.
 *
 * Sensor reads are limited to once per second to keep I2C traffic predictable;
 * LVGL's timer handler is still serviced on every loop iteration.
 */
void loop()
{
  static uint32_t lastSensorRead = 0;
  static int lastTemperature = INT_MIN;
  static int lastHumidity = INT_MIN;
  uint32_t now = millis();
  // Limit sensor traffic to one measurement per second without blocking LVGL.
  if (now - lastSensorRead >= 1000) {
    lastSensorRead = now;
    int temperature = (int)dht20.getTemperature();
    int humidity = (int)dht20.getHumidity();
    char DHT_buffer[6];
    // Update each label only when its displayed value has changed.
    if (temperature != lastTemperature) {
      lastTemperature = temperature;
      snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", temperature);
      lv_label_set_text(ui_TempLabel, DHT_buffer);
    }
    if (humidity != lastHumidity) {
      lastHumidity = humidity;
      snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", humidity);
      lv_label_set_text(ui_HumiLabel, DHT_buffer);
    }
  }

  // Apply the state requested by the generated UI callbacks to GPIO 38.
  if(led == 1)
    digitalWrite(38, HIGH);
  if(led == 0)
    digitalWrite(38, LOW);
  
  lv_timer_handler(); /* let the GUI do its work */
  delay( 10 );
}
