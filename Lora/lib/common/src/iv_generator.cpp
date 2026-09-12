#include "iv_generator.h"
#include <Preferences.h>
#include <esp_system.h>
#include "crypto.h"

// Namespace y clave para almacenamiento persistente del contador
static const char *NVS_NAMESPACE = "lora_proto";
static const char *NVS_KEY_COUNTER = "iv_cnt";

// Número de IV que se reservan con cada escritura en NVS
static constexpr uint64_t IV_COUNTER_BLOCK_SIZE = 1024;

// Estado del bloque actualmente reservado en RAM
static uint64_t next_iv_counter = 0;
static uint64_t iv_counter_end = 0;
static bool iv_block_ready = false;

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



/**
 * @brief Reserva un nuevo bloque de contadores IV.
 *
 * NVS almacena el límite superior ya reservado.
 * El nuevo bloque se persiste ANTES de utilizar ninguno
 * de sus contadores.
 */
static bool reserve_iv_counter_block()
{
  uint64_t previous_end = read_iv_counter();

  // Comprobar overflow antes de sumar
  if (previous_end > UINT64_MAX - IV_COUNTER_BLOCK_SIZE)
  {
    Serial.println("IV counter exhausted");
    return false;
  }

  uint64_t new_end = previous_end + IV_COUNTER_BLOCK_SIZE;

  // Primero persistimos la reserva completa.
  // Si el dispositivo se apaga después de este punto,
  // al reiniciar saltará al siguiente bloque.
  if (!write_iv_counter(new_end))
  {
    Serial.println("Failed to reserve IV counter block");
    return false;
  }

  next_iv_counter = previous_end + 1;
  iv_counter_end = new_end;
  iv_block_ready = true;

  Serial.printf("IV block reserved: %llu - %llu\n", (unsigned long long)next_iv_counter, (unsigned long long)iv_counter_end);

  return true;
}





/**
 * @brief Genera un IV único combinando ID de dispositivo y contador persistente.
 */
bool generateSecureIV(uint8_t *iv_out)
{
  if (iv_out == nullptr)
  {
    return false;
  }

  // Si todavía no tenemos bloque o se ha agotado,
  // reservar uno nuevo.
  if (!iv_block_ready ||
      next_iv_counter > iv_counter_end)
  {
    if (!reserve_iv_counter_block())
    {
      return false;
    }
  }

  // Consumir el siguiente contador del bloque
  uint64_t counter = next_iv_counter++;

  // Identificador estable del ESP32
  uint64_t chipId = ESP.getEfuseMac();
  uint32_t device_id = (uint32_t)(chipId & 0xFFFFFFFFULL);

  // -----------------------------------------------------
  // Bytes 0-3: identificador del dispositivo
  // -----------------------------------------------------

  iv_out[0] = (uint8_t)((device_id >> 24) & 0xFF);
  iv_out[1] = (uint8_t)((device_id >> 16) & 0xFF);
  iv_out[2] = (uint8_t)((device_id >> 8) & 0xFF);
  iv_out[3] = (uint8_t)(device_id & 0xFF);

  // -----------------------------------------------------
  // Bytes 4-11: contador monotónico
  // -----------------------------------------------------

  iv_out[4] = (uint8_t)((counter >> 56) & 0xFF);
  iv_out[5] = (uint8_t)((counter >> 48) & 0xFF);
  iv_out[6] = (uint8_t)((counter >> 40) & 0xFF);
  iv_out[7] = (uint8_t)((counter >> 32) & 0xFF);
  iv_out[8] = (uint8_t)((counter >> 24) & 0xFF);
  iv_out[9] = (uint8_t)((counter >> 16) & 0xFF);
  iv_out[10] = (uint8_t)((counter >> 8) & 0xFF);
  iv_out[11] = (uint8_t)(counter & 0xFF);

  return true;
}