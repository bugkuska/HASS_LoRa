#include "LoRaWan_APP.h"     // ✅ ไลบรารีสำหรับการใช้งาน LoRa (รับ-ส่งข้อมูลผ่าน LoRa)
#include "Arduino.h"         // ✅ ไลบรารีหลักของ Arduino (รองรับฟังก์ชันพื้นฐาน เช่น delay(), millis())
#include "HT_SSD1306Wire.h"  // ✅ ไลบรารีสำหรับใช้งานจอ OLED (Heltec SSD1306)
#include <Wire.h>            // ✅ ไลบรารีสำหรับ I2C (ใช้ควบคุม OLED และอุปกรณ์อื่นๆ)
#include <WiFi.h>            // ✅ ไลบรารีสำหรับเชื่อมต่อ WiFi บน ESP32
#include <PubSubClient.h>    // ✅ ไลบรารีสำหรับเชื่อมต่อและส่งข้อมูลไปยัง MQTT Broker

extern SSD1306Wire display;  // ✅ ใช้ `extern` เพื่อเข้าถึงตัวแปร `display` ของ OLED ที่ถูกกำหนดไว้ในบอร์ด Heltec

// ✅ กำหนดค่าของ LoRa
#define RF_FREQUENCY 922200000            // ✅ กำหนดความถี่ของ LoRa (922.2 MHz) - ต้องตรงกับอุปกรณ์ที่รับส่งข้อมูล
#define TX_OUTPUT_POWER 5                 // ✅ กำหนดกำลังส่งของ LoRa (5 dBm)
#define LORA_BANDWIDTH 0                  // ✅ ความกว้างของแบนด์วิดท์ (0 = 125 kHz)
#define LORA_SPREADING_FACTOR 7           // ✅ ค่า Spreading Factor (ค่ามากขึ้น = สัญญาณไปได้ไกลขึ้น แต่ช้าลง)
#define LORA_CODINGRATE 1                 // ✅ ค่า Coding Rate (1 = 4/5, ใช้สำหรับแก้ไขข้อผิดพลาดของสัญญาณ)
#define LORA_PREAMBLE_LENGTH 8            // ✅ ความยาวของ Preamble (ใช้จับสัญญาณ LoRa)
#define LORA_SYMBOL_TIMEOUT 0             // ✅ ค่า Timeout ของสัญญาณ LoRa
#define LORA_FIX_LENGTH_PAYLOAD_ON false  // ✅ ใช้ขนาด Payload คงที่หรือไม่ (false = ใช้ขนาดไม่คงที่)
#define LORA_IQ_INVERSION_ON false        // ✅ ใช้ Inverted IQ หรือไม่ (false = ค่าเริ่มต้น)
#define RX_TIMEOUT_VALUE 1000             // ✅ Timeout ของการรับข้อมูล LoRa (1 วินาที)
#define BUFFER_SIZE 80                    // ✅ กำหนดขนาดของ Buffer ที่ใช้เก็บข้อมูล LoRa (80 ไบต์)

// ✅ ตัวแปรเก็บข้อมูลที่ได้รับจาก LoRa
// ใช้ขนาดของ Buffer ที่กำหนดไว้ก่อนหน้า (BUFFER_SIZE)
char rxpacket[BUFFER_SIZE];

// ✅ กำหนดโครงสร้างสำหรับจัดเก็บฟังก์ชัน Callback ของ LoRa
// ฟังก์ชันเหล่านี้จะถูกเรียกเมื่อมีเหตุการณ์เกี่ยวกับ LoRa เช่น รับข้อมูลสำเร็จ หรือมีข้อผิดพลาด
static RadioEvents_t RadioEvents;

