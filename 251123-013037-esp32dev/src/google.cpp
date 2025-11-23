#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> // ต้องเพิ่ม Library นี้

String get_google(String path);
String post_google(String path, String body);

String post_google(String path, String body)
{
  if (path == "") return "";

  // 1. สร้าง Client แบบ Secure
  WiFiClientSecure client;
  // 2. สั่งให้ไม่ต้องเช็ค Certificate (แก้ Error -76)
  client.setInsecure(); 

  HTTPClient http;

  String payload = "";

  Serial.print("[HTTP] begin...\n");
  Serial.println(body);

  // 3. เริ่มเชื่อมต่อโดยใช้ client ที่เราสร้าง
  // หมายเหตุ: ใช้ client เป็นตัวแปรแรก
  if (http.begin(client, path)) {  
      
      http.setTimeout(10000);
      
      // การตาม Redirect สำหรับ Google Script
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

      Serial.printf("[HTTP] POST \n");
      
      // เพิ่ม Header ให้ Google รู้ว่าเป็นข้อความ
      http.addHeader("Content-Type", "text/plain");

      int httpCode = http.POST(body);

      // httpCode will be negative on error
      if (httpCode > 0)
      {
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND)
        {
          payload = http.getString();
          Serial.println(payload);
        }
      }
      else
      {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        payload = "";
      }

      http.end();
  } else {
      Serial.printf("[HTTP] Unable to connect\n");
  }

  return payload;
}

String get_google(String path)
{
  // ฟังก์ชันนี้อาจไม่ได้ใช้แล้วถ้าใช้ post_google ตัวใหม่
  // แต่เขียนกันไว้ให้ทำงานได้เหมือนกันครับ
  if (path == "") return "";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String payload = "";

  if (http.begin(client, path)) {
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.setTimeout(5000);

      int httpCode = http.GET();

      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      if (httpCode > 0)
      {
        payload = http.getString();
      }
      http.end();
  }

  return payload;
}