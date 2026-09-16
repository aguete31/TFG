#include "mqtt_handler.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "mqtt_config.h"
#include "telemetry_store.h"

// =========================================================
// CONFIGURACIÓN
// =========================================================
static constexpr uint32_t MQTT_RECONNECT_DELAY_MS = 5000;
static constexpr uint32_t MQTT_NO_WIFI_DELAY_MS = 1000;
static constexpr uint32_t MQTT_NO_CONFIG_DELAY_MS = 2000;

// =========================================================
// ESTADO
// =========================================================
static WiFiClient g_networkClient;
static PubSubClient g_mqttClient(g_networkClient);

static TaskHandle_t g_mqttTaskHandle = nullptr;

static String g_deviceId;
static MqttConfig g_config;

static volatile MqttHandlerState g_state = MqttHandlerState::STOPPED;

// =========================================================
// UTILIDADES
// =========================================================
static String buildClientId()
{
  return "lora-p2p-gw-" + g_deviceId;
}

static String buildAvailabilityTopic()
{
  return "lora-p2p/gateway/" + g_deviceId + "/availability";
}

// =========================================================
// CONEXIÓN
// =========================================================
static bool connectToBroker()
{
  if (!mqttConfigLoad(g_config))
  {
    g_state = MqttHandlerState::WAITING_CONFIG;
    return false;
  }

  g_state = MqttHandlerState::CONNECTING;
  g_mqttClient.setServer(g_config.host.c_str(), g_config.port);

  String clientId = buildClientId();
  String availabilityTopic = buildAvailabilityTopic();

  Serial.printf("MQTT: conectando a %s:%u clientId=%s\n", g_config.host.c_str(), g_config.port, clientId.c_str());

  bool connected = false;

  if (g_config.user.length() > 0)
  {
    connected = g_mqttClient.connect(clientId.c_str(), g_config.user.c_str(), g_config.password.c_str(), availabilityTopic.c_str(), 0, true, "offline");
  }
  else
  {
    connected = g_mqttClient.connect(clientId.c_str(), availabilityTopic.c_str(), 0, true, "offline");
  }

  if (!connected)
  {
    g_state = MqttHandlerState::ERROR;
    Serial.printf("MQTT: conexión fallida state=%d\n", g_mqttClient.state());

    return false;
  }

  // Publicar disponibilidad actual
  bool published = g_mqttClient.publish(availabilityTopic.c_str(), "online", true);

  if (!published)
  {
    Serial.println("MQTT: conectado pero no se pudo publicar availability");
  }

  g_state = MqttHandlerState::CONNECTED;
  Serial.println("MQTT: conectado correctamente");

  return true;
}


static void publishPendingEvents()
{
  TelemetryEvent event;

  // Procesamos la cola mientras:
  //  - haya eventos
  //  - MQTT siga conectado
  while (g_mqttClient.connected() && telemetryStorePeekEvent(event))
  {
    String topic = "lora-p2p/device/" + event.deviceId + "/event";
    JsonDocument doc;

    doc["timestamp"] = event.timestamp;
    doc["type"] = telemetryEventTypeToString(event.type);
    doc["value"] = event.value;
    doc["threshold"] = event.threshold;

    char payload[256];
    size_t len = serializeJson(doc, payload, sizeof(payload));

    if (len == 0)
    {
      Serial.println("MQTT: error serializando telemetry event");

      // No eliminamos el evento.
      return;
    }

    bool published = g_mqttClient.publish(topic.c_str(), payload, false); // NO retained

    if (!published)
    {
      Serial.printf("MQTT: fallo publicando event device=%s type=%s\n", event.deviceId.c_str(), telemetryEventTypeToString(event.type));

      // Se queda en la cola para reintentar.
      return;
    }

    Serial.printf("MQTT EVENT TX: topic=%s payload=%s\n", topic.c_str(), payload);

    // Solo eliminamos después de publish OK.
    telemetryStorePopEvent();
  }
}


