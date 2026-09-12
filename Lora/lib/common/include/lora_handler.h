#pragma once
#include <Arduino.h>
#include <protocol.h>

// Inicializa el handler con el DEVICE_ID actual
void lora_handler_init(const String &deviceId);

/**
 * @brief Procesa una trama LoRa binaria recibida.
 *
 * @param packet Buffer recibido.
 * @param packetLen Longitud real de la trama.
 * @param rssi RSSI medido por la radio.
 * @param snr SNR medido por la radio.
 */
void lora_handle_binary_packet(const uint8_t *packet, size_t packetLen, int rssi, float snr);