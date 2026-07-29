#include <PCA9557.h>
#include <lvgl.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#define TFT_BL 2

// UART pins used by the external GPS receiver.
#define GPS_RX 44
#define GPS_TX 43
HardwareSerial gpsSerial(1);

// NMEA line buffer and the latest decoded navigation values.
char nmeaLine[128];
byte nmeaIndex = 0;

struct {
  bool valid = false;
  float lat = 0;
  float lon = 0;
  char latDir = 'N';
  char lonDir = 'E';
  float alt = 0;
  float speed = 0;
  uint8_t sats = 0;
  uint8_t fixType = 0;
  char timeStr[10] = "--:--:--";
  char dateStr[12] = "----/--/--";
} gps;

class LGFX : public lgfx::LGFX_Device
{
public:

  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;

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
static constexpr uint32_t screenWidth = 800;
static constexpr uint32_t screenHeight = 480;

/*******************************************************************************
   Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"

/* Display flushing */
void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap)
{
  if (!lcd._bus_instance.presentFrameBuffer(pixelMap)) {
    Serial.println("LovyanGFX VSYNC frame switch timeout");
  }
  lv_display_flush_ready(display);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      Serial.print( "Data x :" );
      Serial.println( touch_last_x );

      Serial.print( "Data y :" );
      Serial.println( touch_last_y );
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

// ==================== GPS Parsing Functions ====================
/** Validate the XOR checksum appended to an NMEA sentence. */
bool checkNMEA(const char* line) {
  const char* star = strchr(line, '*');
  if (!star || strlen(star) < 3) return false;
  byte calc = 0;
  for (const char* p = line + 1; *p && *p != '*'; p++) {
    calc ^= *p;
  }
  byte recv = (byte)strtol(star + 1, NULL, 16);
  return calc == recv;
}

/** Convert NMEA degrees/minutes text to signed decimal degrees. */
float dmToDd(const char* dm, char dir) {
  if (!dm || strlen(dm) < 3) return 0;
  float val = atof(dm);
  int deg = (int)(val / 100);
  float min = val - deg * 100;
  float dd = deg + min / 60.0;
  return (dir == 'S' || dir == 'W') ? -dd : dd;
}

/** Decode a GGA fix sentence into time, position, altitude, and satellites. */
void parseGGA(char* p) {
  char* tok = strtok(p, ",");
  tok = strtok(NULL, ","); // time
  if (tok && strlen(tok) >= 6) {
    snprintf(gps.timeStr, sizeof(gps.timeStr), "%c%c:%c%c:%c%c",
             tok[0], tok[1], tok[2], tok[3], tok[4], tok[5]);
  }
  tok = strtok(NULL, ","); // lat
  char* lat = tok;
  tok = strtok(NULL, ","); // N/S
  char latD = tok ? tok[0] : 'N';
  tok = strtok(NULL, ","); // lon
  char* lon = tok;
  tok = strtok(NULL, ","); // E/W
  char lonD = tok ? tok[0] : 'E';
  tok = strtok(NULL, ","); // fix
  gps.fixType = tok ? atoi(tok) : 0;
  gps.valid = (gps.fixType > 0);
  tok = strtok(NULL, ","); // sats
  gps.sats = tok ? atoi(tok) : 0;
  tok = strtok(NULL, ","); // hdop
  tok = strtok(NULL, ","); // alt
  gps.alt = (tok && strlen(tok) > 0) ? atof(tok) : 0;
  
  if (gps.valid) {
    gps.lat = dmToDd(lat, latD);
    gps.lon = dmToDd(lon, lonD);
    gps.latDir = latD;
    gps.lonDir = lonD;
  }
}

/** Decode an RMC sentence into position, speed, date, and validity. */
void parseRMC(char* p) {
  char* tok = strtok(p, ",");
  tok = strtok(NULL, ","); // time
  tok = strtok(NULL, ","); // status
  gps.valid = (tok && tok[0] == 'A');
  tok = strtok(NULL, ","); // lat
  char* lat = tok;
  tok = strtok(NULL, ","); // N/S
  char latD = tok ? tok[0] : 'N';
  tok = strtok(NULL, ","); // lon
  char* lon = tok;
  tok = strtok(NULL, ","); // E/W
  char lonD = tok ? tok[0] : 'E';
  tok = strtok(NULL, ","); // speed knots
  gps.speed = (tok && strlen(tok) > 0) ? atof(tok) * 1.852 : 0;
  tok = strtok(NULL, ","); // course
  tok = strtok(NULL, ","); // date
  if (tok && strlen(tok) == 6) {
    snprintf(gps.dateStr, sizeof(gps.dateStr), "20%c%c/%c%c/%c%c",
             tok[4], tok[5], tok[2], tok[3], tok[0], tok[1]);
  }
  
  if (gps.valid) {
    gps.lat = dmToDd(lat, latD);
    gps.lon = dmToDd(lon, lonD);
    gps.latDir = latD;
    gps.lonDir = lonD;
  }
}

/** Decode a VTG sentence and update the speed in km/h. */
void parseVTG(char* p) {
  char* tok = strtok(p, ",");
  tok = strtok(NULL, ","); // true track
  tok = strtok(NULL, ","); // T
  tok = strtok(NULL, ","); // mag track
  tok = strtok(NULL, ","); // M
  tok = strtok(NULL, ","); // speed knots
  tok = strtok(NULL, ","); // N
  tok = strtok(NULL, ","); // speed km/h
  if (tok && strlen(tok) > 0) {
    gps.speed = atof(tok);
  }
}

/** Validate and dispatch one complete NMEA sentence. */
void handleNMEA() {
  if (nmeaIndex < 10) return;
  nmeaLine[nmeaIndex] = '\0';
  
  if (!checkNMEA(nmeaLine)) return;
  
  if (strncmp(nmeaLine, "$GPGGA", 6) == 0 || strncmp(nmeaLine, "$GNGGA", 6) == 0) {
    parseGGA(nmeaLine);
  }
  else if (strncmp(nmeaLine, "$GPRMC", 6) == 0 || strncmp(nmeaLine, "$GNRMC", 6) == 0) {
    parseRMC(nmeaLine);
  }
  else if (strncmp(nmeaLine, "$GPVTG", 6) == 0 || strncmp(nmeaLine, "$GNVTG", 6) == 0) {
    parseVTG(nmeaLine);
  }
}

// ==================== Simple GPS UI ====================
lv_obj_t* labelStatus;
lv_obj_t* labelTime;
lv_obj_t* labelLat;
lv_obj_t* labelLon;
lv_obj_t* labelAlt;
lv_obj_t* labelSpeed;
lv_obj_t* labelSat;
lv_obj_t* labelDate;

/** Create the status bar and labels used by the GPS screen. */
void createGpsUI()
{
  // White background
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);
  
  // Status bar (top colored bar)
  lv_obj_t* statusBar = lv_obj_create(lv_screen_active());
  lv_obj_set_size(statusBar, 800, 50);
  lv_obj_align(statusBar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(statusBar, lv_color_hex(0x1B5E), 0); // Default green
  lv_obj_set_style_radius(statusBar, 0, 0);
  lv_obj_set_style_border_width(statusBar, 0, 0);
  
  // Status text
  labelStatus = lv_label_create(statusBar);
  lv_label_set_text(labelStatus, "  GPS LOCKED");
  lv_obj_set_style_text_color(labelStatus, lv_color_white(), 0);
  lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_24, 0);
  lv_obj_align(labelStatus, LV_ALIGN_LEFT_MID, 10, 0);
  
  // Time
  labelTime = lv_label_create(statusBar);
  lv_label_set_text(labelTime, "--:--:--");
  lv_obj_set_style_text_color(labelTime, lv_color_white(), 0);
  lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_16, 0);
  lv_obj_align(labelTime, LV_ALIGN_RIGHT_MID, -20, 0);
  
  // === Not positioned: large text prompt ===
  labelSat = lv_label_create(lv_screen_active());
  lv_label_set_text(labelSat, "Acquiring...");
  lv_obj_set_style_text_color(labelSat, lv_color_hex(0xC000), 0);
  lv_obj_set_style_text_font(labelSat, &lv_font_montserrat_36, 0);
  lv_obj_align(labelSat, LV_ALIGN_CENTER, 0, -60);
  lv_obj_add_flag(labelSat, LV_OBJ_FLAG_HIDDEN); // Hidden by default
  
  // === Positioned data display ===
  
  // Latitude (large font)
  labelLat = lv_label_create(lv_screen_active());
  lv_label_set_text(labelLat, "0.00000");
  lv_obj_set_style_text_color(labelLat, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelLat, &lv_font_montserrat_36, 0);
  lv_obj_align(labelLat, LV_ALIGN_TOP_LEFT, 30, 80);
  lv_obj_add_flag(labelLat, LV_OBJ_FLAG_HIDDEN);
  
  // Longitude (large font)
  labelLon = lv_label_create(lv_screen_active());
  lv_label_set_text(labelLon, "0.00000");
  lv_obj_set_style_text_color(labelLon, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelLon, &lv_font_montserrat_36, 0);
  lv_obj_align(labelLon, LV_ALIGN_TOP_LEFT, 30, 140);
  lv_obj_add_flag(labelLon, LV_OBJ_FLAG_HIDDEN);
  
  // Divider line
  lv_obj_t* line = lv_line_create(lv_screen_active());
  static lv_point_precise_t line_points[] = {{30, 200}, {400, 200}};
  lv_line_set_points(line, line_points, 2);
  lv_obj_set_style_line_color(line, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_line_width(line, 2, 0);
  
  // Altitude
  labelAlt = lv_label_create(lv_screen_active());
  lv_label_set_text(labelAlt, "ALT  0.0 m");
  lv_obj_set_style_text_color(labelAlt, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelAlt, &lv_font_montserrat_24, 0);
  lv_obj_align(labelAlt, LV_ALIGN_TOP_LEFT, 30, 220);
  lv_obj_add_flag(labelAlt, LV_OBJ_FLAG_HIDDEN);
  
  // Speed
  labelSpeed = lv_label_create(lv_screen_active());
  lv_label_set_text(labelSpeed, "SPD  0.0 km/h");
  lv_obj_set_style_text_color(labelSpeed, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelSpeed, &lv_font_montserrat_24, 0);
  lv_obj_align(labelSpeed, LV_ALIGN_TOP_LEFT, 30, 260);
  lv_obj_add_flag(labelSpeed, LV_OBJ_FLAG_HIDDEN);
  
  // Satellites label
  lv_obj_t* satLabel = lv_label_create(lv_screen_active());
  lv_label_set_text(satLabel, "SAT");
  lv_obj_set_style_text_color(satLabel, lv_color_black(), 0);
  lv_obj_set_style_text_font(satLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(satLabel, LV_ALIGN_TOP_LEFT, 30, 300);
  
  // Date
  labelDate = lv_label_create(lv_screen_active());
  lv_label_set_text(labelDate, "----/--/--");
  lv_obj_set_style_text_color(labelDate, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(labelDate, &lv_font_montserrat_16, 0);
  lv_obj_align(labelDate, LV_ALIGN_TOP_LEFT, 200, 310);
  lv_obj_add_flag(labelDate, LV_OBJ_FLAG_HIDDEN);
}

/** Update visibility, colors, and text according to the latest GPS fix. */
void updateGpsDisplay()
{
  static bool lastValid = false;
  char buf[48];
  
  // Switch display mode when status changes
  if (gps.valid != lastValid)
  {
    if (gps.valid)
    {
      // Positioned: show data, hide waiting prompt
      lv_obj_add_flag(labelSat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelLat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelLon, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelAlt, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelSpeed, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelDate, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      // Not positioned: show waiting, hide data
      lv_obj_clear_flag(labelSat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelLat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelLon, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelAlt, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelSpeed, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelDate, LV_OBJ_FLAG_HIDDEN);
    }
    lastValid = gps.valid;
  }
  
  // Update status bar color
  lv_obj_t* statusBar = lv_obj_get_parent(labelStatus);
  if (gps.valid) {
    lv_obj_set_style_bg_color(statusBar, lv_color_hex(0x1B5E), 0); // Green
    lv_label_set_text(labelStatus, "  GPS LOCKED");
  } else {
    lv_obj_set_style_bg_color(statusBar, lv_color_hex(0xC000), 0); // Red
    lv_label_set_text(labelStatus, "  NO SIGNAL");
  }
  
  // Update time
  lv_label_set_text(labelTime, gps.timeStr);
  
  if (!gps.valid)
  {
    // Not positioned: only show satellite count
    snprintf(buf, sizeof(buf), "Satellites: %d", gps.sats);
    lv_label_set_text(labelSat, buf);
    return;
  }
  
  // Positioned: update data
  snprintf(buf, sizeof(buf), "%.5f", gps.lat);
  lv_label_set_text(labelLat, buf);
  
  snprintf(buf, sizeof(buf), "%.5f", gps.lon);
  lv_label_set_text(labelLon, buf);
  
  snprintf(buf, sizeof(buf), "ALT  %.1f m", gps.alt);
  lv_label_set_text(labelAlt, buf);
  
  snprintf(buf, sizeof(buf), "SPD  %.1f km/h", gps.speed);
  lv_label_set_text(labelSpeed, buf);
  
  lv_label_set_text(labelDate, gps.dateStr);
}

// ==================== Setup & Loop ====================
PCA9557 Out;

/**
 * @brief Configure the display, touch controller, GPS UART, and LVGL UI.
 *
 * Called once after reset. The startup label remains visible while the
 * receiver searches for satellites.
 */
void setup()
{
  Serial.begin(115200);
  
  // Reset the display carrier's I/O expander before enabling peripherals.
  Wire.begin(19, 20);
  Out.reset();
  Out.setMode(IO_OUTPUT);  
  Out.setState(IO0, IO_LOW);
  Out.setState(IO1, IO_LOW);
  delay(20);
  Out.setState(IO0, IO_HIGH);
  delay(100);
  Out.setMode(IO1, IO_INPUT);

  // Keep the backlight disabled until display initialization succeeds.
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  
  // Initialize the RGB panel and allocate LVGL's full-screen frame buffers.
  if (!lcd.begin()) {
    Serial.println("lcd.begin() failed!");
    Serial.println("Check Arduino Tools > PSRAM is set to OPI PSRAM.");
    return;
  } else {
    Serial.println("LovyanGFX lcd.begin() OK");
  }
  delay(200);

  // Initialize LVGL and connect its display and input callbacks.
  lv_init();
  lv_tick_set_cb(millis);
  delay(100);

  touch_init();

  lv_color_t *frameBuffer0 = (lv_color_t *)lcd._bus_instance.getFrameBuffer(0);
  lv_color_t *frameBuffer1 = (lv_color_t *)lcd._bus_instance.getFrameBuffer(1);
  lv_display_t *display = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, my_disp_flush);
  lv_display_set_buffers(display, frameBuffer0, frameBuffer1,
                         screenWidth * screenHeight * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_FULL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  // Build the GPS status and data widgets.
  createGpsUI();
  
  // Show a short startup message while the receiver acquires a fix.
  lv_obj_t* startup = lv_label_create(lv_screen_active());
  lv_label_set_text(startup, "GPS Display\nWaiting for satellites...");
  lv_obj_set_style_text_color(startup, lv_color_black(), 0);
  lv_obj_set_style_text_font(startup, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(startup, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(startup, LV_ALIGN_CENTER, 0, 0);
  
  lv_timer_handler();
  delay(1000);
  lv_obj_delete(startup);
  
  Serial.println("GPS Display ready");
}

/**
 * @brief Read complete NMEA lines, update the GPS data model, and refresh UI.
 *
 * Called continuously. Checksum validation happens before any sentence is
 * parsed, so malformed serial data cannot overwrite the displayed values.
 */
void loop()
{
  // Assemble complete NMEA lines from the GPS UART.
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (c == '\n' || c == '\r') {
      if (nmeaIndex > 0) {
        handleNMEA();
        nmeaIndex = 0;
      }
    } else if (nmeaIndex < sizeof(nmeaLine) - 1) {
      nmeaLine[nmeaIndex++] = c;
    }
  }
  
  // Refresh labels at a human-readable rate while LVGL remains responsive.
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 800) {
    updateGpsDisplay();
    lastDraw = millis();
  }
  
  lv_timer_handler();
  delay(5);
}
