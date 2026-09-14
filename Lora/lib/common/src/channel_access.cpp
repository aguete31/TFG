#include "channel_access.h"

#include <LoRa.h>
#include <esp_system.h>

static constexpr uint8_t MAX_CAD_ATTEMPTS = 3;
static constexpr uint32_t CAD_TIMEOUT_MS = 100;

static volatile bool cadFinished = false;
static volatile bool cadBusy = false;

static void onCadDone(boolean signalDetected)
{
  cadBusy = signalDetected;
  cadFinished = true;
}

static uint32_t randomBackoff(uint8_t attempt)
{
  uint32_t minMs;
  uint32_t maxMs;

  switch (attempt)
  {
  case 0:
    minMs = 100;
    maxMs = 500;
    break;

  case 1:
    minMs = 200;
    maxMs = 1000;
    break;

  default:
    minMs = 400;
    maxMs = 2000;
    break;
  }

  uint32_t range = maxMs - minMs + 1;
  return minMs + (esp_random() % range);
}

bool waitForFreeChannel()
{
  LoRa.onCadDone(onCadDone);

  for (uint8_t attempt = 0; attempt < MAX_CAD_ATTEMPTS; ++attempt)
  {
    cadFinished = false;
    cadBusy = false;

    // Asegurar un estado conocido antes de iniciar CAD.
    // Especialmente importante después de RX_SINGLE / ACK timeout.
    LoRa.idle();
    delay(2);

    LoRa.channelActivityDetection();
    uint32_t startedAt = millis();

    while (!cadFinished)
    {
      if ((millis() - startedAt) >= CAD_TIMEOUT_MS)
      {
        Serial.printf("CAD: timeout intento=%u/%u\n", attempt + 1, MAX_CAD_ATTEMPTS);

        // Recuperar el transceptor antes de repetir CAD
        LoRa.idle();
        delay(2);

        break;
      }
      delay(1);
    }

    // CAD terminó correctamente
    if (cadFinished)
    {
      if (!cadBusy)
      {
        Serial.printf("CAD: canal libre intento=%u\n", attempt + 1);
        return true;
      }

      uint32_t backoffMs = randomBackoff(attempt);

      Serial.printf("CAD: canal ocupado intento=%u/%u backoff=%lu ms\n", attempt + 1, MAX_CAD_ATTEMPTS, (unsigned long)backoffMs);

      LoRa.idle();
      delay(backoffMs);
    }
  }

  Serial.println("CAD: no se pudo obtener canal libre");
  LoRa.idle();

  return false;
}