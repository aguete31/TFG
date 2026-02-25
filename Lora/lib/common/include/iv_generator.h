#pragma once
#include <Arduino.h>

/**
 * @brief Genera un IV (Initialization Vector) seguro para AES-GCM.
 * 
 * El IV se compone de:
 *  - ID único del dispositivo (4 bytes)
 *  - Contador incremental persistente (8 bytes)
 * Esto garantiza unicidad y seguridad criptográfica.
 * 
 * @param iv_out Buffer de salida donde se escribirá el IV (12 bytes)
 */
void generateSecureIV(uint8_t* iv_out); 