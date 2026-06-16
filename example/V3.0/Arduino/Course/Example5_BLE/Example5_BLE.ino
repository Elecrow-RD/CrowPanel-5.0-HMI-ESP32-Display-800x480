#include "BLEDevice.h"              //BLE drive library
#include "BLEServer.h"              //BLE server library
#include "BLEUtils.h"               //BLE Utility library
#include "BLE2902.h"                //Feature Add Descriptor Library
#include <BLECharacteristic.h>      //BLE Feature function library
BLEAdvertising* pAdvertising = NULL;
BLEServer* pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic* pCharacteristic = NULL;
#define bleServerName "ESP32SPI-BLE"  //BLE Server name
#define SERVICE_UUID "6479571c-2e6d-4b34-abe9-c35116712345"  //Server UUID
#define CHARACTERISTIC_UUID "826f072d-f87c-4ae6-a416-6ffdcaa02d73"


bool connected_state = false;   //Create device connection identifier

class MyServerCallbacks: public BLEServerCallbacks  //Create connection and disconnection calling classes
{
    void onConnect(BLEServer *pServer)//Start connecting function
    {
      connected_state = true;   //Device is connected correctly
    }
    void onDisconnect(BLEServer *pServer)//disconnect function
    {
      connected_state = false;  //Device connection error
    }

};
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  //BLE
  BLEDevice::init(bleServerName);  //Create BLE and set name
  pServer = BLEDevice::createServer();  //Create BLE server
  pServer->setCallbacks(new MyServerCallbacks());  //Set up connection and disconnection calling classes
  pService = pServer->createService(SERVICE_UUID); //Create BLE service

  pCharacteristic = pService->createCharacteristic(  //Create ble feature（Characterristic_UUID）
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->setValue("ELECROW");
  //start broadcast
  pAdvertising = BLEDevice::getAdvertising();  //A bleadvertising class pointer padvertising is defined, which points to bledevice:: getadvertising()
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();  //Start broadcasting
  pService->start();
  //  pAdvertising->stop();  //Stop broadcasting
  //  pService->stop();      //Stop service
}

void loop() {

}