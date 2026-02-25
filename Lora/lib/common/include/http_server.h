#pragma once

#include <Arduino.h>

/**
 * @file http_server.h
 * @brief Servidor HTTP en modo estación (STA) para integración con Home Assistant.
 *
 * Este módulo:
 *   - Expone un endpoint HTTP: GET /status
 *   - Reporta si el dispositivo está online, su ID y si está emparejado.
 *   - NO gestiona la conexión WiFi: se asume que la STA ya está conectada.
 *
 * Debe llamarse http_init() cuando la WiFi del dispositivo (modo STA)
 * ya está conectada a la red local.
 */

/**
 * @brief Inicializa el servidor HTTP accesible desde la red local.
 *
 * Debe llamarse una vez que la interfaz WiFi STA ya esté conectada
 * (WiFi.status() == WL_CONNECTED).
 *
 * @param deviceId  ID único del gateway que se reportará en /status.
 * @param isPaired  true si el gateway ya tiene clave AES almacenada en NVS.
 */
void http_init(const String &deviceId, bool isPaired);

/**
 * @brief Actualiza el estado "paired" que reporta /status.
 *
 * Esto permite que otras partes del programa informen cuando el proceso
 * de emparejamiento se complete correctamente.
 *
 * @param paired  true si el dispositivo está emparejado.
 */
void http_set_paired(bool paired);

/**
 * @brief Atiende peticiones HTTP.
 *
 * Debe llamarse periódicamente desde loop().
 * Solo procesa peticiones si el servidor HTTP fue iniciado correctamente
 * mediante http_init() y la WiFi sigue conectada.
 */
void http_loop();

/**
 * Guarda la última telemetría recibida por LoRa para exponerla vía /telemetry.
 */
void http_set_last_telemetry(uint64_t timestamp, int retry, const String &deviceId, const String &payload, int rssi, float snr);
