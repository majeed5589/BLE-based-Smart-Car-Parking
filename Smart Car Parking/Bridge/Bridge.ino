/*
    ESP32 Client acting as BLE Client and Server
    - Connects to two BLE servers (Slot 1 and Slot 2) to receive parking status updates.
    - Acts as a BLE server to share parking statuses with a mobile app via BLE characteristics.
    - Mobile app connects to the ESP32 client to read real-time parking slot statuses.

    Author: [Your Name]
    Date: [Current Date]
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLE2902.h>

// Define UUIDs for the BLE Service and Characteristic (Parking Slot Servers)
#define SERVICE_UUID_SERVER    "de732546-7e06-4e08-b0d1-9a0ecf076248"
#define CHARACTERISTIC_UUID_SERVER "df17ace5-dcc8-42d9-8813-acdeb5d1e618"

// Define UUIDs for the BLE Service and Characteristics (Mobile App)
#define SERVICE_UUID_APP         "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID_SLOT1 "abcd1234-5678-9101-1121-314151617181"
#define CHARACTERISTIC_UUID_SLOT2 "abcd1234-5678-9101-1121-314151617182"

#define SCAN_TIME 5 // Duration of the scan in seconds

// Struct to hold information about each server
struct ServerInfo {
  String deviceName;
  String addressString; // Store address as a String
  BLEClient* pClient;
  BLERemoteCharacteristic* pRemoteCharacteristic;
  bool doConnect;
  bool connected;

  // Current status
  String currentStatus;

  // Default constructor
  ServerInfo()
    : deviceName(""), addressString(""), pClient(nullptr), pRemoteCharacteristic(nullptr),
      doConnect(false), connected(false), currentStatus("") {}
};

// Array to hold information for each server
#define NUM_SERVERS 2
ServerInfo servers[NUM_SERVERS];

// BLE Server variables (for mobile app)
BLEServer* pBLEServer = nullptr;
BLEService* pAppService = nullptr;
BLECharacteristic* pCharacteristicSlot1 = nullptr;
BLECharacteristic* pCharacteristicSlot2 = nullptr;

// Forward declarations
bool connectToServer(ServerInfo* serverInfo);

// Notification callback for Slot 1
void notifyCallbackSlot1(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  String value = "";
  for (size_t i = 0; i < length; i++) {
      value += (char)pData[i];
  }

  Serial.print("Slot 1: ");
  Serial.println(value);

  // Update the BLE characteristic for the mobile app
  pCharacteristicSlot1->setValue(value.c_str());
  pCharacteristicSlot1->notify();
}

// Notification callback for Slot 2
void notifyCallbackSlot2(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  String value = "";
  for (size_t i = 0; i < length; i++) {
      value += (char)pData[i];
  }

  Serial.print("Slot 2: ");
  Serial.println(value);

  // Update the BLE characteristic for the mobile app
  pCharacteristicSlot2->setValue(value.c_str());
  pCharacteristicSlot2->notify();
}

// Callback class for scan results
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Check if the advertised device has the desired service UUID
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID_SERVER))) {
            String devName = advertisedDevice.getName().c_str();
            // Check if device is Slot 1 or Slot 2
            for (int i = 0; i < NUM_SERVERS; i++) {
                if (servers[i].deviceName == devName && servers[i].addressString == "") {
                    servers[i].addressString = advertisedDevice.getAddress().toString().c_str();
                    servers[i].doConnect = true;
                    Serial.print("Found ");
                    Serial.println(devName);
                    break;
                }
            }
        }
    }
};

// Client connection callback
class MyClientCallback : public BLEClientCallbacks {
    ServerInfo* serverInfo;
  public:
    MyClientCallback(ServerInfo* info) {
      serverInfo = info;
    }
    void onConnect(BLEClient* pClient) {
        Serial.print("Connected to ");
        Serial.println(serverInfo->deviceName);
    }

    void onDisconnect(BLEClient* pClient) {
        Serial.print("Disconnected from ");
        Serial.println(serverInfo->deviceName);
        serverInfo->connected = false;
        serverInfo->doConnect = true; // Reconnect when disconnected
    }
};

// Function to set up BLE server for mobile app
void setupBLEServer() {
    // Create the BLE Device with a name
    BLEDevice::init("SmartParking_Client");

    // Create the BLE Server
    pBLEServer = BLEDevice::createServer();

    // Create the BLE Service
    pAppService = pBLEServer->createService(SERVICE_UUID_APP);

    // Create BLE Characteristics for each slot
    pCharacteristicSlot1 = pAppService->createCharacteristic(
                            CHARACTERISTIC_UUID_SLOT1,
                            BLECharacteristic::PROPERTY_READ   |
                            BLECharacteristic::PROPERTY_NOTIFY
                          );
    pCharacteristicSlot2 = pAppService->createCharacteristic(
                            CHARACTERISTIC_UUID_SLOT2,
                            BLECharacteristic::PROPERTY_READ   |
                            BLECharacteristic::PROPERTY_NOTIFY
                          );

    // Add descriptors to enable notification
    pCharacteristicSlot1->addDescriptor(new BLE2902());
    pCharacteristicSlot2->addDescriptor(new BLE2902());

    // Set initial values
    pCharacteristicSlot1->setValue("Initializing...");
    pCharacteristicSlot2->setValue("Initializing...");

    // Start the service
    pAppService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_APP);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // Functions that help with iPhone issues
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE Server for Mobile App is up and running!");
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Client and Server...");

    // Initialize server info
    servers[0].deviceName = "SmartParkingESP32_Slot1";
    servers[0].doConnect = false;
    servers[0].connected = false;

    servers[1].deviceName = "SmartParkingESP32_Slot2";
    servers[1].doConnect = false;
    servers[1].connected = false;

    // Set up BLE server for mobile app
    setupBLEServer();

    // Initialize BLE client functionality
    // Note: We don't call BLEDevice::init() again since it's already called in setupBLEServer()

    // Create a scan object
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); // Active scan to get results faster
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);  // Must be <= setInterval value

    // Start scanning
    pBLEScan->start(SCAN_TIME, false);
}

void loop() {
    // Connect to servers if needed
    for (int i = 0; i < NUM_SERVERS; i++) {
        if (servers[i].doConnect) {
            if (connectToServer(&servers[i])) {
                Serial.print("Connected to ");
                Serial.println(servers[i].deviceName);
            } else {
                Serial.print("Failed to connect to ");
                Serial.println(servers[i].deviceName);
            }
            servers[i].doConnect = false;
        }
    }

    // If not connected to all servers, keep scanning
    bool allConnected = true;
    for (int i = 0; i < NUM_SERVERS; i++) {
        if (!servers[i].connected) {
            allConnected = false;
            break;
        }
    }

    if (!allConnected) {
        // Start scanning again
        BLEDevice::getScan()->start(SCAN_TIME, false);
    }

    delay(1000);
}

bool connectToServer(ServerInfo* serverInfo) {
    Serial.print("Connecting to ");
    Serial.println(serverInfo->deviceName);

    serverInfo->pClient = BLEDevice::createClient();
    serverInfo->pClient->setClientCallbacks(new MyClientCallback(serverInfo));

    // Create BLEAddress from addressString
    BLEAddress bleAddress(serverInfo->addressString.c_str());
    if (!serverInfo->pClient->connect(bleAddress)) {
        Serial.print("Failed to connect to ");
        Serial.println(serverInfo->deviceName);
        return false;
    }

    BLERemoteService* pRemoteService = serverInfo->pClient->getService(SERVICE_UUID_SERVER);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find service UUID on ");
        Serial.println(serverInfo->deviceName);
        serverInfo->pClient->disconnect();
        return false;
    }

    serverInfo->pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_SERVER);
    if (serverInfo->pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find characteristic UUID on ");
        Serial.println(serverInfo->deviceName);
        serverInfo->pClient->disconnect();
        return false;
    }

    // Register for notifications
    if (serverInfo->pRemoteCharacteristic->canNotify()) {
        if (serverInfo->deviceName == "SmartParkingESP32_Slot1") {
            serverInfo->pRemoteCharacteristic->registerForNotify(notifyCallbackSlot1);
        } else if (serverInfo->deviceName == "SmartParkingESP32_Slot2") {
            serverInfo->pRemoteCharacteristic->registerForNotify(notifyCallbackSlot2);
        }
        Serial.print("Registered for notifications from ");
        Serial.println(serverInfo->deviceName);
    } else {
        Serial.print("Characteristic does not support notifications on ");
        Serial.println(serverInfo->deviceName);
    }

    serverInfo->connected = true;
    return true;
}
