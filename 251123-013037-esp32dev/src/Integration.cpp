#include <Arduino.h>
#include "HX711.h"
#include <Keypad.h>

extern void sendToGoogleFromMain(String dataPayload);

// ===== Data Structure for Storing Input =====
struct TankData { 
  String brand; 
  float weight; 
  String timestamp;
};
TankData session_data[5];
int tank_count = 0;
int vehicle_id = 1;
float price_per_unit = 10.0; 

// ===== HX711 Configuration =====
const int LOADCELL_DOUT_PIN = 23;
const int LOADCELL_SCK_PIN  = 22;
HX711 scale;
float calibration_factor = -1081.554199;

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

// ===== Motor Pin (ปรับตามฮาร์ดแวร์จริง) =====
const int MOTOR_PIN = 5;  // เปลี่ยนเป็น Pin ที่ต่อกับมอเตอร์

// ===== Data Storage Structure =====
struct WeightData {
  float before;      // น้ำหนักก่อนเติม
  float after;       // น้ำหนักหลังเติม
  float difference;  // ผลต่าง
  String range;      // ช่วงน้ำหนัก
};

WeightData data_PTT[5];      // เก็บข้อมูลยี่ห้อ PTT
WeightData data_Other[5];    // เก็บข้อมูลยี่ห้อ Other
int count_PTT = 0;
int count_Other = 0;
int total_count = 0;

// ===== Function Prototypes =====
bool waitForReady();
void fillWater();
void displayResults();
float readWeight();
void controlMotor(bool state);

void integration_setup() {
  Serial.begin(115200);
  delay(100);

  // ตั้งค่า Load Cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();

  // ตั้งค่า Motor
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  Serial.println("========================================");
  Serial.println("   ระบบเติมน้ำอัตโนมัติ");
  Serial.println("   PTT & Other Brand");
  Serial.println("========================================");
  Serial.println();
}

void integration_loop() {
  // วนลูปจนครบ 5 ครั้ง
  while (total_count < 5) {
    // รอให้กดปุ่ม A เพื่อเริ่มทำงาน
    if (waitForReady()) {
      fillWater();
    }
  }

  Serial.println("\n----------------------------------------");
  Serial.print("  ครบ 5 ถังแล้ว (คันที่ "); Serial.print(vehicle_id); Serial.println(")");
  Serial.println("  กด 'D' เพื่อส่งข้อมูลขึ้น Google Sheet");
  Serial.println("----------------------------------------");
  // แสดงผลลัพธ์ทั้งหมด
  while (true) {
    char key = keypad.getKey();
    if (key == 'D') {
      Serial.println(">> กำลังส่งข้อมูล...");

      float sumPTT = 0;
      float sumOther = 0;

      // --- STEP 1: ส่งข้อมูลทีละแถว (5 ครั้ง) ---
      for (int i = 0; i < 5; i++) {
        // คำนวณยอดรวมเตรียมไว้
        if (session_data[i].brand == "PTT") sumPTT += session_data[i].weight;
        else sumOther += session_data[i].weight;

        // รูปแบบส่ง: ROW;คันที่;ถังที่;เวลา;ยี่ห้อ;น้ำหนัก
        // ตัวอย่าง: ROW;1;1;XX;PTT;50.00
        String payload = "ROW;" + String(vehicle_id) + ";" + String(i + 1) + ";XX;" + session_data[i].brand + ";" + String(session_data[i].weight, 2);(session_data[i].weight, 2);
        
        Serial.print("Sending Tank "); Serial.println(i + 1);
        sendToGoogleFromMain(payload);
        delay(1500); // รอ 1.5 วิ ให้ Google บันทึกทัน
      }

      // --- STEP 2: ส่งข้อมูลสรุป (1 ครั้ง) ---
      float pricePTT = sumPTT * price_per_unit;
      float priceOther = sumOther * price_per_unit;
      float netPrice = pricePTT + priceOther;

      // รูปแบบส่ง: SUM;SumPTT;PricePTT;SumOther;PriceOther;NetPrice
      String sumPayload = "SUM;" + String(sumPTT, 2) + ";" + String(pricePTT, 2) + ";" + String(sumOther, 2) + ";" + String(priceOther, 2) + ";" + String(netPrice, 2);(priceOther, 2) + ";" + String(netPrice, 2);
      
      Serial.println("Sending Summary...");
      sendToGoogleFromMain(sumPayload);

      Serial.println(">> เสร็จสิ้น! พร้อมสำหรับคันต่อไป");
      
      // รีเซ็ตค่าสำหรับคันถัดไป
      vehicle_id++;   // ขึ้นคันใหม่
      tank_count = 0; // เริ่มนับถัง 1 ใหม่
      total_count = 0;   // ⭐ เพิ่มบรรทัดนี้ - รีเซ็ตตัวนับให้เริ่มใหม่
      count_PTT = 0;     // รีเซ็ตนับ PTT
      count_Other = 0;   // รีเซ็ตนับ Other

      for (int i = 0; i < 5; i++) {
          session_data[i].brand = "";
          session_data[i].weight = 0.0;
          session_data[i].timestamp = "";  // ล้างเวลาด้วย
          data_PTT[i].before = 0;
          data_PTT[i].after = 0;
          data_PTT[i].difference = 0;
          data_PTT[i].range = "";
          data_Other[i].before = 0;
          data_Other[i].after = 0;
          data_Other[i].difference = 0;
          data_Other[i].range = "";
      }
      Serial.println("\n========================================");
      Serial.print("   เริ่มต้นรถคันที่ "); Serial.println(vehicle_id);
      Serial.println("========================================\n");

      break;          // ออกจาก while เพื่อกลับไปเริ่มชั่งใหม่
    }
    delay(50);
  }
}

