#ifndef GAS_AND_WIFI_LINK_MGR_H
#define GAS_AND_WIFI_LINK_MGR_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>

class ConfigManager {
private:
    WebServer server;
    Preferences prefs;
    
    const int MAX_FACTORY_WIFI = 7; 
    const int TEMPORARY_INDEX = 7; // 8番目が臨時（インデックス7）

    // WebUIのHTMLページを生成する関数
    String getHtmlPage() {
        return R"rawliteral(
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MAKExTRAIL LOGGER config</title>
    <style>
        body { font-family: sans-serif; background-color: #f4f6f9; color: #333; margin: 0; padding: 20px; }
        .container { max-width: 500px; margin: 0 auto; background: #fff; padding: 25px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        h2 { text-align: center; color: #007bff; margin-bottom: 20px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        select, input[type="text"] { width: 100%; padding: 10px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; font-size: 16px; }
        .temporary-section { background-color: #fff9e6; padding: 15px; border: 1px dashed #ffc107; border-radius: 4px; margin-top: 15px; display: none; }
        .btn-submit { width: 100%; padding: 12px; background-color: #007bff; border: none; color: white; font-size: 16px; font-weight: bold; border-radius: 4px; cursor: pointer; margin-top: 20px; }
        .btn-submit:hover { background-color: #0056b3; }
        .note { font-size: 12px; color: #666; margin-top: 5px; }
    </style>
</head>
<body>
<div class="container">
    <h2>MAKExTRAIL LOGGER 設定変更</h2>
    <form action="/save" method="POST">
        <div class="form-group">
            <label for="wifi-select">接続するWi-Fi環境</label>
            <select id="wifi-select" name="wifi_index" onchange="toggleInputs()">
                <option value="0">1. モバイルルータ１</option>
                <option value="1">2. モバイルルータ２</option>
                <option value="2">3. モバイルルータ３</option>
                <option value="3">4. 校内IoT_AP１</option>
                <option value="4">5. 校内IoT_AP２</option>
                <option value="5">6. 校内IoT_AP３</option>
                <option value="6">7. 予備用AP</option>
                <option value="7">8. 臨時アクセスポイント</option>
            </select>
        </div>
        <div id="temp-wifi-fields" class="temporary-section">
            <div class="form-group">
                <label for="temp-ssid">臨時 SSID</label>
                <input type="text" id="temp-ssid" name="temp_ssid" placeholder="例: Buffalo-G-XXXX">
            </div>
            <div class="form-group">
                <label for="temp-pass">臨時 パスワード</label>
                <input type="text" id="temp-pass" name="temp_pass" placeholder="例: password123">
                <span class="note">※臨時接続は入力内容がそのまま表示されます。</span>
            </div>
        </div>
        <hr style="border: 0; border-top: 1px solid #eee; margin: 25px 0;">
        <div class="form-group">
            <label for="gas-id">GAS デプロイID</label>
            <input type="text" id="gas-id" name="gas_id" placeholder="デプロイした時のアドレスの /s/ 以降の文字列を入力">
        </div>
        <div class="form-group">
            <label for="device-status">デバイス設置場所 / ステータス</label>
            <input type="text" id="device-status" name="device_status" placeholder="例: TEST_NODE_01">
        </div>
        <button type="submit" class="btn-submit">設定を保存して再起動</button>
    </form>
</div>
<script>
    function toggleInputs() {
        var select = document.getElementById("wifi-select");
        var tempFields = document.getElementById("temp-wifi-fields");
        if (select.value === "7") {
            tempFields.style.display = "block";
        } else {
            tempFields.style.display = "none";
        }
    }
</script>
</body>
</html>
)rawliteral";
    }

public:
    ConfigManager() : server(80) {}

    // メイン側から参照できる公開メンバ変数
    String targetSSID = "";
    String targetPass = "";
    String gasId = "";
    String statusStr = "";

    // APモード（設定画面）の立ち上げ
    void startAPMode() {
        WiFi.softAP("ESP32-Logger-Config");
        Serial.println("APモード起動。192.168.4.1 にアクセスしてください。");

        server.on("/", HTTP_GET, [this]() {
            server.send(200, "text/html", getHtmlPage());
        });

        server.on("/save", HTTP_POST, [this]() {
            int wifiIndex   = server.arg("wifi_index").toInt();
            String tempSSID = server.arg("temp_ssid");
            String tempPass = server.arg("temp_pass");
            String gas      = server.arg("gas_id");
            String stat     = server.arg("device_status");

            prefs.begin("user-config", false);
            prefs.putInt("wifi_idx", wifiIndex);
            prefs.putString("tmp_ssid", tempSSID);
            prefs.putString("tmp_pass", tempPass);
            prefs.putString("gas_id", gas);
            prefs.putString("status", stat);
            prefs.end();

            server.send(200, "text/html; charset=utf-8", "<h1>設定を保存しました。再起動します...</h1>");
            delay(1000);
            ESP.restart();
        });

        server.begin();
        
        while(true) {
            server.handleClient();
            delay(1);
        }
    }

    // 不揮発メモリからWi-Fi情報を読み出して自動接続
    void connectWiFi() {
        prefs.begin("user-config", true);
        int wifiIndex   = prefs.getInt("wifi_idx", 0);
        gasId           = prefs.getString("gas_id", "");
        statusStr       = prefs.getString("status", "DEFAULT_NODE");
        String tmpSSID  = prefs.getString("tmp_ssid", "");
        String tmpPass  = prefs.getString("tmp_pass", "");
        prefs.end();

        if (wifiIndex == TEMPORARY_INDEX) {
            targetSSID = tmpSSID;
            targetPass = tmpPass;
            Serial.println("→ 8番(臨時Wi-Fi)を使用。");
        } else {
            prefs.begin("factory-config", true);
            targetSSID = prefs.getString(("ssid_" + String(wifiIndex)).c_str(), "");
            targetPass = prefs.getString(("pass_" + String(wifiIndex)).c_str(), "");
            prefs.end();
            Serial.printf("→ %d番(既定Wi-Fi)を使用。\n", wifiIndex + 1);
        }

        Serial.printf("接続先SSID: %s\n", targetSSID.c_str());
        WiFi.begin(targetSSID.c_str(), targetPass.c_str());

        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            timeout++;
            if(timeout > 30) {
                Serial.println("\nWi-Fi接続失敗。PINをLOWにして再起動し、設定を確認してください。");
                return;
            }
        }
        Serial.println("\nWi-Fi接続成功！");
    }

    // 1行目にヘッダータイトルを強制上書きで書き込むメソッド
    void sendHeaderToGAS(String csvHeaderTitle) {
        if (WiFi.status() != WL_CONNECTED) return;

        String url = "https://script.google.com/macros/s/" + gasId + "/exec";
        HTTPClient http;
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS); // 302止めで高速化
        http.begin(url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.addHeader("X-Type", "header"); // GAS側でヘッダーと判別するためのカスタムヘッダー
        
        int httpCode = http.POST(csvHeaderTitle);
        http.end();
    }

    // 通常のCSVデータをポスト送信するメソッド
    void sendDataToGAS(String csvPayload) {
        if (WiFi.status() != WL_CONNECTED) return;

        String url = "https://script.google.com/macros/s/" + gasId + "/exec";
        HTTPClient http;
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS); // 302止めで高速化
        http.begin(url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        int httpCode = http.POST(csvPayload);
        http.end();
    }
};

#endif // GAS_AND_WIFI_LINK_MGR_H
