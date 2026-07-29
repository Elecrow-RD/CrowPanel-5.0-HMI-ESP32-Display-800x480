/*---------------------------------------------------------------
 * CrowPanel 5.0-inch LVGL application
 * Initializes the RGB display, GT911 touch controller, DHT20 sensor,
 * and generated UI under ESP-IDF's Arduino component.
 *--------------------------------------------------------------*/
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <DHT20.h>
#include <PCA9557.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "ui.h"

namespace {

// Board pins, update cadence, and display-buffer parameters used at runtime.
constexpr uint8_t kBacklightPin = 2;
constexpr uint8_t kLedPin = 38;
constexpr uint32_t kSensorUpdateIntervalMs = 2000;
constexpr uint32_t kDrawBufferRows = 48;
constexpr int32_t kPixelClockHz = 16000000;
constexpr size_t kBounceBufferPixels = 800 * 10;

PCA9557 output_expander;
DHT20 dht20;

Arduino_ESP32RGBPanel *rgb_bus = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 0 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */,
    43 /* hsync_back_porch */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */,
    4 /* vsync_pulse_width */, 12 /* vsync_back_porch */, 1 /* pclk_active_neg */,
    kPixelClockHz /* prefer_speed */, false /* useBigEndian */, 0 /* de_idle_high */,
    0 /* pclk_idle_high */, kBounceBufferPixels /* bounce_buffer_size_px */);

// The LVGL display must use the panel's physical 800x480 resolution.
Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
    800 /* width */, 480 /* height */, rgb_bus, 0 /* rotation */, true /* auto_flush */);

}  // namespace

#include "touch.h"

namespace {

uint8_t *lvgl_draw_buffer = nullptr;
int last_led_state = 0;

/**
 * @brief Stop execution after an unrecoverable initialization failure.
 *
 * @param message Diagnostic text retained in the serial monitor.
 * @note Called only when the firmware cannot safely continue.
 */
[[noreturn]] void halt(const char *message)
{
    Serial.println(message);
    while (true) {
        delay(1000);
    }
}

/**
 * @brief Read DHT20 values and update the generated UI labels.
 *
 * Called from loop() at the configured interval.
 */
void update_sensor_values()
{
    const int temperature = dht20.getTemperature();
    const int humidity = dht20.getHumidity();

    if (ui_TempLabel != nullptr) {
        lv_label_set_text_fmt(ui_TempLabel, "%d", temperature);
    }
    if (ui_HumiLabel != nullptr) {
        lv_label_set_text_fmt(ui_HumiLabel, "%d", humidity);
    }

    Serial.printf("Temp: %d C\nHumi: %d %%\n", temperature, humidity);
}

/**
 * @brief Copy an LVGL partial-render buffer to the RGB display.
 *
 * @param display LVGL display requesting the flush.
 * @param area Pixel rectangle represented by pixel_map.
 * @param pixel_map RGB565 pixel data.
 */
void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map)
{
    // Convert LVGL's inclusive area into the bitmap dimensions expected by GFX.
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;
    auto *pixels = reinterpret_cast<uint16_t *>(pixel_map);

    lcd->draw16bitRGBBitmap(area->x1, area->y1, pixels, width, height);
    // Release LVGL's renderer only after the partial area has been copied.
    lv_display_flush_ready(display);
}

/**
 * @brief Convert the GT911 sample into an LVGL pointer event.
 *
 * @param indev LVGL input device (unused).
 * @param data Output state and coordinates.
 */
void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    // Default to released so an absent sample cannot leave a widget pressed.
    data->state = LV_INDEV_STATE_RELEASED;

    // Publish mapped GT911 coordinates only while a valid touch is present.
    if (touch_has_signal() && touch_touched()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;
    }
}

/**
 * @brief Allocate LVGL draw memory and register display/input callbacks.
 *
 * Called once after the hardware display and touch controller are ready.
 */
void initialize_display_driver()
{
    const uint32_t screen_width = lcd->width();
    const uint32_t screen_height = lcd->height();
    const size_t draw_buffer_size = screen_width * kDrawBufferRows * sizeof(uint16_t);

    // Keep the partial draw buffer in PSRAM to preserve internal RAM.
    lvgl_draw_buffer = static_cast<uint8_t *>(
        heap_caps_malloc(draw_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (lvgl_draw_buffer == nullptr) {
        halt("LVGL draw buffer allocation failed");
    }

    // Register the RGB565 partial renderer and its panel-copy callback.
    lv_display_t *display = lv_display_create(screen_width, screen_height);
    if (display == nullptr) {
        halt("LVGL display creation failed");
    }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, display_flush);
    lv_display_set_buffers(display, lvgl_draw_buffer, nullptr, draw_buffer_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Bind GT911 as the pointer device for this display.
    lv_indev_t *touch_indev = lv_indev_create();
    if (touch_indev == nullptr) {
        halt("LVGL input device creation failed");
    }
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, touchpad_read);
    lv_indev_set_display(touch_indev, display);
}

}  // namespace

extern "C" {
// Share the generated C UI's LED request with this C++ application.
int led = 0;
}

/**
 * @brief Initialize peripherals, LVGL, and the generated screen.
 *
 * Called once after boot. Fatal allocation or hardware errors stop in halt()
 * so the serial monitor identifies the failed stage.
 */
void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("setup() start");

    dht20.begin();

    Wire.begin(19, 20);
    output_expander.reset();
    output_expander.setMode(IO_OUTPUT);
    output_expander.setState(0, IO_LOW);
    output_expander.setState(1, IO_LOW);
    delay(20);
    output_expander.setState(0, IO_HIGH);
    delay(100);
    output_expander.setMode(1, IO_INPUT);

    pinMode(kLedPin, OUTPUT);
    digitalWrite(kLedPin, LOW);

    lv_init();

    Serial.println("Calling lcd->begin()...");
    const bool display_started = lcd->begin();
    Serial.printf("lcd->begin() returned: %d\r\n", display_started);
    if (!display_started) {
        halt("lcd->begin() failed");
    }

    lcd->fillScreen(0x0000);
    delay(100);

    touch_init();
    delay(50);

    initialize_display_driver();
    ui_init();

    pinMode(kBacklightPin, OUTPUT);
    digitalWrite(kBacklightPin, HIGH);

    Serial.println("Setup done");
}

/**
 * @brief Run LVGL timers, apply LED changes, and refresh sensor labels.
 *
 * Called continuously by the Arduino component task.
 */
void loop()
{
    static uint32_t last_tick_ms = millis();
    static uint32_t last_sensor_update_ms = 0;

    const uint32_t now = millis();
    // Advance LVGL with real elapsed time before processing its timers.
    lv_tick_inc(now - last_tick_ms);
    last_tick_ms = now;

    lv_timer_handler();
    delay(5);

    // Touch-triggered LED requests are applied only when the state changes.
    if (led != last_led_state) {
        last_led_state = led;
        digitalWrite(kLedPin, led != 0 ? HIGH : LOW);
        Serial.printf("LED state changed: %d\n", led);
    }

    // Sensor work runs at a slower cadence so UI handling stays responsive.
    if (now - last_sensor_update_ms >= kSensorUpdateIntervalMs) {
        update_sensor_values();
        last_sensor_update_ms = now;
    }
}
