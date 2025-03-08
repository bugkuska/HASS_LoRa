#include "LoRaWan_APP.h"     // ✅ ไลบรารีสำหรับการใช้งาน LoRaWAN (รองรับการรับ-ส่งข้อมูลผ่าน LoRa)
#include "Arduino.h"         // ✅ ไลบรารีหลักของ Arduino (รองรับฟังก์ชันพื้นฐาน เช่น `delay()`, `millis()`, `pinMode()` ฯลฯ)
#include "HT_SSD1306Wire.h"  // ✅ ไลบรารีสำหรับจอ OLED (Heltec SSD1306) ใช้สำหรับแสดงผลข้อมูล
#include "DHT.h"             // ✅ ไลบรารีสำหรับเซ็นเซอร์ DHT (DHT11, DHT22) ใช้วัดอุณหภูมิและความชื้น
#include <Wire.h>            // ✅ ไลบรารีสำหรับโปรโตคอล I2C (ใช้สื่อสารกับอุปกรณ์ เช่น OLED, เซ็นเซอร์, EEPROM)
#include "esp_sleep.h"       // ✅ ไลบรารีสำหรับจัดการโหมด Sleep ของ ESP32 เพื่อลดการใช้พลังงาน

// ✅ ใช้ `extern` เพื่อประกาศตัวแปร `display` สำหรับจอ OLED ที่กำหนดไว้ในบอร์ด Heltec ESP32 LoRa
extern SSD1306Wire display;

// ✅ กำหนดค่าของ LoRa
#define RF_FREQUENCY 922200000            // ✅ ความถี่ของ LoRa (922.2 MHz) - ต้องตรงกับอุปกรณ์ที่รับ-ส่งข้อมูล
#define TX_OUTPUT_POWER 5                 // ✅ กำลังส่งของ LoRa (5 dBm) - ค่าที่สูงขึ้นช่วยเพิ่มระยะทาง แต่ใช้พลังงานมากขึ้น
#define LORA_BANDWIDTH 0                  // ✅ ความกว้างของแบนด์วิดท์ (0 = 125 kHz, 1 = 250 kHz, 2 = 500 kHz) - ค่า BW ที่ต่ำช่วยให้รับสัญญาณไกลขึ้น แต่ลดอัตราการส่งข้อมูล
#define LORA_SPREADING_FACTOR 7           // ✅ ค่า Spreading Factor (SF) (ค่ามากขึ้นช่วยให้รับสัญญาณได้ไกลขึ้น แต่ลดอัตราการส่งข้อมูล)
#define LORA_CODINGRATE 1                 // ✅ ค่า Coding Rate (1 = 4/5, 2 = 4/6, 3 = 4/7, 4 = 4/8) - ค่า CR สูงขึ้นช่วยให้แก้ไขข้อผิดพลาดได้ดีขึ้น แต่ลดอัตราการส่งข้อมูล
#define LORA_PREAMBLE_LENGTH 8            // ✅ ความยาวของ Preamble (ใช้สำหรับซิงค์สัญญาณ LoRa) - ค่าเริ่มต้นที่เหมาะสมคือ 8
#define LORA_SYMBOL_TIMEOUT 0             // ✅ ค่า Timeout ของสัญญาณ LoRa (0 = ไม่กำหนด)
#define LORA_FIX_LENGTH_PAYLOAD_ON false  // ✅ ใช้ขนาด Payload แบบคงที่หรือไม่ (false = ใช้ขนาดไม่คงที่)
#define LORA_IQ_INVERSION_ON false        // ✅ ใช้ Inverted IQ หรือไม่ (false = ค่าเริ่มต้น) - ใช้ในบางโหมดเพื่อช่วยลดสัญญาณรบกวน
#define RX_TIMEOUT_VALUE 1000             // ✅ Timeout ของการรับข้อมูล LoRa (1 วินาที)
#define BUFFER_SIZE 80                    // ✅ ขนาดของ Buffer ที่ใช้เก็บข้อมูล LoRa (80 ไบต์)

// ✅ กำหนดค่าของ DHT22 (เซ็นเซอร์วัดอุณหภูมิและความชื้น)
#define DHTPIN 5           // กำหนดขา GPIO ที่ใช้เชื่อมต่อกับเซ็นเซอร์ DHT22 (ขา 5)
#define DHTTYPE DHT22      // ระบุประเภทของเซ็นเซอร์ที่ใช้เป็น DHT22 (มีความแม่นยำสูงกว่า DHT11)
DHT dht(DHTPIN, DHTTYPE);  // สร้างอ็อบเจ็กต์ `dht` เพื่อใช้งานเซ็นเซอร์ DHT22

