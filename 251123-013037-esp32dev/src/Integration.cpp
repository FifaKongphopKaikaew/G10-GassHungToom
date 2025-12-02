#include <Arduino.h>
#include "HX711.h"
#include <Keypad.h>
#include <LiquidCrystal.h>

// ฟังก์ชันภายนอกสำหรับส่งข้อมูล
extern void sendToGoogleFromMain(String dataPayload);

// ===== LCD Pin Configuration (ตามรูปที่คุณส่งมา) =====
const int RS = 2;
const int EN = 4;
const int D4 = 18;
const int D5 = 19;
const int D6 = 21;
const int D7 = 15;

LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// ===== Data Structure =====
struct TankData { 
  String brand; 
  float weight; 
  String timestamp;
};

TankData session_data[50]; // เก็บได้สูงสุด 50 ถัง
int tank_count = 0;
int vehicle_id = 1;
float price_per_unit = 10.0; 

// ===== HX711 Configuration =====
const int LOADCELL_DOUT_PIN = 23;
const int LOADCELL_SCK_PIN  = 22;
HX711 scale;
float calibration_factor = -543.948181; // ⭐ ค่าเริ่มต้น (แก้ได้ผ่าน Serial 'cal')

// ===== Keypad Configuration =====
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {26, 25, 33, 32};
byte colPins[COLS] = {13, 12, 14, 27};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== Motor Pin =====
const int MOTOR_PIN = 5;

// ===== Data Storage Structure =====
struct WeightData {
  float before;
  float after;
  float difference;
  String range;
};

WeightData data_PTT[50];
WeightData data_Other[50];
int count_PTT = 0;
int count_Other = 0;
int total_count = 0;

// ===== ตัวแปรสำหรับ Smooth Filter (เพิ่มใหม่) =====
float previous_weight = 0;
float weight_buffer[20]; // Buffer 20 ค่า
int buffer_index = 0;
float estimated_rate = 0; // อัตราการไหล (g/s)

// ===== Function Prototypes =====
bool waitForReady();
void fillWater();
void displayResults();
float readWeightSmooth(); // ⭐ พระเอกของเรา
void controlMotor(bool state);
void updateLCD(String line1, String line2);
void checkSerialCommands(); // ⭐ เช็คคำสั่งจากคอม

void integration_setup() {
  Serial.begin(115200);
  delay(100);

  // Setup LCD
  lcd.begin(16, 2);
  updateLCD("Auto Water Sys.", "Initializing...");
  
  // ตั้งค่า Load Cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  // ⭐ ตั้งค่า HX711 ให้อัปเดตเร็วขึ้น (ลด Gain ลงจะอ่านไวขึ้น)
  scale.set_gain(64); 
  scale.set_scale(calibration_factor);
  scale.tare();
  
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  Serial.println("========================================");
  Serial.println("   ระบบเติมน้ำอัตโนมัติ (High-Speed)");
  Serial.println("   PTT & Other Brand");
  Serial.println("========================================");
  delay(1000);
}

