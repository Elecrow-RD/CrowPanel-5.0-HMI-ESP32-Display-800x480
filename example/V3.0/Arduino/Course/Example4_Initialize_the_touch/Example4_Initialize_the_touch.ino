#include "touch.h"

/**
 * @brief Start the serial monitor and initialize the GT911 touch controller.
 *
 * Called once after reset. The touch driver performs the I2C setup and
 * controller reset; later polling is handled by loop().
 */
void setup() {
  Serial.begin(115200);
  touch_init();
}

/**
 * @brief Print each detected touch point to the serial monitor.
 *
 * The coordinates are already mapped to the 800x480 display by touch.h.
 */
void loop() {
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      Serial.print("Data x :");
      Serial.println(touch_last_x);
      Serial.print("Data y :");
      Serial.println(touch_last_y);
    }
  }
}