// ✅ กำหนดค่าของเซ็นเซอร์วัดความชื้นในดิน (Soil Moisture Sensor)
#define SOIL_MOISTURE_PIN 7      // ขา GPIO ที่ใช้รับสัญญาณจากเซ็นเซอร์วัดความชื้นในดิน (ขา 7)
#define BLUE_LED 35              // ขา GPIO ที่ใช้ควบคุมไฟ LED สีฟ้า (ขา 35)
int soilMoistureRaw = 0;         // ตัวแปรเก็บค่าความชื้นจากเซ็นเซอร์ (เป็นค่าดิบจาก ADC)
float moisturePercentage = 0.0;  // ตัวแปรเก็บค่าความชื้นในดินเป็นเปอร์เซ็นต์
int dryValue = 2500;             // ค่าที่อ่านได้จากเซ็นเซอร์เมื่อดินแห้งสนิท (ค่าที่ต้องปรับเทียบเอง)
int wetValue = 1000;             // ค่าที่อ่านได้จากเซ็นเซอร์เมื่อดินเปียกสนิท (ค่าที่ต้องปรับเทียบเอง)

// ✅ กำหนด `DEVICE_ID`
#define DEVICE_ID "node1"

// ✅ ตั้งค่าข้อมูลล่าสุด
float lastTemperature = -1000.0;
float lastHumidity = -1000.0;
int lastSoilMoisture = -1;

// ✅ ตั้งค่าเงื่อนไขการส่งข้อมูล
#define TEMP_THRESHOLD 0.5  // ✅ ส่งข้อมูลถ้าอุณหภูมิเปลี่ยนมากกว่า ±0.5°C
#define HUM_THRESHOLD 1.0   // ✅ ส่งข้อมูลถ้าความชื้นเปลี่ยนมากกว่า ±1.0%
#define SOIL_THRESHOLD 3    // ✅ ส่งข้อมูลถ้าค่าความชื้นในดินเปลี่ยนมากกว่า ±3%
#define SEND_INTERVAL 10    // ✅ ส่งข้อมูลทุก ๆ 10 วินาที

// ✅ สร้างโครงสร้างตัวแปร `RadioEvents` สำหรับจัดเก็บฟังก์ชัน callback ที่เกี่ยวข้องกับ LoRa
static RadioEvents_t RadioEvents;

// ✅ ฟังก์ชัน Callback ที่จะถูกเรียกเมื่อการส่งข้อมูลสำเร็จ (Tx Done)
// ฟังก์ชันนี้จะถูกกำหนดให้กับ `RadioEvents`
void OnTxDone(void);

// ✅ ฟังก์ชัน Callback ที่จะถูกเรียกเมื่อการส่งข้อมูลล้มเหลว (Tx Timeout)
// ฟังก์ชันนี้จะถูกเรียกเมื่อ LoRa ไม่สามารถส่งข้อมูลภายในเวลาที่กำหนด
void OnTxTimeout(void);

// ✅ ตัวแปรสำหรับเก็บข้อมูลที่ต้องการส่งผ่าน LoRa
// ใช้ขนาดของ Buffer ที่กำหนดไว้ก่อนหน้า (`BUFFER_SIZE`)
// `txpacket` จะถูกใช้เก็บข้อความหรือข้อมูลที่ต้องการส่งไปยังอุปกรณ์ปลายทาง
char txpacket[BUFFER_SIZE];

// ✅ ฟังก์ชันอ่านค่า DHT22
// ฟังก์ชันนี้ใช้สำหรับอ่านค่าอุณหภูมิและความชื้นจากเซ็นเซอร์ DHT22
// และตรวจสอบว่าค่าที่อ่านได้มีความถูกต้องหรือไม่
void readDHT(float &temperature, float &humidity) {
  // อ่านค่าอุณหภูมิจากเซ็นเซอร์ DHT22 (ค่าเป็นองศาเซลเซียส)
  temperature = dht.readTemperature();
  // อ่านค่าความชื้นสัมพัทธ์จากเซ็นเซอร์ DHT22 (ค่าเป็น %RH)
  humidity = dht.readHumidity();
  // ตรวจสอบว่าการอ่านค่าจากเซ็นเซอร์สำเร็จหรือไม่
  // หากค่าที่ได้เป็น NaN (Not a Number) แสดงว่าการอ่านค่าล้มเหลว
  if (isnan(temperature) || isnan(humidity)) {
    // แสดงข้อความแจ้งเตือนผ่าน Serial Monitor
    Serial.println("❌ Error reading from DHT sensor!");
    // แสดงข้อความแจ้งเตือนบนหน้าจอ OLED ให้ผู้ใช้ตรวจสอบเซ็นเซอร์
    updateOLED("DHT Error!", "Check Sensor", "");
    // เข้าสู่โหมด deep sleep เพื่อหยุดการทำงานชั่วคราว (หรือรีสตาร์ทระบบ)
    deepSleep();
  }
}