void integration_loop() {
  while (true) {
    // 1. รอเริ่มทำงาน (และเช็คคำสั่ง Serial ไปด้วย)
    if (waitForReady()) {
      fillWater();
    }

    // 2. เมนูหลังเติมเสร็จ
    Serial.println("\n========================================");
    Serial.println("  กด 'C' -> ถังต่อไป");
    Serial.println("  กด 'D' -> ส่งข้อมูล Google Sheet");
    Serial.println("========================================");
    
    updateLCD("Done! Select:", "C:Next  D:Send");
    delay(500);

    char choice = NO_KEY;
    while (choice != 'C' && choice != 'D') {
      choice = keypad.getKey();
      checkSerialCommands(); // เช็คคำสั่งเผื่ออยาก Tare
      delay(50);
    }

    if (choice == 'C') {
      Serial.println("\n>> ถังต่อไป...\n");
      updateLCD("Next Tank...", "Please Wait");
      delay(1000);
      continue;
    }

    if (choice == 'D') {
      Serial.println(">> กำลังส่งข้อมูล...");
      updateLCD("Sending Data...", "Please Wait");
      delay(500);

      float sumPTT = 0;
      float sumOther = 0;

      // --- STEP 1: ส่งข้อมูลรายถัง ---
      for (int i = 0; i < tank_count; i++) {
        if (session_data[i].brand == "PTT") sumPTT += session_data[i].weight;
        else sumOther += session_data[i].weight;

        // ⭐ รูปแบบ payload ที่ถูกต้อง (มี ;XX;)
        String payload = "ROW;" + String(vehicle_id) + ";" + String(i + 1) + ";XX;" + session_data[i].brand + ";" + String(session_data[i].weight, 2);

        Serial.print("Sending Tank "); Serial.println(i + 1);
        
        lcd.setCursor(0, 1);
        lcd.print("Tank " + String(i + 1) + "/" + String(tank_count) + "   ");

        sendToGoogleFromMain(payload);
        delay(1500); 
      }

      // --- STEP 2: ส่งข้อมูลสรุป ---
      float pricePTT = sumPTT * price_per_unit;
      float priceOther = sumOther * price_per_unit;
      float netPrice = pricePTT + priceOther;

      String sumPayload = "SUM;" + String(sumPTT, 2) + ";" + String(pricePTT, 2) + ";" + String(sumOther, 2) + ";" + String(priceOther, 2) + ";" + String(netPrice, 2);

      Serial.println("Sending Summary...");
      updateLCD("Sending Summary", "Finalizing...");
      sendToGoogleFromMain(sumPayload);

      Serial.println(">> เสร็จสิ้น!");
      updateLCD("Upload Complete", "Ready for New Car");
      delay(2000);

      // รีเซ็ตระบบ
      vehicle_id++;   
      tank_count = 0; 
      total_count = 0;   
      count_PTT = 0;     
      count_Other = 0;   

      // ล้างข้อมูล
      for (int i = 0; i < 50; i++) {
        session_data[i].brand = ""; session_data[i].weight = 0.0; session_data[i].timestamp = "";
        data_PTT[i] = {0,0,0,""}; data_Other[i] = {0,0,0,""};
      }
      
      Serial.println("\n=== เริ่มต้นรถคันใหม่ ===");
    }
  }
}

// =======================================================
//  Helper Functions
// =======================================================

void updateLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

bool waitForReady() {
  Serial.println("\nกด 'A' เพื่อเริ่ม (หรือพิมพ์ 't' เพื่อ Tare)");
  updateLCD(" Ready... ", "Press A to Start");
  
  while (true) {
    checkSerialCommands(); // ⭐ เช็คคำสั่ง Serial (Tare/Calibrate) ได้ตลอดเวลา
    
    char key = keypad.getKey();
    if (key == 'A') {
      Serial.println(">> เริ่มทำงาน!");
      return true;
    }
    delay(50);
  }
}

