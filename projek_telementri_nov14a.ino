#include "arduino_secrets.h"
#include <SPI.h>
#include <LoRa.h>
#include "thingProperties.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PIN_LORA_CS     5
#define PIN_LORA_RST    2
#define PIN_LORA_DIO0   4
#define LORA_FREQUENCY  433E6

// Inisialisasi LCD I2C pada alamat 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Struktur untuk menyimpan data sensor
struct SensorData {
  float temp1;
  float humidity1;
  float temp2;
  float humidity2;
  bool fan_status;
  bool pump_status;
  bool autoMode;
};

String inString = "";
SensorData sensorData;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Inisialisasi LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LoRa Receiver");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  Serial.println("LoRa Receiver Starting...");
  
  // Inisialisasi pin LoRa
  LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("Starting LoRa failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LoRa Failed!");
    while (1);
  }

  // Set LoRa parameter
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(433E6);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc(); 

  Serial.println("LoRa Initialized!");
  
  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  // Update LCD dengan status awal
  updateLCD();
}

void loop() {
  ArduinoCloud.update();
  
  // Check for LoRa packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    processLoRaPacket();
  }
}

void updateLCD() {
  lcd.clear();
  
  // Baris pertama: Mode
  lcd.setCursor(0, 0);
  lcd.print("Mode: ");
  lcd.print(sensorData.autoMode ? "AUTO" : "MANUAL");
  
  // Baris kedua: Status Fan dan Pump
  lcd.setCursor(0, 1);
  lcd.print("F:");
  lcd.print(sensorData.fan_status ? "ON" : "OFF");
  lcd.print(" P:");
  lcd.print(sensorData.pump_status ? "ON" : "OFF");
}

void processLoRaPacket() {
  inString = "";
  while (LoRa.available()) {
    inString += (char)LoRa.read();
  }

  // Check if it's sensor data
  if (inString.startsWith("DATA:")) {
    if (parseData(inString.substring(5))) {
      updateCloudData();
      updateLCD(); // Update LCD setelah menerima data baru
    } else {
      Serial.println("Error parsing sensor data.");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Parse Error!");
    }
  }

  Serial.print("RSSI: ");
  Serial.println(LoRa.packetRssi());
}

bool parseData(String data) {
  int index = 0;
  int comma = -1;
  int nextComma;
  
  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.temp1 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.humidity1 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.temp2 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.humidity2 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.fan_status = data.substring(comma + 1, nextComma).toInt() == 1;
  comma = nextComma;

  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.pump_status = data.substring(comma + 1, nextComma).toInt() == 1;
  comma = nextComma;

  sensorData.autoMode = data.substring(comma + 1).toInt() == 1;

  return true;
}

void updateCloudData() {
  // Update cloud variables dengan data dari LoRa
  humidity_1 = sensorData.temp1;
  suhu_1 = sensorData.humidity1;
}

void sendCommand(String command) {
  LoRa.beginPacket();
  LoRa.print("CMD:");
  LoRa.print(command);
  int result = LoRa.endPacket();
  if (result == 1) {
    Serial.println("Command sent successfully.");
  } else {
    Serial.println("Failed to send command.");
  }
}

// Callback functions for Arduino Cloud
void onManualButtonChange() {
  if (manualButton == 1) {
    sendCommand("MANUAL");
    sensorData.autoMode = false;
  } else {
    sendCommand("AUTO");
    sensorData.autoMode = true;
  }
  updateLCD(); // Update LCD setelah perubahan mode
}

void onOnOffFanChange() {
  if (onOff_Fan == 1) {
    sendCommand("FAN_ON");
    sensorData.fan_status = true;
  } else {
    sendCommand("FAN_OFF");
    sensorData.fan_status = false;
  }
  updateLCD(); // Update LCD setelah perubahan status fan
}

void onOnOffPumpChange() {
  if (onOff_Pump == 1) {
    sendCommand("PUMP_ON");
    sensorData.pump_status = true;
  } else {
    sendCommand("PUMP_OFF");
    sensorData.pump_status = false;
  }
  updateLCD(); // Update LCD setelah perubahan status pump
}