// ✅ ฟังก์ชันอ่านค่าความชื้นในดิน
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

// ✅ ฟังก์ชันแสดงผลบน OLED (รองรับการแสดงผล 3 แถว)
// ฟังก์ชันนี้ใช้สำหรับแสดงข้อความบนหน้าจอ OLED SSD1306 บนบอร์ด Heltec ESP32 LoRa
// รับพารามิเตอร์ 3 บรรทัด (line1, line2, line3) และแสดงผลในตำแหน่งที่กำหนด
void updateOLED(String line1, String line2, String line3) {
  display.clear();  // ✅ ล้างหน้าจอ OLED ก่อนแสดงผลใหม่
  // ✅ ตั้งค่าฟอนต์ที่ใช้ (Arial ขนาด 10px)
  display.setFont(ArialMT_Plain_10);
  // ✅ ตั้งค่าการจัดวางข้อความ (ชิดซ้าย)
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  // ✅ แสดงข้อความบรรทัดที่ 1 (ตำแหน่ง X = 5, Y = 0)
  display.drawString(5, 0, line1);
  // ✅ แสดงข้อความบรรทัดที่ 2 (ตำแหน่ง X = 5, Y = 12)
  display.drawString(5, 12, line2);
  // ✅ แสดงข้อความบรรทัดที่ 3 (ตำแหน่ง X = 5, Y = 24)
  display.drawString(5, 24, line3);
  // ✅ อัปเดตหน้าจอ OLED เพื่อให้ข้อความปรากฏ
  display.display();
}

// ✅ ฟังก์ชันส่งข้อมูลผ่าน LoRa
// ฟังก์ชันนี้ใช้สำหรับส่งข้อมูลอุณหภูมิ ความชื้น และความชื้นในดินผ่านเครือข่าย LoRa
void sendLoRa(float temperature, float humidity, int soilMoisture) {
  // ✅ จัดรูปแบบข้อมูลที่ต้องการส่งโดยใช้ `sprintf()`
  // รูปแบบข้อมูลที่ส่ง: "ID:<DEVICE_ID>|T: <Temperature> C, H: <Humidity> %, Soil: <SoilMoisture> %"
  sprintf(txpacket, "ID:%s|T: %.2f C, H: %.2f %%, Soil: %d%%", DEVICE_ID, temperature, humidity, soilMoisture);
  // ✅ แสดงข้อมูลที่กำลังจะส่งไปยัง LoRa บน Serial Monitor
  Serial.printf("\r\n📡 Sending packet: \"%s\", length %d\r\n", txpacket, strlen(txpacket));
  // ✅ อัปเดตหน้าจอ OLED เพื่อแสดงข้อความว่ากำลังส่งข้อมูล
  updateOLED("Sending Data...", "Temp: " + String(temperature, 2) + "C, Humi: " + String(humidity, 2) + "%", "Soil: " + String(soilMoisture) + "%");
  // ✅ เปิดไฟ LED สีน้ำเงินเพื่อระบุว่ากำลังส่งข้อมูล
  digitalWrite(BLUE_LED, HIGH);
  // ✅ ส่งข้อมูลผ่าน LoRa
  // `Radio.Send()` ใช้สำหรับส่งข้อมูลในรูปแบบของ byte array (`uint8_t`)
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
  // ✅ ปิดไฟ LED หลังจากส่งข้อมูลเสร็จ
  digitalWrite(BLUE_LED, LOW);
  // ✅ หน่วงเวลา 1 วินาที (1000 มิลลิวินาที) เพื่อให้แน่ใจว่าข้อมูลถูกส่งออกไปก่อน
  delay(1000);
  // ✅ เข้าโหมด Deep Sleep เพื่อลดการใช้พลังงาน (อุปกรณ์จะพักการทำงานชั่วคราว)
  //deepSleep();
}

