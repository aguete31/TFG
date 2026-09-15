#include "pairing_service.h"

#include <Preferences.h>
#include <cstring>

#include "crypto.h"
#include "iv_generator.h"

// =========================================================
// ESTADO INTERNO
// =========================================================
static pair_callback_t g_callback = nullptr;

#ifdef DEVICE_GATEWAY
static volatile PairState g_state = PAIR_IDLE;
#endif

#ifdef DEVICE_HELTEC
volatile PairStatus pairStatus = IDLE;
#endif

// =========================================================
// TAREA DE PAIRING
// =========================================================
static void pairTask(void *param)
{
  char *pw = static_cast<char *>(param);

  Serial.println("PAIRING: starting derivation");

#ifdef DEVICE_GATEWAY
  g_state = PAIR_BUSY;
#endif

  String password(pw ? pw : "");

  bool ok = derive_key_from_password(password, AES_KEY, AES_KEY_SIZE);

  // -------------------------------------------------------
  // Crear nuevo espacio IV para la nueva clave
  // -------------------------------------------------------
  if (ok)
  {
    if (!initializeIvForNewKey())
    {
      Serial.println("PAIRING: failed to initialize IV epoch");

      clear_aes_key();
      ok = false;
    }
  }

  // -------------------------------------------------------
  // Borrar contraseña temporal
  // -------------------------------------------------------
  if (pw)
  {
    size_t len = strlen(pw);

    memset(pw, 0, len);
    free(pw);
  }

  password = String();

  // -------------------------------------------------------
  // Guardar clave
  // -------------------------------------------------------
  if (ok)
  {
    if (!store_device_key_in_nvs(AES_KEY, AES_KEY_SIZE))
    {
      Serial.println("PAIRING: failed to store device key");

      clear_aes_key();
      ok = false;
    }
  }

  // -------------------------------------------------------
  // Estado final
  // -------------------------------------------------------
  if (ok)
  {
#ifdef DEVICE_GATEWAY

    Preferences prefs;

    if (prefs.begin("lora_proto", false))
    {
      prefs.putBool("paired", true);
      prefs.end();
    }
    else
    {
      Serial.println("PAIRING: failed to open lora_proto NVS");

      clear_aes_key();
      ok = false;
    }

    if (ok)
      g_state = PAIR_SUCCESS;

#endif
  }

  if (!ok)
  {
#ifdef DEVICE_GATEWAY
    g_state = PAIR_FAILED;
#endif

#ifdef DEVICE_HELTEC
    pairStatus = FAILED;
#endif

    Serial.println("PAIRING: derivation/store FAILED");
  }
  else
  {
    Serial.println("PAIRING: derivation + store OK");
  }

  // -------------------------------------------------------
  // Callback
  // -------------------------------------------------------
  if (g_callback)
  {
    g_callback(ok);
  }

  vTaskDelete(nullptr);
}

// =========================================================
// API
// =========================================================
void pairingServiceSetCallback(pair_callback_t cb)
{
  g_callback = cb;
}


PairingStartResult pairingServiceStart(const String &passwordIn)
{
  String password = passwordIn;

  password.trim();

  if (password.length() < 8)
  {
    return PairingStartResult::INVALID_PASSWORD;
  }

#ifdef DEVICE_GATEWAY

  if (g_state == PAIR_BUSY || g_state == PAIR_PENDING)
  {
    return PairingStartResult::BUSY;
  }

#endif

#ifdef DEVICE_HELTEC

  if (pairStatus == PAIRING)
  {
    return PairingStartResult::BUSY;
  }

#endif

  char *passwordBuffer = static_cast<char *>(malloc(password.length() + 1));

  if (!passwordBuffer)
  {
    pairingServiceMarkFailed();
    return PairingStartResult::ALLOC_ERROR;
  }

  strcpy(passwordBuffer, password.c_str());

#ifdef DEVICE_GATEWAY
  g_state = PAIR_PENDING;
#endif

#ifdef DEVICE_HELTEC
  pairStatus = PAIRING;
#endif

  BaseType_t result = xTaskCreatePinnedToCore(pairTask, "pairTask", 8192, passwordBuffer, 1, nullptr, 1);

  if (result != pdPASS)
  {
    memset(passwordBuffer, 0, password.length());
    free(passwordBuffer);
    pairingServiceMarkFailed();

    return PairingStartResult::TASK_ERROR;
  }

  return PairingStartResult::STARTED;
}


PairState pairingServiceGetState()
{
#ifdef DEVICE_GATEWAY
  return g_state;
#else
  return PAIR_IDLE;
#endif
}


const char *pairingServiceStateToString(PairState state)
{
  switch (state)
  {
  case PAIR_IDLE:
    return "idle";

  case PAIR_PENDING:
    return "pending";

  case PAIR_BUSY:
    return "busy";

  case PAIR_SUCCESS:
    return "success";

  case PAIR_FAILED:
    return "failed";
  }

  return "unknown";
}


bool pairingServiceIsPaired()
{
#ifdef DEVICE_GATEWAY

  Preferences prefs;

  if (!prefs.begin("lora_proto", true))
  {
    return false;
  }

  bool paired = false;

  if (prefs.isKey("paired"))
  {
    paired = prefs.getBool("paired", false);
  }

  prefs.end();
  return paired;

#else
  return pairStatus == PAIRED;
#endif
}


void pairingServiceMarkFailed()
{
#ifdef DEVICE_GATEWAY
  g_state = PAIR_FAILED;
#endif

#ifdef DEVICE_HELTEC
  pairStatus = FAILED;
#endif
}


void pairingServiceResetRuntimeState()
{
#ifdef DEVICE_GATEWAY
  g_state = PAIR_IDLE;
#endif
}