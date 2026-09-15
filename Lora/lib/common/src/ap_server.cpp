#include "ap_server.h"

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>

#include "pairing_service.h"
#include "wifi_config.h"

// ==================== Estado global interno ====================

// Servidor HTTP del modo AP (puerto 80)
static WebServer server(80);

// Tarea del servidor HTTP (AP)
static TaskHandle_t webServerTaskHandle = nullptr;

// Flag para saber si el AP está activo
static bool ap_running = false;

// ==================== Prototipos internos ====================

/**
 * @brief Tarea RTOS que atiende las peticiones HTTP del servidor en modo AP.
 */
static void webServerTask(void *param);

/**
 * @brief Manejador HTTP: página inicial de emparejamiento (GET /).
 */
static void handleRoot();

/**
 * @brief Manejador HTTP: página de configuración WiFi del gateway (GET /wifi).
 */
static void handleWifiPage();

/**
 * @brief Manejador HTTP: recibe la configuración WiFi (POST /wifi_config).
 */
static void handleWifiConfig();

/**
 * @brief Manejador HTTP: devuelve estado WiFi en JSON (GET /wifi_status).
 */
static void handleWifiStatus();

/**
 * @brief Manejador HTTP: devuelve estado de emparejamiento en JSON (GET /status).
 */
static void handleStatus();

/**
 * @brief Manejador HTTP: para rutas no encontradas (404).
 */
static void handleNotFound();

/**
 * @brief Manejador HTTP: inicia el proceso de emparejamiento (POST /pair).
 */
static void handlePair();

// ==================== Manejadores HTTP (HTML / JSON) ====================
/**
 * @brief Manejador para la ruta raíz "/" que sirve la página de emparejamiento.
 *
 * - En gateway:  sirve /gateway_form.html
 * - En Heltec :  sirve /heltec_form.html
 */
static void handleRoot()
{
#if defined(DEVICE_GATEWAY)
  const char *path = "/gateway_form.html";
#elif defined(DEVICE_HELTEC)
  const char *path = "/heltec_form.html";
#endif

#if defined(DEVICE_GATEWAY) || defined(DEVICE_HELTEC)
  if (SPIFFS.exists(path))
  {
    File f = SPIFFS.open(path, "r");
    server.streamFile(f, "text/html");
    f.close();
  }
  else
  {
    server.send(500, "text/plain", "Archivo de formulario HTML no encontrado");
  }
#endif

  Serial.println("HTTP GET /  -> served form");
}

/**
 * @brief Sirve la página de configuración WiFi del gateway.
 *
 * Ruta: GET /wifi
 * Fichero: /gateway_wifi.html en SPIFFS
 */
static void handleWifiPage()
{
  const char *path = "/gateway_wifi.html";

  if (SPIFFS.exists(path))
  {
    File f = SPIFFS.open(path, "r");
    server.streamFile(f, "text/html");
    f.close();
  }
  else
  {
    server.send(500, "text/plain", "Archivo gateway_wifi.html no encontrado");
  }

  Serial.println("HTTP GET /wifi -> gateway_wifi.html");
}

/**
 * @brief Recibe SSID + password desde gateway_wifi.html y arranca STA.
 *
 * Ruta: POST /wifi_config
 * Body: application/x-www-form-urlencoded
 *       ssid=...&password=...
 *
 * - Guarda SSID y password en NVS ("wifi_cfg").
 * - Pone el ESP32 en modo WIFI_AP_STA.
 * - Inicia la conexión WiFi STA sin apagar aún el AP.
 */
