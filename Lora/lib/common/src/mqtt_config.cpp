#include "mqtt_config.h"

#include <Preferences.h>

static const char *NVS_NAMESPACE = "mqtt_cfg";

static const char *KEY_HOST = "host";
static const char *KEY_PORT = "port";
static const char *KEY_USER = "user";
static const char *KEY_PASS = "pass";

bool mqttConfigSave(const MqttConfig &config)
{
  String host = config.host;
  String user = config.user;
  String password = config.password;

  host.trim();
  user.trim();

  if (host.length() == 0)
  {
    Serial.println("MQTT CONFIG: host vacío");
    return false;
  }

  if (config.port == 0)
  {
    Serial.println("MQTT CONFIG: puerto inválido");
    return false;
  }

  Preferences prefs;

  if (!prefs.begin(NVS_NAMESPACE, false))
  {
    Serial.println("MQTT CONFIG: error abriendo NVS");
    return false;
  }

  size_t hostWritten = prefs.putString(KEY_HOST, host);
  size_t portWritten = prefs.putUShort(KEY_PORT, config.port);

  prefs.putString(KEY_USER, user);
  prefs.putString(KEY_PASS, password);

  prefs.end();

  if (hostWritten == 0 || portWritten == 0)
  {
    Serial.println("MQTT CONFIG: error guardando configuración");
    return false;
  }

  Serial.printf("MQTT CONFIG: guardada host=%s port=%u\n", host.c_str(), config.port);
  return true;
}


bool mqttConfigLoad(MqttConfig &config)
{
  Preferences prefs;

  if (!prefs.begin(NVS_NAMESPACE, false))
  {
    return false;
  }

  if (!prefs.isKey(KEY_HOST))
  {
    prefs.end();
    return false;
  }

  config.host = prefs.getString(KEY_HOST, "");
  config.port = prefs.getUShort(KEY_PORT, 1883);
  config.user = prefs.isKey(KEY_USER) ? prefs.getString(KEY_USER, "") : "";
  config.password = prefs.isKey(KEY_PASS) ? prefs.getString(KEY_PASS, "") : "";

  prefs.end();

  if (config.host.length() == 0 || config.port == 0)
  {
    return false;
  }

  return true;
}


bool mqttConfigExists()
{
  MqttConfig config;
  return mqttConfigLoad(config);
}


bool mqttConfigClear()
{
  Preferences prefs;

  if (!prefs.begin(NVS_NAMESPACE, false))
  {
    return false;
  }

  bool ok = prefs.clear();

  prefs.end();

  Serial.println("MQTT CONFIG: configuración eliminada");
  return ok;
}