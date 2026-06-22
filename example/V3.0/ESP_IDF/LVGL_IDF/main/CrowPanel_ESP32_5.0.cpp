#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

//UI
#include "ui.h"
#include <SPI.h>

#include <DHT20.h>

static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static lv_color_t *lvgl_draw_buf = nullptr;

#include <Arduino_GFX_Library.h>
#include <PCA9557.h>
#define TFT_BL 2

PCA9557 Out;

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 0 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 9000000 /* prefer_speed */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 480 /* bounce_buffer_size_px */
);

Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
    800 /* width */, 480 /* height */, bus, 0 /* rotation */, false /* auto_flush */);

#include "touch.h"

DHT20 dht20;

// LED control variable (referenced by UI button events)
int led = 0;
static int last_led_state = 0;

// Use labels from UI (ui_Label1 for temperature, ui_Label2 for humidity)

void update_sensor_values() {
  int temperature = dht20.getTemperature();
  int humidity = dht20.getHumidity();

  if (ui_Label1) lv_label_set_text_fmt(ui_Label1, "%d", temperature);
  if (ui_Label2) lv_label_set_text_fmt(ui_Label2, "%d", humidity);

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humi: ");
  Serial.print(humidity);
  Serial.println(" %");
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    lcd->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    lcd->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    if (touch_has_signal())
    {
        if (touch_touched())
        {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
            Serial.printf("[LVGL] x=%d y=%d\n", touch_last_x, touch_last_y);
        }
        else if (touch_released())
        {
            data->state = LV_INDEV_STATE_REL;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
    delay(15);
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("setup() start");

    dht20.begin();

    // PCA9557 LCD power/reset control (matching Arduino version logic)
    Wire.begin(19, 20);
    Out.reset();
    Out.setMode(IO_OUTPUT);        // Set all pins as output first
    Out.setState(0, IO_LOW);
    Out.setState(1, IO_LOW);
    delay(20);
    Out.setState(0, IO_HIGH);
    delay(100);
    Out.setMode(1, IO_INPUT);      // IO1 as input

    // Control pin 38 (matching Arduino version)
    pinMode(38, OUTPUT);
    digitalWrite(38, LOW);

    lv_init();

    Serial.println("Calling lcd->begin()...");
    bool ok = lcd->begin();
    Serial.printf("lcd->begin() returned: %d\r\n", ok);
    if (!ok) {
        Serial.println("lcd->begin() failed, halt");
        while (1) {
            delay(1000);
        }
    }

    // Test: fill screen with black (matching Arduino version)
    lcd->fillScreen(0x0000);
    delay(100);

    touch_init();
    delay(50);

    //pinMode(38, OUTPUT);
    //digitalWrite(38, LOW);

    screenWidth = lcd->width();
    screenHeight = lcd->height();

    // Use 1/10 of screen as draw buffer (recommended for LVGL 8.x)
    const size_t draw_buf_pixels = screenWidth * screenHeight / 10;
    lvgl_draw_buf = (lv_color_t *)heap_caps_malloc(draw_buf_pixels * sizeof(lv_color_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!lvgl_draw_buf) {
        Serial.println("draw buffer alloc failed");
        while (1) {
            delay(1000);
        }
    }
    lv_disp_draw_buf_init(&draw_buf, lvgl_draw_buf, NULL, draw_buf_pixels);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();

    // Turn on backlight (matching Arduino version timing)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    Serial.println("Setup done");
}

void loop()
{
    static uint32_t last_tick_ms = millis();

    uint32_t now = millis();
    lv_tick_inc(now - last_tick_ms);
    last_tick_ms = now;

    lv_timer_handler();
    delay(5);

    // Handle LED state changes from UI button events
    if (led != last_led_state) {
        last_led_state = led;
        // TODO: Change LED_PIN to match your actual LED connection
        const int LED_PIN = 38;
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, led ? HIGH : LOW);
        Serial.printf("LED state changed: %d\n", led);
    }

    static unsigned long last_update = 0;
    if (millis() - last_update > 2000) {
        update_sensor_values();
        last_update = millis();
    }
}