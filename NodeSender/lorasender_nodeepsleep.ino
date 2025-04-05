// Removed all deep sleep and related calls
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "HT_SSD1306Wire.h"
#include "DHT.h"
#include <Wire.h>

extern SSD1306Wire display;

#define RF_FREQUENCY 922200000
#define TX_OUTPUT_POWER 5
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 1000
#define BUFFER_SIZE 80

#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_MOISTURE_PIN 7
#define BLUE_LED 35
int soilMoistureRaw = 0;
float moisturePercentage = 0.0;
int dryValue = 2500;
int wetValue = 1000;

#define DEVICE_ID "node1"

float lastTemperature = -1000.0;
float lastHumidity = -1000.0;
int lastSoilMoisture = -1;

#define TEMP_THRESHOLD 0.5
#define HUM_THRESHOLD 1.0
#define SOIL_THRESHOLD 3
#define SEND_INTERVAL 10

static RadioEvents_t RadioEvents;

void OnTxDone(void);
void OnTxTimeout(void);
char txpacket[BUFFER_SIZE];

void readDHT(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("❌ Error reading from DHT sensor!");
    updateOLED("DHT Error!", "Check Sensor", "");
    return;
  }
}

void readSoilMoisture() {
  soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
  Serial.printf("🔍 Soil Raw ADC: %d\n", soilMoistureRaw);
  if (soilMoistureRaw <= 0 || soilMoistureRaw > 4095) {
    Serial.println("⚠️ Sensor Error: Invalid ADC Reading!");
    moisturePercentage = 0;
  } else {
    moisturePercentage = map(soilMoistureRaw, dryValue, wetValue, 0, 100);
    moisturePercentage = constrain(moisturePercentage, 0, 100);
  }
  Serial.printf("🌱 Moisture Percentage: %.1f%%\n", moisturePercentage);
}

void updateOLED(String line1, String line2, String line3) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(5, 0, line1);
  display.drawString(5, 12, line2);
  display.drawString(5, 24, line3);
  display.display();
}

void sendLoRa(float temperature, float humidity, int soilMoisture) {
  sprintf(txpacket, "ID:%s|T: %.2f C, H: %.2f %%, Soil: %d%%", DEVICE_ID, temperature, humidity, soilMoisture);
  Serial.printf("\r\n📡 Sending packet: \"%s\", length %d\r\n", txpacket, strlen(txpacket));
  updateOLED("Sending Data...", "Temp: " + String(temperature, 2) + "C, Humi: " + String(humidity, 2) + "%", "Soil: " + String(soilMoisture) + "%");
  digitalWrite(BLUE_LED, HIGH);
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
  digitalWrite(BLUE_LED, LOW);
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Mcu.begin();
  dht.begin();
  pinMode(BLUE_LED, OUTPUT);
  digitalWrite(BLUE_LED, LOW);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);
  display.init();
  display.clear();
  display.display();
  updateOLED("LoRa Ready", "Waiting...", "");
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON, true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
  Serial.println("✅ Setup Complete!");
}

void loop() {
  float temperature, humidity;
  readDHT(temperature, humidity);
  readSoilMoisture();
  if (abs(temperature - lastTemperature) >= TEMP_THRESHOLD ||
      abs(humidity - lastHumidity) >= HUM_THRESHOLD ||
      abs(moisturePercentage - lastSoilMoisture) >= SOIL_THRESHOLD) {
    lastTemperature = temperature;
    lastHumidity = humidity;
    lastSoilMoisture = moisturePercentage;
    sendLoRa(temperature, humidity, lastSoilMoisture);
  } else {
    Serial.println("⚠️ No significant change, not sending.");
    updateOLED("No Change", "Temp: " + String(temperature, 2) + "C, Humi: " + String(humidity, 2) + "%", "Soil: " + String(moisturePercentage) + "%");
  }
  Radio.IrqProcess();
  delay(SEND_INTERVAL * 1000); // replace deep sleep with regular delay
}

void OnTxDone(void) {
  Serial.println("✅ TX Done!");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(5, 0, "Node1: LoRa Sender");
  display.drawString(5, 12, "T: " + String(lastTemperature, 2) + "C  H: " + String(lastHumidity, 2) + " %RH");
  display.drawString(5, 24, "SoilMoisture: " + String(lastSoilMoisture, 2) + " %");
  display.drawString(5, 36, "TX Done! Wait next...");
  display.display();
}

void OnTxTimeout(void) {
  Serial.println("❌ TX Timeout...");
  updateOLED("TX Timeout!", "Retrying...", "");
}
