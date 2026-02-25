#pragma once
#include <Arduino.h>
#include <protocol.h>

// Inicializa el handler con el DEVICE_ID actual
void lora_handler_init(const String &deviceId);

// Procesa un mensaje LoRa recibido
void lora_handle_message(const String &msgStr, int rssi, float snr);