// ✅ ฟังก์ชัน Setup
// ฟังก์ชันนี้ทำงานเพียงครั้งเดียวเมื่อตัว ESP32 เปิดเครื่องหรือรีสตาร์ท
// ใช้สำหรับกำหนดค่าต่าง ๆ เช่น การสื่อสาร Serial, LoRa, OLED Display และเซ็นเซอร์
void setup() {
  // ✅ เริ่มต้นการสื่อสารผ่าน Serial Monitor ที่ baud rate 115200
  Serial.begin(115200);
  // ✅ เริ่มต้นการทำงานของ MCU (Heltec ESP32 LoRa)
  Mcu.begin();
  // ✅ เริ่มต้นการทำงานของเซ็นเซอร์ DHT22
  dht.begin();
  // ✅ กำหนดขา LED สีฟ้าเป็น OUTPUT และปิดไฟ LED ก่อนเริ่มต้นทำงาน
  pinMode(BLUE_LED, OUTPUT);
  digitalWrite(BLUE_LED, LOW);
  // ✅ กำหนดขาของเซ็นเซอร์วัดความชื้นในดินให้เป็น INPUT
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  // ✅ เปิดใช้งานไฟ Vext (ใช้จ่ายไฟให้กับ OLED Display)
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);  // หน่วงเวลา 100 มิลลิวินาที เพื่อให้จอ OLED พร้อมทำงาน
  // ✅ เริ่มต้นการทำงานของ OLED Display
  display.init();
  display.clear();
  display.display();
  // ✅ แสดงข้อความบนหน้าจอ OLED ว่าระบบ LoRa พร้อมใช้งาน
  updateOLED("LoRa Ready", "Waiting...", "");
  // ✅ ตั้งค่าฟังก์ชัน callback ของ LoRa
  // - `TxDone` จะถูกเรียกเมื่อการส่งข้อมูลสำเร็จ
  // - `TxTimeout` จะถูกเรียกเมื่อการส่งข้อมูลล้มเหลวหรือหมดเวลา
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  // ✅ เริ่มต้นโมดูล LoRa และกำหนดค่าพารามิเตอร์ต่าง ๆ
  Radio.Init(&RadioEvents);        // กำหนดค่าการเชื่อมต่อ LoRa
  Radio.SetChannel(RF_FREQUENCY);  // ตั้งค่าความถี่ของ LoRa
  // ✅ กำหนดค่าการส่งข้อมูลของ LoRa
  Radio.SetTxConfig(
    MODEM_LORA,                  // เลือกโหมดการทำงานเป็น LoRa
    TX_OUTPUT_POWER,             // กำหนดกำลังส่งของ LoRa (dBm)
    0,                           // Frequency deviation (ใช้เฉพาะ FSK, ไม่ใช้กับ LoRa)
    LORA_BANDWIDTH,              // กำหนดค่าความกว้างของแบนด์วิดท์
    LORA_SPREADING_FACTOR,       // กำหนดค่า Spreading Factor (ค่ามากขึ้น = ส่งไกลขึ้นแต่ช้าลง)
    LORA_CODINGRATE,             // กำหนดค่า Coding Rate (ใช้สำหรับแก้ไขข้อผิดพลาด)
    LORA_PREAMBLE_LENGTH,        // กำหนดความยาวของ Preamble (ใช้จับสัญญาณ LoRa)
    LORA_FIX_LENGTH_PAYLOAD_ON,  // ใช้ Payload แบบขนาดคงที่หรือไม่
    true,                        // เปิดใช้งาน CRC (Cyclic Redundancy Check)
    0, 0,                        // กำหนดค่าควบคุมพลังงาน RF (0 = ค่าเริ่มต้น)
    LORA_IQ_INVERSION_ON,        // ใช้ Inverted IQ หรือไม่ (false = ค่าเริ่มต้น)
    3000                         // กำหนด timeout (3 วินาที)
  );
  // ✅ แสดงข้อความแจ้งเตือนใน Serial Monitor ว่าการตั้งค่าเสร็จสมบูรณ์
  Serial.println("✅ Setup Complete!");
}


