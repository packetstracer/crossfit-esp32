#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <TaskScheduler.h>

#define SERVICE_UUID              "9a8ca9ef-e43f-4157-9fee-c37a3d7dc12d"
#define BLINK_UUID                "e94f85c8-7f57-4dbd-b8d3-2b56e107ed60"
#define SPEED_UUID                "a8985fda-51aa-4f19-a777-71cf52abba1e"

// GATT services and characteristics (https://www.bluetooth.com/specifications/gatt/services/ - https://www.bluetooth.com/specifications/gatt/characteristics/)
#define DEVINFO_UUID              (uint16_t)0x180a
#define DEVINFO_MANUFACTURER_UUID (uint16_t)0x2a29
#define DEVINFO_NAME_UUID         (uint16_t)0x2a24
#define DEVINFO_SERIAL_UUID       (uint16_t)0x2a25

#define DEVICE_MANUFACTURER "Crabify corp."
#define DEVICE_NAME         "XFit"

#define PIN_BUTTON 0
#define PIN_LED LED_BUILTIN

Scheduler scheduler;

void buttonCb();
void blinkCb();
void blinkOffCb();

Task taskBlink(500, TASK_FOREVER, &blinkCb, &scheduler, false, NULL, &blinkOffCb);
Task taskButton(30, TASK_FOREVER, &buttonCb, &scheduler, true);

uint8_t blinkOn;
uint8_t blinkSpeed = 5;

BLECharacteristic *pCharBlink;
BLECharacteristic *pCharSpeed;


void setBlink(bool on, bool notify = false) {
  if (blinkOn == on) return;

  blinkOn = on;
  if (blinkOn) {
    Serial.println("Blink ON");
    taskBlink.restartDelayed(0);
  } else {
    Serial.println("Blink OFF");
    taskBlink.disable();
  }

  pCharBlink->setValue(&blinkOn, 1);
  if (notify) {
    pCharBlink->notify();
  }
}

void setBlinkSpeed(uint8_t v) {
  blinkSpeed = v;
  taskBlink.setInterval(v * 100);
  Serial.println("Blink speed updated");
}

void buttonCb() {
  uint8_t btn = digitalRead(PIN_BUTTON) != HIGH;
  if (btn) {
    setBlink(!blinkOn, true);
    taskButton.delay(1000);
  }
}

void blinkCb() {
  digitalWrite(PIN_LED, taskBlink.getRunCounter() & 1);
}

void blinkOffCb() {
  digitalWrite(PIN_LED, 0);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Connected");
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Disconnected");
    }
};

class BlinkCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length()  == 1) {
        uint8_t v = value[0];
        Serial.print("Got blink value: ");
        Serial.println(v);
        setBlink(v ? true : false);
      } else {
        Serial.println("Invalid data received");
      }
    }
};

class SpeedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() == 1) {
        uint8_t v = value[0];
        Serial.print("Got speed value: ");
        Serial.println(v);
        if (v >= 1 && v <= 10) {
          setBlinkSpeed(v);
          return;
        }
      }
      pCharSpeed->setValue(&blinkSpeed, 1);
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

  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_LED, OUTPUT);
}

BLEServer* initBLEServer(String devName) {
  BLEDevice::init(devName.c_str());
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  return pServer;
}

void createDeviceInfoService(BLEServer* pServer) {
  BLEService *pService = pServer->createService(DEVINFO_UUID);

  BLECharacteristic *pChar = pService->createCharacteristic(DEVINFO_MANUFACTURER_UUID, BLECharacteristic::PROPERTY_READ);
  pChar->setValue(DEVICE_MANUFACTURER);

  pChar = pService->createCharacteristic(DEVINFO_NAME_UUID, BLECharacteristic::PROPERTY_READ);
  pChar->setValue(DEVICE_NAME);

  pChar = pService->createCharacteristic(DEVINFO_SERIAL_UUID, BLECharacteristic::PROPERTY_READ);
  pChar->setValue(getDeviceChipId().c_str());

  pService->start();
}

void createBlinkerService(BLEServer* pServer) {
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharBlink = pService->createCharacteristic(BLINK_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);
  pCharBlink->setCallbacks(new BlinkCallbacks());
  pCharBlink->addDescriptor(new BLE2902());

  pCharSpeed = pService->createCharacteristic(SPEED_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharSpeed->setCallbacks(new SpeedCallbacks());
  pCharSpeed->setValue(&blinkSpeed, 1);

  pService->start();
}

void advertiseBlinkerService(BLEAdvertising* pAdvertising, String devName) {
  BLEAdvertisementData adv;
  adv.setName(devName.c_str());
  //adv.setCompleteServices(BLEUUID(SERVICE_UUID));
  pAdvertising->setAdvertisementData(adv);
}

void advertiseManufacturerService(BLEAdvertising* pAdvertising) {
  BLEAdvertisementData adv;
  //adv.setName(devName.c_str());
  adv.setCompleteServices(BLEUUID(SERVICE_UUID));
  pAdvertising->setScanResponseData(adv);
}

void advertiseServices(BLEServer* pServer, String devName) {
  BLEAdvertising *pAdvertising = pServer->getAdvertising();

  advertiseBlinkerService(pAdvertising, DEVICE_NAME);
  advertiseManufacturerService(pAdvertising);

  pAdvertising->start();
}

void setup() {
  configBoard();

  Serial.println("Starting XFit BLE server...");

  BLEServer *pServer = initBLEServer(DEVICE_NAME);

  createDeviceInfoService(pServer);
  createBlinkerService(pServer);
  advertiseServices(pServer, DEVICE_NAME);

  Serial.println("Ready!");

  printDeviceName();
}

void loop() {
  scheduler.execute();
}