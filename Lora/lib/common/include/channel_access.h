#pragma once

#include <Arduino.h>

/**
 * @brief Espera a que el canal LoRa esté libre usando CAD.
 *
 * @return true si el canal quedó libre y se puede transmitir.
 * @return false si siguió ocupado tras todos los intentos.
 */
bool waitForFreeChannel();