// ✅ ฟังก์ชัน Loop
// ฟังก์ชันนี้ทำงานเป็นลูปซ้ำไปเรื่อย ๆ หลังจาก `setup()` ทำงานเสร็จ
// ทำหน้าที่อ่านค่าจากเซ็นเซอร์ ตรวจสอบความเปลี่ยนแปลงของข้อมูล และส่งข้อมูลผ่าน LoRa เมื่อจำเป็น
void loop() {
  float temperature, humidity;  // ✅ ตัวแปรเก็บค่าอุณหภูมิและความชื้นที่อ่านจาก DHT22
  // ✅ อ่านค่าอุณหภูมิและความชื้นจากเซ็นเซอร์ DHT22
  readDHT(temperature, humidity);
  // ✅ อ่านค่าความชื้นในดินจากเซ็นเซอร์ความชื้นในดิน
  readSoilMoisture();
  // ✅ ตรวจสอบว่าค่าที่อ่านมาเปลี่ยนแปลงจากค่าล่าสุดเกินกว่าค่าที่กำหนดหรือไม่
  if (abs(temperature - lastTemperature) >= TEMP_THRESHOLD || abs(humidity - lastHumidity) >= HUM_THRESHOLD || abs(moisturePercentage - lastSoilMoisture) >= SOIL_THRESHOLD) {
    // ✅ อัปเดตค่าล่าสุดของอุณหภูมิ ความชื้น และความชื้นในดิน
    lastTemperature = temperature;
    lastHumidity = humidity;
    lastSoilMoisture = moisturePercentage;
    // ✅ ส่งข้อมูลผ่าน LoRa (เฉพาะเมื่อค่ามีการเปลี่ยนแปลงที่สำคัญ)
    sendLoRa(temperature, humidity, lastSoilMoisture);
  } else {
    // ✅ กรณีค่าที่อ่านได้ไม่มีการเปลี่ยนแปลงที่สำคัญ แสดงข้อความใน Serial Monitor
    Serial.println("⚠️ No significant change, not sending.");
    // ✅ แสดงค่าปัจจุบันบนหน้าจอ OLED โดยไม่มีการส่งข้อมูล
    updateOLED("Recv. & FW to MQTT", "Temp: " + String(temperature, 2) + "C, Humi: " + String(humidity, 2) + "%", "Soil: " + String(moisturePercentage) + "%");
    // ✅ เข้าโหมด Deep Sleep เพื่อประหยัดพลังงาน
    deepSleep();
  }
  // ✅ จัดการอินเทอร์รัพท์ (Interrupt) ของ LoRa
  // ใช้สำหรับจัดการเหตุการณ์ที่เกิดขึ้น เช่น การรับ-ส่งข้อมูล
  Radio.IrqProcess();
}

// ✅ ฟังก์ชันเข้าสู่โหมด Deep Sleep
// ฟังก์ชันนี้ใช้สำหรับทำให้ ESP32 เข้าสู่โหมดประหยัดพลังงาน (Deep Sleep)
// และตั้งเวลาให้ปลุกตัวเองขึ้นมาทำงานใหม่หลังจากเวลาที่กำหนด
void deepSleep() {
  // ✅ แสดงข้อความแจ้งเตือนผ่าน Serial Monitor ว่ากำลังเข้าสู่โหมด Deep Sleep
  Serial.println("💤 Entering Deep Sleep...");
  // ✅ ปิดการทำงานของวงจรจ่ายไฟให้กับ OLED Display
  digitalWrite(Vext, HIGH);
  // ✅ ปิดการทำงานของโมดูล LoRa เพื่อลดการใช้พลังงาน
  Radio.Sleep();
  // ✅ ตั้งค่าให้ ESP32 ตื่นขึ้นมาอีกครั้งหลังจากเวลาที่กำหนด (SEND_INTERVAL วินาที)
  // `SEND_INTERVAL` ถูกกำหนดเป็นวินาที ดังนั้นต้องแปลงเป็นไมโครวินาที (1 วินาที = 1,000,000 ไมโครวินาที)
  esp_sleep_enable_timer_wakeup(SEND_INTERVAL * 1000000);
  // ✅ เริ่มโหมด Deep Sleep
  esp_deep_sleep_start();
}

