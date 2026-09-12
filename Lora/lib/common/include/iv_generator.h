#pragma once
#include <Arduino.h>

/**
 * @brief Genera un IV único de 12 bytes para AES-GCM.
 *
 * Utiliza un identificador estable del dispositivo y un contador
 * persistente reservado por bloques para evitar reutilización de IV.
 *
 * @param iv_out Buffer de salida de AES_IV_SIZE bytes.
 * @return true si el IV se generó correctamente, false si no se pudo
 *         reservar un contador persistente seguro.
 */
bool generateSecureIV(uint8_t *iv_out);