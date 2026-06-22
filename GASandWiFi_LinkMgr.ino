// サンプルプログラムだよ
#include <WiFi.h>
#include "GASandWiFi_LinkMgr/GASandWiFi_LinkMgr.h"

#define CONFIG_PIN 4  

ConfigManager configMgr;

unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 10000; 

void setup() {
  Serial.begin(115200);
  
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  delay(100); 

  if (digitalRead(CONFIG_PIN) == LOW) {
      Serial.println("【設定モード検出】");
      configMgr.startAPMode();
  } else {
      Serial.println("【通常ロガーモード起動】");
      configMgr.connectWiFi();
      
      // ★ 起動時にスプシの1行目（ヘッダー）を確定させる
      configMgr.sendHeaderToGAS("日時,デバイス名,起動時間(ms),センサー値1,センサー値2");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = currentMillis;

    // ★ 送りたいデータを全部まとめたCSVを引数で渡して送信！
    String data = configMgr.statusStr + "," + String(millis()) + ",24.5,58.2";
    configMgr.sendDataToGAS(data); 
  }
}