// ✅ ฟังก์ชัน Callback เมื่อ LoRa ส่งข้อมูลสำเร็จ (Tx Done)
// ฟังก์ชันนี้จะถูกเรียกอัตโนมัติเมื่อ ESP32 ส่งข้อมูลผ่าน LoRa สำเร็จ
void OnTxDone(void) {
  Serial.println("✅ TX Done!");  // ✅ แสดงข้อความใน Serial Monitor ว่าการส่งข้อมูลสำเร็จ
  // ✅ แสดงข้อมูลที่ส่งล่าสุดบน Serial Monitor
  Serial.printf("TX Done! Sent Data -> T: %.2f°C, H: %.2f%%, Soil: %.2f%%\n", lastTemperature, lastHumidity, lastSoilMoisture);
  // ✅ แสดงข้อมูลบน OLED จำนวน 4 บรรทัด
  display.clear();                            // ✅ ล้างหน้าจอ OLED ก่อนแสดงข้อมูลใหม่
  display.setFont(ArialMT_Plain_10);          // ✅ กำหนดฟอนต์เป็น Arial ขนาด 10px
  display.setTextAlignment(TEXT_ALIGN_LEFT);  // ✅ จัดวางข้อความให้อยู่ชิดซ้าย
  // ✅ บรรทัดที่ 1: แสดงชื่อ Node ของ LoRa
  display.drawString(5, 0, "Node1: LoRa Sender");
  // ✅ บรรทัดที่ 2: แสดงค่าอุณหภูมิและความชื้นสัมพัทธ์
  display.drawString(5, 12, "T: " + String(lastTemperature, 2) + "C  H: " + String(lastHumidity, 2) + " %RH");
  // ✅ บรรทัดที่ 3: แสดงค่าความชื้นในดิน (%)
  display.drawString(5, 24, "SoilMoisture: " + String(lastSoilMoisture, 2) + " %");
  // ✅ บรรทัดที่ 4: แสดงข้อความแจ้งว่าอุปกรณ์กำลังจะเข้าสู่ Sleep ใน 10 วินาที
  display.drawString(5, 36, "TX Done! Sleeping in 10s...");
  // ✅ แสดงข้อมูลทั้งหมดบนจอ OLED
  display.display();
  // ✅ ใช้ millis() รอ 10 วินาทีก่อนเข้าสู่ Deep Sleep
  Serial.println("💤 Waiting 10s before Deep Sleep...");
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) {
    // ✅ วนลูปรอ 10 วินาทีโดยไม่เข้า Sleep ทันที เพื่อให้ OLED แสดงข้อมูลครบตามเวลา
  }
  // ✅ หลังจากรอครบ 10 วินาที ให้แสดงข้อความแจ้งว่า ESP32 กำลังเข้าสู่โหมด Deep Sleep
  Serial.println("💤 Now entering Deep Sleep...");
  // ✅ เข้าสู่โหมด Deep Sleep เพื่อลดการใช้พลังงาน
  deepSleep();
}

// ✅ ฟังก์ชัน Callback เมื่อ LoRa ส่งข้อมูลล้มเหลว (TX Timeout)
// ฟังก์ชันนี้จะถูกเรียกอัตโนมัติเมื่อ LoRa ไม่สามารถส่งข้อมูลได้ภายในเวลาที่กำหนด
void OnTxTimeout(void) {
    Serial.println("❌ TX Timeout...");  // ✅ แสดงข้อความใน Serial Monitor ว่าการส่งข้อมูลล้มเหลว
    // ✅ แสดงข้อความบน OLED แจ้งว่าการส่งข้อมูลล้มเหลว และจะลองใหม่ใน 10 วินาที
    updateOLED("TX Timeout!", "Retrying in 10s...", "");
    // ✅ แสดงข้อความใน Serial Monitor ว่าระบบกำลังรอ 10 วินาทีก่อนเข้าสู่ Deep Sleep
    Serial.println("🕒 Waiting 10s before Deep Sleep...");
    // ✅ ใช้ `millis()` รอ 10 วินาทีก่อนเข้า Deep Sleep แทน `delay(10000);`
    unsigned long startTime = millis();
    while (millis() - startTime < 10000) {
        // ✅ วนลูปรอ 10 วินาทีโดยไม่เข้า Sleep ทันที เพื่อให้ข้อมูลแสดงครบตามเวลา
    }
    // ✅ หลังจากรอครบ 10 วินาที ให้แสดงข้อความแจ้งว่า ESP32 กำลังเข้าสู่โหมด Deep Sleep
    Serial.println("💤 Now entering Deep Sleep...");
    // ✅ เข้าสู่โหมด Deep Sleep เพื่อลดการใช้พลังงาน และรอรอบการส่งถัดไป
    deepSleep();
}
