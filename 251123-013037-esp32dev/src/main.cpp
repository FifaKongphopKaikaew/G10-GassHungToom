#include <Arduino.h>
#include <WiFi.h>
#include "Integration.h" // ต้องมั่นใจว่ามีไฟล์ Integration.h หรือประกาศ Prototype ไว้

// ================== CONFIGURATION ==================
const char *ssid = "test";           // ชื่อ WiFi ของคุณ
const char *password = "pppppppp";   // รหัสผ่าน WiFi

// ใส่ URL ของ Web App ที่ลงท้ายด้วย /exec (ห้ามใช้ /edit)
#define GOOGLE_URL "https://script.google.com/macros/s/AKfycbwJCTiK5v6shzISv6MlgMHarXEZEzwcqpE2x3RygXNQJlvMEmx_4kMNKnus1l8OeaU/exec"

// ประกาศฟังก์ชันจาก google.cpp เพื่อให้เรียกใช้ได้
String post_google(String path, String body);

// ================== CALLBACK FUNCTION ==================
// ฟังก์ชันนี้จะถูกเรียกจาก Integration.cpp เมื่อทำงานเสร็จและได้ข้อมูลครบแล้ว
// แก้ไขฟังก์ชันนี้ใน main.cpp
void sendToGoogleFromMain(String dataPayload) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[Main] กำลังส่งข้อมูล: " + dataPayload);
        
        // เรียกใช้ post_google จากไฟล์ google.cpp
        String response = post_google(GOOGLE_URL, dataPayload);
        
        Serial.println("[Main] Google ตอบกลับ: " + response);
    } else {
        Serial.println("!! Error: WiFi หลุด !!");
    }
}

// ================== SETUP ==================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n--- System Starting ---");

    // 1. เชื่อมต่อ WiFi ให้เสร็จที่นี่ (ก่อนเริ่มระบบอื่น)
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // รอจนกว่าจะต่อติด
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("-----------------------");

    // 2. เรียก setup ของ Integration (เปลี่ยนชื่อมาจาก setup เดิมของ Integration.cpp)
    // เพื่อเริ่มการทำงานของ Load Cell, Motor, Keypad
    integration_setup(); 
}

// ================== LOOP ==================
void loop() {
    // 3. เรียก loop ของ Integration เพื่อให้ระบบทำงานตามปกติ
    integration_loop();
    
    // ปล่อยให้ loop ทำงานอื่นได้นิดหน่อยถ้าจำเป็น (แต่ Integration มักจะมี delay ของมันเอง)
}