#pragma once

#include <Arduino.h>

/**
 * @brief Inicializa un nuevo espacio de IV para una nueva clave AES.
 *
 * Genera un epoch aleatorio de 64 bits, lo guarda en NVS
 * y reinicia el contador de IV.
 *
 * Debe llamarse al realizar un nuevo emparejamiento.
 *
 * @return true si el nuevo estado IV se almacenó correctamente.
 */
bool initializeIvForNewKey();

/**
 * @brief Genera un IV único de 12 bytes para AES-GCM.
 *
 * Formato:
 *   bytes 0-7  : epoch aleatorio asociado al pairing
 *   bytes 8-11 : contador monotónico
 *
 * @param iv_out Buffer de salida de 12 bytes.
 * @return true si el IV se generó correctamente.
 */
bool generateSecureIV(uint8_t *iv_out);