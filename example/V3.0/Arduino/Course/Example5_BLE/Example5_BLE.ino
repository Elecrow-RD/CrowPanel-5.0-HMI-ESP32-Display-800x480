#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include <BLECharacteristic.h>

// These handles keep the BLE server objects alive for the lifetime of the sketch.
BLEAdvertising* pAdvertising = NULL;
BLEServer* pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic* pCharacteristic = NULL;
#define bleServerName "ESP32SPI-BLE"
#define SERVICE_UUID "6479571c-2e6d-4b34-abe9-c35116712345"
#define CHARACTERISTIC_UUID "826f072d-f87c-4ae6-a416-6ffdcaa02d73"

// Tracks whether a central device is currently connected to this server.
bool connected_state = false;

/**
 * @brief Update the connection flag when a BLE central connects or disconnects.
 */
class MyServerCallbacks: public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
      connected_state = true;
    }
    void onDisconnect(BLEServer *pServer)
    {
      connected_state = false;
    }

};
/**
 * @brief Create the BLE server, service, characteristic, and advertisement.
 *
 * Called once after reset. A phone or computer can discover the advertised
 * service and read/write the characteristic value "ELECROW".
 */
void setup() {
  Serial.begin(115200);
  BLEDevice::init(bleServerName);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->setValue("ELECROW");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  pService->start();
}

/**
 * @brief Keep the BLE server alive.
 *
 * BLE callbacks handle connection events, so no polling work is required here.
 */
void loop() {

}
