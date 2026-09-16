#pragma once

#include <Arduino.h>

enum class MqttHandlerState
{
  STOPPED,
  WAITING_WIFI,
  WAITING_CONFIG,
  CONNECTING,
  CONNECTED,
  ERROR
};

bool mqttHandlerStart(const String &deviceId);

bool mqttHandlerIsConnected();

MqttHandlerState mqttHandlerGetState();

const char *mqttHandlerStateToString(MqttHandlerState state);