bool waitForReady() {
  Serial.println("\nกด 'A' เพื่อเริ่มทำงาน");
  
  while (true) {
    char key = keypad.getKey();
    
    if (key == 'A') {
      Serial.println(">> เริ่มทำงาน!");
      return true;
    }
    
    delay(50);
  }
}

void fillWater() {
  Serial.println("\n>>> กำลังวัดน้ำหนักเริ่มต้น...");
  delay(1000);
  
  float initial_weight = readWeight();
  Serial.print("น้ำหนักที่ตรวจพบ: ");
  Serial.print(initial_weight, 2);
  Serial.println(" g");
  
  // ตรวจสอบว่ามีการวางภาชนะหรือยัง
  if (initial_weight >= -20 && initial_weight <= 20) {
    Serial.println("!! ยังไม่มีน้ำหนักบนเครื่องชั่ง !!");
    Serial.println("กรุณาวางภาชนะบนเครื่องชั่งก่อน");
    Serial.println("(น้ำหนักต้องมากกว่า 20g)");
    Serial.println("\nกลับไปรอกดปุ่ม A อีกครั้ง...");
    return;  // ไม่นับครั้งที่ชั่ง
  }
  
  Serial.print("น้ำหนักก่อนเติม: ");
  Serial.print(initial_weight, 2);
  Serial.println(" g");
  
  float target_weight = 0;
  String weight_range = "";
  
  // ตรวจสอบว่าอยู่ในช่วงไหน
  if (initial_weight >= 190 && initial_weight <= 210) {
    Serial.println(">> ตรวจพบ: ช่วงน้ำหนัก 190-210g");
    Serial.println(">> เป้าหมาย: 390g");
    target_weight = 390;
    weight_range = "190-210g";
  } 
  else if (initial_weight >= 310 && initial_weight <= 340) {
    Serial.println(">> ตรวจพบ: ช่วงน้ำหนัก 310-340g");
    Serial.println(">> เป้าหมาย: 560g");
    target_weight = 560;
    weight_range = "310-340g";
  } 
  else {
    Serial.println("!! น้ำหนักไม่อยู่ในช่วงที่กำหนด !!");
    Serial.println("กรุณาวางภาชนะที่มีน้ำหนักถูกต้อง");
    Serial.println("ช่วงที่รองรับ: 190-210g หรือ 310-340g");
    Serial.println("\nกลับไปรอกดปุ่ม A อีกครั้ง...");
    return;  // ไม่นับครั้งที่ชั่ง
  }
  
  // เริ่มเติมน้ำ
  Serial.println("\n=== เริ่มเติมน้ำ ===");
  controlMotor(true);
  
  float current_weight = initial_weight;
  
  while (current_weight < target_weight) {
    current_weight = readWeight();
    Serial.print("น้ำหนักปัจจุบัน: ");
    Serial.print(current_weight, 2);
    Serial.print(" g / ");
    Serial.print(target_weight, 0);
    Serial.println(" g");
    delay(500);
  }
  
  // หยุดมอเตอร์
  controlMotor(false);
  Serial.println("=== เติมน้ำเสร็จสิ้น ===\n");
  
  float final_weight = readWeight();
  float difference = final_weight - initial_weight;
  
  Serial.print("น้ำหนักหลังเติม: ");
  Serial.print(final_weight, 2);
  Serial.println(" g");
  Serial.print("ผลต่าง (น้ำที่เติม): ");
  Serial.print(difference, 2);
  Serial.println(" g");
  Serial.print("ช่วงน้ำหนัก: ");
  Serial.println(weight_range);
  
  // รอให้กดปุ่มยืนยันยี่ห้อ
  Serial.println("\n--- ระบุยี่ห้อสินค้า ---");
  Serial.println("กด '*' ถ้าเป็นยี่ห้อ PTT");
  Serial.println("กด '#' ถ้าเป็นยี่ห้อ Other");
  
  char confirm_key = NO_KEY;
  while (confirm_key != '*' && confirm_key != '#') {
    confirm_key = keypad.getKey();
    delay(50);
  }
  
  // สร้างข้อมูลที่จะบันทึก
  WeightData current_data;
  current_data.before = initial_weight;
  current_data.after = final_weight;
  current_data.difference = difference;
  current_data.range = weight_range;
  
  String brand = "";
  // บันทึกข้อมูลลง Array ตามยี่ห้อที่กดยืนยัน
  if (confirm_key == '*') {
    data_PTT[count_PTT] = current_data;
    count_PTT++;
    total_count++;  // นับครั้งที่ชั่งเมื่อบันทึกสำเร็จแล้วเท่านั้น
    Serial.println("✓ บันทึกลง Array ยี่ห้อ PTT");
    brand = "PTT";
  } 
  else if (confirm_key == '#') {
    data_Other[count_Other] = current_data;
    count_Other++;
    total_count++;  // นับครั้งที่ชั่งเมื่อบันทึกสำเร็จแล้วเท่านั้น
    Serial.println("✓ บันทึกลง Array ยี่ห้อ Other");
    brand = "Other";
  }
  
  Serial.print("ชั่งไปแล้ว: ");
  Serial.print(total_count);
  Serial.println(" ครั้ง");

  if (tank_count < 5) {
     session_data[tank_count].brand = brand;
     session_data[tank_count].weight = difference; 
     
     Serial.print(">>> บันทึกข้อมูลถังที่ "); 
     Serial.print(tank_count + 1);
     Serial.println(" เก็บไว้รอส่งแล้ว");
     
     tank_count++; // ขยับไปถังต่อไป
  }
  Serial.println("----------------------------------------");
}

