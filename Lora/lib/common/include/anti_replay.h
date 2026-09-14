#pragma once

#include <Arduino.h>

enum class AntiReplayResult
{
  ACCEPTED,
  REPLAY,
  INVALID_EPOCH
};

/**
 * @brief Comprueba la frescura de un paquete autenticado.
 *
 * El primer paquete tras resetear el estado establece el epoch esperado.
 * Dentro del mismo epoch solo se aceptan contadores estrictamente crecientes.
 */
AntiReplayResult antiReplayCheck(
    uint64_t epoch,
    uint32_t counter);

/**
 * @brief Borra el estado anti-replay.
 *
 * Debe llamarse cuando se completa un nuevo pairing.
 */
void antiReplayReset();