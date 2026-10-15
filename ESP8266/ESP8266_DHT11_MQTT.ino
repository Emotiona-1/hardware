/*
 * ESP8266 + DHT11  MQTT 温湿度上报 + 串口接收
 *
 *  硬件接线:
 *   DHT11 VCC → 3.3V
 *   DHT11 GND → GND
 *   DHT11 DAT → D4 (GPIO2)
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT11.h>

// ──────────────── 本地配置 ────────────────
// WiFi 账号密码、MQTT 服务器地址与主题都放在 secrets.h（已加入 .gitignore）。
// 首次使用请把 secrets.h.example 复制为 secrets.h 并填入自己的值。
// secrets.h 中定义：WIFI_SSID / WIFI_PASSWORD / MQTT_SERVER / MQTT_PORT /
//                   TOPIC_PUB（上报温湿度）/ TOPIC_SUB（订阅下行指令）
#include "secrets.h"

// ──────────────── MQTT ────────────────
const char* MQTT_CLIENT_ID = "ESP8266_DHT11";

// ──────────────── 其他 ────────────────
const unsigned long INTERVAL = 1000;   // 1秒上报一次
const int DHT_PIN = D4;

// ──────────────── 对象 ────────────────
DHT11        dht11(DHT_PIN);
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastTime = 0;

// ================================================================
//                            初始化
// ================================================================
void setup() {
  Serial.begin(115200);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("[WiFi] 连接中"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(F("[WiFi] 已连接, IP: "));
  Serial.println(WiFi.localIP());

  // MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectMQTT();
}

// ================================================================
//                         MQTT 连接
// ================================================================
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print(F("[MQTT] 连接中..."));
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println(F(" 成功"));
      mqttClient.subscribe(TOPIC_SUB);
      Serial.print(F("[MQTT] 订阅: "));
      Serial.println(TOPIC_SUB);
    } else {
      Serial.print(F(" 失败, 5秒后重试"));
      delay(5000);
    }
  }
}

// ================================================================
//                        主循环
// ================================================================
void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  if (millis() - lastTime >= INTERVAL) {
    lastTime = millis();
    publishData();
  }
}

// ================================================================
//                     上报温湿度
// ================================================================
void publishData() {
  int t = 0, h = 0;

  if (dht11.readTemperatureHumidity(t, h) == 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"temp\":%d,\"humi\":%d}", t, h);
    mqttClient.publish(TOPIC_PUB, buf);
  }
}

// ================================================================
//                  MQTT 接收 → 串口输出
// ================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[256];
  unsigned int len = (length < sizeof(msg) - 1) ? length : (sizeof(msg) - 1);
  memcpy(msg, payload, len);
  msg[len] = '\0';

  Serial.println(msg);
}