static void handleWifiConfig()
{
  if (!server.hasArg("ssid") || !server.hasArg("password"))
  {
    server.send(400, "text/plain", "Missing ssid or password");
    Serial.println("HTTP POST /wifi_config missing arg");

    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");

  if (!wifiConfigSaveAndConnect(ssid, password))
  {
    server.send(400, "text/plain", "Invalid WiFi configuration");
    return;
  }

  server.send(200, "text/plain", "OK");
}

/**
 * @brief Devuelve el estado de la conexión WiFi en JSON para la página WiFi.
 *
 * Ruta: GET /wifi_status
 *
 * Respuesta esperada por gateway_wifi.html:
 *   {
 *     "state": "idle|connecting|connected|failed",
 *     "ssid" : "xxx"
 *   }
 *
 * - Actualiza el estado en función de WiFi.status().
 * - Incluye la IP local si la STA está conectada.
 */
static void handleWifiStatus()
{
  wifiConfigUpdate();

  WifiConfigState state = wifiConfigGetState();
  String json = "{";

  json += "\"state\":\"";
  json += wifiConfigStateToString(state);
  json += "\",";

  json += "\"ssid\":\"";
  json += wifiConfigGetSsid();
  json += "\"";

  if (state == WIFI_CFG_CONNECTED)
  {
    json += ",\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\"";
  }

  json += "}";
  server.send(200, "application/json", json);
}

/**
 * @brief Manejador para la ruta "/status" que devuelve el estado actual en JSON.
 *
 * Ruta: GET /status
 *
 * - En gateway:
 *   {
 *     "state" : "idle|pending|busy|success|failed",
 *     "paired": true/false
 *   }
 *
 * - En Heltec:
 *   {
 *     "state" : "idle|pairing|waiting|paired|failed"
 *   }
 */
static void handleStatus()
{
  #ifdef DEVICE_GATEWAY

  PairState state = pairingServiceGetState();
  bool paired = pairingServiceIsPaired();

  String json = String("{\"state\":\"") + pairingServiceStateToString(state) + String("\",\"paired\":") + (paired ? "true" : "false") + String("}");

  server.send(200, "application/json", json);
  Serial.printf("HTTP GET /status -> %s\n", json.c_str());

  #endif

  #ifdef DEVICE_HELTEC

  String json = "{\"state\":\"";

  switch (pairStatus)
  {
    case IDLE:
      json += "idle";
      break;
    case PAIRING:
      json += "pairing";
      break;
    case WAITING:
      json += "waiting";
      break;
    case PAIRED:
      json += "paired";
      break;
    case FAILED:
      json += "failed";
      break;
  }

  json += "\"}";
  server.send(200, "application/json", json);
  Serial.printf("HTTP GET /status -> %s\n", json.c_str());
  
  #endif
}

/**
 * @brief Manejador por defecto para rutas no encontradas.
 *
 * Ruta: cualquier ruta no registrada -> 404.
 */
static void handleNotFound()
{
  server.send(404, "text/plain", "Not found");
  Serial.println("HTTP 404");
}

/**
 * @brief Manejador para la ruta "/pair" que inicia el proceso de emparejamiento.
 *
 * Ruta: POST /pair
 * Body: password=...
 *
 * - Valida la contraseña.
 * - Lanza la tarea pairTask() para derivar la clave.
 * - Devuelve "OK" si la petición se acepta.
 */
static void handlePair()
{
  if (!server.hasArg("password"))
  {
    server.send(400, "text/plain", "Missing password");
    Serial.println("HTTP POST /pair missing password");

    return;
  }

  String password = server.arg("password");
  PairingStartResult result = pairingServiceStart(password);

  switch (result)
  {
  case PairingStartResult::STARTED:

    server.send(200, "text/plain", "OK");
    Serial.println("HTTP POST /pair accepted");
    break;

  case PairingStartResult::INVALID_PASSWORD:

    server.send(400, "text/plain", "Password too short (min 8)");
    Serial.println("HTTP POST /pair password too short");
    break;

  case PairingStartResult::BUSY:

    server.send(409, "text/plain", "Pairing already in progress");
    Serial.println("HTTP POST /pair rejected: busy");
    break;

  case PairingStartResult::ALLOC_ERROR:

    server.send(500, "text/plain", "Memory allocation failed");
    Serial.println("HTTP POST /pair allocation error");
    break;

  case PairingStartResult::TASK_ERROR:

    server.send(500, "text/plain", "Failed to start pairing task");
    Serial.println("HTTP POST /pair task error");
    break;
  }
}

// ==================== Tareas RTOS ====================

/**
 * @brief Tarea que atiende el servidor HTTP mientras el AP está activo.
 *
 * - Llama periódicamente a server.handleClient().
 * - Debe ejecutarse en paralelo al resto de la lógica del dispositivo.
 */
static void webServerTask(void *param)
{
  (void)param;
  Serial.println("webServerTask started");
  while (true)
  {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ==================== API pública: control del AP ====================

/**
 * @brief Inicia el punto de acceso WiFi y el servidor HTTP de configuración.
 *
 * @param ssid        SSID del AP.
 * @param ap_pass     Contraseña del AP.
 * @param max_clients Número máximo de clientes conectados al AP.
 *
 * - Monta SPIFFS (para servir HTML).
 * - Arranca el AP en 192.168.4.1.
 * - Registra las rutas HTTP de emparejamiento y WiFi.
 * - Lanza la tarea webServerTask() para atender clientes.
 */
void ap_start(const char *ssid, const char *ap_pass, uint8_t max_clients)
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  // Configurar IP del AP
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPdisconnect(true);
  bool cfgOk = WiFi.softAPConfig(local_IP, gateway, subnet);
  Serial.printf("softAPConfig returned %d\n", cfgOk ? 1 : 0);

  // Arrancar AP
  bool ok = WiFi.softAP(ssid, ap_pass, 1, false, max_clients);
  if (!ok)
  {
    Serial.println("ap_start: WiFi.softAP failed");
    pairingServiceMarkFailed();

    return;
  }

  ap_running = true;

  Serial.printf("AP started: SSID='%s' IP=%s\n", ssid, WiFi.softAPIP().toString().c_str());

  // Rutas de emparejamiento y estado
  server.on("/", HTTP_GET, handleRoot);
  server.on("/pair", HTTP_POST, handlePair);
  server.on("/status", HTTP_GET, handleStatus);

  // Rutas de configuración WiFi (gateway)
  #ifdef DEVICE_GATEWAY
  server.on("/wifi", HTTP_GET, handleWifiPage);
  server.on("/wifi_config", HTTP_POST, handleWifiConfig);
  server.on("/wifi_status", HTTP_GET, handleWifiStatus);
  #endif

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started on 192.168.4.1");

  // Lanzar tarea del servidor HTTP si no está ya creada
  if (webServerTaskHandle == nullptr)
  {
    BaseType_t res = xTaskCreatePinnedToCore(
        webServerTask,
        "webServerTask",
        4096,
        nullptr,
        1,
        &webServerTaskHandle,
        1);
    if (res != pdPASS)
    {
      Serial.println("Failed to create webServerTask");
    }
  }
}

/**
 * @brief Detiene el punto de acceso WiFi y el servidor HTTP de configuración.
 *
 * - Detiene la tarea webServerTask().
 * - Para el servidor HTTP asociado al AP.
 * - Desactiva el AP, dejando la interfaz STA activa (WiFi.mode(WIFI_STA)).
 */
void ap_stop()
{
  Serial.println("Stopping AP and HTTP server");

  // Parar tarea HTTP del AP
  if (webServerTaskHandle)
  {
    vTaskDelete(webServerTaskHandle);
    webServerTaskHandle = nullptr;
  }

  // Parar servidor HTTP del AP
  server.stop();

  // Apagar solo el AP, manteniendo la STA
  if (ap_running)
  {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA); // Dejar solo STA activo
    ap_running = false;
  }

  pairingServiceResetRuntimeState();

}

/**
 * @brief Registra la función callback para notificar el resultado del emparejamiento.
 *
 * @param cb Función del usuario que será llamada con true/false al terminar pairTask().
 */
void ap_set_pair_callback(pair_callback_t cb)
{
  pairingServiceSetCallback(cb);
}

/**
 * @brief Obtiene el estado actual del emparejamiento (solo gateway).
 *
 * @return Estado PairState actual. Si no es gateway, devuelve PAIR_IDLE.
 */
PairState ap_get_state()
{
  return pairingServiceGetState();
}