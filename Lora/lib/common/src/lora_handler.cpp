#include "lora_handler.h"
#include <LoRa.h>
#include "http_server.h"
#include "device_id.h"
#include <protocol.h>
#include "telemetry_buffer.h"
#include "gateway_time.h"

// ==================== Gestión de duplicados LoRa ====================

#define DUPLICATE_BUFFER 10

struct SeqEntry
{
  String deviceId;
  uint32_t seq;
};

static SeqEntry lastSeqReceived[DUPLICATE_BUFFER];
static uint8_t seqIndex = 0;
static String g_device_id;

static bool isDuplicate(const String &deviceId, uint32_t seq)
{
  for (int i = 0; i < DUPLICATE_BUFFER; i++)
  {
    if (lastSeqReceived[i].seq == seq &&
        lastSeqReceived[i].deviceId == deviceId)
    {
      return true;
    }
  }
  return false;
}

static void addToBuffer(const String &deviceId, uint32_t seq)
{
  lastSeqReceived[seqIndex].deviceId = deviceId;
  lastSeqReceived[seqIndex].seq = seq;
  seqIndex = (seqIndex + 1) % DUPLICATE_BUFFER;
}

// ==================== Gestión de ACK LoRa ====================

static void sendAck(uint32_t seq)
{
  String jsonStr = LoRaProtocol::createAckMessage(seq, g_device_id);

  LoRa.beginPacket();
  LoRa.print(jsonStr);
  LoRa.endPacket();

  Serial.printf("ACK sent [%u] (%d bytes)\n", seq, jsonStr.length());
}

// ==================== API ====================

void lora_handler_init(const String &deviceId)
{
  g_device_id = deviceId;
}

void lora_handle_message(const String &msgStr, int rssi, float snr)
{
  LoRaMessage msg;

  if (!LoRaProtocol::parseMessage(msgStr, msg))
  {
    // Mensaje no válido
    return;
  }

  Serial.printf("Message from deviceId: %s\n", msg.deviceId.c_str());

  if (isDuplicate(msg.deviceId, msg.seq))
  {
    Serial.printf("DUPLICATE [%u] from %s: resending ACK\n", msg.seq, msg.deviceId.c_str());

    sendAck(msg.seq);
    return;
  }

  addToBuffer(msg.deviceId, msg.seq);

  Serial.printf("RX [%u] retry=%u from deviceId=%s: %s | RSSI=%d SNR=%.1f\n", msg.seq, msg.retry, msg.deviceId.c_str(), msg.payload.c_str(), rssi, snr);
  sendAck(msg.seq);

  // Almacén de datos para caso de HA apagado
  TelemetryData data;
  data.timestamp = gateway_time_now();
  data.deviceId = msg.deviceId;
  data.payload = msg.payload;
  data.rssi = rssi;
  data.snr = snr;

  saveTelemetryData(data);

  // Último dato para respuesta simple de /telemetry (sin from_ts)
  http_set_last_telemetry(data.timestamp, msg.retry, msg.deviceId, msg.payload, rssi, snr);
}