static void publishPendingTelemetry()
{
  size_t count = telemetryStoreDeviceCount();

  for (size_t i = 0; i < count; ++i)
  {
    TelemetryState state;

    if (!telemetryStoreGetDevice(i, state))
    {
      continue;
    }

    if (!state.used || !state.pendingPublish)
    {
      continue;
    }

    String topic = "lora-p2p/device/" + state.deviceId + "/state";
    JsonDocument doc;

    doc["seq"] = state.seq;
    doc["timestamp"] = state.timestamp;

    if (state.hasTemperature)
    {
      doc["temp"] = state.temperature;
    }

    if (state.hasHumidity)
    {
      doc["hum"] = state.humidity;
    }

    doc["rssi"] = state.rssi;
    doc["snr"] = state.snr;

    char payload[256];
    size_t len = serializeJson(doc, payload, sizeof(payload));

    if (len == 0)
    {
      Serial.println("MQTT: error serializando telemetry state");
      continue;
    }

    bool published = g_mqttClient.publish(topic.c_str(), payload, true); // retained

    if (!published)
    {
      Serial.printf("MQTT: fallo publicando state device=%s seq=%lu\n", state.deviceId.c_str(), static_cast<unsigned long>(state.seq));
      continue;
    }

    telemetryStoreMarkPublished(state.deviceId, state.seq);
    Serial.printf("MQTT TX: topic=%s payload=%s\n", topic.c_str(), payload);
  }
}


// =========================================================
// TAREA MQTT
// =========================================================
static void mqttTask(void *param)
{
  (void)param;

  Serial.println(
      "MQTT: task started");

  // PubSubClient usa por defecto un socket timeout bastante
  // mayor. Lo reducimos porque el broker está en LAN y
  // además MQTT no debe atascar esta tarea demasiado tiempo.
  g_mqttClient.setSocketTimeout(2);

  g_mqttClient.setKeepAlive(30);

  // Suficiente para telemetría sencilla.
  // Más adelante podremos aumentarlo para HA Discovery.
  g_mqttClient.setBufferSize(512);

  while (true)
  {
    // -----------------------------------------------------
    // WiFi no disponible
    // -----------------------------------------------------
    if (WiFi.status() != WL_CONNECTED)
    {
      if (g_mqttClient.connected())
      {
        g_mqttClient.disconnect();
      }

      g_state = MqttHandlerState::WAITING_WIFI;
      vTaskDelay(pdMS_TO_TICKS(MQTT_NO_WIFI_DELAY_MS));

      continue;
    }

    // -----------------------------------------------------
    // MQTT conectado
    // -----------------------------------------------------
    if (g_mqttClient.connected())
    {
      g_state = MqttHandlerState::CONNECTED;
      g_mqttClient.loop();

      // Primero eventos pendientes.
      publishPendingEvents();

      // Después último estado de cada nodo.
      publishPendingTelemetry();

      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // -----------------------------------------------------
    // Intentar conexión/reconexión
    // -----------------------------------------------------
    if (!connectToBroker())
    {
      vTaskDelay(pdMS_TO_TICKS(g_state == MqttHandlerState::WAITING_CONFIG ? MQTT_NO_CONFIG_DELAY_MS : MQTT_RECONNECT_DELAY_MS));
      continue;
    }
  }
}

// =========================================================
// API
// =========================================================
bool mqttHandlerStart(const String &deviceId)
{
  if (g_mqttTaskHandle != nullptr)
  {
    return true;
  }

  if (deviceId.length() == 0)
  {
    return false;
  }

  g_deviceId = deviceId;

  BaseType_t result = xTaskCreate(mqttTask, "mqttTask", 6144, nullptr, 1, &g_mqttTaskHandle);

  if (result != pdPASS)
  {
    g_mqttTaskHandle = nullptr;
    Serial.println("MQTT: no se pudo crear mqttTask");

    return false;
  }

  return true;
}


bool mqttHandlerIsConnected()
{
  return
      g_state == MqttHandlerState::CONNECTED;
}


MqttHandlerState mqttHandlerGetState()
{
  return g_state;
}


const char *mqttHandlerStateToString(MqttHandlerState state)
{
  switch (state)
  {
  case MqttHandlerState::STOPPED:
    return "stopped";

  case MqttHandlerState::WAITING_WIFI:
    return "waiting_wifi";

  case MqttHandlerState::WAITING_CONFIG:
    return "waiting_config";

  case MqttHandlerState::CONNECTING:
    return "connecting";

  case MqttHandlerState::CONNECTED:
    return "connected";

  case MqttHandlerState::ERROR:
    return "error";
  }

  return "unknown";
}