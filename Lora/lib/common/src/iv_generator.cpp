#include "iv_generator.h"
#include <Preferences.h>
#include <esp_system.h>
#include "crypto.h"

// Namespace y clave para almacenamiento persistente del contador
static const char *NVS_NAMESPACE = "lora_proto";
static const char *NVS_KEY_COUNTER = "iv_cnt";

/**
 * @brief Lee el contador persistente desde NVS.
 *
 * Si no existe, devuelve 0.
 */
static uint64_t read_iv_counter()
{
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true); // modo solo lectura
  uint64_t counter = prefs.getULong64(NVS_KEY_COUNTER, 0);
  prefs.end();
  return counter;
}

/**
 * @brief Guarda el contador persistente en NVS.
 *
 * @param counter Nuevo valor del contador
 * @return true si se guardó correctamente, false en caso contrario
 */
static bool write_iv_counter(uint64_t counter)
{
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);                     // modo escritura
  size_t w = prefs.putULong64(NVS_KEY_COUNTER, counter); // guarda bytes
  prefs.end();
  return (w == sizeof(counter)); // verifica que se haya escrito todo
}

#ifndef LORA_DEVICE_ID
#warning "LORA_DEVICE_ID not defined, using esp_random() as device id"
#define LORA_DEVICE_ID ((uint32_t)esp_random())
#endif

/**
 * @brief Genera un IV único combinando ID de dispositivo y contador persistente.
 *
 * El IV tiene 12 bytes:
 *  - Bytes 0-3: ID del dispositivo (big-endian)
 *  - Bytes 4-11: Contador (big-endian)
 */
void generateSecureIV(uint8_t *iv_out)
{
  uint32_t device_id = (uint32_t)LORA_DEVICE_ID; // ID único del dispositivo
  uint64_t counter = read_iv_counter();          // obtiene contador actual
  counter++;                                     // incrementa para nuevo IV
  if (!write_iv_counter(counter))
  { // guarda nuevo valor
    Serial.println("Warning: failed to write IV counter to NVS");
  }

  // device_id big-endian (4 bytes)
  iv_out[0] = (device_id >> 24) & 0xFF;
  iv_out[1] = (device_id >> 16) & 0xFF;
  iv_out[2] = (device_id >> 8) & 0xFF;
  iv_out[3] = device_id & 0xFF;

  // counter big-endian (8 bytes)
  iv_out[4] = (counter >> 56) & 0xFF;
  iv_out[5] = (counter >> 48) & 0xFF;
  iv_out[6] = (counter >> 40) & 0xFF;
  iv_out[7] = (counter >> 32) & 0xFF;
  iv_out[8] = (counter >> 24) & 0xFF;
  iv_out[9] = (counter >> 16) & 0xFF;
  iv_out[10] = (counter >> 8) & 0xFF;
  iv_out[11] = counter & 0xFF;
}