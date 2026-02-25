#pragma once

#include <Arduino.h>
#include <stdint.h>

/**
 * @brief Inicializa la hora del gateway vía NTP.
 *
 * Debe llamarse DESPUÉS de que la WiFi STA esté conectada.
 * Bloquea unos segundos hasta conseguir la hora o agotar reintentos.
 */
void gateway_time_init();

/**
 * @brief Indica si el gateway tiene hora real (NTP OK).
 */
bool gateway_time_is_synced();

/**
 * @brief Devuelve marca de tiempo del gateway en segundos.
 *
 * - Si hay NTP: epoch real (segundos desde 1970-01-01).
 * - Si NO hay NTP: segundos desde arranque (no real, pero monótono).
 */
uint64_t gateway_time_now();
