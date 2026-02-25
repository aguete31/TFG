#pragma once

#include <Arduino.h>
#include <vector>
#include <stdint.h>

struct TelemetryData
{
    uint64_t timestamp;  // tiempo del GATEWAY en segundos (epoch o desde arranque)
    String   deviceId;
    String   payload;    // JSON del nodo: {"temp":..,"hum":..,"ts":..}
    int      rssi;
    float    snr;
};

static const int MAX_TELEMETRY_ENTRIES   = 100;
static const int MAX_ENTRIES_PER_REQUEST = 20;

bool initTelemetryBuffer();
void saveTelemetryData(const TelemetryData &dataIn);

// fromTs = timestamp mínimo a partir del cual devolver datos
std::vector<TelemetryData> getTelemetryDataFromTs(uint64_t fromTs, int limit);

uint64_t getLatestTs();

String telemetryDataToJson(const TelemetryData &data);
String telemetryArrayToJson(const std::vector<TelemetryData> &dataArray);
