#include "telemetry_store.h"

#include <ArduinoJson.h>

// =========================================================
// ESTADO INTERNO
// =========================================================
static TelemetryState g_devices[MAX_TELEMETRY_DEVICES];
static TelemetryEvent g_events[MAX_TELEMETRY_EVENTS];

static size_t g_eventStart = 0;
static size_t g_eventCount = 0;

// =========================================================
// LÍMITES INICIALES
//
// NO representan todavía un estándar universal.
// Son valores iniciales configurables para validar
// el funcionamiento.
//
// Más adelante podrán configurarse desde el Gateway.
// =========================================================

static TelemetryLimits g_limits = {
    5.0f,   // tempMin
    40.0f,  // tempMax

    20.0f,  // humidityMin
    80.0f,  // humidityMax

    1.0f,   // tempHysteresis
    5.0f    // humidityHysteresis
};


// =========================================================
// FUNCIONES INTERNAS
// =========================================================
static TelemetryState *findDevice(const String &deviceId)
{
    for (size_t i = 0; i < MAX_TELEMETRY_DEVICES; i++)
    {
        if (g_devices[i].used && g_devices[i].deviceId == deviceId)
        {
            return &g_devices[i];
        }
    }

    return nullptr;
}


static TelemetryState *createDevice(const String &deviceId)
{
    for (size_t i = 0; i < MAX_TELEMETRY_DEVICES; i++)
    {
        if (!g_devices[i].used)
        {
            TelemetryState &state = g_devices[i];

            state = TelemetryState{};

            state.used = true;
            state.deviceId = deviceId;

            Serial.printf("TELEMETRY: nuevo dispositivo %s\n",deviceId.c_str());

            return &state;
        }
    }

    Serial.println("TELEMETRY: máximo de dispositivos alcanzado");
    return nullptr;
}


static TelemetryState *findOrCreateDevice(const String &deviceId)
{
    TelemetryState *state = findDevice(deviceId);

    if (state)
        return state;

    return createDevice(deviceId);
}


static void pushEvent(const String &deviceId, TelemetryEventType type, float value, float threshold, uint64_t timestamp)
{
    size_t writeIndex;

    if (g_eventCount < MAX_TELEMETRY_EVENTS)
    {
        writeIndex = (g_eventStart + g_eventCount) % MAX_TELEMETRY_EVENTS;
        g_eventCount++;
    }
    else
    {
        // Cola llena:
        // descartamos el evento más antiguo y conservamos
        // los eventos más recientes.
        writeIndex = g_eventStart;

        g_eventStart = (g_eventStart + 1) % MAX_TELEMETRY_EVENTS;

        Serial.println("TELEMETRY: event queue llena, descartando evento más antiguo");
    }

    TelemetryEvent &event = g_events[writeIndex];

    event.deviceId = deviceId;
    event.type = type;
    event.value = value;
    event.threshold = threshold;
    event.timestamp = timestamp;

    Serial.printf("TELEMETRY EVENT: %s device=%s value=%.2f threshold=%.2f\n", telemetryEventTypeToString(type), deviceId.c_str(), value, threshold);
}


// =========================================================
// DETECCIÓN DE TEMPERATURA
// =========================================================
static void evaluateTemperature(TelemetryState &state, float value, uint64_t timestamp)
{
    // ---------- HIGH ----------
    if (!state.highTempActive && value > g_limits.tempMax)
    {
        state.highTempActive = true;

        pushEvent(state.deviceId, TelemetryEventType::HIGH_TEMP_START, value, g_limits.tempMax, timestamp);
    }

    else if (state.highTempActive && value <= (g_limits.tempMax - g_limits.tempHysteresis))
    {
        state.highTempActive = false;
        pushEvent(state.deviceId, TelemetryEventType::HIGH_TEMP_END, value, g_limits.tempMax, timestamp);
    }

    // ---------- LOW ----------
    if (!state.lowTempActive && value < g_limits.tempMin)
    {
        state.lowTempActive = true;
        pushEvent(state.deviceId, TelemetryEventType::LOW_TEMP_START, value, g_limits.tempMin, timestamp);
    }

    else if (state.lowTempActive && value >= (g_limits.tempMin + g_limits.tempHysteresis))
    {
        state.lowTempActive = false;
        pushEvent(state.deviceId, TelemetryEventType::LOW_TEMP_END, value, g_limits.tempMin, timestamp);
    }
}


// =========================================================
// DETECCIÓN DE HUMEDAD
// =========================================================
static void evaluateHumidity(TelemetryState &state, float value, uint64_t timestamp)
{
    // ---------- HIGH ----------
    if (!state.highHumidityActive && value > g_limits.humidityMax)
    {
        state.highHumidityActive = true;
        pushEvent(state.deviceId, TelemetryEventType::HIGH_HUMIDITY_START, value, g_limits.humidityMax, timestamp);
    }

    else if (state.highHumidityActive && value <= (g_limits.humidityMax - g_limits.humidityHysteresis))
    {
        state.highHumidityActive = false;
        pushEvent(state.deviceId, TelemetryEventType::HIGH_HUMIDITY_END, value, g_limits.humidityMax, timestamp);
    }

    // ---------- LOW ----------
    if (!state.lowHumidityActive && value < g_limits.humidityMin)
    {
        state.lowHumidityActive = true;
        pushEvent(state.deviceId, TelemetryEventType::LOW_HUMIDITY_START, value, g_limits.humidityMin, timestamp);
    }

    else if (state.lowHumidityActive && value >= (g_limits.humidityMin + g_limits.humidityHysteresis))
    {
        state.lowHumidityActive = false;
        pushEvent(state.deviceId, TelemetryEventType::LOW_HUMIDITY_END, value, g_limits.humidityMin, timestamp);
    }
}


