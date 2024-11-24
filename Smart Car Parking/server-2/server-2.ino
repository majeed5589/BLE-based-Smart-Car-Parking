/*
    Smart Car Parking System - Server for Slot 2
    - BLE Server: Hosts a BLE service to provide parking slot status for Slot 2
    - Ultrasonic Sensor: Measures distance to determine if the slot is occupied or empty
    - Based on distance, updates the BLE characteristic with appropriate status message

    Author: [Your Name]
    Date: [Current Date]
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>  // Include the BLE2902 descriptor

// Define UUIDs for the BLE Service and Characteristic (same as Slot 1)
#define SERVICE_UUID        "de732546-7e06-4e08-b0d1-9a0ecf076248"
#define CHARACTERISTIC_UUID "df17ace5-dcc8-42d9-8813-acdeb5d1e618"

// Define pins for the HC-SR04 Ultrasonic Sensor
#define TRIG_PIN 4    // Trigger Pin
#define ECHO_PIN 2    // Echo Pin

// Distance threshold in centimeters to determine if the slot is occupied
#define DISTANCE_THRESHOLD 20  // Adjust this value based on your setup

// Variables for distance measurement
long duration;
float distance;

// BLE Characteristic
BLECharacteristic *pCharacteristic;

// Current parking status
String currentStatus = "";

void setupBLE() {
  // Initialize BLE Device with a unique name for Slot 2
  BLEDevice::init("SmartParkingESP32_Slot2");

  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic with Read and Notify properties
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  // Add CCCD to the characteristic to enable notifications
  pCharacteristic->addDescriptor(new BLE2902());

  // Set initial value for the characteristic
  pCharacteristic->setValue("Initializing...");

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Functions that help with iPhone connection issues
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE Server for Slot 2 is up and running!");
}

void setupUltrasonic() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

float measureDistance() {
  // Clear the trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Trigger the sensor
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read the echo pin
  duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms

  // Calculate the distance in centimeters
  if (duration == 0) {
    // Timeout occurred
    return 0;
  }

  distance = duration / 58.2;
  return distance;
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize Ultrasonic Sensor
  setupUltrasonic();

  // Initialize BLE Server
  setupBLE();
}

void loop() {
  // Measure distance
  float currentDistance = measureDistance();

  // Determine parking slot status
  String parkingStatus;
  if (currentDistance > DISTANCE_THRESHOLD && currentDistance != 0) {
    parkingStatus = "Slot 2 Empty";
  } else {
    parkingStatus = "Slot 2 Occupied";
  }

  // Update BLE Characteristic if status has changed
  if (parkingStatus != currentStatus) {
    pCharacteristic->setValue(parkingStatus.c_str());
    pCharacteristic->notify(); // Notify connected clients
    Serial.println(parkingStatus);
    currentStatus = parkingStatus;
  }

  // Wait before next measurement
  delay(1000);
}
