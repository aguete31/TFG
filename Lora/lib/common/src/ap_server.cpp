#include "ap_server.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <cstring>

#include "pair_status.h"
#include "crypto.h"
#include "iv_generator.h"

// ==================== Estado global interno ====================

// Servidor HTTP del modo AP (puerto 80)
static WebServer server(80);

// Callback de usuario para notificar resultado de emparejamiento
static pair_callback_t user_cb = nullptr;

#ifdef DEVICE_GATEWAY
// Estado del emparejamiento (gateway)
static volatile PairState g_state = PAIR_IDLE;
#endif

#ifdef DEVICE_HELTEC
volatile PairStatus pairStatus = IDLE;
#endif

// Tarea del servidor HTTP (AP)
static TaskHandle_t webServerTaskHandle = nullptr;

// Manejador de almacenamiento NVS (usado para clave y WiFi)
static Preferences prefs;

// Flag para saber si el AP está activo
static bool ap_running = false;

// ===== Estado de configuración WiFi mientras el AP está encendido =====
enum WifiCfgState
{
  WIFI_CFG_IDLE,
  WIFI_CFG_CONNECTING,
  WIFI_CFG_CONNECTED,
  WIFI_CFG_FAILED
};

static WifiCfgState g_wifi_state = WIFI_CFG_IDLE;
static String g_wifi_ssid;
static unsigned long g_wifi_connect_start = 0;

// ==================== Prototipos internos ====================

/**
 * @brief Tarea RTOS que atiende las peticiones HTTP del servidor en modo AP.
 */
static void webServerTask(void *param);

/**
 * @brief Tarea RTOS que realiza la derivación de clave a partir de la contraseña.
 */
static void pairTask(void *param);

/**
 * @brief Convierte el estado de emparejamiento del gateway a cadena.
 */
static const char *stateToStr(PairState s);

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

// ==================== Funciones auxiliares ====================

/**
 * @brief Convierte el estado de emparejamiento del gateway a cadena legible.
 *
 * @param s Estado interno de emparejamiento.
 * @return Cadena constante con el nombre del estado.
 */
static const char *stateToStr(PairState s)
{
  switch (s)
  {
  case PAIR_IDLE:
    return "idle";
  case PAIR_PENDING:
    return "pending";
  case PAIR_BUSY:
    return "busy";
  case PAIR_SUCCESS:
    return "success";
  case PAIR_FAILED:
    return "failed";
  }
  return "unknown";
}

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

  g_wifi_ssid = server.arg("ssid");
  String wifi_pass = server.arg("password");

  g_wifi_ssid.trim();
  wifi_pass.trim();

  if (g_wifi_ssid.length() == 0)
  {
    server.send(400, "text/plain", "SSID vacío");
    Serial.println("HTTP POST /wifi_config with empty SSID");
    return;
  }

  // Guardar configuración WiFi en NVS para futuros arranques
  prefs.begin("wifi_cfg", false);
  prefs.putString("ssid", g_wifi_ssid);
  prefs.putString("pass", wifi_pass);
  prefs.end();

  // Arrancar STA sin apagar el AP (modo AP+STA)
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(g_wifi_ssid.c_str(), wifi_pass.c_str());

  g_wifi_state = WIFI_CFG_CONNECTING;
  g_wifi_connect_start = millis();

  Serial.printf("WiFi config recibida. SSID='%s' -> conectando...\n", g_wifi_ssid.c_str());

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
  // Actualizar estado según WiFi.status() si estamos en fase de conexión
  if (g_wifi_state == WIFI_CFG_CONNECTING)
  {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED)
    {
      g_wifi_state = WIFI_CFG_CONNECTED;
      Serial.println("WiFi STA conectada correctamente");
    }
    else
    {
      // Timeout de 15 segundos
      if (millis() - g_wifi_connect_start > 15000)
      {
        g_wifi_state = WIFI_CFG_FAILED;
        Serial.println("WiFi STA fallo de conexión (timeout)");
      }
    }
  }

  String stateStr;
  switch (g_wifi_state)
  {
  case WIFI_CFG_IDLE:
    stateStr = "idle";
    break;
  case WIFI_CFG_CONNECTING:
    stateStr = "connecting";
    break;
  case WIFI_CFG_CONNECTED:
    stateStr = "connected";
    break;
  case WIFI_CFG_FAILED:
    stateStr = "failed";
    break;
  }

  String json = "{";
  json += "\"state\":\"" + stateStr + "\",";
  json += "\"ssid\":\"" + g_wifi_ssid + "\"";
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
  bool paired = false;
  prefs.begin("lora_proto", true);
  paired = prefs.getBool("paired", false);
  prefs.end();

  String json = String("{\"state\":\"") + stateToStr(g_state) +
                String("\",\"paired\":") + (paired ? "true" : "false") + String("}");

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

#if defined(DEVICE_GATEWAY)
  if (g_state == PAIR_BUSY || g_state == PAIR_PENDING)
  {
    server.send(409, "text/plain", "Pairing already in progress");
    Serial.println("HTTP POST /pair rejected: already in progress");
    return;
  }
#elif defined(DEVICE_HELTEC)
  if (pairStatus == PAIRING)
  {
    server.send(409, "text/plain", "Pairing already in progress");
    Serial.println("HTTP POST /pair rejected: already in progress");
    return;
  }
