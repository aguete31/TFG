#include "protocol.h"
#include "message_builder.h"
#include "message_parser.h"
#include "hex_utils.h"


void writeUint32BE(uint8_t *buffer, uint32_t value)
{
  buffer[0] = (uint8_t)((value >> 24) & 0xFF);
  buffer[1] = (uint8_t)((value >> 16) & 0xFF);
  buffer[2] = (uint8_t)((value >> 8) & 0xFF);
  buffer[3] = (uint8_t)(value & 0xFF);
}

uint32_t readUint32BE(const uint8_t *buffer)
{
  return ((uint32_t)buffer[0] << 24) |
         ((uint32_t)buffer[1] << 16) |
         ((uint32_t)buffer[2] << 8) |
         ((uint32_t)buffer[3]);
}

bool writeDeviceIdBytes(const String &deviceIdHex, uint8_t *outBytes)
{
  // El deviceId debe contener exactamente 6 bytes:
  // 12 caracteres hexadecimales.
  if (deviceIdHex.length() != DEVICE_ID_SIZE * 2)
  {
    return false;
  }

  if (!isValidHex(deviceIdHex))
  {
    return false;
  }

  return hexToBytes(deviceIdHex, outBytes, DEVICE_ID_SIZE);
}

String readDeviceIdHex(const uint8_t *bytes)
{
  return bytesToHex(bytes, DEVICE_ID_SIZE);
}

bool buildBinaryHeader(uint8_t type, uint32_t seq, const String &deviceIdHex, uint8_t *outHeader, size_t outSize)
{
  // Comprobar buffer
  if (outHeader == nullptr || outSize < LORA_HEADER_SIZE)
  {
    return false;
  }

  // Solo admitimos los tipos definidos actualmente
  if (type != LORA_TYPE_DATA && type != LORA_TYPE_ACK)
  {
    return false;
  }

  // Versión del protocolo
  outHeader[OFFSET_VERSION] = LORA_PROTOCOL_VERSION;

  // Tipo de mensaje
  outHeader[OFFSET_TYPE] = type;

  // Número de secuencia: 4 bytes big-endian
  writeUint32BE(&outHeader[OFFSET_SEQ], seq);

  // Device ID: 12 caracteres HEX -> 6 bytes
  if (!writeDeviceIdBytes(deviceIdHex, &outHeader[OFFSET_DEVICE_ID]))
  {
    return false;
  }

  return true;
}

bool parseBinaryHeader(const uint8_t *buffer, size_t len, LoRaBinaryHeader &header)
{
  // Comprobar buffer mínimo
  if (buffer == nullptr || len < LORA_HEADER_SIZE)
  {
    return false;
  }

  // Leer versión
  header.version = buffer[OFFSET_VERSION];

  if (header.version != LORA_PROTOCOL_VERSION)
  {
    return false;
  }

  // Leer tipo
  header.type = buffer[OFFSET_TYPE];

  if (header.type != LORA_TYPE_DATA && header.type != LORA_TYPE_ACK)
  {
    return false;
  }

  // Leer secuencia
  header.seq = readUint32BE(&buffer[OFFSET_SEQ]);

  // Leer deviceId
  header.deviceId = readDeviceIdHex(&buffer[OFFSET_DEVICE_ID]);

  return true;
}

bool LoRaProtocol::createDataMessageBinary(uint32_t seq, const String &deviceId, const String &payload, uint8_t *outPacket, size_t outCapacity, size_t &outLen)
{
  return ::createDataMessageBinary(seq, deviceId, payload, outPacket, outCapacity, outLen);
}

bool LoRaProtocol::createAckMessageBinary(uint32_t seq, const String &deviceId, uint8_t *outPacket, size_t outCapacity, size_t &outLen)
{
  return ::createAckMessageBinary(seq, deviceId, outPacket, outCapacity, outLen);
}

bool LoRaProtocol::parseMessageBinary(const uint8_t *packet, size_t packetLen, LoRaMessage &msg)
{
  return ::parseDataMessageBinary(packet, packetLen, msg);
}

bool LoRaProtocol::parseAckBinary(const uint8_t *packet, size_t packetLen, LoRaAck &ack)
{
  return ::parseAckMessageBinary(packet, packetLen, ack);
}