#pragma once

#include <Arduino.h>
#include <stdint.h>

struct MqttConfig
{
  String host;
  uint16_t port = 1883;
  String user;
  String password;
};

/**
 * Guarda la configuración MQTT en NVS.
 */
bool mqttConfigSave(const MqttConfig &config);

/**
 * Carga la configuración MQTT desde NVS.
 */
bool mqttConfigLoad(MqttConfig &config);

/**
 * Indica si existe una configuración MQTT válida.
 */
bool mqttConfigExists();

/**
 * Elimina la configuración MQTT guardada.
 */
bool mqttConfigClear();