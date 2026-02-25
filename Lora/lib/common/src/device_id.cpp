#include "device_id.h"
#include <esp_system.h>

String DEVICE_ID;

String getDeviceIdFromChip()
{
  uint64_t chipId = ESP.getEfuseMac();
  char idStr[13];
  sprintf(idStr, "%012llX", chipId);
  return String(idStr);
}
