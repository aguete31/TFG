#include "mqtt_handler.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// === CONFIGURA ESTAS CONSTANTES ===
const char* WIFI_SSID     = "22A8";
const char* WIFI_PASSWORD = "7zvvf53j5tjuuv";
const char* MQTT_BROKER   = "192.168.1.139";
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_USER     = "loragw";
const char* MQTT_PASS     = "TFGfranLoRa2019";
// ==================================

WiFiClient espClient;
PubSubClient client(espClient);

String clientId;
String baseTopic;

void connectWiFi() {
  Serial.printf("[MQTT] Conectando a WiFi '%s' ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (millis() - start > 30000) {
      Serial.println("\n[MQTT] Timeout WiFi. Reintentando...");
      start = millis();
    }
  }
  Serial.printf("\n[MQTT] WiFi conectado. IP: %s\n", WiFi.localIP().toString().c_str());
}

void connectMQTT() {
  if (client.connected()) return;

  if (clientId.length() == 0) {
    uint64_t chipId = ESP.getEfuseMac();
    char macstr[13];
    sprintf(macstr, "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
    clientId = String("loragw-") + String(macstr);
    baseTopic = String("loragateway/") + clientId;
  }

  client.setServer(MQTT_BROKER, MQTT_PORT);
  // No usamos callback en este ejemplo, pero puedes añadirlo si necesitas recibir comandos

  String availTopic = baseTopic + "/availability";
  Serial.printf("[MQTT] Conectando como %s ...\n", clientId.c_str());
  bool ok = client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS,
                           availTopic.c_str(), 1, true, "offline");
  if (ok) {
    Serial.println("[MQTT] Conectado al broker");
    client.publish(availTopic.c_str(), "online", true);
  } else {
    Serial.printf("[MQTT] Fallo al conectar, rc=%d. Reintentando...\n", client.state());
  }
}

void mqtt_setup() {
  connectWiFi();
}

void mqtt_loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();
}

void publish_telemetry(const String& device_id, const String& payload, int rssi, float snr, uint32_t seq) {
  if (!client.connected()) return;

  // Parsear el payload JSON del nodo
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("[MQTT] Error al parsear payload JSON del nodo");
    return;
  }

  // Añadir metadatos
  doc["rssi"] = rssi;
  doc["snr"] = snr;
  doc["seq"] = seq;
  doc["ts"] = millis() / 1000;

  // Serializar y publicar
  char buffer[512];
  size_t n = serializeJson(doc, buffer);
  String topic = String("loradev/") + device_id + "/telemetry";
  bool ok = client.publish(topic.c_str(), buffer, n);

  Serial.printf("[MQTT] Publish %s -> %s\n", ok ? "OK" : "FAIL", topic.c_str());
}