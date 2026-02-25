#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>

void mqtt_setup();
void mqtt_loop();
void publish_telemetry(const String& device_id, const String& payload, int rssi, float snr, uint32_t seq);

#endif