#include <DHT.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <RTClib.h>

// Pin definitions for DHT sensors
#define DHTPIN1 3       // Pin untuk DHT11 pertama
#define DHTPIN2 4       // Pin untuk DHT11 kedua
#define DHTTYPE DHT11   // Tipe sensor DHT11

// Inisialisasi sensor DHT
DHT dht_sensor1(DHTPIN1, DHTTYPE);
DHT dht_sensor2(DHTPIN2, DHTTYPE);

// Inisialisasi RTC
RTC_DS3231 rtc;

// Pin definitions for relays
#define RELAY1_PIN 6    // Pin untuk relay kipas (untuk suhu)
#define RELAY2_PIN 7    // Pin untuk relay pompa (untuk kelembapan)

// LoRa pins
#define LORA_SS_PIN     10
#define LORA_RESET_PIN  9
#define LORA_DIO0_PIN   8
#define LORA_FREQUENCY  433E6

// Threshold values
const int temp_threshold_max = 35;
const int temp_threshold_min = 20;
const int humidity_threshold_max = 95;
const int humidity_threshold_min = 70;

// Mode operasi
bool autoMode = true;  // true = otomatis, false = manual
bool manualFanStatus = false;
bool manualPumpStatus = false;

// Timer untuk kontrol otomatis berdasarkan waktu
unsigned long timerStart = 0;
const unsigned long TIMER_DURATION = 10000; // 10 detik dalam milidetik
bool timerActive = false;

// Struktur untuk data sensor
struct SensorData {
  float temp1;
  float humidity1;
  float temp2;
  float humidity2;
  bool fan_status;
  bool pump_status;
};

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 2000;

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  // Inisialisasi RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // Inisialisasi DHT sensors
  dht_sensor1.begin();
  dht_sensor2.begin();
  
  // Setup relay pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);  // Relay aktif LOW
  digitalWrite(RELAY2_PIN, HIGH);  // Relay aktif LOW

  // Inisialisasi LoRa
  Serial.println("LoRa Sender Starting...");
  LoRa.setPins(LORA_SS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(433E6);
  LoRa.setCodingRate4(5);
  Serial.println("LoRa Initialized!");

  // Set mode AUTO sebagai default
  autoMode = true;
  Serial.println("Default mode: AUTO");
}

void loop() {
  // Serial.println("Checking for commands...");
  checkForCommands();
  // delay(1000);
  // Kirim data sensor secara periodik
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= sendInterval) {
    Serial.println("Sending sensor data...");
    sendSensorData();
    lastSendTime = currentTime;
  }

  // Check RTC time dan timer jika dalam mode auto
  if (autoMode) {
    checkRTCSchedule();
    checkTimer();
  }
}

void checkRTCSchedule() {
  DateTime now = rtc.now();
  
  // Cek apakah waktu sekarang adalah jam 7 pagi atau jam 15 sore
  if ((now.hour() == 7 || now.hour() == 16) && now.minute() == 25 && now.second() == 0) {
    if (!timerActive) {
      // Aktifkan timer dan nyalakan relay
      timerActive = true;
      timerStart = millis();
      digitalWrite(RELAY1_PIN, LOW);  // Nyalakan fan
      digitalWrite(RELAY2_PIN, LOW);  // Nyalakan pump
      Serial.println("Scheduled activation started");
    }
  }
}

void checkTimer() {
  if (timerActive) {
    if (millis() - timerStart >= TIMER_DURATION) {
      timerActive = false;
      Serial.println("Timer completed, returning to normal auto mode");
      
      // Baca data sensor untuk mode auto
      SensorData data;
      data.temp1 = dht_sensor1.readTemperature();
      data.humidity1 = dht_sensor1.readHumidity();
      data.temp2 = dht_sensor2.readTemperature();
      data.humidity2 = dht_sensor2.readHumidity();
      
      // Periksa pembacaan sensor valid
      if (!isnan(data.temp1) && !isnan(data.humidity1) && !isnan(data.temp2) && !isnan(data.humidity2)) {
        // Langsung jalankan kontroler auto
        controlRelaysAuto(data);
      } else {
        // Jika sensor error, matikan relay untuk keamanan
        digitalWrite(RELAY1_PIN, HIGH);  // Matikan fan
        digitalWrite(RELAY2_PIN, HIGH);  // Matikan pump
      }
    }
  }
}
void checkForCommands() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String command = "";
    while (LoRa.available()) {
      command += (char)LoRa.read();
    }
    
    // Process command
    if (command.startsWith("CMD:")) {
      executeCommand(command.substring(4));
    }
  }
}