void fillWater() {
  Serial.println("\n>>> วัดน้ำหนักเริ่มต้น...");
  updateLCD("Measuring...", "Please Wait");
  
  // ล้าง buffer ก่อนอ่านค่าจริงจัง
  for (int i=0; i<20; i++) weight_buffer[i] = 0;
  buffer_index = 0;
  previous_weight = 0;
  
  delay(500); // รอให้นิ่ง
  float initial_weight = readWeightSmooth();
  
  Serial.print("Initial W: "); Serial.println(initial_weight);
  lcd.setCursor(0, 1);
  lcd.print("W: " + String(initial_weight, 1) + " g      ");

  if (initial_weight >= -20 && initial_weight <= 20) {
    Serial.println("!! ยังไม่วางถัง !!");
    updateLCD("Error: No Tank", "Put Tank First");
    delay(2000);
    return;
  }

  float target_weight = 0;
  String weight_range = "";

  // ⭐ Logic ตัดน้ำ (Offset เผื่อน้ำไหลเกิน)
  if (initial_weight >= 190 && initial_weight <= 210) {
    target_weight = 370; // ตัดที่ 370 เพื่อให้จบที่ 390
    weight_range = "190-210g";
    Serial.println("Target: 390g (Cut@370)");
  } 
  else if (initial_weight >= 310 && initial_weight <= 340) {
    target_weight = 535; // ตัดที่ 535 เพื่อให้จบที่ 560
    weight_range = "310-340g";
    Serial.println("Target: 560g (Cut@535)");
  } 
  else {
    Serial.println("!! น้ำหนักผิดช่วง !!");
    updateLCD("Weight Invalid", "Check Range!");
    delay(2000);
    return;
  }

  Serial.println("\n=== เริ่มเติมน้ำ ===");
  
  // Pre-fill Buffer เพื่อความแม่น
  for(int i=0; i<10; i++) { readWeightSmooth(); delay(10); }

  controlMotor(true);
  
  float current_weight = initial_weight;
  unsigned long last_lcd_update = 0;

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Filling...");

  // ⭐ ลูปเติมน้ำแบบ High-Speed
  while (current_weight < target_weight) {
    current_weight = readWeightSmooth();
    
    // อัปเดตจอและ Serial ทุก 200ms (ไม่ทำทุกรอบเดี๋ยวช้า)
    if (millis() - last_lcd_update > 200) {
        Serial.print(current_weight, 1); Serial.print("/"); Serial.println(target_weight, 0);
        
        lcd.setCursor(0, 1);
        lcd.print(String(current_weight, 0) + "/" + String(target_weight, 0) + " g   ");
        
        last_lcd_update = millis();
    }
    
    delay(10); // Loop เร็วขึ้นเพื่อเช็คเงื่อนไขตัดน้ำ
  }

  controlMotor(false);
  Serial.println("=== ตัดน้ำแล้ว รอให้นิ่ง... ===");
  updateLCD("Filling Done!", "Stabilizing...");
  
  delay(2000); // รอให้น้ำหยุดกระเพื่อม
  
  // ล้าง buffer และอ่านค่าสุดท้ายแบบละเอียด (Median)
  for (int i=0; i<20; i++) weight_buffer[i] = 0;
  buffer_index = 0;
  
  // อ่าน 10 ครั้ง หาค่าเฉลี่ยแบบตัดหัวท้าย
  float final_readings[10];
  for(int i=0; i<10; i++) { final_readings[i] = readWeightSmooth(); delay(50); }
  
  // Sort
  for(int i=0; i<9; i++) {
    for(int j=i+1; j<10; j++) {
       if(final_readings[i] > final_readings[j]) {
          float temp = final_readings[i]; final_readings[i] = final_readings[j]; final_readings[j] = temp;
       }
    }
  }
  // เอาค่ากลางๆ
  float final_sum = 0;
  for(int i=2; i<8; i++) final_sum += final_readings[i];
  float final_weight = final_sum / 6.0;
  
  float difference = final_weight - initial_weight;

  Serial.print("Final: "); Serial.println(final_weight);
  Serial.print("Added: "); Serial.println(difference);

  updateLCD("Added: " + String(difference, 1) + "g", "Select Brand?");
  
  Serial.println("\n--- เลือกยี่ห้อ ---");
  Serial.println("* = PTT, # = Other");
  
  // บรรทัดนี้แสดงเมนูบนจอ
  lcd.setCursor(0, 0); lcd.print("*:PTT  #:Other  ");
  lcd.setCursor(0, 1); lcd.print("Amt: " + String(difference, 1) + "g    ");

  char confirm_key = NO_KEY;
  while (confirm_key != '*' && confirm_key != '#') {
    confirm_key = keypad.getKey();
    delay(50);
  }

  WeightData current_data;
  current_data.before = initial_weight;
  current_data.after = final_weight;
  current_data.difference = difference;
  current_data.range = weight_range;

  String brand = "";
  if (confirm_key == '*') {
    data_PTT[count_PTT] = current_data; count_PTT++;
    brand = "PTT";
    updateLCD("Saved: PTT", "Amt: " + String(difference, 1));
  } else {
    data_Other[count_Other] = current_data; count_Other++;
    brand = "Other";
    updateLCD("Saved: Other", "Amt: " + String(difference, 1));
  }
  
  total_count++;

  if (tank_count < 50) {
     session_data[tank_count].brand = brand;
     session_data[tank_count].weight = difference; 
     tank_count++;
  }
  Serial.println("Saved!");
  delay(1000);
}