// =========================================================
// API
// =========================================================
bool telemetryStoreInit()
{
    for (size_t i = 0; i < MAX_TELEMETRY_DEVICES; i++)
    {
        g_devices[i] = TelemetryState{};
    }

    g_eventStart = 0;
    g_eventCount = 0;

    Serial.println("TELEMETRY STORE: initialized");
    return true;
}


bool telemetryStoreUpdate(const String &deviceId, uint32_t seq, const String &payload, int rssi, float snr, uint64_t timestamp)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err)
    {
        Serial.printf("TELEMETRY: JSON inválido device=%s error=%s\n", deviceId.c_str(), err.c_str());
        return false;
    }

    TelemetryState *state = findOrCreateDevice(deviceId);

    if (!state)
        return false;

    state->seq = seq;
    state->timestamp = timestamp;

    state->rssi = rssi;
    state->snr = snr;

    // =====================================================
    // TEMPERATURA
    // =====================================================

    if (!doc["temp"].isNull())
    {
        float value = doc["temp"].as<float>();

        state->temperature = value;
        state->hasTemperature = true;

        evaluateTemperature(*state, value, timestamp);
    }
    else
    {
        state->hasTemperature = false;
    }

    // =====================================================
    // HUMEDAD
    // =====================================================
    if (!doc["hum"].isNull())
    {
        float value = doc["hum"].as<float>();

        state->humidity = value;
        state->hasHumidity = true;

        evaluateHumidity(*state, value, timestamp);
    }
    else
    {
        state->hasHumidity = false;
    }

    // MQTT deberá publicar este último estado
    state->pendingPublish = true;

    Serial.printf("TELEMETRY STATE: device=%s seq=%lu temp=%.2f hum=%.2f RSSI=%d SNR=%.1f\n", deviceId.c_str(), (unsigned long)seq, state->temperature, state->humidity, rssi, snr);
    return true;
}


size_t telemetryStoreDeviceCount()
{
    size_t count = 0;

    for (size_t i = 0; i < MAX_TELEMETRY_DEVICES; i++)
    {
        if (g_devices[i].used)
            count++;
    }

    return count;
}


bool telemetryStoreGetDevice(size_t index, TelemetryState &out)
{
    size_t current = 0;

    for (size_t i = 0; i < MAX_TELEMETRY_DEVICES; i++)
    {
        if (!g_devices[i].used)
            continue;

        if (current == index)
        {
            out = g_devices[i];
            return true;
        }

        current++;
    }

    return false;
}


bool telemetryStoreMarkPublished(const String &deviceId)
{
    TelemetryState *state = findDevice(deviceId);

    if (!state)
        return false;

    state->pendingPublish = false;

    return true;
}


size_t telemetryStoreEventCount()
{
    return g_eventCount;
}


bool telemetryStorePeekEvent(TelemetryEvent &out)
{
    if (g_eventCount == 0)
        return false;

    out = g_events[g_eventStart];

    return true;
}


bool telemetryStorePopEvent()
{
    if (g_eventCount == 0)
        return false;

    g_events[g_eventStart] = TelemetryEvent{};

    g_eventStart = (g_eventStart + 1) % MAX_TELEMETRY_EVENTS;
    g_eventCount--;

    return true;
}


void telemetryStoreSetLimits(const TelemetryLimits &limits)
{
    g_limits = limits;
}


TelemetryLimits telemetryStoreGetLimits()
{
    return g_limits;
}


const char *telemetryEventTypeToString(TelemetryEventType type)
{
    switch (type)
    {
    case TelemetryEventType::HIGH_TEMP_START:
        return "HIGH_TEMP_START";

    case TelemetryEventType::HIGH_TEMP_END:
        return "HIGH_TEMP_END";

    case TelemetryEventType::LOW_TEMP_START:
        return "LOW_TEMP_START";

    case TelemetryEventType::LOW_TEMP_END:
        return "LOW_TEMP_END";

    case TelemetryEventType::HIGH_HUMIDITY_START:
        return "HIGH_HUMIDITY_START";

    case TelemetryEventType::HIGH_HUMIDITY_END:
        return "HIGH_HUMIDITY_END";

    case TelemetryEventType::LOW_HUMIDITY_START:
        return "LOW_HUMIDITY_START";

    case TelemetryEventType::LOW_HUMIDITY_END:
        return "LOW_HUMIDITY_END";

    default:
        return "UNKNOWN";
    }
}