#include "http_server.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "telemetry_buffer.h"

// ==================== Configuración básica ====================

// Puerto HTTP (debe coincidir con el configurado en Home Assistant)
static const uint16_t HTTP_PORT = 8000;

// ==================== Estado interno ====================

// Servidor HTTP en modo STA (red de casa)
static WebServer httpServer(HTTP_PORT);

// Indica si hay WiFi conectada cuando se inicializa el módulo
static bool g_wifi_connected = false;

// Indica si el servidor HTTP ya se ha iniciado (para evitar doble begin())
static bool g_http_started = false;

// Indica si el dispositivo está emparejado (para reportar a HA)
static bool g_is_paired = false;

// Identificador del dispositivo (se incluye en la respuesta /status)
static String g_device_id;

// Identificadores de cada dispositivo publicador
static uint64_t h_last_ts = 0;
static int h_last_retry = 0;
static String h_deviceId = "";
static String h_last_payload = "{}";
static int h_last_rssi = 0;
static float h_last_snr = 0;

// ==================== Prototipos internos ====================

/**
 * @brief Registra las rutas HTTP y arranca el servidor en HTTP_PORT.
 */
static void setup_http_server();

/**
 * @brief Manejador HTTP: devuelve estado para Home Assistant (GET /status).
 */
static void handle_http_status();

void http_set_last_telemetry(uint64_t timestamp, int retry, const String &deviceId, const String &payload, int rssi, float snr)
{
  h_last_ts = timestamp;
  h_last_retry = retry;
  h_deviceId = deviceId;
  h_last_payload = payload;
  h_last_rssi = rssi;
  h_last_snr = snr;
}

// ==================== API pública ====================

/**
 * @brief Inicializa el servidor HTTP para Home Assistant.
 *
 * @param deviceId  ID único del gateway (por ejemplo, derivado de la MAC).
 * @param isPaired  true si el gateway ya está emparejado (clave AES en NVS).
 *
 * Esta función:
 *  - Guarda el ID y el estado de emparejamiento.
 *  - Comprueba si la WiFi STA ya está conectada.
 *  - Si hay WiFi, llama a setup_http_server() para exponer /status.
 *
 */
void http_init(const String &deviceId, bool isPaired)
{
  g_device_id = deviceId;
  g_is_paired = isPaired;

  // Comprobar si la STA ya está conectada
  if (WiFi.status() == WL_CONNECTED)
  {
    g_wifi_connected = true;
    Serial.print("[HTTP] WiFi ya conectada.");
  }
  else
  {
    g_wifi_connected = false;
    Serial.println("[HTTP] No hay WiFi conectada al llamar http_init(), no se inicia el servidor HTTP");
    return;
  }

  // Arrancar servidor HTTP si procede
  setup_http_server();
}

/**
 * @brief Actualiza el flag interno de "emparejado" para reportarlo en /status.
 *
 * @param paired  true si el dispositivo está emparejado.
 */
void http_set_paired(bool paired)
{
  g_is_paired = paired;
}

/**
 * @brief Bucle de servicio del servidor HTTP.
 *
 * Debe llamarse periódicamente desde loop().
 * Solo atiende peticiones si:
 *  - El servidor HTTP se ha iniciado correctamente.
 *  - La WiFi STA sigue conectada.
 */
void http_loop()
{
  if (!g_http_started)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    // Si se pierde la WiFi, deja de atender
    g_wifi_connected = false;
    return;
  }

  httpServer.handleClient();
}

// ==================== Implementación interna ====================

/**
 * @brief Manejador para GET /status (estado para Home Assistant).
 *
 * Respuesta JSON:
 *   {
 *     "status"    : "online",
 *     "device_id" : "<ID del gateway>",
 *     "paired"    : true/false
 *   }
 *
 * - "status" se considera "online" si el endpoint responde.
 * - "paired" refleja el estado que se pase vía http_set_paired() / http_init().
 */
static void handle_http_status()
{
  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"device_id\":\"" + g_device_id + "\",";
  json += "\"paired\":";
  json += (g_is_paired ? "true" : "false");
  json += "}";

  httpServer.send(200, "application/json", json);
}

static void handle_http_telemetry()
{
  // Si NO from_ts → devolver último dato simple
  if (!httpServer.hasArg("from_ts"))
  {
    String json = "{";
    json += "\"timestamp\":" + String((unsigned long long)h_last_ts) + ",";
    json += "\"retry\":" + String(h_last_retry) + ",";
    json += "\"device_id\":\"" + h_deviceId + "\",";
    json += "\"payload\":" + h_last_payload + ",";
    json += "\"rssi\":" + String(h_last_rssi) + ",";
    json += "\"snr\":" + String(h_last_snr);
    json += "}";
    httpServer.send(200, "application/json", json);
    return;
  }

  // Parámetros para paginación por timestamp
  uint64_t from_ts = strtoull(httpServer.arg("from_ts").c_str(), nullptr, 10);
  int limit = MAX_ENTRIES_PER_REQUEST;

  if (httpServer.hasArg("limit"))
  {
    int l = httpServer.arg("limit").toInt();
    if (l > 0 && l <= MAX_ENTRIES_PER_REQUEST)
      limit = l;
  }

  // Obtener datos del buffer
  std::vector<TelemetryData> dataArray = getTelemetryDataFromTs(from_ts, limit);

  // Convertir a JSON array
  String json = telemetryArrayToJson(dataArray);

  httpServer.send(200, "application/json", json);
}

/**
 * @brief Registra las rutas HTTP y arranca el servidor en HTTP_PORT.
 *
 * - Solo se llama si g_wifi_connected es true.
 * - Usa la instancia global httpServer(HTTP_PORT).
 */
static void setup_http_server()
{
  if (!g_wifi_connected)
  {
    Serial.println("[HTTP] No WiFi, no se inicia el servidor HTTP");
    return;
  }

  if (g_http_started)
  {
    Serial.println("[HTTP] Servidor HTTP ya estaba iniciado, se omite setup_http_server()");
    return;
  }

  // Endpoint GET /status y /telemetry para HA
  httpServer.on("/status", HTTP_GET, handle_http_status);
  httpServer.on("/telemetry", HTTP_GET, handle_http_telemetry);

  httpServer.begin();
  g_http_started = true;

  Serial.printf("[HTTP] Servidor HTTP iniciado en puerto %u\n", HTTP_PORT);
}