// ✅ ฟังก์ชัน Callback ที่จะถูกเรียกเมื่อมีการรับข้อมูล LoRa สำเร็จ
// - payload: ข้อมูลที่ได้รับจาก LoRa (เป็น array ของ byte)
// - size: ขนาดของข้อมูลที่รับมา (หน่วยเป็นไบต์)
// - rssi: ค่า RSSI (Received Signal Strength Indicator) วัดความแรงของสัญญาณ LoRa
// - snr: ค่า SNR (Signal-to-Noise Ratio) ใช้บอกคุณภาพของสัญญาณ LoRa
void OnRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);

// ✅ กำหนดค่าการเชื่อมต่อ WiFi และ MQTT
const char* ssid = "";       // ✅ ชื่อเครือข่าย WiFi ที่ต้องการเชื่อมต่อ (SSID)
const char* password = "";  // ✅ รหัสผ่านของ WiFi ที่ต้องใช้ในการเชื่อมต่อ

const char* mqtt_server = "";  // ✅ IP Address ของ MQTT Broker 
const char* mqtt_user = "";          // ✅ ชื่อผู้ใช้สำหรับเชื่อมต่อ MQTT Broker
const char* mqtt_pass = "";       // ✅ รหัสผ่านของ MQTT Broker

// ✅ สร้างอ็อบเจ็กต์สำหรับการเชื่อมต่อ WiFi และ MQTT
WiFiClient espClient;            // ✅ สร้างอ็อบเจ็กต์ WiFiClient สำหรับการเชื่อมต่ออินเทอร์เน็ตผ่าน WiFi
PubSubClient client(espClient);  // ✅ สร้างอ็อบเจ็กต์ PubSubClient สำหรับการเชื่อมต่อ MQTT โดยใช้ WiFiClient เป็นตัวกลาง

// ✅ ฟังก์ชันแสดงผลบน OLED (รองรับ 3 แถว + จัดข้อความกึ่งกลาง)
// ใช้สำหรับแสดงค่าที่รับจาก LoRa และสถานะการทำงานของอุปกรณ์
void updateOLED(String line1, String line2, String line3) {
  display.clear();                              // ✅ ล้างหน้าจอ OLED ก่อนแสดงข้อความใหม่
  display.setFont(ArialMT_Plain_10);            // ✅ กำหนดฟอนต์เป็น Arial ขนาด 10px
  display.setTextAlignment(TEXT_ALIGN_CENTER);  // ✅ จัดให้ข้อความอยู่ตรงกลางหน้าจอ
  // ✅ แสดงข้อความแถวที่ 1 (ตำแหน่งแกน Y = 0)
  display.drawString(display.getWidth() / 2, 0, line1);
  // ✅ แสดงข้อความแถวที่ 2 (ตำแหน่งแกน Y = 12)
  display.drawString(display.getWidth() / 2, 12, line2);
  // ✅ แสดงข้อความแถวที่ 3 (ตำแหน่งแกน Y = 24)
  display.drawString(display.getWidth() / 2, 24, line3);
  display.display();  // ✅ อัปเดตหน้าจอ OLED เพื่อให้ข้อความแสดงผล
}

// ✅ ฟังก์ชันเชื่อมต่อ WiFi
// ใช้สำหรับเชื่อมต่อ ESP32 กับเครือข่าย WiFi ที่กำหนดไว้ (SSID & Password)
void connectWiFi() {
  WiFi.begin(ssid, password);             // ✅ เริ่มต้นการเชื่อมต่อ WiFi โดยใช้ค่าที่กำหนดไว้ใน `ssid` และ `password`
  Serial.print("Connecting to WiFi...");  // ✅ แสดงข้อความใน Serial Monitor เพื่อบอกว่ากำลังเชื่อมต่อ WiFi
  // ✅ วนลูปจนกว่าการเชื่อมต่อจะสำเร็จ (`WL_CONNECTED`)
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);         // ✅ หน่วงเวลา 500ms เพื่อลดการโหลดของ CPU
    Serial.print(".");  // ✅ แสดงจุด (`.`) บน Serial Monitor เพื่อแสดงความคืบหน้าในการเชื่อมต่อ
  }
  // ✅ เมื่อเชื่อมต่อสำเร็จ
  Serial.println("\nWiFi Connected!");  // ✅ แสดงข้อความว่าเชื่อมต่อสำเร็จ
  Serial.print("IP Address: ");         // ✅ แสดง IP Address ที่ได้รับจาก Router
  Serial.println(WiFi.localIP());       // ✅ แสดง IP Address ของ ESP32 บนเครือข่ายที่เชื่อมต่อ
}