void executeCommand(String command) {
  Serial.print("Received command: ");
  Serial.println(command);
  
  if (command == "AUTO") {
    autoMode = true;
    timerActive = false;  // Reset timer when switching to auto
    Serial.println("Switching to AUTO mode");
  }
  else if (command == "MANUAL") {
    autoMode = false;
    timerActive = false;  // Reset timer when switching to manual
    Serial.println("Switching to MANUAL mode");
  }
  else if (command == "FAN_ON") {
    manualFanStatus = true;
    if (!autoMode) digitalWrite(RELAY1_PIN, LOW);
    Serial.println("Fan turned ON manually");
  }
  else if (command == "FAN_OFF") {
    manualFanStatus = false;
    if (!autoMode) digitalWrite(RELAY1_PIN, HIGH);
    Serial.println("Fan turned OFF manually");
  }
  else if (command == "PUMP_ON") {
    manualPumpStatus = true;
    if (!autoMode) digitalWrite(RELAY2_PIN, LOW);
    Serial.println("Pump turned ON manually");
  }
  else if (command == "PUMP_OFF") {
    manualPumpStatus = false;
    if (!autoMode) digitalWrite(RELAY2_PIN, HIGH);
    Serial.println("Pump turned OFF manually");
  }
}

void sendSensorData() {
  SensorData data;

  // Baca data sensor
  data.temp1 = dht_sensor1.readTemperature() - 3;
  data.humidity1 = dht_sensor1.readHumidity() - 4;
  data.temp2 = dht_sensor2.readTemperature();
  data.humidity2 = dht_sensor2.readHumidity();

  // Validasi pembacaan data sensor
  if (isnan(data.temp1) || isnan(data.humidity1) || isnan(data.temp2) || isnan(data.humidity2)) {
    Serial.println("Error reading DHT sensor data");
    return;
  }

  // Kontrol relay berdasarkan mode
  if (autoMode && !timerActive) {
    controlRelaysAuto(data);
  } else if (!autoMode) {
    data.fan_status = manualFanStatus;
    data.pump_status = manualPumpStatus;
  }

  // Update status relay berdasarkan pin output
  data.fan_status = (digitalRead(RELAY1_PIN) == LOW);
  data.pump_status = (digitalRead(RELAY2_PIN) == LOW);

  // Kirim data LoRa
  int attempts = 0;
  bool sent = false;
  while (!sent && attempts < 5) {
    LoRa.beginPacket();
    LoRa.print("DATA:");
    LoRa.print(data.temp1);
    LoRa.print(",");
    LoRa.print(data.humidity1);
    LoRa.print(",");
    LoRa.print(data.temp2);
    LoRa.print(",");
    LoRa.print(data.humidity2);
    LoRa.print(",");
    LoRa.print(data.fan_status);
    LoRa.print(",");
    LoRa.print(data.pump_status);
    LoRa.print(",");
    LoRa.print(autoMode);
    int result = LoRa.endPacket();
    
    if (result == 1) {
      sent = true;
      Serial.println("Data sent successfully.");
    } else {
      Serial.println("Failed to send data, retrying...");
      attempts++;
    }
  }

  if (!sent) {
    Serial.println("Failed to send data after 5 attempts.");
  }

  // Print data untuk debugging
  printSensorData(data);
}

void controlRelaysAuto(SensorData &data) {
  // Jika timer aktif, skip kontrol otomatis
  if (timerActive) return;
  
  // Kontrol Kipas
  if (data.temp1 > temp_threshold_max || data.temp2 > temp_threshold_max) {
    digitalWrite(RELAY1_PIN, LOW);  // Relay aktif LOW
    data.fan_status = true;
  } else {
    digitalWrite(RELAY1_PIN, HIGH);  // Matikan fan
    data.fan_status = false;
  }
  
  // Kontrol Pompa
  if (data.humidity1 < humidity_threshold_min || data.humidity2 < humidity_threshold_min) {
    digitalWrite(RELAY2_PIN, LOW);  // Relay aktif LOW
    data.pump_status = true;
  } else {
    digitalWrite(RELAY2_PIN, HIGH);  // Matikan pump
    data.pump_status = false;
  }
}


void printSensorData(SensorData data) {
  DateTime now = rtc.now();
  
  Serial.println("\n--- Data Sensor ---");
  Serial.print("Time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
  
  Serial.print("Mode: ");
  Serial.println(autoMode ? "AUTO" : "MANUAL");
  
  if (timerActive) {
    Serial.println("Timer Active: Scheduled control in progress");
  }
  
  Serial.print("Sensor 1 - Suhu: ");
  Serial.print(data.temp1);
  Serial.print("°C, Kelembapan: ");
  Serial.print(data.humidity1);
  Serial.println("%");
  
  Serial.print("Sensor 2 - Suhu: ");
  Serial.print(data.temp2);
  Serial.print("°C, Kelembapan: ");
  Serial.print(data.humidity2);
  Serial.println("%");
  
  Serial.print("Status Kipas: ");
  Serial.println(data.fan_status ? "ON" : "OFF");
  
  Serial.print("Status Pompa: ");
  Serial.println(data.pump_status ? "ON" : "OFF");
  Serial.println("------------------");
} 