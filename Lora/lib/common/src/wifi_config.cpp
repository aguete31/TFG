#include "wifi_config.h"

#include <WiFi.h>
#include <Preferences.h>

static WifiConfigState g_wifi_state = WIFI_CFG_IDLE;
static String g_wifi_ssid;
static unsigned long g_wifi_connect_start = 0;

static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;

bool wifiConfigSaveAndConnect(const String &ssidIn, const String &passwordIn)
{
  String ssid = ssidIn;
  String password = passwordIn;

  ssid.trim();
  password.trim();

  if (ssid.length() == 0)
  {
    Serial.println("WIFI CONFIG: SSID vacío");
    return false;
  }

  // -----------------------------------------------------
  // Guardar configuración en NVS
  // -----------------------------------------------------
  Preferences prefs;

  if (!prefs.begin("wifi_cfg", false))
  {
    Serial.println("WIFI CONFIG: error abriendo NVS");
    return false;
  }

  size_t ssidWritten = prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.end();

  if (ssidWritten == 0)
  {
    Serial.println("WIFI CONFIG: error guardando SSID en NVS");
    return false;
  }

  // -----------------------------------------------------
  // Iniciar conexión STA manteniendo AP activo
  // -----------------------------------------------------
  g_wifi_ssid = ssid;
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  g_wifi_state = WIFI_CFG_CONNECTING;
  g_wifi_connect_start = millis();

  Serial.printf("WIFI CONFIG: SSID='%s' -> conectando...\n", ssid.c_str());
  return true;
}


void wifiConfigUpdate()
{
  if (g_wifi_state != WIFI_CFG_CONNECTING)
    return;

  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED)
  {
    g_wifi_state = WIFI_CFG_CONNECTED;
    Serial.printf("WIFI CONFIG: conectada. IP=%s\n",WiFi.localIP().toString().c_str());

    return;
  }

  if (millis() - g_wifi_connect_start > WIFI_CONNECT_TIMEOUT_MS)
  {
    g_wifi_state = WIFI_CFG_FAILED;
    Serial.println("WIFI CONFIG: fallo de conexión (timeout)");
  }
}


WifiConfigState wifiConfigGetState()
{
  return g_wifi_state;
}


String wifiConfigGetSsid()
{
  return g_wifi_ssid;
}


const char *wifiConfigStateToString(WifiConfigState state)
{
  switch (state)
  {
  case WIFI_CFG_IDLE:
    return "idle";

  case WIFI_CFG_CONNECTING:
    return "connecting";

  case WIFI_CFG_CONNECTED:
    return "connected";

  case WIFI_CFG_FAILED:
    return "failed";
  }

  return "unknown";
}

bool wifiConfigConnectStored(unsigned long timeoutMs)
{
  Preferences prefs;

  if (!prefs.begin("wifi_cfg", true))
  {
    Serial.println("WIFI CONFIG: no se pudo abrir NVS");
    return false;
  }

  String ssid;
  String password;

  if (prefs.isKey("ssid"))
    ssid = prefs.getString("ssid", "");

  if (prefs.isKey("pass"))
    password = prefs.getString("pass", "");

  prefs.end();

  if (ssid.length() == 0)
  {
    Serial.println("WIFI CONFIG: no hay WiFi configurada en NVS");
    return false;
  }

  g_wifi_ssid = ssid;
  g_wifi_state = WIFI_CFG_CONNECTING;
  g_wifi_connect_start = millis();

  Serial.printf("WIFI CONFIG: cargada SSID='%s' -> conectando...\n", ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startedAt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs)
  {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    g_wifi_state = WIFI_CFG_CONNECTED;
    Serial.printf("WIFI CONFIG: conectada. IP=%s\n", WiFi.localIP().toString().c_str());

    return true;
  }

  g_wifi_state = WIFI_CFG_FAILED;
  Serial.println("WIFI CONFIG: no se pudo conectar a la WiFi guardada");

  return false;
}