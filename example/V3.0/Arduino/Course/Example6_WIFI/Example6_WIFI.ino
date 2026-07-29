#include <WiFi.h>

// Replace these values with the network credentials used for the experiment.
const char *ssid = "elecrow888";
const char *password = "elecrow2014";

/**
 * @brief Connect to the configured Wi-Fi network and print the assigned IP.
 *
 * The function blocks until the station receives a connection, which makes
 * the successful connection condition unambiguous in the serial monitor.
 */
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("connecting");
  }
  Serial.println("WiFi is connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Leave the Wi-Fi connection active.
 *
 * Reconnection is provided by the WiFi library, so the sketch has no
 * periodic polling task.
 */
void loop() {

}
