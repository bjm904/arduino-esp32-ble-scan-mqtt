#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

const int ledPin = 2;
const char* ssid = "TSNW";
const char* password = "a1b2c3d4e5";
const char* mqttServer = "10.254.12.26";
const int mqttPort = 1883;
const int maxDevices = 10;
const int bluetoothScanDuration = 5;
String mqttRegTopic = "kt/registration/";
String mqttTopic = "kt/presence/bt/";

WiFiClient wifiClient;

PubSubClient client(wifiClient);

BLEScan* pBLEScan;

const int capacity = JSON_ARRAY_SIZE(maxDevices) + (maxDevices * JSON_OBJECT_SIZE(3));
StaticJsonDocument<capacity> btDevices;
char jsonOutput[128];

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void reconnect() {
  const String bluetoothMac = BLEDevice::getAddress().toString().c_str();
  String clientId = "KounterTop-";
  clientId += bluetoothMac;
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println(clientId);
    Serial.print("Connecting MQTT to: ");
    Serial.println(mqttServer);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT connected");
      client.publish(mqttRegTopic.c_str(), "hello world");
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_bluetooth() {
  Serial.println("Setting up Bluetooth");

  BLEDevice::init("KounterTop");

  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  const String bluetoothMac = BLEDevice::getAddress().toString().c_str();
  // Create full MQTT topic
  mqttTopic += bluetoothMac;

  Serial.print("Bluetooth ready. MAC: ");
  Serial.println(bluetoothMac);
}

void setup_wifi() {
  Serial.print("Connecting WiFi to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.print("WiFi connected IP: ");
  Serial.println(WiFi.localIP());
}

void setup_mqtt() {
  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);
  reconnect();
}

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  delay(10);

  // Set up devices JSON object
  int i = 0;
  while(i < maxDevices) {
    btDevices.createNestedObject();
    i++;
  }
  
  setup_bluetooth();
  setup_wifi();
  setup_mqtt();

  Serial.print("MQTT Topic: ");
  Serial.println(mqttTopic.c_str());
}

void loop() {
  // Make sure MQTT is connected
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Scan for BLE devices
  digitalWrite(ledPin, HIGH);
  BLEScanResults foundDevices = pBLEScan->start(bluetoothScanDuration, false);
  digitalWrite(ledPin, LOW);
  Serial.println("Scan complete. Devices:");

  // Go over found devices and generate JSON payload
  int i = 0;
  while(i < foundDevices.getCount()) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    String deviceAddress = device.getAddress().toString().c_str();
    String deviceName = device.getName().c_str();
    const int deviceRssi = device.getRSSI();

    Serial.print("Device #");
    Serial.print(i);
    Serial.print(" ");
    Serial.print(deviceAddress);
    Serial.print(": ");
    Serial.println(deviceRssi);
  
    JsonObject btDevice = btDevices[i];
    btDevice["name"] = deviceName;
    btDevice["rssi"] = deviceRssi;
    
    serializeJson(btDevice, jsonOutput);
    client.publish((mqttTopic + "/" + deviceAddress).c_str(), jsonOutput);

    i++;
  }

  // Clear scan results to free buffer
  pBLEScan->clearResults();
  delay(1000);
}
