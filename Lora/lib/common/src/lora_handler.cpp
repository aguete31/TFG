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
  uint8_t packet[LORA_MAX_PACKET_SIZE];
  size_t packetLen = 0;

  bool ok = LoRaProtocol::createAckMessageBinary(seq, g_device_id, packet, sizeof(packet), packetLen);

  if (!ok)
  {
    Serial.println("ACK cancelado: no se pudo construir ACK binario");
    return;
  }

  LoRa.beginPacket();
  LoRa.write(packet, packetLen);

  int result = LoRa.endPacket();

  if (result != 1)
  {
    Serial.println("ACK binario: error en LoRa.endPacket()");
    return;
  }

  Serial.printf("ACK BIN sent [%lu] (%u bytes)\n", (unsigned long)seq, (unsigned int)packetLen);
}

// ==================== API ====================

void lora_handler_init(const String &deviceId)
{
  g_device_id = deviceId;
}


void lora_handle_binary_packet(const uint8_t *packet, size_t packetLen, int rssi, float snr)
{
  LoRaMessage msg;

  if (!LoRaProtocol::parseMessageBinary(packet, packetLen, msg))
  {
    // Trama binaria inválida
    return;
  }

  Serial.printf("Message BIN from deviceId: %s\n", msg.deviceId.c_str());

  if (isDuplicate(msg.deviceId, msg.seq))
  {
    Serial.printf("DUPLICATE BIN [%lu] from %s\n", (unsigned long)msg.seq, msg.deviceId.c_str());

    // De momento seguimos enviando ACK JSON
    sendAck(msg.seq);
    return;
  }

  addToBuffer(msg.deviceId,msg.seq);

  Serial.printf("RX BIN [%lu] from deviceId=%s: %s | RSSI=%d SNR=%.1f\n", (unsigned long)msg.seq, msg.deviceId.c_str(), msg.payload.c_str(), rssi, snr);

  // De momento el ACK real sigue siendo JSON
  sendAck(msg.seq);

  // Guardar telemetría exactamente igual que con JSON
  TelemetryData data;
  data.timestamp = gateway_time_now();
  data.deviceId = msg.deviceId;
  data.payload = msg.payload;
  data.rssi = rssi;
  data.snr = snr;

  saveTelemetryData(data);

  // En el protocolo binario ya no existe retry.
  // Usamos 0 mientras esta API antigua siga esperando ese campo.
  http_set_last_telemetry(
      data.timestamp,
      0,
      msg.deviceId,
      msg.payload,
      rssi,
      snr);
}