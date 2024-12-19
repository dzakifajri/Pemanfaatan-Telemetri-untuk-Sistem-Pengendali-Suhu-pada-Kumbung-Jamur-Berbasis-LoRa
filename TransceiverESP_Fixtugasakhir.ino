#include <SPI.h>
#include <LoRa.h>

#define PIN_LORA_CS     5
#define PIN_LORA_RST    2
#define PIN_LORA_DIO0   4
#define LORA_FREQUENCY  433E6

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

// Serial command buffer
String serialCommand = "";
bool commandComplete = false;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Receiver Starting...");
  
  // Inisialisasi pin LoRa
  LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);

  // Cek apakah LoRa berhasil diinisialisasi
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("Starting LoRa failed!");
    while (1); // Jika LoRa gagal, program berhenti di sini
  }

  // Set LoRa parameter
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(433E6);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc(); 

  Serial.println("LoRa Initialized!");
  printHelp();
}

void loop() {
  // Check for LoRa packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    processLoRaPacket();
  }

  // Check for Serial commands
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      commandComplete = true;
    } else {
      serialCommand += inChar;
    }
  }

  // Process Serial command if complete
  if (commandComplete) {
    processSerialCommand(serialCommand);
    serialCommand = "";
    commandComplete = false;
  }
}

void processLoRaPacket() {
  inString = "";
  while (LoRa.available()) {
    inString += (char)LoRa.read();
  }

  // Check if it's sensor data
  if (inString.startsWith("DATA:")) {
    if (parseData(inString.substring(5))) {
      displayData();
    } else {
      Serial.println("Error parsing sensor data.");
    }
  } else {
    Serial.println("Received unknown packet format.");
  }

  Serial.print("RSSI: ");
  Serial.println(LoRa.packetRssi());
  Serial.println("------------------------");
}

bool parseData(String data) {
  int index = 0;
  int comma = -1;
  int nextComma;
  
  // Parse temp1
  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;  // Kesalahan parsing
  sensorData.temp1 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  // Parse humidity1
  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.humidity1 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  // Parse temp2
  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.temp2 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  // Parse humidity2
  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.humidity2 = data.substring(comma + 1, nextComma).toFloat();
  comma = nextComma;

  // Parse fan_status
  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.fan_status = data.substring(comma + 1, nextComma).toInt() == 1;
  comma = nextComma;

  // Parse pump_status
  nextComma = data.indexOf(',', comma + 1);
  if (nextComma == -1) return false;
  sensorData.pump_status = data.substring(comma + 1, nextComma).toInt() == 1;
  comma = nextComma;

  // Parse autoMode
  sensorData.autoMode = data.substring(comma + 1).toInt() == 1;

  return true; // Data berhasil diurai
}

void displayData() {
  Serial.println("\nData Sensor yang Diterima:");
  
  Serial.print("Mode: ");
  Serial.println(sensorData.autoMode ? "AUTO" : "MANUAL");
  
  Serial.print("Sensor 1 - Suhu: ");
  Serial.print(sensorData.temp1);
  Serial.print("°C, Kelembapan: ");
  Serial.print(sensorData.humidity1);
  Serial.println("%");
  
  Serial.print("Sensor 2 - Suhu: ");
  Serial.print(sensorData.temp2);
  Serial.print("°C, Kelembapan: ");
  Serial.print(sensorData.humidity2);
  Serial.println("%");
  
  Serial.print("Status Kipas: ");
  Serial.println(sensorData.fan_status ? "ON" : "OFF");
  
  Serial.print("Status Pompa: ");
  Serial.println(sensorData.pump_status ? "ON" : "OFF");
}

void processSerialCommand(String command) {
  command.trim();
  command.toUpperCase();
  
  if (command == "HELP") {
    printHelp();
    return;
  }
  // Validasi command dan kirim via LoRa
  if (isValidCommand(command)) {
    sendCommand(command);
    
    // Kirimkan ACK sebagai tanda penerimaan
    LoRa.beginPacket();
    LoRa.print("ACK");
    LoRa.endPacket();
    
    Serial.println("ACK sent");
  } else {
    Serial.println("Invalid command. Type 'HELP' for available commands.");
  }
}


bool isValidCommand(String command) {
  return command == "AUTO" || 
         command == "MANUAL" || 
         command == "FAN_ON" || 
         command == "FAN_OFF" || 
         command == "PUMP_ON" || 
         command == "PUMP_OFF";
}

void sendCommand(String command) {
  LoRa.beginPacket();
  LoRa.print("CMD:");
  LoRa.print(command);
  int result = LoRa.endPacket();  // Cek hasil pengiriman
  if (result == 1) {
    Serial.println("Command sent successfully.");
  } else {
    Serial.println("Failed to send command.");
  }
}

bool waitForAck() {
  unsigned long timeout = millis() + 5000; // Waktu tunggu ACK ditambah menjadi 5 detik
  
  while (millis() < timeout) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String ackMessage = "";
      while (LoRa.available()) {
        ackMessage += (char)LoRa.read();
      }
      // Cek apakah pesan adalah ACK
      if (ackMessage == "ACK") {
        Serial.println("ACK received");
        return true; // ACK diterima
      } else {
        Serial.print("Received unknown message: ");
        Serial.println(ackMessage);
      }
    }
  }

  Serial.println("Timeout: No ACK received.");
  return false; // Tidak ada ACK dalam waktu tunggu
}


void printHelp() {
  Serial.println("\nAvailable commands:");
  Serial.println("AUTO     - Switch to automatic mode");
  Serial.println("MANUAL   - Switch to manual mode");
  Serial.println("FAN_ON   - Turn fan on (manual mode only)");
  Serial.println("FAN_OFF  - Turn fan off (manual mode only)");
  Serial.println("PUMP_ON  - Turn pump on (manual mode only)");
  Serial.println("PUMP_OFF - Turn pump off (manual mode only)");
}