float readWeight() {
  if (scale.is_ready()) {
    return scale.get_units(5);  // เฉลี่ย 5 ครั้ง
  }
  return 0;
}

void controlMotor(bool state) {
  if (state) {
    digitalWrite(MOTOR_PIN, HIGH);
    Serial.println("[มอเตอร์: เปิด]");
  } else {
    digitalWrite(MOTOR_PIN, LOW);
    Serial.println("[มอเตอร์: ปิด]");
  }
}

void displayResults() {
  Serial.println("\n========================================");
  Serial.println("        สรุปผลการชั่งทั้งหมด");
  Serial.println("========================================");
  
  // ===== แสดงข้อมูลยี่ห้อ PTT =====
  Serial.println("\n┌─────────────────────────────────────┐");
  Serial.println("│         ยี่ห้อ PTT                  │");
  Serial.println("└─────────────────────────────────────┘");
  
  if (count_PTT > 0) {
    Serial.print("จำนวน: ");
    Serial.print(count_PTT);
    Serial.println(" ชิ้น\n");
    
    float total_diff_PTT = 0;
    
    for (int i = 0; i < count_PTT; i++) {
      Serial.print("  [");
      Serial.print(i + 1);
      Serial.println("]");
      Serial.print("    ช่วง: ");
      Serial.println(data_PTT[i].range);
      Serial.print("    ก่อนเติม: ");
      Serial.print(data_PTT[i].before, 2);
      Serial.println(" g");
      Serial.print("    หลังเติม: ");
      Serial.print(data_PTT[i].after, 2);
      Serial.println(" g");
      Serial.print("    น้ำที่เติม: ");
      Serial.print(data_PTT[i].difference, 2);
      Serial.println(" g");
      Serial.println();
      
      total_diff_PTT += data_PTT[i].difference;
    }
    
    Serial.println("  ───────────────────────────────");
    Serial.print("  ผลรวมน้ำที่เติมทั้งหมด: ");
    Serial.print(total_diff_PTT, 2);
    Serial.println(" g");
  } else {
    Serial.println("ไม่มีข้อมูล");
  }
  
  // ===== แสดงข้อมูลยี่ห้อ Other =====
  Serial.println("\n┌─────────────────────────────────────┐");
  Serial.println("│         ยี่ห้อ Other                │");
  Serial.println("└─────────────────────────────────────┘");
  
  if (count_Other > 0) {
    Serial.print("จำนวน: ");
    Serial.print(count_Other);
    Serial.println(" ชิ้น\n");
    
    float total_diff_Other = 0;
    
    for (int i = 0; i < count_Other; i++) {
      Serial.print("  [");
      Serial.print(i + 1);
      Serial.println("]");
      Serial.print("    ช่วง: ");
      Serial.println(data_Other[i].range);
      Serial.print("    ก่อนเติม: ");
      Serial.print(data_Other[i].before, 2);
      Serial.println(" g");
      Serial.print("    หลังเติม: ");
      Serial.print(data_Other[i].after, 2);
      Serial.println(" g");
      Serial.print("    น้ำที่เติม: ");
      Serial.print(data_Other[i].difference, 2);
      Serial.println(" g");
      Serial.println();
      
      total_diff_Other += data_Other[i].difference;
    }
    
    Serial.println("  ───────────────────────────────");
    Serial.print("  ผลรวมน้ำที่เติมทั้งหมด: ");
    Serial.print(total_diff_Other, 2);
    Serial.println(" g");
  } else {
    Serial.println("ไม่มีข้อมูล");
  }
  
  // ===== สรุปรวมทั้งหมด =====
  Serial.println("\n========================================");
  Serial.print("รวมทั้งหมด: ");
  Serial.print(count_PTT + count_Other);
  Serial.println(" ชิ้น");
  Serial.println("========================================");
}