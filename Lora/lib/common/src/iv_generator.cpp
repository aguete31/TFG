#include "iv_generator.h"

#include <Preferences.h>
#include <esp_system.h>
#include <string.h>

// Namespace NVS del estado del protocolo
static const char *NVS_NAMESPACE = "lora_proto";

// Estado persistente del nuevo esquema IV
static const char *NVS_KEY_EPOCH = "iv_epoch";
static const char *NVS_KEY_COUNTER = "iv_cnt32";

// Reservamos contadores por bloques para reducir escrituras en flash
static constexpr uint32_t IV_COUNTER_BLOCK_SIZE = 1024;

// Epoch activo en RAM
static uint8_t iv_epoch[8] = {0};
static bool iv_epoch_ready = false;

// Estado del bloque de contadores reservado en RAM
static uint32_t next_iv_counter = 0;
static uint32_t iv_counter_end = 0;
static bool iv_block_ready = false;


/**
 * @brief Carga el epoch IV persistente desde NVS.
 */
static bool load_iv_epoch()
{
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);

  if (!prefs.isKey(NVS_KEY_EPOCH))
  {
    prefs.end();
    return false;
  }

  uint8_t tmp[sizeof(iv_epoch)];

  size_t r = prefs.getBytes(NVS_KEY_EPOCH, tmp, sizeof(tmp));
  prefs.end();

  if (r != sizeof(tmp))
  {
    return false;
  }

  memcpy(iv_epoch, tmp, sizeof(iv_epoch));
  iv_epoch_ready = true;

  return true;
}


/**
 * @brief Lee el límite superior de contador reservado.
 */
static uint32_t read_iv_counter()
{
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);

  uint32_t counter = prefs.getUInt(NVS_KEY_COUNTER, 0);

  prefs.end();
  return counter;
}


/**
 * @brief Guarda el límite superior reservado del contador.
 */
static bool write_iv_counter(uint32_t counter)
{
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);

  size_t written = prefs.putUInt(NVS_KEY_COUNTER, counter);
  prefs.end();

  return written == sizeof(counter);
}


/**
 * @brief Inicializa un nuevo espacio de IV para una nueva clave.
 */
bool initializeIvForNewKey()
{
  uint8_t newEpoch[sizeof(iv_epoch)];

  // 64 bits aleatorios
  esp_fill_random(newEpoch, sizeof(newEpoch));

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);

  /*
   * IMPORTANTE:
   * Primero escribimos el nuevo epoch y DESPUÉS
   * reiniciamos el contador.
   *
   * Si se corta la alimentación entre ambas escrituras,
   * tendremos epoch nuevo + contador antiguo, que sigue
   * siendo seguro.
   *
   * Nunca queremos contador reiniciado + epoch antiguo.
   */
  size_t epochWritten = prefs.putBytes(NVS_KEY_EPOCH, newEpoch, sizeof(newEpoch));

  if (epochWritten != sizeof(newEpoch))
  {
    prefs.end();
    Serial.println("IV: failed to store new epoch");
    return false;
  }

  size_t counterWritten = prefs.putUInt(NVS_KEY_COUNTER, 0);
  prefs.end();

  if (counterWritten != sizeof(uint32_t))
  {
    Serial.println("IV: failed to reset counter");
    return false;
  }

  // Activar nuevo estado en RAM
  memcpy(iv_epoch, newEpoch, sizeof(iv_epoch));

  iv_epoch_ready = true;

  next_iv_counter = 0;
  iv_counter_end = 0;
  iv_block_ready = false;

  Serial.println("IV: new epoch initialized");
  return true;
}


/**
 * @brief Reserva un nuevo bloque de contadores.
 *
 * El límite superior se persiste ANTES de utilizar
 * cualquiera de los contadores del bloque.
 */
static bool reserve_iv_counter_block()
{
  uint32_t previousEnd = read_iv_counter();

  if (previousEnd > UINT32_MAX - IV_COUNTER_BLOCK_SIZE)
  {
    Serial.println("IV counter exhausted");
    return false;
  }

  uint32_t newEnd = previousEnd + IV_COUNTER_BLOCK_SIZE;

  // Persistir primero todo el bloque reservado
  if (!write_iv_counter(newEnd))
  {
    Serial.println("Failed to reserve IV counter block");
    return false;
  }

  next_iv_counter = previousEnd + 1;
  iv_counter_end = newEnd;
  iv_block_ready = true;

  Serial.printf("IV block reserved: %lu - %lu\n", (unsigned long)next_iv_counter, (unsigned long)iv_counter_end);
  return true;
}


/**
 * @brief Genera un IV único de 96 bits para AES-GCM.
 */
bool generateSecureIV(uint8_t *iv_out)
{
  if (iv_out == nullptr)
  {
    return false;
  }

  // Cargar el epoch persistente si todavía no está en RAM
  if (!iv_epoch_ready)
  {
    if (!load_iv_epoch())
    {
      /*
       * Compatibilidad/migración:
       * si todavía no existe epoch, crear uno nuevo.
       */
      Serial.println("IV: no epoch found, creating one");

      if (!initializeIvForNewKey())
      {
        return false;
      }
    }
  }

  // Reservar bloque si hace falta
  if (!iv_block_ready || next_iv_counter > iv_counter_end)
  {
    if (!reserve_iv_counter_block())
    {
      return false;
    }
  }

  uint32_t counter = next_iv_counter++;

  // Bytes 0-7: epoch aleatorio
  memcpy(&iv_out[0], iv_epoch, sizeof(iv_epoch));

  // Bytes 8-11: contador big-endian
  iv_out[8] = (uint8_t)((counter >> 24) & 0xFF);
  iv_out[9] = (uint8_t)((counter >> 16) & 0xFF);
  iv_out[10] = (uint8_t)((counter >> 8) & 0xFF);
  iv_out[11] = (uint8_t)(counter & 0xFF);

  return true;
}