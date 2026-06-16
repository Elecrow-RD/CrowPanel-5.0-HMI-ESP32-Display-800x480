#include "touch.h"

void setup() {
  Serial.begin(115200);
  touch_init();
}

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