void controlMotor(bool state) {
  if (state) {
    digitalWrite(MOTOR_PIN, HIGH);
  } else {
    digitalWrite(MOTOR_PIN, LOW);
  }
}

// ⭐⭐⭐ ฟังก์ชันอ่านน้ำหนักแบบ Smooth ขั้นเทพ (จากไฟล์ message.txt) ⭐⭐⭐
float readWeightSmooth() {
  if (!scale.is_ready()) return previous_weight;
  
  static unsigned long last_time = 0;
  unsigned long current_time = millis();
  float time_delta = (current_time - last_time) / 1000.0;
  if (last_time == 0) time_delta = 0.1;
  last_time = current_time;
  
  // อ่าน 5 ครั้ง (เร็ว)
  float readings[5];
  for (int i = 0; i < 5; i++) {
    readings[i] = scale.get_units(1);
    delay(4); // ลด delay
  }
  
  // Sort
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (readings[i] > readings[j]) {
        float temp = readings[i]; readings[i] = readings[j]; readings[j] = temp;
      }
    }
  }
  
  // Median (ค่ากลาง)
  float raw_median = (readings[1] + readings[2] + readings[3]) / 3.0;
  
  // ใส่ Buffer
  weight_buffer[buffer_index] = raw_median;
  buffer_index = (buffer_index + 1) % 20;
  
  float buffer_avg = 0;
  int valid_count = 0;
  for (int i = 0; i < 20; i++) {
    if (weight_buffer[i] > 0) {
      buffer_avg += weight_buffer[i];
      valid_count++;
    }
  }
  if (valid_count > 0) buffer_avg /= valid_count;
  else buffer_avg = raw_median;
  
  // คำนวณ Rate (g/s)
  if (previous_weight > 0 && time_delta > 0 && time_delta < 0.5) {
    float instant_rate = (raw_median - previous_weight) / time_delta;
    if (instant_rate > -100 && instant_rate < 300) {
      estimated_rate = estimated_rate * 0.5 + instant_rate * 0.5;
    }
  }
  
  // คำนวณค่าทำนาย (Predictive)
  float latency_compensation = estimated_rate * 0.15; 
  float predicted = previous_weight + (estimated_rate * time_delta) + latency_compensation;
  
  float result;
  // Adaptive Fusion logic
  if (previous_weight == 0 || valid_count < 5) {
    result = raw_median;
  } else {
    float raw_weight = 0.3;
    float buffer_weight = 0.4;
    float pred_weight = 0.3;
    
    if (abs(estimated_rate) > 30) { // ถ้าน้ำไหลเร็ว เชื่อค่าทำนายมากขึ้น
      raw_weight = 0.2; buffer_weight = 0.3; pred_weight = 0.5;
    }
    
    float fusion = raw_median * raw_weight + buffer_avg * buffer_weight + predicted * pred_weight;
    
    // ป้องกันค่ากระโดด
    if (fusion < previous_weight - 20) fusion = previous_weight - 15;
    if (fusion > previous_weight + 60) fusion = previous_weight + 40;
    
    result = previous_weight * 0.3 + fusion * 0.7;
  }
  
  previous_weight = result;
  return result;
}

// ⭐ ฟังก์ชันเช็คคำสั่ง Serial (Tare/Calibrate)
void checkSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); command.toLowerCase();
    
    if (command == "t" || command == "tare") {
       Serial.println(">> Taring...");
       scale.tare();
       // Reset filter
       previous_weight = 0; estimated_rate = 0;
       for(int i=0; i<20; i++) weight_buffer[i] = 0;
       Serial.println(">> Done!");
    }
    else if (command == "cal") {
       Serial.println(">> Enter Calibration Mode");
       Serial.println("1. Remove Weight -> Enter");
       while(Serial.available()==0) delay(10); Serial.readStringUntil('\n');
       scale.tare();
       Serial.println("2. Put Weight -> Type Weight (e.g. 200) -> Enter");
       while(Serial.available()==0) delay(10);
       float known = Serial.parseFloat();
       long raw = scale.get_value(20);
       float new_cal = (float)raw / known;
       Serial.print("New Factor: "); Serial.println(new_cal);
       calibration_factor = new_cal;
       scale.set_scale(new_cal);
    }
  }
}