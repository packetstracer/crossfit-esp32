#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <TaskScheduler.h>


// GATT services and characteristics (https://www.bluetooth.com/specifications/gatt/services/ - https://www.bluetooth.com/specifications/gatt/characteristics/)
#define SERVICE_DEVINFO_UUID      (uint16_t)0x180a
#define DEVINFO_MANUFACTURER_UUID (uint16_t)0x2a29
#define DEVINFO_NAME_UUID         (uint16_t)0x2a24
#define DEVINFO_SERIAL_UUID       (uint16_t)0x2a25

// See the following for generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_BLINKER_UUID  "9a8ca9ef-e43f-4157-9fee-c37a3d7dc12d"
#define BLINKER_BLINK_UUID    "e94f85c8-7f57-4dbd-b8d3-2b56e107ed60"
#define BLINKER_SPEED_UUID    "a8985fda-51aa-4f19-a777-71cf52abba1e"

#define SERVICE_SNIFFER_UUID    "ef66ccad-f4a8-4c51-b025-bc2a2c9fdaa6"
#define SNIFFER_STATUS_UUID     "5b7dd565-6e90-4df8-b806-f4ac85df74ca"
#define SNIFFER_SPEED_UUID      "c8fb3a51-d44c-4d9f-a8ec-a7598c1bf2ea"
#define SNIFFER_VOLTAGE_UUID    "d8b2c95b-b317-47ca-ac02-ccfd3d85a666"
#define SNIFFER_TIMESTAMP_UUID  "7127a1b2-ed4d-433a-9780-5a9e38f6a040"

#define SERVICE_PROXY_UUID  "b20cc0be-c805-4f63-92e3-c0352c5d6eba"
#define PROXY_VOLTAGE_UUID  "fcdb3ab4-5c25-4817-acfd-e989e73baf3d"

#define SERVICE_CALIBRATE_UUID        "ba5421cf-aeef-4d55-bb66-146b9222c684"
#define CALIBRATE_VOLTAGE_LEFT_UUID   "c1efc841-b656-4926-a866-ce05626ed7f8"
#define CALIBRATE_VOLTAGE_RIGHT_UUID  "b296065a-729b-4ac6-b684-b9b75eaf696e"

#define DEVICE_MANUFACTURER "Crabify corp."
#define DEVICE_NAME         "XFit"

#define PIN_BLINKER_BUTTON 0
#define PIN_BLINKER_LED LED_BUILTIN

// #define PIN_SNIFFER_IN   1
// #define PIN_SNIFFER_OUT  2


Scheduler scheduler;

void blinkerButtonCb();
void blinkerCb();
void blinkerOffCb();

void snifferCb();
//bool snifferOnCb();
void snifferOffCb();

Task taskBlinker(500, TASK_FOREVER, &blinkerCb, &scheduler, false, NULL, &blinkerOffCb);
Task taskBlinkerButton(30, TASK_FOREVER, &blinkerButtonCb, &scheduler, true);

Task taskSniffer(0, TASK_FOREVER, &snifferCb, &scheduler, false, NULL, NULL);


uint8_t blinkerOn;
uint8_t blinkerSpeed = 5;

uint8_t snifferOn;
uint8_t snifferSpeed = 1;


BLECharacteristic *pCharBlinkerBlink;
BLECharacteristic *pCharBlinkerSpeed;

BLECharacteristic *pCharSnifferStatus;
BLECharacteristic *pCharSnifferSpeed;
BLECharacteristic *pCharSnifferVoltage;
BLECharacteristic *pCharSnifferTimestamp;


void setBlinker(bool on, bool notify = false) {
  if (blinkerOn == on) return;

  blinkerOn = on;
  if (blinkerOn) {
    Serial.println("Blink ON");
    taskBlinker.restartDelayed(0);
  } else {
    Serial.println("Blink OFF");
    taskBlinker.disable();
  }

  pCharBlinkerBlink->setValue(&blinkerOn, 1);
  if (notify) {
    pCharBlinkerBlink->notify();
  }
}

void setBlinkerSpeed(uint8_t v) {
  blinkerSpeed = v;
  taskBlinker.setInterval(v * 100);
  Serial.println("Blink speed updated");
}

void blinkerButtonCb() {
  uint8_t btn = digitalRead(PIN_BLINKER_BUTTON) != HIGH;
  if (btn) {
    setBlinker(!blinkerOn, true);
    taskBlinkerButton.delay(1000);
  }
}

void blinkerCb() {
  digitalWrite(PIN_BLINKER_LED, taskBlinker.getRunCounter() & 1);
}

void blinkerOffCb() {
  digitalWrite(PIN_BLINKER_LED, 0);
}

void setSniffer(bool on, bool notify = false) {
  if (snifferOn == on) return;

  snifferOn = on;
  if (snifferOn) {
    Serial.println("Sniffer ON");
    taskSniffer.restartDelayed(0);
  } else {
    Serial.println("Sniffer OFF");
    taskSniffer.disable();
  }

  pCharSnifferStatus->setValue(&snifferOn, 1);
  if (notify) {
    pCharSnifferStatus->notify();
  }
}

void setSnifferSpeed(uint8_t v) {
  snifferSpeed = v;
  taskSniffer.setInterval(v);
  Serial.println("Sniffer speed updated");
}

void snifferCb() {
  setSniffer(!snifferOn, true);
}

void snifferOffCb() {
  setSniffer(!snifferOn, true);
}

class XfitServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Connected");
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Disconnected");
    }
};

class BlinkerBlinkCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length()  == 1) {
        uint8_t v = value[0];
        Serial.print("Got blinker blink value: ");
        Serial.println(v);
        setBlinker(v ? true : false);
      } else {
        Serial.println("Invalid data received");
      }
    }
};

class BlinkerSpeedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() == 1) {
        uint8_t v = value[0];
        Serial.print("Got blinker speed value: ");
        Serial.println(v);
        if (v >= 1 && v <= 10) {
          setBlinkerSpeed(v);
          return;
        }
      }
      pCharBlinkerSpeed->setValue(&blinkerSpeed, 1);
      Serial.println("Invalid data received");
    }
};

class SnifferStatusCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length()  == 1) {
        uint8_t v = value[0];
        Serial.print("Got sniffer value: ");
        Serial.println(v);
        setSniffer(v ? true : false);
      } else {
        Serial.println("Invalid data received");
      }
    }
};

class SnifferSpeedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() == 1) {
        uint8_t v = value[0];
        Serial.print("Got sniffer speed value: ");
        Serial.println(v);
        if (v >= 1 && v <= 10) {
          setSnifferSpeed(v);
          return;
        }
      }
      pCharSnifferSpeed->setValue(&snifferSpeed, 1);
      Serial.println("Invalid data received");
    }
};

String getDeviceChipId() {
  return String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
}

String getDeviceFullName(String deviceName = DEVICE_NAME) {
  return deviceName + '_' + getDeviceChipId();
}

void printDeviceName() {
  Serial.println("Device name: " + getDeviceFullName());
}

void configBoard() {
  Serial.begin(115200);

  pinMode(PIN_BLINKER_BUTTON, INPUT);
  pinMode(PIN_BLINKER_LED, OUTPUT);
}

BLEServer* initBLEServer(String devName) {
  BLEDevice::init(devName.c_str());
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new XfitServerCallbacks());

  return pServer;
}

void createDeviceInfoService(BLEServer* pServer) {
  BLEService *pService = pServer->createService(SERVICE_DEVINFO_UUID);

  BLECharacteristic *pChar = pService->createCharacteristic(DEVINFO_MANUFACTURER_UUID, BLECharacteristic::PROPERTY_READ);
  pChar->setValue(DEVICE_MANUFACTURER);

  pChar = pService->createCharacteristic(DEVINFO_NAME_UUID, BLECharacteristic::PROPERTY_READ);
  pChar->setValue(DEVICE_NAME);

  pChar = pService->createCharacteristic(DEVINFO_SERIAL_UUID, BLECharacteristic::PROPERTY_READ);
  pChar->setValue(getDeviceChipId().c_str());

  pService->start();
}

void createBlinkerService(BLEServer* pServer) {
  BLEService *pService = pServer->createService(SERVICE_BLINKER_UUID);

  pCharBlinkerBlink = pService->createCharacteristic(BLINKER_BLINK_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);
  pCharBlinkerBlink->setCallbacks(new BlinkerBlinkCallbacks());
  pCharBlinkerBlink->addDescriptor(new BLE2902());

  pCharBlinkerSpeed = pService->createCharacteristic(BLINKER_SPEED_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharBlinkerSpeed->setCallbacks(new BlinkerSpeedCallbacks());
  pCharBlinkerSpeed->setValue(&blinkerSpeed, 1);

  pService->start();
}

void createSnifferService(BLEServer* pServer) {
  BLEService *pService = pServer->createService(SERVICE_SNIFFER_UUID);

  pCharSnifferStatus = pService->createCharacteristic(SNIFFER_VOLTAGE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);
  pCharSnifferStatus->setCallbacks(new SnifferStatusCallbacks());

  pCharSnifferSpeed = pService->createCharacteristic(SNIFFER_TIMESTAMP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);
  pCharSnifferSpeed->setCallbacks(new SnifferSpeedCallbacks());
  pCharBlinkerSpeed->setValue(&snifferSpeed, 1);

  pCharSnifferVoltage = pService->createCharacteristic(SNIFFER_VOLTAGE_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  pCharSnifferTimestamp = pService->createCharacteristic(SNIFFER_TIMESTAMP_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  pService->start();
}

void advertiseManufacturerService(BLEAdvertising* pAdvertising, String devName) {
  BLEAdvertisementData adv;
  adv.setName(devName.c_str());
  pAdvertising->setAdvertisementData(adv);
}

void advertiseBlinkerService(BLEAdvertising* pAdvertising) {
  BLEAdvertisementData adv;
  adv.setCompleteServices(BLEUUID(SERVICE_BLINKER_UUID));
  pAdvertising->setScanResponseData(adv);
}

void advertiseSnifferService(BLEAdvertising* pAdvertising) {
  BLEAdvertisementData adv;
  adv.setName("Sniffer");
  pAdvertising->setAdvertisementData(adv);
}

void advertiseServices(BLEServer* pServer, String devName) {
  BLEAdvertising *pAdvertising = pServer->getAdvertising();

  advertiseManufacturerService(pAdvertising, DEVICE_NAME);
  advertiseBlinkerService(pAdvertising);
  advertiseSnifferService(pAdvertising);

  pAdvertising->start();
}

void setup() {
  configBoard();

  Serial.println("Starting XFit BLE server...");

  BLEServer *pServer = initBLEServer(DEVICE_NAME);

  createDeviceInfoService(pServer);
  createBlinkerService(pServer);
  createSnifferService(pServer);
  advertiseServices(pServer, DEVICE_NAME);

  Serial.println("Ready!");

  printDeviceName();
}

void loop() {
  scheduler.execute();
}