#endif

  String password = server.arg("password");
  password.trim();

  if (password.length() < 8)
  {
    server.send(400, "text/plain", "Password too short (min 8)");
    Serial.println("HTTP POST /pair password too short");
    return;
  }

  // Respuesta inmediata al cliente HTTP
  server.send(200, "text/plain", "OK");
  Serial.println("HTTP POST /pair accepted: spawning pair task");

  // Copiar la contraseña a un buffer dinámico para pasarla a la tarea
  char *pass_c = static_cast<char *>(malloc(password.length() + 1));
  if (!pass_c)
  {
    Serial.println("malloc failed for pass_c");
#if defined(DEVICE_GATEWAY)
    g_state = PAIR_FAILED;
#elif defined(DEVICE_HELTEC)
    pairStatus = FAILED;
#endif
    return;
  }
  strcpy(pass_c, password.c_str());

#if defined(DEVICE_GATEWAY)
  g_state = PAIR_PENDING;
#elif defined(DEVICE_HELTEC)
  pairStatus = PAIRING;
#endif

  // Crear la tarea que hará la derivación de clave
  BaseType_t res = xTaskCreatePinnedToCore(
      pairTask,
      "pairTask",
      8192,
      pass_c,
      1,
      nullptr,
      1);

  if (res != pdPASS)
  {
    Serial.println("Failed to create pairTask");
    free(pass_c);
#if defined(DEVICE_GATEWAY)
    g_state = PAIR_FAILED;
#elif defined(DEVICE_HELTEC)
    pairStatus = FAILED;
#endif
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

/**
 * @brief Tarea que realiza la derivación de la clave a partir de la contraseña.
 *
 * Flujo:
 *  - Recibe la contraseña en un buffer dinámico.
 *  - Deriva la clave AES (AES_KEY) usando derive_key_from_password().
 *  - Guarda la clave en NVS si la derivación es correcta.
 *  - Limpia la contraseña de memoria (borrado + free).
 *  - Actualiza el estado de emparejamiento.
 *  - Llama al callback de usuario (user_cb) con el resultado.
 */
static void pairTask(void *param)
{
  char *pw = static_cast<char *>(param);
  Serial.println("pairTask: starting derivation");

#if defined(DEVICE_GATEWAY)
  g_state = PAIR_BUSY;
#endif

  String pwStr(pw ? pw : "");
  bool ok = false;

  // Derivar clave desde la contraseña -> AES_KEY
  ok = derive_key_from_password(pwStr, AES_KEY, AES_KEY_SIZE);

  // Crear un nuevo espacio de IV para esta clave
  if (ok)
  {
    if (!initializeIvForNewKey())
    {
      Serial.println("pairTask: failed to initialize IV epoch");
      clear_aes_key();
      ok = false;
    }
  }

  // Borrar la contraseña de memoria
  if (pw)
  {
    size_t len = strlen(pw);
    memset(pw, 0, len);
    free(pw);
  }
  pwStr = String();

  // Si la derivación fue bien, guardar la clave en NVS
  if (ok)
  {
    if (!store_device_key_in_nvs(AES_KEY, AES_KEY_SIZE))
    {
      Serial.println("pairTask: failed to store device key in NVS");
      clear_aes_key();
      ok = false;
    }
  }

  // Actualizar estado según ok/fallo
  if (ok)
  {
#if defined(DEVICE_GATEWAY)
    prefs.begin("lora_proto", false);
    prefs.putBool("paired", true);
    prefs.end();
    g_state = PAIR_SUCCESS;
#endif
    Serial.println("pairTask: derivation + store OK");
  }
  else
  {
#if defined(DEVICE_GATEWAY)
    g_state = PAIR_FAILED;
#elif defined(DEVICE_HELTEC)
    pairStatus = FAILED;
#endif
    Serial.println("pairTask: derivation/store FAILED");
  }

  // Avisar al callback de usuario
  if (user_cb)
  {
    user_cb(ok);
  }

  vTaskDelete(nullptr);
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
#if defined(DEVICE_GATEWAY)
    g_state = PAIR_FAILED;
#elif defined(DEVICE_HELTEC)
    pairStatus = FAILED;
#endif
    return;
  }

  ap_running = true;

  Serial.printf("AP started: SSID='%s' IP=%s\n", ssid, WiFi.softAPIP().toString().c_str());

  // Rutas de emparejamiento y estado
  server.on("/", HTTP_GET, handleRoot);
  server.on("/pair", HTTP_POST, handlePair);
  server.on("/status", HTTP_GET, handleStatus);

  // Rutas de configuración WiFi (gateway)
  server.on("/wifi", HTTP_GET, handleWifiPage);
  server.on("/wifi_config", HTTP_POST, handleWifiConfig);
  server.on("/wifi_status", HTTP_GET, handleWifiStatus);

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

#if defined(DEVICE_GATEWAY)
  g_state = PAIR_IDLE;
#endif
}

/**
 * @brief Registra la función callback para notificar el resultado del emparejamiento.
 *
 * @param cb Función del usuario que será llamada con true/false al terminar pairTask().
 */
void ap_set_pair_callback(pair_callback_t cb)
{
  user_cb = cb;
}

/**
 * @brief Obtiene el estado actual del emparejamiento (solo gateway).
 *
 * @return Estado PairState actual. Si no es gateway, devuelve PAIR_IDLE.
 */
PairState ap_get_state()
{
#if defined(DEVICE_GATEWAY)
  return g_state;
#else
  return PAIR_IDLE;
#endif
}