// GPIO 38 drives the on-board LED through the display carrier board.
#define D_PIN 38

/**
 * @brief Configure the serial port and LED output.
 *
 * Called once after reset before the repeating blink sequence starts.
 */
void setup() {
  Serial.begin(115200);
  pinMode(D_PIN, OUTPUT);
}

/**
 * @brief Alternate the LED state at a visible rate.
 *
 * Each state is held for 500 ms, so a complete on/off cycle lasts one
 * second. This makes the GPIO result easy to verify without instruments.
 */
void loop() {
  digitalWrite(D_PIN, HIGH);
  delay(500);
  digitalWrite(D_PIN, LOW);
  delay(500);
}
