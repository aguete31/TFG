#include "telemetry_buffer.h"

// ==================== Estado interno (solo RAM) ====================

static TelemetryData g_buffer[MAX_TELEMETRY_ENTRIES];
static int g_startIndex = 0;  // índice del dato más antiguo
static int g_count = 0;       // nº de elementos válidos
static uint64_t g_lastTs = 0; // último timestamp guardado

// ==================== API ====================

bool initTelemetryBuffer()
{
    g_startIndex = 0;
    g_count = 0;
    g_lastTs = 0;
    return true;
}

void saveTelemetryData(const TelemetryData &dataIn)
{
    TelemetryData data = dataIn;

    int writeIndex;
    if (g_count < MAX_TELEMETRY_ENTRIES)
    {
        writeIndex = (g_startIndex + g_count) % MAX_TELEMETRY_ENTRIES;
        g_count++;
    }
    else
    {
        // buffer lleno → sobrescribimos el más antiguo
        writeIndex = g_startIndex;
        g_startIndex = (g_startIndex + 1) % MAX_TELEMETRY_ENTRIES;
    }

    g_buffer[writeIndex] = data;
    if (data.timestamp > g_lastTs)
    {
        g_lastTs = data.timestamp;
    }
}

std::vector<TelemetryData> getTelemetryDataFromTs(uint64_t fromTs, int limit)
{
    std::vector<TelemetryData> result;

    if (g_count == 0)
        return result;

    if (limit > MAX_ENTRIES_PER_REQUEST)
        limit = MAX_ENTRIES_PER_REQUEST;

    for (int i = 0; i < g_count && (int)result.size() < limit; i++)
    {
        int idx = (g_startIndex + i) % MAX_TELEMETRY_ENTRIES;
        const TelemetryData &d = g_buffer[idx];

        if (d.timestamp >= fromTs)
        {
            result.push_back(d);
        }
    }

    return result;
}

uint64_t getLatestTs()
{
    return g_lastTs;
}

// ==================== JSON ====================

String telemetryDataToJson(const TelemetryData &data)
{
    String json = "{";
    json += "\"timestamp\":" + String((unsigned long long)data.timestamp) + ",";
    json += "\"device_id\":\"" + data.deviceId + "\",";
    json += "\"payload\":" + data.payload + ",";
    json += "\"rssi\":" + String(data.rssi) + ",";
    json += "\"snr\":" + String(data.snr);
    json += "}";
    return json;
}

String telemetryArrayToJson(const std::vector<TelemetryData> &dataArray)
{
    String json = "[";
    for (size_t i = 0; i < dataArray.size(); i++)
    {
        json += telemetryDataToJson(dataArray[i]);
        if (i + 1 < dataArray.size())
            json += ",";
    }
    json += "]";
    return json;
}