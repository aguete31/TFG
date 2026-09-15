#pragma once

#include <Arduino.h>
#include "pair_status.h"

// Callback al finalizar pairing
typedef void (*pair_callback_t)(bool ok);

// Estado interno del pairing del Gateway
enum PairState
{
  PAIR_IDLE,
  PAIR_PENDING,
  PAIR_BUSY,
  PAIR_SUCCESS,
  PAIR_FAILED
};

enum class PairingStartResult
{
  STARTED,
  INVALID_PASSWORD,
  BUSY,
  ALLOC_ERROR,
  TASK_ERROR
};

#ifdef DEVICE_HELTEC
extern volatile PairStatus pairStatus;
#endif

/**
 * Registra callback de finalización.
 */
void pairingServiceSetCallback(pair_callback_t cb);

/**
 * Inicia derivación/emparejamiento en una tarea RTOS.
 */
PairingStartResult pairingServiceStart(const String &password);

/**
 * Estado runtime del pairing.
 */
PairState pairingServiceGetState();

/**
 * Convierte estado a texto.
 */
const char *pairingServiceStateToString(PairState state);

/**
 * Devuelve si el Gateway está marcado como paired en NVS.
 */
bool pairingServiceIsPaired();

/**
 * Marca fallo del proceso.
 */
void pairingServiceMarkFailed();

/**
 * Reinicia únicamente el estado runtime.
 * No elimina claves ni NVS.
 */
void pairingServiceResetRuntimeState();