// ✅ ฟังก์ชันเชื่อมต่อ MQTT
// ใช้สำหรับเชื่อมต่อ ESP32 กับ MQTT Broker (เช่น Mosquitto, Home Assistant, AWS IoT)
void reconnectMQTT() {
  // ✅ วนลูปจนกว่าการเชื่อมต่อ MQTT จะสำเร็จ
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");  // ✅ แสดงข้อความว่ากำลังเชื่อมต่อ MQTT Broker
    // ✅ พยายามเชื่อมต่อกับ MQTT Broker โดยใช้ Client ID และข้อมูลล็อกอิน
    if (client.connect("Heltec_LoRa_Receiver", mqtt_user, mqtt_pass)) {
      Serial.println("Connected!");  // ✅ แสดงข้อความว่าเชื่อมต่อ MQTT สำเร็จ
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());                 // ✅ แสดงรหัสข้อผิดพลาด (return code) ของการเชื่อมต่อ MQTT
      Serial.println(" retrying in 5 seconds...");  // ✅ แจ้งให้ทราบว่ากำลังลองใหม่ใน 5 วินาที
      delay(5000);                                  // ✅ หน่วงเวลา 5 วินาทีแล้วลองเชื่อมต่อใหม่
    }
  }
}

// ✅ ฟังก์ชัน Setup
// ฟังก์ชันนี้จะทำงานครั้งเดียวเมื่อตัว ESP32 เริ่มต้นการทำงาน
void setup() {
  Serial.begin(115200);  // ✅ เริ่มต้น Serial Monitor ที่ baud rate 115200 สำหรับการ Debug
  Mcu.begin();           // ✅ เริ่มต้น MCU บนบอร์ด Heltec ESP32 LoRa
  // ✅ เปิดไฟให้จอ OLED ทำงาน (Vext ควบคุมพลังงานของจอ OLED บนบอร์ด Heltec)
  pinMode(Vext, OUTPUT);    // ✅ กำหนดขา Vext เป็นขา OUTPUT
  digitalWrite(Vext, LOW);  // ✅ เปิดไฟจอ OLED
  delay(100);               // ✅ หน่วงเวลา 100ms เพื่อให้จอพร้อมทำงาน
  // ✅ เริ่มต้นการทำงานของจอ OLED
  display.init();     // ✅ เริ่มต้นไลบรารี OLED
  display.clear();    // ✅ ล้างข้อมูลที่เคยแสดงผลก่อนหน้า
  display.display();  // ✅ อัปเดตหน้าจอให้เป็นค่าว่าง
  // ✅ แสดงข้อความบน OLED แจ้งสถานะว่ากำลังเชื่อมต่อ WiFi
  updateOLED("LoRa Ready", "Connecting WiFi...", "");
  // ✅ เชื่อมต่อ WiFi และ MQTT Broker
  connectWiFi();                        // ✅ เรียกฟังก์ชันเชื่อมต่อ WiFi
  client.setServer(mqtt_server, 1883);  // ✅ กำหนด MQTT Broker (ใช้พอร์ต 1883 เป็นค่าเริ่มต้น)
  reconnectMQTT();                      // ✅ เรียกฟังก์ชันเชื่อมต่อ MQTT
  // ✅ แสดงข้อความบน OLED แจ้งว่าสามารถเชื่อมต่อ MQTT สำเร็จ
  updateOLED("LoRa Ready", "MQTT Connected", "");
  // ✅ ตั้งค่า Callback ของ LoRa (กำหนดให้เรียกฟังก์ชัน `OnRxDone()` เมื่อมีข้อมูลเข้า)
  RadioEvents.RxDone = OnRxDone;
  // ✅ เริ่มต้นการทำงานของ LoRa และกำหนดค่าต่างๆ
  Radio.Init(&RadioEvents);        // ✅ กำหนดค่าเริ่มต้นให้โมดูล LoRa
  Radio.SetChannel(RF_FREQUENCY);  // ✅ กำหนดความถี่ LoRa ที่ใช้รับ-ส่งข้อมูล
  // ✅ ตั้งค่าการรับข้อมูล LoRa (Modulation และการปรับแต่งค่าต่างๆ)
  Radio.SetRxConfig(
    MODEM_LORA,                  // ✅ ใช้โหมด LoRa (ไม่ใช่ FSK)
    LORA_BANDWIDTH,              // ✅ กำหนดค่าความกว้างของแบนด์วิดท์
    LORA_SPREADING_FACTOR,       // ✅ กำหนดค่า Spreading Factor (ส่งไกลขึ้นแต่ช้าลง)
    LORA_CODINGRATE,             // ✅ กำหนดค่า Coding Rate (ใช้สำหรับแก้ไขข้อผิดพลาดของสัญญาณ)
    0,                           // ✅ ค่า Frequency Deviation (ใช้เฉพาะ FSK, ไม่ใช้กับ LoRa)
    LORA_PREAMBLE_LENGTH,        // ✅ กำหนดความยาวของ Preamble (ช่วยให้จับสัญญาณได้ดีขึ้น)
    LORA_SYMBOL_TIMEOUT,         // ✅ Timeout ของสัญญาณ LoRa
    LORA_FIX_LENGTH_PAYLOAD_ON,  // ✅ ใช้ Payload แบบขนาดคงที่หรือไม่
    0,                           // ✅ ขนาดของ Payload (0 = ไม่กำหนด)
    true,                        // ✅ ใช้ Continuous Receive Mode (เปิดรับสัญญาณตลอด)
    0, 0,                        // ✅ กำหนดค่าควบคุมความแรงของสัญญาณ (0 = ค่าเริ่มต้น)
    LORA_IQ_INVERSION_ON,        // ✅ ใช้ Inverted IQ หรือไม่ (false = ค่าเริ่มต้น)
    true                         // ✅ เปิดโหมด RX Continuous Mode (ESP32 รับข้อมูลตลอด)
  );
  // ✅ เริ่มรับข้อมูล LoRa ทันทีหลังจากบูตเครื่อง
  Radio.Rx(0);  // ✅ เปิดโหมดรับข้อมูลแบบไม่กำหนดเวลาหมดอายุ
}

