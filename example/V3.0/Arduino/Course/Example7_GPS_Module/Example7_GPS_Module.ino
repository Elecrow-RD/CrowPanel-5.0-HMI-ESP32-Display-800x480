#include <PCA9557.h>
#include <lvgl.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#define TFT_BL 2

// ==================== GPS Serial Config ====================
#define GPS_RX 44
#define GPS_TX 43
HardwareSerial gpsSerial(1);

// ==================== GPS Parsing Variables ====================
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

// ==================== Screen Config ====================
class LGFX : public lgfx::LGFX_Device
{
public:
  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;

  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      
      cfg.pin_d0  = GPIO_NUM_8;
      cfg.pin_d1  = GPIO_NUM_3;
      cfg.pin_d2  = GPIO_NUM_46;
      cfg.pin_d3  = GPIO_NUM_9;
      cfg.pin_d4  = GPIO_NUM_1;
      cfg.pin_d5  = GPIO_NUM_5;
      cfg.pin_d6  = GPIO_NUM_6;
      cfg.pin_d7  = GPIO_NUM_7;
      cfg.pin_d8  = GPIO_NUM_15;
      cfg.pin_d9  = GPIO_NUM_16;
      cfg.pin_d10 = GPIO_NUM_4;
      cfg.pin_d11 = GPIO_NUM_45;
      cfg.pin_d12 = GPIO_NUM_48;
      cfg.pin_d13 = GPIO_NUM_47;
      cfg.pin_d14 = GPIO_NUM_21;
      cfg.pin_d15 = GPIO_NUM_14;

      cfg.pin_henable = GPIO_NUM_40;
      cfg.pin_vsync   = GPIO_NUM_41;
      cfg.pin_hsync   = GPIO_NUM_39;
      cfg.pin_pclk    = GPIO_NUM_0;
      cfg.freq_write  = 9000000;

      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch  = 43;
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch  = 12;

      cfg.pclk_active_neg   = 1;
      cfg.de_idle_high      = 0;
      cfg.pclk_idle_high    = 0;

      _bus_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 800;
      cfg.memory_height = 480;
      cfg.panel_width   = 800;
      cfg.panel_height  = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
  }
};

LGFX lcd;

// ==================== LVGL Related ====================
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[800 * 480 / 10];
static lv_disp_drv_t disp_drv;

#include "touch.h"

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
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

float dmToDd(const char* dm, char dir) {
  if (!dm || strlen(dm) < 3) return 0;
  float val = atof(dm);
  int deg = (int)(val / 100);
  float min = val - deg * 100;
  float dd = deg + min / 60.0;
  return (dir == 'S' || dir == 'W') ? -dd : dd;
}

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

void createGpsUI()
{
  // White background
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), LV_PART_MAIN);
  
  // Status bar (top colored bar)
  lv_obj_t* statusBar = lv_obj_create(lv_scr_act());
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
  labelSat = lv_label_create(lv_scr_act());
  lv_label_set_text(labelSat, "Acquiring...");
  lv_obj_set_style_text_color(labelSat, lv_color_hex(0xC000), 0);
  lv_obj_set_style_text_font(labelSat, &lv_font_montserrat_36, 0);
  lv_obj_align(labelSat, LV_ALIGN_CENTER, 0, -60);
  lv_obj_add_flag(labelSat, LV_OBJ_FLAG_HIDDEN); // Hidden by default
  
  // === Positioned data display ===
  
  // Latitude (large font)
  labelLat = lv_label_create(lv_scr_act());
  lv_label_set_text(labelLat, "0.00000");
  lv_obj_set_style_text_color(labelLat, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelLat, &lv_font_montserrat_36, 0);
  lv_obj_align(labelLat, LV_ALIGN_TOP_LEFT, 30, 80);
  lv_obj_add_flag(labelLat, LV_OBJ_FLAG_HIDDEN);
  
  // Longitude (large font)
  labelLon = lv_label_create(lv_scr_act());
  lv_label_set_text(labelLon, "0.00000");
  lv_obj_set_style_text_color(labelLon, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelLon, &lv_font_montserrat_36, 0);
  lv_obj_align(labelLon, LV_ALIGN_TOP_LEFT, 30, 140);
  lv_obj_add_flag(labelLon, LV_OBJ_FLAG_HIDDEN);
  
  // Divider line
  lv_obj_t* line = lv_line_create(lv_scr_act());
  static lv_point_t line_points[] = {{30, 200}, {400, 200}};
  lv_line_set_points(line, line_points, 2);
  lv_obj_set_style_line_color(line, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_line_width(line, 2, 0);
  
  // Altitude
  labelAlt = lv_label_create(lv_scr_act());
  lv_label_set_text(labelAlt, "ALT  0.0 m");
  lv_obj_set_style_text_color(labelAlt, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelAlt, &lv_font_montserrat_24, 0);
  lv_obj_align(labelAlt, LV_ALIGN_TOP_LEFT, 30, 220);
  lv_obj_add_flag(labelAlt, LV_OBJ_FLAG_HIDDEN);
  
  // Speed
  labelSpeed = lv_label_create(lv_scr_act());
  lv_label_set_text(labelSpeed, "SPD  0.0 km/h");
  lv_obj_set_style_text_color(labelSpeed, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelSpeed, &lv_font_montserrat_24, 0);
  lv_obj_align(labelSpeed, LV_ALIGN_TOP_LEFT, 30, 260);
  lv_obj_add_flag(labelSpeed, LV_OBJ_FLAG_HIDDEN);
  
  // Satellites label
  lv_obj_t* satLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(satLabel, "SAT");
  lv_obj_set_style_text_color(satLabel, lv_color_black(), 0);
  lv_obj_set_style_text_font(satLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(satLabel, LV_ALIGN_TOP_LEFT, 30, 300);
  
  // Date
  labelDate = lv_label_create(lv_scr_act());
  lv_label_set_text(labelDate, "----/--/--");
  lv_obj_set_style_text_color(labelDate, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(labelDate, &lv_font_montserrat_16, 0);
  lv_obj_align(labelDate, LV_ALIGN_TOP_LEFT, 200, 310);
  lv_obj_add_flag(labelDate, LV_OBJ_FLAG_HIDDEN);
}

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

void setup()
{
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  
  // IO expander init
  Wire.begin(19, 20);
  Out.reset();
  Out.setMode(IO_OUTPUT);  
  Out.setState(IO0, IO_LOW);
  Out.setState(IO1, IO_LOW);
  delay(20);
  Out.setState(IO0, IO_HIGH);
  delay(100);
  Out.setMode(IO1, IO_INPUT);

  // Backlight
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  
  // Display init
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  delay(200);

  // LVGL init
  lv_init();
  delay(100);
  touch_init();

  screenWidth = lcd.width();
  screenHeight = lcd.height();

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 10);
  
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

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  // Create simple GPS UI
  createGpsUI();
  
  // Startup screen
  lv_obj_t* startup = lv_label_create(lv_scr_act());
  lv_label_set_text(startup, "GPS Display\nWaiting for satellites...");
  lv_obj_set_style_text_color(startup, lv_color_black(), 0);
  lv_obj_set_style_text_font(startup, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(startup, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(startup, LV_ALIGN_CENTER, 0, 0);
  
  lv_timer_handler();
  delay(1000);
  lv_obj_del(startup); // Delete startup screen
  
  Serial.println("GPS Display ready");
}

void loop()
{
  // Read GPS data
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
  
  // Refresh screen every 800ms
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 800) {
    updateGpsDisplay();
    lastDraw = millis();
  }
  
  lv_timer_handler();
  delay(5);
}