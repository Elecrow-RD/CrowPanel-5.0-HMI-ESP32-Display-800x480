#include <WiFi.h>

const char *ssid = "elecrow888"; //Change to your SSID
const char *password = "elecrow2014"; //Change to your password
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);//Set up automatic reconnection
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("connecting");
  }
  Serial.println("WiFi is connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
//  WiFi.disconnect();
}

void loop() {

}