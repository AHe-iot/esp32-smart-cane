/*
 * ============================================================
 *  H4 · 步骤 9：WiFi + MQTT 联网验证
 * ------------------------------------------------------------
 *  目标：让 ESP32 连上手机热点，再连上公共 MQTT 服务器，
 *       每隔 5 秒发一条消息，用 MQTTX 订阅确认能收到。
 *
 *  本测试不接任何传感器，只发一个带计数的心跳消息。
 *
 *  服务器：broker.emqx.io，TCP 端口 1883
 *  主题  ：cane/test
 *
 *  通过标准：
 *    1) 串口打印 WiFi 已连接并给出 IP；
 *    2) 打印 MQTT 已连接；
 *    3) MQTTX 订阅 cane/test 能持续收到 "ESP32 online #0/1/2..."
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "HONOR 80 Pro";
const char* WIFI_PWD  = "hhhhhhhh";

const char* MQTT_HOST = "broker.emqx.io";
const int   MQTT_PORT = 1883;
const char* TOPIC     = "cane/test";

WiFiClient   espClient;
PubSubClient mqtt(espClient);

int msgCount = 0;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== WiFi + MQTT 联网测试 ===");

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  // 掉线就重连
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  // 每 5 秒发一条心跳
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    String payload = "ESP32 online #";
    payload += msgCount;
    msgCount++;

    boolean ok = mqtt.publish(TOPIC, payload.c_str());
    if (ok) {
      Serial.print("已发送：");
      Serial.println(payload);
    } else {
      Serial.println("发送失败，下轮重试");
    }
  }
}

// 连接 WiFi
void connectWiFi() {
  Serial.print("正在连接 WiFi：");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi 连接成功");
  Serial.print("本机 IP：");
  Serial.println(WiFi.localIP());
  Serial.print("信号强度 RSSI：");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

// 连接 MQTT 服务器
void connectMQTT() {
  // 用芯片 MAC 拼一个唯一 Client ID，避免和别人重名被踢
  String clientId = "ahe-cane-";
  clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

  while (!mqtt.connected()) {
    Serial.print("正在连接 MQTT... ");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("成功");
    } else {
      Serial.print("失败，状态码 ");
      Serial.print(mqtt.state());
      Serial.println("，2 秒后重试");
      delay(2000);
    }
  }
}
