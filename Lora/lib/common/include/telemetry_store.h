#pragma once

#include <Arduino.h>
#include <stdint.h>

static constexpr size_t MAX_TELEMETRY_DEVICES = 10;
static constexpr size_t MAX_TELEMETRY_EVENTS  = 20;

struct TelemetryLimits
{
    float tempMin;
    float tempMax;

    float humidityMin;
    float humidityMax;

    float tempHysteresis;
    float humidityHysteresis;
};

struct TelemetryState
{
    bool used;

    String deviceId;

    uint32_t seq;
    uint64_t timestamp;

    float temperature;
    float humidity;

    bool hasTemperature;
    bool hasHumidity;

    int rssi;
    float snr;

    // MQTT deberá publicar este estado cuando sea true
    bool pendingPublish;

    // Estado de alarmas para evitar eventos repetidos
    bool highTempActive;
    bool lowTempActive;
    bool highHumidityActive;
    bool lowHumidityActive;
};

enum class TelemetryEventType
{
    HIGH_TEMP_START,
    HIGH_TEMP_END,

    LOW_TEMP_START,
    LOW_TEMP_END,

    HIGH_HUMIDITY_START,
    HIGH_HUMIDITY_END,

    LOW_HUMIDITY_START,
    LOW_HUMIDITY_END
};

struct TelemetryEvent
{
    String deviceId;
    TelemetryEventType type;

    float value;
    float threshold;

    uint64_t timestamp;
};


// Inicialización
bool telemetryStoreInit();

// Entrada principal desde LoRa
bool telemetryStoreUpdate(const String &deviceId, uint32_t seq, const String &payload, int rssi, float snr, uint64_t timestamp);

// Estados pendientes para MQTT
size_t telemetryStoreDeviceCount();

bool telemetryStoreGetDevice(size_t index, TelemetryState &out);

bool telemetryStoreMarkPublished(const String &deviceId, uint32_t seq);

// Eventos pendientes para MQTT
size_t telemetryStoreEventCount();

bool telemetryStorePeekEvent(TelemetryEvent &out);

bool telemetryStorePopEvent();


// Configuración de límites
void telemetryStoreSetLimits(const TelemetryLimits &limits);

TelemetryLimits telemetryStoreGetLimits();

// Utilidad para logs / MQTT
const char *telemetryEventTypeToString(TelemetryEventType type);