// ✅ ฟังก์ชัน Loop
// ฟังก์ชันนี้จะทำงานซ้ำไปเรื่อย ๆ หลังจาก `setup()` ทำงานเสร็จ
void loop() {
  // ✅ ตรวจสอบและจัดการอินเทอร์รัพท์ (Interrupt) ของ LoRa
  // ใช้สำหรับจัดการเหตุการณ์ต่างๆ เช่น รับข้อมูล หรือส่งข้อมูล LoRa
  Radio.IrqProcess();
  // ✅ ตรวจสอบว่าการเชื่อมต่อ MQTT ยังทำงานอยู่หรือไม่
  if (!client.connected()) {
    reconnectMQTT();  // ✅ ถ้าการเชื่อมต่อขาด ให้ทำการเชื่อมต่อใหม่
  }
  // ✅ ดำเนินการเกี่ยวกับ MQTT (เช่น รับข้อความใหม่, ส่ง Keep-Alive Ping)
  client.loop();
}

// ✅ ฟังก์ชัน รับ แสดงผลจอ และส่งต่อ mqtt
void OnRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr) {
  // คัดลอกข้อมูลที่รับเข้ามาไปยังตัวแปร `rxpacket`
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';  // เพิ่มเครื่องหมายสิ้นสุดสตริง
  Serial.printf("Received raw packet: \"%s\", RSSI: %d, SNR: %d\n", rxpacket, rssi, snr);

  // ✅ สำรองข้อมูล `rxpacket` ไว้ก่อนใช้ strtok() เพื่อลดความเสี่ยงของปัญหาสตริงที่ถูกเปลี่ยนค่า
  char rxpacket_copy[BUFFER_SIZE];
  strncpy(rxpacket_copy, rxpacket, BUFFER_SIZE);

  // ✅ แยกข้อมูลจากแพ็กเก็ตที่รับมา โดยใช้ '|' เป็นตัวแบ่ง
  char* id_part = strtok(rxpacket_copy, "|");  // ส่วนที่เป็น ID ของอุปกรณ์
  char* data_part = strtok(NULL, "|");        // ส่วนที่เป็นค่าข้อมูลเซ็นเซอร์

  Serial.printf("id_part: %s\n", id_part);
  Serial.printf("data_part: %s\n", data_part);

  if (id_part != NULL && data_part != NULL) {
    // ✅ ดึงค่า Device ID จาก id_part ที่อยู่ในรูปแบบ "ID:xxxx"
    char device_id[10];
    sscanf(id_part, "ID:%s", device_id);

    // ✅ แยกค่าข้อมูลเซ็นเซอร์ โดยใช้ ',' เป็นตัวแบ่ง
    char* temp_part = strtok(data_part, ",");  // ค่าอุณหภูมิ
    char* hum_part = strtok(NULL, ",");        // ค่าความชื้น
    char* soil_part = strtok(NULL, ",");       // ค่าความชื้นในดิน

    Serial.printf("temp_part: %s\n", temp_part);
    Serial.printf("hum_part: %s\n", hum_part);
    Serial.printf("soil_part: %s\n", soil_part);

    // ✅ ตรวจสอบค่าว่าง (NULL) หากไม่มีข้อมูลให้กำหนดเป็น "N/A"
    String tempStr = temp_part != NULL ? String(temp_part) : "T: N/A";
    String humStr = hum_part != NULL ? String(hum_part) : "H: N/A";
    String soilStr = soil_part != NULL ? String(soil_part) : "Soil: N/A";

    // ✅ สร้างข้อความที่ใช้แสดงผลบน OLED
    String displayLine2 = tempStr + " " + humStr;  // แสดงอุณหภูมิและความชื้นบนบรรทัดที่ 2
    String displayLine3 = soilStr;                 // แสดงค่าความชื้นในดินบนบรรทัดที่ 3

    // ✅ แสดงผลข้อมูลบนจอ OLED
    updateOLED("Recv. & FW to MQTT", displayLine2, displayLine3);

    // ✅ สร้าง MQTT Topic สำหรับแต่ละข้อมูลที่ต้องการส่ง
    char topic_temp[50], topic_hum[50], topic_soil[50];
    sprintf(topic_temp, "helteclora32/NodeV3/%s/temperature", device_id);
    sprintf(topic_hum, "helteclora32/NodeV3/%s/humidity", device_id);
    sprintf(topic_soil, "helteclora32/NodeV3/%s/soil_moisture", device_id);

    // ✅ ส่งค่าข้อมูลไปยัง MQTT Broker ตามหัวข้อที่กำหนด
    client.publish(topic_temp, tempStr.c_str());
    client.publish(topic_hum, humStr.c_str());
    client.publish(topic_soil, soilStr.c_str());

    Serial.printf("MQTT Published: %s -> %s\n", topic_temp, tempStr.c_str());
    Serial.printf("MQTT Published: %s -> %s\n", topic_hum, humStr.c_str());
    Serial.printf("MQTT Published: %s -> %s\n", topic_soil, soilStr.c_str());
  }

  // ✅ เปิดรับข้อมูลใหม่อีกครั้ง เพื่อรอแพ็กเก็ตถัดไป
  Radio.Rx(0);
}
