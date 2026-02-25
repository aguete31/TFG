#pragma once
#include <Arduino.h>

// DEVICE_ID global, accesible desde otros .cpp
extern String DEVICE_ID;

// Devuelve el ID basado en la MAC
String getDeviceIdFromChip();
