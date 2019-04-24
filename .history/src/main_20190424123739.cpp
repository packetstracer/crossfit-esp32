/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE"
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second.
*/
#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "1b68cacd-2818-4a71-a49b-0061181c97a2"
#define CHARACTERISTIC_UUID_RX "582adc94-2eab-4735-b821-293883e26762"
#define CHARACTERISTIC_UUID_TX "0da69903-6d69-4810-bccc-4c56d2880188"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      /*
       * my iOS app sends a 5 byte payload
       * so thisis how it is listened for
       */

      if (rxValue.length() == 5) {

        byte data0 = (uint8_t)rxValue[0];
        byte data1 = (uint8_t)rxValue[1];
        byte data2 = (uint8_t)rxValue[2];
        byte data3 = (uint8_t)rxValue[3];
        byte data4 = (uint8_t)rxValue[4];

        Serial.print(data0);
        Serial.print(", ");
        Serial.print(data1);
        Serial.print(", ");
        Serial.print(data2);
        Serial.print(", ");
        Serial.print(data3);
        Serial.print(", ");
        Serial.println(data4);

      }

    }

};


void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  //BLEDevice::init("UART Service");
  BLEDevice::init("NIX4");
  /*
   * this appears to need to be 5 characters or less
   * when advertising the service UUID
   */


  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                         | BLECharacteristic::PROPERTY_WRITE_NR
                                         /*
                                          * the redbear library explicitly does not listen for a write response
                                          * so this needs to be here
                                          *
                                          */
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->addServiceUUID(pService->getUUID());  // needed for RB compat.
  /*
   * need to advertise the service UUID for the redbear code to work
   * as it looks for it
   */
  pServer->getAdvertising()->start();

  Serial.println("Waiting a client connection to notify...");
}

void loop() {

  if (deviceConnected) {
    Serial.printf("*** Sent Value: %d ***\n", txValue);
    pCharacteristic->setValue(&txValue, 1);
    pCharacteristic->notify();
    txValue++;
  }
  // delay(1000);
}