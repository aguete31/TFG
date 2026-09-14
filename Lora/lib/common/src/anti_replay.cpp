#include "anti_replay.h"

static bool epoch_known = false;
static uint64_t expected_epoch = 0;
static uint32_t highest_counter = 0;

AntiReplayResult antiReplayCheck(uint64_t epoch, uint32_t counter)
{
  // Primer paquete válido de la sesión
  if (!epoch_known)
  {
    expected_epoch = epoch;
    highest_counter = counter;
    epoch_known = true;

    Serial.printf("ANTI-REPLAY: epoch learned=%016llX counter=%lu\n", (unsigned long long)epoch, (unsigned long)counter);
    return AntiReplayResult::ACCEPTED;
  }

  // Un epoch distinto no pertenece a la sesión actual
  if (epoch != expected_epoch)
  {
    Serial.printf("ANTI-REPLAY: invalid epoch recv=%016llX expected=%016llX\n", (unsigned long long)epoch, (unsigned long long)expected_epoch);
    return AntiReplayResult::INVALID_EPOCH;
  }

  // Counter viejo o repetido = replay
  if (counter <= highest_counter)
  {
    Serial.printf("ANTI-REPLAY: replay counter=%lu highest=%lu\n", (unsigned long)counter, (unsigned long)highest_counter);
    return AntiReplayResult::REPLAY;
  }

  highest_counter = counter;
  return AntiReplayResult::ACCEPTED;
}

void antiReplayReset()
{
  epoch_known = false;
  expected_epoch = 0;
  highest_counter = 0;

  Serial.println("ANTI-REPLAY: state reset");
}